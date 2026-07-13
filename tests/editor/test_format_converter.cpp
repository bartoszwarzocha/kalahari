/// @file test_format_converter.cpp
/// @brief Unit tests for FormatConverter (Sub-Project C WS4.2)
///
/// FormatConverter is a pure static utility bridging KML inline elements
/// (bold, italic, sub/superscript, ...) to Qt QTextCharFormat / FormatRanges.
/// These tests are deterministic and require no database. A QApplication is
/// provided by the test harness (tests/test_main.cpp) for QFont/QTextCharFormat.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <kalahari/editor/format_converter.h>
#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/kml_text_run.h>
#include <kalahari/editor/kml_inline_elements.h>

#include <QFont>
#include <QTextCharFormat>

#include <memory>

using Catch::Approx;
using namespace kalahari::editor;

namespace {

/// @brief Build a fresh base font shared by the format tests
QFont makeBaseFont()
{
    QFont font("Arial", 12);
    return font;
}

}  // namespace

// =============================================================================
// elementTypeToFormat - single attribute mapping
// =============================================================================

TEST_CASE("FormatConverter maps element types to char formats", "[editor][format][converter]") {
    const QFont base = makeBaseFont();

    SECTION("Bold sets font weight to Bold") {
        QTextCharFormat fmt = FormatConverter::elementTypeToFormat(ElementType::Bold, base);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
    }

    SECTION("Italic sets font italic") {
        QTextCharFormat fmt = FormatConverter::elementTypeToFormat(ElementType::Italic, base);
        REQUIRE(fmt.fontItalic());
    }

    SECTION("Underline sets font underline") {
        QTextCharFormat fmt = FormatConverter::elementTypeToFormat(ElementType::Underline, base);
        REQUIRE(fmt.fontUnderline());
    }

    SECTION("Strikethrough sets font strikeout") {
        QTextCharFormat fmt = FormatConverter::elementTypeToFormat(ElementType::Strikethrough, base);
        REQUIRE(fmt.fontStrikeOut());
    }

    SECTION("Subscript lowers baseline and reduces point size") {
        QTextCharFormat fmt = FormatConverter::elementTypeToFormat(ElementType::Subscript, base);
        REQUIRE(fmt.verticalAlignment() == QTextCharFormat::AlignSubScript);
        // SCRIPT_SIZE_FACTOR == 0.7, base point size is 12
        REQUIRE(fmt.fontPointSize() == Approx(12.0 * 0.7));
    }

    SECTION("Superscript raises baseline and reduces point size") {
        QTextCharFormat fmt = FormatConverter::elementTypeToFormat(ElementType::Superscript, base);
        REQUIRE(fmt.verticalAlignment() == QTextCharFormat::AlignSuperScript);
        REQUIRE(fmt.fontPointSize() == Approx(12.0 * 0.7));
    }

    SECTION("Non-size attributes do not embed the base font (actual behavior)") {
        // NOTE (API deviation from design doc case 7): elementTypeToFormat does
        // NOT copy the base font family/size into the returned format for
        // Bold/Italic/etc. Only the specific attribute is set; the base font is
        // consulted only for sub/superscript size calculations. A Bold format
        // therefore carries the weight but leaves family/point-size unset.
        QTextCharFormat fmt = FormatConverter::elementTypeToFormat(ElementType::Bold, base);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
        REQUIRE_FALSE(fmt.hasProperty(QTextFormat::FontFamilies));
        REQUIRE(fmt.fontPointSize() == Approx(0.0));  // point size unset
    }

    SECTION("Text/Link/CharacterStyle produce an empty format") {
        QTextCharFormat fmt = FormatConverter::elementTypeToFormat(ElementType::Text, base);
        REQUIRE_FALSE(fmt.fontItalic());
        REQUIRE_FALSE(fmt.fontUnderline());
        REQUIRE(fmt.properties().isEmpty());
    }
}

// =============================================================================
// combineFormats / applyElementType - accumulation
// =============================================================================

TEST_CASE("FormatConverter combines multiple element types", "[editor][format][converter]") {
    const QFont base = makeBaseFont();

    SECTION("combineFormats([Bold, Italic]) sets both") {
        QTextCharFormat fmt = FormatConverter::combineFormats(
            {ElementType::Bold, ElementType::Italic}, base);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
        REQUIRE(fmt.fontItalic());
    }

    SECTION("applyElementType stacks onto an existing format") {
        QTextCharFormat fmt;
        FormatConverter::applyElementType(fmt, ElementType::Bold, base);
        FormatConverter::applyElementType(fmt, ElementType::Italic, base);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
        REQUIRE(fmt.fontItalic());
    }

    SECTION("combineFormats of an empty list yields an empty format") {
        QTextCharFormat fmt = FormatConverter::combineFormats({}, base);
        REQUIRE(fmt.properties().isEmpty());
    }
}

// =============================================================================
// buildFormatRanges - paragraph traversal
// =============================================================================

TEST_CASE("FormatConverter builds format ranges from paragraphs", "[editor][format][converter]") {
    const QFont base = makeBaseFont();

    SECTION("Plain paragraph produces no ranges (only styled runs emit ranges)") {
        // A plain text run has no active formatting types, so buildFormatRanges
        // emits nothing. This documents the actual behavior.
        KmlParagraph paragraph("hello world");
        auto ranges = FormatConverter::buildFormatRanges(paragraph, base);
        REQUIRE(ranges.isEmpty());
    }

    SECTION("Single bold run in the middle: one range covering the bold text") {
        // Represents "a <b>bold</b> c"
        KmlParagraph paragraph;
        paragraph.addElement(std::make_unique<KmlTextRun>("a "));

        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("bold"));
        paragraph.addElement(std::move(bold));

        paragraph.addElement(std::make_unique<KmlTextRun>(" c"));

        auto ranges = FormatConverter::buildFormatRanges(paragraph, base);
        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 2);   // after "a "
        REQUIRE(ranges[0].length == 4);  // "bold"
        REQUIRE(ranges[0].format.fontWeight() == QFont::Bold);
    }

    SECTION("Nested <b><i>x</i></b> produces bold+italic over the inner text") {
        KmlParagraph paragraph;
        auto bold = std::make_unique<KmlBold>();
        auto italic = std::make_unique<KmlItalic>();
        italic->appendChild(std::make_unique<KmlTextRun>("x"));
        bold->appendChild(std::move(italic));
        paragraph.addElement(std::move(bold));

        auto ranges = FormatConverter::buildFormatRanges(paragraph, base);
        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 0);
        REQUIRE(ranges[0].length == 1);
        REQUIRE(ranges[0].format.fontWeight() == QFont::Bold);
        REQUIRE(ranges[0].format.fontItalic());
    }

    SECTION("Sibling <b>a</b><i>b</i> produces two non-overlapping ranges") {
        KmlParagraph paragraph;

        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("a"));
        paragraph.addElement(std::move(bold));

        auto italic = std::make_unique<KmlItalic>();
        italic->appendChild(std::make_unique<KmlTextRun>("b"));
        paragraph.addElement(std::move(italic));

        auto ranges = FormatConverter::buildFormatRanges(paragraph, base);
        REQUIRE(ranges.size() == 2);

        REQUIRE(ranges[0].start == 0);
        REQUIRE(ranges[0].length == 1);
        REQUIRE(ranges[0].format.fontWeight() == QFont::Bold);

        REQUIRE(ranges[1].start == 1);
        REQUIRE(ranges[1].length == 1);
        REQUIRE(ranges[1].format.fontItalic());

        // Ranges do not overlap
        REQUIRE(ranges[0].start + ranges[0].length <= ranges[1].start);
    }

    SECTION("Range offsets stay within the paragraph plain text length") {
        KmlParagraph paragraph;
        paragraph.addElement(std::make_unique<KmlTextRun>("prefix "));
        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("mid"));
        paragraph.addElement(std::move(bold));
        paragraph.addElement(std::make_unique<KmlTextRun>(" suffix"));

        const int textLen = paragraph.plainText().length();
        auto ranges = FormatConverter::buildFormatRanges(paragraph, base);
        REQUIRE_FALSE(ranges.isEmpty());
        for (const auto& range : ranges) {
            REQUIRE(range.start >= 0);
            REQUIRE(range.start + range.length <= textLen);
        }
    }
}
