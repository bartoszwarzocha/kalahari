/// @file test_list_dialogs.cpp
/// @brief Format > Bullets and Numbering... and Set Numbering Value...: the windows, their
///        fields and what the editor gets from them

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_lists.h"
#include "kalahari/gui/dialogs/list_dialogs.h"
#include "kalahari/gui/dialogs/special_character_dialog.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include "document_window.h"

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
#include <QTimer>

#include <optional>

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
    gui::EditorPanel panel;
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
        CHECK(dialog->isNumberingShown());
        tileOf(*dialog, QStringLiteral("A. B. C."))->click();
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
