/// @file test_editor_stage5.cpp
/// @brief Editor Stage 5: one highlight layer (annotations from the KML data, check
///        results kept with the paragraphs, the word read aloud); replacing the content
///        as one undo step

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
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

/// The margin left of a paragraph's first line (where marker icons are drawn)
QRect marginOf(BookEditor& editor, int paragraph) {
    const QRectF line = caretAt(editor, {paragraph, 0});
    return QRect(QPoint(0, static_cast<int>(line.top())),
                 QPoint(static_cast<int>(line.left()) - 2, static_cast<int>(line.bottom())));
}

}  // anonymous namespace

// =============================================================================
// Annotations come from the KML data
// =============================================================================

TEST_CASE("Stage5 highlights: comments and markers come from the KML data",
          "[editor][stage5][highlight]") {
    auto editor = editorWith(QStringLiteral(
        "<kml><p>Start <comment id=\"c1\" author=\"A\">noted <b>text</b></comment> and "
        "<todo id=\"t1\">fix this</todo>.</p>"
        "<p><todo id=\"t2\" completed=\"true\">done</todo> <todo id=\"n1\" type=\"note\">aside</todo> "
        "<comment id=\"c2\" resolved=\"true\">old</comment></p>"
        "<p><comment id=\"c3\">one</comment><comment id=\"c4\">two</comment></p></kml>"));

    // A comment over a bold word is one highlight
    CHECK(highlightsOf(*editor, 0) == std::vector<TextHighlight>{
                                          {6, 10, HighlightKind::Comment},
                                          {21, 8, HighlightKind::Todo}});
    CHECK(highlightsOf(*editor, 1) == std::vector<TextHighlight>{
                                          {0, 4, HighlightKind::CompletedTodo},
                                          {5, 5, HighlightKind::Note},
                                          {11, 3, HighlightKind::ResolvedComment}});
    // Two comments side by side stay two
    CHECK(highlightsOf(*editor, 2) == std::vector<TextHighlight>{
                                          {0, 3, HighlightKind::Comment},
                                          {3, 3, HighlightKind::Comment}});
}

TEST_CASE("Stage5 highlights: typed TODO and comment patterns are plain text",
          "[editor][stage5][highlight]") {
    // Regression: paragraphs starting with "TODO:", "[NOTE]" or "[x]" got a marker icon and
    // a tint, and "/* */" or "<!-- -->" in the text a comment highlight, while TODO
    // markers and comments saved in KML were not shown
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

TEST_CASE("Stage5 highlights: annotations are painted on their text, markers with an icon",
          "[editor][stage5][highlight]") {
    auto annotated = editorWith(QStringLiteral(
        "<kml><p>Plain <comment id=\"c1\">noted</comment> words here</p>"
        "<p>Some <todo id=\"t1\">fix</todo> more words</p>"
        "<p>Only plain words</p></kml>"));
    auto plain = editorWith(kmlOf({QStringLiteral("Plain noted words here"),
                                   QStringLiteral("Some fix more words"),
                                   QStringLiteral("Only plain words")}));
    const QRect words = rangeArea(*annotated, 0, 0, 5);
    const QRect comment = rangeArea(*annotated, 0, 6, 11);
    const QRect todo = rangeArea(*annotated, 1, 5, 8);
    const QRect margins[] = {marginOf(*annotated, 0), marginOf(*annotated, 1),
                             marginOf(*annotated, 2)};
    plain->setCursorPosition(annotated->cursorPosition());  // the caret at the same place
    const QImage withAnnotations = editorImage(*annotated);
    const QImage without = editorImage(*plain);

    CHECK(differingPixels(withAnnotations, without, comment) > comment.width() * comment.height() / 2);
    CHECK(differingPixels(withAnnotations, without, todo) > todo.width() * todo.height() / 2);
    CHECK(differingPixels(withAnnotations, without, words) == 0);

    // The TODO marker has an icon in the margin, level with its line
    CHECK(differingPixels(withAnnotations, without, margins[1]) > 0);
    CHECK(differingPixels(withAnnotations, without, margins[0]) == 0);
    CHECK(differingPixels(withAnnotations, without, margins[2]) == 0);
}

TEST_CASE("Stage5 highlights: a marker added in the editor is shown", "[editor][stage5][highlight]") {
    auto editor = editorWith(kmlOf({QStringLiteral("Words to do")}));
    editor->setCursorPosition({0, 0});
    editor->addTodoAtCursor();
    CHECK(highlightsOf(*editor, 0) == std::vector<TextHighlight>{{0, 1, HighlightKind::Todo}});

    editor->toggleTodoAtCursor();
    CHECK(highlightsOf(*editor, 0) ==
          std::vector<TextHighlight>{{0, 1, HighlightKind::CompletedTodo}});

    editor->undo();
    editor->undo();
    CHECK(highlightsOf(*editor, 0).empty());
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
