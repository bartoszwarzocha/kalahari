/// @file kalahari_text_document_layout.cpp
/// @brief Custom QAbstractTextDocumentLayout implementation (OpenSpec #00043)

#include <kalahari/editor/kalahari_text_document_layout.h>
#include <QFontMetricsF>
#include <QPainter>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>
#include <algorithm>

namespace kalahari::editor {

namespace {

/// Line width used when the document has no text width (no wrapping)
constexpr qreal UNWRAPPED_LINE_WIDTH = 10000.0;

/// Extent Qt itself uses for an "everything" update rect
constexpr qreal UNBOUNDED_EXTENT = 1000000000.0;

}  // anonymous namespace

// =============================================================================
// Constructor
// =============================================================================

KalahariTextDocumentLayout::KalahariTextDocumentLayout(QTextDocument* doc)
    : QAbstractTextDocumentLayout(doc) {
}

// =============================================================================
// Configuration
// =============================================================================

void KalahariTextDocumentLayout::setTextWidth(qreal width) {
    // QTextDocument::setTextWidth() re-lays out the whole document through
    // documentChanged() even when the width does not change.
    if (document()->textWidth() != width) {
        document()->setTextWidth(width);
    }
}

qreal KalahariTextDocumentLayout::textWidth() const {
    return document()->textWidth();
}

void KalahariTextDocumentLayout::setFont(const QFont& font) {
    // Same as setTextWidth(): setDefaultFont() always re-lays out the whole document.
    if (document()->defaultFont() != font) {
        document()->setDefaultFont(font);
    }
}

QFont KalahariTextDocumentLayout::font() const {
    return document()->defaultFont();
}

void KalahariTextDocumentLayout::layoutAllBlocks() {
    relayoutRange(0, document()->blockCount() - 1,
                  static_cast<int>(m_blockHeights.size()) - 1);
}

// =============================================================================
// Document Changed - Core Layout Logic
// =============================================================================

void KalahariTextDocumentLayout::documentChanged(int from, int charsRemoved, int charsAdded) {
    Q_UNUSED(charsRemoved);
    const QTextDocument* doc = document();

    // Qt reports the change in new-document coordinates: [from, from + charsAdded) holds
    // everything inserted or reformatted (setTextWidth()/setDefaultFont() report the
    // whole document). Every block touching that range is re-laid out, including the
    // block holding the first character after it: it may have been split off from, or
    // merged with, the changed text. Blocks outside the range are untouched.
    QTextBlock first = doc->findBlock(from);
    if (!first.isValid()) {
        first = doc->firstBlock();
    }
    QTextBlock last = doc->findBlock(from + charsAdded);
    if (!last.isValid()) {
        last = doc->lastBlock();
    }

    // Untouched blocks keep their cache entries, so the old entries of the changed range
    // end where the new range ends, shifted by the change in block count.
    const int lastNumber = last.blockNumber();
    const int oldLast =
        lastNumber - (doc->blockCount() - static_cast<int>(m_blockHeights.size()));
    relayoutRange(first.blockNumber(), lastNumber, oldLast);
}

void KalahariTextDocumentLayout::relayoutRange(int first, int last, int oldLast) {
    const QTextDocument* doc = document();
    const int oldCount = static_cast<int>(m_blockHeights.size());

    if (oldLast < first - 1 || oldLast >= oldCount) {
        // The height cache is out of step with the document: rebuild it completely.
        first = 0;
        last = doc->blockCount() - 1;
        oldLast = oldCount - 1;
    }

    const int rangeSize = last - first + 1;
    std::vector<qreal> heights;
    heights.reserve(static_cast<size_t>(std::max(rangeSize, 0)));
    QTextBlock block = doc->findBlockByNumber(first);
    for (int number = first; number <= last && block.isValid(); ++number) {
        layoutBlock(block);
        heights.push_back(measuredHeight(block));
        block = block.next();
    }

    // Replace the range's cache entries. Positions only move when a height changed or
    // blocks were added/removed - typing within a line leaves them valid.
    const auto rangeBegin = m_blockHeights.begin() + first;
    bool geometryChanged = true;
    if (static_cast<int>(heights.size()) == oldLast - first + 1) {
        geometryChanged = !std::equal(heights.begin(), heights.end(), rangeBegin);
        std::copy(heights.begin(), heights.end(), rangeBegin);
    } else {
        // oldLast + 1 first: oldLast is -1 when the cache is empty or blocks were
        // inserted at the very start, and begin() - 1 is not a valid iterator
        m_blockHeights.erase(rangeBegin, m_blockHeights.begin() + (oldLast + 1));
        m_blockHeights.insert(m_blockHeights.begin() + first, heights.begin(), heights.end());
    }
    if (geometryChanged) {
        m_positionsDirty = true;
    }

    emit blocksLaidOut(first, static_cast<int>(heights.size()));

    const QSizeF size = documentSize();
    if (size != m_lastReportedSize) {
        m_lastReportedSize = size;
        emit documentSizeChanged(size);
    }

    // Repaint the re-laid-out blocks, and everything below them if they moved it
    const qreal top = blockY(first);
    const qreal bottom = (geometryChanged || heights.empty())
        ? UNBOUNDED_EXTENT
        : blockY(last) + m_blockHeights[static_cast<size_t>(last)];
    emit update(QRectF(0, top, UNBOUNDED_EXTENT, bottom - top));
}

void KalahariTextDocumentLayout::layoutBlock(const QTextBlock& block) const {
    QTextLayout* layout = block.layout();
    if (!layout) return;

    // Glyph fonts come from the document's character formats (resolved against the
    // document's default font), so the QTextLayout's own font is irrelevant here.

    // Get alignment from QTextBlockFormat and configure QTextOption
    Qt::Alignment alignment = block.blockFormat().alignment();
    if (alignment == 0) {
        alignment = Qt::AlignLeft;  // Default to left if not set
    }

    QTextOption textOption;
    textOption.setAlignment(alignment);
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout->setTextOption(textOption);

    // Enable layout caching, as Qt's own manual-QTextLayout example does. Candidate fix
    // for Qt::AlignJustify rendering ragged: the uncached path may drop the per-line
    // justified spacing at draw time. Also a small perf win (avoids re-shaping on redraw).
    layout->setCacheEnabled(true);

    const qreal width = document()->textWidth();
    const qreal lineWidth = width > 0 ? width : UNWRAPPED_LINE_WIDTH;

    // Lines start at x=0, y=0. Qt handles horizontal alignment via QTextOption - DO NOT
    // manually offset x (Qt + manual offset = double alignment).
    layout->beginLayout();
    qreal y = 0;
    for (QTextLine line = layout->createLine(); line.isValid(); line = layout->createLine()) {
        line.setLineWidth(lineWidth);
        line.setPosition(QPointF(0, y));
        y += line.height();
    }
    layout->endLayout();
}

qreal KalahariTextDocumentLayout::measuredHeight(const QTextBlock& block) const {
    if (QTextLayout* layout = block.layout()) {
        // boundingRect gives tight bounds without extra leading
        const qreal height = layout->boundingRect().height();
        if (height > 0) {
            return height;
        }
    }

    // Fallback: estimate from font
    return QFontMetricsF(document()->defaultFont()).height();
}

// =============================================================================
// Block Position Management
// =============================================================================

void KalahariTextDocumentLayout::updateBlockPositions() const {
    if (!m_positionsDirty) return;

    m_blockYPositions.resize(m_blockHeights.size());
    qreal y = 0;
    for (size_t i = 0; i < m_blockHeights.size(); ++i) {
        m_blockYPositions[i] = y;
        y += m_blockHeights[i];
    }

    m_cachedDocumentHeight = y;
    m_positionsDirty = false;
}

qreal KalahariTextDocumentLayout::blockY(int blockNumber) const {
    updateBlockPositions();

    if (blockNumber < 0) {
        return 0;
    }
    if (static_cast<size_t>(blockNumber) >= m_blockYPositions.size()) {
        return m_cachedDocumentHeight;
    }
    return m_blockYPositions[static_cast<size_t>(blockNumber)];
}

int KalahariTextDocumentLayout::blockNumberAtY(qreal y) const {
    updateBlockPositions();

    if (m_blockYPositions.empty()) {
        return -1;
    }
    // Last block starting at or above y
    const auto it = std::upper_bound(m_blockYPositions.begin(), m_blockYPositions.end(), y);
    if (it == m_blockYPositions.begin()) {
        return 0;
    }
    return static_cast<int>(std::distance(m_blockYPositions.begin(), it)) - 1;
}

qreal KalahariTextDocumentLayout::documentWidth() const {
    return qMax<qreal>(0, document()->textWidth());
}

// =============================================================================
// QAbstractTextDocumentLayout Overrides
// =============================================================================

void KalahariTextDocumentLayout::draw(QPainter* painter, const PaintContext& context) {
    if (!painter) return;

    const QRectF& clip = context.clip;
    painter->save();
    if (!clip.isEmpty()) {
        painter->setClipRect(clip);
    }

    int number = clip.isEmpty() ? 0 : blockNumberAtY(clip.top());
    for (QTextBlock block = document()->findBlockByNumber(number); block.isValid();
         block = block.next(), ++number) {
        const qreal y = blockY(number);
        if (!clip.isEmpty() && y > clip.bottom()) {
            break;
        }
        if (QTextLayout* layout = block.layout()) {
            layout->draw(painter, QPointF(0, y));
        }
    }

    painter->restore();
}

int KalahariTextDocumentLayout::hitTest(const QPointF& point, Qt::HitTestAccuracy accuracy) const {
    const bool exact = (accuracy == Qt::ExactHit);
    const qreal y = point.y();
    if (exact && (y < 0 || y >= documentSize().height())) {
        return -1;
    }

    // A fuzzy hit above or below the document snaps to the first or last block
    const int number = blockNumberAtY(y);
    const QTextBlock block = document()->findBlockByNumber(number);
    QTextLayout* layout = block.isValid() ? block.layout() : nullptr;
    if (!layout || layout->lineCount() == 0) {
        return exact ? -1 : qMax(0, document()->characterCount() - 1);
    }

    // Line covering the point (or the nearest line, for a fuzzy hit)
    const qreal localY = y - blockY(number);
    int lineIndex = layout->lineCount() - 1;
    for (int i = 0; i < layout->lineCount(); ++i) {
        const QTextLine line = layout->lineAt(i);
        if (localY < line.y() + line.height()) {
            lineIndex = i;
            break;
        }
    }
    const QTextLine line = layout->lineAt(lineIndex);
    const int pos = line.xToCursor(point.x(), exact ? QTextLine::CursorOnCharacter
                                                    : QTextLine::CursorBetweenCharacters);
    return block.position() + pos;
}

int KalahariTextDocumentLayout::pageCount() const {
    return 1;  // Continuous layout, single page
}

QSizeF KalahariTextDocumentLayout::documentSize() const {
    updateBlockPositions();
    return QSizeF(documentWidth(), m_cachedDocumentHeight);
}

QRectF KalahariTextDocumentLayout::frameBoundingRect(QTextFrame* frame) const {
    if (frame == document()->rootFrame()) {
        return QRectF(QPointF(0, 0), documentSize());
    }
    return QRectF();
}

QRectF KalahariTextDocumentLayout::blockBoundingRect(const QTextBlock& block) const {
    if (!block.isValid()) return QRectF();

    const int number = block.blockNumber();
    const qreal height = static_cast<size_t>(number) < m_blockHeights.size()
        ? m_blockHeights[static_cast<size_t>(number)]
        : measuredHeight(block);
    return QRectF(0, blockY(number), documentWidth(), height);
}

}  // namespace kalahari::editor
