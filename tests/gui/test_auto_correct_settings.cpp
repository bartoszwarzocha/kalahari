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
#include "kalahari/gui/auto_correct_replacements.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/edit_coordinator.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/toolbar_manager.h"
#include "kalahari/gui/utils/caps_lock.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <algorithm>
#include <array>
#include <nlohmann/json.hpp>
#include <string>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// The texts of the page's check boxes, in its order
constexpr std::array<const char*, 4> CHECK_BOXES = {
    "Straight quotes with typographic ones",
    "Apostrophe ' with ’",
    "Hyphen opening a paragraph with:",
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

/// The page's widget of a type with an accessible name
template <typename Widget>
Widget* namedChild(const QWidget* page, const char* name) {
    for (Widget* widget : page->findChildren<Widget*>()) {
        if (widget->accessibleName() == QString::fromUtf8(name)) {
            return widget;
        }
    }
    return nullptr;
}

/// The page's check box with a text
QCheckBox* checkBoxOf(const QWidget* page, const QString& text) {
    for (QCheckBox* box : page->findChildren<QCheckBox*>()) {
        if (box->text() == text) {
            return box;
        }
    }
    return nullptr;
}

QCheckBox* checkBoxOf(const QWidget* page, const char* text) {
    return checkBoxOf(page, QString::fromUtf8(text));
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

/// The options with the lists (the replacement table, the exceptions) of @p from; their
/// switches are compared apart from the lists
editor::AutoCorrectOptions withListsOf(editor::AutoCorrectOptions options,
                                       const editor::AutoCorrectOptions& from) {
    options.replacements = from.replacements;
    options.languageAbbreviations = from.languageAbbreviations;
    options.twoCapitalsExceptions = from.twoCapitalsExceptions;
    return options;
}

/// Whether a list of the settings has a text
bool textsContain(const nlohmann::json& list, const char* text) {
    return list.is_array() && std::find(list.begin(), list.end(), text) != list.end();
}

/// Type the text with Caps Lock on: Shift held for its small letters
void typeWithCapsLock(QWidget* widget, const QString& text) {
    for (const QChar ch : text) {
        QKeyEvent pressed(QEvent::KeyPress, 0, ch.isLower() ? Qt::ShiftModifier : Qt::NoModifier,
                          QString(ch));
        QApplication::sendEvent(widget, &pressed);
    }
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
    const editor::AutoCorrectOptions lists = bookEditor->autoCorrect();
    CHECK(bookEditor->autoCorrect() == withListsOf({}, lists));

    SettingsDialog dialog(nullptr);
    QWidget* page = openAutoCorrectPage(dialog);
    for (const char* text : CHECK_BOXES) {
        INFO(text);
        const QCheckBox* box = checkBoxOf(page, text);
        REQUIRE(box != nullptr);
        CHECK(box->isChecked());
        CHECK(box->isEnabled());
    }
    auto* dash = namedChild<QComboBox>(page, "Dialogue dash");
    REQUIRE(dash != nullptr);
    CHECK(dash->currentText() == QStringLiteral("Em Dash (—)"));
    CHECK(dash->isEnabled());

    // The dash is chosen only for the hyphen changed
    QCheckBox* dialogueDash = checkBoxOf(page, CHECK_BOXES[2]);
    dialogueDash->setChecked(false);
    CHECK_FALSE(dash->isEnabled());
    dialogueDash->setChecked(true);
    CHECK(dash->isEnabled());

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

    editor::AutoCorrectOptions expected = withListsOf({}, lists);
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
    CHECK(namedChild<QComboBox>(shown, "Dialogue dash")->currentText() ==
          QStringLiteral("En Dash (–)"));
}

TEST_CASE("Settings > Auto-correct: the dash is greyed out while the dialogue dash is off",
          "[gui][settings][autocorrect]") {
    core::SettingsManager::getInstance().set<bool>("editor.autoCorrect.dialogueDash", false);
    SettingsDialog dialog(nullptr);
    QWidget* page = openAutoCorrectPage(dialog);
    const QCheckBox* dialogueDash = checkBoxOf(page, CHECK_BOXES[2]);
    REQUIRE(dialogueDash != nullptr);
    CHECK_FALSE(dialogueDash->isChecked());
    CHECK_FALSE(namedChild<QComboBox>(page, "Dialogue dash")->isEnabled());
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
    auto* dash = namedChild<QComboBox>(page, "Dialogue dash");
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

TEST_CASE("Settings > Auto-correct: the notes give the keys Undo has",
          "[gui][settings][autocorrect]") {
    const auto undoNote = [](const QString& undo) {
        return undo + QStringLiteral(" right after a replacement undoes just the replacement. In "
                                     "a paragraph of the Code style auto-correct changes nothing.");
    };
    const auto learnNote = [](const QString& undo) {
        return undo + QStringLiteral(" right after a capital or a change of two capitals adds the "
                                     "abbreviation or the word to its list.");
    };
    SECTION("The command's keys, also those the user gives it") {
        registerAllCommands(CommandCallbacks{});
        const Command* undo = CommandRegistry::getInstance().getCommand("edit.undo");
        REQUIRE(undo != nullptr);
        const QString keys = undo->shortcut.toQKeySequence().toString(QKeySequence::NativeText);
        REQUIRE_FALSE(keys.isEmpty());

        SettingsDialog dialog(nullptr);
        QWidget* page = openAutoCorrectPage(dialog);
        CHECK(labelOf(page, undoNote(keys)) != nullptr);
        CHECK(labelOf(page, learnNote(keys)) != nullptr);

        const KeyboardShortcut userKeys(Qt::Key_F12, Qt::ControlModifier);
        CommandRegistry::getInstance().setCustomShortcuts({{"edit.undo", userKeys}});
        const QString newKeys = userKeys.toQKeySequence().toString(QKeySequence::NativeText);
        CHECK(labelOf(page, undoNote(newKeys)) != nullptr);
        CHECK(labelOf(page, learnNote(newKeys)) != nullptr);

        CommandRegistry::getInstance().setCustomShortcuts({{"edit.undo", KeyboardShortcut()}});
        CHECK(labelOf(page, undoNote(QStringLiteral("Edit > Undo"))) != nullptr);
        CHECK(labelOf(page, learnNote(QStringLiteral("Edit > Undo"))) != nullptr);
    }

    SECTION("No keys, no mention of them") {
        CommandRegistry::getInstance().clear();
        SettingsDialog dialog(nullptr);
        QWidget* page = openAutoCorrectPage(dialog);
        CHECK(labelOf(page, undoNote(QStringLiteral("Edit > Undo"))) != nullptr);
        CHECK(labelOf(page, learnNote(QStringLiteral("Edit > Undo"))) != nullptr);
    }
}

TEST_CASE("Settings > Auto-correct: three tabs, Ctrl+Page Down and Ctrl+Page Up go between them",
          "[gui][settings][autocorrect]") {
    SettingsDialog dialog(nullptr);
    QWidget* page = openAutoCorrectPage(dialog);
    auto* tabs = page->findChild<QTabWidget*>();
    REQUIRE(tabs != nullptr);
    REQUIRE(tabs->count() == 3);
    CHECK(tabs->tabText(0) == QStringLiteral("While Typing"));
    CHECK(tabs->tabText(1) == QStringLiteral("Capitals"));
    CHECK(tabs->tabText(2) == QStringLiteral("Replacement Table"));
    CHECK(tabs->currentIndex() == 0);

    if (!canActivateWindows()) {
        SKIP(NEEDS_OFFSCREEN);
    }
    dialog.show();
    dialog.activateWindow();
    REQUIRE(test::waitUntil([&dialog] { return QApplication::activeWindow() == &dialog; }));
    QCheckBox* enabled = checkBoxOf(page, "Auto-correct as you type");
    enabled->setFocus(Qt::OtherFocusReason);
    REQUIRE(test::waitUntil([enabled] { return QApplication::focusWidget() == enabled; }));
    press(enabled, Qt::Key_PageDown, Qt::ControlModifier);
    CHECK(tabs->currentIndex() == 1);
    press(QApplication::focusWidget(), Qt::Key_PageDown, Qt::ControlModifier);
    CHECK(tabs->currentIndex() == 2);
    // From the last one to the first one, and back
    press(QApplication::focusWidget(), Qt::Key_PageDown, Qt::ControlModifier);
    CHECK(tabs->currentIndex() == 0);
    press(QApplication::focusWidget(), Qt::Key_PageUp, Qt::ControlModifier);
    CHECK(tabs->currentIndex() == 2);
}

TEST_CASE("Settings > Auto-correct: the replacements of the words, the capitals and the text "
          "elements",
          "[gui][settings][autocorrect]") {
    auto& settings = core::SettingsManager::getInstance();
    SettingsDialog dialog(nullptr);
    QWidget* page = openAutoCorrectPage(dialog);
    const std::array<std::pair<const char*, const char*>, 11> boxes = {{
        {"“ - ” between words with:", "editor.autoCorrect.dashBetweenWords"},
        {"In dialogue: the dash it starts with", "editor.autoCorrect.dashInDialogue"},
        {"“--” with an en dash “–”", "editor.autoCorrect.doubleHyphen"},
        {"Replacements from the table", "editor.autoCorrect.replacementTable"},
        {"“***” and Enter: a scene break", "editor.autoCorrect.sceneBreak"},
        {"A capital at the start of a sentence", "editor.autoCorrect.sentenceCapitals"},
        {"Correct TWo INitial CApitals", "editor.autoCorrect.twoInitialCapitals"},
        {"Correct words typed with Caps Lock on", "editor.autoCorrect.capsLock"},
        {"“*” and a space: a bulleted list", "editor.autoCorrect.bulletedLists"},
        {"“1.” or “1)” and a space: a numbered list", "editor.autoCorrect.numberedLists"},
        {"Internet addresses with links", "editor.autoCorrect.links"},
    }};
    for (const auto& [text, key] : boxes) {
        INFO(text);
        const QCheckBox* box = checkBoxOf(page, text);
        REQUIRE(box != nullptr);
        CHECK(box->isChecked());
    }
    auto* wordDash = namedChild<QComboBox>(page, "Dash between words");
    REQUIRE(wordDash != nullptr);
    CHECK(wordDash->currentText() == QStringLiteral("En Dash (–)"));

    // The dash and the dash of dialogue only with the replacement between words
    QCheckBox* betweenWords = checkBoxOf(page, boxes[0].first);
    QCheckBox* inDialogue = checkBoxOf(page, boxes[1].first);
    betweenWords->setChecked(false);
    CHECK_FALSE(wordDash->isEnabled());
    CHECK_FALSE(inDialogue->isEnabled());
    betweenWords->setChecked(true);
    CHECK(inDialogue->isEnabled());

    // Off as a whole: the capitals are greyed out too
    QCheckBox* enabled = checkBoxOf(page, "Auto-correct as you type");
    enabled->setChecked(false);
    CHECK_FALSE(checkBoxOf(page, boxes[5].first)->isEnabled());
    CHECK_FALSE(inDialogue->isEnabled());
    enabled->setChecked(true);
    CHECK(checkBoxOf(page, boxes[5].first)->isEnabled());

    // The table not used: its tab says so
    page->findChild<QTabWidget*>()->setCurrentIndex(2);
    const QString unused = QStringLiteral("The table is not used now: “Replacements from the "
                                          "table” is off on the While Typing tab.");
    CHECK_FALSE(labelOf(page, unused)->isVisibleTo(page));
    checkBoxOf(page, boxes[3].first)->setChecked(false);
    CHECK(labelOf(page, unused)->isVisibleTo(page));

    for (const auto& [text, key] : boxes) {
        checkBoxOf(page, text)->setChecked(false);
    }
    wordDash->setCurrentIndex(wordDash->findData(QStringLiteral("emDash")));
    dialog.applyButton()->click();
    for (const auto& [text, key] : boxes) {
        INFO(key);
        CHECK_FALSE(settings.get<bool>(key));
    }
    CHECK(settings.get<std::string>("editor.autoCorrect.dashBetweenWordsType") == "emDash");
    CHECK(settings.get<bool>("editor.autoCorrect.horizontalRule"));
}

TEST_CASE("Settings > Auto-correct: the exceptions of the capitals",
          "[gui][settings][autocorrect]") {
    auto& settings = core::SettingsManager::getInstance();
    SettingsDialog dialog(nullptr);
    QWidget* page = openAutoCorrectPage(dialog);
    auto* polish = namedChild<QListWidget>(page, "Abbreviations, Polish");
    auto* english = namedChild<QListWidget>(page, "Abbreviations, English and other languages");
    auto* twoCapitals = namedChild<QListWidget>(page, "Words with two capitals");
    REQUIRE(polish != nullptr);
    REQUIRE(english != nullptr);
    REQUIRE(twoCapitals != nullptr);
    CHECK_FALSE(polish->findItems(QStringLiteral("np."), Qt::MatchExactly).isEmpty());
    CHECK_FALSE(english->findItems(QStringLiteral("Mr."), Qt::MatchExactly).isEmpty());
    CHECK(twoCapitals->count() == 2);
    page->findChild<QTabWidget*>()->setCurrentIndex(1);

    // The list of the language chosen; the program speaks English here
    auto* language = labelOf(page, QStringLiteral("For a text in:"))->buddy();
    auto* languages = qobject_cast<QComboBox*>(language);
    REQUIRE(languages != nullptr);
    CHECK(languages->currentText() == QStringLiteral("English and other languages"));
    CHECK(english->isVisibleTo(page));
    CHECK_FALSE(polish->isVisibleTo(page));
    languages->setCurrentIndex(languages->findText(QStringLiteral("Polish")));
    CHECK(polish->isVisibleTo(page));

    // Enter in the field adds the word at the end; the dialog stays open
    auto* field = namedChild<QLineEdit>(page, "A word to add: Abbreviations, Polish");
    REQUIRE(field != nullptr);
    field->setText(QStringLiteral("ps."));
    press(field, Qt::Key_Return);
    CHECK(polish->item(polish->count() - 1)->text() == QStringLiteral("ps."));
    CHECK(field->text().isEmpty());
    // A word in the list already, or with a space, is not added
    field->setText(QStringLiteral("np."));
    press(field, Qt::Key_Return);
    field->setText(QStringLiteral("a b"));
    press(field, Qt::Key_Return);
    CHECK(polish->findItems(QStringLiteral("np."), Qt::MatchExactly).size() == 1);
    CHECK(polish->findItems(QStringLiteral("a b"), Qt::MatchExactly).isEmpty());

    // Delete takes the words selected off
    const int before = polish->count();
    polish->setCurrentItem(polish->findItems(QStringLiteral("np."), Qt::MatchExactly).front());
    press(polish, Qt::Key_Delete);
    CHECK(polish->count() == before - 1);
    CHECK(polish->findItems(QStringLiteral("np."), Qt::MatchExactly).isEmpty());

    auto* capitalsField = namedChild<QLineEdit>(page, "A word to add: Words with two capitals");
    REQUIRE(capitalsField != nullptr);
    capitalsField->setText(QStringLiteral("CDs"));
    press(capitalsField, Qt::Key_Enter);

    dialog.applyButton()->click();
    const nlohmann::json abbreviations =
        settings.get<nlohmann::json>("editor.autoCorrect.abbreviations.pl");
    CHECK(textsContain(abbreviations, "ps."));
    CHECK_FALSE(textsContain(abbreviations, "np."));
    CHECK(textsContain(settings.get<nlohmann::json>("editor.autoCorrect.twoCapitalsExceptions"),
                       "CDs"));
    CHECK(textsContain(settings.get<nlohmann::json>("editor.autoCorrect.abbreviations.en"), "Mr."));
}

TEST_CASE("Settings > Auto-correct: the replacement table", "[gui][settings][autocorrect]") {
    // Each section starts with the program's pairs (a section before may have applied others)
    setReplacementEntries(defaultReplacementEntries());
    SettingsDialog dialog(nullptr);
    QWidget* page = openAutoCorrectPage(dialog);
    auto* table = namedChild<QTableWidget>(page, "Replacement table");
    auto* word = namedChild<QLineEdit>(page, "The word of a new pair");
    auto* replacement = namedChild<QLineEdit>(page, "Its replacement");
    REQUIRE(table != nullptr);
    REQUIRE(word != nullptr);
    REQUIRE(replacement != nullptr);
    const auto rowOf = [table](const QString& from) {
        for (int row = 0; row < table->rowCount(); ++row) {
            if (table->item(row, 1)->text() == from) {
                return row;
            }
        }
        return -1;
    };
    const auto pairsStored = []() { return replacementEntries(); };
    page->findChild<QTabWidget*>()->setCurrentIndex(2);

    // The program's own pairs, each one on
    REQUIRE(table->rowCount() == 11);
    CHECK(table->item(0, 1)->text() == QStringLiteral("(c)"));
    CHECK(table->item(0, 2)->text() == QStringLiteral("©"));
    CHECK(table->item(0, 0)->checkState() == Qt::Checked);
    CHECK(table->item(0, 3)->text() == QStringLiteral("built-in"));

    SECTION("A new pair: Enter in its fields adds it at the end, the writer's own") {
        word->setText(QStringLiteral("kal"));
        replacement->setText(QStringLiteral("Kalahari"));
        press(replacement, Qt::Key_Return);
        REQUIRE(table->rowCount() == 12);
        CHECK(table->item(11, 1)->text() == QStringLiteral("kal"));
        CHECK(table->item(11, 3)->text() == QStringLiteral("your own"));
        CHECK(table->currentRow() == 11);
        CHECK(word->text().isEmpty());
        dialog.applyButton()->click();
        CHECK(pairsStored().back() ==
              ReplacementEntry{QStringLiteral("kal"), QStringLiteral("Kalahari"), true});
    }

    SECTION("A word of the table: Add gives it the new replacement, saying so first") {
        word->setText(QStringLiteral("->"));
        replacement->setText(QStringLiteral("⇒"));
        const QLabel* note = labelOf(page, QStringLiteral("Auto-correct replaces “->” with “→” "
                                                          "now: Add puts “⇒” in its place."));
        REQUIRE(note != nullptr);
        CHECK(note->isVisibleTo(page));
        press(word, Qt::Key_Enter);
        CHECK(table->rowCount() == 11);
        CHECK(table->item(rowOf(QStringLiteral("->")), 2)->text() == QStringLiteral("⇒"));
        CHECK(table->item(rowOf(QStringLiteral("->")), 3)->text() == QStringLiteral("your own"));
        CHECK_FALSE(note->isVisibleTo(page));
    }

    SECTION("Space turns the pairs selected off and on, Delete removes them") {
        table->setCurrentCell(1, 1);
        table->selectionModel()->select(table->model()->index(2, 0),
                                        QItemSelectionModel::Select | QItemSelectionModel::Rows);
        press(table, Qt::Key_Space);
        CHECK(table->item(1, 0)->checkState() == Qt::Unchecked);
        CHECK(table->item(2, 0)->checkState() == Qt::Unchecked);
        CHECK(table->item(0, 0)->checkState() == Qt::Checked);
        dialog.applyButton()->click();
        CHECK_FALSE(pairsStored().at(1).enabled);
        CHECK(autoCorrectSources(QStringLiteral("®")).isEmpty());
        press(table, Qt::Key_Space);
        CHECK(table->item(1, 0)->checkState() == Qt::Checked);

        press(table, Qt::Key_Delete);
        CHECK(table->rowCount() == 9);
        CHECK(rowOf(QStringLiteral("(r)")) == -1);
        CHECK(rowOf(QStringLiteral("(tm)")) == -1);
        // The keys go on from the pair after them
        CHECK(table->currentRow() == 1);
        CHECK(table->item(1, 1)->text() == QStringLiteral("1/2"));

        // Restore Defaults: the program's pairs come back, the writer's stay
        table->item(0, 2)->setText(QStringLiteral("(C)"));
        word->setText(QStringLiteral("kal"));
        replacement->setText(QStringLiteral("Kalahari"));
        press(word, Qt::Key_Return);
        QPushButton* restore = nullptr;
        for (QPushButton* button : page->findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("Restore Defaults")) {
                restore = button;
            }
        }
        REQUIRE(restore != nullptr);
        restore->click();
        CHECK(table->rowCount() == 12);
        CHECK(table->item(0, 2)->text() == QStringLiteral("©"));
        CHECK(rowOf(QStringLiteral("(r)")) >= 0);
        CHECK(rowOf(QStringLiteral("kal")) >= 0);
    }

    SECTION("An edit the table cannot have is put back, saying why") {
        table->item(0, 1)->setText(QStringLiteral("(r)"));
        CHECK(table->item(0, 1)->text() == QStringLiteral("(c)"));
        CHECK(labelOf(page, QStringLiteral("The table has a pair of “(r)” already: “(c)” "
                                           "stays."))
                  ->isVisibleTo(page));
        table->item(0, 1)->setText(QStringLiteral("a b"));
        CHECK(table->item(0, 1)->text() == QStringLiteral("(c)"));
        table->item(0, 2)->setText(QStringLiteral(" "));
        CHECK(table->item(0, 2)->text() == QStringLiteral("©"));
        CHECK(labelOf(page, QStringLiteral("A word cannot be replaced with nothing: “©” stays."))
                  ->isVisibleTo(page));
        // A word edited well
        table->item(0, 1)->setText(QStringLiteral(" (C) "));
        CHECK(table->item(0, 1)->text() == QStringLiteral("(C)"));
        CHECK(table->item(0, 3)->text() == QStringLiteral("your own"));
        dialog.applyButton()->click();
        CHECK(pairsStored().front().from == QStringLiteral("(C)"));
    }

    SECTION("Insert goes to the word of a new pair") {
        if (!canActivateWindows()) {
            SKIP(NEEDS_OFFSCREEN);
        }
        dialog.show();
        dialog.activateWindow();
        REQUIRE(test::waitUntil([&dialog] { return QApplication::activeWindow() == &dialog; }));
        table->setFocus(Qt::OtherFocusReason);
        REQUIRE(test::waitUntil([table] { return QApplication::focusWidget() == table; }));
        press(table, Qt::Key_Insert);
        CHECK(QApplication::focusWidget() == word);
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
        nothing.dashBetweenWords = false;
        nothing.doubleHyphen = false;
        nothing.sentenceCapitals = false;
        nothing.twoInitialCapitals = false;
        nothing.capsLock = false;
        nothing.sceneBreak = false;
        nothing.horizontalRule = false;
        nothing.bulletedLists = false;
        nothing.numberedLists = false;
        nothing.links = false;
        nothing.languageAbbreviations = panel.getBookEditor()->autoCorrect().languageAbbreviations;
        nothing.twoCapitalsExceptions = panel.getBookEditor()->autoCorrect().twoCapitalsExceptions;
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
            return bookEditor->autoCorrect() == withListsOf(chosen, bookEditor->autoCorrect()) &&
                   !bookEditor->autoCorrect().replacements.empty();
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

TEST_CASE("Autocorrect: the settings of the words reach the editors",
          "[gui][settings][autocorrect]") {
    auto& settings = core::SettingsManager::getInstance();
    EditorPanel panel;
    editor::BookEditor* bookEditor = panel.getBookEditor();
    REQUIRE(bookEditor != nullptr);

    // The program's own lists
    const editor::AutoCorrectOptions& options = bookEditor->autoCorrect();
    CHECK(options.replacements.size() == 11);
    CHECK(std::find(options.replacements.begin(), options.replacements.end(),
                    editor::Replacement{QStringLiteral("(c)"), QStringLiteral("©")}) !=
          options.replacements.end());
    REQUIRE(options.languageAbbreviations.contains(QStringLiteral("pl")));
    REQUIRE(options.languageAbbreviations.contains(QStringLiteral("en")));
    CHECK(options.languageAbbreviations.at(QStringLiteral("pl")).contains(QStringLiteral("np.")));
    CHECK_FALSE(
        options.languageAbbreviations.at(QStringLiteral("pl")).contains(QStringLiteral("itd.")));
    CHECK(options.languageAbbreviations.at(QStringLiteral("en")).contains(QStringLiteral("Mr.")));
    CHECK(options.twoCapitalsExceptions == QStringList{QStringLiteral("IDs"), QStringLiteral("PCs")});

    // A pair turned off, a pair of the writer's, the dashes, the switches
    nlohmann::json pairs = settings.get<nlohmann::json>("editor.autoCorrect.replacements");
    REQUIRE(pairs.is_array());
    pairs[0]["enabled"] = false;  // (c)
    pairs.push_back({{"from", "kal"}, {"to", "Kalahari"}});
    settings.set<nlohmann::json>("editor.autoCorrect.replacements", pairs);
    settings.set<std::string>("editor.autoCorrect.dashBetweenWordsType", "emDash");
    settings.set<bool>("editor.autoCorrect.dashInDialogue", false);
    settings.set<bool>("editor.autoCorrect.sentenceCapitals", false);
    settings.set<bool>("editor.autoCorrect.sceneBreak", false);
    REQUIRE(test::waitUntil([bookEditor]() { return !bookEditor->autoCorrect().sceneBreak; }));
    const editor::AutoCorrectOptions& changed = bookEditor->autoCorrect();
    CHECK(changed.replacements.size() == 11);
    CHECK(changed.replacements.front() ==
          editor::Replacement{QStringLiteral("(r)"), QStringLiteral("®")});
    CHECK(changed.replacements.back() ==
          editor::Replacement{QStringLiteral("kal"), QStringLiteral("Kalahari")});
    CHECK(changed.wordDash == editor::DialogueDash::Em);
    CHECK_FALSE(changed.dialogueDashInDialogue);
    CHECK_FALSE(changed.sentenceCapitals);
    CHECK(changed.horizontalRule);

    bookEditor->fromKml(test::kmlOf({QString()}));
    type(bookEditor, QStringLiteral("o kal (c) - (r) "));
    CHECK(bookEditor->plainText() == QStringLiteral("o Kalahari (c) — ® "));

    // The table off as a whole: no pair
    settings.set<bool>("editor.autoCorrect.replacementTable", false);
    REQUIRE(test::waitUntil([bookEditor]() { return bookEditor->autoCorrect().replacements.empty(); }));
}

TEST_CASE("Autocorrect: a capital undone right after it adds to the exceptions",
          "[gui][autocorrect][undo]") {
    auto& settings = core::SettingsManager::getInstance();
    REQUIRE(core::ProjectManager::getInstance().closeProject(false));
    settings.set<std::string>("editor.spellCheck.language", "pl");
    QMainWindow window;
    auto* panel = new EditorPanel();
    window.setCentralWidget(panel);
    editor::BookEditor* bookEditor = panel->getBookEditor();
    bookEditor->fromKml(test::kmlOf({QString()}));

    SECTION("An abbreviation, in the list of the text's language") {
        type(bookEditor, QStringLiteral("Pies zw. reksem "));
        REQUIRE(bookEditor->plainText() == QStringLiteral("Pies zw. Reksem "));
        bookEditor->undo();
        CHECK(bookEditor->plainText() == QStringLiteral("Pies zw. reksem "));
        CHECK(settings.get<nlohmann::json>("editor.autoCorrect.abbreviations.pl").back() ==
              "zw.");
        CHECK(window.statusBar()->currentMessage() ==
              QStringLiteral("After “zw.” auto-correct no longer starts a sentence with a "
                             "capital: it is among the exceptions now."));

        // The editors have it: the next word after it stays
        REQUIRE(test::waitUntil([bookEditor]() {
            return bookEditor->autoCorrect()
                .languageAbbreviations.at(QStringLiteral("pl"))
                .contains(QStringLiteral("zw."));
        }));
        type(bookEditor, QStringLiteral("i zw. burkiem "));
        CHECK(bookEditor->plainText() == QStringLiteral("Pies zw. reksem i zw. burkiem "));
    }

    SECTION("A word with two capitals") {
        type(bookEditor, QStringLiteral("Ala ABc "));
        REQUIRE(bookEditor->plainText() == QStringLiteral("Ala Abc "));
        bookEditor->undo();
        CHECK(settings.get<nlohmann::json>("editor.autoCorrect.twoCapitalsExceptions").back() ==
              "ABc");
        CHECK(window.statusBar()->currentMessage() ==
              QStringLiteral(
                  "Auto-correct no longer changes “ABc”: it is among the exceptions now."));
    }

    SECTION("Not when the writer turned it off") {
        settings.set<bool>("editor.autoCorrect.learnExceptions", false);
        type(bookEditor, QStringLiteral("Pies ps. reksem "));
        REQUIRE(bookEditor->plainText() == QStringLiteral("Pies ps. Reksem "));
        bookEditor->undo();
        CHECK_FALSE(textsContain(settings.get<nlohmann::json>("editor.autoCorrect.abbreviations.pl"),
                                 "ps."));
        CHECK(window.statusBar()->currentMessage().isEmpty());
    }
}

TEST_CASE("Autocorrect: a word typed with Caps Lock on turns it off, or the writer is told",
          "[gui][autocorrect]") {
    QMainWindow window;
    auto* panel = new EditorPanel();
    window.setCentralWidget(panel);
    editor::BookEditor* bookEditor = panel->getBookEditor();
    bookEditor->fromKml(test::kmlOf({QString()}));

    int turnedOff = 0;
    bool systemLets = true;
    setCapsLockForTests([&turnedOff, &systemLets]() {
        ++turnedOff;
        return systemLets;
    });

    typeWithCapsLock(bookEditor, QStringLiteral("Ala wIELKI "));
    CHECK(bookEditor->plainText() == QStringLiteral("Ala Wielki "));
    CHECK(turnedOff == 1);
    CHECK(window.statusBar()->currentMessage() ==
          QStringLiteral("Caps Lock was on: the word typed with it is corrected, and Caps Lock "
                         "is off now."));

    systemLets = false;
    typeWithCapsLock(bookEditor, QStringLiteral("kOT "));
    CHECK(bookEditor->plainText() == QStringLiteral("Ala Wielki Kot "));
    CHECK(turnedOff == 2);
    CHECK(window.statusBar()->currentMessage() ==
          QStringLiteral("Caps Lock is on: the word typed with it is corrected. Turn Caps Lock "
                         "off."));
    setCapsLockForTests({});
}
