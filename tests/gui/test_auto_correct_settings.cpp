/// @file test_auto_correct_settings.cpp
/// @brief Settings > Editor > Auto-correct and the editors: what is changed as the text is
///        typed, in the quotation marks of the text's language, and Ctrl+Z right after a
///        change; Tools > Auto-correct as You Type turns all the replacements on and off

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/auto_correct.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/auto_correct_coordinator.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/edit_coordinator.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/toolbar_manager.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <array>
#include <string>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// The texts of the page's check boxes, in its order
constexpr std::array<const char*, 4> CHECK_BOXES = {
    "Straight quotes with typographic ones",
    "Apostrophe ' with ’",
    "Hyphen opening a paragraph with a dash",
    "“...” with an ellipsis “…”",
};

/// Open Editor > Auto-correct, in the language of the program; the page in front of the
/// dialog, in its scroll area
QWidget* openAutoCorrectPage(SettingsDialog& dialog) {
    const QString page = QCoreApplication::translate("kalahari::gui::SettingsDialog", "Auto-correct");
    const QString group = QCoreApplication::translate("kalahari::gui::SettingsDialog", "Editor");
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->text(0) == page && (*it)->parent() != nullptr &&
            (*it)->parent()->text(0) == group) {
            tree->setCurrentItem(*it);
        }
    }
    auto* stack = dialog.findChild<QStackedWidget*>();
    REQUIRE(stack != nullptr);
    REQUIRE(stack->currentWidget() != nullptr);
    return stack->currentWidget();
}

/// The page's check box with a text
QCheckBox* checkBoxOf(const QWidget* page, const char* text) {
    for (QCheckBox* box : page->findChildren<QCheckBox*>()) {
        if (box->text() == QString::fromUtf8(text)) {
            return box;
        }
    }
    return nullptr;
}

/// The page's label with a text
QLabel* labelOf(const QWidget* page, const QString& text) {
    for (QLabel* label : page->findChildren<QLabel*>()) {
        if (label->text() == text) {
            return label;
        }
    }
    return nullptr;
}

/// The page's label whose text starts with @p start
QString labelStartingWith(const QWidget* page, const QString& start) {
    for (const QLabel* label : page->findChildren<QLabel*>()) {
        if (label->text().startsWith(start)) {
            return label->text();
        }
    }
    return QString();
}

/// Type the text as the keyboard does, a character at a time
void type(QWidget* widget, const QString& text) {
    for (const QChar ch : text) {
        QKeyEvent pressed(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(widget, &pressed);
    }
}

/// A key pressed and released, as the keyboard does it: the window's shortcuts first
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent pressed(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &pressed);
    QKeyEvent released(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(widget, &released);
}

/// The window's tests need an active window; the offscreen platform has one without a window
/// on screen
bool canActivateWindows() {
    return QGuiApplication::platformName() == QStringLiteral("offscreen");
}

constexpr const char* NEEDS_OFFSCREEN =
    "needs an active window without a window on screen: run with QT_QPA_PLATFORM=offscreen";

}  // anonymous namespace

TEST_CASE("Settings > Auto-correct: everything is on at first, the open editors follow the "
          "changes",
          "[gui][settings][autocorrect]") {
    auto& settings = core::SettingsManager::getInstance();
    EditorPanel panel;
    editor::BookEditor* bookEditor = panel.getBookEditor();
    REQUIRE(bookEditor != nullptr);
    CHECK(bookEditor->autoCorrect() == editor::AutoCorrectOptions{});

    SettingsDialog dialog(nullptr);
    QWidget* page = openAutoCorrectPage(dialog);
    for (const char* text : CHECK_BOXES) {
        INFO(text);
        const QCheckBox* box = checkBoxOf(page, text);
        REQUIRE(box != nullptr);
        CHECK(box->isChecked());
        CHECK(box->isEnabled());
    }
    auto* dash = page->findChild<QComboBox*>();
    REQUIRE(dash != nullptr);
    const QLabel* dashLabel = labelOf(page, QStringLiteral("Dialogue dash:"));
    REQUIRE(dashLabel != nullptr);
    CHECK(dashLabel->buddy() == dash);
    CHECK(dash->currentText() == QStringLiteral("Em Dash (—)"));
    CHECK(dash->isEnabled());

    // The dash is chosen only for the hyphen changed
    QCheckBox* dialogueDash = checkBoxOf(page, CHECK_BOXES[2]);
    dialogueDash->setChecked(false);
    CHECK_FALSE(dash->isEnabled());
    CHECK_FALSE(dashLabel->isEnabled());
    dialogueDash->setChecked(true);
    CHECK(dash->isEnabled());
    CHECK(dashLabel->isEnabled());

    // Applied, the settings reach the editors
    checkBoxOf(page, CHECK_BOXES[0])->setChecked(false);
    checkBoxOf(page, CHECK_BOXES[3])->setChecked(false);
    dash->setCurrentIndex(dash->findData(QStringLiteral("enDash")));
    REQUIRE(dash->currentText() == QStringLiteral("En Dash (–)"));
    dialog.applyButton()->click();
    CHECK_FALSE(settings.get<bool>("editor.autoCorrect.quotes"));
    CHECK(settings.get<bool>("editor.autoCorrect.apostrophe"));
    CHECK(settings.get<bool>("editor.autoCorrect.dialogueDash"));
    CHECK(settings.get<std::string>("editor.autoCorrect.dialogueDashType") == "enDash");
    CHECK_FALSE(settings.get<bool>("editor.autoCorrect.ellipsis"));
    QCoreApplication::processEvents();

    editor::AutoCorrectOptions expected;
    expected.quotes = false;
    expected.dash = editor::DialogueDash::En;
    expected.ellipsis = false;
    CHECK(bookEditor->autoCorrect() == expected);
    bookEditor->fromKml(test::kmlOf({QString()}));
    type(bookEditor, QStringLiteral("- \"Don't\"..."));
    CHECK(bookEditor->plainText() == QStringLiteral("– \"Don’t\"..."));

    // An editor opened later starts with them
    EditorPanel later;
    CHECK(later.getBookEditor()->autoCorrect() == expected);

    // A page opened again shows what is stored
    SettingsDialog again(nullptr);
    QWidget* shown = openAutoCorrectPage(again);
    CHECK_FALSE(checkBoxOf(shown, CHECK_BOXES[0])->isChecked());
    CHECK(checkBoxOf(shown, CHECK_BOXES[1])->isChecked());
    CHECK_FALSE(checkBoxOf(shown, CHECK_BOXES[3])->isChecked());
    CHECK(shown->findChild<QComboBox*>()->currentText() == QStringLiteral("En Dash (–)"));
}

TEST_CASE("Settings > Auto-correct: the dash is greyed out while the dialogue dash is off",
          "[gui][settings][autocorrect]") {
    core::SettingsManager::getInstance().set<bool>("editor.autoCorrect.dialogueDash", false);
    SettingsDialog dialog(nullptr);
    QWidget* page = openAutoCorrectPage(dialog);
    const QCheckBox* dialogueDash = checkBoxOf(page, CHECK_BOXES[2]);
    REQUIRE(dialogueDash != nullptr);
    CHECK_FALSE(dialogueDash->isChecked());
    CHECK_FALSE(page->findChild<QComboBox*>()->isEnabled());
    const QLabel* dashLabel = labelOf(page, QStringLiteral("Dialogue dash:"));
    REQUIRE(dashLabel != nullptr);
    CHECK_FALSE(dashLabel->isEnabled());
    CHECK_FALSE(dialog.hasChanges());
}

TEST_CASE("Settings > Auto-correct: off as a whole, the replacements are greyed out and keep "
          "their choices",
          "[gui][settings][autocorrect]") {
    auto& settings = core::SettingsManager::getInstance();
    SettingsDialog dialog(nullptr);
    QWidget* page = openAutoCorrectPage(dialog);
    QCheckBox* enabled = checkBoxOf(page, "Auto-correct as you type");
    REQUIRE(enabled != nullptr);
    CHECK(enabled->isChecked());
    auto* dash = page->findChild<QComboBox*>();
    REQUIRE(dash != nullptr);

    enabled->setChecked(false);
    for (const char* text : CHECK_BOXES) {
        INFO(text);
        const QCheckBox* box = checkBoxOf(page, text);
        REQUIRE(box != nullptr);
        CHECK_FALSE(box->isEnabled());
        CHECK(box->isChecked());
    }
    CHECK_FALSE(dash->isEnabled());
    dialog.applyButton()->click();
    CHECK_FALSE(settings.get<bool>("editor.autoCorrect.enabled"));
    CHECK(settings.get<bool>("editor.autoCorrect.quotes"));
    CHECK(settings.get<bool>("editor.autoCorrect.ellipsis"));

    // On again: as they were
    enabled->setChecked(true);
    for (const char* text : CHECK_BOXES) {
        INFO(text);
        CHECK(checkBoxOf(page, text)->isEnabled());
    }
    CHECK(dash->isEnabled());
}

TEST_CASE("Settings > Auto-correct: the note gives the keys Undo has", "[gui][settings][autocorrect]") {
    SECTION("The command's keys, also those the user gives it") {
        registerAllCommands(CommandCallbacks{});
        const Command* undo = CommandRegistry::getInstance().getCommand("edit.undo");
        REQUIRE(undo != nullptr);
        const QString keys = undo->shortcut.toQKeySequence().toString(QKeySequence::NativeText);
        REQUIRE_FALSE(keys.isEmpty());

        SettingsDialog dialog(nullptr);
        QWidget* page = openAutoCorrectPage(dialog);
        CHECK(labelStartingWith(page, keys) ==
              keys + QStringLiteral(" right after a replacement undoes just the replacement."));

        const KeyboardShortcut userKeys(Qt::Key_F12, Qt::ControlModifier);
        CommandRegistry::getInstance().setCustomShortcuts({{"edit.undo", userKeys}});
        const QString newKeys = userKeys.toQKeySequence().toString(QKeySequence::NativeText);
        CHECK(labelStartingWith(page, newKeys) ==
              newKeys + QStringLiteral(" right after a replacement undoes just the replacement."));

        CommandRegistry::getInstance().setCustomShortcuts({{"edit.undo", KeyboardShortcut()}});
        CHECK_FALSE(
            labelStartingWith(page, QStringLiteral("Edit > Undo right after a replacement")).isEmpty());
    }

    SECTION("No keys, no mention of them") {
        CommandRegistry::getInstance().clear();
        SettingsDialog dialog(nullptr);
        QWidget* page = openAutoCorrectPage(dialog);
        CHECK(labelStartingWith(page, QStringLiteral("Edit > Undo")) ==
              QStringLiteral("Edit > Undo right after a replacement undoes just the replacement."));
    }
}

TEST_CASE("Autocorrect: a chapter has the quotation marks of the book's language, a file "
          "those of the program's",
          "[gui][autocorrect]") {
    auto& settings = core::SettingsManager::getInstance();
    auto& projects = core::ProjectManager::getInstance();
    REQUIRE(projects.closeProject(false));
    settings.setLanguage("en");
    settings.set<std::string>("editor.spellCheck.language", "");

    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    REQUIRE(projects.createProject(folder.path(), QStringLiteral("Autocorrect Test"),
                                   QStringLiteral("Author"), QStringLiteral("pl"), true));

    EditorPanel chapter;
    chapter.setProperty("elementId", QStringLiteral("chapter-1"));
    EditorPanel file;
    CHECK(chapter.textLanguage() == QStringLiteral("pl"));
    CHECK(file.textLanguage() == QStringLiteral("en"));

    for (EditorPanel* panel : {&chapter, &file}) {
        panel->getBookEditor()->fromKml(test::kmlOf({QString()}));
        type(panel->getBookEditor(), QStringLiteral("\"Tak\""));
    }
    CHECK(chapter.getBookEditor()->plainText() == QStringLiteral("„Tak”"));
    CHECK(file.getBookEditor()->plainText() == QStringLiteral("“Tak”"));

    // A language chosen on the Spelling page goes before the book's and the program's
    settings.set<std::string>("editor.spellCheck.language", "de_DE");
    CHECK(chapter.textLanguage() == QStringLiteral("de_DE"));
    CHECK(file.textLanguage() == QStringLiteral("de_DE"));
    type(file.getBookEditor(), QStringLiteral(" \"Ja\""));
    CHECK(file.getBookEditor()->plainText() == QStringLiteral("“Tak” „Ja“"));

    REQUIRE(projects.closeProject(false));
}

TEST_CASE("Autocorrect: Ctrl+Z right after a change brings back what was typed",
          "[gui][autocorrect][edit]") {
    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    QMainWindow window;
    auto* book = new editor::BookEditor();
    window.setCentralWidget(book);
    book->fromKml(test::kmlOf({QString()}));
    book->setTextLanguage([]() { return QStringLiteral("pl"); });

    // The window's Edit commands, as MainWindow gives them to the element with the keys
    EditCoordinator coordinator([]() -> editor::BookEditor* { return nullptr; });
    CommandCallbacks callbacks;
    callbacks.onUndo = [&coordinator] { coordinator.execute(EditCoordinator::Edit::Undo); };
    callbacks.onRedo = [&coordinator] { coordinator.execute(EditCoordinator::Edit::Redo); };
    registerAllCommands(callbacks);
    for (const char* id : {"edit.undo", "edit.redo"}) {
        window.addAction(CommandRegistry::getInstance().getAction(QString::fromLatin1(id)));
    }
    coordinator.connectCommands();
    window.show();
    window.activateWindow();
    REQUIRE(test::waitUntil([&window] { return QApplication::activeWindow() == &window; }));
    book->setFocus(Qt::OtherFocusReason);
    REQUIRE(test::waitUntil([book] { return QApplication::focusWidget() == book; }));

    type(book, QStringLiteral("- Tak..."));
    REQUIRE(book->plainText() == QStringLiteral("— Tak…"));

    // One press for each change: the ellipsis, then the text, then the dash
    press(book, Qt::Key_Z, Qt::ControlModifier);
    CHECK(book->plainText() == QStringLiteral("— Tak..."));
    CHECK(book->cursorPosition() == editor::CursorPosition{0, 8});
    press(book, Qt::Key_Z, Qt::ControlModifier);
    CHECK(book->plainText() == QStringLiteral("— "));
    press(book, Qt::Key_Z, Qt::ControlModifier);
    CHECK(book->plainText() == QStringLiteral("- "));
    CHECK(book->cursorPosition() == editor::CursorPosition{0, 2});

    // Typing goes on after the hyphen brought back
    type(book, QStringLiteral("tak"));
    CHECK(book->plainText() == QStringLiteral("- tak"));
}

TEST_CASE("Autocorrect: Tools > Auto-correct as You Type turns all the replacements on and off, "
          "its tooltip names them",
          "[gui][autocorrect][command]") {
    auto& settings = core::SettingsManager::getInstance();
    registerAllCommands(CommandCallbacks{});
    registerAllIcons();
    auto& registry = CommandRegistry::getInstance();
    QStatusBar statusBar;
    EditorPanel panel;
    editor::BookEditor* bookEditor = panel.getBookEditor();
    REQUIRE(bookEditor != nullptr);
    {
        AutoCorrectCoordinator coordinator(&statusBar);
        coordinator.connectCommands();
        QAction* action = registry.getAction(std::string("tools.autocorrect"));
        REQUIRE(action != nullptr);
        CHECK(action->text() == QStringLiteral("A&uto-correct as You Type"));
        CHECK(action->isCheckable());
        CHECK(action->isChecked());
        CHECK(action->isEnabled());
        CHECK(core::IconRegistry::getInstance().hasIcon(QStringLiteral("tools.autocorrect")));

        // In the Tools menu after the spelling, and on the Tools toolbar after its button (with
        // only the grammar's between them)
        const Command* command = registry.getCommand("tools.autocorrect");
        const Command* spelling = registry.getCommand("tools.spellcheck");
        REQUIRE(command != nullptr);
        REQUIRE(spelling != nullptr);
        CHECK(command->menuPath == "TOOLS/Auto-correct as You Type");
        CHECK(command->menuOrder > spelling->menuOrder);
        {
            QMainWindow window;
            ToolbarManager manager(&window);
            manager.createToolbars(registry);
            const QToolBar* tools = manager.getToolbar("tools");
            REQUIRE(tools != nullptr);
            const QList<QAction*> buttons = tools->actions();
            const qsizetype spellingButton =
                buttons.indexOf(registry.getAction(std::string("tools.spellcheck")));
            const qsizetype button = buttons.indexOf(action);
            REQUIRE(spellingButton >= 0);
            REQUIRE(button > spellingButton);
            for (qsizetype between = spellingButton + 1; between < button; ++between) {
                CHECK(buttons[between] == registry.getAction(std::string("tools.grammar")));
            }
        }

        // The tooltip: the name, the replacements chosen and where they are chosen
        const QString where = QStringLiteral(
            "The replacements are chosen in Edit > Settings..., group Editor > Auto-correct.");
        CHECK(action->toolTip() ==
              QStringLiteral("Auto-correct as You Type\n"
                             "Active replacements: typographic quotes, apostrophe ’, dialogue "
                             "dash —, ellipsis …\n") +
                  where);

        // Off: the editors replace nothing, the replacements keep their choices
        action->trigger();
        CHECK_FALSE(settings.get<bool>("editor.autoCorrect.enabled"));
        CHECK(settings.get<bool>("editor.autoCorrect.quotes"));
        CHECK_FALSE(action->isChecked());
        CHECK(statusBar.currentMessage() == QStringLiteral("Auto-correct as you type: off"));
        editor::AutoCorrectOptions nothing;
        nothing.quotes = false;
        nothing.apostrophe = false;
        nothing.dialogueDash = false;
        nothing.ellipsis = false;
        REQUIRE(test::waitUntil([bookEditor, &nothing]() {
            return bookEditor->autoCorrect() == nothing;
        }));
        bookEditor->fromKml(test::kmlOf({QString()}));
        type(bookEditor, QStringLiteral("- \"Don't\"..."));
        CHECK(bookEditor->plainText() == QStringLiteral("- \"Don't\"..."));
        CHECK(test::waitUntil([action]() {
            return action->toolTip().contains(
                QStringLiteral("\nOff. When turned on: typographic quotes, apostrophe ’, "
                               "dialogue dash —, ellipsis …\n"));
        }));

        // A replacement chosen in the Settings dialog: the tooltip follows
        settings.set<bool>("editor.autoCorrect.apostrophe", false);
        settings.set<std::string>("editor.autoCorrect.dialogueDashType", "enDash");
        CHECK(test::waitUntil([action]() {
            return action->toolTip().contains(QStringLiteral(
                "\nOff. When turned on: typographic quotes, dialogue dash –, ellipsis …\n"));
        }));

        // On again: the replacements chosen
        action->trigger();
        CHECK(settings.get<bool>("editor.autoCorrect.enabled"));
        CHECK(action->isChecked());
        CHECK(statusBar.currentMessage() == QStringLiteral("Auto-correct as you type: on"));
        editor::AutoCorrectOptions chosen;
        chosen.apostrophe = false;
        chosen.dash = editor::DialogueDash::En;
        REQUIRE(test::waitUntil([bookEditor, &chosen]() {
            return bookEditor->autoCorrect() == chosen;
        }));
        CHECK(test::waitUntil([action]() {
            return action->toolTip().contains(QStringLiteral(
                "\nActive replacements: typographic quotes, dialogue dash –, ellipsis …\n"));
        }));

        // None chosen
        for (const char* key : {"editor.autoCorrect.quotes", "editor.autoCorrect.dialogueDash",
                                "editor.autoCorrect.ellipsis"}) {
            settings.set<bool>(key, false);
        }
        CHECK(test::waitUntil([action]() {
            return action->toolTip().contains(QStringLiteral("\nNo replacement is chosen\n"));
        }));

        // Turned off in the Settings dialog: the command shows it
        settings.set<bool>("editor.autoCorrect.enabled", false);
        CHECK(test::waitUntil([action]() { return !action->isChecked(); }));
        CHECK(test::waitUntil([action]() {
            return action->toolTip().contains(
                QStringLiteral("\nOff. No replacement is chosen\n"));
        }));
    }

    // The command must not reach the coordinator that is gone
    const Command* command = registry.getCommand("tools.autocorrect");
    REQUIRE(command != nullptr);
    CHECK_FALSE(static_cast<bool>(command->execute));
}
