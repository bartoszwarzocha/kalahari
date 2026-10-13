/// @file test_auto_correct_settings.cpp
/// @brief Settings > Editor > Auto-correct and the editors: what is changed as the text is
///        typed, in the quotation marks of the text's language, and Ctrl+Z right after a
///        change

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/auto_correct.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/edit_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/settings_dialog.h"

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
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <array>
#include <optional>
#include <string>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// The texts of the page's check boxes, in its order
constexpr std::array<const char*, 4> CHECK_BOXES = {
    "Straight quotes with typographic ones",
    "Apostrophe ' with ’",
    "Hyphen at the start of a paragraph with a dash",
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

/// The layouts of a shown window settle
void settle() {
    for (int pass = 0; pass < 3; ++pass) {
        QApplication::processEvents();
    }
}

/// The program in Polish while it lives: the translation it ships
class Polish {
public:
    Polish() {
        m_loaded = m_translator.load(QStringLiteral(":/i18n/kalahari_pl.qm"));
        if (m_loaded) {
            QCoreApplication::installTranslator(&m_translator);
        }
    }
    ~Polish() { QCoreApplication::removeTranslator(&m_translator); }
    Polish(const Polish&) = delete;
    Polish& operator=(const Polish&) = delete;

    [[nodiscard]] bool loaded() const { return m_loaded; }

private:
    QTranslator m_translator;
    bool m_loaded = false;
};

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

TEST_CASE("Settings > Auto-correct: fits a small screen, in English and in Polish",
          "[gui][settings][autocorrect]") {
    for (const bool inPolish : {false, true}) {
        INFO((inPolish ? "in Polish" : "in English"));
        std::optional<Polish> polish;
        if (inPolish) {
            polish.emplace();
            REQUIRE(polish->loaded());
        }
        SettingsDialog dialog(nullptr);
        auto* scrollArea = qobject_cast<QScrollArea*>(openAutoCorrectPage(dialog));
        REQUIRE(scrollArea != nullptr);
        REQUIRE(scrollArea->widget() != nullptr);
        dialog.show();

        // 1366 × 768 at 125%: the dialog gets 90% of 1093 × 566 above the taskbar; at 150%,
        // 90% of 911 × 464. The page may scroll down, never sideways
        for (const QSize size : {QSize(840, 509), QSize(820, 417)}) {
            INFO("window " << size.width() << " x " << size.height());
            dialog.resize(size);
            settle();
            CHECK(scrollArea->widget()->minimumSizeHint().width() <=
                  scrollArea->viewport()->width());
            CHECK_FALSE(scrollArea->horizontalScrollBar()->isVisible());
        }
    }
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
