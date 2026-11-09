/// @file test_character_formatting.cpp
/// @brief Superscript, subscript, small and all capitals, Format > Font and the colors,
///        Change Case and Copy and Paste Formatting

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/character_format.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QImage>
#include <QKeyEvent>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;


namespace {

/// An editor with KML paragraphs loaded
std::unique_ptr<BookEditor> editorWith(const QString& paragraphs) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(QStringLiteral("<kml>") + paragraphs + QStringLiteral("</kml>"));
    return editor;
}

/// KML of paragraphs as UTF-8, for the report of a failed check
std::string kml(const QString& paragraphs) {
    return (QStringLiteral("<kml>") + paragraphs + QStringLiteral("</kml>")).toStdString();
}

/// The KML of an editor as UTF-8
std::string kmlOf(const BookEditor& editor) {
    return editor.toKml().toStdString();
}

/// Type the text as the keyboard does, a character at a time
void type(BookEditor& editor, const QString& text) {
    for (const QChar ch : text) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
        QCoreApplication::sendEvent(&editor, &event);
    }
}

}  // namespace

// =============================================================================
// Superscript, subscript, small and all capitals
// =============================================================================

TEST_CASE("Character formatting: superscript and subscript toggle, one takes the other off",
          "[editor][format]") {
    auto editor = editorWith(QStringLiteral("<p>E mc2 H2O</p>"));
    editor->setSelection({{0, 4}, {0, 5}});

    editor->toggleSuperscript();
    CHECK(kmlOf(*editor) == kml(QStringLiteral("<p>E mc<sup>2</sup> H2O</p>")));
    CHECK(editor->isSuperscript());
    CHECK_FALSE(editor->isSubscript());

    editor->toggleSubscript();
    CHECK(kmlOf(*editor) == kml(QStringLiteral("<p>E mc<sub>2</sub> H2O</p>")));
    CHECK(editor->isSubscript());
    CHECK_FALSE(editor->isSuperscript());

    editor->toggleSubscript();
    CHECK(kmlOf(*editor) == kml(QStringLiteral("<p>E mc2 H2O</p>")));

    SECTION("For the text typed next, toggled off it is normal again") {
        editor->clearSelection();
        editor->setCursorPosition({0, 9});
        editor->toggleSubscript();
        type(*editor, QStringLiteral("2"));
        editor->toggleSuperscript();
        type(*editor, QStringLiteral("3"));
        editor->toggleSuperscript();
        type(*editor, QStringLiteral("x"));
        CHECK(kmlOf(*editor) ==
              kml(QStringLiteral("<p>E mc2 H2O<sub>2</sub><sup>3</sup>x</p>")));
    }
}

TEST_CASE("Character formatting: small and all capitals show the letters, the text stays",
          "[editor][format]") {
    auto editor = editorWith(QStringLiteral("<p>Rome in 44 BC</p>"));
    editor->setSelection({{0, 11}, {0, 13}});
    editor->toggleSmallCaps();
    CHECK(kmlOf(*editor) == kml(QStringLiteral("<p>Rome in 44 <span caps=\"small\">BC</span></p>")));
    CHECK(editor->isSmallCaps());

    editor->toggleAllCaps();
    CHECK(kmlOf(*editor) == kml(QStringLiteral("<p>Rome in 44 <span caps=\"all\">BC</span></p>")));
    CHECK(editor->isAllCaps());
    CHECK_FALSE(editor->isSmallCaps());
    CHECK(editor->plainText() == QStringLiteral("Rome in 44 BC"));

    // The word with the cursor
    editor->clearSelection();
    editor->setCursorPosition({0, 2});
    editor->toggleAllCaps();
    CHECK(kmlOf(*editor) ==
          kml(QStringLiteral("<p><span caps=\"all\">Rome</span> in 44 <span caps=\"all\">BC</span></p>")));

    // Each is one undo step
    editor->undo();
    CHECK(kmlOf(*editor) == kml(QStringLiteral("<p>Rome in 44 <span caps=\"all\">BC</span></p>")));
}

// =============================================================================
// Format > Font and the colors
// =============================================================================

TEST_CASE("Character formatting: Format > Font shows what all of the selection has",
          "[editor][format]") {
    auto editor = editorWith(QStringLiteral(
        "<p><b font=\"Georgia\" size=\"14\" color=\"#aa0000\" caps=\"small\" spacing=\"1.5\">One</b>"
        "<b font=\"Georgia\" size=\"14\" color=\"#0000aa\" bg=\"#ffff00\">Two</b></p>"));

    editor->setSelection({{0, 0}, {0, 3}});
    const CharacterFormat one = editor->shownCharacterFormat();
    CHECK(one.family == QStringLiteral("Georgia"));
    CHECK(one.size == 14.0);
    CHECK(one.bold == true);
    CHECK(one.italic == false);
    CHECK(one.position == QTextCharFormat::AlignNormal);
    CHECK(one.capitals == QFont::SmallCaps);
    CHECK(one.letterSpacing == 1.5);
    CHECK(one.color == QColor(QStringLiteral("#aa0000")));
    CHECK(one.highlight == QColor());

    // A part not the same everywhere has no value
    editor->setSelection({{0, 0}, {0, 6}});
    const CharacterFormat both = editor->shownCharacterFormat();
    CHECK(both.family == QStringLiteral("Georgia"));
    CHECK(both.bold == true);
    CHECK_FALSE(both.capitals.has_value());
    CHECK_FALSE(both.letterSpacing.has_value());
    CHECK_FALSE(both.color.has_value());
    CHECK_FALSE(both.highlight.has_value());

    // Without a selection: the text typed at the cursor, the editor's font where it has none
    auto plain = editorWith(QStringLiteral("<p>Plain</p>"));
    plain->setCursorPosition({0, 5});
    const CharacterFormat typed = plain->shownCharacterFormat();
    CHECK(typed.family == plain->currentFontFamily());
    CHECK(typed.size.has_value());
    CHECK(typed.bold == false);
    CHECK(typed.color == QColor());
}

TEST_CASE("Character formatting: Format > Font gives the parts it has, in one step",
          "[editor][format]") {
    const std::string original = kml(QStringLiteral("<p>A <span color=\"#aa0000\">red</span> word</p>"));
    auto editor = editorWith(QStringLiteral("<p>A <span color=\"#aa0000\">red</span> word</p>"));
    editor->setSelection({{0, 2}, {0, 10}});

    CharacterFormat changes;
    changes.family = QStringLiteral("Georgia");
    changes.italic = true;
    changes.capitals = QFont::AllUppercase;
    changes.letterSpacing = 2.0;
    changes.highlight = QColor(QStringLiteral("#ffff00"));
    editor->applyCharacterFormat(changes);
    CHECK(kmlOf(*editor) ==
          kml(QStringLiteral("<p>A <i font=\"Georgia\" color=\"#aa0000\" bg=\"#ffff00\" caps=\"all\" "
                             "spacing=\"2\">red</i><i font=\"Georgia\" bg=\"#ffff00\" caps=\"all\" "
                             "spacing=\"2\"> word</i></p>")));

    editor->undo();
    CHECK(kmlOf(*editor) == original);
    CHECK(editor->selection().normalized().end == CursorPosition{0, 10});

    SECTION("The automatic color, no highlight and a normal spacing take them off") {
        editor->redo();
        CharacterFormat off;
        off.color = QColor();
        off.highlight = QColor();
        off.letterSpacing = 0.0;
        off.capitals = QFont::MixedCase;
        editor->applyCharacterFormat(off);
        CHECK(kmlOf(*editor) == kml(QStringLiteral("<p>A <i font=\"Georgia\">red word</i></p>")));
    }
}

TEST_CASE("Character formatting: a color for the text typed next", "[editor][format]") {
    auto editor = editorWith(QStringLiteral("<p><span color=\"#aa0000\">Red</span></p>"));
    editor->setCursorPosition({0, 3});

    CharacterFormat automatic;
    automatic.color = QColor();
    editor->applyCharacterFormat(automatic);
    type(*editor, QStringLiteral(" plain"));

    CharacterFormat blue;
    blue.color = QColor(QStringLiteral("#0000aa"));
    blue.highlight = QColor(QStringLiteral("#ffff00"));
    editor->applyCharacterFormat(blue);
    type(*editor, QStringLiteral(" blue"));

    CHECK(kmlOf(*editor) ==
          kml(QStringLiteral("<p><span color=\"#aa0000\">Red</span> plain"
                             "<span color=\"#0000aa\" bg=\"#ffff00\"> blue</span></p>")));
}

// =============================================================================
// Format > Change Case
// =============================================================================

TEST_CASE("Character formatting: the case of letters, letter for letter", "[editor][format]") {
    const auto changed = [](const QString& text, CaseChange change) {
        return changedCase(text, 0, text.size(), change);
    };
    CHECK(changed(QStringLiteral("ala MA kota. to jest! żółw? „tak.” no"), CaseChange::Sentence) ==
          QStringLiteral("Ala ma kota. To jest! Żółw? „Tak.” No"));
    CHECK(changed(QStringLiteral("ŻÓŁTA Łódź"), CaseChange::Lower) == QStringLiteral("żółta łódź"));
    CHECK(changed(QStringLiteral("żółta łódź"), CaseChange::Upper) == QStringLiteral("ŻÓŁTA ŁÓDŹ"));
    CHECK(changed(QStringLiteral("jan-paweł o’NEILL don't 2nd"), CaseChange::Capitalize) ==
          QStringLiteral("Jan-Paweł O’neill Don't 2nd"));
    CHECK(changed(QStringLiteral("Ala Ma KOTA"), CaseChange::Toggle) == QStringLiteral("aLA mA kota"));

    // Only the part; the length stays (ß has no capital of one letter)
    CHECK(changedCase(QStringLiteral("abc def"), 4, 7, CaseChange::Upper) == QStringLiteral("abc DEF"));
    CHECK(changed(QStringLiteral("straße"), CaseChange::Upper).size() == 6);
}

TEST_CASE("Character formatting: Change Case keeps the formats and annotations, in one step",
          "[editor][format]") {
    const QString content = QStringLiteral(
        "<annotations><annotation id=\"c1\" kind=\"comment\">Note</annotation></annotations>"
        "<p>the <b>QUICK</b> <anchor ref=\"c1\">brown</anchor> fox</p><p>jumps OVER</p>");
    auto editor = editorWith(content);
    editor->selectAll();

    editor->changeCase(CaseChange::Capitalize);
    CHECK(kmlOf(*editor) ==
          kml(QStringLiteral("<annotations><annotation id=\"c1\" kind=\"comment\">Note</annotation>"
                             "</annotations><p>The <b>Quick</b> <anchor ref=\"c1\">Brown</anchor> Fox</p>"
                             "<p>Jumps Over</p>")));
    CHECK(editor->hasSelection());

    editor->undo();
    CHECK(kmlOf(*editor) == kml(content));

    SECTION("Without a selection, the word with the cursor") {
        editor->clearSelection();
        editor->setCursorPosition({1, 7});
        editor->changeCase(CaseChange::Lower);
        CHECK(editor->plainText().endsWith(QStringLiteral("jumps over")));
    }
}

// =============================================================================
// Copy and Paste Formatting, Clear Formatting
// =============================================================================

TEST_CASE("Character formatting: Paste Formatting puts the copied formatting in place of the "
          "text's own",
          "[editor][format]") {
    auto editor = editorWith(QStringLiteral(
        "<p align=\"center\" space-after=\"18\"><b color=\"#aa0000\">Red</b> source</p>"
        "<p><i>Plain</i> target</p><p>Whole paragraph</p>"));
    editor->setCursorPosition({0, 1});
    editor->copyFormatting();
    REQUIRE(BookEditor::hasCopiedFormatting());

    // The word with the cursor: its italic goes, it gets bold and red; its paragraph stays
    editor->clearSelection();
    editor->setCursorPosition({1, 2});
    editor->pasteFormatting();
    CHECK(kmlOf(*editor) ==
          kml(QStringLiteral("<p align=\"center\" space-after=\"18\"><b color=\"#aa0000\">Red</b> source</p>"
                             "<p><b color=\"#aa0000\">Plain</b> target</p><p>Whole paragraph</p>")));

    // A paragraph the selection holds whole also gets the paragraph formatting
    editor->setSelection({{2, 0}, {2, 15}});
    editor->pasteFormatting();
    CHECK(kmlOf(*editor) ==
          kml(QStringLiteral("<p align=\"center\" space-after=\"18\"><b color=\"#aa0000\">Red</b> source</p>"
                             "<p><b color=\"#aa0000\">Plain</b> target</p>"
                             "<p align=\"center\" space-after=\"18\"><b color=\"#aa0000\">Whole paragraph</b></p>")));
}

TEST_CASE("Character formatting: Clear Formatting takes off the capitals, the spacing and the "
          "paragraph's own formatting",
          "[editor][format]") {
    auto editor = editorWith(QStringLiteral(
        "<p indent-left=\"36\" space-after=\"18\" line-height=\"2\" keep-with-next=\"true\" "
        "align=\"right\"><span caps=\"small\" spacing=\"2\">Small</span> text</p>"));
    editor->selectAll();
    editor->clearFormatting();
    CHECK(kmlOf(*editor) == kml(QStringLiteral("<p>Small text</p>")));

    SECTION("For the text typed next, the capitals and the spacing are off") {
        auto typed = editorWith(QStringLiteral("<p><span caps=\"all\" spacing=\"2\">Caps</span></p>"));
        typed->setCursorPosition({0, 4});
        typed->clearFormatting();
        type(*typed, QStringLiteral(" low"));
        CHECK(kmlOf(*typed) ==
              kml(QStringLiteral("<p><span caps=\"all\" spacing=\"2\">Caps</span> low</p>")));
    }
}

// =============================================================================
// Colors on dark paper
// =============================================================================

TEST_CASE("Character formatting: on dark paper colored text is lighter, highlights darker",
          "[editor][format]") {
    const QColor paper(35, 35, 40);
    const QColor text(224, 224, 224);
    CHECK(isDarkPaper(paper));
    CHECK_FALSE(isDarkPaper(QColor(255, 255, 255)));
    CHECK(contrastRatio(QColor(Qt::black), QColor(Qt::white)) > 20.9);

    // Dark red stays red, light enough to read; a light color stays as it is
    const QColor red = lightenedForDarkPaper(QColor(QStringLiteral("#8b1a1a")), paper);
    CHECK(contrastRatio(red, paper) >= MIN_TEXT_CONTRAST);
    CHECK(red.hslHue() == QColor(QStringLiteral("#8b1a1a")).hslHue());
    CHECK(lightenedForDarkPaper(QColor(QStringLiteral("#ffd966")), paper) == QColor(QStringLiteral("#ffd966")));

    // Yellow under light text is dark enough for it
    const QColor yellow = darkenedForDarkPaper(QColor(QStringLiteral("#ffff00")), text);
    CHECK(contrastRatio(text, yellow) >= MIN_TEXT_CONTRAST);
}

TEST_CASE("Character formatting: the editor draws colored text readable on dark paper",
          "[editor][format][view]") {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 300));
    EditorAppearance appearance;
    appearance.colorMode = EditorColorMode::Dark;
    appearance.cursor.blinking = false;
    editor->setAppearance(appearance);
    editor->fromKml(QStringLiteral("<kml><p><span color=\"#550000\" size=\"30\">"
                                   "MMMMMMMMMM</span></p></kml>"));

    QImage image(editor->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    editor->render(&image);

    // The letters are drawn lighter than the saved color: some pixel is light and red
    bool lightRed = false;
    for (int y = 0; y < image.height() && !lightRed; ++y) {
        for (int x = 0; x < image.width() && !lightRed; ++x) {
            const QColor pixel = image.pixelColor(x, y);
            lightRed = pixel.red() > 150 && pixel.red() > pixel.green() + 60;
        }
    }
    CHECK(lightRed);
    CHECK(editor->toKml().contains(QStringLiteral("color=\"#550000\"")));
}
