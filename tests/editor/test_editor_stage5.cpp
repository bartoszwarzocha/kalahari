/// @file test_editor_stage5.cpp
/// @brief Editor Stage 5: one highlight layer (check results kept with the paragraphs, the
///        word read aloud) that leaves annotations and typed patterns as plain text;
///        replacing the content as one undo step; what files outside a project need from
///        the editor; Focus dims the paragraphs other than the cursor's, in any view; the
///        continuous view is one endless page; Distraction-Free darkens the sides of any
///        view; the first show zooms a page wider than the view out to its width

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/grammar_check_service.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/spell_check_service.h>
#include <kalahari/editor/text_source_adapter.h>
#include "editor_test_utils.h"

#include <QImage>
#include <QMouseEvent>
#include <QScreen>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextLayout>
#include <QWheelEvent>
#include <cmath>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

using namespace kalahari::editor;
using namespace kalahari::test;
using Catch::Approx;

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

/// The page's margin left of a paragraph's first line (40 pixels of the default inch)
QRect marginOf(BookEditor& editor, int paragraph) {
    const QRectF line = caretAt(editor, {paragraph, 0});
    return QRect(QPoint(static_cast<int>(line.left()) - 40, static_cast<int>(line.top())),
                 QPoint(static_cast<int>(line.left()) - 2, static_cast<int>(line.bottom())));
}

}  // anonymous namespace

// =============================================================================
// Annotations and typed patterns are plain text
// =============================================================================

TEST_CASE("Stage5 highlights: annotations in the KML leave the text as it is",
          "[editor][stage5][highlight]") {
    // Comments, TODOs and notes stay in the chapter (see the annotation tests) but are not
    // drawn until their look is designed: no tint, underline or margin icon
    auto annotated = editorWith(QStringLiteral(
        "<kml><annotations><annotation id=\"c1\" kind=\"comment\" author=\"A\">C</annotation>"
        "<annotation id=\"t1\" kind=\"todo\">T</annotation>"
        "<annotation id=\"t2\" kind=\"todo\" done=\"true\">D</annotation>"
        "<annotation id=\"n1\" kind=\"note\">N</annotation>"
        "<annotation id=\"c2\" kind=\"comment\" done=\"true\">R</annotation></annotations>"
        "<p>Start <anchor ref=\"c1\">noted <b>text</b></anchor> and "
        "<anchor ref=\"t1\">fix this</anchor> now<anchor ref=\"n1\"/></p>"
        "<p><anchor ref=\"t2\">done</anchor> aside <anchor ref=\"c2\">old</anchor></p></kml>"));
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

    // Nothing in the margins or behind the lines (the icon and the tint across the column).
    // The paper from the page's top margin (the desk is around the page).
    const QImage image = editorImage(*editor);
    const QRectF first = caretAt(*editor, {0, 0});
    const QRgb paper =
        image.pixel(static_cast<int>(first.left()), static_cast<int>(first.top()) - 10);
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
        "<kml><annotations><annotation id=\"t1\" kind=\"todo\">Check</annotation></annotations>"
        "<p align=\"center\">First <b>version</b></p><p>with <anchor ref=\"t1\">two</anchor> "
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
// Focus
// =============================================================================

TEST_CASE("Stage5 focus: the paragraphs other than the cursor's are dimmed", "[editor][stage5][focus]") {
    auto editor = editorWith(kmlOf({QStringLiteral("First paragraph"), QStringLiteral("Second paragraph"),
                                    QStringLiteral("Third paragraph")}));
    // Focus only dims: the layout stays, so the same areas hold the text
    const QRect first = rangeArea(*editor, 0, 0, 5);
    const QRect second = rangeArea(*editor, 1, 0, 6);
    const QRect third = rangeArea(*editor, 2, 0, 5);
    editor->setCursorPosition({1, 2});
    const QImage continuous = editorImage(*editor);

    editor->setFocusModeEnabled(true);
    CHECK(editor->viewMode() == ViewMode::Continuous);
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

    SECTION("turning Focus off shows all the text as before") {
        editor->setFocusModeEnabled(false);
        CHECK(differingPixels(continuous, editorImage(*editor), editor->rect()) == 0);
    }
}

TEST_CASE("Stage5 focus: a toggle that keeps the pages of the page view", "[editor][stage5][focus]") {
    // Regression: Focus was a view mode of its own, so turning it on in the page view
    // left the pages for the continuous view
    auto editor = editorWith(kmlOf({QStringLiteral("First paragraph"), QStringLiteral("Second paragraph"),
                                    QStringLiteral("Third paragraph")}));
    editor->setViewMode(ViewMode::Page);
    const QRect first = rangeArea(*editor, 0, 0, 5);
    const QRect second = rangeArea(*editor, 1, 0, 6);
    editor->setCursorPosition({1, 2});
    const int pages = editor->totalPages();
    REQUIRE(pages >= 1);
    const QImage page = editorImage(*editor);

    int changes = 0;
    QObject::connect(editor.get(), &BookEditor::focusModeChanged, [&changes](bool) { ++changes; });
    editor->setFocusModeEnabled(true);
    editor->setFocusModeEnabled(true);
    CHECK(changes == 1);
    CHECK(editor->isFocusModeEnabled());
    CHECK(editor->viewMode() == ViewMode::Page);
    CHECK(editor->totalPages() == pages);
    CHECK(rangeArea(*editor, 0, 0, 5) == first);
    editor->setCursorPosition({1, 2});
    const QImage focus = editorImage(*editor);
    CHECK(differingPixels(page, focus, first) > 0);
    CHECK(differingPixels(page, focus, second) == 0);

    editor->setFocusModeEnabled(false);
    CHECK(changes == 2);
    CHECK(editor->viewMode() == ViewMode::Page);
    CHECK(differingPixels(page, editorImage(*editor), editor->rect()) == 0);
}

TEST_CASE("Stage5 focus: the appearance turns Focus on", "[editor][stage5][focus]") {
    // The settings reach the editor through its appearance (editor.focus.enabled)
    auto editor = editorWith(kmlOf({QStringLiteral("First paragraph"), QStringLiteral("Second paragraph")}));
    const QRect first = rangeArea(*editor, 0, 0, 5);
    editor->setCursorPosition({1, 2});
    const QImage plain = editorImage(*editor);

    EditorAppearance appearance = editor->appearance();
    appearance.focusMode.enabled = true;
    editor->setAppearance(appearance);
    CHECK(editor->isFocusModeEnabled());
    CHECK(differingPixels(plain, editorImage(*editor), first) > 0);
}

// =============================================================================
// The text frame border
// =============================================================================

namespace {

/// Columns of an image row where the frame is drawn: the pixels it turned red
std::vector<int> frameColumns(const QImage& plain, const QImage& framed, int row) {
    std::vector<int> columns;
    for (int x = 0; x < framed.width(); ++x) {
        const QRgb pixel = framed.pixel(x, row);
        if (pixel != plain.pixel(x, row) && qRed(pixel) > qGreen(pixel) + 60 &&
            qRed(pixel) > qBlue(pixel) + 60) {
            columns.push_back(x);
        }
    }
    return columns;
}

/// Rows of an image where the frame draws a line at least @p length pixels long
int frameEdges(const QImage& plain, const QImage& framed, int length) {
    int edges = 0;
    for (int y = 0; y < framed.height(); ++y) {
        if (static_cast<int>(frameColumns(plain, framed, y).size()) >= length) {
            ++edges;
        }
    }
    return edges;
}

/// The appearance with a red frame around the text area
EditorAppearance withRedFrame(EditorAppearance appearance) {
    appearance.textFrameBorder.show = true;
    appearance.textFrameBorder.color = QColor(255, 0, 0);
    appearance.textFrameBorder.width = 1;
    return appearance;
}

}  // anonymous namespace

TEST_CASE("Stage5 text frame: the appearance draws the frame of the text area in both views",
          "[editor][stage5][frame][render]") {
    // The settings reach the editor through its appearance (editor.textFrameBorder.*)
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(1000, 500));  // the whole page width in the view
    editor->fromKml(kmlOf({QStringLiteral("First paragraph"), QStringLiteral("Second paragraph")}));
    const EditorAppearance plainAppearance = editor->appearance();
    const double textWidth = editor->textDocument()->textWidth();

    for (ViewMode mode : {ViewMode::Continuous, ViewMode::Page}) {
        CAPTURE(static_cast<int>(mode));
        editor->setViewMode(mode);
        const QImage plain = editorImage(*editor);

        editor->setAppearance(withRedFrame(plainAppearance));
        const QImage framed = editorImage(*editor);
        // Far below the two paragraphs: a short chapter's frame is as tall as a page's text
        // area, as the paper around it
        const auto sides = frameColumns(plain, framed, framed.height() - 40);
        REQUIRE(sides.size() >= 2);
        CHECK(sides.size() <= 6);
        CHECK(sides.back() - sides.front() == Approx(textWidth).margin(3));
        // And its top edge, as wide as the text
        CHECK(frameEdges(plain, framed, static_cast<int>(textWidth / 2)) >= 1);

        editor->setAppearance(plainAppearance);
        CHECK(differingPixels(plain, editorImage(*editor), editor->rect()) == 0);
    }
}

TEST_CASE("Stage5 text frame: a long chapter's frame has its sides wherever it is scrolled",
          "[editor][stage5][frame][render]") {
    QStringList paragraphs;
    for (int i = 0; i < 120; ++i) {
        paragraphs << QStringLiteral("Paragraph %1, one short line").arg(i);
    }
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(1000, 500));
    editor->fromKml(kmlOf(paragraphs));
    const EditorAppearance plainAppearance = editor->appearance();
    editor->setScrollOffset(1.0e9);  // clamped to the end
    REQUIRE(editor->scrollOffset() > 2000.0);
    editor->setScrollOffset(editor->scrollOffset() / 2.0);
    const QImage plain = editorImage(*editor);

    editor->setAppearance(withRedFrame(plainAppearance));
    const QImage framed = editorImage(*editor);
    for (int row : {0, framed.height() / 2, framed.height() - 1}) {
        CAPTURE(row);
        CHECK(frameColumns(plain, framed, row).size() >= 2);
    }
    // Neither its top nor its bottom edge in the middle of the chapter
    CHECK(frameEdges(plain, framed, 20) == 0);
}

// =============================================================================
// The continuous views: one endless page
// =============================================================================

namespace {

/// Paragraphs long enough to wrap onto several lines of a page
QStringList longParagraphs(int count) {
    QStringList list;
    for (int i = 0; i < count; ++i) {
        list << QStringLiteral("Paragraph %1 is long enough to wrap onto several lines of the "
                               "page, so that its line breaks depend on the width of the text "
                               "and would show any difference between the views.").arg(i);
    }
    return list;
}

/// Where each line of the text starts: (paragraph, first character of the line)
std::vector<std::pair<int, int>> lineStarts(BookEditor& editor) {
    auto* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
    REQUIRE(layout != nullptr);
    layout->layoutPendingBlocks();
    std::vector<std::pair<int, int>> starts;
    for (QTextBlock block = editor.textDocument()->begin(); block.isValid(); block = block.next()) {
        const QTextLayout* lines = block.layout();
        for (int i = 0; i < lines->lineCount(); ++i) {
            starts.emplace_back(block.blockNumber(), lines->lineAt(i).textStart());
        }
    }
    return starts;
}

/// The paper on an image row: from a column on the paper out to the last paper pixels
std::pair<int, int> paperSpan(const QImage& image, int row, int column, QRgb paper) {
    int left = column;
    while (left > 0 && image.pixel(left - 1, row) == paper) {
        --left;
    }
    int right = column;
    while (right + 1 < image.width() && image.pixel(right + 1, row) == paper) {
        ++right;
    }
    return {left, right};
}

/// The paper's top edge in an image column: from a row on the paper up to the last paper
/// pixel
int paperTop(const QImage& image, int row, int column, QRgb paper) {
    while (row > 0 && image.pixel(column, row - 1) == paper) {
        --row;
    }
    return row;
}

QScrollBar* scrollBar(BookEditor& editor, Qt::Orientation orientation) {
    for (auto* bar : editor.findChildren<QScrollBar*>()) {
        if (bar->orientation() == orientation) {
            return bar;
        }
    }
    return nullptr;
}

}  // anonymous namespace

TEST_CASE("Stage5 continuous view: the lines break as on the pages, at every width and zoom",
          "[editor][stage5][continuous]") {
    auto editor = editorWith(kmlOf(longParagraphs(12)));
    const auto starts = lineStarts(*editor);
    REQUIRE(starts.size() > 24);

    // The page's width less its margins, whatever the window
    const EditorAppearance& appearance = editor->appearance();
    const double dpi = editor->screen() != nullptr ? editor->screen()->logicalDotsPerInch() : 96.0;
    const double textMm = appearance.pageLayout.pageSizeMm().width() -
                          appearance.pageMargins.left - appearance.pageMargins.right;
    CHECK(editor->textDocument()->textWidth() == Approx(textMm / 25.4 * dpi).margin(0.01));

    resizeWidget(*editor, QSize(1200, 400));
    CHECK(lineStarts(*editor) == starts);
    editor->setZoomFactor(1.7);
    CHECK(lineStarts(*editor) == starts);
    for (ViewMode mode : {ViewMode::Page, ViewMode::Continuous}) {
        editor->setViewMode(mode);
        CAPTURE(static_cast<int>(mode));
        CHECK(lineStarts(*editor) == starts);
    }
    editor->setDistractionFree(true);
    CHECK(lineStarts(*editor) == starts);
}

TEST_CASE("Stage5 continuous view: one endless page, as wide as the pages and in their place",
          "[editor][stage5][continuous][render]") {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(1000, 500));  // the whole page width in the view
    editor->fromKml(kmlOf(longParagraphs(30)));
    const EditorAppearance& appearance = editor->appearance();
    const QRgb paper = appearance.colors.background(appearance.colorMode).rgb();
    const QRectF first = caretAt(*editor, {0, 0});
    const int row = static_cast<int>(first.top()) - 10;  // in the page's top margin
    const int column = static_cast<int>(first.left());

    editor->setViewMode(ViewMode::Page);
    const QImage pages = editorImage(*editor);
    editor->setViewMode(ViewMode::Continuous);
    const QImage continuous = editorImage(*editor);

    // The paper where the first page has it, with the desk on both sides
    const auto strip = paperSpan(continuous, row, column, paper);
    CHECK(strip == paperSpan(pages, row, column, paper));
    CHECK(paperTop(continuous, row, column, paper) == paperTop(pages, row, column, paper));
    const int viewWidth = editor->width() - scrollBar(*editor, Qt::Vertical)->width();
    REQUIRE(strip.first > 10);
    REQUIRE(strip.second < viewWidth - 10);
    CHECK(continuous.pixel(strip.first - 10, row) != paper);
    CHECK(continuous.pixel(strip.second + 10, row) != paper);
    CHECK(paperTop(continuous, row, column, paper) > 0);

    // The text between the page's margins (equal by default); the first line is indented
    const QTextLine firstLine = editor->textDocument()->firstBlock().layout()->lineAt(0);
    const int textLeft = static_cast<int>(std::lround(first.left() - firstLine.cursorToX(0)));
    const int textRight = textLeft + static_cast<int>(editor->textDocument()->textWidth());
    CHECK(std::abs((textLeft - strip.first) - (strip.second - textRight)) <= 2);

    SECTION("no page breaks: the paper goes on between the lines of the whole text") {
        // Where the page view has the gap between the first two pages
        for (int y = row; y < editor->height(); ++y) {
            CHECK(continuous.pixel(strip.first + 2, y) == paper);
        }
    }

    SECTION("the page ends below the last line with the page's bottom margin") {
        editor->moveCursorToDocEnd();
        const QImage end = editorImage(*editor);
        const QRectF last = editor->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
        int bottom = static_cast<int>(last.bottom());
        while (bottom + 1 < end.height() && end.pixel(column, bottom + 1) == paper) {
            ++bottom;
        }
        const double dpi =
            editor->screen() != nullptr ? editor->screen()->logicalDotsPerInch() : 96.0;
        // From the caret's bottom: the line's spacing below the glyphs comes on top
        CHECK(bottom - last.bottom() ==
              Approx(appearance.pageMargins.bottom / 25.4 * dpi).margin(first.height()));
        CHECK(bottom + 1 < end.height());  // the desk below the page
    }
}

TEST_CASE("Stage5 continuous view: a chapter shorter than a page is on a whole page",
          "[editor][stage5][continuous][render]") {
    // No page break to leave out: both views show the same sheet, a page high
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(1000, 700));
    editor->fromKml(kmlOf(longParagraphs(2)));
    QScrollBar* vertical = scrollBar(*editor, Qt::Vertical);
    REQUIRE(vertical != nullptr);

    // The same scroll range: down to the page's bottom edge
    editor->setViewMode(ViewMode::Page);
    const int pagesMaximum = vertical->maximum();
    REQUIRE(pagesMaximum > 0);  // the page is higher than the view
    editor->setViewMode(ViewMode::Continuous);
    CHECK(vertical->maximum() == pagesMaximum);

    // The whole page in the view: the paper ends at the same row, a page below its top
    editor->setZoomFactor(0.5);
    const EditorAppearance& appearance = editor->appearance();
    const QRgb paper = appearance.colors.background(appearance.colorMode).rgb();
    const QRectF first = caretAt(*editor, {0, 0});
    const int row = static_cast<int>(first.top()) - 10;  // in the page's top margin
    const QImage continuous = editorImage(*editor);
    editor->setViewMode(ViewMode::Page);
    const QImage pages = editorImage(*editor);

    const int column = paperSpan(continuous, row, static_cast<int>(first.left()), paper).first + 4;
    const auto paperBottom = [&](const QImage& image) {
        int bottom = row;
        while (bottom + 1 < image.height() && image.pixel(column, bottom + 1) == paper) {
            ++bottom;
        }
        return bottom;
    };
    CHECK(paperBottom(continuous) == paperBottom(pages));
    const double dpi = editor->screen() != nullptr ? editor->screen()->logicalDotsPerInch() : 96.0;
    const double pageHeight = appearance.pageLayout.pageSizeMm().height() / 25.4 * dpi * 0.5;
    CHECK(paperBottom(continuous) - paperTop(continuous, row, column, paper) + 1 ==
          Approx(pageHeight).margin(3.0));
    CHECK(paperBottom(continuous) + 1 < continuous.height());  // the desk below the page
}

TEST_CASE("Stage5 continuous view: switching views keeps the cursor's place on the screen",
          "[editor][stage5][continuous]") {
    // A view taller than the text of a page: a page break lies between the top of the view
    // and the cursor, so keeping only the text at the top in place would move the cursor by
    // the margins and the gap around the break
    auto editor = editorWith(kmlOf(longParagraphs(60)));
    resizeWidget(*editor, QSize(600, 1600));
    editor->setCursorPosition({40, 10});                       // at the bottom of the view
    editor->setScrollOffset(editor->scrollOffset() + 400.0);  // 400 pixels above it
    const double top = editor->inputMethodQuery(Qt::ImCursorRectangle).toRectF().top();
    REQUIRE(top > 1000.0);
    REQUIRE(top < 1300.0);

    for (ViewMode mode : {ViewMode::Page, ViewMode::Continuous}) {
        editor->setViewMode(mode);
        CAPTURE(static_cast<int>(mode));
        CHECK(editor->inputMethodQuery(Qt::ImCursorRectangle).toRectF().top() ==
              Approx(top).margin(1.0));
    }
    editor->setDistractionFree(true);
    CHECK(editor->inputMethodQuery(Qt::ImCursorRectangle).toRectF().top() == Approx(top).margin(1.0));
}

TEST_CASE("Stage5 continuous view: switching views leaves a view scrolled away from the cursor",
          "[editor][stage5][continuous]") {
    // Regression: the view jumped back to the cursor. The text at the top stays instead,
    // on the second page here: the page view places it below the first page's break.
    auto editor = editorWith(kmlOf(longParagraphs(60)));
    auto* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout());
    REQUIRE(layout != nullptr);
    layout->layoutPendingBlocks();
    editor->setScrollOffset(1300.0);
    const int topBlock = layout->blockNumberAtY(editor->scrollOffset());

    editor->setViewMode(ViewMode::Page);
    CHECK(editor->scrollOffset() > 1400.0);
    CHECK(layout->blockNumberAtY(editor->scrollOffset()) == topBlock);
    editor->setViewMode(ViewMode::Continuous);
    CHECK(editor->scrollOffset() == Approx(1300.0).margin(1.0));
    CHECK(layout->blockNumberAtY(editor->scrollOffset()) == topBlock);
    CHECK(editor->cursorPosition() == CursorPosition{0, 0});
}

TEST_CASE("Stage5 continuous view: a page wider than the view scrolls sideways to the cursor",
          "[editor][stage5][continuous]") {
    // A window far narrower than the page at 100% (at 72 dpi too, as on macOS): the
    // horizontal scroll bar
    auto editor = editorWith(kmlOf(longParagraphs(1)));
    resizeWidget(*editor, QSize(300, 400));
    QScrollBar* sideways = scrollBar(*editor, Qt::Horizontal);
    REQUIRE(sideways != nullptr);
    REQUIRE_FALSE(sideways->isHidden());
    REQUIRE(sideways->value() == 0);
    const int viewWidth = editor->width() - scrollBar(*editor, Qt::Vertical)->width();

    // The end of the first line, right of the view
    const QTextLine line = editor->textDocument()->firstBlock().layout()->lineAt(0);
    const QRectF end = caretAt(*editor, {0, line.textStart() + line.textLength() - 1});
    CHECK(sideways->value() > 0);
    CHECK(end.left() >= 0.0);
    CHECK(end.right() <= viewWidth);

    // And back to the start of the line
    const QRectF start = caretAt(*editor, {0, 0});
    CHECK(start.left() >= 0.0);
    CHECK(start.right() <= viewWidth);
}

// =============================================================================
// Distraction-Free
// =============================================================================

TEST_CASE("Stage5 distraction-free: a toggle that keeps the pages and darkens the sides",
          "[editor][stage5][distraction-free]") {
    // The whole page in the middle of the view, with the desk on both sides; the texts at
    // the edges fade out quickly
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(1400, 500));
    EditorAppearance appearance = editor->appearance();
    appearance.distractionFree.uiFadeTimeout = 50;
    editor->setAppearance(appearance);
    editor->fromKml(kmlOf({QStringLiteral("First paragraph"), QStringLiteral("Second paragraph")}));
    editor->setViewMode(ViewMode::Page);
    const int pages = editor->totalPages();
    const QRect first = rangeArea(*editor, 0, 0, 5);
    const QImage page = editorImage(*editor);

    int changes = 0;
    QObject::connect(editor.get(), &BookEditor::distractionFreeModeChanged,
                     [&changes](bool) { ++changes; });
    editor->setDistractionFree(true);
    CHECK(changes == 1);
    CHECK(editor->viewMode() == ViewMode::Page);
    CHECK(editor->totalPages() == pages);
    CHECK(rangeArea(*editor, 0, 0, 5) == first);
    CHECK(scrollBar(*editor, Qt::Vertical)->isHidden());

    // The sides darken toward the edges of the view; the middle stays as it was
    const QImage dark = editorImage(*editor);
    const int viewWidth = editor->width() - scrollBar(*editor, Qt::Vertical)->width();
    const int row = editor->height() / 2;
    CHECK(qGray(dark.pixel(0, row)) < qGray(page.pixel(0, row)));
    CHECK(qGray(dark.pixel(viewWidth - 1, row)) < qGray(page.pixel(viewWidth - 1, row)));
    const QRect middle(viewWidth / 4, 60, viewWidth / 2, editor->height() - 120);
    REQUIRE(middle.contains(first));
    CHECK(differingPixels(page, dark, middle) == 0);

    // The word count at the bottom, at first; the middle of the top edge stays free for
    // the window's menus
    const QRect top(viewWidth / 4, 0, viewWidth / 2, 60);
    const QRect bottom(viewWidth / 4, editor->height() - 60, viewWidth / 2, 60);
    CHECK(differingPixels(page, dark, top) == 0);
    CHECK(differingPixels(page, dark, bottom) > 0);

    SECTION("the texts at the edges fade out and come back when the mouse nears an edge") {
        runEventLoop(150);
        const QImage faded = editorImage(*editor);
        CHECK(differingPixels(page, faded, top) == 0);
        CHECK(differingPixels(page, faded, bottom) == 0);
        CHECK(qGray(faded.pixel(0, row)) < qGray(page.pixel(0, row)));

        const QPointF nearTop(viewWidth / 2.0, 10.0);
        QMouseEvent move(QEvent::MouseMove, nearTop, editor->mapToGlobal(nearTop), Qt::NoButton,
                         Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(editor.get(), &move);
        CHECK(differingPixels(page, editorImage(*editor), bottom) > 0);
    }

    SECTION("turning it off shows the view as before") {
        editor->setDistractionFree(false);
        CHECK(changes == 2);
        CHECK(editor->viewMode() == ViewMode::Page);
        CHECK_FALSE(scrollBar(*editor, Qt::Vertical)->isHidden());
        CHECK(differingPixels(page, editorImage(*editor), editor->rect()) == 0);
    }
}

// =============================================================================
// Zoom of the first show
// =============================================================================

namespace {

/// The zoom at which the page with the gap on both sides fills the view left of the
/// vertical scroll bar (View > Zoom > Page Width)
double pageWidthZoomOf(BookEditor& editor) {
    const EditorAppearance& appearance = editor.appearance();
    const double dpi = editor.screen() != nullptr ? editor.screen()->logicalDotsPerInch() : 96.0;
    const double pageWidth = appearance.pageLayout.pageSizeMm().width() / 25.4 * dpi;
    const double viewWidth = editor.width() - scrollBar(editor, Qt::Vertical)->width();
    return viewWidth / ((pageWidth + 2.0 * appearance.pageLayout.pageGap) * editor.paperScale());
}

void sendCtrlWheel(BookEditor& editor) {
    QWheelEvent wheel(QPointF(100, 100), QPointF(100, 100), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&editor, &wheel);
}

}  // anonymous namespace

TEST_CASE("Stage5 first show: a page wider than the view shrinks to its width",
          "[editor][stage5][zoom]") {
    auto editor = editorWith(kmlOf(longParagraphs(30)));
    REQUIRE(pageWidthZoomOf(*editor) < 1.0);
    CHECK(editor->zoomFactor() == 1.0);  // an editor not asked to: the zoom of the settings

    editor->shrinkToPageWidthOnFirstShow();
    CHECK(editor->zoomFactor() == Approx(pageWidthZoomOf(*editor)));
    CHECK(scrollBar(*editor, Qt::Horizontal)->isHidden());
    CHECK(editor->scrollOffset() == Approx(0.0).margin(0.5));  // the start of the text

    SECTION("a page that fits the view keeps 100%") {
        resizeWidget(*editor, QSize(1200, 500));
        REQUIRE(pageWidthZoomOf(*editor) > 1.0);
        CHECK(editor->zoomFactor() == 1.0);
    }
}

TEST_CASE("Stage5 first show: the view's size counts until the first paint",
          "[editor][stage5][zoom]") {
    // The window and the panels take their size before the editor shows
    auto editor = editorWith(kmlOf(longParagraphs(30)));
    editor->shrinkToPageWidthOnFirstShow();
    const double narrow = editor->zoomFactor();
    REQUIRE(narrow < 1.0);
    resizeWidget(*editor, QSize(1200, 500));
    CHECK(editor->zoomFactor() == 1.0);
    resizeWidget(*editor, QSize(600, 400));
    CHECK(editor->zoomFactor() == Approx(narrow));

    // Shown, the zoom no longer follows the size
    editorImage(*editor);  // the first paint
    for (const QSize& size : {QSize(1200, 500), QSize(450, 300)}) {
        CAPTURE(size.width());
        resizeWidget(*editor, size);
        CHECK(editor->zoomFactor() == Approx(narrow));
    }
}

TEST_CASE("Stage5 first show: a new text, paper scale or page size before it keeps the shrink",
          "[editor][stage5][zoom]") {
    auto editor = editorWith(kmlOf(longParagraphs(5)));
    editor->shrinkToPageWidthOnFirstShow();

    // The chapter's text comes after: it does not bring back 100%
    editor->fromKml(kmlOf(longParagraphs(30)));
    CHECK(editor->zoomFactor() == Approx(pageWidthZoomOf(*editor)));

    // Pages at their size on paper: a wider page, a smaller zoom
    const double before = editor->zoomFactor();
    editor->setPaperScale(1.5);
    CHECK(editor->zoomFactor() == Approx(pageWidthZoomOf(*editor)));
    CHECK(editor->zoomFactor() < before);

    EditorAppearance appearance = editor->appearance();
    appearance.pageLayout.pageSize = PageLayout::PageSize::A5;
    editor->setAppearance(appearance);
    CHECK(editor->zoomFactor() == Approx(pageWidthZoomOf(*editor)));
}

TEST_CASE("Stage5 first show: a zoom asked for stays", "[editor][stage5][zoom]") {
    const std::vector<std::pair<const char*, std::function<void(BookEditor&)>>> zooms = {
        {"Zoom In", [](BookEditor& editor) { editor.zoomIn(); }},
        {"Zoom 100%", [](BookEditor& editor) { editor.zoomReset(); }},
        {"Page Width", [](BookEditor& editor) { editor.zoomToPageWidth(); }},
        {"Whole Page", [](BookEditor& editor) { editor.zoomToWholePage(); }},
        {"Ctrl+wheel", [](BookEditor& editor) { sendCtrlWheel(editor); }},
    };
    for (const auto& zoom : zooms) {
        CAPTURE(zoom.first);
        auto editor = editorWith(kmlOf(longParagraphs(10)));
        editor->shrinkToPageWidthOnFirstShow();
        zoom.second(*editor);
        const double asked = editor->zoomFactor();

        // Before the first paint: a smaller view would shrink the page again
        resizeWidget(*editor, QSize(450, 300));
        CHECK(editor->zoomFactor() == asked);
    }
}
