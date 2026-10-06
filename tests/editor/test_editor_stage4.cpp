/// @file test_editor_stage4.cpp
/// @brief Editor stage 4: page mode (page flow in the layout, pages in the view), zoom
///        and typewriter scrolling

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_render_pipeline.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/render_context.h>
#include <kalahari/editor/view_modes.h>
#include "editor_test_utils.h"

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

using namespace kalahari::editor;
using namespace kalahari::test;
using Catch::Approx;

namespace {

constexpr qreal kWidth = 300.0;
constexpr qreal kTextHeight = 200.0;  ///< Text area of a page
constexpr qreal kPitch = 260.0;       ///< Text area plus margins and gap

QFont testFont() {
    QFont font(QStringLiteral("Arial"));
    font.setPointSizeF(12.0);
    return font;
}

/// Paragraphs of different lengths and letter widths
QStringList mixedParagraphs(int count) {
    QStringList list;
    for (int i = 0; i < count; ++i) {
        QString text;
        switch (i % 4) {
            case 0: text = QStringLiteral("WWW MMM WWWW MMMM ").repeated(4 + i % 5); break;
            case 1: text = QStringLiteral("iii lll iiii llll ").repeated(6 + i % 5); break;
            case 2:
                text = QStringLiteral("Pneumonoultramicroscopicsilicovolcanoconiosis is long. ")
                           .repeated(2 + i % 3);
                break;
            default:
                text = QStringLiteral("Paragraph %1 has enough words to wrap onto several "
                                      "lines when the layout is narrow. ")
                           .arg(i)
                           .repeated(1 + i % 3);
                break;
        }
        list << text.trimmed();
    }
    return list;
}

/// A document wired like BookEditor::ensureEditMode() wires it, with page flow
struct PagedDocument {
    std::unique_ptr<QTextDocument> doc = std::make_unique<QTextDocument>();
    KalahariTextDocumentLayout* layout = nullptr;

    PagedDocument() {
        layout = new KalahariTextDocumentLayout(doc.get());
        doc->setDocumentLayout(layout);
        doc->setDefaultFont(testFont());
        doc->setDocumentMargin(0);
        doc->setTextWidth(kWidth);
        LayoutTypography typography;
        typography.lineSpacing = 1.5;
        typography.paragraphSpacing = 9.0;
        layout->setTypography(typography);
        layout->setPageFlow(PageFlow{true, kPitch, kTextHeight});
    }

    void load(const QStringList& paragraphs) {
        QTextCursor cursor(doc.get());
        cursor.beginEditBlock();
        for (int i = 0; i < paragraphs.size(); ++i) {
            if (i > 0) cursor.insertBlock();
            cursor.insertText(paragraphs[i]);
        }
        cursor.endEditBlock();
    }

    QTextBlock block(int number) const { return doc->findBlockByNumber(number); }
};

/// A line box in document coordinates
struct LineBox {
    int block = 0;
    int line = 0;
    int textStart = 0;
    qreal x = 0;
    qreal top = 0;
    qreal bottom = 0;

    bool operator==(const LineBox&) const = default;
};

/// Every line box of the document, the whole document laid out first
std::vector<LineBox> lineBoxes(const PagedDocument& d) {
    d.layout->layoutPendingBlocks();
    std::vector<LineBox> boxes;
    const qreal lineSpacing = d.layout->typography().lineSpacing;
    for (QTextBlock block = d.doc->begin(); block.isValid(); block = block.next()) {
        const qreal top = d.layout->blockY(block.blockNumber());
        const QTextLayout* lines = block.layout();
        for (int i = 0; i < lines->lineCount(); ++i) {
            const QTextLine line = lines->lineAt(i);
            const QRectF box = KalahariTextDocumentLayout::lineBox(line, lineSpacing);
            boxes.push_back({block.blockNumber(), i, line.textStart(), line.x(),
                             top + box.top(), top + box.bottom()});
        }
    }
    return boxes;
}

qreal pageOf(qreal y) {
    return std::floor((y + 0.01) / kPitch);
}

}  // anonymous namespace

// =============================================================================
// Page flow in the layout
// =============================================================================

TEST_CASE("Stage4 page flow: lines stay within the text areas of the pages",
          "[editor][stage4][pageflow]") {
    PagedDocument d;
    d.load(mixedParagraphs(40));
    const auto boxes = lineBoxes(d);
    REQUIRE(boxes.size() > 40);
    const qreal spacing = d.layout->paragraphSpacing();

    int pageBreaks = 0;
    int breaksWithinParagraph = 0;
    for (size_t i = 0; i < boxes.size(); ++i) {
        const LineBox& box = boxes[i];
        const qreal page = pageOf(box.top);
        INFO("block " << box.block << " line " << box.line << " top " << box.top);
        CHECK(box.top - page * kPitch >= -0.01);
        CHECK(box.bottom - page * kPitch <= kTextHeight + 0.01);
        if (i == 0) {
            CHECK(box.top == 0.0);
            continue;
        }
        const LineBox& previous = boxes[i - 1];
        CHECK(box.top >= previous.bottom - 0.01);  // in order, never overlapping
        if (pageOf(previous.top) != page) {
            // A line starting a page starts at the top of its text area, and only when
            // it would not have fitted below the previous one
            ++pageBreaks;
            breaksWithinParagraph += box.block == previous.block ? 1 : 0;
            CHECK(box.top == Approx(page * kPitch));
            const qreal roomLeft = kTextHeight - (previous.bottom - pageOf(previous.top) * kPitch);
            const qreal gapBefore = box.block == previous.block ? 0.0 : spacing;
            CHECK(roomLeft < gapBefore + (box.bottom - box.top));
        } else {
            CHECK(box.top - previous.bottom == Approx(box.block == previous.block ? 0.0 : spacing));
        }
    }
    CHECK(pageBreaks >= 3);
    CHECK(breaksWithinParagraph >= 1);  // a paragraph split between two pages
    CHECK(d.layout->pageCount() == static_cast<int>(pageOf(boxes.back().top)) + 1);
}

TEST_CASE("Stage4 page flow: after edits the pages are those of a layout from scratch",
          "[editor][stage4][pageflow]") {
    PagedDocument d;
    d.load(mixedParagraphs(60));
    d.layout->layoutPendingBlocks();

    const auto checkAgainstScratch = [&d] {
        const auto edited = lineBoxes(d);
        const QSizeF editedSize = d.layout->documentSize();
        const int editedPages = d.layout->pageCount();
        d.layout->layoutAllBlocks();
        CHECK(lineBoxes(d) == edited);
        CHECK(d.layout->documentSize() == editedSize);
        CHECK(d.layout->pageCount() == editedPages);
    };

    QTextCursor cursor(d.doc.get());
    SECTION("a paragraph grows by several lines") {
        cursor.setPosition(d.block(5).position() + 3);
        cursor.insertText(QStringLiteral("more words that wrap onto new lines ").repeated(6));
        checkAgainstScratch();
    }
    SECTION("a paragraph is removed") {
        cursor.setPosition(d.block(10).position());
        cursor.setPosition(d.block(11).position(), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        checkAgainstScratch();
    }
    SECTION("a paragraph is split and characters are typed") {
        cursor.setPosition(d.block(20).position() + 10);
        cursor.insertBlock();
        for (int i = 0; i < 20; ++i) {
            cursor.insertText(QStringLiteral("x"));
        }
        checkAgainstScratch();
    }
    SECTION("a long paste waits for layout and is paged when laid out") {
        cursor.setPosition(d.block(2).position());
        cursor.insertText(mixedParagraphs(50).join(QChar::ParagraphSeparator));
        CHECK(d.layout->pendingBlockCount() > 0);
        checkAgainstScratch();
    }
}

TEST_CASE("Stage4 page flow: the document ends with the text area of its last page",
          "[editor][stage4][pageflow]") {
    PagedDocument d;
    d.load({QStringLiteral("One short paragraph.")});
    d.layout->layoutPendingBlocks();
    CHECK(d.layout->pageCount() == 1);
    CHECK(d.layout->documentSize().height() == kTextHeight);

    d.load(mixedParagraphs(30));
    const auto boxes = lineBoxes(d);
    const int pages = static_cast<int>(pageOf(boxes.back().top)) + 1;
    REQUIRE(pages >= 3);
    CHECK(d.layout->pageCount() == pages);
    CHECK(d.layout->documentSize().height() == Approx((pages - 1) * kPitch + kTextHeight));

    SECTION("while blocks wait for layout, the estimated pages end the document too") {
        d.doc->setTextWidth(kWidth + 1);  // every block waits again
        REQUIRE(d.layout->pendingBlockCount() > 0);
        const int estimated = d.layout->pageCount();
        CHECK(estimated >= 2);
        CHECK(d.layout->documentSize().height() ==
              Approx((estimated - 1) * kPitch + kTextHeight));
    }
}

TEST_CASE("Stage4 page flow: hit test on the lines of a later page",
          "[editor][stage4][pageflow]") {
    PagedDocument d;
    d.load(mixedParagraphs(40));
    const auto boxes = lineBoxes(d);

    // The first line of the second page
    const auto it = std::find_if(boxes.begin(), boxes.end(),
                                 [](const LineBox& box) { return pageOf(box.top) == 1.0; });
    REQUIRE(it != boxes.end());
    const QTextBlock block = d.block(it->block);
    const QTextLine line = block.layout()->lineAt(it->line);
    const int lineStart = block.position() + line.textStart();
    const int lineEnd = lineStart + line.textLength();

    const qreal middle = (it->top + it->bottom) / 2.0;
    const int fuzzy = d.layout->hitTest(QPointF(20.0, middle), Qt::FuzzyHit);
    CHECK(fuzzy >= lineStart);
    CHECK(fuzzy <= lineEnd);
    const int exact = d.layout->hitTest(QPointF(20.0, middle), Qt::ExactHit);
    CHECK(exact >= lineStart);
    CHECK(exact < lineEnd);

    // Between the text areas there is no text
    CHECK(d.layout->hitTest(QPointF(20.0, kTextHeight + 10.0), Qt::ExactHit) == -1);
}

TEST_CASE("Stage4 page flow: turning it off gives the continuous layout back",
          "[editor][stage4][pageflow]") {
    const QStringList paragraphs = mixedParagraphs(30);
    PagedDocument d;
    d.load(paragraphs);
    d.layout->layoutPendingBlocks();
    REQUIRE(d.layout->pageCount() > 1);

    d.layout->setPageFlow(PageFlow{});
    CHECK(d.layout->pendingBlockCount() == d.doc->blockCount());  // laid out again
    CHECK(d.layout->pageCount() == 1);

    PagedDocument reference;
    reference.layout->setPageFlow(PageFlow{});
    reference.load(paragraphs);
    CHECK(lineBoxes(d) == lineBoxes(reference));
    CHECK(d.layout->documentSize() == reference.layout->documentSize());
}

// =============================================================================
// The editor: pages in the view, zoom, typewriter scrolling
// =============================================================================

namespace {

/// Paragraphs that fill several pages
QStringList chapter(int count) {
    QStringList list;
    for (int i = 0; i < count; ++i) {
        list << QStringLiteral("Paragraph %1 tells a story long enough to wrap over a few lines "
                               "of the page, so that pages break inside paragraphs too.")
                    .arg(i);
    }
    return list;
}

std::unique_ptr<BookEditor> editorIn(ViewMode mode, int paragraphs = 60) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(700, 500));
    editor->fromKml(kmlOf(chapter(paragraphs)));
    editor->setViewMode(mode);
    return editor;
}

void paint(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    editor.render(&image);
}

QRectF caret(BookEditor& editor) {
    return editor.inputMethodQuery(Qt::ImCursorRectangle).toRectF();
}

void pressAndRelease(BookEditor& editor, const QPointF& pos) {
    QMouseEvent press(QEvent::MouseButtonPress, pos, editor.mapToGlobal(pos), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, editor.mapToGlobal(pos), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &release);
}

/// A single click. The editor takes a click near the previous one for a double click until
/// its timer runs out, which needs an event loop the tests do not run: a click aside on the
/// same line comes first.
void click(BookEditor& editor, const QPointF& pos) {
    pressAndRelease(editor, pos + QPointF(40.0, 0.0));
    pressAndRelease(editor, pos);
}

void pressKey(BookEditor& editor, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(&editor, &event);
}

/// Typewriter scrolling on, without the glide (the tests run no event loop)
void typewriterOn(BookEditor& editor) {
    EditorAppearance appearance = editor.appearance();
    appearance.typewriter.enabled = true;
    appearance.typewriter.smoothScroll = false;
    editor.setAppearance(appearance);
}

/// First character of every line of the first paragraphs
std::vector<int> lineStarts(BookEditor& editor, int blocks) {
    std::vector<int> starts;
    QTextBlock block = editor.textDocument()->begin();
    for (int i = 0; i < blocks && block.isValid(); ++i, block = block.next()) {
        const QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);
        for (int line = 0; layout && line < layout->lineCount(); ++line) {
            starts.push_back(block.position() + layout->lineAt(line).textStart());
        }
    }
    return starts;
}

struct LineSpan {
    double top;
    double bottom;
};

/// The text of every line, in document units
std::vector<LineSpan> lineSpans(BookEditor& editor) {
    QAbstractTextDocumentLayout* layout = editor.textDocument()->documentLayout();
    std::vector<LineSpan> lines;
    for (QTextBlock block = editor.textDocument()->begin(); block.isValid();
         block = block.next()) {
        const QTextLayout* blockLayout = KalahariTextDocumentLayout::blockLayout(block);
        const double blockTop = layout->blockBoundingRect(block).top();
        for (int i = 0; blockLayout && i < blockLayout->lineCount(); ++i) {
            const QTextLine line = blockLayout->lineAt(i);
            lines.push_back({blockTop + line.y(), blockTop + line.y() + line.height()});
        }
    }
    return lines;
}

/// Widget pixels per document unit: the zoom, and the paper scale in the Page Layout view
double viewScale(BookEditor& editor) {
    return editor.zoomFactor() * (editor.viewMode() == ViewMode::Page ? editor.paperScale() : 1.0);
}

/// The top of the cursor's line in document units
double cursorLineTop(BookEditor& editor) {
    const CursorPosition position = editor.cursorPosition();
    const QTextBlock block = editor.textDocument()->findBlockByNumber(position.paragraph);
    return editor.textDocument()->documentLayout()->blockBoundingRect(block).top() +
           block.layout()->lineForTextPosition(position.offset).y();
}

/// The top of the view in document units, from the cursor's line and its row in the view
double viewTop(BookEditor& editor) {
    return cursorLineTop(editor) - caret(editor).top() / viewScale(editor);
}

}  // namespace

TEST_CASE("Stage4 page mode: a click lands on the character under it, on every page and zoom",
          "[editor][stage4][pagemode]") {
    auto editor = editorIn(ViewMode::Page);
    paint(*editor);
    REQUIRE(editor->totalPages() >= 3);

    // At the paper scale of a laptop screen at 125% display scaling too
    for (const double paper : {1.0, 1.483}) {
        editor->setPaperScale(paper);
        for (const double zoom : {0.75, 1.0, 1.5}) {
            editor->setZoomFactor(zoom);
            for (int page = 1; page <= 3; ++page) {
                editor->goToPage(page);
                paint(*editor);
                REQUIRE(editor->currentPage() == page);

                // A few characters into the first line of the page
                const CursorPosition first = editor->cursorPosition();
                const CursorPosition target{first.paragraph, first.offset + 3};
                editor->setCursorPosition(target);
                const QRectF at = caret(*editor);
                CAPTURE(paper, zoom, page);
                click(*editor, QPointF(at.left() + 1.0, at.center().y()));
                CHECK(editor->cursorPosition() == target);
            }
        }
    }
}

TEST_CASE("Stage4 page mode: the zoom keeps the line breaks", "[editor][stage4][pagemode]") {
    auto editor = editorIn(ViewMode::Page);
    paint(*editor);
    const std::vector<int> atHundred = lineStarts(*editor, 10);
    REQUIRE(atHundred.size() >= 20);

    for (double zoom : {0.5, 1.25, 2.0}) {
        editor->setZoomFactor(zoom);
        paint(*editor);
        CAPTURE(zoom);
        CHECK(lineStarts(*editor, 10) == atHundred);
    }
}

TEST_CASE("Stage4 page mode: the first line of a page is shown on that page",
          "[editor][stage4][pagemode]") {
    auto editor = editorIn(ViewMode::Page);
    paint(*editor);
    editor->goToPage(2);
    paint(*editor);

    // The cursor at the start of page 2: below the gap and the top margin of its sheet,
    // which goToPage() puts at the top of the view
    const QRectF at = caret(*editor);
    CHECK(at.top() > 20.0 + 50.0);
    CHECK(at.top() < editor->height() / 2.0);
    CHECK(editor->currentPage() == 2);
}

TEST_CASE("Stage4 page mode: the lines stay on their sheets on far pages",
          "[editor][stage4][pagemode]") {
    // A4 at 96 dpi is 1122.67 px high: the sheets and the text areas of the layout must
    // follow one page pitch, or the text drifts off the sheets page by page. Long
    // paragraphs give a few hundred pages in a few hundred blocks; a view taller than a
    // page shows a whole sheet.
    QStringList paragraphs;
    for (int i = 0; i < 500; ++i) {
        paragraphs << QStringLiteral("Paragraph %1 goes on over many lines of the page. ")
                          .arg(i)
                          .repeated(20)
                          .trimmed();
    }
    BookEditor editor;
    resizeWidget(editor, QSize(900, 1400));
    editor.fromKml(kmlOf(paragraphs));
    editor.setViewMode(ViewMode::Page);
    auto* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
    layout->layoutPendingBlocks();
    paint(editor);
    const int pages = editor.totalPages();
    REQUIRE(pages >= 150);
    const double textHeight = layout->pageFlow().textHeight * editor.zoomFactor();

    // goToPage() puts the sheet's top at the top of the view and the cursor on the page's
    // first line: on page 2 that line is where the first line of every page belongs
    editor.goToPage(2);
    paint(editor);
    const double firstLineTop = caret(editor).top();

    for (int page : {pages / 2, pages - 2}) {
        CAPTURE(page);
        // The last character of the page: just before the first one of the next page
        editor.goToPage(page + 1);
        CursorPosition last = editor.cursorPosition();
        if (last.offset > 0) {
            --last.offset;
        } else {
            --last.paragraph;
            last.offset = static_cast<int>(paragraphs[last.paragraph].size());
        }

        editor.goToPage(page);
        paint(editor);
        CHECK(caret(editor).top() == Approx(firstLineTop).margin(1.0));
        editor.setCursorPosition(last);
        paint(editor);
        CHECK(caret(editor).bottom() <= firstLineTop + textHeight + 1.0);
    }
}

TEST_CASE("Stage4 page mode: the page format and the gap come from the appearance",
          "[editor][stage4][pagemode]") {
    auto editor = editorIn(ViewMode::Page);
    paint(*editor);
    const int a4Pages = editor->totalPages();

    // A smaller page holds less text
    EditorAppearance appearance = editor->appearance();
    appearance.pageLayout.pageSize = PageLayout::PageSize::A5;
    editor->setAppearance(appearance);
    paint(*editor);
    CHECK(editor->totalPages() > a4Pages);

    // A wider gap moves the sheet of a page, put at the top of the view, down by as much
    const auto pageTwoTop = [&](double gap) {
        appearance.pageLayout.pageGap = gap;
        editor->setAppearance(appearance);
        editor->goToPage(2);
        paint(*editor);
        return caret(*editor).top();
    };
    const double narrow = pageTwoTop(20.0);
    const double wide = pageTwoTop(60.0);
    CHECK(wide - narrow == Catch::Approx(40.0).margin(1.0));
}

TEST_CASE("Stage4 page mode: text laid out above the view in the background repaints the view",
          "[editor][stage4][pagemode]") {
    // Regression: after a page format change, the text above the view laid out in the
    // background moved the page breaks around the text kept at the top of the view, and the
    // view was not painted again: the cursor blink painted its box from the new layout into
    // the old picture.
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        SKIP("reads the painted window back: run with QT_QPA_PLATFORM=offscreen");
    }
    BookEditor editor;
    resizeWidget(editor, QSize(700, 500));
    editor.fromKml(kmlOf(mixedParagraphs(600)));
    editor.setViewMode(ViewMode::Page);
    auto* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
    layout->layoutPendingBlocks();
    editor.goToPage(editor.totalPages() / 2);
    editor.show();
    runEventLoop(30);

    int scrollChanges = 0;
    QObject::connect(&editor, &BookEditor::scrollOffsetChanged,
                     [&scrollChanges] { ++scrollChanges; });
    EditorAppearance appearance = editor.appearance();
    appearance.pageLayout.pageSize = PageLayout::PageSize::A5;
    editor.setAppearance(appearance);
    REQUIRE(waitUntil([layout] { return layout->pendingBlockCount() == 0; }, 10000));
    runEventLoop(30);
    REQUIRE(scrollChanges > 0);  // the background pass moved the text above the view

    // What the window shows is what a paint of the view gives now
    const QImage shown = editor.screen()
                             ->grabWindow(editor.winId(), 0, 0, editor.width(), editor.height())
                             .toImage()
                             .convertToFormat(QImage::Format_RGB32);
    const QImage painted = editor.grab().toImage().convertToFormat(QImage::Format_RGB32);
    CHECK(shown == painted);
}

TEST_CASE("Stage4 zoom: 100% shows the pages at their size on paper",
          "[editor][stage4][pagemode][dpi]") {
    // Regression: zoom 100% gave the size of the system's display scaling, which on a laptop
    // screen (2560 x 1600 pixels, 37 cm wide, 125% scaling) is two thirds of the paper
    const double physicalDpi = 2560.0 / (370.0 / 25.4) / 1.25;  // Device-independent
    const double scale = BookEditor::paperScaleFor(physicalDpi, 96.0);
    REQUIRE(scale == Approx(1.464).margin(0.001));

    EditorRenderPipeline pipeline;
    RenderContext context;
    context.viewMode = ViewMode::Page;
    context.zoomMode = ZoomMode::PageScaling;
    context.pageMode.pageSize = QSizeF(595.28, 841.89);  // A4 in points
    context.paperScale = scale;
    pipeline.configure(context);
    const auto& computed = pipeline.context().computed;
    // 210 mm on the screen
    CHECK(computed.pageWidthPixels * computed.viewScale / physicalDpi * 25.4 ==
          Approx(210.0).margin(0.1));
    context.zoomFactor = 2.0;
    pipeline.configure(context);
    CHECK(pipeline.context().computed.viewScale == Approx(2.0 * scale));

    // The scroll modes lay the text out at the zoomed font size, as before
    context.viewMode = ViewMode::Continuous;
    context.zoomMode = ZoomMode::FontScaling;
    context.font = QFont(QStringLiteral("Arial"), 12);
    pipeline.configure(context);
    CHECK(pipeline.context().computed.viewScale == Approx(1.0));
    CHECK(pipeline.context().computed.effectiveFont.pointSizeF() == Approx(24.0));

    // A screen that reports no size or a made-up one keeps the size of the display scaling
    CHECK(BookEditor::paperScaleFor(0.0, 96.0) == 1.0);
    CHECK(BookEditor::paperScaleFor(96.0, 0.0) == 1.0);
    CHECK(BookEditor::paperScaleFor(30.0, 96.0) == 1.0);
    CHECK(BookEditor::paperScaleFor(1000.0, 96.0) == 1.0);

    SECTION("the editor's pages and its zoom to fit") {
        auto editor = editorIn(ViewMode::Page);
        paint(*editor);
        const double caretHeight = caret(*editor).height();
        editor->zoomToPageWidth();
        const double pageWidthZoom = editor->zoomFactor();

        editor->setZoomFactor(1.0);
        editor->setPaperScale(scale);
        paint(*editor);
        CHECK(editor->paperScale() == Approx(scale));
        CHECK(caret(*editor).height() == Approx(caretHeight * scale).margin(0.5));
        // Page Width fills the view as before: the zoom it needs is smaller by the scale
        editor->zoomToPageWidth();
        CHECK(editor->zoomFactor() == Approx(pageWidthZoom / scale).margin(0.001));
    }
}

TEST_CASE("Stage4 typewriter: typing keeps the cursor line at the focus height",
          "[editor][stage4][typewriter]") {
    for (ViewMode mode : {ViewMode::Continuous, ViewMode::Page}) {
        CAPTURE(static_cast<int>(mode));
        auto editor = editorIn(mode);
        typewriterOn(*editor);
        editor->setCursorPosition({20, 0});
        paint(*editor);
        const double focusY = editor->height() * 0.5;
        CHECK(caret(*editor).center().y() == Approx(focusY).margin(1.0));

        // Typing wraps onto new lines and new paragraphs; the line stays where it is
        for (int i = 0; i < 40; ++i) {
            editor->insertText(QStringLiteral("more words "));
        }
        pressKey(*editor, Qt::Key_Return);
        editor->insertText(QStringLiteral("A new paragraph."));
        paint(*editor);
        CHECK(caret(*editor).center().y() == Approx(focusY).margin(1.0));

        // The keyboard too
        pressKey(*editor, Qt::Key_Up);
        pressKey(*editor, Qt::Key_Up);
        paint(*editor);
        CHECK(caret(*editor).center().y() == Approx(focusY).margin(1.0));
    }
}

TEST_CASE("Stage4 typewriter: the text starts at the top, the line holds at the end",
          "[editor][stage4][typewriter]") {
    // Regression: the room above the first line put the start of a chapter in the middle
    // of the view, a large blank above it
    for (ViewMode mode : {ViewMode::Continuous, ViewMode::Page}) {
        CAPTURE(static_cast<int>(mode));
        auto editor = editorIn(mode, 200);
        paint(*editor);
        const double textTop = caret(*editor).top();  // the first line, typewriter off
        const double focusY = editor->height() * 0.5;

        // A chapter opened with typewriter scrolling on starts at the top of the view
        BookEditor opened;
        resizeWidget(opened, editor->size());
        typewriterOn(opened);
        opened.setViewMode(mode);
        opened.fromKml(kmlOf(chapter(200)));
        paint(opened);
        CHECK(opened.scrollOffset() == Approx(0.0).margin(0.5));
        CHECK(caret(opened).top() == Approx(textTop).margin(0.5));

        // The cursor line comes down to the focus height and stays there to the end
        typewriterOn(*editor);
        for (int i = 0; i < 40; ++i) {
            pressKey(*editor, Qt::Key_Down);
        }
        paint(*editor);
        CHECK(caret(*editor).center().y() == Approx(focusY).margin(1.0));
        editor->moveCursorToDocEnd();
        paint(*editor);
        CHECK(caret(*editor).center().y() == Approx(focusY).margin(1.0));

        // Back at the start, the view is at the top again
        editor->moveCursorToDocStart();
        paint(*editor);
        CHECK(editor->scrollOffset() == Approx(0.0).margin(0.5));
        CHECK(caret(*editor).top() == Approx(textTop).margin(0.5));
    }
}

TEST_CASE("Stage4 typewriter: a click and manual scrolling leave the view",
          "[editor][stage4][typewriter]") {
    auto editor = editorIn(ViewMode::Continuous);
    // The background pass done: no estimated heights for scroll anchoring to correct
    qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout())
        ->layoutPendingBlocks();
    typewriterOn(*editor);
    editor->setCursorPosition({20, 0});
    paint(*editor);
    const double focusY = editor->height() * 0.5;

    // A click on a line above, well within the view, moves the cursor, not the view
    const double scroll = editor->scrollOffset();
    click(*editor, QPointF(200.0, focusY - 100.0));
    CHECK(editor->scrollOffset() == Approx(scroll).margin(0.5));
    CHECK(editor->cursorPosition().paragraph < 20);

    // The wheel scrolls away from the line, and the view stays there
    editor->setScrollOffset(scroll + 300.0);
    paint(*editor);
    CHECK(editor->scrollOffset() == Approx(scroll + 300.0).margin(0.5));

    // Typing brings the line back to its height
    editor->insertText(QStringLiteral("x"));
    paint(*editor);
    CHECK(caret(*editor).center().y() == Approx(focusY).margin(1.0));
}

TEST_CASE("Stage4 typewriter: turning it on and off keeps the text in place",
          "[editor][stage4][typewriter]") {
    auto editor = editorIn(ViewMode::Page);
    editor->setCursorPosition({25, 0});
    paint(*editor);

    editor->setTypewriterEnabled(true);
    paint(*editor);
    const double focusY = editor->height() * 0.5;
    CHECK(caret(*editor).center().y() == Approx(focusY).margin(1.0));

    editor->setTypewriterEnabled(false);
    paint(*editor);
    CHECK(caret(*editor).center().y() == Approx(focusY).margin(1.0));
}

TEST_CASE("Stage4 keys: Page Down moves the cursor and the view, the cursor in its row",
          "[editor][stage4]") {
    auto editor = editorIn(ViewMode::Continuous, 120);
    // The background pass done: no estimated heights
    qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout())
        ->layoutPendingBlocks();
    editor->setCursorPosition({2, 10});
    paint(*editor);
    const QRectF before = caret(*editor);
    const double scroll = editor->scrollOffset();
    const double viewHeight = editor->height() / viewScale(*editor);
    // The first line the view does not show in full
    const double viewBottom = viewTop(*editor) + viewHeight;
    const std::vector<LineSpan> lines = lineSpans(*editor);
    const auto cut = std::find_if(lines.begin(), lines.end(), [&](const LineSpan& line) {
        return line.bottom > viewBottom + 0.5;
    });
    REQUIRE(cut != lines.end());

    pressKey(*editor, Qt::Key_PageDown);
    paint(*editor);
    CHECK(editor->cursorPosition().paragraph > 2);
    CHECK(editor->scrollOffset() > scroll + viewHeight / 2.0);
    // No line skipped: the view starts at that line at the latest (it may pass over the
    // space between the paragraphs at the bottom of the view)
    CHECK(viewTop(*editor) <= cut->top + 0.5);
    // The cursor keeps its place in the view: its line in the same row
    CHECK(caret(*editor).top() == Approx(before.top()).margin(0.5));

    // Page Up comes back to the same text, in the same row
    pressKey(*editor, Qt::Key_PageUp);
    paint(*editor);
    CHECK(editor->cursorPosition() == CursorPosition{2, 10});
    CHECK(editor->scrollOffset() == Approx(scroll).margin(0.5));
    CHECK(caret(*editor).top() == Approx(before.top()).margin(0.5));
}

TEST_CASE("Stage4 keys: Page Down through the text keeps the row and skips no line",
          "[editor][stage4]") {
    auto editor = editorIn(ViewMode::Continuous, 120);
    auto* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout());
    layout->layoutPendingBlocks();
    const double viewHeight = editor->height() / viewScale(*editor);
    const double documentHeight = layout->documentSize().height();
    const std::vector<LineSpan> lines = lineSpans(*editor);

    editor->setCursorPosition({1, 5});
    paint(*editor);
    const double row = caret(*editor).top();
    std::vector<bool> seen(lines.size(), false);
    for (int press = 0; press < 400; ++press) {
        // The lines fully in view
        const double top = viewTop(*editor);
        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].top >= top - 0.5 && lines[i].bottom <= top + viewHeight + 0.5) {
                seen[i] = true;
            }
        }
        const CursorPosition before = editor->cursorPosition();
        pressKey(*editor, Qt::Key_PageDown);
        paint(*editor);
        if (editor->cursorPosition() == before) {
            break;  // The end of the text
        }
        // The row holds until the view reaches the end of the text
        if (viewTop(*editor) + viewHeight < documentHeight - 1.0) {
            CAPTURE(press);
            CHECK(caret(*editor).top() == Approx(row).margin(0.5));
        }
    }
    CHECK(editor->cursorPosition().paragraph == 119);
    CHECK(std::count(seen.begin(), seen.end(), false) == 0);
}

TEST_CASE("Stage4 keys: in page mode Page Down and Page Up move by one page",
          "[editor][stage4][pagemode]") {
    // The next page shows where this one was, the cursor on the line at the same place of
    // it. Also at the paper scale of a laptop screen at 125% display scaling.
    for (const double paper : {1.0, 1.483}) {
        CAPTURE(paper);
        auto editor = editorIn(ViewMode::Page, 160);
        editor->setPaperScale(paper);
        qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout())
            ->layoutPendingBlocks();
        editor->goToPage(3);
        const double pageThree = editor->scrollOffset();
        editor->goToPage(2);
        const double pitch = pageThree - editor->scrollOffset();
        REQUIRE(pitch > editor->height() / viewScale(*editor));  // more than the view shows

        // The cursor a few lines down page 2
        for (int i = 0; i < 4; ++i) {
            pressKey(*editor, Qt::Key_Down);
        }
        paint(*editor);
        const CursorPosition start = editor->cursorPosition();
        const double scroll = editor->scrollOffset();
        const double startTop = cursorLineTop(*editor);
        const std::vector<LineSpan> lines = lineSpans(*editor);
        // The line nearest to where the cursor's line would be @p pages further on
        const auto lineAt = [&](int pages) {
            const double goal = startTop + pages * pitch;
            return std::min_element(lines.begin(), lines.end(),
                                    [goal](const LineSpan& a, const LineSpan& b) {
                                        return std::abs(a.top - goal) < std::abs(b.top - goal);
                                    })
                ->top;
        };

        for (int pages = 1; pages <= 3; ++pages) {
            CAPTURE(pages);
            pressKey(*editor, Qt::Key_PageDown);
            paint(*editor);
            CHECK(editor->scrollOffset() == Approx(scroll + pages * pitch).margin(0.5));
            CHECK(editor->currentPage() == 2 + pages);
            CHECK(cursorLineTop(*editor) == Approx(lineAt(pages)).margin(0.5));
        }
        for (int pages = 2; pages >= 0; --pages) {
            CAPTURE(pages);
            pressKey(*editor, Qt::Key_PageUp);
            paint(*editor);
            CHECK(editor->scrollOffset() == Approx(scroll + pages * pitch).margin(0.5));
        }
        CHECK(editor->cursorPosition() == start);

        // From page 2 Page Up goes to page 1, and from there to the first line
        pressKey(*editor, Qt::Key_PageUp);
        paint(*editor);
        CHECK(editor->currentPage() == 1);
        CHECK(cursorLineTop(*editor) == Approx(lineAt(-1)).margin(0.5));
        pressKey(*editor, Qt::Key_PageUp);
        paint(*editor);
        CHECK(editor->cursorPosition().paragraph == 0);
        CHECK(cursorLineTop(*editor) == Approx(lines.front().top).margin(0.5));
    }
}

TEST_CASE("Stage4 keys: Page Up and Page Down in a row and the arrows keep the line and the column",
          "[editor][stage4][pagemode]") {
    for (ViewMode mode : {ViewMode::Continuous, ViewMode::Page}) {
        CAPTURE(static_cast<int>(mode));
        auto editor = editorIn(mode, 120);
        qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout())
            ->layoutPendingBlocks();
        for (int offset : {3, 25, 40}) {
            CAPTURE(offset);
            const CursorPosition start{40, offset};
            editor->setCursorPosition(start);
            paint(*editor);

            // Each press goes on from the previous goal (between the pages too), not from the
            // line and the character the previous press hit
            for (int i = 0; i < 3; ++i) {
                pressKey(*editor, Qt::Key_PageDown);
                paint(*editor);
            }
            for (int i = 0; i < 3; ++i) {
                pressKey(*editor, Qt::Key_PageUp);
                paint(*editor);
            }
            CHECK(editor->cursorPosition() == start);

            // The arrows and the page keys keep one column
            for (int key : {Qt::Key_Down, Qt::Key_PageDown, Qt::Key_PageUp, Qt::Key_Up}) {
                pressKey(*editor, key);
                paint(*editor);
            }
            CHECK(editor->cursorPosition() == start);
        }
    }
}

TEST_CASE("Stage4 keys: typing drops the column the arrows keep", "[editor][stage4]") {
    // One paragraph of full lines
    BookEditor editor;
    resizeWidget(editor, QSize(700, 500));
    editor.fromKml(kmlOf({QStringLiteral("Words on a line that wraps. ").repeated(40).trimmed()}));
    editor.setCursorPosition({0, 10});
    paint(editor);
    pressKey(editor, Qt::Key_Down);
    const QRectF column = caret(editor);

    // Typing moves the cursor to the right on its line: the next line down is under it,
    // not under the column of the earlier move
    editor.insertText(QStringLiteral("WWWWW"));
    paint(editor);
    const QRectF typed = caret(editor);
    REQUIRE(typed.top() == Approx(column.top()));
    REQUIRE(typed.left() > column.left() + 20.0);
    pressKey(editor, Qt::Key_Down);
    paint(editor);
    const double below = caret(editor).left();
    CHECK(std::abs(below - typed.left()) < std::abs(below - column.left()));
}

TEST_CASE("Stage4 render: page mode and the scroll modes paint at every zoom",
          "[editor][stage4][render]") {
    // With KALAHARI_RENDER_DUMP_DIR set, the images are saved there to be looked at (the
    // sharpness of the letters at each zoom, on each platform)
    const QString dumpDir = QString::fromLocal8Bit(qgetenv("KALAHARI_RENDER_DUMP_DIR"));
    const auto shot = [&](BookEditor& editor, const QString& name) {
        QImage image(editor.size(), QImage::Format_RGB32);
        editor.render(&image);
        if (!dumpDir.isEmpty()) {
            image.save(QStringLiteral("%1/%2.png").arg(dumpDir, name));
        }
        return image;
    };
    const auto distinctColors = [](const QImage& image) {
        std::vector<QRgb> colors;
        for (int y = 0; y < image.height(); y += 4) {
            for (int x = 0; x < image.width(); x += 4) {
                const QRgb color = image.pixel(x, y);
                if (std::find(colors.begin(), colors.end(), color) == colors.end()) {
                    colors.push_back(color);
                }
            }
        }
        return colors.size();
    };

    auto editor = editorIn(ViewMode::Page, 20);
    for (double zoom : {0.75, 1.0, 1.5, 2.0}) {
        editor->setZoomFactor(zoom);
        editor->goToPage(1);
        CAPTURE(zoom);
        // Desk, paper and anti-aliased text
        CHECK(distinctColors(shot(*editor, QStringLiteral("page_%1").arg(qRound(zoom * 100)))) > 8);
    }
    editor->setViewMode(ViewMode::Continuous);
    for (double zoom : {1.0, 1.5}) {
        editor->setZoomFactor(zoom);
        CAPTURE(zoom);
        CHECK(distinctColors(shot(*editor, QStringLiteral("continuous_%1").arg(qRound(zoom * 100)))) > 4);
    }
}
