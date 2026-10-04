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
#include <cmath>

namespace kalahari::editor {

namespace {

/// Line width used when the document has no text width (no wrapping)
constexpr qreal UNWRAPPED_LINE_WIDTH = 10000.0;

/// Extent Qt itself uses for an "everything" update rect
constexpr qreal UNBOUNDED_EXTENT = 1000000000.0;

/// Accepted range of the line spacing multiplier
constexpr qreal MIN_LINE_SPACING = 0.5;
constexpr qreal MAX_LINE_SPACING = 4.0;

/// Extra space a line gets from the line spacing; whole pixels, so that lines stay on
/// the pixel grid (Qt rounds line heights up to whole pixels too)
qreal extraLeading(qreal lineHeight, qreal lineSpacing) {
    return std::round(lineHeight * (lineSpacing - 1.0));
}

/// Part of the extra leading that goes above the line (the rest goes below it)
qreal leadingAbove(qreal extra) {
    return std::floor(extra / 2.0);
}

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

void KalahariTextDocumentLayout::setTypography(const LayoutTypography& typography) {
    LayoutTypography normalized = typography;
    normalized.lineSpacing = std::clamp(typography.lineSpacing, MIN_LINE_SPACING, MAX_LINE_SPACING);
    normalized.paragraphSpacing = std::max<qreal>(0, typography.paragraphSpacing);
    normalized.firstLineIndent = std::max<qreal>(0, typography.firstLineIndent);
    normalized.referencePointSize = std::max<qreal>(0, typography.referencePointSize);
    if (normalized == m_typography) {
        return;
    }
    m_typography = normalized;
    layoutAllBlocks();
}

qreal KalahariTextDocumentLayout::typographyScale() const {
    if (m_typography.referencePointSize <= 0) {
        return 1.0;
    }
    const qreal pointSize = document()->defaultFont().pointSizeF();
    return pointSize > 0 ? pointSize / m_typography.referencePointSize : 1.0;
}

qreal KalahariTextDocumentLayout::paragraphSpacing() const {
    // Whole pixels keep the blocks below on the pixel grid
    return std::round(m_typography.paragraphSpacing * typographyScale());
}

qreal KalahariTextDocumentLayout::firstLineIndent() const {
    return m_typography.firstLineIndent * typographyScale();
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
    const qreal spacing = paragraphSpacing();
    const qreal indent = firstLineIndent();
    QTextBlock block = doc->findBlockByNumber(first);
    for (int number = first; number <= last && block.isValid(); ++number) {
        heights.push_back(layoutBlock(block, spacing, indent));
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

qreal KalahariTextDocumentLayout::layoutBlock(const QTextBlock& block, qreal spacing,
                                              qreal indent) const {
    QTextLayout* layout = block.layout();
    if (!layout) {
        return QFontMetricsF(document()->defaultFont()).height() + spacing;
    }

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

    // The first-line indent belongs to paragraphs whose lines start at their leading
    // edge (left-aligned and justified ones), never more than half of the line.
    // Right-to-left lines keep x = 0: the shorter line already leaves the indent on
    // their leading (right) side.
    const bool leadingAligned = !(alignment & (Qt::AlignRight | Qt::AlignHCenter));
    const qreal firstIndent = leadingAligned ? std::min(indent, lineWidth / 2.0) : 0.0;
    const bool rightToLeft = block.textDirection() == Qt::RightToLeft;

    // Lines are stacked from y = 0, each with its extra leading split above and below
    // it. Horizontal alignment is Qt's (QTextOption) - the only manual x offset is the
    // indent, which also shortens the line (Qt aligns within the shorter width).
    layout->beginLayout();
    qreal y = 0;
    for (QTextLine line = layout->createLine(); line.isValid(); line = layout->createLine()) {
        const qreal lineIndent = line.lineNumber() == 0 ? firstIndent : 0.0;
        line.setLineWidth(lineWidth - lineIndent);
        const qreal extra = extraLeading(line.height(), m_typography.lineSpacing);
        line.setPosition(QPointF(rightToLeft ? 0.0 : lineIndent, y + leadingAbove(extra)));
        y += line.height() + extra;
    }
    layout->endLayout();

    return y + spacing;
}

QRectF KalahariTextDocumentLayout::lineBox(const QTextLine& line, qreal lineSpacing) {
    const qreal extra = extraLeading(line.height(), lineSpacing);
    return QRectF(line.x(), line.y() - leadingAbove(extra), line.width(), line.height() + extra);
}

int KalahariTextDocumentLayout::lineIndexAt(const QTextLayout& layout, qreal localY,
                                            qreal lineSpacing) {
    const int count = layout.lineCount();
    if (count == 0) {
        return -1;
    }
    // First line whose box ends below localY (boxes are stacked in line order)
    int low = 0;
    int high = count - 1;
    while (low < high) {
        const int middle = low + (high - low) / 2;
        if (localY < lineBox(layout.lineAt(middle), lineSpacing).bottom()) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }
    return low;
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

qreal KalahariTextDocumentLayout::blockHeight(int blockNumber) const {
    if (blockNumber < 0 || static_cast<size_t>(blockNumber) >= m_blockHeights.size()) {
        return 0;
    }
    return m_blockHeights[static_cast<size_t>(blockNumber)];
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

    // Line whose box covers the point (or the nearest line, for a fuzzy hit)
    const qreal localY = y - blockY(number);
    const QTextLine line = layout->lineAt(lineIndexAt(*layout, localY, m_typography.lineSpacing));
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
    // A block the height cache does not know yet (queried while the document is in the
    // middle of a change) gets the default line height
    const qreal height = static_cast<size_t>(number) < m_blockHeights.size()
        ? m_blockHeights[static_cast<size_t>(number)]
        : QFontMetricsF(document()->defaultFont()).height();
    return QRectF(0, blockY(number), documentWidth(), height);
}

}  // namespace kalahari::editor
