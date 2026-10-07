/// @file test_kml_load.cpp
/// @brief KML reading through the path a chapter takes when it is opened
///
/// Every case loads KML with BookEditor::fromKml() and checks the document the editor
/// edits. The cases come from the tests of the second KML parser (KmlParser), which only
/// the tests used; they now pin down the reader the application uses.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kml_format_registry.h>
#include <QFont>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QVariantMap>

using namespace kalahari::editor;

namespace {

/// An editor with @p kml loaded, which owns the document the checks read
struct Loaded {
    explicit Loaded(const QString& kml) { editor.fromKml(kml); }

    QTextDocument* doc() const { return editor.textDocument(); }

    QString text() const { return doc() ? doc()->toPlainText() : QString(); }

    int blockCount() const { return doc() ? doc()->blockCount() : 0; }

    QString blockText(int index) const {
        return doc() ? doc()->findBlockByNumber(index).text() : QString();
    }

    /// Format of the character at @p position
    QTextCharFormat formatAt(int position) const {
        QTextCursor cursor(doc());
        cursor.setPosition(position);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        return cursor.charFormat();
    }

    Qt::Alignment alignment(int block) const {
        return doc()->findBlockByNumber(block).blockFormat().alignment() & Qt::AlignHorizontal_Mask;
    }

    BookEditor editor;
};

}  // anonymous namespace

// =============================================================================
// Paragraphs and root element
// =============================================================================

TEST_CASE("KML load: empty KML gives an empty document", "[editor][kml][load]") {
    Loaded loaded(QString{});
    REQUIRE(loaded.doc() != nullptr);
    REQUIRE(loaded.text().isEmpty());
}

TEST_CASE("KML load: paragraphs", "[editor][kml][load]") {
    SECTION("One paragraph") {
        Loaded loaded(QStringLiteral("<p>Text content</p>"));
        REQUIRE(loaded.blockCount() == 1);
        REQUIRE(loaded.blockText(0) == QStringLiteral("Text content"));
    }

    SECTION("Three paragraphs") {
        Loaded loaded(QStringLiteral("<p>A</p><p>B</p><p>C</p>"));
        REQUIRE(loaded.blockCount() == 3);
        REQUIRE(loaded.blockText(0) == QStringLiteral("A"));
        REQUIRE(loaded.blockText(1) == QStringLiteral("B"));
        REQUIRE(loaded.blockText(2) == QStringLiteral("C"));
    }

    SECTION("Empty paragraphs") {
        Loaded loaded(QStringLiteral("<p>First</p><p></p><p>Third</p><p></p>"));
        REQUIRE(loaded.blockCount() == 4);
        REQUIRE(loaded.blockText(1).isEmpty());
        REQUIRE(loaded.blockText(2) == QStringLiteral("Third"));
        REQUIRE(loaded.blockText(3).isEmpty());
    }
}

TEST_CASE("KML load: root element variants", "[editor][kml][load]") {
    for (const char* kml : {"<kml><p>Content</p></kml>", "<doc><p>Content</p></doc>",
                            "<document><p>Content</p></document>", "<p>Content</p>"}) {
        DYNAMIC_SECTION(kml) {
            Loaded loaded(QString::fromLatin1(kml));
            REQUIRE(loaded.text() == QStringLiteral("Content"));
        }
    }
}

TEST_CASE("KML load: text runs are plain text", "[editor][kml][load]") {
    Loaded loaded(QStringLiteral("<p><t>Text run</t> and <text>text content</text></p>"));
    REQUIRE(loaded.text() == QStringLiteral("Text run and text content"));
}

// =============================================================================
// Formatting
// =============================================================================

TEST_CASE("KML load: formatting tags and their synonyms", "[editor][kml][load]") {
    struct Case {
        const char* tag;
        bool (*applied)(const QTextCharFormat&);
    };
    const auto bold = [](const QTextCharFormat& f) { return f.fontWeight() == QFont::Bold; };
    const auto italic = [](const QTextCharFormat& f) { return f.fontItalic(); };
    const auto underline = [](const QTextCharFormat& f) { return f.fontUnderline(); };
    const auto strike = [](const QTextCharFormat& f) { return f.fontStrikeOut(); };
    const auto sub = [](const QTextCharFormat& f) {
        return f.verticalAlignment() == QTextCharFormat::AlignSubScript;
    };
    const auto sup = [](const QTextCharFormat& f) {
        return f.verticalAlignment() == QTextCharFormat::AlignSuperScript;
    };
    const Case cases[] = {
        {"b", bold},     {"bold", bold},           {"strong", bold},
        {"i", italic},   {"italic", italic},       {"em", italic},
        {"u", underline}, {"underline", underline},
        {"s", strike},   {"strike", strike},       {"strikethrough", strike},
        {"sub", sub},    {"subscript", sub},
        {"sup", sup},    {"superscript", sup},
    };

    for (const auto& c : cases) {
        DYNAMIC_SECTION("<" << c.tag << ">") {
            Loaded loaded(QStringLiteral("<p>x<%1>formatted</%1>y</p>").arg(QLatin1String(c.tag)));
            REQUIRE(loaded.text() == QStringLiteral("xformattedy"));
            CHECK_FALSE(c.applied(loaded.formatAt(0)));
            CHECK(c.applied(loaded.formatAt(1)));
            CHECK(c.applied(loaded.formatAt(9)));
            CHECK_FALSE(c.applied(loaded.formatAt(10)));
        }
    }
}

TEST_CASE("KML load: nested formatting combines", "[editor][kml][load]") {
    SECTION("Bold inside italic and italic inside bold") {
        for (const char* kml : {"<p><i><b>both</b></i></p>", "<p><b><i>both</i></b></p>"}) {
            Loaded loaded(QString::fromLatin1(kml));
            REQUIRE(loaded.text() == QStringLiteral("both"));
            CHECK(loaded.formatAt(0).fontWeight() == QFont::Bold);
            CHECK(loaded.formatAt(0).fontItalic());
        }
    }

    SECTION("Three levels") {
        Loaded loaded(QStringLiteral("<p><b><u><i>text</i></u></b></p>"));
        const QTextCharFormat format = loaded.formatAt(0);
        CHECK(format.fontWeight() == QFont::Bold);
        CHECK(format.fontUnderline());
        CHECK(format.fontItalic());
    }

    SECTION("Subscript inside bold keeps the bold") {
        Loaded loaded(QStringLiteral("<p><b>x<sub>2</sub></b></p>"));
        REQUIRE(loaded.text() == QStringLiteral("x2"));
        CHECK(loaded.formatAt(0).fontWeight() == QFont::Bold);
        CHECK(loaded.formatAt(1).fontWeight() == QFont::Bold);
        CHECK(loaded.formatAt(1).verticalAlignment() == QTextCharFormat::AlignSubScript);
    }

    SECTION("Deep nesting") {
        Loaded loaded(QStringLiteral("<p><b><i><u><s><b><i><u><s>Deep</s></u></i></b></s></u></i></b></p>"));
        REQUIRE(loaded.text() == QStringLiteral("Deep"));
        CHECK(loaded.formatAt(0).fontStrikeOut());
    }
}

TEST_CASE("KML load: formatting covers only its own text", "[editor][kml][load]") {
    Loaded loaded(QStringLiteral("<p>Text <b>bold</b> and <i>italic</i> end</p>"));
    REQUIRE(loaded.text() == QStringLiteral("Text bold and italic end"));
    CHECK(loaded.formatAt(4).fontWeight() != QFont::Bold);
    CHECK(loaded.formatAt(5).fontWeight() == QFont::Bold);
    CHECK(loaded.formatAt(9).fontWeight() != QFont::Bold);
    CHECK_FALSE(loaded.formatAt(13).fontItalic());
    CHECK(loaded.formatAt(14).fontItalic());
    CHECK_FALSE(loaded.formatAt(20).fontItalic());
}

TEST_CASE("KML load: paragraph alignment", "[editor][kml][load]") {
    Loaded loaded(QStringLiteral(
        "<p align=\"left\">L</p><p align=\"center\">C</p><p align=\"right\">R</p>"
        "<p align=\"justify\">J</p>"));
    REQUIRE(loaded.blockCount() == 4);
    CHECK(loaded.alignment(0) == Qt::AlignLeft);
    CHECK(loaded.alignment(1) == Qt::AlignHCenter);
    CHECK(loaded.alignment(2) == Qt::AlignRight);
    CHECK(loaded.alignment(3) == Qt::AlignJustify);
}

// =============================================================================
// Metadata
// =============================================================================

TEST_CASE("KML load: comment, todo and footnote metadata", "[editor][kml][load]") {
    SECTION("Comment") {
        Loaded loaded(QStringLiteral(
            "<p>Text <comment id=\"c1\" author=\"Jan\" resolved=\"true\">annotated</comment> text</p>"));
        REQUIRE(loaded.text() == QStringLiteral("Text annotated text"));
        const QVariantMap metadata = loaded.formatAt(5).property(KmlPropComment).toMap();
        CHECK(metadata.value(QStringLiteral("id")).toString() == QStringLiteral("c1"));
        CHECK(metadata.value(QStringLiteral("author")).toString() == QStringLiteral("Jan"));
        CHECK(metadata.value(QStringLiteral("resolved")).toBool());
        CHECK_FALSE(loaded.formatAt(4).hasProperty(KmlPropComment));
    }

    SECTION("Todo") {
        Loaded loaded(QStringLiteral(
            "<p><todo id=\"t2\" completed=\"true\" priority=\"high\">done task</todo></p>"));
        REQUIRE(loaded.text() == QStringLiteral("done task"));
        const QVariantMap metadata = loaded.formatAt(0).property(KmlPropTodo).toMap();
        CHECK(metadata.value(QStringLiteral("id")).toString() == QStringLiteral("t2"));
        CHECK(metadata.value(QStringLiteral("completed")).toBool());
        CHECK(metadata.value(QStringLiteral("priority")).toString() == QStringLiteral("high"));
    }

    SECTION("Footnote") {
        Loaded loaded(QStringLiteral(
            "<p>Text with<footnote id=\"f1\" number=\"1\">note</footnote> reference</p>"));
        REQUIRE(loaded.text() == QStringLiteral("Text withnote reference"));
        const QVariantMap metadata = loaded.formatAt(9).property(KmlPropFootnote).toMap();
        CHECK(metadata.value(QStringLiteral("id")).toString() == QStringLiteral("f1"));
        CHECK(metadata.value(QStringLiteral("number")).toInt() == 1);
    }

    SECTION("Metadata and formatting in one document") {
        Loaded loaded(QStringLiteral(
            "<doc><p>Text with <comment id=\"c1\" author=\"Test\">comment</comment> here</p>"
            "<p><b>Bold</b> and <todo id=\"t1\">todo item</todo></p></doc>"));
        REQUIRE(loaded.blockCount() == 2);
        CHECK(loaded.blockText(1) == QStringLiteral("Bold and todo item"));
    }
}

// =============================================================================
// Text
// =============================================================================

TEST_CASE("KML load: whitespace is kept", "[editor][kml][load]") {
    for (const char* text : {"   Leading spaces", "Trailing spaces   ", "Multiple   spaces   here",
                             "Tab\there\tthere", "   "}) {
        DYNAMIC_SECTION('"' << text << '"') {
            Loaded loaded(QStringLiteral("<p>%1</p>").arg(QString::fromLatin1(text)));
            REQUIRE(loaded.text() == QString::fromLatin1(text));
        }
    }
}

TEST_CASE("KML load: XML entities are decoded", "[editor][kml][load]") {
    Loaded loaded(QStringLiteral("<p>&lt;tag&gt; Rock &amp; Roll &quot;quoted&quot;</p>"));
    REQUIRE(loaded.text() == QStringLiteral("<tag> Rock & Roll \"quoted\""));
}

TEST_CASE("KML load: Unicode text", "[editor][kml][load]") {
    const QString samples[] = {
        QString::fromUtf8("Zażółć gęślą jaźń"),
        QString::fromUtf8("中文测试"),
        QString::fromUtf8("Hello 👋 world 🌍"),
        QString::fromUtf8("English, Русский"),
    };
    for (const QString& text : samples) {
        DYNAMIC_SECTION(text.toStdString()) {
            Loaded loaded(QStringLiteral("<p>%1</p>").arg(text));
            REQUIRE(loaded.text() == text);
        }
    }
}

TEST_CASE("KML load: long chapters and long paragraphs", "[editor][kml][load]") {
    SECTION("100 paragraphs") {
        QString kml = QStringLiteral("<kml>");
        for (int i = 0; i < 100; ++i) {
            kml += QStringLiteral("<p>Paragraph %1 with <b>bold</b> text</p>").arg(i);
        }
        kml += QStringLiteral("</kml>");
        Loaded loaded(kml);
        REQUIRE(loaded.blockCount() == 100);
        CHECK(loaded.blockText(99) == QStringLiteral("Paragraph 99 with bold text"));
    }

    SECTION("A paragraph of 1000 words") {
        QStringList words;
        for (int i = 0; i < 1000; ++i) {
            words << QStringLiteral("Word%1").arg(i);
        }
        Loaded loaded(QStringLiteral("<p>%1</p>").arg(words.join(QLatin1Char(' '))));
        REQUIRE(loaded.blockCount() == 1);
        REQUIRE(loaded.text() == words.join(QLatin1Char(' ')));
    }
}

// =============================================================================
// Loading again and unreadable KML
// =============================================================================

TEST_CASE("KML load: loading again replaces the text", "[editor][kml][load]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral("<p>First content</p>"));
    REQUIRE(editor.plainText() == QStringLiteral("First content"));

    editor.fromKml(QStringLiteral("<p>New content</p>"));
    REQUIRE(editor.plainText() == QStringLiteral("New content"));

    SECTION("Also after unreadable KML") {
        editor.fromKml(QStringLiteral("<invalid<<<"));
        editor.fromKml(QStringLiteral("<p>Valid</p>"));
        REQUIRE(editor.plainText() == QStringLiteral("Valid"));
    }
}

TEST_CASE("KML load: unreadable KML leaves a usable editor", "[editor][kml][load]") {
    for (const char* kml : {"<p>Unclosed", "<p><b>Text</i></p>", "<p><b>Bold text</p>", "<invalid<<<"}) {
        DYNAMIC_SECTION(kml) {
            BookEditor editor;
            editor.fromKml(QString::fromLatin1(kml));
            editor.insertText(QStringLiteral("typed"));
            REQUIRE(editor.plainText().contains(QStringLiteral("typed")));
        }
    }
}

// =============================================================================
// Elements the editor does not support yet (docs/kml_format.md)
// =============================================================================

TEST_CASE("KML load: unsupported elements are not kept", "[editor][kml][load][unsupported]") {
    SECTION("Elements other than paragraphs, with their content") {
        BookEditor editor;
        editor.fromKml(QStringLiteral(
            "<kml><p>A</p><h1>Title</h1><table><tr><td>Cell</td></tr></table>"
            "<ul><li>Item</li></ul><img src=\"map.png\"/><p>B</p></kml>"));
        REQUIRE(editor.plainText() == QStringLiteral("A\nB"));
        REQUIRE(editor.toKml() == QStringLiteral("<kml><p>A</p><p>B</p></kml>"));
    }

    SECTION("Paragraph attributes other than align") {
        BookEditor editor;
        editor.fromKml(QStringLiteral("<kml><p style=\"dialog\" align=\"right\">Text</p></kml>"));
        REQUIRE(editor.toKml() == QStringLiteral("<kml><p align=\"right\">Text</p></kml>"));
    }
}

TEST_CASE("KML load: the example of docs/kml_format.md is written back unchanged",
          "[editor][kml][load]") {
    const QStringList paragraphs = {
        QStringLiteral("<p align=\"center\"><b>Chapter One</b></p>"),
        QStringLiteral("<p>Plain text, <i>italic</i>, <b><i>bold italic</i></b> and "
                       "<span color=\"#aa0000\">red</span>.</p>"),
        QStringLiteral("<p>She met <charref id=\"r1\" target=\"anna\">Anna</charref> in "
                       "<locref id=\"r2\" target=\"krakow\">Kraków</locref>.</p>"),
        QStringLiteral("<p><comment id=\"c1\" author=\"Bartosz\" resolved=\"true\">"
                       "Check the date.</comment></p>")};

    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml>\n  ") + paragraphs.join(QStringLiteral("\n  ")) +
                   QStringLiteral("\n</kml>"));
    REQUIRE(editor.paragraphCount() == 4);
    REQUIRE(editor.toKml() == QStringLiteral("<kml>") + paragraphs.join(QString()) +
                                  QStringLiteral("</kml>"));
}
