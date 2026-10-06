/// @file test_editor_stage4.cpp
/// @brief Editor stage 4: page mode (page flow in the layout, pages in the view), zoom
///        and typewriter scrolling

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/view_modes.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <algorithm>
#include <cmath>
#include <memory>
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

}  // namespace

TEST_CASE("Stage4 page mode: a click lands on the character under it, on every page and zoom",
          "[editor][stage4][pagemode]") {
    auto editor = editorIn(ViewMode::Page);
    paint(*editor);
    REQUIRE(editor->totalPages() >= 3);

    for (double zoom : {0.75, 1.0, 1.5}) {
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
            CAPTURE(zoom, page);
            click(*editor, QPointF(at.left() + 1.0, at.center().y()));
            CHECK(editor->cursorPosition() == target);
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

TEST_CASE("Stage4 typewriter: the line holds at the start and the end of the chapter",
          "[editor][stage4][typewriter]") {
    for (ViewMode mode : {ViewMode::Continuous, ViewMode::Page}) {
        CAPTURE(static_cast<int>(mode));
        auto editor = editorIn(mode, 200);
        typewriterOn(*editor);
        const double focusY = editor->height() * 0.5;

        editor->moveCursorToDocEnd();
        paint(*editor);
        CHECK(caret(*editor).center().y() == Approx(focusY).margin(1.0));

        editor->moveCursorToDocStart();
        paint(*editor);
        CHECK(caret(*editor).center().y() == Approx(focusY).margin(1.0));
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

TEST_CASE("Stage4 keys: Page Down moves the cursor and the view by one view height",
          "[editor][stage4][pagemode]") {
    for (ViewMode mode : {ViewMode::Continuous, ViewMode::Page}) {
        CAPTURE(static_cast<int>(mode));
        auto editor = editorIn(mode, 120);
        // The background pass done: no estimated heights
        qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout())
            ->layoutPendingBlocks();
        editor->setCursorPosition({2, 10});
        paint(*editor);
        const QRectF before = caret(*editor);
        const double scroll = editor->scrollOffset();

        pressKey(*editor, Qt::Key_PageDown);
        paint(*editor);
        CHECK(editor->cursorPosition().paragraph > 2);
        CHECK(editor->scrollOffset() == Approx(scroll + editor->height() /
                                                          editor->zoomFactor()).margin(0.5));
        // The cursor keeps its place in the view: on the line at the same height (the line
        // boxes, with the line spacing, are about 1.6 caret heights)
        CHECK(caret(*editor).center().y() ==
              Approx(before.center().y()).margin(before.height() * 0.8 + 1.0));

        // Page Up comes back to the same text
        pressKey(*editor, Qt::Key_PageUp);
        paint(*editor);
        CHECK(editor->cursorPosition() == CursorPosition{2, 10});
        CHECK(editor->scrollOffset() == Approx(scroll).margin(0.5));
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
