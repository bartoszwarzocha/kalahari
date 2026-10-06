/// @file test_editor_stage0_kml_roundtrip.cpp
/// @brief Stage 0 safety net: KML load/save through the REAL BookEditor path
///
/// Every test drives BookEditor::fromKml() (KmlDocumentModel parse -> ensureEditMode()
/// QTextDocument build) and BookEditor::toKml() (KmlSerializer). Nothing here exercises
/// KmlParser/KmlSerializer in isolation - those have their own unit tests. The point is
/// to pin down what survives the path the application actually uses when a chapter is
/// opened and saved.
///
/// A test that exposes a defect not fixed yet is tagged [known-bug] and [!mayfail]. Once
/// the defect is fixed the tags go and a "Regression" comment says what used to go wrong.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/kml_format_registry.h>
#include "editor_test_utils.h"
#include <QTextBlock>
#include <QTextDocument>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// Load KML into a fresh editor and serialise it straight back.
QString roundTrip(const QString& kml) {
    BookEditor editor;
    editor.fromKml(kml);
    return editor.toKml();
}

}  // anonymous namespace

// =============================================================================
// Paragraph alignment
// =============================================================================

TEST_CASE("Stage0 KML: paragraph alignment survives load", "[editor][stage0][kml]") {
    struct Case {
        const char* attr;
        Qt::Alignment expected;
    };
    const Case cases[] = {
        {"left", Qt::AlignLeft},
        {"center", Qt::AlignHCenter},
        {"right", Qt::AlignRight},
        {"justify", Qt::AlignJustify},
    };

    for (const auto& c : cases) {
        DYNAMIC_SECTION("align=" << c.attr) {
            BookEditor editor;
            editor.fromKml(QStringLiteral("<kml><p>Left</p><p align=\"%1\">Aligned</p></kml>")
                               .arg(QLatin1String(c.attr)));

            QTextDocument* doc = editor.textDocument();
            REQUIRE(doc != nullptr);
            REQUIRE(doc->blockCount() == 2);

            // First paragraph has no attribute: no alignment of its own (shown justified)
            CHECK(ownAlignment(doc->begin().blockFormat()) == Qt::Alignment());

            // Second paragraph carries the attribute
            CHECK(ownAlignment(doc->begin().next().blockFormat()) == c.expected);
        }
    }
}

TEST_CASE("Stage0 KML: paragraph alignment survives save", "[editor][stage0][kml]") {
    const QString kml = QStringLiteral(
        "<kml><p align=\"center\">C</p><p align=\"right\">R</p>"
        "<p align=\"justify\">J</p><p align=\"left\">L</p><p>D</p></kml>");

    const QString saved = roundTrip(kml);
    CHECK(saved.contains(QStringLiteral("<p align=\"center\">C</p>")));
    CHECK(saved.contains(QStringLiteral("<p align=\"right\">R</p>")));
    CHECK(saved.contains(QStringLiteral("<p align=\"justify\">J</p>")));
    CHECK(saved.contains(QStringLiteral("<p align=\"left\">L</p>")));
    CHECK(saved.contains(QStringLiteral("<p>D</p>")));

    // Stable across a second load/save cycle
    CHECK(roundTrip(saved) == saved);
}

// =============================================================================
// Inline character formatting
// =============================================================================

TEST_CASE("Stage0 KML: bold/italic/underline/strike survive load", "[editor][stage0][kml]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral(
        "<kml><p>a <b>BOLD</b> b <i>ITAL</i> c <u>UNDER</u> d <s>STRIKE</s> e</p></kml>"));

    QTextDocument* doc = editor.textDocument();
    REQUIRE(doc != nullptr);
    const QTextBlock block = doc->begin();
    REQUIRE(block.text() == QStringLiteral("a BOLD b ITAL c UNDER d STRIKE e"));

    CHECK(formatOfFragmentContaining(block, QStringLiteral("BOLD")).fontWeight() == QFont::Bold);
    CHECK(formatOfFragmentContaining(block, QStringLiteral("ITAL")).fontItalic());
    CHECK(formatOfFragmentContaining(block, QStringLiteral("UNDER")).fontUnderline());
    CHECK(formatOfFragmentContaining(block, QStringLiteral("STRIKE")).fontStrikeOut());

    // Plain runs must stay plain (no format bleed between runs)
    const QTextCharFormat plain = formatOfFragmentContaining(block, QStringLiteral(" e"));
    CHECK(plain.fontWeight() != QFont::Bold);
    CHECK_FALSE(plain.fontItalic());
    CHECK_FALSE(plain.fontUnderline());
    CHECK_FALSE(plain.fontStrikeOut());
}

TEST_CASE("Stage0 KML: bold/italic/underline/strike survive save", "[editor][stage0][kml]") {
    const QString kml = QStringLiteral(
        "<kml><p>a <b>BOLD</b> b <i>ITAL</i> c <u>UNDER</u> d <s>STRIKE</s> e</p></kml>");

    const QString saved = roundTrip(kml);
    CHECK(saved.contains(QStringLiteral("<b>BOLD</b>")));
    CHECK(saved.contains(QStringLiteral("<i>ITAL</i>")));
    CHECK(saved.contains(QStringLiteral("<u>UNDER</u>")));
    CHECK(saved.contains(QStringLiteral("<s>STRIKE</s>")));
    CHECK(roundTrip(saved) == saved);
}

TEST_CASE("Stage0 KML: nested formatting survives load and save", "[editor][stage0][kml]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p>x <b><i>BOTH</i></b> y</p></kml>"));

    const QTextCharFormat both =
        formatOfFragmentContaining(editor.textDocument()->begin(), QStringLiteral("BOTH"));
    CHECK(both.fontWeight() == QFont::Bold);
    CHECK(both.fontItalic());

    // Re-load the saved text and check the formatting again (tag order is free)
    BookEditor reloaded;
    reloaded.fromKml(editor.toKml());
    const QTextCharFormat again =
        formatOfFragmentContaining(reloaded.textDocument()->begin(), QStringLiteral("BOTH"));
    CHECK(again.fontWeight() == QFont::Bold);
    CHECK(again.fontItalic());
    CHECK(reloaded.plainText() == QStringLiteral("x BOTH y"));
}

TEST_CASE("Stage0 KML: formatting does not bleed into the next paragraph", "[editor][stage0][kml]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p><b>Title</b></p><p>Body text</p></kml>"));

    const QTextBlock body = editor.textDocument()->begin().next();
    const QTextCharFormat fmt = formatOfFragmentContaining(body, QStringLiteral("Body"));
    CHECK(fmt.fontWeight() != QFont::Bold);
    CHECK(editor.toKml().contains(QStringLiteral("<p>Body text</p>")));
}

// =============================================================================
// Metadata: comments, TODO, footnotes
// =============================================================================

TEST_CASE("Stage0 KML: comment anchor and id survive load and save", "[editor][stage0][kml]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral(
        "<kml><p>Before <comment id=\"c1\">anchored</comment> after</p></kml>"));

    const QTextCharFormat fmt =
        formatOfFragmentContaining(editor.textDocument()->begin(), QStringLiteral("anchored"));
    REQUIRE(fmt.hasProperty(KmlPropComment));
    CHECK(metadataOf(fmt, KmlPropComment).value(QStringLiteral("id")).toString() ==
          QStringLiteral("c1"));

    const QString saved = editor.toKml();
    CHECK(saved.contains(QStringLiteral("<comment id=\"c1\">anchored</comment>")));
}

TEST_CASE("Stage0 KML: comment author/created/resolved survive load", "[editor][stage0][kml]") {
    // Regression (fixed in Stage 1): loading used to keep only the "id" attribute of
    // metadata tags, so author/created/resolved (and todo completed/priority, footnote
    // number) were lost on the next save.
    BookEditor editor;
    editor.fromKml(QStringLiteral(
        "<kml><p><comment id=\"c1\" author=\"Ann\" created=\"2026-01-02T03:04:05\" "
        "resolved=\"true\">anchored</comment></p></kml>"));

    const QString saved = editor.toKml();
    CHECK(saved.contains(QStringLiteral("author=\"Ann\"")));
    CHECK(saved.contains(QStringLiteral("created=\"2026-01-02T03:04:05\"")));
    CHECK(saved.contains(QStringLiteral("resolved=\"true\"")));
}

TEST_CASE("Stage0 KML: todo anchor and id survive load and save", "[editor][stage0][kml]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p>Do <todo id=\"t1\">this</todo> now</p></kml>"));

    const QTextCharFormat fmt =
        formatOfFragmentContaining(editor.textDocument()->begin(), QStringLiteral("this"));
    REQUIRE(fmt.hasProperty(KmlPropTodo));
    CHECK(editor.toKml().contains(QStringLiteral("<todo id=\"t1\">this</todo>")));
}

TEST_CASE("Stage0 KML: todo completed/priority survive load", "[editor][stage0][kml]") {
    // Regression (fixed in Stage 1): same cause as the comment-attribute test above.
    const QString saved = roundTrip(QStringLiteral(
        "<kml><p><todo id=\"t1\" completed=\"true\" priority=\"high\">x</todo></p></kml>"));
    CHECK(saved.contains(QStringLiteral("completed=\"true\"")));
    CHECK(saved.contains(QStringLiteral("priority=\"high\"")));
}

TEST_CASE("Stage0 KML: footnote anchor and id survive load and save", "[editor][stage0][kml]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p>Text<footnote id=\"f1\">1</footnote>.</p></kml>"));

    const QTextCharFormat fmt =
        formatOfFragmentContaining(editor.textDocument()->begin(), QStringLiteral("1"));
    REQUIRE(fmt.hasProperty(KmlPropFootnote));
    CHECK(editor.toKml().contains(QStringLiteral("<footnote id=\"f1\">1</footnote>")));
}

TEST_CASE("Stage0 KML: footnote number survives load", "[editor][stage0][kml]") {
    // Regression (fixed in Stage 1): same cause as the comment-attribute test above.
    const QString saved = roundTrip(QStringLiteral(
        "<kml><p>Text<footnote id=\"f1\" number=\"7\">7</footnote>.</p></kml>"));
    CHECK(saved.contains(QStringLiteral("number=\"7\"")));
}

TEST_CASE("Stage0 KML: TODO marker added in the editor survives save and reload",
          "[editor][stage0][kml]") {
    // Regression (fixed in Stage 1): markers used to be stored as a JSON string under
    // KmlPropTodo, while the serializer and the parser use an attribute map, so a marker
    // was saved as a bare <todo> and was invisible to findAllMarkers() after reload.
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p>Some text here</p></kml>"));
    editor.setCursorPosition({0, 5});
    editor.addTodoAtCursor(QStringLiteral("Check this"));

    auto before = findAllMarkers(editor.textDocument());
    REQUIRE(before.size() == 1);
    REQUIRE(before.front().text == QStringLiteral("Check this"));

    BookEditor reloaded;
    reloaded.fromKml(editor.toKml());
    auto after = findAllMarkers(reloaded.textDocument());
    REQUIRE(after.size() == 1);
    CHECK(after.front().text == QStringLiteral("Check this"));
    CHECK(after.front().id == before.front().id);
    CHECK(reloaded.plainText() == QStringLiteral("Some text here"));
}

// =============================================================================
// XML special characters
// =============================================================================

TEST_CASE("Stage0 KML: special characters in text survive load and save", "[editor][stage0][kml]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral(
        "<kml><p>Tom &amp; Jerry &lt;tag&gt; \"quoted\" it&apos;s</p></kml>"));

    CHECK(editor.plainText() == QStringLiteral("Tom & Jerry <tag> \"quoted\" it's"));

    const QString saved = editor.toKml();
    CHECK(saved.contains(QStringLiteral("Tom &amp; Jerry &lt;tag&gt;")));
    CHECK_FALSE(saved.contains(QStringLiteral("<tag>")));

    BookEditor reloaded;
    reloaded.fromKml(saved);
    CHECK(reloaded.plainText() == editor.plainText());
}

TEST_CASE("Stage0 KML: special characters inside formatted runs survive", "[editor][stage0][kml]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p>a <b>R&amp;D &lt;x&gt;</b> b</p></kml>"));
    CHECK(editor.plainText() == QStringLiteral("a R&D <x> b"));

    BookEditor reloaded;
    reloaded.fromKml(editor.toKml());
    CHECK(reloaded.plainText() == QStringLiteral("a R&D <x> b"));
    CHECK(formatOfFragmentContaining(reloaded.textDocument()->begin(), QStringLiteral("R&D"))
              .fontWeight() == QFont::Bold);
}

TEST_CASE("Stage0 KML: special characters typed in the editor survive save", "[editor][stage0][kml]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p></p></kml>"));
    editor.insertText(QStringLiteral("A & B < C > D \" E"));

    BookEditor reloaded;
    reloaded.fromKml(editor.toKml());
    CHECK(reloaded.plainText() == QStringLiteral("A & B < C > D \" E"));
}

TEST_CASE("Stage0 KML: special characters in attribute values survive load",
          "[editor][stage0][kml]") {
    // Regression (fixed in Stage 1): loading used to re-serialise each <p> into a string,
    // rebuilding attributes from decoded values without escaping them, so a value with
    // & < or " produced malformed XML and the rest of the paragraph was lost.
    BookEditor editor;
    editor.fromKml(QStringLiteral(
        "<kml><p>One <comment id=\"a&amp;b &quot;q&quot; &lt;x&gt;\">anchored</comment> two</p>"
        "<p>Next paragraph</p></kml>"));

    CHECK(editor.paragraphCount() == 2);
    CHECK(editor.paragraphPlainText(0) == QStringLiteral("One anchored two"));
    CHECK(editor.paragraphPlainText(1) == QStringLiteral("Next paragraph"));

    const QTextCharFormat fmt =
        formatOfFragmentContaining(editor.textDocument()->begin(), QStringLiteral("anchored"));
    CHECK(metadataOf(fmt, KmlPropComment).value(QStringLiteral("id")).toString() ==
          QStringLiteral("a&b \"q\" <x>"));

    // And the value is escaped again on save
    CHECK(editor.toKml().contains(QStringLiteral("id=\"a&amp;b &quot;q&quot; &lt;x&gt;\"")));
}

// =============================================================================
// Whole-document invariants
// =============================================================================

TEST_CASE("Stage0 KML: paragraph count, empty paragraphs and whitespace survive",
          "[editor][stage0][kml]") {
    const QString kml = QStringLiteral(
        "<kml><p>First</p><p></p><p>  two leading spaces</p><p>Last</p></kml>");

    BookEditor editor;
    editor.fromKml(kml);
    REQUIRE(editor.paragraphCount() == 4);
    CHECK(editor.paragraphPlainText(1).isEmpty());
    CHECK(editor.paragraphPlainText(2) == QStringLiteral("  two leading spaces"));

    BookEditor reloaded;
    reloaded.fromKml(editor.toKml());
    CHECK(reloaded.paragraphCount() == 4);
    CHECK(reloaded.plainText() == editor.plainText());
}

TEST_CASE("Stage0 KML: loading does not leave an undoable step or a dirty document",
          "[editor][stage0][kml]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p>Alpha</p><p>Beta</p></kml>"));
    CHECK_FALSE(editor.canUndo());
    CHECK_FALSE(editor.canRedo());
}
