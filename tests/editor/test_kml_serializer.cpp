/// @file test_kml_serializer.cpp
/// @brief Unit tests for KmlSerializer (OpenSpec #00043 Phase 11.2.5)
///
/// Tests the KmlSerializer that converts QTextDocument back to KML format.
/// Focus on round-trip correctness: parse -> serialize -> parse should preserve content.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/kml_parser.h>
#include <kalahari/editor/kml_serializer.h>
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

/// @brief Create QTextDocument with plain text
static QTextDocument* createDocWithText(const QString& text)
{
    QTextDocument* doc = new QTextDocument();
    QTextCursor cursor(doc);
    cursor.insertText(text);
    return doc;
}

/// @brief Create QTextDocument with bold text
static QTextDocument* createDocWithBold(const QString& text)
{
    QTextDocument* doc = new QTextDocument();
    QTextCursor cursor(doc);
    QTextCharFormat fmt;
    fmt.setFontWeight(QFont::Bold);
    cursor.insertText(text, fmt);
    return doc;
}

/// @brief Create QTextDocument with italic text
static QTextDocument* createDocWithItalic(const QString& text)
{
    QTextDocument* doc = new QTextDocument();
    QTextCursor cursor(doc);
    QTextCharFormat fmt;
    fmt.setFontItalic(true);
    cursor.insertText(text, fmt);
    return doc;
}

// =============================================================================
// Basic Serialization Tests
// =============================================================================

TEST_CASE("KmlSerializer - Empty document", "[editor][kml_serializer][basic]") {
    KmlSerializer serializer;

    SECTION("Null document returns empty string") {
        QString result = serializer.toKml(nullptr);
        REQUIRE(result.isEmpty());
    }

    SECTION("Empty document returns kml wrapper") {
        QTextDocument doc;
        QString result = serializer.toKml(&doc);
        // Empty document has at least one empty block
        REQUIRE(result.contains("<kml>"));
        REQUIRE(result.contains("</kml>"));
    }
}

TEST_CASE("KmlSerializer - Plain text", "[editor][kml_serializer][basic]") {
    KmlSerializer serializer;

    SECTION("Single paragraph without formatting") {
        std::unique_ptr<QTextDocument> doc(createDocWithText("Hello world"));
        QString kml = serializer.toKml(doc.get());

        REQUIRE(kml.contains("<kml>"));
        REQUIRE(kml.contains("</kml>"));
        REQUIRE(kml.contains("<p>Hello world</p>"));
    }

    SECTION("Single word") {
        std::unique_ptr<QTextDocument> doc(createDocWithText("Word"));
        QString kml = serializer.toKml(doc.get());

        REQUIRE(kml.contains("<p>Word</p>"));
    }
}

TEST_CASE("KmlSerializer - Multiple paragraphs", "[editor][kml_serializer][basic]") {
    KmlSerializer serializer;

    SECTION("Two paragraphs") {
        QTextDocument doc;
        QTextCursor cursor(&doc);
        cursor.insertText("First paragraph");
        cursor.insertBlock();
        cursor.insertText("Second paragraph");

        QString kml = serializer.toKml(&doc);

        REQUIRE(kml.contains("<p>First paragraph</p>"));
        REQUIRE(kml.contains("<p>Second paragraph</p>"));
    }

    SECTION("Three paragraphs") {
        QTextDocument doc;
        QTextCursor cursor(&doc);
        cursor.insertText("A");
        cursor.insertBlock();
        cursor.insertText("B");
        cursor.insertBlock();
        cursor.insertText("C");

        QString kml = serializer.toKml(&doc);

        REQUIRE(kml.contains("<p>A</p>"));
        REQUIRE(kml.contains("<p>B</p>"));
        REQUIRE(kml.contains("<p>C</p>"));
    }
}

// =============================================================================
// Format Serialization Tests
// =============================================================================

TEST_CASE("KmlSerializer - Bold formatting", "[editor][kml_serializer][formatting]") {
    KmlSerializer serializer;

    SECTION("Bold text produces <b> tags") {
        std::unique_ptr<QTextDocument> doc(createDocWithBold("bold text"));
        QString kml = serializer.toKml(doc.get());

        REQUIRE(kml.contains("<b>bold text</b>"));
    }
}

TEST_CASE("KmlSerializer - Italic formatting", "[editor][kml_serializer][formatting]") {
    KmlSerializer serializer;

    SECTION("Italic text produces <i> tags") {
        std::unique_ptr<QTextDocument> doc(createDocWithItalic("italic text"));
        QString kml = serializer.toKml(doc.get());

        REQUIRE(kml.contains("<i>italic text</i>"));
    }
}

TEST_CASE("KmlSerializer - Underline formatting", "[editor][kml_serializer][formatting]") {
    KmlSerializer serializer;

    SECTION("Underline text produces <u> tags") {
        QTextDocument doc;
        QTextCursor cursor(&doc);
        QTextCharFormat fmt;
        fmt.setFontUnderline(true);
        cursor.insertText("underlined", fmt);

        QString kml = serializer.toKml(&doc);
        REQUIRE(kml.contains("<u>underlined</u>"));
    }
}

TEST_CASE("KmlSerializer - Strikethrough formatting", "[editor][kml_serializer][formatting]") {
    KmlSerializer serializer;

    SECTION("Strikethrough text produces <s> tags") {
        QTextDocument doc;
        QTextCursor cursor(&doc);
        QTextCharFormat fmt;
        fmt.setFontStrikeOut(true);
        cursor.insertText("struck", fmt);

        QString kml = serializer.toKml(&doc);
        REQUIRE(kml.contains("<s>struck</s>"));
    }
}

TEST_CASE("KmlSerializer - Subscript formatting", "[editor][kml_serializer][formatting]") {
    KmlSerializer serializer;

    SECTION("Subscript produces <sub> tags") {
        QTextDocument doc;
        QTextCursor cursor(&doc);

        cursor.insertText("H");

        QTextCharFormat subFmt;
        subFmt.setVerticalAlignment(QTextCharFormat::AlignSubScript);
        cursor.insertText("2", subFmt);

        cursor.insertText("O", QTextCharFormat());

        QString kml = serializer.toKml(&doc);
        REQUIRE(kml.contains("H<sub>2</sub>O"));
    }
}

TEST_CASE("KmlSerializer - Superscript formatting", "[editor][kml_serializer][formatting]") {
    KmlSerializer serializer;

    SECTION("Superscript produces <sup> tags") {
        QTextDocument doc;
        QTextCursor cursor(&doc);

        cursor.insertText("x");

        QTextCharFormat supFmt;
        supFmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
        cursor.insertText("2", supFmt);

        QString kml = serializer.toKml(&doc);
        REQUIRE(kml.contains("x<sup>2</sup>"));
    }
}

TEST_CASE("KmlSerializer - Nested formatting", "[editor][kml_serializer][formatting]") {
    KmlSerializer serializer;

    SECTION("Bold and italic combined") {
        QTextDocument doc;
        QTextCursor cursor(&doc);
        QTextCharFormat fmt;
        fmt.setFontWeight(QFont::Bold);
        fmt.setFontItalic(true);
        cursor.insertText("bold italic", fmt);

        QString kml = serializer.toKml(&doc);
        // Should contain both tags - order may vary based on implementation
        REQUIRE(kml.contains("<b>"));
        REQUIRE(kml.contains("<i>"));
        REQUIRE(kml.contains("bold italic"));
        REQUIRE(kml.contains("</i>"));
        REQUIRE(kml.contains("</b>"));
    }

    SECTION("Bold, italic, and underline combined") {
        QTextDocument doc;
        QTextCursor cursor(&doc);
        QTextCharFormat fmt;
        fmt.setFontWeight(QFont::Bold);
        fmt.setFontItalic(true);
        fmt.setFontUnderline(true);
        cursor.insertText("formatted", fmt);

        QString kml = serializer.toKml(&doc);
        REQUIRE(kml.contains("<b>"));
        REQUIRE(kml.contains("<i>"));
        REQUIRE(kml.contains("<u>"));
        REQUIRE(kml.contains("formatted"));
    }
}

TEST_CASE("KmlSerializer - Mixed content", "[editor][kml_serializer][formatting]") {
    KmlSerializer serializer;

    SECTION("Normal text with bold in middle") {
        QTextDocument doc;
        QTextCursor cursor(&doc);

        cursor.insertText("Normal ");

        QTextCharFormat boldFmt;
        boldFmt.setFontWeight(QFont::Bold);
        cursor.insertText("bold", boldFmt);

        cursor.insertText(" normal", QTextCharFormat());

        QString kml = serializer.toKml(&doc);
        REQUIRE(kml.contains("Normal <b>bold</b> normal"));
    }
}

// =============================================================================
// Metadata Serialization Tests
// =============================================================================

TEST_CASE("KmlSerializer - Comment metadata", "[editor][kml_serializer][metadata]") {
    KmlSerializer serializer;

    SECTION("Comment with id and author") {
        QTextDocument doc;
        QTextCursor cursor(&doc);

        QTextCharFormat fmt;
        QVariantMap commentData;
        commentData["id"] = "c1";
        commentData["author"] = "Jan";
        fmt.setProperty(KmlPropComment, commentData);

        cursor.insertText("annotated", fmt);

        QString kml = serializer.toKml(&doc);
        REQUIRE(kml.contains("<comment"));
        REQUIRE(kml.contains("id=\"c1\""));
        REQUIRE(kml.contains("author=\"Jan\""));
        REQUIRE(kml.contains(">annotated</comment>"));
    }

    SECTION("Comment with resolved attribute") {
        QTextDocument doc;
        QTextCursor cursor(&doc);

        QTextCharFormat fmt;
        QVariantMap commentData;
        commentData["id"] = "c2";
        commentData["resolved"] = true;
        fmt.setProperty(KmlPropComment, commentData);

        cursor.insertText("done", fmt);

        QString kml = serializer.toKml(&doc);
        REQUIRE(kml.contains("resolved=\"true\""));
    }
}

TEST_CASE("KmlSerializer - Todo metadata", "[editor][kml_serializer][metadata]") {
    KmlSerializer serializer;

    SECTION("Todo with id") {
        QTextDocument doc;
        QTextCursor cursor(&doc);

        QTextCharFormat fmt;
        QVariantMap todoData;
        todoData["id"] = "t1";
        fmt.setProperty(KmlPropTodo, todoData);

        cursor.insertText("task item", fmt);

        QString kml = serializer.toKml(&doc);
        REQUIRE(kml.contains("<todo"));
        REQUIRE(kml.contains("id=\"t1\""));
        REQUIRE(kml.contains(">task item</todo>"));
    }

    SECTION("Todo with completed and priority") {
        QTextDocument doc;
        QTextCursor cursor(&doc);

        QTextCharFormat fmt;
        QVariantMap todoData;
        todoData["id"] = "t2";
        todoData["completed"] = true;
        todoData["priority"] = "high";
        fmt.setProperty(KmlPropTodo, todoData);

        cursor.insertText("done task", fmt);

        QString kml = serializer.toKml(&doc);
        REQUIRE(kml.contains("completed=\"true\""));
        REQUIRE(kml.contains("priority=\"high\""));
    }
}

TEST_CASE("KmlSerializer - Footnote metadata", "[editor][kml_serializer][metadata]") {
    KmlSerializer serializer;

    SECTION("Footnote with id and number") {
        QTextDocument doc;
        QTextCursor cursor(&doc);

        QTextCharFormat fmt;
        QVariantMap footnoteData;
        footnoteData["id"] = "f1";
        footnoteData["number"] = 1;
        fmt.setProperty(KmlPropFootnote, footnoteData);

        cursor.insertText("note", fmt);

        QString kml = serializer.toKml(&doc);
        REQUIRE(kml.contains("<footnote"));
        REQUIRE(kml.contains("id=\"f1\""));
        REQUIRE(kml.contains("number=\"1\""));
        REQUIRE(kml.contains(">note</footnote>"));
    }
}

// =============================================================================
// Round-Trip Tests (CRITICAL)
// =============================================================================

TEST_CASE("KmlSerializer - Round trip plain text", "[editor][kml_serializer][roundtrip]") {
    KmlParser parser;
    KmlSerializer serializer;

    SECTION("Simple paragraph round-trip") {
        QString originalKml = "<kml><p>Hello world</p></kml>";

        // Parse
        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        // Serialize
        QString serializedKml = serializer.toKml(doc.get());

        // Parse again
        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        // Compare content
        REQUIRE(getPlainText(doc.get()) == getPlainText(doc2.get()));
        REQUIRE(blockCount(doc.get()) == blockCount(doc2.get()));
    }

    SECTION("Multiple paragraphs round-trip") {
        QString originalKml = "<kml><p>First</p><p>Second</p><p>Third</p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        REQUIRE(blockCount(doc.get()) == blockCount(doc2.get()));
        REQUIRE(blockText(doc.get(), 0) == blockText(doc2.get(), 0));
        REQUIRE(blockText(doc.get(), 1) == blockText(doc2.get(), 1));
        REQUIRE(blockText(doc.get(), 2) == blockText(doc2.get(), 2));
    }
}

TEST_CASE("KmlSerializer - Round trip formatted text", "[editor][kml_serializer][roundtrip]") {
    KmlParser parser;
    KmlSerializer serializer;

    SECTION("Bold text round-trip") {
        QString originalKml = "<kml><p><b>bold text</b></p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        REQUIRE(getPlainText(doc.get()) == getPlainText(doc2.get()));

        // Check formatting preserved
        QTextCharFormat fmt = getFormatAt(doc2.get(), 0);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
    }

    SECTION("Italic text round-trip") {
        QString originalKml = "<kml><p><i>italic text</i></p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        QTextCharFormat fmt = getFormatAt(doc2.get(), 0);
        REQUIRE(fmt.fontItalic());
    }

    SECTION("Mixed formatting round-trip") {
        QString originalKml = "<kml><p>Normal <b>bold</b> normal</p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        REQUIRE(getPlainText(doc.get()) == getPlainText(doc2.get()));

        // Check formatting at specific positions
        // "Normal " - not bold
        QTextCharFormat fmtNormal = getFormatAt(doc2.get(), 0);
        REQUIRE(fmtNormal.fontWeight() != QFont::Bold);

        // "bold" - bold (position 7)
        QTextCharFormat fmtBold = getFormatAt(doc2.get(), 7);
        REQUIRE(fmtBold.fontWeight() == QFont::Bold);
    }

    SECTION("Subscript round-trip") {
        QString originalKml = "<kml><p>H<sub>2</sub>O</p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        REQUIRE(getPlainText(doc.get()) == "H2O");
        REQUIRE(getPlainText(doc2.get()) == "H2O");

        // Check subscript at position 1 (the "2")
        QTextCharFormat fmt2 = getFormatAt(doc2.get(), 1);
        REQUIRE(fmt2.verticalAlignment() == QTextCharFormat::AlignSubScript);
    }

    SECTION("Superscript round-trip") {
        QString originalKml = "<kml><p>x<sup>2</sup></p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        REQUIRE(getPlainText(doc.get()) == "x2");
        REQUIRE(getPlainText(doc2.get()) == "x2");

        QTextCharFormat fmt2 = getFormatAt(doc2.get(), 1);
        REQUIRE(fmt2.verticalAlignment() == QTextCharFormat::AlignSuperScript);
    }
}

TEST_CASE("KmlSerializer - Round trip complex document", "[editor][kml_serializer][roundtrip]") {
    KmlParser parser;
    KmlSerializer serializer;

    SECTION("Document with multiple formatting types") {
        QString originalKml = R"(<kml>
            <p>Normal text with <b>bold</b> and <i>italic</i></p>
            <p>Formula: H<sub>2</sub>O and E=mc<sup>2</sup></p>
            <p><b><i>Bold italic</i></b> text</p>
        </kml>)";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        REQUIRE(blockCount(doc.get()) == blockCount(doc2.get()));
        REQUIRE(getPlainText(doc.get()) == getPlainText(doc2.get()));
    }
}

TEST_CASE("KmlSerializer - Round trip metadata", "[editor][kml_serializer][roundtrip]") {
    KmlParser parser;
    KmlSerializer serializer;

    SECTION("Comment round-trip") {
        QString originalKml = R"(<kml><p>Text <comment id="c1" author="Jan">annotated</comment> text</p></kml>)";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        REQUIRE(getPlainText(doc.get()) == getPlainText(doc2.get()));

        // Check comment metadata preserved (position 5 = "annotated")
        QTextCharFormat fmt = getFormatAt(doc2.get(), 5);
        QVariant commentData = fmt.property(KmlPropComment);
        REQUIRE(commentData.isValid());

        QVariantMap meta = commentData.toMap();
        REQUIRE(meta["id"].toString() == "c1");
        REQUIRE(meta["author"].toString() == "Jan");
    }

    SECTION("Todo round-trip") {
        QString originalKml = R"(<kml><p><todo id="t1" completed="true">done task</todo></p></kml>)";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        QTextCharFormat fmt = getFormatAt(doc2.get(), 0);
        QVariant todoData = fmt.property(KmlPropTodo);
        REQUIRE(todoData.isValid());

        QVariantMap meta = todoData.toMap();
        REQUIRE(meta["id"].toString() == "t1");
        REQUIRE(meta["completed"].toBool() == true);
    }

    SECTION("Footnote round-trip") {
        QString originalKml = R"(<kml><p>Text<footnote id="f1" number="1">note</footnote></p></kml>)";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        // "Text" = 4 chars, "note" starts at position 4
        QTextCharFormat fmt = getFormatAt(doc2.get(), 4);
        QVariant footnoteData = fmt.property(KmlPropFootnote);
        REQUIRE(footnoteData.isValid());

        QVariantMap meta = footnoteData.toMap();
        REQUIRE(meta["id"].toString() == "f1");
        REQUIRE(meta["number"].toInt() == 1);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("KmlSerializer - XML special characters", "[editor][kml_serializer][edge]") {
    KmlSerializer serializer;
    KmlParser parser;

    SECTION("Less than and greater than escaped") {
        std::unique_ptr<QTextDocument> doc(createDocWithText("<tag>"));
        QString kml = serializer.toKml(doc.get());

        REQUIRE(kml.contains("&lt;tag&gt;"));
        REQUIRE_FALSE(kml.contains("<tag>"));  // Not raw (except in markup)
    }

    SECTION("Ampersand escaped") {
        std::unique_ptr<QTextDocument> doc(createDocWithText("Rock & Roll"));
        QString kml = serializer.toKml(doc.get());

        REQUIRE(kml.contains("Rock &amp; Roll"));
    }

    SECTION("Quotes escaped") {
        std::unique_ptr<QTextDocument> doc(createDocWithText("\"quoted\""));
        QString kml = serializer.toKml(doc.get());

        REQUIRE(kml.contains("&quot;quoted&quot;"));
    }

    SECTION("Special characters round-trip") {
        QString originalKml = "<kml><p>&lt;tag&gt; &amp; &quot;text&quot;</p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);
        REQUIRE(getPlainText(doc.get()) == "<tag> & \"text\"");

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);
        REQUIRE(getPlainText(doc2.get()) == "<tag> & \"text\"");
    }
}

TEST_CASE("KmlSerializer - Empty paragraphs", "[editor][kml_serializer][edge]") {
    KmlSerializer serializer;
    KmlParser parser;

    SECTION("Empty paragraph preserved") {
        QString originalKml = "<kml><p>First</p><p></p><p>Third</p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        REQUIRE(blockCount(doc.get()) == blockCount(doc2.get()));
        REQUIRE(blockText(doc2.get(), 0) == "First");
        REQUIRE(blockText(doc2.get(), 1) == "");
        REQUIRE(blockText(doc2.get(), 2) == "Third");
    }
}

TEST_CASE("KmlSerializer - Unicode text", "[editor][kml_serializer][edge]") {
    KmlSerializer serializer;
    KmlParser parser;

    SECTION("Polish characters round-trip") {
        QString polishText = QString::fromUtf8("Zażółć gęślą jaźń");
        std::unique_ptr<QTextDocument> doc(createDocWithText(polishText));

        QString kml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(kml));
        REQUIRE(doc2 != nullptr);
        REQUIRE(getPlainText(doc2.get()) == polishText);
    }

    SECTION("Chinese characters round-trip") {
        QString chineseText = QString::fromUtf8("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95");
        std::unique_ptr<QTextDocument> doc(createDocWithText(chineseText));

        QString kml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(kml));
        REQUIRE(doc2 != nullptr);
        REQUIRE(getPlainText(doc2.get()) == chineseText);
    }

    SECTION("Emoji characters round-trip") {
        QString emojiText = QString::fromUtf8("Hello \xF0\x9F\x91\x8B world \xF0\x9F\x8C\x8D");
        std::unique_ptr<QTextDocument> doc(createDocWithText(emojiText));

        QString kml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(kml));
        REQUIRE(doc2 != nullptr);
        REQUIRE(getPlainText(doc2.get()) == emojiText);
    }

    SECTION("Mixed scripts round-trip") {
        QString mixedText = QString::fromUtf8("English, Русский");
        std::unique_ptr<QTextDocument> doc(createDocWithText(mixedText));

        QString kml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(kml));
        REQUIRE(doc2 != nullptr);
        REQUIRE(getPlainText(doc2.get()) == mixedText);
    }
}

TEST_CASE("KmlSerializer - Whitespace preservation", "[editor][kml_serializer][edge]") {
    KmlSerializer serializer;
    KmlParser parser;

    SECTION("Leading spaces preserved") {
        QString originalKml = "<kml><p>   Leading spaces</p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);
        REQUIRE(getPlainText(doc2.get()) == "   Leading spaces");
    }

    SECTION("Trailing spaces preserved") {
        QString originalKml = "<kml><p>Trailing spaces   </p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);
        REQUIRE(getPlainText(doc2.get()) == "Trailing spaces   ");
    }

    SECTION("Multiple internal spaces preserved") {
        QString originalKml = "<kml><p>Multiple   spaces   here</p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);
        REQUIRE(getPlainText(doc2.get()) == "Multiple   spaces   here");
    }

    SECTION("Tabs preserved") {
        QString originalKml = "<kml><p>Tab\there\tthere</p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);
        REQUIRE(getPlainText(doc2.get()) == "Tab\there\tthere");
    }
}

// =============================================================================
// Options Tests
// =============================================================================

TEST_CASE("KmlSerializer - Indentation option", "[editor][kml_serializer][options]") {
    KmlSerializer serializer;

    SECTION("Default is not indented") {
        REQUIRE_FALSE(serializer.isIndented());
    }

    SECTION("Can enable indentation") {
        serializer.setIndented(true);
        REQUIRE(serializer.isIndented());
    }

    SECTION("Indented output contains newlines") {
        serializer.setIndented(true);
        std::unique_ptr<QTextDocument> doc(createDocWithText("Hello"));
        QString kml = serializer.toKml(doc.get());

        REQUIRE(kml.contains("\n"));
    }

    SECTION("Non-indented output is compact") {
        serializer.setIndented(false);
        std::unique_ptr<QTextDocument> doc(createDocWithText("Hello"));
        QString kml = serializer.toKml(doc.get());

        // Should be single line (no newlines except possibly at end)
        QString trimmed = kml.trimmed();
        REQUIRE_FALSE(trimmed.contains("\n"));
    }
}

// =============================================================================
// Block Serialization Tests
// =============================================================================

TEST_CASE("KmlSerializer - blockToKml", "[editor][kml_serializer][block]") {
    KmlSerializer serializer;

    SECTION("Single block serialization without wrapper") {
        QTextDocument doc;
        QTextCursor cursor(&doc);
        cursor.insertText("Block content");

        QTextBlock block = doc.begin();
        QString blockKml = serializer.blockToKml(block);

        // blockToKml returns content without <p> wrapper
        REQUIRE(blockKml == "Block content");
        REQUIRE_FALSE(blockKml.contains("<p>"));
    }

    SECTION("Block with formatting") {
        QTextDocument doc;
        QTextCursor cursor(&doc);

        QTextCharFormat boldFmt;
        boldFmt.setFontWeight(QFont::Bold);
        cursor.insertText("bold", boldFmt);

        QTextBlock block = doc.begin();
        QString blockKml = serializer.blockToKml(block);

        REQUIRE(blockKml.contains("<b>bold</b>"));
    }

    SECTION("Invalid block returns empty string") {
        QTextBlock invalidBlock;
        QString blockKml = serializer.blockToKml(invalidBlock);

        REQUIRE(blockKml.isEmpty());
    }
}

// =============================================================================
// Serializer Reusability Tests
// =============================================================================

TEST_CASE("KmlSerializer - Serializer reusability", "[editor][kml_serializer][reuse]") {
    KmlSerializer serializer;

    SECTION("Serializer can serialize multiple documents") {
        std::unique_ptr<QTextDocument> doc1(createDocWithText("First"));
        QString kml1 = serializer.toKml(doc1.get());
        REQUIRE(kml1.contains("First"));

        std::unique_ptr<QTextDocument> doc2(createDocWithText("Second"));
        QString kml2 = serializer.toKml(doc2.get());
        REQUIRE(kml2.contains("Second"));

        // First result should still be valid
        REQUIRE(kml1.contains("First"));
    }

    SECTION("Options persist between serializations") {
        serializer.setIndented(true);

        std::unique_ptr<QTextDocument> doc1(createDocWithText("First"));
        QString kml1 = serializer.toKml(doc1.get());

        std::unique_ptr<QTextDocument> doc2(createDocWithText("Second"));
        QString kml2 = serializer.toKml(doc2.get());

        // Both should be indented
        REQUIRE(kml1.contains("\n"));
        REQUIRE(kml2.contains("\n"));
    }
}

// =============================================================================
// Performance Sanity Tests
// =============================================================================

TEST_CASE("KmlSerializer - Performance sanity", "[editor][kml_serializer][performance]") {
    KmlSerializer serializer;
    KmlParser parser;

    SECTION("Serialize 100 paragraphs") {
        QTextDocument doc;
        QTextCursor cursor(&doc);

        for (int i = 0; i < 100; ++i) {
            if (i > 0) cursor.insertBlock();
            cursor.insertText(QString("Paragraph %1 with some text").arg(i));
        }

        QString kml = serializer.toKml(&doc);

        REQUIRE_FALSE(kml.isEmpty());
        REQUIRE(kml.contains("<p>Paragraph 0"));
        REQUIRE(kml.contains("<p>Paragraph 99"));
    }

    SECTION("Round-trip 100 paragraphs") {
        QString originalKml = "<kml>";
        for (int i = 0; i < 100; ++i) {
            originalKml += QString("<p>Paragraph %1 with <b>bold</b> text</p>").arg(i);
        }
        originalKml += "</kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        REQUIRE(blockCount(doc.get()) == blockCount(doc2.get()));
        REQUIRE(getPlainText(doc.get()) == getPlainText(doc2.get()));
    }

    SECTION("Serialize large paragraph") {
        QString longText;
        for (int i = 0; i < 1000; ++i) {
            longText += QString("Word%1 ").arg(i);
        }

        std::unique_ptr<QTextDocument> doc(createDocWithText(longText.trimmed()));
        QString kml = serializer.toKml(doc.get());

        REQUIRE_FALSE(kml.isEmpty());
        REQUIRE(kml.contains("Word0"));
        REQUIRE(kml.contains("Word999"));
    }
}

// =============================================================================
// Formatting Combinations Tests
// =============================================================================

TEST_CASE("KmlSerializer - All formatting combinations", "[editor][kml_serializer][comprehensive]") {
    KmlSerializer serializer;
    KmlParser parser;

    SECTION("All basic formats in one document") {
        QString originalKml = R"(<kml>
            <p><b>Bold</b> <i>Italic</i> <u>Underline</u> <s>Strike</s></p>
            <p>H<sub>2</sub>O and x<sup>2</sup></p>
        </kml>)";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        REQUIRE(getPlainText(doc.get()) == getPlainText(doc2.get()));
    }

    SECTION("Nested formatting preserved") {
        QString originalKml = "<kml><p><b><i>bold italic</i></b></p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        QTextCharFormat fmt = getFormatAt(doc2.get(), 0);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
        REQUIRE(fmt.fontItalic());
    }

    SECTION("Three-level nesting preserved") {
        QString originalKml = "<kml><p><b><i><u>formatted</u></i></b></p></kml>";

        std::unique_ptr<QTextDocument> doc(parser.parseKml(originalKml));
        REQUIRE(doc != nullptr);

        QString serializedKml = serializer.toKml(doc.get());

        std::unique_ptr<QTextDocument> doc2(parser.parseKml(serializedKml));
        REQUIRE(doc2 != nullptr);

        QTextCharFormat fmt = getFormatAt(doc2.get(), 0);
        REQUIRE(fmt.fontWeight() == QFont::Bold);
        REQUIRE(fmt.fontItalic());
        REQUIRE(fmt.fontUnderline());
    }
}
