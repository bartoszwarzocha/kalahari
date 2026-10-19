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

    SECTION("Annotations left out, the text they are anchored to kept") {
        QString result = ClipboardHandler::kmlToText(
            "<kml><annotations><annotation id=\"a\" kind=\"note\">Hidden</annotation></annotations>"
            "<p>One <anchor ref=\"a\">two</anchor></p></kml>");
        REQUIRE(result == "One two");
    }
}

TEST_CASE("ClipboardHandler htmlToKml conversion", "[editor][clipboard]") {
    SECTION("Empty HTML returns empty KML") {
        QString result = ClipboardHandler::htmlToKml("");
        REQUIRE(result.isEmpty());
    }

    SECTION("Bold tag converted") {
        QString result = ClipboardHandler::htmlToKml("<b>Bold</b>");
        REQUIRE(result == "<kml><p><b>Bold</b></p></kml>");
    }

    SECTION("Strong tag converted to bold") {
        QString result = ClipboardHandler::htmlToKml("<strong>Bold</strong>");
        REQUIRE(result == "<kml><p><b>Bold</b></p></kml>");
    }

    SECTION("Italic tag converted") {
        QString result = ClipboardHandler::htmlToKml("<i>Italic</i>");
        REQUIRE(result == "<kml><p><i>Italic</i></p></kml>");
    }

    SECTION("Em tag converted to italic") {
        QString result = ClipboardHandler::htmlToKml("<em>Italic</em>");
        REQUIRE(result == "<kml><p><i>Italic</i></p></kml>");
    }

    SECTION("Underline tag converted") {
        QString result = ClipboardHandler::htmlToKml("<u>Underlined</u>");
        REQUIRE(result == "<kml><p><u>Underlined</u></p></kml>");
    }

    SECTION("Strike tag converted") {
        QString result = ClipboardHandler::htmlToKml("<s>Strikethrough</s>");
        REQUIRE(result == "<kml><p><s>Strikethrough</s></p></kml>");
    }

    SECTION("HTML entities decoded, and escaped again in the KML") {
        QString result = ClipboardHandler::htmlToKml("&lt;test&gt; &amp; &quot;quote&quot;");
        REQUIRE(ClipboardHandler::kmlToText(result) == "<test> & \"quote\"");
        REQUIRE(result.contains("&lt;test&gt; &amp;"));
    }

    SECTION("Plain text wrapped in paragraph") {
        QString result = ClipboardHandler::htmlToKml("Plain text");
        REQUIRE(result == "<kml><p>Plain text</p></kml>");
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

TEST_CASE("ClipboardHandler: line breaks in a paragraph", "[editor][clipboard][linebreak]") {
    const QChar lineBreak(QChar::LineSeparator);

    SECTION("textToKml: U+2028 and Word's vertical tab are <br/>, a line end a paragraph") {
        CHECK(ClipboardHandler::textToKml(QStringLiteral("a") + lineBreak +
                                          QStringLiteral("b\vc\nd")) ==
              QStringLiteral("<p><text>a<br/>b<br/>c</text></p><p><text>d</text></p>"));
    }

    SECTION("kmlToHtml: <br/>, and a no-break space for the line after a final break") {
        CHECK(ClipboardHandler::kmlToHtml(QStringLiteral("<kml><p>a<br/>b</p></kml>")) ==
              QStringLiteral("<p>a<br/>b</p>"));
        CHECK(ClipboardHandler::kmlToHtml(QStringLiteral("<kml><p><b>a<br/></b></p></kml>")) ==
              QStringLiteral("<p><b>a<br/></b>\u00A0</p>"));
        CHECK(ClipboardHandler::kmlToHtml(QStringLiteral("<kml><p><br/></p></kml>")) ==
              QStringLiteral("<p><br/>\u00A0</p>"));
    }

    SECTION("kmlToText: a line end") {
        CHECK(ClipboardHandler::kmlToText(QStringLiteral("<p>a<br/>b</p><p>c</p>")) ==
              QStringLiteral("a\nb\nc"));
    }
}

TEST_CASE("ClipboardHandler: the line ends of a text are read off its HTML",
          "[editor][clipboard][linebreak]") {
    const QChar lineBreak(QChar::LineSeparator);
    const auto withBreaks = [](const char* text, const char* html) {
        return ClipboardHandler::withLineEndsOf(QString::fromUtf8(text), QString::fromUtf8(html));
    };

    SECTION("A line end at <br> is a line break, one at a paragraph's end stays") {
        CHECK(withBreaks("a\nb\nc", "<p>a<br>b</p><p>c</p>") ==
              QStringLiteral("a") + lineBreak + QStringLiteral("b\nc"));
        CHECK(withBreaks("a\r\nb", "<p>a<br>b</p>") ==
              QStringLiteral("a") + lineBreak + QStringLiteral("b"));
        CHECK(withBreaks("a\n\nb", "<p>a<br><br>b</p>") ==
              QStringLiteral("a") + lineBreak + lineBreak + QStringLiteral("b"));
        // A break that ends a paragraph is the paragraph's end
        CHECK(withBreaks("a\n\nb", "<p>a<br></p><p>b</p>") == QStringLiteral("a\n\nb"));
    }

    SECTION("A browser's blank line between two paragraphs goes, an empty paragraph stays") {
        // The browsers leave a blank line between two paragraphs, and after a heading
        CHECK(withBreaks("a\n\nb", "<p>a</p><p>b</p>") == QStringLiteral("a\nb"));
        CHECK(withBreaks("Title\n\na\n\nb", "<h1>Title</h1><p>a</p><p>b</p>") ==
              QStringLiteral("Title\na\nb"));
        CHECK(withBreaks("a\r\n\r\nb", "<p>a</p><p>b</p>") == QStringLiteral("a\r\nb"));
        CHECK(withBreaks("a\nb\n\nc", "<p>a<br>b</p><p>c</p>") ==
              QStringLiteral("a") + lineBreak + QStringLiteral("b\nc"));
        // The white space that starts the next paragraph stays with it
        CHECK(withBreaks("a\n\n  b", "<p>a</p><p>&nbsp; b</p>") == QStringLiteral("a\n  b"));
        // An empty paragraph of the HTML stays
        CHECK(withBreaks("a\n\n \n\nb", "<p>a</p><p>&nbsp;</p><p>b</p>") ==
              QStringLiteral("a\n\nb"));
        CHECK(withBreaks("a\r\n\r\nb", "<p>a</p><p>&nbsp;</p><p>b</p>") ==
              QStringLiteral("a\r\n\r\nb"));
        // The line ends that start or end the text are not between two paragraphs
        CHECK(withBreaks("\n\na\n\n", "<p>a</p>") == QStringLiteral("\n\na\n\n"));
    }

    SECTION("White space, images and invisible characters are passed over") {
        CHECK(withBreaks("copy right\nnext", "<p>co&shy;py&nbsp;right <img src=\"x.png\"><br>\n"
                                              "next</p>") ==
              QStringLiteral("copy right") + lineBreak + QStringLiteral("next"));
        CHECK(withBreaks("a\tb\nc", "<table><tr><td>a</td><td>b</td></tr></table><p>c</p>") ==
              QStringLiteral("a\tb\nc"));
    }

    SECTION("Without HTML, without a line end, or with another text: the text as it is") {
        CHECK(withBreaks("a\nb", "") == QStringLiteral("a\nb"));
        CHECK(withBreaks("a b", "<p>a<br>b</p>") == QStringLiteral("a b"));
        CHECK(withBreaks("a\nc", "<p>a<br>b</p>") == QStringLiteral("a\nc"));
        CHECK(withBreaks("a\nb\nextra", "<p>a<br>b</p>") == QStringLiteral("a\nb\nextra"));
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
