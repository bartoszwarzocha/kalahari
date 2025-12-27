/// @file test_kml_converter.cpp
/// @brief Unit tests for KmlConverter (OpenSpec #00043 Phase 7)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/kml_converter.h>
#include <kalahari/editor/text_buffer.h>
#include <kalahari/editor/format_layer.h>

using namespace kalahari::editor;

// =============================================================================
// Basic Parsing Tests
// =============================================================================

TEST_CASE("KmlConverter - Basic Parsing", "[kml_converter]") {
    KmlConverter converter;

    SECTION("Empty input") {
        auto result = converter.parseKml("");
        REQUIRE(result.success);
        // TextBuffer always has at least 1 paragraph (like a text editor)
        REQUIRE(result.buffer->paragraphCount() == 1);
        REQUIRE(result.buffer->paragraphText(0).isEmpty());
    }

    SECTION("Single paragraph plain text") {
        auto result = converter.parseKml("<p>Hello world</p>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphCount() == 1);
        REQUIRE(result.buffer->paragraphText(0) == "Hello world");
    }

    SECTION("Multiple paragraphs") {
        auto result = converter.parseKml("<p>First</p><p>Second</p><p>Third</p>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphCount() == 3);
        REQUIRE(result.buffer->paragraphText(0) == "First");
        REQUIRE(result.buffer->paragraphText(1) == "Second");
        REQUIRE(result.buffer->paragraphText(2) == "Third");
    }

    SECTION("KML root element") {
        auto result = converter.parseKml("<kml><p>Content</p></kml>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphCount() == 1);
        REQUIRE(result.buffer->paragraphText(0) == "Content");
    }

    SECTION("Document root element") {
        auto result = converter.parseKml("<document><p>Content</p></document>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphCount() == 1);
    }

    SECTION("Doc root element") {
        auto result = converter.parseKml("<doc><p>Content</p></doc>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphCount() == 1);
    }
}

// =============================================================================
// Format Parsing Tests
// =============================================================================

TEST_CASE("KmlConverter - Format Parsing", "[kml_converter]") {
    KmlConverter converter;

    SECTION("Bold formatting") {
        auto result = converter.parseKml("<p><b>bold text</b></p>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphText(0) == "bold text");

        // Check format layer
        auto formats = result.formatLayer->getFormatsAt(0);
        REQUIRE(formats.size() == 1);
        REQUIRE(formats[0].format.hasFlag(FormatType::Bold));
    }

    SECTION("Bold with long tag") {
        auto result = converter.parseKml("<p><bold>bold text</bold></p>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphText(0) == "bold text");

        auto formats = result.formatLayer->getFormatsAt(0);
        REQUIRE(formats.size() == 1);
        REQUIRE(formats[0].format.hasFlag(FormatType::Bold));
    }

    SECTION("Italic formatting") {
        auto result = converter.parseKml("<p><i>italic text</i></p>");
        REQUIRE(result.success);

        auto formats = result.formatLayer->getFormatsAt(0);
        REQUIRE(formats.size() == 1);
        REQUIRE(formats[0].format.hasFlag(FormatType::Italic));
    }

    SECTION("Underline formatting") {
        auto result = converter.parseKml("<p><u>underlined</u></p>");
        REQUIRE(result.success);

        auto formats = result.formatLayer->getFormatsAt(0);
        REQUIRE(formats.size() == 1);
        REQUIRE(formats[0].format.hasFlag(FormatType::Underline));
    }

    SECTION("Strikethrough formatting") {
        auto result = converter.parseKml("<p><s>struck</s></p>");
        REQUIRE(result.success);

        auto formats = result.formatLayer->getFormatsAt(0);
        REQUIRE(formats.size() == 1);
        REQUIRE(formats[0].format.hasFlag(FormatType::Strikethrough));
    }

    SECTION("Subscript formatting") {
        auto result = converter.parseKml("<p>H<sub>2</sub>O</p>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphText(0) == "H2O");

        auto formats = result.formatLayer->getFormatsAt(1);  // '2' position
        REQUIRE(formats.size() == 1);
        REQUIRE(formats[0].format.hasFlag(FormatType::Subscript));
    }

    SECTION("Superscript formatting") {
        auto result = converter.parseKml("<p>x<sup>2</sup></p>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphText(0) == "x2");

        auto formats = result.formatLayer->getFormatsAt(1);  // '2' position
        REQUIRE(formats.size() == 1);
        REQUIRE(formats[0].format.hasFlag(FormatType::Superscript));
    }

    SECTION("Mixed formatting") {
        auto result = converter.parseKml("<p>normal <b>bold</b> normal</p>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphText(0) == "normal bold normal");

        // 'n' at position 0 - no format
        REQUIRE(result.formatLayer->getFormatsAt(0).empty());

        // 'b' at position 7 - bold
        auto formats = result.formatLayer->getFormatsAt(7);
        REQUIRE(formats.size() == 1);
        REQUIRE(formats[0].format.hasFlag(FormatType::Bold));

        // ' ' at position 11 - no format
        REQUIRE(result.formatLayer->getFormatsAt(12).empty());
    }
}

// =============================================================================
// Nested Format Parsing Tests
// =============================================================================

TEST_CASE("KmlConverter - Nested Formats", "[kml_converter]") {
    KmlConverter converter;

    SECTION("Bold and italic nested") {
        auto result = converter.parseKml("<p><b><i>bold italic</i></b></p>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphText(0) == "bold italic");

        auto formats = result.formatLayer->getFormatsAt(0);
        // Should have both bold and italic
        bool hasBold = false, hasItalic = false;
        for (const auto& f : formats) {
            if (f.format.hasFlag(FormatType::Bold)) hasBold = true;
            if (f.format.hasFlag(FormatType::Italic)) hasItalic = true;
        }
        REQUIRE(hasBold);
        REQUIRE(hasItalic);
    }

    SECTION("Partially overlapping formats") {
        auto result = converter.parseKml("<p><b>bold <i>both</i></b><i> italic</i></p>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphText(0) == "bold both italic");

        // "bold " - only bold
        REQUIRE(result.formatLayer->hasFormatAt(0, FormatType::Bold));
        REQUIRE_FALSE(result.formatLayer->hasFormatAt(0, FormatType::Italic));

        // "both" - bold and italic
        REQUIRE(result.formatLayer->hasFormatAt(5, FormatType::Bold));
        REQUIRE(result.formatLayer->hasFormatAt(5, FormatType::Italic));

        // " italic" - only italic
        REQUIRE_FALSE(result.formatLayer->hasFormatAt(10, FormatType::Bold));
        REQUIRE(result.formatLayer->hasFormatAt(10, FormatType::Italic));
    }
}

// =============================================================================
// Serialization Tests
// =============================================================================

TEST_CASE("KmlConverter - Serialization", "[kml_converter]") {
    KmlConverter converter;

    SECTION("Plain text serialization") {
        TextBuffer buffer;
        buffer.setPlainText("Hello world");
        FormatLayer formatLayer;

        QString kml = converter.toKml(buffer, formatLayer);
        REQUIRE(kml.contains("<kml>"));
        REQUIRE(kml.contains("<p>"));
        REQUIRE(kml.contains("Hello world"));
        REQUIRE(kml.contains("</p>"));
        REQUIRE(kml.contains("</kml>"));
    }

    SECTION("Multiple paragraphs serialization") {
        TextBuffer buffer;
        buffer.setPlainText("First\nSecond\nThird");
        FormatLayer formatLayer;

        QString kml = converter.toKml(buffer, formatLayer);

        // Count <p> occurrences
        int pCount = kml.count("<p>");
        REQUIRE(pCount == 3);
    }

    SECTION("Bold format serialization") {
        TextBuffer buffer;
        buffer.setPlainText("Hello world");
        FormatLayer formatLayer;

        TextFormat bold;
        bold.setBold();
        formatLayer.addFormat(0, 5, bold);  // "Hello" is bold

        QString kml = converter.toKml(buffer, formatLayer);
        REQUIRE(kml.contains("<b>"));
        REQUIRE(kml.contains("</b>"));
    }

    SECTION("Multiple formats serialization") {
        TextBuffer buffer;
        buffer.setPlainText("text");
        FormatLayer formatLayer;

        TextFormat bold;
        bold.setBold();
        formatLayer.addFormat(0, 4, bold);

        TextFormat italic;
        italic.setItalic();
        formatLayer.addFormat(0, 4, italic);

        QString kml = converter.toKml(buffer, formatLayer);
        REQUIRE(kml.contains("<b>"));
        REQUIRE(kml.contains("<i>"));
    }
}

// =============================================================================
// Round-Trip Tests
// =============================================================================

TEST_CASE("KmlConverter - Round Trip", "[kml_converter]") {
    KmlConverter converter;

    SECTION("Plain text round trip") {
        QString original = "<kml><p>Hello world</p></kml>";

        auto result = converter.parseKml(original);
        REQUIRE(result.success);

        QString serialized = converter.toKml(*result.buffer, *result.formatLayer);

        // Parse again
        auto result2 = converter.parseKml(serialized);
        REQUIRE(result2.success);

        // Compare
        REQUIRE(result.buffer->paragraphCount() == result2.buffer->paragraphCount());
        REQUIRE(result.buffer->paragraphText(0) == result2.buffer->paragraphText(0));
    }

    SECTION("Formatted text round trip") {
        QString original = "<kml><p><b>Bold</b> and <i>italic</i></p></kml>";

        auto result = converter.parseKml(original);
        REQUIRE(result.success);

        QString serialized = converter.toKml(*result.buffer, *result.formatLayer);

        auto result2 = converter.parseKml(serialized);
        REQUIRE(result2.success);

        // Check text
        REQUIRE(result.buffer->paragraphText(0) == result2.buffer->paragraphText(0));

        // Check formats
        REQUIRE(result.formatLayer->hasFormatAt(0, FormatType::Bold));
        REQUIRE(result2.formatLayer->hasFormatAt(0, FormatType::Bold));
    }

    SECTION("Complex format round trip") {
        QString original = "<kml><p>H<sub>2</sub>O is water</p></kml>";

        auto result = converter.parseKml(original);
        REQUIRE(result.success);

        QString serialized = converter.toKml(*result.buffer, *result.formatLayer);

        auto result2 = converter.parseKml(serialized);
        REQUIRE(result2.success);

        REQUIRE(result.buffer->paragraphText(0) == "H2O is water");
        REQUIRE(result2.buffer->paragraphText(0) == "H2O is water");
        REQUIRE(result2.formatLayer->hasFormatAt(1, FormatType::Subscript));
    }
}

// =============================================================================
// Metadata Tests
// =============================================================================

TEST_CASE("KmlConverter - Metadata", "[kml_converter]") {
    KmlConverter converter;

    SECTION("Parse comments") {
        QString kml = R"(<kml>
            <p>Hello world</p>
            <comments>
                <comment start="0" end="5" author="Test" id="c1">Comment text</comment>
            </comments>
        </kml>)";

        auto result = converter.parseKml(kml);
        REQUIRE(result.success);
        REQUIRE(result.metadataLayer != nullptr);

        auto comments = result.metadataLayer->allComments();
        REQUIRE(comments.size() == 1);
        REQUIRE(comments[0].anchorStart == 0);
        REQUIRE(comments[0].anchorEnd == 5);
        REQUIRE(comments[0].author == "Test");
        REQUIRE(comments[0].text == "Comment text");
    }

    SECTION("Serialize comments") {
        TextBuffer buffer;
        buffer.setPlainText("Hello world");
        FormatLayer formatLayer;
        MetadataLayer metadata;

        TextComment comment;
        comment.anchorStart = 0;
        comment.anchorEnd = 5;
        comment.author = "Author";
        comment.text = "A comment";
        comment.id = "c1";
        metadata.addComment(comment);

        QString kml = converter.toKml(buffer, formatLayer, &metadata);
        REQUIRE(kml.contains("<comments>"));
        REQUIRE(kml.contains("<comment"));
        REQUIRE(kml.contains("A comment"));
    }
}

// =============================================================================
// MetadataLayer Tests
// =============================================================================

TEST_CASE("MetadataLayer - Operations", "[kml_converter]") {
    MetadataLayer metadata;

    SECTION("Add and get comments") {
        TextComment c1;
        c1.anchorStart = 10;
        c1.anchorEnd = 20;
        c1.id = "c1";
        metadata.addComment(c1);

        auto comments = metadata.getCommentsAt(15);
        REQUIRE(comments.size() == 1);
        REQUIRE(comments[0].id == "c1");
    }

    SECTION("Comments in range") {
        TextComment c1;
        c1.anchorStart = 10;
        c1.anchorEnd = 20;
        c1.id = "c1";
        metadata.addComment(c1);

        TextComment c2;
        c2.anchorStart = 50;
        c2.anchorEnd = 60;
        c2.id = "c2";
        metadata.addComment(c2);

        auto inRange = metadata.getCommentsInRange(5, 25);
        REQUIRE(inRange.size() == 1);
        REQUIRE(inRange[0].id == "c1");
    }

    SECTION("Remove comment") {
        TextComment c1;
        c1.id = "c1";
        metadata.addComment(c1);

        REQUIRE(metadata.allComments().size() == 1);

        metadata.removeComment("c1");
        REQUIRE(metadata.allComments().empty());
    }

    SECTION("Text insert shifts comments") {
        TextComment c1;
        c1.anchorStart = 10;
        c1.anchorEnd = 20;
        c1.id = "c1";
        metadata.addComment(c1);

        // Insert 5 chars at position 5
        metadata.onTextInserted(5, 5);

        auto comments = metadata.allComments();
        REQUIRE(comments.size() == 1);
        REQUIRE(comments[0].anchorStart == 15);
        REQUIRE(comments[0].anchorEnd == 25);
    }

    SECTION("Text delete shrinks comments") {
        TextComment c1;
        c1.anchorStart = 10;
        c1.anchorEnd = 20;
        c1.id = "c1";
        metadata.addComment(c1);

        // Delete 5 chars starting at position 5
        metadata.onTextDeleted(5, 5);

        auto comments = metadata.allComments();
        REQUIRE(comments.size() == 1);
        REQUIRE(comments[0].anchorStart == 5);
        REQUIRE(comments[0].anchorEnd == 15);
    }
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("KmlConverter - Error Handling", "[kml_converter]") {
    KmlConverter converter;

    SECTION("Malformed XML") {
        auto result = converter.parseKml("<p>Unclosed tag");
        REQUIRE_FALSE(result.success);
        REQUIRE_FALSE(result.errorMessage.isEmpty());
    }

    SECTION("Mismatched tags") {
        auto result = converter.parseKml("<p><b>Text</i></p>");
        // This may or may not error depending on XML parser strictness
        // At minimum it should not crash
        // The parser might try to recover
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("KmlConverter - Edge Cases", "[kml_converter]") {
    KmlConverter converter;

    SECTION("Empty paragraph") {
        auto result = converter.parseKml("<p></p>");
        REQUIRE(result.success);
        // Empty paragraph is valid
    }

    SECTION("Whitespace preservation") {
        auto result = converter.parseKml("<p>  spaces  </p>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphText(0) == "  spaces  ");
    }

    SECTION("Special characters") {
        auto result = converter.parseKml("<p>&lt;test&gt; &amp; more</p>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphText(0) == "<test> & more");
    }

    SECTION("Unicode text") {
        auto result = converter.parseKml(u8"<p>Zażółć gęślą jaźń</p>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphText(0) == u8"Zażółć gęślą jaźń");
    }

    SECTION("Text run element") {
        auto result = converter.parseKml("<p><t>Text run</t></p>");
        REQUIRE(result.success);
        REQUIRE(result.buffer->paragraphText(0) == "Text run");
    }
}
