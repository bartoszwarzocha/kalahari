/// @file test_kml_parser.cpp
/// @brief Unit tests for KML Parser (OpenSpec #00042 Phase 1.10)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/kml_parser.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/kml_text_run.h>
#include <kalahari/editor/kml_inline_elements.h>

using namespace kalahari::editor;

// =============================================================================
// ParseResult Tests
// =============================================================================

TEST_CASE("ParseResult success check", "[editor][kml_parser]") {
    SECTION("Successful result is truthy") {
        auto result = ParseResult<KmlDocument>::ok(std::make_unique<KmlDocument>());
        REQUIRE(result);
        REQUIRE(result.success);
        REQUIRE(result.result != nullptr);
        REQUIRE(result.errorMessage.isEmpty());
    }

    SECTION("Error result is falsy") {
        auto result = ParseResult<KmlDocument>::error("Test error", 5, 10);
        REQUIRE(!result);
        REQUIRE(!result.success);
        REQUIRE(result.result == nullptr);
        REQUIRE(result.errorMessage == "Test error");
        REQUIRE(result.errorLine == 5);
        REQUIRE(result.errorColumn == 10);
    }
}

// =============================================================================
// Empty/Trivial Input Tests
// =============================================================================

TEST_CASE("KmlParser empty input", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Empty document string returns empty document") {
        auto result = parser.parseDocument("");
        REQUIRE(result);
        REQUIRE(result.result->isEmpty());
        REQUIRE(result.result->paragraphCount() == 0);
    }

    SECTION("Empty paragraph string returns error") {
        auto result = parser.parseParagraph("");
        REQUIRE(!result);
        REQUIRE(!result.errorMessage.isEmpty());
    }

    SECTION("Empty element string returns error") {
        auto result = parser.parseElement("");
        REQUIRE(!result);
        REQUIRE(!result.errorMessage.isEmpty());
    }
}

// =============================================================================
// Document Parsing Tests
// =============================================================================

TEST_CASE("KmlParser parseDocument basic", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Single paragraph without doc wrapper") {
        auto result = parser.parseDocument("<p>Hello world</p>");
        REQUIRE(result);
        REQUIRE(result.result->paragraphCount() == 1);
        REQUIRE(result.result->paragraph(0)->plainText() == "Hello world");
    }

    SECTION("Single paragraph with doc wrapper") {
        auto result = parser.parseDocument("<doc><p>Hello world</p></doc>");
        REQUIRE(result);
        REQUIRE(result.result->paragraphCount() == 1);
        REQUIRE(result.result->paragraph(0)->plainText() == "Hello world");
    }

    SECTION("Multiple paragraphs") {
        auto result = parser.parseDocument(
            "<doc>"
            "<p>First paragraph</p>"
            "<p>Second paragraph</p>"
            "<p>Third paragraph</p>"
            "</doc>");
        REQUIRE(result);
        REQUIRE(result.result->paragraphCount() == 3);
        REQUIRE(result.result->paragraph(0)->plainText() == "First paragraph");
        REQUIRE(result.result->paragraph(1)->plainText() == "Second paragraph");
        REQUIRE(result.result->paragraph(2)->plainText() == "Third paragraph");
    }

    SECTION("Multiple paragraphs without doc wrapper") {
        auto result = parser.parseDocument(
            "<p>First</p>"
            "<p>Second</p>");
        REQUIRE(result);
        REQUIRE(result.result->paragraphCount() == 2);
    }
}

TEST_CASE("KmlParser parseDocument with styles", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Paragraph with style attribute") {
        auto result = parser.parseDocument("<p style=\"heading1\">Chapter One</p>");
        REQUIRE(result);
        REQUIRE(result.result->paragraphCount() == 1);
        REQUIRE(result.result->paragraph(0)->styleId() == "heading1");
        REQUIRE(result.result->paragraph(0)->plainText() == "Chapter One");
    }

    SECTION("Multiple paragraphs with different styles") {
        auto result = parser.parseDocument(
            "<doc>"
            "<p style=\"heading1\">Title</p>"
            "<p style=\"body\">Body text</p>"
            "<p>Default style</p>"
            "</doc>");
        REQUIRE(result);
        REQUIRE(result.result->paragraph(0)->styleId() == "heading1");
        REQUIRE(result.result->paragraph(1)->styleId() == "body");
        REQUIRE(result.result->paragraph(2)->styleId().isEmpty());
    }
}

// =============================================================================
// Paragraph Parsing Tests
// =============================================================================

TEST_CASE("KmlParser parseParagraph basic", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Simple text paragraph") {
        auto result = parser.parseParagraph("<p>Simple text</p>");
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "Simple text");
    }

    SECTION("Paragraph with style") {
        auto result = parser.parseParagraph("<p style=\"myStyle\">Styled text</p>");
        REQUIRE(result);
        REQUIRE(result.result->styleId() == "myStyle");
        REQUIRE(result.result->plainText() == "Styled text");
    }

    SECTION("Empty paragraph") {
        auto result = parser.parseParagraph("<p></p>");
        REQUIRE(result);
        REQUIRE(result.result->isEmpty());
    }

    SECTION("Wrong element type returns error") {
        auto result = parser.parseParagraph("<b>Not a paragraph</b>");
        REQUIRE(!result);
        REQUIRE(result.errorMessage.contains("<p>"));
    }
}

// =============================================================================
// Inline Element Parsing Tests
// =============================================================================

TEST_CASE("KmlParser parseElement text run", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Simple text run") {
        auto result = parser.parseElement("<t>Hello</t>");
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Text);
        REQUIRE(result.result->plainText() == "Hello");
    }

    SECTION("Text run with style") {
        auto result = parser.parseElement("<t style=\"emphasis\">Important</t>");
        REQUIRE(result);

        auto* textRun = dynamic_cast<KmlTextRun*>(result.result.get());
        REQUIRE(textRun != nullptr);
        REQUIRE(textRun->styleId() == "emphasis");
        REQUIRE(textRun->text() == "Important");
    }

    SECTION("Empty text run") {
        auto result = parser.parseElement("<t></t>");
        REQUIRE(result);
        REQUIRE(result.result->isEmpty());
    }
}

TEST_CASE("KmlParser parseElement bold", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Bold with text") {
        auto result = parser.parseElement("<b>Bold text</b>");
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Bold);
        REQUIRE(result.result->plainText() == "Bold text");
    }

    SECTION("Bold with wrapped text run") {
        auto result = parser.parseElement("<b><t>Bold text</t></b>");
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Bold);
        REQUIRE(result.result->plainText() == "Bold text");
    }

    SECTION("Empty bold") {
        auto result = parser.parseElement("<b></b>");
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Bold);
        REQUIRE(result.result->isEmpty());
    }
}

TEST_CASE("KmlParser parseElement italic", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Italic with text") {
        auto result = parser.parseElement("<i>Italic text</i>");
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Italic);
        REQUIRE(result.result->plainText() == "Italic text");
    }
}

TEST_CASE("KmlParser parseElement underline", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Underline with text") {
        auto result = parser.parseElement("<u>Underlined text</u>");
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Underline);
        REQUIRE(result.result->plainText() == "Underlined text");
    }
}

TEST_CASE("KmlParser parseElement strikethrough", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Strikethrough with text") {
        auto result = parser.parseElement("<s>Deleted text</s>");
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Strikethrough);
        REQUIRE(result.result->plainText() == "Deleted text");
    }
}

TEST_CASE("KmlParser parseElement subscript", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Subscript with text") {
        auto result = parser.parseElement("<sub>2</sub>");
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Subscript);
        REQUIRE(result.result->plainText() == "2");
    }
}

TEST_CASE("KmlParser parseElement superscript", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Superscript with text") {
        auto result = parser.parseElement("<sup>2</sup>");
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Superscript);
        REQUIRE(result.result->plainText() == "2");
    }
}

// =============================================================================
// Nested Element Tests
// =============================================================================

TEST_CASE("KmlParser nested elements", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Bold inside italic") {
        auto result = parser.parseElement("<i><b>Bold and italic</b></i>");
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Italic);
        REQUIRE(result.result->plainText() == "Bold and italic");

        auto* italic = dynamic_cast<KmlItalic*>(result.result.get());
        REQUIRE(italic != nullptr);
        REQUIRE(italic->childCount() == 1);
        REQUIRE(italic->childAt(0)->type() == ElementType::Bold);
    }

    SECTION("Three levels deep") {
        auto result = parser.parseElement("<b><i><u>Deep nesting</u></i></b>");
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Bold);
        REQUIRE(result.result->plainText() == "Deep nesting");

        auto* bold = dynamic_cast<KmlBold*>(result.result.get());
        REQUIRE(bold->childCount() == 1);

        auto* italic = dynamic_cast<KmlItalic*>(bold->childAt(0));
        REQUIRE(italic != nullptr);
        REQUIRE(italic->childCount() == 1);

        auto* underline = dynamic_cast<KmlUnderline*>(italic->childAt(0));
        REQUIRE(underline != nullptr);
        REQUIRE(underline->plainText() == "Deep nesting");
    }

    SECTION("Mixed content - text and nested elements") {
        auto result = parser.parseElement("<b>Normal <i>italic</i> bold</b>");
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "Normal italic bold");

        auto* bold = dynamic_cast<KmlBold*>(result.result.get());
        REQUIRE(bold != nullptr);
        REQUIRE(bold->childCount() == 3);

        // First child: text "Normal "
        REQUIRE(bold->childAt(0)->plainText() == "Normal ");

        // Second child: italic
        REQUIRE(bold->childAt(1)->type() == ElementType::Italic);
        REQUIRE(bold->childAt(1)->plainText() == "italic");

        // Third child: text " bold"
        REQUIRE(bold->childAt(2)->plainText() == " bold");
    }
}

// =============================================================================
// Paragraph with Inline Elements
// =============================================================================

TEST_CASE("KmlParser paragraph with inline elements", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Paragraph with bold text") {
        auto result = parser.parseParagraph("<p>Normal and <b>bold</b> text</p>");
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "Normal and bold text");
        REQUIRE(result.result->elementCount() == 3);
    }

    SECTION("Paragraph with multiple formatting") {
        auto result = parser.parseParagraph(
            "<p>Text with <b>bold</b> and <i>italic</i> formatting</p>");
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "Text with bold and italic formatting");
    }

    SECTION("Chemical formula H2O") {
        auto result = parser.parseParagraph("<p>H<sub>2</sub>O</p>");
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "H2O");
        REQUIRE(result.result->elementCount() == 3);
    }

    SECTION("Mathematical expression x^2") {
        auto result = parser.parseParagraph("<p>x<sup>2</sup></p>");
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "x2");
    }
}

// =============================================================================
// Full Document with Complex Content
// =============================================================================

TEST_CASE("KmlParser complex document", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Document with mixed content") {
        QString kml = R"(
            <doc>
                <p style="heading1">Chapter One</p>
                <p>This is a paragraph with <b>bold</b> and <i>italic</i> text.</p>
                <p>Formula: H<sub>2</sub>O and E=mc<sup>2</sup></p>
                <p style="quote"><i>A quote in italic</i></p>
            </doc>
        )";

        auto result = parser.parseDocument(kml);
        REQUIRE(result);
        REQUIRE(result.result->paragraphCount() == 4);

        // First paragraph: heading
        REQUIRE(result.result->paragraph(0)->styleId() == "heading1");
        REQUIRE(result.result->paragraph(0)->plainText() == "Chapter One");

        // Second paragraph: mixed formatting
        REQUIRE(result.result->paragraph(1)->plainText() == "This is a paragraph with bold and italic text.");

        // Third paragraph: formulas
        REQUIRE(result.result->paragraph(2)->plainText() == "Formula: H2O and E=mc2");

        // Fourth paragraph: quoted italic
        REQUIRE(result.result->paragraph(3)->styleId() == "quote");
    }
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("KmlParser error handling", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Malformed XML - unclosed tag") {
        auto result = parser.parseDocument("<p>Unclosed paragraph");
        // Should still produce partial result or error
        // Implementation specific - just verify it doesn't crash
        // and provides error info if it fails
        if (!result) {
            REQUIRE(!parser.lastError().isEmpty());
        }
    }

    SECTION("Malformed XML - mismatched tags") {
        auto result = parser.parseDocument("<p>Wrong close</b>");
        // Implementation specific - verify graceful handling
        if (!result) {
            REQUIRE(!parser.lastError().isEmpty());
        }
    }

    SECTION("Invalid XML characters") {
        // XML parser should handle or report this
        auto result = parser.parseDocument("<p>Text with \x00 null</p>");
        // Just verify no crash
    }

    SECTION("Error information is accessible") {
        auto result = parser.parseParagraph("<notparagraph>test</notparagraph>");
        REQUIRE(!result);
        REQUIRE(!parser.lastError().isEmpty());
        // lastError() should match result.errorMessage
        REQUIRE(parser.lastError() == result.errorMessage);
    }
}

// =============================================================================
// Special Character Tests
// =============================================================================

TEST_CASE("KmlParser special characters", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("XML entities") {
        auto result = parser.parseParagraph("<p>&lt;tag&gt; &amp; &quot;quotes&quot;</p>");
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "<tag> & \"quotes\"");
    }

    SECTION("Unicode characters") {
        // Polish characters: zolw (turtle)
        auto result = parser.parseParagraph(QString::fromUtf8("<p>Polski: \xC5\xBC\xC3\xB3\xC5\x82w</p>"));
        REQUIRE(result);
        REQUIRE(result.result->plainText() == QString::fromUtf8("Polski: \xC5\xBC\xC3\xB3\xC5\x82w"));
    }

    SECTION("Whitespace preservation") {
        auto result = parser.parseParagraph("<p>  spaces  and\nnewlines  </p>");
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "  spaces  and\nnewlines  ");
    }

    SECTION("Empty paragraph with whitespace") {
        auto result = parser.parseParagraph("<p>   </p>");
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "   ");
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("KmlParser edge cases", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Deeply nested same-type elements") {
        auto result = parser.parseElement("<b><b><b>Triple bold</b></b></b>");
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Bold);
        REQUIRE(result.result->plainText() == "Triple bold");
    }

    SECTION("Adjacent inline elements") {
        auto result = parser.parseParagraph("<p><b>A</b><i>B</i><u>C</u></p>");
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "ABC");
        REQUIRE(result.result->elementCount() == 3);
    }

    SECTION("Inline element with only whitespace") {
        auto result = parser.parseElement("<b>   </b>");
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "   ");
        REQUIRE(result.result->length() == 3);
    }

    SECTION("Unknown element is skipped") {
        auto result = parser.parseParagraph("<p>Text <unknown>ignored</unknown> more</p>");
        REQUIRE(result);
        // Unknown elements are skipped, but text around them should be preserved
        // The exact behavior depends on implementation
        REQUIRE(result.result->plainText().contains("Text"));
        REQUIRE(result.result->plainText().contains("more"));
    }

    SECTION("Self-closing elements") {
        // Self-closing inline elements should be handled gracefully
        auto result = parser.parseParagraph("<p>Before<b/>After</p>");
        REQUIRE(result);
        // Should not crash, behavior may vary
    }

    SECTION("Parser can be reused") {
        auto result1 = parser.parseDocument("<p>First</p>");
        REQUIRE(result1);
        REQUIRE(result1.result->paragraph(0)->plainText() == "First");

        auto result2 = parser.parseDocument("<p>Second</p>");
        REQUIRE(result2);
        REQUIRE(result2.result->paragraph(0)->plainText() == "Second");

        // Error state should be cleared between parses
        REQUIRE(parser.lastError().isEmpty());
    }
}

// =============================================================================
// Round-Trip Tests (Parse -> Serialize -> Parse)
// =============================================================================

TEST_CASE("KmlParser round-trip", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Simple paragraph round-trip") {
        QString original = "<p>Simple text</p>";
        auto result1 = parser.parseParagraph(original);
        REQUIRE(result1);

        QString serialized = result1.result->toKml();
        auto result2 = parser.parseParagraph(serialized);
        REQUIRE(result2);

        REQUIRE(result2.result->plainText() == result1.result->plainText());
    }

    SECTION("Formatted paragraph round-trip") {
        QString original = "<p>Text with <b>bold</b> formatting</p>";
        auto result1 = parser.parseParagraph(original);
        REQUIRE(result1);

        QString serialized = result1.result->toKml();
        auto result2 = parser.parseParagraph(serialized);
        REQUIRE(result2);

        REQUIRE(result2.result->plainText() == "Text with bold formatting");
    }

    SECTION("Document round-trip") {
        QString original =
            "<doc>"
            "<p style=\"heading\">Title</p>"
            "<p>Body with <i>italic</i></p>"
            "</doc>";

        auto result1 = parser.parseDocument(original);
        REQUIRE(result1);

        QString serialized = result1.result->toKml();
        auto result2 = parser.parseDocument(serialized);
        REQUIRE(result2);

        REQUIRE(result2.result->paragraphCount() == 2);
        REQUIRE(result2.result->paragraph(0)->plainText() == "Title");
        REQUIRE(result2.result->paragraph(0)->styleId() == "heading");
        REQUIRE(result2.result->paragraph(1)->plainText() == "Body with italic");
    }
}

// =============================================================================
// Performance Sanity Check
// =============================================================================

TEST_CASE("KmlParser performance sanity", "[editor][kml_parser]") {
    KmlParser parser;

    SECTION("Parse 100 paragraphs") {
        QString kml = "<doc>";
        for (int i = 0; i < 100; ++i) {
            kml += QString("<p>Paragraph %1 with <b>bold</b> text</p>").arg(i);
        }
        kml += "</doc>";

        auto result = parser.parseDocument(kml);
        REQUIRE(result);
        REQUIRE(result.result->paragraphCount() == 100);
    }

    SECTION("Parse deeply nested structure") {
        // 10 levels of nesting
        QString kml = "<b><i><u><s><b><i><u><s><b><i>Deep</i></b></s></u></i></b></s></u></i></b>";

        auto result = parser.parseElement(kml);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "Deep");
    }
}

// =============================================================================
// Comprehensive Round-Trip Tests (Phase 1.11 - KML Serializer)
// =============================================================================

TEST_CASE("KmlParser round-trip XML escaping", "[editor][kml_parser][serializer]") {
    KmlParser parser;

    SECTION("Special XML characters in text content") {
        // Create paragraph with special characters
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("Text with <tag> & \"quotes\" and 'apostrophes'"));

        QString serialized = para->toKml();
        INFO("Serialized: " << serialized.toStdString());

        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "Text with <tag> & \"quotes\" and 'apostrophes'");
    }

    SECTION("Special XML characters in style attribute") {
        // Style IDs normally don't contain special chars, but test robustness
        auto para = std::make_unique<KmlParagraph>();
        para->setStyleId("style&name\"test");
        para->addElement(std::make_unique<KmlTextRun>("Content"));

        QString serialized = para->toKml();
        INFO("Serialized: " << serialized.toStdString());

        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->styleId() == "style&name\"test");
    }

    SECTION("All XML entities in bold element") {
        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("< > & \" '"));

        QString serialized = bold->toKml();
        INFO("Serialized: " << serialized.toStdString());

        auto result = parser.parseElement(serialized);
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Bold);
        REQUIRE(result.result->plainText() == "< > & \" '");
    }

    SECTION("Ampersand edge cases") {
        // Test various ampersand patterns
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("AT&T & B&&C && &"));

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "AT&T & B&&C && &");
    }
}

TEST_CASE("KmlParser round-trip Unicode", "[editor][kml_parser][serializer]") {
    KmlParser parser;

    SECTION("Polish characters") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>(QString::fromUtf8("Polskie znaki: \xC4\x85\xC4\x87\xC4\x99\xC5\x82\xC5\x84\xC3\xB3\xC5\x9B\xC5\xBA\xC5\xBC")));

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == QString::fromUtf8("Polskie znaki: \xC4\x85\xC4\x87\xC4\x99\xC5\x82\xC5\x84\xC3\xB3\xC5\x9B\xC5\xBA\xC5\xBC"));
    }

    SECTION("Chinese characters") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>(QString::fromUtf8("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95"))); // "Chinese test" in Chinese

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == QString::fromUtf8("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95"));
    }

    SECTION("Japanese characters") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>(QString::fromUtf8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"))); // "Japanese" in Japanese

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == QString::fromUtf8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"));
    }

    SECTION("Emoji characters") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>(QString::fromUtf8("Hello \xF0\x9F\x91\x8B world \xF0\x9F\x8C\x8D"))); // Wave and Earth emojis

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == QString::fromUtf8("Hello \xF0\x9F\x91\x8B world \xF0\x9F\x8C\x8D"));
    }

    SECTION("Mixed scripts") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>(QString::fromUtf8("English, \xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9, \xCE\x95\xCE\xBB\xCE\xBB\xCE\xB7\xCE\xBD\xCE\xB9\xCE\xBA\xCE\xAC"))); // English, Russian, Greek

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == QString::fromUtf8("English, \xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9, \xCE\x95\xCE\xBB\xCE\xBB\xCE\xB7\xCE\xBD\xCE\xB9\xCE\xBA\xCE\xAC"));
    }
}

TEST_CASE("KmlParser round-trip whitespace", "[editor][kml_parser][serializer]") {
    KmlParser parser;

    SECTION("Preserve leading spaces") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("   Leading spaces"));

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "   Leading spaces");
    }

    SECTION("Preserve trailing spaces") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("Trailing spaces   "));

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "Trailing spaces   ");
    }

    SECTION("Preserve multiple internal spaces") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("Word   with   spaces"));

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "Word   with   spaces");
    }

    SECTION("Preserve tabs") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("Tab\there\tthere"));

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "Tab\there\tthere");
    }

    SECTION("Preserve newlines in text") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("Line one\nLine two\nLine three"));

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "Line one\nLine two\nLine three");
    }

    SECTION("Only whitespace content") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("   \t\n   "));

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "   \t\n   ");
    }
}

TEST_CASE("KmlParser round-trip complex structures", "[editor][kml_parser][serializer]") {
    KmlParser parser;

    SECTION("Nested formatting elements") {
        auto para = std::make_unique<KmlParagraph>();

        auto bold = std::make_unique<KmlBold>();
        auto italic = std::make_unique<KmlItalic>();
        italic->appendChild(std::make_unique<KmlTextRun>("nested text"));
        bold->appendChild(std::move(italic));
        para->addElement(std::move(bold));

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "nested text");
        REQUIRE(result.result->elementCount() == 1);
    }

    SECTION("Mixed plain text and formatting") {
        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>("Start "));

        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("bold"));
        para->addElement(std::move(bold));

        para->addElement(std::make_unique<KmlTextRun>(" middle "));

        auto italic = std::make_unique<KmlItalic>();
        italic->appendChild(std::make_unique<KmlTextRun>("italic"));
        para->addElement(std::move(italic));

        para->addElement(std::make_unique<KmlTextRun>(" end"));

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "Start bold middle italic end");
        REQUIRE(result.result->elementCount() == 5);
    }

    SECTION("Multi-paragraph document") {
        auto doc = std::make_unique<KmlDocument>();

        auto para1 = std::make_unique<KmlParagraph>("First paragraph", "heading");
        doc->addParagraph(std::move(para1));

        auto para2 = std::make_unique<KmlParagraph>();
        para2->addElement(std::make_unique<KmlTextRun>("Second with "));
        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("formatting"));
        para2->addElement(std::move(bold));
        doc->addParagraph(std::move(para2));

        auto para3 = std::make_unique<KmlParagraph>("Third paragraph");
        doc->addParagraph(std::move(para3));

        QString serialized = doc->toKml();
        auto result = parser.parseDocument(serialized);
        REQUIRE(result);
        REQUIRE(result.result->paragraphCount() == 3);
        REQUIRE(result.result->paragraph(0)->plainText() == "First paragraph");
        REQUIRE(result.result->paragraph(0)->styleId() == "heading");
        REQUIRE(result.result->paragraph(1)->plainText() == "Second with formatting");
        REQUIRE(result.result->paragraph(2)->plainText() == "Third paragraph");
    }

    SECTION("All inline element types") {
        auto para = std::make_unique<KmlParagraph>();

        auto bold = std::make_unique<KmlBold>();
        bold->appendChild(std::make_unique<KmlTextRun>("bold"));
        para->addElement(std::move(bold));

        para->addElement(std::make_unique<KmlTextRun>(" "));

        auto italic = std::make_unique<KmlItalic>();
        italic->appendChild(std::make_unique<KmlTextRun>("italic"));
        para->addElement(std::move(italic));

        para->addElement(std::make_unique<KmlTextRun>(" "));

        auto underline = std::make_unique<KmlUnderline>();
        underline->appendChild(std::make_unique<KmlTextRun>("underline"));
        para->addElement(std::move(underline));

        para->addElement(std::make_unique<KmlTextRun>(" "));

        auto strike = std::make_unique<KmlStrikethrough>();
        strike->appendChild(std::make_unique<KmlTextRun>("strike"));
        para->addElement(std::move(strike));

        para->addElement(std::make_unique<KmlTextRun>(" H"));

        auto sub = std::make_unique<KmlSubscript>();
        sub->appendChild(std::make_unique<KmlTextRun>("2"));
        para->addElement(std::move(sub));

        para->addElement(std::make_unique<KmlTextRun>("O x"));

        auto sup = std::make_unique<KmlSuperscript>();
        sup->appendChild(std::make_unique<KmlTextRun>("2"));
        para->addElement(std::move(sup));

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == "bold italic underline strike H2O x2");
    }
}

TEST_CASE("KmlParser round-trip multiple cycles", "[editor][kml_parser][serializer]") {
    KmlParser parser;

    SECTION("Three round-trips produce same result") {
        QString original = "<p style=\"heading\">Text with <b>bold</b> and <i>italic &amp; special</i></p>";

        auto result1 = parser.parseParagraph(original);
        REQUIRE(result1);
        QString plainText1 = result1.result->plainText();
        QString style1 = result1.result->styleId();

        QString serialized1 = result1.result->toKml();
        auto result2 = parser.parseParagraph(serialized1);
        REQUIRE(result2);
        REQUIRE(result2.result->plainText() == plainText1);
        REQUIRE(result2.result->styleId() == style1);

        QString serialized2 = result2.result->toKml();
        auto result3 = parser.parseParagraph(serialized2);
        REQUIRE(result3);
        REQUIRE(result3.result->plainText() == plainText1);
        REQUIRE(result3.result->styleId() == style1);

        // Serialized form should stabilize
        QString serialized3 = result3.result->toKml();
        REQUIRE(serialized2 == serialized3);
    }

    SECTION("Document round-trip stability") {
        auto doc = std::make_unique<KmlDocument>();
        auto para = std::make_unique<KmlParagraph>();
        para->setStyleId("test-style");
        para->addElement(std::make_unique<KmlTextRun>("Text with <special> & \"chars\""));
        doc->addParagraph(std::move(para));

        QString serialized1 = doc->toKml();
        auto result1 = parser.parseDocument(serialized1);
        REQUIRE(result1);

        QString serialized2 = result1.result->toKml();
        auto result2 = parser.parseDocument(serialized2);
        REQUIRE(result2);

        QString serialized3 = result2.result->toKml();

        // After normalization, serialized form should be identical
        REQUIRE(serialized2 == serialized3);
    }
}

TEST_CASE("KmlParser round-trip edge cases", "[editor][kml_parser][serializer]") {
    KmlParser parser;

    SECTION("Empty document") {
        auto doc = std::make_unique<KmlDocument>();

        QString serialized = doc->toKml();
        auto result = parser.parseDocument(serialized);
        REQUIRE(result);
        REQUIRE(result.result->isEmpty());
    }

    SECTION("Empty paragraph") {
        auto para = std::make_unique<KmlParagraph>();

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->isEmpty());
    }

    SECTION("Empty styled paragraph") {
        auto para = std::make_unique<KmlParagraph>();
        para->setStyleId("mystyle");

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->isEmpty());
        REQUIRE(result.result->styleId() == "mystyle");
    }

    SECTION("Empty bold element") {
        auto bold = std::make_unique<KmlBold>();

        QString serialized = bold->toKml();
        auto result = parser.parseElement(serialized);
        REQUIRE(result);
        REQUIRE(result.result->type() == ElementType::Bold);
        REQUIRE(result.result->isEmpty());
    }

    SECTION("Very long text") {
        QString longText;
        for (int i = 0; i < 1000; ++i) {
            longText += QString("Word%1 ").arg(i);
        }

        auto para = std::make_unique<KmlParagraph>();
        para->addElement(std::make_unique<KmlTextRun>(longText));

        QString serialized = para->toKml();
        auto result = parser.parseParagraph(serialized);
        REQUIRE(result);
        REQUIRE(result.result->plainText() == longText);
    }

    SECTION("Document with many paragraphs") {
        auto doc = std::make_unique<KmlDocument>();
        for (int i = 0; i < 50; ++i) {
            doc->addParagraph(std::make_unique<KmlParagraph>(QString("Paragraph %1").arg(i)));
        }

        QString serialized = doc->toKml();
        auto result = parser.parseDocument(serialized);
        REQUIRE(result);
        REQUIRE(result.result->paragraphCount() == 50);

        for (int i = 0; i < 50; ++i) {
            REQUIRE(result.result->paragraph(i)->plainText() == QString("Paragraph %1").arg(i));
        }
    }
}
