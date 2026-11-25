// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_list_dialogs.cpp
/// @brief Format > Bullets and Numbering... and Set Numbering Value...: the windows, their
///        fields and what the editor gets from them

#include <catch2/catch_test_macros.hpp>
#include "kalahari_tests/editor/editor_test_utils.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_lists.h"
#include "kalahari/gui/dialogs/list_dialogs.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/special_character_dialog.h"
#include "kalahari/gui/list_sets.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include "kalahari_tests/gui/document_window.h"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>

#include <nlohmann/json.hpp>

#include <cstdio>
#include <optional>
#include <string>
#include <vector>
#include "kalahari_tests/test_support/window_parts.h"

using namespace kalahari;
using namespace kalahari::editor;
using kalahari::gui::LengthSpinBox;
using kalahari::gui::dialogs::BulletsNumberingDialog;
using kalahari::gui::dialogs::ListNumberSpinBox;
using kalahari::gui::dialogs::ListPreview;
using kalahari::gui::dialogs::NumberingValueDialog;
using kalahari::gui::dialogs::SpecialCharacterDialog;

namespace {

/// A tile of a format by its name, on the tab in front
QAbstractButton* tileOf(const BulletsNumberingDialog& dialog, const QString& name)
{
    for (QAbstractButton* button : dialog.findChildren<QAbstractButton*>()) {
        if (button->accessibleName() == name && button->isVisibleTo(&dialog)) {
            return button;
        }
    }
    return nullptr;
}

/// A button by its text, shown in the window
QAbstractButton* buttonOf(const QWidget& dialog, const QString& text)
{
    for (QAbstractButton* button : dialog.findChildren<QAbstractButton*>()) {
        if (button->text() == text && button->isVisibleTo(&dialog)) {
            return button;
        }
    }
    return nullptr;
}

/// The field a label names, on the tab in front
template <typename Field>
Field* fieldOf(const QWidget& dialog, const QString& label)
{
    for (QLabel* each : dialog.findChildren<QLabel*>()) {
        if (each->text() == label && each->isVisibleTo(&dialog)) {
            return qobject_cast<Field*>(each->buddy());
        }
    }
    return nullptr;
}

/// The preview of the tab in front
ListPreview* previewOf(const QWidget& dialog)
{
    for (ListPreview* preview : dialog.findChildren<ListPreview*>()) {
        if (preview->isVisibleTo(&dialog)) {
            return preview;
        }
    }
    return nullptr;
}

/// Type a text in a field, in place of its text
void typeIn(QAbstractSpinBox& field, const QString& text)
{
    QLineEdit* line = field.findChild<QLineEdit*>();
    line->selectAll();
    line->insert(text);
    field.interpretText();
}

/// Close a window of another kind than the test waited for, so that the test goes on
void closeShownDialog()
{
    if (QDialog* dialog = test::shownDialog()) {
        dialog->reject();
    }
}

}  // namespace

TEST_CASE("Bullets and Numbering: the tiles give the level its numbers or its bullet",
          "[gui][lists][dialog]") {
    BulletsNumberingDialog dialog(ListDefinition::numbered(), 0);
    dialog.show();
    QApplication::processEvents();
    CHECK(dialog.windowTitle() == QStringLiteral("Bullets and Numbering"));
    // The window opens on the sets of whole lists; the tab of the level's kind next to it
    CHECK(dialog.isSetsShown());
    dialog.showNumbering(true);
    CHECK(dialog.isNumberingShown());
    CHECK(previewOf(dialog)->shownMarks() ==
          QStringList({QStringLiteral("1."), QStringLiteral("2."), QStringLiteral("a)"),
                       QStringLiteral("b)"), QStringLiteral("3.")}));

    // The tile of the level's format is chosen; another gives the level its format
    REQUIRE(tileOf(dialog, QStringLiteral("1. 2. 3.")) != nullptr);
    CHECK(tileOf(dialog, QStringLiteral("1. 2. 3."))->isChecked());
    tileOf(dialog, QStringLiteral("I. II. III."))->click();
    CHECK(dialog.definition().levels[0].format == ListNumberFormat::UpperRoman);
    CHECK(dialog.definition().levels[0].suffix == QStringLiteral("."));
    CHECK(previewOf(dialog)->shownMarks().first() == QStringLiteral("I."));
    CHECK_FALSE(tileOf(dialog, QStringLiteral("1. 2. 3."))->isChecked());

    // Start at, written as the level writes its numbers
    auto* start = fieldOf<ListNumberSpinBox>(dialog, QStringLiteral("&Start at"));
    REQUIRE(start != nullptr);
    CHECK(start->text() == QStringLiteral("I"));
    typeIn(*start, QStringLiteral("IV"));
    CHECK(dialog.definition().levels[0].start == 4);
    CHECK(previewOf(dialog)->shownMarks().first() == QStringLiteral("IV."));

    // Another level: its own format; the first one keeps its own
    dialog.setLevel(1);
    CHECK(tileOf(dialog, QStringLiteral("a) b) c)"))->isChecked());
    CHECK(start->text() == QStringLiteral("a"));
    tileOf(dialog, QStringLiteral("1) 2) 3)"))->click();
    CHECK(dialog.definition().levels[1].format == ListNumberFormat::Decimal);
    CHECK(dialog.definition().levels[1].suffix == QStringLiteral(")"));
    CHECK(dialog.definition().levels[0].format == ListNumberFormat::UpperRoman);
    // The preview of a level shows the level above it
    CHECK(previewOf(dialog)->shownMarks() ==
          QStringList({QStringLiteral("IV."), QStringLiteral("V."), QStringLiteral("1)"),
                       QStringLiteral("2)"), QStringLiteral("VI.")}));
}

TEST_CASE("Bullets and Numbering: the tab Bullets gives the level a bullet",
          "[gui][lists][dialog]") {
    BulletsNumberingDialog dialog(ListDefinition::numbered(), 0);
    dialog.show();
    QApplication::processEvents();

    // The other tab: the level gets the bullet of a new bulleted list
    dialog.showNumbering(false);
    CHECK(dialog.definition().levels[0].isBullet());
    CHECK(dialog.definition().levels[0].bullet == QString(QChar(BULLET_DISC)));
    CHECK(tileOf(dialog, QStringLiteral("Disc"))->isChecked());
    tileOf(dialog, QStringLiteral("Check mark"))->click();
    CHECK(dialog.definition().levels[0].bullet == QString(QChar(BULLET_CHECK)));
    CHECK(dialog.definition().kind() == ListKind::Bulleted);
    // The other levels keep their numbers
    CHECK(dialog.definition().levels[1].format == ListNumberFormat::LowerAlpha);

    // Character...: any character of the Special Character window
    dialog.setCharacterChooser([]() { return std::optional<QString>(QStringLiteral("★")); });
    REQUIRE(buttonOf(dialog, QStringLiteral("&Character...")) != nullptr);
    buttonOf(dialog, QStringLiteral("&Character..."))->click();
    CHECK(dialog.definition().levels[0].bullet == QStringLiteral("★"));
    CHECK(tileOf(dialog, QStringLiteral("Check mark"))->isChecked() == false);
    bool named = false;
    for (QLabel* label : dialog.findChildren<QLabel*>()) {
        named = named || label->text() == QStringLiteral("The bullet: ★");
    }
    CHECK(named);
    CHECK(previewOf(dialog)->shownMarks().first() == QStringLiteral("★"));

    // Chosen nothing: the bullet stays
    dialog.setCharacterChooser([]() { return std::optional<QString>(); });
    buttonOf(dialog, QStringLiteral("&Character..."))->click();
    CHECK(dialog.definition().levels[0].bullet == QStringLiteral("★"));

    // Back on Numbering: numbers again
    dialog.showNumbering(true);
    CHECK(dialog.definition().levels[0].format == ListNumberFormat::Decimal);
}

TEST_CASE("Bullets and Numbering: the indents, the alignment and Restore Defaults",
          "[gui][lists][dialog]") {
    ListDefinition bulleted = ListDefinition::bulleted();
    BulletsNumberingDialog dialog(bulleted, 2);
    dialog.show();
    QApplication::processEvents();
    dialog.showNumbering(false);
    CHECK_FALSE(dialog.isNumberingShown());
    CHECK(dialog.level() == 2);

    // The level chosen in the list of levels
    auto* levels = fieldOf<QListWidget>(dialog, QStringLiteral("&Level"));
    REQUIRE(levels != nullptr);
    CHECK(levels->currentRow() == 2);
    levels->setCurrentRow(0);
    CHECK(dialog.level() == 0);

    auto* text = fieldOf<LengthSpinBox>(dialog, QStringLiteral("&Text indent"));
    auto* marker = fieldOf<LengthSpinBox>(dialog, QStringLiteral("Bullet &indent"));
    REQUIRE(text != nullptr);
    REQUIRE(marker != nullptr);
    CHECK(text->storedValue() == bulleted.levels[0].textIndent);
    text->setStoredValue(40.0);
    marker->setStoredValue(10.0);
    CHECK(dialog.definition().levels[0].textIndent == 40.0);
    CHECK(dialog.definition().levels[0].markerIndent == 10.0);

    // The alignment of a number
    dialog.showNumbering(true);
    auto* alignment = fieldOf<QComboBox>(dialog, QStringLiteral("Number &alignment"));
    REQUIRE(alignment != nullptr);
    alignment->setCurrentIndex(alignment->findText(QStringLiteral("Right")));
    CHECK(dialog.definition().levels[0].alignment == ListMarkerAlignment::Right);

    // Restore Defaults: every level as a new list of the tab's kind
    REQUIRE(buttonOf(dialog, QStringLiteral("&Restore Defaults")) != nullptr);
    buttonOf(dialog, QStringLiteral("&Restore Defaults"))->click();
    CHECK(dialog.definition() == ListDefinition::numbered());
    CHECK(dialog.isNumberingShown());
}

TEST_CASE("Bullets and Numbering: the window fits a small screen", "[gui][lists][dialog]") {
    BulletsNumberingDialog dialog(ListDefinition::numbered(), 0);
    // 1366x768 at 150%: 911 pixels wide, as test_dock_layout.cpp counts
    CHECK(dialog.sizeHint().width() <= 911);
    NumberingValueDialog value(ListDefinition::numbered().levels[0], 3);
    CHECK(value.sizeHint().width() <= 911);
}

TEST_CASE("Set Numbering Value: the number as its level writes it", "[gui][lists][dialog]") {
    ListLevel letters;
    letters.format = ListNumberFormat::LowerAlpha;
    letters.suffix = QStringLiteral(")");
    NumberingValueDialog dialog(letters, 3);
    dialog.show();
    QApplication::processEvents();
    CHECK(dialog.windowTitle() == QStringLiteral("Set Numbering Value"));
    auto* field = fieldOf<ListNumberSpinBox>(dialog, QStringLiteral("Set &value to"));
    REQUIRE(field != nullptr);
    CHECK(field->text() == QStringLiteral("c"));
    CHECK(previewOf(dialog)->shownMarks() == QStringList{QStringLiteral("c)")});

    // Typed as a letter or as digits
    typeIn(*field, QStringLiteral("e"));
    CHECK(dialog.value() == 5);
    CHECK(previewOf(dialog)->shownMarks() == QStringList{QStringLiteral("e)")});
    typeIn(*field, QStringLiteral("27"));
    CHECK(dialog.value() == 27);
    CHECK(field->text() == QStringLiteral("aa"));
}

TEST_CASE("List number field: what can be typed", "[gui][lists][dialog]") {
    ListNumberSpinBox field;
    field.setNumberFormat(ListNumberFormat::LowerRoman);
    field.setValue(4);
    CHECK(field.text() == QStringLiteral("iv"));
    const QAbstractSpinBox& box = field;
    QString input = QStringLiteral("xi");
    int position = 0;
    CHECK(box.validate(input, position) == QValidator::Acceptable);
    input = QStringLiteral("iiii");
    CHECK(box.validate(input, position) == QValidator::Intermediate);
    input = QStringLiteral("q");
    CHECK(box.validate(input, position) == QValidator::Invalid);

    // A bullet has no numbers of its own: digits
    field.setNumberFormat(ListNumberFormat::Bullet);
    CHECK(field.text() == QStringLiteral("4"));
}

TEST_CASE("Special Character window: choosing a bullet", "[gui][lists][dialog]") {
    SpecialCharacterDialog window({}, {}, {});
    window.setChoosing();
    window.show();
    QApplication::processEvents();
    CHECK(window.acceptButton()->text() == QStringLiteral("&Choose"));
    CHECK(buttonOf(window, QStringLiteral("Assign &Shortcut...")) == nullptr);
    CHECK(buttonOf(window, QStringLiteral("Add &Replacement...")) == nullptr);
    window.setView(SpecialCharacterDialog::View::List);
    CHECK(buttonOf(window, QStringLiteral("Assign &Shortcut...")) == nullptr);

    QList<char32_t> chosen;
    QObject::connect(&window, &SpecialCharacterDialog::insertRequested,
                     [&chosen](char32_t code) { chosen.append(code); });
    window.selectCharacter(U'★');
    window.acceptButton()->click();
    CHECK(chosen == QList<char32_t>{U'★'});
    CHECK_FALSE(window.isVisible());
}

TEST_CASE("Editor panel: the list windows change the list at the cursor", "[gui][lists][dialog]") {
    gui::EditorPanel panel(kalahari::test::book(), kalahari::test::commands());
    BookEditor* editor = panel.getBookEditor();
    editor->fromKml(QStringLiteral("<kml><ol><li>One</li><li>Two</li></ol></kml>"));
    editor->setCursorPosition({1, 0});

    // Bullets and Numbering: the formats of the window's tiles
    QTimer::singleShot(0, [&]() {
        auto* dialog = qobject_cast<BulletsNumberingDialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            closeShownDialog();
            return;
        }
        CHECK(dialog->isSetsShown());
        dialog->showNumbering(true);
        QAbstractButton* tile = tileOf(*dialog, QStringLiteral("A. B. C."));
        CHECK(tile != nullptr);
        if (tile != nullptr) {
            tile->click();
        }
        dialog->accept();
    });
    panel.showBulletsAndNumbering();
    CHECK(listMarkers(*editor->textDocument()) ==
          std::vector<QString>{QStringLiteral("A."), QStringLiteral("B.")});

    // Set Numbering Value: the item's number, as its level writes it
    QTimer::singleShot(0, [&]() {
        auto* dialog = qobject_cast<NumberingValueDialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            closeShownDialog();
            return;
        }
        CHECK(dialog->value() == 2);
        dialog->setValue(5);
        dialog->accept();
    });
    panel.showNumberingValue();
    CHECK(listMarkers(*editor->textDocument()) ==
          std::vector<QString>{QStringLiteral("A."), QStringLiteral("E.")});

    // Cancel changes nothing
    QTimer::singleShot(0, [&]() {
        auto* dialog = qobject_cast<NumberingValueDialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            closeShownDialog();
            return;
        }
        dialog->setValue(9);
        dialog->reject();
    });
    panel.showNumberingValue();
    CHECK(listMarkers(*editor->textDocument())[1] == QStringLiteral("E."));
}

namespace {

/// The settings of the sets as they were, given back after a test
struct KeptListSets {
    std::vector<std::string> keys{gui::list_sets::PROGRAM_SETS_KEY,
                                  gui::list_sets::NUMBERED_DEFAULT_KEY,
                                  gui::list_sets::BULLETED_DEFAULT_KEY};
    nlohmann::json sets;
    std::string numbered;
    std::string bulleted;

    KeptListSets() {
        auto& settings = core::SettingsManager::getInstance();
        sets = settings.get<nlohmann::json>(gui::list_sets::PROGRAM_SETS_KEY, nlohmann::json::array());
        numbered = gui::list_sets::defaultId(ListKind::Numbered).toStdString();
        bulleted = gui::list_sets::defaultId(ListKind::Bulleted).toStdString();
        gui::list_sets::setProgram({});
        gui::list_sets::setDefault(ListKind::Numbered, QStringLiteral("numbered"));
        gui::list_sets::setDefault(ListKind::Bulleted, QStringLiteral("bulleted"));
    }
    ~KeptListSets() {
        try {
            auto& settings = core::SettingsManager::getInstance();
            settings.set<nlohmann::json>(gui::list_sets::PROGRAM_SETS_KEY, sets);
            settings.set<std::string>(gui::list_sets::NUMBERED_DEFAULT_KEY, numbered);
            settings.set<std::string>(gui::list_sets::BULLETED_DEFAULT_KEY, bulleted);
        } catch (...) {
            // A destructor may not throw: the later tests then find the sets of this one
            static_cast<void>(std::fputs("KeptListSets: the list sets of the test stay\n", stderr));
        }
    }
    KeptListSets(const KeptListSets&) = delete;
    KeptListSets& operator=(const KeptListSets&) = delete;
};

/// Name the set in the window that asks for it, once it is open
void nameSetSoon(const QString& name) {
    QTimer::singleShot(0, [name]() {
        auto* window = qobject_cast<gui::dialogs::ListSetNameDialog*>(test::shownDialog());
        CHECK(window != nullptr);
        if (window == nullptr) {
            closeShownDialog();
            return;
        }
        window->nameField()->setText(name);
        window->accept();
    });
}

}  // namespace

TEST_CASE("Bullets and Numbering: the tab Sets gives the whole list a set and keeps the "
          "writer's",
          "[gui][lists][dialog][sets]") {
    const KeptListSets kept;
    BulletsNumberingDialog dialog(ListDefinition::numbered(), 0);
    dialog.show();
    QApplication::processEvents();
    REQUIRE(dialog.isSetsShown());
    CHECK(dialog.chosenSet() == QStringLiteral("numbered"));
    CHECK(dialog.sets().size() == gui::list_sets::builtIn().size());

    // A set chosen: every level takes its formats
    dialog.chooseSet(QStringLiteral("multilevel"));
    CHECK(dialog.definition() ==
          gui::list_sets::find(QStringLiteral("multilevel"), nullptr)->definition);
    CHECK(dialog.chosenSet() == QStringLiteral("multilevel"));
    // Changed by hand: no set
    dialog.showNumbering(true);
    REQUIRE(tileOf(dialog, QStringLiteral("A. B. C.")) != nullptr);
    tileOf(dialog, QStringLiteral("A. B. C."))->click();
    CHECK(dialog.chosenSet().isEmpty());

    // Save as Set...: a text outside a book keeps it in the program
    nameSetSoon(QStringLiteral("Letters"));
    REQUIRE(dialog.saveAsSet());
    REQUIRE(gui::list_sets::program().size() == 1);
    const gui::ListSet mine = gui::list_sets::program().first();
    CHECK(mine.name == QStringLiteral("Letters"));
    CHECK(mine.definition == dialog.definition());
    CHECK(dialog.sets().size() == gui::list_sets::builtIn().size() + 1);
    CHECK(dialog.chosenSet() == mine.id);

    // The default of F12, a new name, and deleted
    dialog.makeDefault(mine.id);
    CHECK(gui::list_sets::defaultId(ListKind::Numbered) == mine.id);
    nameSetSoon(QStringLiteral("Capitals"));
    REQUIRE(dialog.renameSet(mine.id));
    CHECK(gui::list_sets::program().first().name == QStringLiteral("Capitals"));
    CHECK_FALSE(dialog.renameSet(QStringLiteral("numbered")));  // a built-in one keeps its name
    QTimer::singleShot(0, []() {
        auto* confirm = qobject_cast<gui::dialogs::MessageDialog*>(test::shownDialog());
        CHECK(confirm != nullptr);
        if (confirm != nullptr) {
            confirm->acceptButton()->click();
        }
    });
    REQUIRE(dialog.deleteSet(mine.id));
    CHECK(gui::list_sets::program().isEmpty());
    CHECK(dialog.sets().size() == gui::list_sets::builtIn().size());
    CHECK_FALSE(dialog.deleteSet(QStringLiteral("numbered")));
}

TEST_CASE("Bullets and Numbering: the text around the number and the numbers above",
          "[gui][lists][dialog][multilevel]") {
    BulletsNumberingDialog dialog(ListDefinition::numbered(), 1);
    dialog.show();
    QApplication::processEvents();
    dialog.showNumbering(true);
    auto* prefix = fieldOf<QLineEdit>(dialog, QStringLiteral("Te&xt before number"));
    auto* suffix = fieldOf<QLineEdit>(dialog, QStringLiteral("Text after nu&mber"));
    auto* levels = fieldOf<QComboBox>(dialog, QStringLiteral("Numbers of &higher levels"));
    REQUIRE(prefix != nullptr);
    REQUIRE(suffix != nullptr);
    REQUIRE(levels != nullptr);
    CHECK(levels->count() == 2);  // the second level: none, or the first one's
    tileOf(dialog, QStringLiteral("1. 2. 3."))->click();
    levels->setCurrentIndex(1);
    CHECK(dialog.definition().levels[1].shownLevels == 1);
    prefix->setText(QStringLiteral("Art. "));
    emit prefix->textEdited(prefix->text());
    CHECK(dialog.definition().levels[1].prefix == QStringLiteral("Art. "));
    CHECK(previewOf(dialog)->shownMarks().contains(QStringLiteral("Art. 2.1.")));
}

TEST_CASE("Settings > Editor > Lists: the defaults and the sets of the program",
          "[gui][lists][settings][sets]") {
    const KeptListSets kept;
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    gui::ListSet steps;
    steps.id = gui::list_sets::newId();
    steps.name = QStringLiteral("Steps");
    steps.definition = ListDefinition::numbered();
    steps.definition.levels[0].prefix = QStringLiteral("Step ");
    steps.place = gui::ListSetPlace::Program;
    gui::list_sets::setProgram({steps});

    gui::EditorListsPage page(kalahari::test::book(), kalahari::test::commands());
    page.load();
    auto* numbered = page.findChild<QComboBox*>(QStringLiteral("numberedSet"));
    auto* bulleted = page.findChild<QComboBox*>(QStringLiteral("bulletedSet"));
    auto* list = page.findChild<QListWidget*>(QStringLiteral("programSets"));
    REQUIRE(numbered != nullptr);
    REQUIRE(bulleted != nullptr);
    REQUIRE(list != nullptr);
    CHECK(numbered->count() == 7);  // the built-in numbered and multilevel ones, and Steps
    CHECK(bulleted->count() == 3);
    CHECK(numbered->currentData().toString() == QStringLiteral("numbered"));
    REQUIRE(list->count() == 1);
    CHECK(list->item(0)->text().startsWith(QStringLiteral("Steps: Step 1.")));

    // Steps the default of F12
    numbered->setCurrentIndex(numbered->findData(steps.id));
    page.apply();
    CHECK(gui::list_sets::defaultId(ListKind::Numbered) == steps.id);

    // Exported, deleted, imported back
    const QString path = folder.filePath(QStringLiteral("sets.json"));
    REQUIRE(page.exportSets(path));
    list->setCurrentRow(0);
    for (QPushButton* button : page.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("&Delete")) {
            button->click();
        }
    }
    CHECK(list->count() == 0);
    page.apply();
    CHECK(gui::list_sets::program().isEmpty());
    CHECK(page.importSets(path) == 1);
    CHECK(page.importSets(path) == 0);  // the same set once
    CHECK(list->count() == 1);
    page.apply();
    REQUIRE(gui::list_sets::program().size() == 1);
    CHECK(gui::list_sets::program().first().name == QStringLiteral("Steps"));
}
