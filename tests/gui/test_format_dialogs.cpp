/// @file test_format_dialogs.cpp
/// @brief Format > Font..., Format > Paragraph... and the submenus Font Color and Highlight
///        Color: the windows, their fields and what the editor gets from them

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/character_format.h"
#include "kalahari/editor/paragraph_format.h"
#include "kalahari/gui/dialogs/color_dialog.h"
#include "kalahari/gui/dialogs/color_palettes.h"
#include "kalahari/gui/dialogs/font_dialog.h"
#include "kalahari/gui/dialogs/paragraph_dialog.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/widgets/color_menu.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include "document_window.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QWidgetAction>

#include <optional>
#include <string>
#include <vector>

using namespace kalahari;
using editor::CharacterFormat;
using editor::LineSpacing;
using editor::LineSpacingRule;
using editor::ParagraphFormat;
using gui::ColorMenu;
using gui::ColorSwatches;
using gui::dialogs::ColorDialog;
using gui::dialogs::FontDialog;
using gui::dialogs::ParagraphDialog;

namespace {

/// The color window's settings as a writer has them the first time
void forgetColorChoices()
{
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::vector<std::string>>("ui.recentColors", {});
    settings.set<std::string>("ui.colorPalette", "basic");
}

/// Press a key in a widget; whether the widget took it
bool press(QWidget* widget, Qt::Key key)
{
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
    return event.isAccepted();
}

/// Counts the keys that reach a widget: those its children leave to it
class KeyCounter : public QObject {
public:
    explicit KeyCounter(QWidget* widget) { widget->installEventFilter(this); }
    int keys = 0;

protected:
    bool eventFilter(QObject* /*watched*/, QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress) {
            ++keys;
        }
        return false;
    }
};

/// The formatting of plain text in the editor's font
CharacterFormat plainText()
{
    CharacterFormat format;
    format.family = QStringLiteral("Serif");
    format.size = 12.0;
    format.bold = false;
    format.italic = false;
    format.underline = false;
    format.strikethrough = false;
    format.position = QTextCharFormat::AlignNormal;
    format.capitals = QFont::MixedCase;
    format.letterSpacing = 0.0;
    format.color = QColor();
    format.highlight = QColor();
    return format;
}

/// The formatting of a paragraph as a style gives it
ParagraphFormat plainParagraph()
{
    ParagraphFormat format;
    format.leftIndent = 0.0;
    format.rightIndent = 0.0;
    format.firstLineIndent = 0.0;
    format.spaceBefore = 0.0;
    format.spaceAfter = 6.0;
    format.lineSpacing = LineSpacing{LineSpacingRule::Multiple, 1.5};
    format.keepWithNext = false;
    format.keepTogether = false;
    format.pageBreakBefore = false;
    format.widowControl = true;
    return format;
}

/// Close a window of another kind than the test waited for, so that the test goes on
void closeShownDialog()
{
    if (QDialog* dialog = test::shownDialog()) {
        dialog->reject();
    }
}

}  // namespace

// =============================================================================
// The color submenus
// =============================================================================

TEST_CASE("Color submenu: Automatic, the palette, the recent colors and More Colors",
          "[gui][format][colors]") {
    forgetColorChoices();
    ColorMenu menu(ColorMenu::Kind::Text);

    // The palette the color window shows: ten families in six shades
    const gui::dialogs::ColorPalette basic = ColorDialog::lastPalette();
    CHECK(basic.id == QStringLiteral("basic"));
    REQUIRE(menu.paletteSwatches()->count() ==
            gui::dialogs::PALETTE_FAMILIES * gui::dialogs::PALETTE_SHADES);
    CHECK(menu.paletteSwatches()->colorAt(0) == basic.colors.at(0).color);
    CHECK(menu.paletteSwatches()->textAt(1) ==
          basic.colors.at(1).name + QStringLiteral(" (") +
              basic.colors.at(1).color.name().toUpper() + QLatin1Char(')'));

    // No recent colors yet: their row is hidden; with one it shows
    CHECK_FALSE(menu.recentAction()->isVisible());
    ColorDialog::addRecentColor(QColor(QStringLiteral("#8b1a1a")));
    menu.refresh();
    CHECK(menu.recentAction()->isVisible());
    REQUIRE(menu.recentSwatches()->count() == 1);
    CHECK(menu.recentSwatches()->colorAt(0) == QColor(QStringLiteral("#8b1a1a")));

    CHECK(menu.noColorAction()->text() == QStringLiteral("&Automatic"));
    CHECK(menu.moreColorsAction()->text() == QStringLiteral("&More Colors..."));
    ColorMenu highlight(ColorMenu::Kind::Highlight);
    CHECK(highlight.noColorAction()->text() == QStringLiteral("&No Color"));

    // Before a color is chosen the command gives a red, or a yellow highlight, as in Word
    CHECK(menu.lastColor().hue() == 0);
    CHECK(highlight.lastColor().hue() > 40);
    CHECK(highlight.lastColor().hue() < 60);
    forgetColorChoices();
}

TEST_CASE("Color submenu: a swatch, Automatic and More Colors choose a color",
          "[gui][format][colors]") {
    forgetColorChoices();
    ColorMenu menu(ColorMenu::Kind::Text);
    std::vector<QColor> chosen;
    QObject::connect(&menu, &ColorMenu::colorChosen,
                     [&chosen](const QColor& color) { chosen.push_back(color); });

    // A swatch with the keys: Enter chooses its color, which the command gives next
    ColorSwatches* swatches = menu.paletteSwatches();
    swatches->setCurrentIndex(12);
    press(swatches, Qt::Key_Return);
    REQUIRE(chosen.size() == 1);
    CHECK(chosen.back() == swatches->colorAt(12));
    CHECK(menu.lastColor() == swatches->colorAt(12));

    // Automatic: no color of its own; the command keeps the last color
    menu.noColorAction()->trigger();
    REQUIRE(chosen.size() == 2);
    CHECK_FALSE(chosen.back().isValid());
    CHECK(menu.lastColor() == swatches->colorAt(12));

    // More Colors...: the color window, from the color chosen last
    std::optional<QColor> asked;
    menu.setColorAsker([&asked](const QColor& initial) -> std::optional<QColor> {
        asked = initial;
        return QColor(QStringLiteral("#123456"));
    });
    menu.moreColorsAction()->trigger();
    CHECK(asked == swatches->colorAt(12));
    REQUIRE(chosen.size() == 3);
    CHECK(chosen.back() == QColor(QStringLiteral("#123456")));

    // Cancelled: nothing chosen
    menu.setColorAsker([](const QColor&) -> std::optional<QColor> { return std::nullopt; });
    menu.moreColorsAction()->trigger();
    CHECK(chosen.size() == 3);
    forgetColorChoices();
}

TEST_CASE("Color submenu: the arrows move among the swatches and leave them at the edges",
          "[gui][format][colors]") {
    forgetColorChoices();
    const QColor marked = ColorDialog::lastPalette().colors.at(23).color;
    ColorMenu menu(ColorMenu::Kind::Text, [marked]() { return marked; });
    ColorSwatches* swatches = menu.paletteSwatches();
    const int columns = swatches->columns();

    // The swatch of the color at the cursor
    CHECK(swatches->indexOf(marked) == 23);
    swatches->setCurrentIndex(0);
    CHECK(press(swatches, Qt::Key_Right));
    CHECK(swatches->currentIndex() == 1);
    CHECK(press(swatches, Qt::Key_Down));
    CHECK(swatches->currentIndex() == 1 + columns);
    CHECK(press(swatches, Qt::Key_End));
    CHECK(swatches->currentIndex() == 2 * columns - 1);
    // At the right edge the arrow stays, the submenu stays open
    CHECK(press(swatches, Qt::Key_Right));
    CHECK(swatches->currentIndex() == 2 * columns - 1);
    CHECK(press(swatches, Qt::Key_Home));
    CHECK(swatches->currentIndex() == columns);

    // Up from the first row and down from the last go on to the menu, as Esc does, which
    // closes it; the other arrows stay in the swatches
    KeyCounter menuKeys(&menu);
    swatches->setCurrentIndex(3);
    press(swatches, Qt::Key_Up);
    CHECK(swatches->currentIndex() == 3);
    CHECK(menuKeys.keys == 1);
    swatches->setCurrentIndex(swatches->count() - 1);
    press(swatches, Qt::Key_Down);
    CHECK(menuKeys.keys == 2);
    press(swatches, Qt::Key_Escape);
    CHECK(menuKeys.keys == 3);
    press(swatches, Qt::Key_Left);
    CHECK(menuKeys.keys == 3);
    forgetColorChoices();
}

// =============================================================================
// Format > Font...
// =============================================================================

TEST_CASE("Font window: shows the formatting of the text, and gives only what changed",
          "[gui][format][dialog]") {
    forgetColorChoices();
    CharacterFormat shown = plainText();
    shown.bold = true;
    shown.size = 14.0;
    shown.color = QColor(QStringLiteral("#cc0000"));
    FontDialog dialog(shown, QStringLiteral("Mist over the river"));

    CHECK(dialog.familyField()->text() == QStringLiteral("Serif"));
    CHECK(dialog.sizeField()->text() == QStringLiteral("14"));
    CHECK(dialog.sizeList()->currentItem()->text() == QStringLiteral("14"));
    CHECK(dialog.styleField()->text() == QStringLiteral("Bold"));
    CHECK(dialog.styleList()->currentRow() == 2);
    CHECK(dialog.underlineBox()->checkState() == Qt::Unchecked);
    CHECK(dialog.colorButton()->text() == QStringLiteral("#CC0000"));
    CHECK(dialog.highlightButton()->text() == QStringLiteral("None"));
    CHECK(dialog.preview()->sample() == QStringLiteral("Mist over the river"));
    CHECK(dialog.spacingBox()->currentIndex() == 0);
    CHECK_FALSE(dialog.spacingField()->isEnabled());

    // Nothing changed: nothing to give
    CHECK(dialog.changes() == CharacterFormat{});

    // A size of the list, a style, an effect and a color
    dialog.sizeList()->setCurrentRow(static_cast<int>(FontDialog::offeredSizes().indexOf(18.0)));
    dialog.styleList()->setCurrentRow(3);
    dialog.smallCapsBox()->click();
    dialog.highlightMenu()->choose(QColor(QStringLiteral("#ffff00")));
    CharacterFormat expected;
    expected.size = 18.0;
    expected.italic = true;
    expected.capitals = QFont::SmallCaps;
    expected.highlight = QColor(QStringLiteral("#ffff00"));
    CHECK(dialog.changes() == expected);
    CHECK(dialog.sizeField()->text() == QStringLiteral("18"));
    CHECK(dialog.styleField()->text() == QStringLiteral("Bold Italic"));
    CHECK(dialog.preview()->sampleFont().capitalization() == QFont::SmallCaps);
    CHECK(dialog.preview()->sampleFont().italic());

    // All caps takes small caps off; Automatic takes the color off
    dialog.allCapsBox()->click();
    CHECK(dialog.smallCapsBox()->checkState() == Qt::Unchecked);
    dialog.colorMenu()->choose(QColor());
    CHECK(dialog.colorButton()->text() == QStringLiteral("Automatic"));
    CHECK(dialog.changes().capitals == QFont::AllUppercase);
    CHECK(dialog.changes().color == QColor());
    forgetColorChoices();
}

TEST_CASE("Font window: a part not the same in all of the selection stays as it is",
          "[gui][format][dialog]") {
    CharacterFormat shown = plainText();
    shown.family.reset();
    shown.bold.reset();
    shown.underline.reset();
    shown.position.reset();
    shown.letterSpacing.reset();
    FontDialog dialog(shown);

    // Empty fields and half checked boxes; the sample is a sentence
    CHECK(dialog.familyField()->text().isEmpty());
    CHECK(dialog.styleField()->text().isEmpty());
    CHECK(dialog.styleList()->currentRow() == -1);
    CHECK(dialog.underlineBox()->checkState() == Qt::PartiallyChecked);
    CHECK(dialog.superscriptBox()->checkState() == Qt::PartiallyChecked);
    CHECK(dialog.subscriptBox()->checkState() == Qt::PartiallyChecked);
    CHECK(dialog.spacingBox()->currentIndex() == -1);
    CHECK(dialog.preview()->sample() == QStringLiteral("The mist came down over the river"));
    CHECK(dialog.changes() == CharacterFormat{});

    // A click checks a half checked box; then it is on or off
    dialog.underlineBox()->click();
    CHECK(dialog.underlineBox()->checkState() == Qt::Checked);
    CHECK(dialog.changes().underline == true);
    dialog.underlineBox()->click();
    CHECK(dialog.underlineBox()->checkState() == Qt::Unchecked);
    CHECK(dialog.changes().underline == false);

    // Superscript checked: the subscript is off, the text raised
    dialog.superscriptBox()->click();
    CHECK(dialog.subscriptBox()->checkState() == Qt::Unchecked);
    CHECK(dialog.changes().position == QTextCharFormat::AlignSuperScript);
    dialog.superscriptBox()->click();
    CHECK(dialog.changes().position == QTextCharFormat::AlignNormal);

    // The letter spacing: expanded starts at 1 pt, condensed by what the field says
    dialog.spacingBox()->setCurrentIndex(1);
    CHECK(dialog.spacingField()->isEnabled());
    CHECK(dialog.changes().letterSpacing == 1.0);
    dialog.spacingField()->setValue(2.5);
    dialog.spacingBox()->setCurrentIndex(2);
    CHECK(dialog.changes().letterSpacing == -2.5);
    dialog.spacingBox()->setCurrentIndex(0);
    CHECK(dialog.changes().letterSpacing == 0.0);
}

TEST_CASE("Font window: a font and a size typed in their fields", "[gui][format][dialog]") {
    FontDialog dialog(plainText());
    REQUIRE(dialog.familyList()->count() > 0);
    const QString family = dialog.familyList()->item(dialog.familyList()->count() - 1)->text();

    // The start of a font's name finds it in the list; the whole name gives it
    dialog.familyField()->setText(family.left(3));
    emit dialog.familyField()->textEdited(family.left(3));
    REQUIRE(dialog.familyList()->currentItem() != nullptr);
    CHECK(dialog.familyList()->currentItem()->text().startsWith(family.left(3),
                                                                 Qt::CaseInsensitive));
    dialog.familyField()->setText(family.toLower());
    emit dialog.familyField()->textEdited(family.toLower());
    if (family != QStringLiteral("Serif")) {
        CHECK(dialog.changes().family == family);
    }

    // Any size, typed: 10.5 points; one out of range gives none
    dialog.sizeField()->setText(QLocale().toString(10.5));
    emit dialog.sizeField()->textEdited(QLocale().toString(10.5));
    CHECK(dialog.changes().size == 10.5);
    dialog.sizeField()->setText(QStringLiteral("5000"));
    emit dialog.sizeField()->textEdited(QStringLiteral("5000"));
    CHECK_FALSE(dialog.changes().size.has_value());

    // Down in the field of the sizes moves in their list
    dialog.sizeList()->setCurrentRow(0);
    press(dialog.sizeField(), Qt::Key_Down);
    CHECK(dialog.sizeList()->currentRow() == 1);
    CHECK(dialog.changes().size == FontDialog::offeredSizes().at(1));
}

TEST_CASE("Font window: fits a small screen", "[gui][format][dialog]") {
    FontDialog font(plainText());
    ParagraphDialog paragraph(plainParagraph(), Qt::AlignLeft);
    // 1366x768 at 150%: 911 pixels wide, as test_dock_layout.cpp counts, and 460 high
    // without the taskbar and the title bar
    CHECK(font.sizeHint().width() <= 911);
    CHECK(paragraph.sizeHint().width() <= 911);
    CHECK(font.sizeHint().height() <= 460);
    CHECK(paragraph.sizeHint().height() <= 460);
}

// =============================================================================
// Format > Paragraph...
// =============================================================================

TEST_CASE("Paragraph window: shows the formatting of the paragraph, and gives what changed",
          "[gui][format][dialog]") {
    ParagraphFormat shown = plainParagraph();
    shown.firstLineIndent = -18.0;
    shown.lineSpacing = LineSpacing{LineSpacingRule::AtLeast, 15.0};
    ParagraphDialog dialog(shown, Qt::AlignJustify);

    CHECK(dialog.alignmentBox()->currentText() == QStringLiteral("Justify"));
    CHECK(dialog.specialBox()->currentIndex() == ParagraphDialog::Hanging);
    CHECK(dialog.specialField()->storedValue() == 18.0);
    CHECK(dialog.afterField()->value() == 6.0);
    CHECK(dialog.lineSpacingBox()->currentIndex() == ParagraphDialog::AtLeast);
    CHECK(dialog.lineSpacingField()->value() == 15.0);
    CHECK(dialog.lineSpacingField()->isEnabled());
    CHECK(dialog.widowControlBox()->isChecked());
    CHECK_FALSE(dialog.keepWithNextBox()->isChecked());

    // Nothing changed: nothing to give
    CHECK(dialog.changes().isEmpty());
    CHECK_FALSE(dialog.alignmentChange().has_value());

    // The alignment, a first line indent, the space before, double spacing and a break
    dialog.alignmentBox()->setCurrentIndex(1);
    dialog.specialBox()->setCurrentIndex(ParagraphDialog::FirstLine);
    dialog.beforeField()->setValue(12.0);
    dialog.lineSpacingBox()->setCurrentIndex(ParagraphDialog::Double);
    dialog.pageBreakBeforeBox()->setChecked(true);
    ParagraphFormat expected;
    expected.firstLineIndent = 18.0;
    expected.spaceBefore = 12.0;
    expected.lineSpacing = LineSpacing{LineSpacingRule::Multiple, 2.0};
    expected.pageBreakBefore = true;
    CHECK(dialog.changes() == expected);
    CHECK(dialog.alignmentChange() == Qt::Alignment(Qt::AlignHCenter));
    CHECK_FALSE(dialog.lineSpacingField()->isEnabled());

    // No special indent: the first line starts with the others
    dialog.specialBox()->setCurrentIndex(ParagraphDialog::NoSpecial);
    CHECK_FALSE(dialog.specialField()->isEnabled());
    CHECK(dialog.changes().firstLineIndent == 0.0);

    // Multiple: in lines
    dialog.lineSpacingBox()->setCurrentIndex(ParagraphDialog::Multiple);
    dialog.lineSpacingField()->setValue(1.25);
    CHECK(dialog.changes().lineSpacing == LineSpacing{LineSpacingRule::Multiple, 1.25});
}

TEST_CASE("Paragraph window: a list item's indents are its level's", "[gui][format][dialog]") {
    ParagraphFormat shown = plainParagraph();
    shown.leftIndent = 36.0;
    shown.firstLineIndent = -18.0;
    ParagraphDialog dialog(shown, Qt::AlignLeft, true);
    CHECK_FALSE(dialog.leftField()->isEnabled());
    CHECK_FALSE(dialog.specialBox()->isEnabled());
    CHECK_FALSE(dialog.specialField()->isEnabled());
    CHECK(dialog.rightField()->isEnabled());

    dialog.rightField()->setStoredValue(10.0);
    ParagraphFormat expected;
    expected.rightIndent = 10.0;
    CHECK(dialog.changes() == expected);
}

TEST_CASE("Paragraph window: the preview follows the indents and the alignment",
          "[gui][format][dialog]") {
    ParagraphDialog dialog(plainParagraph(), Qt::AlignLeft);
    dialog.resize(dialog.sizeHint());
    const QList<QRectF> before = dialog.preview()->paragraphLines();
    REQUIRE(before.size() == 5);

    // A left indent moves every line, a first line indent the first one more
    dialog.leftField()->setStoredValue(72.0);
    emit dialog.leftField()->valueChanged(dialog.leftField()->value());
    dialog.specialBox()->setCurrentIndex(ParagraphDialog::FirstLine);
    const QList<QRectF> indented = dialog.preview()->paragraphLines();
    CHECK(indented.at(1).left() > before.at(1).left());
    CHECK(indented.at(0).left() > indented.at(1).left());

    // Right aligned: the lines end at the same place
    dialog.alignmentBox()->setCurrentIndex(2);
    const QList<QRectF> right = dialog.preview()->paragraphLines();
    CHECK(right.at(1).right() == right.at(4).right());
}

// =============================================================================
// The editor gets what the windows give
// =============================================================================

TEST_CASE("Editor panel: Font and Paragraph change the text at the cursor",
          "[gui][format][dialog]") {
    gui::EditorPanel panel;
    editor::BookEditor* editor = panel.getBookEditor();
    editor->fromKml(QStringLiteral("<kml><p>One two</p><p>Three</p></kml>"));
    editor->selectAll();

    // Font: bold and a size for the selection
    QTimer::singleShot(0, [&]() {
        auto* dialog = qobject_cast<FontDialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            closeShownDialog();
            return;
        }
        dialog->styleList()->setCurrentRow(2);
        dialog->sizeList()->setCurrentRow(
            static_cast<int>(FontDialog::offeredSizes().indexOf(16.0)));
        dialog->accept();
    });
    panel.showFontDialog();
    CHECK(editor->shownCharacterFormat().bold == true);
    CHECK(editor->shownCharacterFormat().size == 16.0);

    // Paragraph: the space before of the selected paragraphs
    QTimer::singleShot(0, [&]() {
        auto* dialog = qobject_cast<ParagraphDialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            closeShownDialog();
            return;
        }
        dialog->beforeField()->setValue(24.0);
        dialog->accept();
    });
    panel.showParagraphDialog();
    editor->clearSelection();
    editor->setCursorPosition({1, 0});
    CHECK(editor->shownParagraphFormat().spaceBefore == 24.0);

    // Cancel changes nothing
    QTimer::singleShot(0, [&]() {
        if (QDialog* dialog = test::shownDialog()) {
            dialog->reject();
        }
    });
    const QString kml = editor->toKml();
    panel.showParagraphDialog();
    CHECK(editor->toKml().toStdString() == kml.toStdString());
}
