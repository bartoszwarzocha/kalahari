/// @file test_editor_stage5.cpp
/// @brief Editor Stage 5: one highlight layer (check results kept with the paragraphs, the
///        word read aloud) that leaves annotations and typed patterns as plain text;
///        replacing the content as one undo step; what files outside a project need from
///        the editor; Focus mode dims the paragraphs other than the cursor's

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/grammar_check_service.h>
#include <kalahari/editor/spell_check_service.h>
#include <kalahari/editor/text_source_adapter.h>
#include "editor_test_utils.h"

#include <QImage>
#include <QTextBlock>
#include <memory>
#include <vector>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// Editor holding @p kml, sized like a small window
std::unique_ptr<BookEditor> editorWith(const QString& kml) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(kml);
    return editor;
}

/// Highlights the editor's document gives for a paragraph
std::vector<TextHighlight> highlightsOf(BookEditor& editor, size_t paragraph) {
    return QTextDocumentSource(editor.textDocument()).paragraphHighlights(paragraph);
}

/// Image of the editor with one pixel per logical pixel (see test_editor_stage2.cpp)
QImage editorImage(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    editor.render(&image);
    return image;
}

/// Caret rectangle at a position (moves the cursor there)
QRectF caretAt(BookEditor& editor, const CursorPosition& position) {
    editor.setCursorPosition(position);
    return editor.inputMethodQuery(Qt::ImCursorRectangle).toRectF();
}

/// Area of a text range within one line: from the caret before it to the caret after it
QRect rangeArea(BookEditor& editor, int paragraph, int start, int end) {
    const QRectF first = caretAt(editor, {paragraph, start});
    const QRectF last = caretAt(editor, {paragraph, end});
    return QRect(QPoint(static_cast<int>(first.right()) + 1, static_cast<int>(first.top())),
                 QPoint(static_cast<int>(last.left()) - 1, static_cast<int>(first.bottom())));
}

/// Pixels of an area that differ between two images of the same size
///
/// Images of the same text are compared, so the glyphs (antialiased with colored fringes
/// on some platforms) cancel out and only the highlights remain.
int differingPixels(const QImage& a, const QImage& b, const QRect& area) {
    int count = 0;
    const QRect clipped = area & a.rect();
    for (int y = clipped.top(); y <= clipped.bottom(); ++y) {
        for (int x = clipped.left(); x <= clipped.right(); ++x) {
            if (a.pixel(x, y) != b.pixel(x, y)) {
                ++count;
            }
        }
    }
    return count;
}

/// Pixels of an area other than @p color
int pixelsOtherThan(const QImage& image, QRgb color, const QRect& area) {
    int count = 0;
    const QRect clipped = area & image.rect();
    for (int y = clipped.top(); y <= clipped.bottom(); ++y) {
        for (int x = clipped.left(); x <= clipped.right(); ++x) {
            if (image.pixel(x, y) != color) {
                ++count;
            }
        }
    }
    return count;
}

/// The margin left of a paragraph's first line
QRect marginOf(BookEditor& editor, int paragraph) {
    const QRectF line = caretAt(editor, {paragraph, 0});
    return QRect(QPoint(0, static_cast<int>(line.top())),
                 QPoint(static_cast<int>(line.left()) - 2, static_cast<int>(line.bottom())));
}

}  // anonymous namespace

// =============================================================================
// Annotations and typed patterns are plain text
// =============================================================================

TEST_CASE("Stage5 highlights: annotations in the KML leave the text as it is",
          "[editor][stage5][highlight]") {
    // Comments, TODO markers and notes stay in the chapter (see the Stage0 KML tests) but
    // are not drawn until their look is designed: no tint, underline or margin icon
    auto annotated = editorWith(QStringLiteral(
        "<kml><p>Start <comment id=\"c1\" author=\"A\">noted <b>text</b></comment> and "
        "<todo id=\"t1\">fix this</todo> now</p>"
        "<p><todo id=\"t2\" completed=\"true\">done</todo> <todo id=\"n1\" type=\"note\">aside</todo> "
        "<comment id=\"c2\" resolved=\"true\">old</comment></p></kml>"));
    auto plain = editorWith(kmlOf({QStringLiteral("Start noted <b>text</b> and fix this now"),
                                   QStringLiteral("done aside old")}));
    REQUIRE(annotated->plainText() == plain->plainText());

    CHECK(highlightsOf(*annotated, 0).empty());
    CHECK(highlightsOf(*annotated, 1).empty());
    plain->setCursorPosition(annotated->cursorPosition());  // the caret at the same place
    CHECK(differingPixels(editorImage(*annotated), editorImage(*plain), annotated->rect()) == 0);
}

TEST_CASE("Stage5 highlights: typed TODO and comment patterns are plain text",
          "[editor][stage5][highlight]") {
    // Regression: paragraphs starting with "TODO:", "[NOTE]" or "[x]" got a marker icon and
    // a tint, and "/* */" or "<!-- -->" in the text a comment highlight
    auto editor = editorWith(kmlOf({QStringLiteral("TODO: write the ending"),
                                    QStringLiteral("[NOTE] check the dates"),
                                    QStringLiteral("[x] done already"),
                                    QStringLiteral("Text /* not a comment */ and &lt;!-- this --&gt;")}));
    for (size_t paragraph = 0; paragraph < 4; ++paragraph) {
        CHECK(highlightsOf(*editor, paragraph).empty());
    }

    // Nothing in the margins or behind the lines (the icon and the tint across the column)
    const QImage image = editorImage(*editor);
    const QRgb paper = image.pixel(2, 2);
    for (int paragraph = 0; paragraph < 4; ++paragraph) {
        CHECK(pixelsOtherThan(image, paper, marginOf(*editor, paragraph)) == 0);
        const int length = static_cast<int>(editor->textDocument()
                                                ->findBlockByNumber(paragraph).text().length());
        const QRectF end = caretAt(*editor, {paragraph, length});
        CHECK(pixelsOtherThan(image, paper,
                              QRect(QPoint(static_cast<int>(end.right()) + 2,
                                           static_cast<int>(end.top())),
                                    QPoint(editor->width() - 30,
                                           static_cast<int>(end.bottom())))) == 0);
    }
}

// =============================================================================
// Check results kept with the paragraphs
// =============================================================================

TEST_CASE("Stage5 highlights: check results apply while the paragraph keeps its text",
          "[editor][stage5][highlight]") {
    auto editor = editorWith(kmlOf({QStringLiteral("Teh cat sat"), QStringLiteral("Other line")}));
    SpellCheckService spelling;
    GrammarCheckService grammar;
    grammar.setEnabled(false);  // no initial check of the document
    editor->setSpellCheckService(&spelling);
    editor->setGrammarCheckService(&grammar);

    emit spelling.paragraphChecked(0, {SpellErrorInfo(0, 3, QStringLiteral("Teh"))});
    emit grammar.paragraphChecked(0, {GrammarError(4, 7, QStringLiteral("cat sat"))});
    const std::vector<TextHighlight> checked{{0, 3, HighlightKind::Spelling},
                                             {4, 7, HighlightKind::Grammar}};
    CHECK(highlightsOf(*editor, 0) == checked);
    CHECK(highlightsOf(*editor, 1).empty());

    SECTION("an edit of the paragraph makes them stale") {
        editor->setCursorPosition({0, 11});
        editor->insertText(QStringLiteral("!"));
        CHECK(highlightsOf(*editor, 0).empty());

        // Back to the text they were made for
        editor->undo();
        CHECK(highlightsOf(*editor, 0) == checked);
    }

    SECTION("an edit of another paragraph keeps them") {
        editor->setCursorPosition({1, 0});
        editor->insertText(QStringLiteral("An "));
        CHECK(highlightsOf(*editor, 0) == checked);
    }

    SECTION("a new check replaces them") {
        emit spelling.paragraphChecked(0, {});
        CHECK(highlightsOf(*editor, 0) ==
              std::vector<TextHighlight>{{4, 7, HighlightKind::Grammar}});
    }

    SECTION("results for text the paragraph no longer has are dropped") {
        // Made before "sat" became "Teh"...: the word is not at its place any more
        emit spelling.paragraphChecked(0, {SpellErrorInfo(8, 3, QStringLiteral("set")),
                                           SpellErrorInfo(0, 3, QStringLiteral("Teh")),
                                           SpellErrorInfo(9, 5, QStringLiteral("sat"))});
        CHECK(highlightsOf(*editor, 0) == checked);
    }
}

TEST_CASE("Stage5 highlights: spelling and grammar issues are drawn as waves",
          "[editor][stage5][highlight]") {
    auto editor = editorWith(kmlOf({QStringLiteral("Teh cat sat on the mat")}));
    SpellCheckService spelling;
    editor->setSpellCheckService(&spelling);
    const QRect word = rangeArea(*editor, 0, 0, 3);
    const QRect rest = rangeArea(*editor, 0, 4, 22);
    const QImage plain = editorImage(*editor);

    emit spelling.paragraphChecked(0, {SpellErrorInfo(0, 3, QStringLiteral("Teh"))});
    const QImage checked = editorImage(*editor);
    CHECK(differingPixels(plain, checked, word) > word.width() / 2);
    CHECK(differingPixels(plain, checked, rest) == 0);
}

// =============================================================================
// The word read aloud
// =============================================================================

TEST_CASE("Stage5 highlights: the word read aloud is highlighted", "[editor][stage5][highlight]") {
    auto editor = editorWith(kmlOf({QStringLiteral("Read this sentence aloud")}));
    const QRect word = rangeArea(*editor, 0, 5, 9);
    const QRect other = rangeArea(*editor, 0, 10, 18);
    const QImage plain = editorImage(*editor);

    editor->setSpokenWord(0, 5, 4);
    const QImage spoken = editorImage(*editor);
    CHECK(differingPixels(plain, spoken, word) > word.width() * word.height() / 2);
    CHECK(differingPixels(plain, spoken, other) == 0);

    editor->setSpokenWord(0, 0, 0);
    CHECK(differingPixels(plain, editorImage(*editor), editor->rect()) == 0);
}

// =============================================================================
// Replacing the content as one undo step
// =============================================================================

TEST_CASE("Stage5 replace: the new content is one undo step", "[editor][stage5][replace]") {
    const QString first = QStringLiteral(
        "<kml><p align=\"center\">First <b>version</b></p><p>with <todo id=\"t1\">two</todo> "
        "paragraphs</p></kml>");
    const QString second = QStringLiteral(
        "<kml><p>Second <i>version</i></p><p align=\"right\">of the</p><p>chapter</p></kml>");
    auto editor = editorWith(first);
    editor->setCursorPosition({1, 4});
    editor->insertText(QStringLiteral("just "));
    const QString edited = editor->toKml();
    editor->setCursorPosition({1, 3});

    editor->replaceWithKml(second);
    CHECK(editor->toKml() == editorWith(second)->toKml());
    CHECK(editor->cursorPosition() == CursorPosition{1, 3});

    editor->undo();
    CHECK(editor->toKml() == edited);
    CHECK(editor->cursorPosition() == CursorPosition{1, 3});

    // The typing before it is a step of its own
    editor->undo();
    CHECK(editor->toKml() == editorWith(first)->toKml());
    CHECK_FALSE(editor->canUndo());

    editor->redo();
    editor->redo();
    CHECK(editor->toKml() == editorWith(second)->toKml());
}

TEST_CASE("Stage5 replace: the cursor stays within shorter content", "[editor][stage5][replace]") {
    auto editor = editorWith(kmlOf({QStringLiteral("One long paragraph"), QStringLiteral("Two"),
                                    QStringLiteral("Three paragraphs")}));
    editor->setCursorPosition({2, 10});
    editor->setSelection({{0, 4}, {2, 10}});
    REQUIRE(editor->hasSelection());

    editor->replaceWithKml(kmlOf({QStringLiteral("Short")}));
    CHECK(editor->toKml() == kmlOf({QStringLiteral("Short")}));
    CHECK(editor->cursorPosition() == CursorPosition{0, 5});
    CHECK_FALSE(editor->hasSelection());

    editor->setCursorPosition({0, 0});
    editor->insertText(QStringLiteral("Very "));
    CHECK(editor->toKml() == kmlOf({QStringLiteral("Very Short")}));
}

TEST_CASE("Stage5 replace: an editor without a chapter gets the content",
          "[editor][stage5][replace]") {
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    const QString kml = kmlOf({QStringLiteral("Restored"), QStringLiteral("text")});
    editor.replaceWithKml(kml);
    CHECK(editor.toKml() == kml);
    editor.undo();
    CHECK(editor.plainText().isEmpty());
}

TEST_CASE("Stage5 files: loading says whether the whole KML was read", "[editor][stage5][files]") {
    BookEditor editor;
    resizeWidget(editor, QSize(600, 400));
    CHECK(editor.fromKml(kmlOf({QStringLiteral("Whole"), QStringLiteral("chapter")})));
    CHECK(editor.fromKml(QString()));

    // A damaged chapter shows the text before the damaged place
    CHECK_FALSE(editor.fromKml(QStringLiteral("<p><t>Before</t></p><p><t>broken</b></p><p><t>After</t></p>")));
    CHECK(editor.plainText().startsWith(QStringLiteral("Before\n")));
    CHECK_FALSE(editor.plainText().contains(QStringLiteral("After")));
}

TEST_CASE("Stage5 files: the plain text keeps no-break spaces", "[editor][stage5][files]") {
    auto editor = editorWith(kmlOf({QStringLiteral("Szed\u0142 w\u00A0d\u00F3\u0142"), QStringLiteral("two")}));
    CHECK(editor->plainText() == QStringLiteral("Szed\u0142 w\u00A0d\u00F3\u0142\ntwo"));
}

TEST_CASE("Stage5 files: text with CR LF line ends gives one paragraph per line",
          "[editor][stage5][files]") {
    auto editor = editorWith(ClipboardHandler::textToKml(QStringLiteral("one\r\ntwo\r\n")));
    CHECK(editor->paragraphCount() == 3);
    CHECK(editor->plainText() == QStringLiteral("one\ntwo\n"));
}

// =============================================================================
// Focus mode
// =============================================================================

TEST_CASE("Stage5 focus: the paragraphs other than the cursor's are dimmed", "[editor][stage5][focus]") {
    auto editor = editorWith(kmlOf({QStringLiteral("First paragraph"), QStringLiteral("Second paragraph"),
                                    QStringLiteral("Third paragraph")}));
    // Focus mode lays the text out as the continuous view, so the same areas hold the text
    const QRect first = rangeArea(*editor, 0, 0, 5);
    const QRect second = rangeArea(*editor, 1, 0, 6);
    const QRect third = rangeArea(*editor, 2, 0, 5);
    editor->setCursorPosition({1, 2});
    const QImage continuous = editorImage(*editor);

    editor->setViewMode(ViewMode::Focus);
    const QImage focus = editorImage(*editor);
    CHECK(differingPixels(continuous, focus, first) > 0);
    CHECK(differingPixels(continuous, focus, second) == 0);
    CHECK(differingPixels(continuous, focus, third) > 0);

    SECTION("the focus follows the cursor") {
        editor->setCursorPosition({2, 0});
        const QImage moved = editorImage(*editor);
        CHECK(differingPixels(continuous, moved, second) > 0);
        CHECK(differingPixels(continuous, moved, third) == 0);
    }

    SECTION("leaving focus mode shows all the text as before") {
        editor->setViewMode(ViewMode::Continuous);
        CHECK(differingPixels(continuous, editorImage(*editor), editor->rect()) == 0);
    }
}
