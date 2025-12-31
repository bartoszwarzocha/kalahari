/// @file test_clipboard_handler.cpp
/// @brief Unit tests for ClipboardHandler (OpenSpec #00042 Phase 4.13-4.16)
/// Phase 11: Updated to test only format conversion utilities

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/clipboard_handler.h>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QClipboard>
#include <QMimeData>

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
// Clipboard Paste Tests (require QApplication)
// =============================================================================

/// @brief Helper to check if clipboard is functional in current environment
/// @return true if clipboard operations work, false otherwise (e.g., headless)
static bool isClipboardFunctional() {
    if (QGuiApplication::instance() == nullptr) {
        return false;
    }
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        return false;
    }
    // Try a roundtrip test to verify clipboard actually works
    const QString testMarker = "__kalahari_clipboard_test__";
    clipboard->setText(testMarker);
    QCoreApplication::processEvents();
    bool works = (clipboard->text() == testMarker);
    if (works) {
        clipboard->clear();
        QCoreApplication::processEvents();
    }
    return works;
}

TEST_CASE("ClipboardHandler paste operations", "[editor][clipboard]") {
    // These tests require a QApplication instance
    if (QGuiApplication::instance() == nullptr) {
        SKIP("QApplication not available");
    }

    SECTION("canPaste returns true when clipboard has text") {
        if (!isClipboardFunctional()) {
            SKIP("Clipboard not functional in headless environment");
        }

        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText("Test text");
        QCoreApplication::processEvents();

        REQUIRE(ClipboardHandler::canPaste() == true);
    }

    SECTION("pasteAsText returns clipboard text") {
        if (!isClipboardFunctional()) {
            SKIP("Clipboard not functional in headless environment");
        }

        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText("Hello World");
        QCoreApplication::processEvents();

        QString result = ClipboardHandler::pasteAsText();
        REQUIRE(result == "Hello World");
    }

    SECTION("pasteAsKml converts text to KML") {
        if (!isClipboardFunctional()) {
            SKIP("Clipboard not functional in headless environment");
        }

        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText("Plain text");
        QCoreApplication::processEvents();

        QString result = ClipboardHandler::pasteAsKml();
        REQUIRE(result.contains("<p>"));
        REQUIRE(result.contains("Plain text"));
    }
}
