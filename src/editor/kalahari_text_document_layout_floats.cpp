// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file kalahari_text_document_layout_floats.cpp
/// @brief KalahariTextDocumentLayout: the images the text flows around - beside the text (on
///        the left or the right of the text column) or above and below it; with page flow
///        such an image goes on to the next page when it does not fit in the rest of its own,
///        and the text after it fills that rest

#include "layout_flow_internal.h"

#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/text_captions.h>
#include <kalahari/editor/text_footnotes.h>
#include <kalahari/editor/text_images.h>

#include <QTextDocument>
#include <QTextLayout>
#include <QTextLine>

#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

/// The narrowest room beside an image a line takes, in centimeters: with less, the line goes
/// on below the image
constexpr qreal NARROWEST_BESIDE_CENTIMETERS = 2.5;

/// The points of a centimeter
constexpr qreal FLOAT_POINTS_PER_CENTIMETER = 72.0 / 2.54;

/// Times a line is fitted beside the images: where the height of the line before puts it,
/// again where its own height (and its notes) put it; one more for each image beside the text
/// (below which a line may go)
constexpr int FIT_ROUNDS = 3;

bool isSquare(ImageWrap wrap) {
    return wrap == ImageWrap::SquareLeft || wrap == ImageWrap::SquareRight;
}

}  // namespace

/// @brief What laying out the lines of a block beside the images needs, and what it gives
struct KalahariTextDocumentLayout::WrapLines {
    const QTextBlock& block;
    const BlockMetrics& metrics;
    LineFrame frame;
    qreal top = 0.0;               ///< The block's top (document coordinates)
    bool paged = false;            ///< With page flow
    std::vector<size_t> notes;     ///< Its footnotes (indices of footnoteNumbers())
    std::vector<char> breakBefore; ///< Per line: it starts the next text area
    std::vector<qreal> pages;      ///< Per line: the page it is on (page flow)
    qreal linesHeight = 0.0;       ///< Its line boxes together, without page breaks
};

// =============================================================================
// The images and their blocks
// =============================================================================

void KalahariTextDocumentLayout::addFloat(FlowEntry& entry, const QTextBlock& block) const {
    // In a cell of a table an image is a part of its line (floatingImage())
    const std::optional<ImageProperties> image = floatingImage(block);
    if (!image) {
        return;
    }
    entry.flags |= FLOW_FLOAT;
    entry.floatWrap = image->wrap;
    entry.floatSize = imageSize(block, *image);
    entry.floatGap = std::round(image->spaceBeside * packagePixels());
    m_hasFloats = true;
}

std::optional<KalahariTextDocumentLayout::FloatRun> KalahariTextDocumentLayout::floatRunAt(
    size_t index, const QTextBlock& block) const {
    const size_t count = m_blockFlowEntries.size();
    if ((m_blockFlowEntries[index].flags & FLOW_FLOAT) == 0) {
        // The caption above the image of the next block starts its blocks
        if (index + 1 >= count || (m_blockFlowEntries[index + 1].flags & FLOW_FLOAT) == 0 ||
            captionBlockOf(block.next()) != block) {
            return std::nullopt;
        }
        return FloatRun{index, index + 1, index + 1};
    }
    const QTextBlock caption = captionBlockOf(block);
    if (caption.isValid() && caption == block.previous()) {
        return std::nullopt;  // its blocks start with its caption above it
    }
    if (caption.isValid() && caption == block.next() && index + 1 < count) {
        return FloatRun{index, index, index + 1};
    }
    return FloatRun{index, index, index};
}

bool KalahariTextDocumentLayout::placeFloatsAt(size_t& index, QTextBlock& block, qreal& y) const {
    if (!m_hasFloats) {
        return false;
    }
    pruneFloats(y);
    const FlowEntry& entry = m_blockFlowEntries[index];
    const std::optional<FloatRun> run = floatRunAt(index, block);
    if (!run) {
        // A row of a table and an object (a line, a break) take the whole width: below the
        // images beside the text
        const bool row = (entry.flags & FLOW_ROW_START) != 0;
        const bool object =
            (entry.flags & FLOW_TABLE) == 0 && blockObject(block) != TextObject::None;
        if (!m_activeFloats.empty() && (row || object)) {
            y = belowFloats(y);
        }
        return false;
    }
    if (!isSquare(m_blockFlowEntries[run->image].floatWrap)) {
        y = belowFloats(y);  // above and below the text: all the width
    }
    if (!placeFloat(*run, block, y)) {
        return false;
    }
    // Its blocks take no room in the flow; in the continuous views the room under them stays
    for (; index < run->last; ++index, block = block.next()) {
        y += m_pageFlow.enabled ? 0.0 : m_blockSpaceBelow[index];
    }
    y += m_pageFlow.enabled ? 0.0 : m_blockSpaceBelow[index];
    return true;
}

bool KalahariTextDocumentLayout::placeFloat(const FloatRun& run, const QTextBlock& first,
                                            qreal y) const {
    const FlowEntry& entry = m_blockFlowEntries[run.image];
    const ImageWrap wrap = entry.floatWrap;
    const bool square = isSquare(wrap);
    if (!square && !m_pageFlow.enabled) {
        return false;  // above and below the text: where the text puts it
    }
    for (size_t k = run.first; k <= run.last; ++k) {
        if (m_blockPending[k] != 0) {
            return false;  // its lines are not laid out yet: estimated, in the flow
        }
    }

    // Its box: beside the text as wide as the image, its caption's lines as wide; above and
    // below the text its blocks as they are
    const qreal documentWidth = document()->textWidth();
    const qreal textWidth = documentWidth > 0 ? documentWidth : UNWRAPPED_LINE_WIDTH;
    const qreal width =
        square ? std::clamp<qreal>(entry.floatSize.width(), 1.0, textWidth) : textWidth;
    qreal height = 0.0;
    for (size_t k = run.first; k <= run.last; ++k) {
        height += m_blockHeights[k];
    }
    qreal top = floatTop(y, square ? 0.0 : height, wrap);
    if (!square && top <= y + FLOW_EPSILON) {
        return false;  // it fits where it is: in the flow
    }
    std::vector<qreal> tops;
    height = 0.0;
    QTextBlock block = first;
    for (size_t k = run.first; k <= run.last; ++k, block = block.next()) {
        const BlockMetrics& metrics = blockMetrics(block);
        tops.push_back(height);
        LineFrame frame = lineFrame(block, metrics);
        if (square) {
            frame.left = 0.0;
            frame.right = width;
            frame.firstIndent = 0.0;
        }
        height += stackLines(block, metrics, frame);
    }
    if (square) {
        top = floatTop(y, height, wrap);
    }

    // Its blocks where the text puts them, with no height in the flow; their lines in its box
    const qreal x = wrap == ImageWrap::SquareRight ? textWidth - width : 0.0;
    block = first;
    for (size_t k = run.first; k <= run.last; ++k, block = block.next()) {
        shiftLines(block, square ? x : 0.0, top - y + tops[k - run.first]);
        m_blockYPositions[k] = y;
        m_blockFlowOffsets[k] = FLOW_WRAPPED;
        m_blockFlowHeights[k] = 0.0;
    }
    m_floats.push_back(FloatPlace{run.first, run.image, run.last, wrap,
                                  QRectF(x, top, width, height), entry.floatGap});
    if (square) {
        m_activeFloats.push_back(m_floats.size() - 1);
    } else {
        reserveTop(top, height);
    }
    return true;
}

qreal KalahariTextDocumentLayout::floatTop(qreal y, qreal height, ImageWrap wrap) const {
    // On its page when it fits there, else at the top of the next one; below another image on
    // its side
    qreal top = y;
    for (size_t round = 0; round <= m_activeFloats.size(); ++round) {
        if (m_pageFlow.enabled) {
            top = topOnPage(top, height);
        }
        qreal below = top;
        for (const size_t index : m_activeFloats) {
            const FloatPlace& place = m_floats[index];
            if (place.wrap == wrap && place.rect.top() < top + std::max(height, 1.0) &&
                place.rect.bottom() > top + FLOW_EPSILON) {
                below = std::max(below, place.rect.bottom());
            }
        }
        if (below <= top + FLOW_EPSILON) {
            return top;
        }
        top = below;
    }
    return top;
}

qreal KalahariTextDocumentLayout::topOnPage(qreal top, qreal height) const {
    const qreal pitch = m_pageFlow.pitch;
    const qreal page = std::floor((top + FLOW_EPSILON) / pitch);
    const qreal areaTop = areaTopOf(page);
    const qreal offset = std::max(top - page * pitch, areaTop);
    // At the top of a text area it stays, as a line does, though it is higher than the area
    if (offset <= areaTop + FLOW_EPSILON ||
        offset + height <= flowAreaHeight(page) + FLOW_EPSILON) {
        return page * pitch + offset;
    }
    const qreal next = page + 1.0;
    return next * pitch + areaTopOf(next);
}

qreal KalahariTextDocumentLayout::areaTopOf(qreal page) const {
    const auto index = static_cast<size_t>(std::max<qreal>(0.0, page));
    return page >= 0.0 && index < m_pageTops.size() ? m_pageTops[index] : 0.0;
}

bool KalahariTextDocumentLayout::touchesTops(qreal top, qreal height) const {
    if (m_pageTops.empty()) {
        return false;
    }
    const qreal pitch = m_pageFlow.pitch;
    const auto first = static_cast<int>(std::floor((top + FLOW_EPSILON) / pitch));
    const int last =
        std::max(first, static_cast<int>(std::floor((top + height - FLOW_EPSILON) / pitch)));
    for (int page = first; page <= last; ++page) {
        if (areaTopOf(static_cast<qreal>(page)) > FLOW_EPSILON) {
            return true;
        }
    }
    return false;
}

void KalahariTextDocumentLayout::reserveTop(qreal top, qreal height) const {
    const qreal page = std::floor((top + FLOW_EPSILON) / m_pageFlow.pitch);
    const auto index = static_cast<size_t>(std::max<qreal>(0.0, page));
    if (index >= m_pageTops.size()) {
        m_pageTops.resize(index + 1, 0.0);
    }
    m_pageTops[index] = std::max(m_pageTops[index], top + height - page * m_pageFlow.pitch);
}

void KalahariTextDocumentLayout::resetFloats() const {
    m_floats.clear();
    m_activeFloats.clear();
    m_pageTops.clear();
}

qreal KalahariTextDocumentLayout::floatsBottom(qreal bottom) const {
    for (const FloatPlace& place : m_floats) {
        bottom = std::max(bottom, place.rect.bottom());
    }
    return bottom;
}

void KalahariTextDocumentLayout::pruneFloats(qreal y) const {
    std::erase_if(m_activeFloats, [this, y](size_t index) {
        return m_floats[index].rect.bottom() <= y + FLOW_EPSILON;
    });
}

qreal KalahariTextDocumentLayout::belowFloats(qreal y) const {
    qreal below = y;
    for (const size_t index : m_activeFloats) {
        if (const QRectF& rect = m_floats[index].rect; rect.top() <= y + FLOW_EPSILON) {
            below = std::max(below, rect.bottom());
        }
    }
    pruneFloats(below);
    return below;
}

// =============================================================================
// Lines
// =============================================================================

KalahariTextDocumentLayout::LineFrame KalahariTextDocumentLayout::lineFrame(
    const QTextBlock& block, const BlockMetrics& metrics, const CellPlacement* cell) const {
    // The lines lie between the margins of the style, in a cell between those of its text.
    // The first-line indent belongs to paragraphs whose lines start at their leading edge
    // (left-aligned and justified ones), never more than half of the line (a negative one, a
    // hanging indent, never out of the left margin). A list item's first line starts after
    // its number. A paragraph of a table has no first-line indent: the cell's text starts at
    // its edge.
    const qreal documentWidth = document()->textWidth();
    const qreal textWidth = documentWidth > 0 ? documentWidth : UNWRAPPED_LINE_WIDTH;
    const qreal width = cell != nullptr ? cell->width : textWidth;
    const qreal lineWidth = std::max<qreal>(1.0, width - metrics.leftMargin - metrics.rightMargin);
    const Qt::Alignment own = ownAlignment(block.blockFormat());
    const Qt::Alignment alignment = own ? own : metrics.alignment;
    const bool leadingAligned = !(alignment & (Qt::AlignRight | Qt::AlignHCenter));
    const qreal ownIndent = metrics.listItem ? markerOverflow(block, metrics)
                            : cell != nullptr ? 0.0
                                              : metrics.firstLineIndent;
    LineFrame frame;
    frame.left = metrics.leftMargin;
    frame.right = metrics.leftMargin + lineWidth;
    frame.firstIndent =
        leadingAligned ? std::clamp(ownIndent, -metrics.leftMargin, lineWidth / 2.0) : 0.0;
    frame.rightToLeft = block.textDirection() == Qt::RightToLeft;
    frame.empty = block.length() <= 1;
    return frame;
}

qreal KalahariTextDocumentLayout::stackLines(const QTextBlock& block, const BlockMetrics& metrics,
                                             const LineFrame& frame) const {
    // As layoutBlock() stacks them, between the edges of the frame
    QTextLayout* layout = block.layout();
    layout->beginLayout();
    qreal y = metrics.spaceBefore;
    for (QTextLine line = layout->createLine(); line.isValid(); line = layout->createLine()) {
        const qreal indent = line.lineNumber() == 0 ? frame.firstIndent : 0.0;
        line.setLineWidth(std::max<qreal>(1.0, frame.right - frame.left - indent));
        const qreal excess = imageExcess(block, line, metrics);
        line.setPosition(QPointF(frame.left + (frame.rightToLeft ? 0.0 : indent),
                                 y + lineTopInBox(line, metrics, frame.empty, excess)));
        y += lineBoxHeight(line, metrics, frame.empty, excess);
    }
    layout->endLayout();
    return y + metrics.spaceAfter;
}

void KalahariTextDocumentLayout::shiftLines(const QTextBlock& block, qreal dx, qreal dy) {
    QTextLayout* layout = block.layout();
    for (int i = 0; i < layout->lineCount(); ++i) {
        QTextLine line = layout->lineAt(i);
        line.setPosition(line.position() + QPointF(dx, dy));
    }
}

KalahariTextDocumentLayout::LineRoom KalahariTextDocumentLayout::roomBeside(
    qreal top, qreal height, const LineFrame& frame, qreal indent) const {
    const qreal pixels = packagePixels();
    const qreal narrowest = NARROWEST_BESIDE_CENTIMETERS * FLOAT_POINTS_PER_CENTIMETER * pixels;
    qreal y = top;
    for (size_t round = 0; round <= m_activeFloats.size(); ++round) {
        qreal left = frame.left;
        qreal right = frame.right;
        qreal below = -1.0;  // the bottom of the lowest image beside the line
        for (const size_t index : m_activeFloats) {
            const FloatPlace& place = m_floats[index];
            if (y + height <= place.rect.top() + FLOW_EPSILON ||
                y >= place.rect.bottom() - FLOW_EPSILON) {
                continue;
            }
            if (place.wrap == ImageWrap::SquareLeft) {
                left = std::max(left, place.rect.right() + place.gap);
            } else {
                right = std::min(right, place.rect.left() - place.gap);
            }
            below = std::max(below, place.rect.bottom());
        }
        const qreal width = right - left - indent;
        if (below < 0.0 || width >= narrowest) {
            return LineRoom{y, left + (frame.rightToLeft ? 0.0 : indent),
                            std::max<qreal>(1.0, width)};
        }
        y = below;  // too little room beside them: below them
    }
    return LineRoom{y, frame.left + (frame.rightToLeft ? 0.0 : indent),
                    std::max<qreal>(1.0, frame.right - frame.left - indent)};
}

bool KalahariTextDocumentLayout::wrapsBeside(size_t index) const {
    // A laid out paragraph (not of a table) beside the images, or laid out beside them before
    return m_blockPending[index] == 0 && (m_blockFlowEntries[index].flags & FLOW_TABLE) == 0 &&
           (!m_activeFloats.empty() || m_blockFlowOffsets[index] == FLOW_WRAPPED);
}

qreal KalahariTextDocumentLayout::wrapBlock(size_t index, const QTextBlock& block,
                                            qreal top) const {
    const BlockMetrics& metrics = blockMetrics(block);
    QTextLayout* layout = block.layout();
    const int linesBefore = layout->lineCount();
    if (m_activeFloats.empty()) {
        // No image beside it any more: its lines across the whole width again
        qreal extent = 0.0;
        qreal lead = 0.0;
        const qreal natural = layoutBlock(block, &extent, layout->cacheEnabled(), &lead);
        m_blockFlowOffsets[index] = FLOW_PLACE_AGAIN;
        m_blockFlowHeights[index] = 0.0;
        qreal height = natural;
        if (m_pageFlow.enabled) {
            height = paginateBlock(block, top, metrics);
            m_blockFlowHeights[index] = height;
        } else {
            m_blockFlowOffsets[index] = FLOW_UNPLACED;
        }
        if (layout->lineCount() != linesBefore) {
            m_marginCountsFrom = std::min(m_marginCountsFrom, static_cast<int>(index));
        }
        return height;
    }

    WrapLines wrap{block, metrics, lineFrame(block, metrics), top, m_pageFlow.enabled, {}, {}, {},
                   0.0};
    if (wrap.paged) {
        wrap.notes = notesBetween(block.position(), block.position() + block.length(),
                                  NoteKind::Footnote);
    }
    // The notes of the pages from the block's on as they were before it: placed again with
    // its lines
    const auto firstPage = static_cast<size_t>(
        wrap.paged ? std::max<qreal>(0.0, std::floor((top + FLOW_EPSILON) / m_pageFlow.pitch))
                   : 0.0);
    const std::vector<PageNotes> notesBefore =
        wrap.notes.empty() || firstPage >= m_pageNotes.size()
            ? std::vector<PageNotes>{}
            : std::vector<PageNotes>(m_pageNotes.begin() + static_cast<std::ptrdiff_t>(firstPage),
                                     m_pageNotes.end());
    const int notesLastBefore = m_notesLastPage;
    const auto restoreNotes = [&]() {
        if (!wrap.notes.empty()) {
            m_pageNotes.resize(std::min(m_pageNotes.size(), firstPage));
            m_pageNotes.insert(m_pageNotes.end(), notesBefore.begin(), notesBefore.end());
            m_notesLastPage = notesLastBefore;
        }
    };
    qreal height = linesBeside(wrap);
    for (int round = 0; round < KEEP_LINES_ROUNDS && wrap.paged && !metrics.pageBreak &&
                        wrap.pages.size() > 1 && wrap.pages.front() != wrap.pages.back();
         ++round) {
        if (!keepLines(wrap)) {
            break;
        }
        restoreNotes();
        height = linesBeside(wrap);
    }
    m_blockFlowOffsets[index] = FLOW_WRAPPED;
    m_blockFlowHeights[index] = height;
    if (layout->lineCount() != linesBefore) {
        m_marginCountsFrom = std::min(m_marginCountsFrom, static_cast<int>(index));
    }
    return height;
}

qreal KalahariTextDocumentLayout::linesBeside(WrapLines& wrap) const {
    // Each line is laid out where it goes: beside the images in the room they leave, below
    // them when that is too little; with page flow on the pages as paginateBlock() places the
    // lines of a paragraph
    QTextLayout* layout = wrap.block.layout();
    const BlockMetrics& metrics = wrap.metrics;
    wrap.pages.clear();
    wrap.linesHeight = 0.0;
    qreal bottom = wrap.top + metrics.spaceBefore;  // of the last line's box
    qreal box = boxHeightOf(metrics.emptyLineHeight, metrics);
    layout->beginLayout();
    for (QTextLine line = layout->createLine(); line.isValid(); line = layout->createLine()) {
        std::vector<size_t> own;
        const LineRoom room = fitLine(line, wrap, bottom, box, own);
        if (!own.empty()) {
            reserveNotes(room.top + box, own);
        }
        if (wrap.paged) {
            wrap.pages.push_back(std::floor((room.top + FLOW_EPSILON) / m_pageFlow.pitch));
        }
        const qreal excess = imageExcess(wrap.block, line, metrics);
        const qreal lineTop = lineTopInBox(line, metrics, wrap.frame.empty, excess);
        line.setPosition(QPointF(room.x, room.top - wrap.top + lineTop));
        wrap.linesHeight += box;
        bottom = room.top + box;
    }
    layout->endLayout();
    return bottom - wrap.top + metrics.spaceAfter;
}

KalahariTextDocumentLayout::LineRoom KalahariTextDocumentLayout::fitLine(
    QTextLine& line, const WrapLines& wrap, qreal boxTop, qreal& box,
    std::vector<size_t>& own) const {
    const auto number = static_cast<size_t>(line.lineNumber());
    const qreal indent = number == 0 ? wrap.frame.firstIndent : 0.0;
    const bool breaks = number < wrap.breakBefore.size() && wrap.breakBefore[number] != 0;
    qreal candidate = boxTop;
    LineRoom room;
    const auto rounds = static_cast<int>(FIT_ROUNDS + m_activeFloats.size());
    for (int round = 0; round < rounds; ++round) {
        const qreal placed = placedInFlow(wrap, candidate, box, breaks, own);
        room = roomBeside(placed, box, wrap.frame, indent);
        line.setLineWidth(room.width);
        const qreal excess = imageExcess(wrap.block, line, wrap.metrics);
        const qreal height = lineBoxHeight(line, wrap.metrics, wrap.frame.empty, excess);
        own = lineNotes(wrap, line);
        const bool settled =
            room.top <= placed + FLOW_EPSILON && std::abs(height - box) < FLOW_EPSILON &&
            placedInFlow(wrap, candidate, height, breaks, own) <= placed + FLOW_EPSILON;
        box = height;
        if (settled) {
            break;
        }
        candidate = std::max(candidate, room.top);  // below the images, with too little room
    }
    return room;
}

qreal KalahariTextDocumentLayout::placedInFlow(const WrapLines& wrap, qreal top, qreal height,
                                               bool breakBefore,
                                               const std::vector<size_t>& notes) const {
    if (!wrap.paged || wrap.metrics.pageBreak) {
        return top;
    }
    const qreal needed = notes.empty() ? 0.0 : notesNeeded(notes);
    qreal placed = flowLineTop(top, height, needed);
    const qreal page = std::floor((placed + FLOW_EPSILON) / m_pageFlow.pitch);
    if (breakBefore && placed - page * m_pageFlow.pitch > areaTopOf(page) + FLOW_EPSILON) {
        placed = flowLineTop(nextPageTop(placed), height, needed);
    }
    return placed;
}

std::vector<size_t> KalahariTextDocumentLayout::lineNotes(const WrapLines& wrap,
                                                          const QTextLine& line) const {
    std::vector<size_t> own;
    if (wrap.notes.empty()) {
        return own;
    }
    const std::vector<NotePlace>& places = footnoteNumbers().notes();
    const int start = wrap.block.position() + line.textStart();
    const int end = start + line.textLength();
    for (const size_t index : wrap.notes) {
        if (index < places.size() && places[index].position >= start &&
            places[index].position < end) {
            own.push_back(index);
        }
    }
    return own;
}

bool KalahariTextDocumentLayout::keepLines(WrapLines& wrap) const {
    // As paginateBlock() keeps them: the whole paragraph on the next page (all its lines on
    // one page, or its first line alone at the end of a page); its last line alone at the top
    // of a page takes the line before it
    std::vector<char>& breakBefore = wrap.breakBefore;
    const std::vector<qreal>& pages = wrap.pages;
    breakBefore.resize(pages.size(), 0);
    const auto startOnNextPage = [&breakBefore]() {
        if (breakBefore.front() != 0) {
            return false;
        }
        std::fill(breakBefore.begin(), breakBefore.end(), 0);
        breakBefore.front() = 1;
        return true;
    };
    if (wrap.metrics.keepTogether && wrap.linesHeight <= m_pageFlow.textHeight + FLOW_EPSILON) {
        return startOnNextPage();
    }
    if (!wrap.metrics.widowControl || pages.size() < 2) {
        return false;
    }
    const size_t last = pages.size() - 1;
    if (pages[0] != pages[1]) {
        return startOnNextPage();
    }
    if (pages[last] != pages[last - 1] && breakBefore[last - 1] == 0) {
        breakBefore[last - 1] = 1;
        return true;
    }
    return false;
}

// =============================================================================
// Queries
// =============================================================================

std::optional<int> KalahariTextDocumentLayout::floatBlockAt(const QPointF& point) const {
    for (const FloatPlace& place : m_floats) {
        if (!place.rect.contains(point)) {
            continue;
        }
        // The block of the line under the point: the image's, or its caption's
        QTextBlock block = document()->findBlockByNumber(static_cast<int>(place.first));
        for (size_t k = place.first; k <= place.last && block.isValid();
             ++k, block = block.next()) {
            const QTextLayout* layout = block.layout();
            const qreal localY = point.y() - m_blockYPositions[k];
            for (int i = 0; layout != nullptr && i < layout->lineCount(); ++i) {
                const QRectF box = lineBoxOf(block, layout->lineAt(i));
                if (localY >= box.top() && localY < box.bottom()) {
                    return static_cast<int>(k);
                }
            }
        }
        return static_cast<int>(place.image);
    }
    return std::nullopt;
}

int KalahariTextDocumentLayout::firstBlockShownAtY(qreal y) const {
    int number = firstBlockOfRow(blockNumberAtY(y));
    // An image placed below the block it goes with (on the next page) reaches up to y
    for (const FloatPlace& place : m_floats) {
        if (place.rect.bottom() > y && static_cast<int>(place.first) < number) {
            number = static_cast<int>(place.first);
        }
    }
    return number;
}

}  // namespace kalahari::editor
