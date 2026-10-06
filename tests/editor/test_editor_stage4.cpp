/// @file test_editor_stage4.cpp
/// @brief Editor stage 4: page mode (page flow in the layout, pages in the view), zoom
///        and typewriter scrolling

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include "editor_test_utils.h"

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
