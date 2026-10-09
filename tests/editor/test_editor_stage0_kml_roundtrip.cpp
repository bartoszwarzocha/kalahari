/// @file test_editor_stage0_kml_roundtrip.cpp
/// @brief Stage 0 safety net: KML load/save through the REAL BookEditor path
///
/// Every test drives BookEditor::fromKml() (KmlDocumentModel parse -> createDocument()
/// QTextDocument build) and BookEditor::toKml() (KmlSerializer). Nothing here exercises
/// KmlSerializer in isolation - it has its own unit tests. The point is
/// to pin down what survives the path the application actually uses when a chapter is
/// opened and saved.
///
/// A test that exposes a defect not fixed yet is tagged [known-bug] and [!mayfail]. Once
/// the defect is fixed the tags go and a "Regression" comment says what used to go wrong.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/chapter_document.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kml_format_registry.h>
#include "editor_test_utils.h"
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextDocument>

#include <utility>

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
// Metadata: annotations, footnotes
// =============================================================================

TEST_CASE("Stage0 KML: annotation anchor and id survive load and save", "[editor][stage0][kml]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral(
        "<kml><annotations><annotation id=\"c1\" kind=\"comment\">Why?</annotation></annotations>"
        "<p>Before <anchor ref=\"c1\">anchored</anchor> after</p></kml>"));

    const QTextCharFormat fmt =
        formatOfFragmentContaining(editor.textDocument()->begin(), QStringLiteral("anchored"));
    CHECK(annotationIds(fmt) == QStringList{QStringLiteral("c1")});

    const QString saved = editor.toKml();
    CHECK(saved.contains(QStringLiteral("<anchor ref=\"c1\">anchored</anchor>")));
    CHECK(saved.contains(
        QStringLiteral("<annotation id=\"c1\" kind=\"comment\">Why?</annotation>")));
}

TEST_CASE("Stage0 KML: annotation author/created/done and unknown attributes survive load",
          "[editor][stage0][kml]") {
    // Regression (fixed in Stage 1, for the comment and TODO elements of the time): loading
    // kept only the "id" attribute, so the others were lost on the next save.
    const QString saved = roundTrip(QStringLiteral(
        "<kml><annotations><annotation id=\"t1\" kind=\"todo\" author=\"Ann\" "
        "created=\"2026-01-02T03:04:05Z\" done=\"true\" priority=\"high\">Do it</annotation>"
        "</annotations><p><anchor ref=\"t1\">anchored</anchor></p></kml>"));
    CHECK(saved.contains(QStringLiteral("author=\"Ann\"")));
    CHECK(saved.contains(QStringLiteral("created=\"2026-01-02T03:04:05Z\"")));
    CHECK(saved.contains(QStringLiteral("done=\"true\"")));
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

TEST_CASE("Stage0 KML: an annotation added in the editor survives save and reload",
          "[editor][stage0][kml]") {
    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p>Some text here</p></kml>"));
    editor.setCursorPosition({0, 5});
    const Annotation added =
        editor.addAnnotation(AnnotationKind::Todo, QStringLiteral("Check this"), QStringLiteral("Ann"));

    BookEditor reloaded;
    reloaded.fromKml(editor.toKml());
    const std::vector<AnnotationPlace> after = reloaded.annotations();
    REQUIRE(after.size() == 1);
    CHECK(after.front().annotation == added);
    CHECK(after.front().start == 5);
    CHECK(after.front().end == 5);
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
        "<kml><annotations><annotation id=\"a&amp;b &quot;q&quot; &lt;x&gt;\" kind=\"note\" "
        "author=\"A &amp; B\">x</annotation></annotations>"
        "<p>One <anchor ref=\"a&amp;b &quot;q&quot; &lt;x&gt;\">anchored</anchor> two</p>"
        "<p>Next paragraph</p></kml>"));

    CHECK(editor.paragraphCount() == 2);
    CHECK(editor.paragraphPlainText(0) == QStringLiteral("One anchored two"));
    CHECK(editor.paragraphPlainText(1) == QStringLiteral("Next paragraph"));

    const QTextCharFormat fmt =
        formatOfFragmentContaining(editor.textDocument()->begin(), QStringLiteral("anchored"));
    CHECK(annotationIds(fmt) == QStringList{QStringLiteral("a&b \"q\" <x>")});
    CHECK(annotationsOf(fmt).value(0).author == QStringLiteral("A & B"));

    // And the values are escaped again on save
    const QString saved = editor.toKml();
    CHECK(saved.contains(QStringLiteral("id=\"a&amp;b &quot;q&quot; &lt;x&gt;\"")));
    CHECK(saved.contains(QStringLiteral("ref=\"a&amp;b &quot;q&quot; &lt;x&gt;\"")));
    CHECK(saved.contains(QStringLiteral("author=\"A &amp; B\"")));
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

// =============================================================================
// Reference files: the chapters of the example project
// =============================================================================

namespace {

/// The KML the editor writes for @p kml: the synonyms of the formatting tags become their
/// short form (docs/kml_format.md, "Inline formatting"); nothing else changes.
QString writtenForm(QString kml) {
    static const std::pair<const char*, const char*> synonyms[] = {
        {"bold", "b"},       {"strong", "b"},       {"italic", "i"},
        {"em", "i"},         {"underline", "u"},    {"strikethrough", "s"},
        {"strike", "s"},     {"subscript", "sub"},  {"superscript", "sup"},
    };
    for (const auto& [synonym, tag] : synonyms) {
        const QRegularExpression element(QStringLiteral("<(/?)%1(?=[\\s>/])")
                                             .arg(QLatin1String(synonym)));
        kml.replace(element, QStringLiteral("<\\1%1").arg(QLatin1String(tag)));
    }
    return kml;
}

}  // anonymous namespace

TEST_CASE("Stage0 KML: the chapters of the example project are saved unchanged",
          "[editor][stage0][kml][examples]") {
    const QString examples = QStringLiteral(KALAHARI_SOURCE_DIR "/examples");
    QStringList chapters;
    QDirIterator it(examples, {QStringLiteral("*.kchapter")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        chapters << it.next();
    }
    chapters.sort();
    REQUIRE(chapters.size() >= 4);

    for (const QString& path : chapters) {
        DYNAMIC_SECTION(QDir(examples).relativeFilePath(path).toStdString()) {
            const auto chapter = kalahari::core::ChapterDocument::load(path);
            REQUIRE(chapter.has_value());
            REQUIRE(chapter->hasContent());

            BookEditor editor;
            editor.fromKml(chapter->kml());

            // The saved chapter is the file as it is, apart from the synonyms
            const QString saved = editor.toKml();
            CHECK(saved == writtenForm(chapter->kml()));

            // The editor reads the text as the chapter's own reader does. (Not the plainText
            // stored in these files: older versions wrote it with other paragraph breaks,
            // and ChapterDocument writes it anew on save.)
            const QString text = kalahari::core::ChapterDocument::kmlToPlainText(chapter->kml());
            CHECK(editor.plainText() == text);
            CHECK(kalahari::core::ChapterDocument::kmlToPlainText(saved) == text);

            // A second save writes the same KML
            BookEditor reloaded;
            reloaded.fromKml(saved);
            CHECK(reloaded.toKml() == saved);
        }
    }
}
