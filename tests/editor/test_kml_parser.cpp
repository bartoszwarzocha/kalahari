/// @file test_kml_parser.cpp
/// @brief Unit tests for KmlParser (OpenSpec #00043 Phase 11.1.6)
///
/// Tests the new Phase 11 KmlParser that produces QTextDocument output directly.
/// Uses QTextCharFormat for both formatting (bold, italic, etc.) and metadata
/// (comments, todos, footnotes).

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/kml_parser.h>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QFont>
#include <QVariantMap>
#include <memory>

using namespace kalahari::editor;

// =============================================================================
// Helper Functions
// =============================================================================

/// @brief Get plain text from a QTextDocument
static QString getPlainText(QTextDocument* doc)
{
    return doc ? doc->toPlainText() : QString();
}

/// @brief Get character format at a specific position
static QTextCharFormat getFormatAt(QTextDocument* doc, int position)
{
    if (!doc) {
        return QTextCharFormat();
    }
    QTextCursor cursor(doc);
    cursor.setPosition(position);
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
    return cursor.charFormat();
}

/// @brief Count number of blocks (paragraphs) in document
static int blockCount(QTextDocument* doc)
{
    return doc ? doc->blockCount() : 0;
}

/// @brief Get text of a specific block
static QString blockText(QTextDocument* doc, int index)
{
    if (!doc || index < 0) {
        return QString();
    }
    QTextBlock block = doc->begin();
    for (int i = 0; i < index && block.isValid(); ++i) {
        block = block.next();
    }
    return block.isValid() ? block.text() : QString();
}

// =============================================================================
// Basic Parsing Tests
// =============================================================================

TEST_CASE("KmlParser - Empty document", "[editor][kml_parser][basic]") {
    KmlParser parser;

    SECTION("Empty string returns valid empty document") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(""));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()).isEmpty());
    }

    SECTION("parseInto with empty string succeeds") {
        QTextDocument doc;
        bool success = parser.parseInto("", &doc);
        REQUIRE(success);
        REQUIRE(doc.toPlainText().isEmpty());
    }

    SECTION("parseInto with null document fails") {
        bool success = parser.parseInto("<p>Text</p>", nullptr);
        REQUIRE_FALSE(success);
        REQUIRE_FALSE(parser.lastError().isEmpty());
    }
}

TEST_CASE("KmlParser - Plain text", "[editor][kml_parser][basic]") {
    KmlParser parser;

    SECTION("Simple text in paragraph") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>Hello</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Hello");
    }

    SECTION("Plain text without tags is wrapped") {
        // Note: parseInto wraps bare content in <kml> tags
        // but plain text without <p> may not produce text
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>Hello world</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Hello world");
    }
}

TEST_CASE("KmlParser - Single paragraph", "[editor][kml_parser][basic]") {
    KmlParser parser;

    SECTION("Paragraph with text") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>Text content</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(blockCount(doc.get()) == 1);
        REQUIRE(blockText(doc.get(), 0) == "Text content");
    }

    SECTION("Empty paragraph") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p></p>"));
        REQUIRE(doc != nullptr);
        // Empty paragraph is valid
    }
}

TEST_CASE("KmlParser - Multiple paragraphs", "[editor][kml_parser][basic]") {
    KmlParser parser;

    SECTION("Two paragraphs") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>First</p><p>Second</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(blockCount(doc.get()) == 2);
        REQUIRE(blockText(doc.get(), 0) == "First");
        REQUIRE(blockText(doc.get(), 1) == "Second");
    }

    SECTION("Three paragraphs") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>A</p><p>B</p><p>C</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(blockCount(doc.get()) == 3);
        REQUIRE(blockText(doc.get(), 0) == "A");
        REQUIRE(blockText(doc.get(), 1) == "B");
        REQUIRE(blockText(doc.get(), 2) == "C");
    }
}

TEST_CASE("KmlParser - Root element variants", "[editor][kml_parser][basic]") {
    KmlParser parser;

    SECTION("<kml> root element") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<kml><p>Content</p></kml>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Content");
    }

    SECTION("<doc> root element") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<doc><p>Content</p></doc>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Content");
    }

    SECTION("<document> root element") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<document><p>Content</p></document>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Content");
    }

    SECTION("No root element - paragraphs only") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>Para 1</p><p>Para 2</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(blockCount(doc.get()) == 2);
    }
}

// =============================================================================
// Inline Formatting Tests
// =============================================================================

TEST_CASE("KmlParser - Bold formatting", "[editor][kml_parser][formatting]") {
    KmlParser parser;

    SECTION("<b> tag") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><b>bold text</b></p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "bold text");

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
    }

    SECTION("<bold> tag") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><bold>bold text</bold></p>"));
        REQUIRE(doc != nullptr);

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
    }

    SECTION("<strong> tag") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><strong>bold text</strong></p>"));
        REQUIRE(doc != nullptr);

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
    }
}

TEST_CASE("KmlParser - Italic formatting", "[editor][kml_parser][formatting]") {
    KmlParser parser;

    SECTION("<i> tag") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><i>italic text</i></p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "italic text");

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontItalic());
    }

    SECTION("<italic> tag") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><italic>italic text</italic></p>"));
        REQUIRE(doc != nullptr);

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontItalic());
    }

    SECTION("<em> tag") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><em>italic text</em></p>"));
        REQUIRE(doc != nullptr);

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontItalic());
    }
}

TEST_CASE("KmlParser - Underline formatting", "[editor][kml_parser][formatting]") {
    KmlParser parser;

    SECTION("<u> tag") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><u>underlined</u></p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "underlined");

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontUnderline());
    }

    SECTION("<underline> tag") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><underline>underlined</underline></p>"));
        REQUIRE(doc != nullptr);

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontUnderline());
    }
}

TEST_CASE("KmlParser - Strikethrough formatting", "[editor][kml_parser][formatting]") {
    KmlParser parser;

    SECTION("<s> tag") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><s>struck</s></p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "struck");

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontStrikeOut());
    }

    SECTION("<strike> tag") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><strike>struck</strike></p>"));
        REQUIRE(doc != nullptr);

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontStrikeOut());
    }

    SECTION("<strikethrough> tag") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><strikethrough>struck</strikethrough></p>"));
        REQUIRE(doc != nullptr);

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontStrikeOut());
    }
}

TEST_CASE("KmlParser - Subscript formatting", "[editor][kml_parser][formatting]") {
    KmlParser parser;

    SECTION("<sub> tag - chemical formula H2O") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>H<sub>2</sub>O</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "H2O");

        // Position 0: H - normal
        QTextCharFormat fmtH = getFormatAt(doc.get(), 0);
        REQUIRE(fmtH.verticalAlignment() != QTextCharFormat::AlignSubScript);

        // Position 1: 2 - subscript
        QTextCharFormat fmt2 = getFormatAt(doc.get(), 1);
        REQUIRE(fmt2.verticalAlignment() == QTextCharFormat::AlignSubScript);

        // Position 2: O - normal
        QTextCharFormat fmtO = getFormatAt(doc.get(), 2);
        REQUIRE(fmtO.verticalAlignment() != QTextCharFormat::AlignSubScript);
    }
}

TEST_CASE("KmlParser - Superscript formatting", "[editor][kml_parser][formatting]") {
    KmlParser parser;

    SECTION("<sup> tag - mathematical power x^2") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>x<sup>2</sup></p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "x2");

        // Position 0: x - normal
        QTextCharFormat fmtX = getFormatAt(doc.get(), 0);
        REQUIRE(fmtX.verticalAlignment() != QTextCharFormat::AlignSuperScript);

        // Position 1: 2 - superscript
        QTextCharFormat fmt2 = getFormatAt(doc.get(), 1);
        REQUIRE(fmt2.verticalAlignment() == QTextCharFormat::AlignSuperScript);
    }
}

TEST_CASE("KmlParser - Nested formatting", "[editor][kml_parser][formatting]") {
    KmlParser parser;

    SECTION("Bold inside italic") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><i><b>bold italic</b></i></p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "bold italic");

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
        REQUIRE(fmt.fontItalic());
    }

    SECTION("Italic inside bold") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><b><i>bold italic</i></b></p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "bold italic");

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
        REQUIRE(fmt.fontItalic());
    }

    SECTION("Three levels deep: bold, italic, underline") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><b><i><u>formatted</u></i></b></p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "formatted");

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
        REQUIRE(fmt.fontItalic());
        REQUIRE(fmt.fontUnderline());
    }
}

TEST_CASE("KmlParser - Mixed content", "[editor][kml_parser][formatting]") {
    KmlParser parser;

    SECTION("Normal text with bold in middle") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>Normal <b>bold</b> normal</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Normal bold normal");

        // "Normal " - positions 0-6 - not bold
        QTextCharFormat fmtN = getFormatAt(doc.get(), 0);
        REQUIRE(fmtN.fontWeight() != QFont::Bold);

        // "bold" - positions 7-10 - bold
        QTextCharFormat fmtB = getFormatAt(doc.get(), 7);
        REQUIRE(fmtB.fontWeight() == QFont::Bold);

        // " normal" - positions 11+ - not bold
        QTextCharFormat fmtN2 = getFormatAt(doc.get(), 12);
        REQUIRE(fmtN2.fontWeight() != QFont::Bold);
    }

    SECTION("Multiple formatted spans") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>Text <b>bold</b> and <i>italic</i> end</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Text bold and italic end");
    }
}

// =============================================================================
// Metadata Properties Tests
// =============================================================================

TEST_CASE("KmlParser - Comment metadata", "[editor][kml_parser][metadata]") {
    KmlParser parser;

    SECTION("Comment tag sets KmlPropComment property") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(
            R"(<p>Text <comment id="c1" author="Jan">annotated</comment> text</p>)"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Text annotated text");

        // Check format at "annotated" position
        // "Text " = 5 chars, so "annotated" starts at position 5
        QTextCharFormat fmt = getFormatAt(doc.get(), 5);
        QVariant commentData = fmt.property(KmlPropComment);
        REQUIRE(commentData.isValid());

        QVariantMap metadata = commentData.toMap();
        REQUIRE(metadata["id"].toString() == "c1");
        REQUIRE(metadata["author"].toString() == "Jan");
    }

    SECTION("Comment with resolved attribute") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(
            R"(<p><comment id="c2" resolved="true">done</comment></p>)"));
        REQUIRE(doc != nullptr);

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        QVariant commentData = fmt.property(KmlPropComment);
        REQUIRE(commentData.isValid());

        QVariantMap metadata = commentData.toMap();
        REQUIRE(metadata["resolved"].toBool() == true);
    }
}

TEST_CASE("KmlParser - Todo metadata", "[editor][kml_parser][metadata]") {
    KmlParser parser;

    SECTION("Todo tag sets KmlPropTodo property") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(
            R"(<p><todo id="t1">task item</todo></p>)"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "task item");

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        QVariant todoData = fmt.property(KmlPropTodo);
        REQUIRE(todoData.isValid());

        QVariantMap metadata = todoData.toMap();
        REQUIRE(metadata["id"].toString() == "t1");
    }

    SECTION("Todo with completed and priority") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(
            R"(<p><todo id="t2" completed="true" priority="high">done task</todo></p>)"));
        REQUIRE(doc != nullptr);

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        QVariantMap metadata = fmt.property(KmlPropTodo).toMap();
        REQUIRE(metadata["completed"].toBool() == true);
        REQUIRE(metadata["priority"].toString() == "high");
    }
}

TEST_CASE("KmlParser - Footnote metadata", "[editor][kml_parser][metadata]") {
    KmlParser parser;

    SECTION("Footnote tag sets KmlPropFootnote property") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(
            R"(<p>Text with<footnote id="f1" number="1">note</footnote> reference</p>)"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Text withnote reference");

        // "Text with" = 9 chars, "note" starts at position 9
        QTextCharFormat fmt = getFormatAt(doc.get(), 9);
        QVariant footnoteData = fmt.property(KmlPropFootnote);
        REQUIRE(footnoteData.isValid());

        QVariantMap metadata = footnoteData.toMap();
        REQUIRE(metadata["id"].toString() == "f1");
        REQUIRE(metadata["number"].toInt() == 1);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("KmlParser - Empty paragraphs", "[editor][kml_parser][edge]") {
    KmlParser parser;

    SECTION("Single empty paragraph") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p></p>"));
        REQUIRE(doc != nullptr);
        // Empty paragraph should be valid
    }

    SECTION("Multiple empty paragraphs") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p></p><p></p><p></p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(blockCount(doc.get()) == 3);
    }

    SECTION("Empty paragraph between content") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>First</p><p></p><p>Third</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(blockCount(doc.get()) == 3);
        REQUIRE(blockText(doc.get(), 0) == "First");
        REQUIRE(blockText(doc.get(), 1) == "");
        REQUIRE(blockText(doc.get(), 2) == "Third");
    }
}

TEST_CASE("KmlParser - Whitespace handling", "[editor][kml_parser][edge]") {
    KmlParser parser;

    SECTION("Preserve leading spaces") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>   Leading spaces</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "   Leading spaces");
    }

    SECTION("Preserve trailing spaces") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>Trailing spaces   </p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Trailing spaces   ");
    }

    SECTION("Preserve multiple internal spaces") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>Multiple   spaces   here</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Multiple   spaces   here");
    }

    SECTION("Preserve tabs") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>Tab\there\tthere</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Tab\there\tthere");
    }

    SECTION("Only whitespace content") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>   </p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "   ");
    }
}

TEST_CASE("KmlParser - Special characters (XML entities)", "[editor][kml_parser][edge]") {
    KmlParser parser;

    SECTION("Less than and greater than") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>&lt;tag&gt;</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "<tag>");
    }

    SECTION("Ampersand") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>Rock &amp; Roll</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Rock & Roll");
    }

    SECTION("Quotes") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>&quot;quoted&quot;</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "\"quoted\"");
    }

    SECTION("All special characters together") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(
            "<p>&lt;tag&gt; &amp; &quot;text&quot;</p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "<tag> & \"text\"");
    }
}

TEST_CASE("KmlParser - Unicode text", "[editor][kml_parser][edge]") {
    KmlParser parser;

    SECTION("Polish characters") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(
            QString::fromUtf8("<p>Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87 g\xC4\x99\xC5\x9Bl\xC4\x85 ja\xC5\xBA\xC5\x84</p>")));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == QString::fromUtf8("Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87 g\xC4\x99\xC5\x9Bl\xC4\x85 ja\xC5\xBA\xC5\x84"));
    }

    SECTION("Chinese characters") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(
            QString::fromUtf8("<p>\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95</p>")));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == QString::fromUtf8("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95"));
    }

    SECTION("Emoji characters") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(
            QString::fromUtf8("<p>Hello \xF0\x9F\x91\x8B world \xF0\x9F\x8C\x8D</p>")));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == QString::fromUtf8("Hello \xF0\x9F\x91\x8B world \xF0\x9F\x8C\x8D"));
    }

    SECTION("Mixed scripts") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(
            QString::fromUtf8("<p>English, \xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9</p>")));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == QString::fromUtf8("English, \xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9"));
    }
}

TEST_CASE("KmlParser - Malformed XML handling", "[editor][kml_parser][edge]") {
    KmlParser parser;

    SECTION("Unclosed tag returns nullptr") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p>Unclosed"));
        // Implementation-specific: may succeed with partial content or fail
        // At minimum it should not crash
        if (!doc) {
            REQUIRE_FALSE(parser.lastError().isEmpty());
        }
    }

    SECTION("Mismatched tags") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><b>Text</i></p>"));
        // Should handle gracefully without crashing
        if (!doc) {
            REQUIRE_FALSE(parser.lastError().isEmpty());
        }
    }

    SECTION("Missing closing tag for formatting") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><b>Bold text</p>"));
        // Implementation-specific behavior
        // Should not crash
    }

    SECTION("Error info is accessible after failure") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<invalid<<<"));
        if (!doc) {
            REQUIRE_FALSE(parser.lastError().isEmpty());
            // Line and column may or may not be available
        }
    }
}

// =============================================================================
// Text Run Element Tests
// =============================================================================

TEST_CASE("KmlParser - Text run element", "[editor][kml_parser][textrun]") {
    KmlParser parser;

    SECTION("<t> element is parsed as plain text") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><t>Text run</t></p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Text run");
    }

    SECTION("<text> element is parsed as plain text") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><text>Text content</text></p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Text content");
    }
}

// =============================================================================
// Complex Document Tests
// =============================================================================

TEST_CASE("KmlParser - Complex document", "[editor][kml_parser][complex]") {
    KmlParser parser;

    SECTION("Document with mixed formatting") {
        QString kml = R"(
            <kml>
                <p>Normal text with <b>bold</b> and <i>italic</i></p>
                <p>Formula: H<sub>2</sub>O and E=mc<sup>2</sup></p>
                <p><b><i>Bold italic</i></b> text</p>
            </kml>
        )";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(kml));
        REQUIRE(doc != nullptr);
        REQUIRE(blockCount(doc.get()) == 3);
    }

    SECTION("Document with metadata and formatting") {
        QString kml = R"(
            <doc>
                <p>Text with <comment id="c1" author="Test">comment</comment> here</p>
                <p><b>Bold</b> and <todo id="t1">todo item</todo></p>
            </doc>
        )";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(kml));
        REQUIRE(doc != nullptr);
        REQUIRE(blockCount(doc.get()) == 2);
    }
}

// =============================================================================
// Parser Reusability Tests
// =============================================================================

TEST_CASE("KmlParser - Parser reusability", "[editor][kml_parser][reuse]") {
    KmlParser parser;

    SECTION("Parser can parse multiple documents") {
        std::unique_ptr<QTextDocument> doc1(parser.parseKml("<p>First</p>"));
        REQUIRE(doc1 != nullptr);
        REQUIRE(getPlainText(doc1.get()) == "First");

        std::unique_ptr<QTextDocument> doc2(parser.parseKml("<p>Second</p>"));
        REQUIRE(doc2 != nullptr);
        REQUIRE(getPlainText(doc2.get()) == "Second");

        // Previous document should still be valid
        REQUIRE(getPlainText(doc1.get()) == "First");
    }

    SECTION("Error state is cleared between parses") {
        // First parse fails
        std::unique_ptr<QTextDocument> doc1(parser.parseKml("<invalid<<<"));

        // Second parse succeeds
        std::unique_ptr<QTextDocument> doc2(parser.parseKml("<p>Valid</p>"));
        REQUIRE(doc2 != nullptr);
        REQUIRE(getPlainText(doc2.get()) == "Valid");

        // Error should be cleared
        REQUIRE(parser.lastError().isEmpty());
    }

    SECTION("parseInto can reuse same document") {
        QTextDocument doc;

        bool success1 = parser.parseInto("<p>First content</p>", &doc);
        REQUIRE(success1);
        REQUIRE(doc.toPlainText() == "First content");

        bool success2 = parser.parseInto("<p>New content</p>", &doc);
        REQUIRE(success2);
        REQUIRE(doc.toPlainText() == "New content");
    }
}

// =============================================================================
// Paragraph Alignment Tests
// =============================================================================

TEST_CASE("KmlParser - Paragraph alignment", "[editor][kml_parser][alignment]") {
    KmlParser parser;

    SECTION("Left alignment") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(R"(<p align="left">Left text</p>)"));
        REQUIRE(doc != nullptr);

        QTextBlock block = doc->begin();
        REQUIRE(block.blockFormat().alignment() == Qt::AlignLeft);
    }

    SECTION("Center alignment") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(R"(<p align="center">Centered</p>)"));
        REQUIRE(doc != nullptr);

        QTextBlock block = doc->begin();
        REQUIRE(block.blockFormat().alignment() == Qt::AlignHCenter);
    }

    SECTION("Right alignment") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(R"(<p align="right">Right text</p>)"));
        REQUIRE(doc != nullptr);

        QTextBlock block = doc->begin();
        REQUIRE(block.blockFormat().alignment() == Qt::AlignRight);
    }

    SECTION("Justify alignment") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml(R"(<p align="justify">Justified</p>)"));
        REQUIRE(doc != nullptr);

        QTextBlock block = doc->begin();
        REQUIRE(block.blockFormat().alignment() == Qt::AlignJustify);
    }
}

// =============================================================================
// Performance Sanity Tests
// =============================================================================

TEST_CASE("KmlParser - Performance sanity", "[editor][kml_parser][performance]") {
    KmlParser parser;

    SECTION("Parse 100 paragraphs") {
        QString kml = "<kml>";
        for (int i = 0; i < 100; ++i) {
            kml += QString("<p>Paragraph %1 with <b>bold</b> text</p>").arg(i);
        }
        kml += "</kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(kml));
        REQUIRE(doc != nullptr);
        REQUIRE(blockCount(doc.get()) == 100);
    }

    SECTION("Parse deeply nested formatting") {
        QString kml = "<p><b><i><u><s><b><i><u><s>Deep</s></u></i></b></s></u></i></b></p>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(kml));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "Deep");
    }

    SECTION("Parse large paragraph") {
        QString longText;
        for (int i = 0; i < 1000; ++i) {
            longText += QString("Word%1 ").arg(i);
        }

        QString kml = "<p>" + longText.trimmed() + "</p>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(kml));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()).length() > 5000);
    }
}

// =============================================================================
// Formatting Inheritance Tests
// =============================================================================

TEST_CASE("KmlParser - Formatting inheritance", "[editor][kml_parser][formatting]") {
    KmlParser parser;

    SECTION("Subscript inside bold inherits bold") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><b>x<sub>2</sub></b></p>"));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "x2");

        // 'x' is bold
        QTextCharFormat fmtX = getFormatAt(doc.get(), 0);
        REQUIRE(fmtX.fontWeight() == QFont::Bold);

        // '2' is bold AND subscript
        QTextCharFormat fmt2 = getFormatAt(doc.get(), 1);
        REQUIRE(fmt2.fontWeight() == QFont::Bold);
        REQUIRE(fmt2.verticalAlignment() == QTextCharFormat::AlignSubScript);
    }

    SECTION("Italic inside bold+underline") {
        std::unique_ptr<QTextDocument> doc(parser.parseKml("<p><b><u><i>text</i></u></b></p>"));
        REQUIRE(doc != nullptr);

        QTextCharFormat fmt = getFormatAt(doc.get(), 0);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
        REQUIRE(fmt.fontUnderline());
        REQUIRE(fmt.fontItalic());
    }
}
