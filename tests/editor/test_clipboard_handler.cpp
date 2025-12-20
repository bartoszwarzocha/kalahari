/// @file test_clipboard_handler.cpp
/// @brief Unit tests for ClipboardHandler (OpenSpec #00042 Phase 4.13-4.16)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/kml_paragraph.h>
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <memory>

using namespace kalahari::editor;

// =============================================================================
// Format Conversion Tests
// =============================================================================

TEST_CASE("ClipboardHandler textToKml conversion", "[editor][clipboard]") {
    SECTION("Empty text returns empty KML") {
        QString result = ClipboardHandler::textToKml("");
        REQUIRE(result.isEmpty());
    }

    SECTION("Single line creates single paragraph") {
        QString result = ClipboardHandler::textToKml("Hello World");
        REQUIRE(result.contains("<p>"));
        REQUIRE(result.contains("</p>"));
        REQUIRE(result.contains("Hello World"));
    }

    SECTION("Multiple lines create multiple paragraphs") {
        QString result = ClipboardHandler::textToKml("Line 1\nLine 2\nLine 3");
        // Count paragraph tags
        int pCount = result.count("<p>");
        REQUIRE(pCount == 3);
        REQUIRE(result.contains("Line 1"));
        REQUIRE(result.contains("Line 2"));
        REQUIRE(result.contains("Line 3"));
    }

    SECTION("Special characters are escaped") {
        QString result = ClipboardHandler::textToKml("<tag> & \"text\"");
        // XML special chars should be escaped in the text element
        REQUIRE(result.contains("<p>"));
        REQUIRE(!result.contains("<<tag>"));  // Should be escaped
    }
}

TEST_CASE("ClipboardHandler kmlToText conversion", "[editor][clipboard]") {
    SECTION("Empty KML returns empty text") {
        QString result = ClipboardHandler::kmlToText("");
        REQUIRE(result.isEmpty());
    }

    SECTION("Simple paragraph extracts text") {
        QString result = ClipboardHandler::kmlToText("<p><text>Hello</text></p>");
        REQUIRE(result == "Hello");
    }

    SECTION("Multiple paragraphs joined with newlines") {
        QString result = ClipboardHandler::kmlToText("<p><text>Line 1</text></p><p><text>Line 2</text></p>");
        REQUIRE(result == "Line 1\nLine 2");
    }

    SECTION("Formatting tags stripped") {
        QString result = ClipboardHandler::kmlToText("<p><text>Hello </text><bold><text>World</text></bold></p>");
        REQUIRE(result == "Hello World");
    }

    SECTION("Line breaks converted to newlines") {
        QString result = ClipboardHandler::kmlToText("<p><text>Line 1</text><br/><text>Line 2</text></p>");
        REQUIRE(result.contains("\n"));
    }
}

TEST_CASE("ClipboardHandler htmlToKml conversion", "[editor][clipboard]") {
    SECTION("Empty HTML returns empty KML") {
        QString result = ClipboardHandler::htmlToKml("");
        REQUIRE(result.isEmpty());
    }

    SECTION("Bold tag converted") {
        QString result = ClipboardHandler::htmlToKml("<b>Bold</b>");
        REQUIRE(result.contains("<bold>"));
        REQUIRE(result.contains("</bold>"));
    }

    SECTION("Strong tag converted to bold") {
        QString result = ClipboardHandler::htmlToKml("<strong>Bold</strong>");
        REQUIRE(result.contains("<bold>"));
        REQUIRE(result.contains("</bold>"));
    }

    SECTION("Italic tag converted") {
        QString result = ClipboardHandler::htmlToKml("<i>Italic</i>");
        REQUIRE(result.contains("<italic>"));
        REQUIRE(result.contains("</italic>"));
    }

    SECTION("Em tag converted to italic") {
        QString result = ClipboardHandler::htmlToKml("<em>Italic</em>");
        REQUIRE(result.contains("<italic>"));
        REQUIRE(result.contains("</italic>"));
    }

    SECTION("Underline tag converted") {
        QString result = ClipboardHandler::htmlToKml("<u>Underlined</u>");
        REQUIRE(result.contains("<underline>"));
        REQUIRE(result.contains("</underline>"));
    }

    SECTION("Strike tag converted") {
        QString result = ClipboardHandler::htmlToKml("<s>Strikethrough</s>");
        REQUIRE(result.contains("<strike>"));
        REQUIRE(result.contains("</strike>"));
    }

    SECTION("HTML entities decoded") {
        QString result = ClipboardHandler::htmlToKml("&lt;test&gt; &amp; &quot;quote&quot;");
        REQUIRE(result.contains("<test>"));
        REQUIRE(result.contains("&"));
        REQUIRE(result.contains("\"quote\""));
    }

    SECTION("Plain text wrapped in paragraph") {
        QString result = ClipboardHandler::htmlToKml("Plain text");
        REQUIRE(result.startsWith("<p>"));
        REQUIRE(result.endsWith("</p>"));
    }
}

TEST_CASE("ClipboardHandler kmlToHtml conversion", "[editor][clipboard]") {
    SECTION("Empty KML returns empty HTML") {
        QString result = ClipboardHandler::kmlToHtml("");
        REQUIRE(result.isEmpty());
    }

    SECTION("Bold tag converted") {
        QString result = ClipboardHandler::kmlToHtml("<bold><text>Bold</text></bold>");
        REQUIRE(result.contains("<b>"));
        REQUIRE(result.contains("</b>"));
    }

    SECTION("Italic tag converted") {
        QString result = ClipboardHandler::kmlToHtml("<italic><text>Italic</text></italic>");
        REQUIRE(result.contains("<i>"));
        REQUIRE(result.contains("</i>"));
    }

    SECTION("Underline tag converted") {
        QString result = ClipboardHandler::kmlToHtml("<underline><text>Under</text></underline>");
        REQUIRE(result.contains("<u>"));
        REQUIRE(result.contains("</u>"));
    }

    SECTION("Paragraph tag preserved") {
        QString result = ClipboardHandler::kmlToHtml("<p><text>Para</text></p>");
        REQUIRE(result.contains("<p>"));
        REQUIRE(result.contains("</p>"));
    }
}

// =============================================================================
// Selection Extraction Tests
// =============================================================================

TEST_CASE("ClipboardHandler extractText", "[editor][clipboard]") {
    auto doc = std::make_unique<KmlDocument>();
    doc->addParagraph(std::make_unique<KmlParagraph>("First paragraph"));
    doc->addParagraph(std::make_unique<KmlParagraph>("Second paragraph"));
    doc->addParagraph(std::make_unique<KmlParagraph>("Third paragraph"));

    SECTION("Single paragraph partial selection") {
        SelectionRange range{{0, 0}, {0, 5}};  // "First"
        QString text = ClipboardHandler::extractText(doc.get(), range);
        REQUIRE(text == "First");
    }

    SECTION("Single paragraph full selection") {
        SelectionRange range{{0, 0}, {0, 15}};  // "First paragraph"
        QString text = ClipboardHandler::extractText(doc.get(), range);
        REQUIRE(text == "First paragraph");
    }

    SECTION("Multi-paragraph selection") {
        SelectionRange range{{0, 6}, {1, 6}};  // "paragraph" + newline + "Second"
        QString text = ClipboardHandler::extractText(doc.get(), range);
        REQUIRE(text == "paragraph\nSecond");
    }

    SECTION("Empty selection returns empty string") {
        SelectionRange range{{0, 5}, {0, 5}};
        QString text = ClipboardHandler::extractText(doc.get(), range);
        REQUIRE(text.isEmpty());
    }

    SECTION("Null document returns empty string") {
        SelectionRange range{{0, 0}, {0, 5}};
        QString text = ClipboardHandler::extractText(nullptr, range);
        REQUIRE(text.isEmpty());
    }
}

TEST_CASE("ClipboardHandler extractKml", "[editor][clipboard]") {
    auto doc = std::make_unique<KmlDocument>();
    doc->addParagraph(std::make_unique<KmlParagraph>("Hello World"));

    SECTION("Extracts content as KML") {
        SelectionRange range{{0, 0}, {0, 5}};  // "Hello"
        QString kml = ClipboardHandler::extractKml(doc.get(), range);
        REQUIRE(kml.contains("<p>"));
        REQUIRE(kml.contains("Hello"));
    }

    SECTION("Empty selection returns empty KML") {
        SelectionRange range{{0, 3}, {0, 3}};
        QString kml = ClipboardHandler::extractKml(doc.get(), range);
        REQUIRE(kml.isEmpty());
    }
}

// =============================================================================
// MIME Data Creation Tests
// =============================================================================

TEST_CASE("ClipboardHandler createMimeData", "[editor][clipboard]") {
    auto doc = std::make_unique<KmlDocument>();
    doc->addParagraph(std::make_unique<KmlParagraph>("Test content"));

    SECTION("Creates MIME data with all formats") {
        SelectionRange range{{0, 0}, {0, 4}};  // "Test"
        auto mimeData = ClipboardHandler::createMimeData(doc.get(), range);

        REQUIRE(mimeData != nullptr);
        REQUIRE(mimeData->hasFormat(MIME_KML));
        REQUIRE(mimeData->hasHtml());
        REQUIRE(mimeData->hasText());
    }

    SECTION("Plain text format contains selection") {
        SelectionRange range{{0, 0}, {0, 4}};
        auto mimeData = ClipboardHandler::createMimeData(doc.get(), range);

        REQUIRE(mimeData->text() == "Test");
    }

    SECTION("Empty selection returns nullptr") {
        SelectionRange range{{0, 5}, {0, 5}};
        auto mimeData = ClipboardHandler::createMimeData(doc.get(), range);

        REQUIRE(mimeData == nullptr);
    }

    SECTION("Null document returns nullptr") {
        SelectionRange range{{0, 0}, {0, 5}};
        auto mimeData = ClipboardHandler::createMimeData(nullptr, range);

        REQUIRE(mimeData == nullptr);
    }
}

// =============================================================================
// Clipboard Operations Tests (require QApplication)
// =============================================================================

TEST_CASE("ClipboardHandler copy and paste roundtrip", "[editor][clipboard]") {
    // These tests require a QApplication instance
    if (QGuiApplication::instance() == nullptr) {
        SKIP("QApplication not available");
    }

    auto doc = std::make_unique<KmlDocument>();
    doc->addParagraph(std::make_unique<KmlParagraph>("Copy this text"));

    SECTION("Copy sets clipboard text") {
        SelectionRange range{{0, 0}, {0, 4}};  // "Copy"
        bool result = ClipboardHandler::copy(doc.get(), range);
        REQUIRE(result == true);

        QString pasted = ClipboardHandler::pasteAsText();
        REQUIRE(pasted == "Copy");
    }

    SECTION("canPaste returns true after copy") {
        SelectionRange range{{0, 0}, {0, 4}};
        ClipboardHandler::copy(doc.get(), range);

        REQUIRE(ClipboardHandler::canPaste() == true);
    }

    SECTION("Copy with empty selection returns false") {
        SelectionRange range{{0, 5}, {0, 5}};
        bool result = ClipboardHandler::copy(doc.get(), range);
        REQUIRE(result == false);
    }
}
