/// @file kalahari_text_document_layout.cpp
/// @brief Custom QAbstractTextDocumentLayout implementation (OpenSpec #00043)

#include <kalahari/editor/kalahari_text_document_layout.h>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QPainter>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>
#include <QTimer>
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

/// An edit touching at most this many blocks lays them out at once. A larger one (a
/// load, a long paste or its undo) leaves them waiting for layout.
constexpr int IMMEDIATE_LAYOUT_LIMIT = 32;

/// Time one step of the background pass may take, so input stays responsive
constexpr qint64 BACKGROUND_STEP_MS = 4;

/// Share of the line width that wrapped text fills on average (estimates)
constexpr qreal AVERAGE_LINE_FILL = 0.95;

/// Page flow offset of a block whose lines follow its own top (not placed on a page)
constexpr qreal FLOW_UNPLACED = -1.0;

/// Tolerance of page flow comparisons (its lengths are whole pixels)
constexpr qreal FLOW_EPSILON = 0.01;

/// Prose whose width per character is the average character width of the estimates
const QString& estimateSample() {
    static const QString sample = QStringLiteral(
        "The quick brown fox jumps over the lazy dog. Zażółć gęślą jaźń, mówiła.");
    return sample;
}

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
    : QAbstractTextDocumentLayout(doc)
    , m_layoutWidth(doc ? doc->textWidth() : -1)
    , m_layoutFont(doc ? doc->defaultFont() : QFont())
    , m_backgroundTimer(new QTimer(this)) {
    if (doc) {
        updateEstimateMetrics();
    }
    // Runs whenever the event loop has nothing else to do, while blocks wait for layout
    m_backgroundTimer->setInterval(0);
    connect(m_backgroundTimer, &QTimer::timeout, this, &KalahariTextDocumentLayout::layoutStep);
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

void KalahariTextDocumentLayout::setPageFlow(const PageFlow& flow) {
    // Whole pixels keep the lines on the pixel grid; a text area is at least a pixel high
    // and fits within the pitch
    PageFlow normalized;
    if (flow.enabled) {
        normalized.enabled = true;
        normalized.textHeight = std::max<qreal>(1.0, std::round(flow.textHeight));
        normalized.pitch = std::max(normalized.textHeight, std::round(flow.pitch));
    }
    if (normalized == m_pageFlow) {
        return;
    }
    // Line breaks change too (design metrics with page flow), so every block is laid out
    // again rather than only placed on new pages
    m_pageFlow = normalized;
    invalidateAll();
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
    invalidateAll();
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
    invalidateAll();
    layoutPendingBlocks();
}

// =============================================================================
// Layout on demand
// =============================================================================

bool KalahariTextDocumentLayout::isLaidOut(int blockNumber) const {
    return blockNumber >= 0 && static_cast<size_t>(blockNumber) < m_blockPending.size() &&
           m_blockPending[static_cast<size_t>(blockNumber)] == 0;
}

bool KalahariTextDocumentLayout::ensureLaidOut(int first, int last) {
    const int pendingBefore = m_pendingCount;
    if (pendingBefore > 0) {
        layOutPending(first, last, -1);
    }
    return m_pendingCount != pendingBefore;
}

void KalahariTextDocumentLayout::layoutPendingBlocks() {
    layOutPending(0, static_cast<int>(m_blockHeights.size()) - 1, -1);
}

void KalahariTextDocumentLayout::ensureLaidOutTo(qreal y) {
    // Laying blocks out changes their heights, and so which block is at y
    while (ensureLaidOut(0, blockNumberAtY(y))) {
    }
}

QTextLayout* KalahariTextDocumentLayout::blockLayout(const QTextBlock& block) {
    if (!block.isValid()) {
        return nullptr;
    }
    if (auto* layout =
            qobject_cast<KalahariTextDocumentLayout*>(block.document()->documentLayout())) {
        layout->ensureLaidOut(block.blockNumber(), block.blockNumber());
    }
    return block.layout();
}

int KalahariTextDocumentLayout::layOutPending(int first, int last, qint64 budgetMs) {
    first = std::max(first, 0);
    last = std::min(last, static_cast<int>(m_blockHeights.size()) - 1);
    if (m_pendingCount == 0 || first > last) {
        return last;
    }

    QElapsedTimer timer;
    timer.start();
    const qreal spacing = paragraphSpacing();
    const qreal indent = firstLineIndent();

    // Runs of consecutive blocks laid out, announced once the caches are consistent
    std::vector<std::pair<int, int>> runs;
    bool heightChanged = false;
    int number = first;
    QTextBlock block = document()->findBlockByNumber(first);
    for (; number <= last && block.isValid(); ++number, block = block.next()) {
        if (m_blockPending[static_cast<size_t>(number)] == 0) {
            continue;
        }
        heightChanged |= layOutBlock(number, block, spacing, indent);
        if (!runs.empty() && runs.back().first + runs.back().second == number) {
            ++runs.back().second;
        } else {
            runs.emplace_back(number, 1);
        }
        if (m_pendingCount == 0 || (budgetMs >= 0 && timer.elapsed() >= budgetMs)) {
            ++number;
            break;
        }
    }
    if (runs.empty()) {
        return number - 1;
    }

    // With page flow, the new lines are placed on the pages with the positions; whether
    // that moves the blocks below is known only then
    heightChanged |= m_pageFlow.enabled;
    if (heightChanged) {
        m_positionsDirty = true;
    }
    if (m_pendingCount == 0) {
        m_backgroundTimer->stop();
    }
    for (const auto& [runFirst, runCount] : runs) {
        emit blocksLaidOut(runFirst, runCount);
    }
    const int firstLaidOut = runs.front().first;
    const int lastLaidOut = runs.back().first + runs.back().second - 1;
    // Also for unchanged heights: the lines are new (a block's estimate can be exact while
    // its text wraps differently from before)
    emit blockGeometryChanged(firstLaidOut);
    notifyGeometryChanged(firstLaidOut,
                          heightChanged ? UNBOUNDED_EXTENT
                                        : blockY(lastLaidOut) + blockHeight(lastLaidOut));
    return number - 1;
}

void KalahariTextDocumentLayout::layoutStep() {
    const auto count = static_cast<int>(m_blockPending.size());
    if (m_pendingCount == 0 || count == 0) {
        m_backgroundTimer->stop();
        return;
    }

    // The next waiting block from where the last step stopped, wrapping around
    if (m_backgroundNext >= count) {
        m_backgroundNext = 0;
    }
    auto it = std::find(m_blockPending.begin() + m_backgroundNext, m_blockPending.end(), 1);
    if (it == m_blockPending.end()) {
        it = std::find(m_blockPending.begin(), m_blockPending.end(), 1);
    }
    if (it == m_blockPending.end()) {
        m_pendingCount = 0;  // nothing waits after all
        m_backgroundTimer->stop();
        return;
    }
    const auto first = static_cast<int>(std::distance(m_blockPending.begin(), it));
    m_backgroundNext = layOutPending(first, count - 1, BACKGROUND_STEP_MS) + 1;
}

void KalahariTextDocumentLayout::invalidateAll() {
    const QTextDocument* doc = document();
    updateEstimateMetrics();

    const auto count = static_cast<size_t>(doc->blockCount());
    if (m_blockHeights.size() != count || m_blockExtents.size() != count) {
        // Out of step with the document (first layout, or a change the cache missed):
        // the text length is the only extent known
        m_blockExtents.clear();
        m_blockExtents.reserve(count);
        for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
            m_blockExtents.push_back(block.length());
        }
        m_blockExtents.resize(count, 1);
        m_blockHeights.resize(count);
    }

    const qreal spacing = paragraphSpacing();
    for (size_t i = 0; i < count; ++i) {
        m_blockHeights[i] = estimatedHeight(m_blockExtents[i], spacing);
    }
    m_blockPending.assign(count, 1);
    m_blockFlowOffsets.assign(count, FLOW_UNPLACED);
    m_blockFlowHeights.assign(count, 0.0);
    m_pendingCount = static_cast<int>(count);
    m_positionsDirty = true;
    m_backgroundNext = 0;
    if (m_pendingCount > 0) {
        m_backgroundTimer->start();
    }
    emit blockGeometryChanged(0);
    notifyGeometryChanged(0, UNBOUNDED_EXTENT);
}

void KalahariTextDocumentLayout::updateEstimateMetrics() {
    const QFontMetricsF metrics(document()->defaultFont());
    const QString& sample = estimateSample();
    m_averageCharWidth =
        std::max<qreal>(1.0, metrics.horizontalAdvance(sample) / static_cast<qreal>(sample.size()));
    // As Qt sizes a line: ascent + descent, rounded up to whole pixels
    const qreal lineHeight = std::ceil(metrics.ascent() + metrics.descent());
    m_estimatedLineHeight = lineHeight + extraLeading(lineHeight, m_typography.lineSpacing);
}

qreal KalahariTextDocumentLayout::estimatedHeight(qreal extent, qreal spacing) const {
    const qreal width = document()->textWidth();
    const qreal lineWidth = (width > 0 ? width : UNWRAPPED_LINE_WIDTH) * AVERAGE_LINE_FILL;
    const qreal lines = std::max<qreal>(1.0, std::ceil(extent * m_averageCharWidth / lineWidth));
    return lines * m_estimatedLineHeight + spacing;
}

void KalahariTextDocumentLayout::notifyGeometryChanged(int first, qreal bottom) {
    const QSizeF size = documentSize();
    if (size != m_lastReportedSize) {
        m_lastReportedSize = size;
        emit documentSizeChanged(size);
    }

    const qreal top = blockY(first);
    emit update(QRectF(0, top, UNBOUNDED_EXTENT, bottom - top));
}

int KalahariTextDocumentLayout::laidOutBlockAtY(qreal y) const {
    // Laying out a block changes its height, and so which block covers y. The layout is
    // a cache of the document: filling it in leaves the query const for its callers.
    auto* self = const_cast<KalahariTextDocumentLayout*>(this);
    int number = blockNumberAtY(y);
    while (number >= 0 && self->ensureLaidOut(number, number)) {
        number = blockNumberAtY(y);
    }
    return number;
}

// =============================================================================
// Document Changed - Core Layout Logic
// =============================================================================

void KalahariTextDocumentLayout::documentChanged(int from, int charsRemoved, int charsAdded) {
    Q_UNUSED(charsRemoved);
    const QTextDocument* doc = document();

    // A new wrap width or default font (QTextDocument reports either as a change of the
    // whole document): every block waits for layout. The view lays out what it shows
    // before the next paint, the background pass the rest.
    if (doc->textWidth() != m_layoutWidth || doc->defaultFont() != m_layoutFont) {
        m_layoutWidth = doc->textWidth();
        m_layoutFont = doc->defaultFont();
        invalidateAll();
        return;
    }

    // Qt reports the change in new-document coordinates: [from, from + charsAdded) holds
    // everything inserted or reformatted. Every block touching that range is laid out
    // again, including the block holding the first character after it: it may have been
    // split off from, or merged with, the changed text. Blocks outside the range are
    // untouched.
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
    replaceRange(first.blockNumber(), lastNumber, oldLast);
}

void KalahariTextDocumentLayout::replaceRange(int first, int last, int oldLast) {
    const QTextDocument* doc = document();
    const int oldCount = static_cast<int>(m_blockHeights.size());

    if (oldLast < first - 1 || oldLast >= oldCount) {
        // The cache is out of step with the document: start it over
        invalidateAll();
        return;
    }

    // A small range is laid out now; a large one waits, with estimated heights
    const int rangeSize = std::max(last - first + 1, 0);
    const bool layOutNow = rangeSize <= IMMEDIATE_LAYOUT_LIMIT;
    const qreal spacing = paragraphSpacing();
    const qreal indent = firstLineIndent();
    std::vector<qreal> heights;
    std::vector<qreal> extents;
    heights.reserve(static_cast<size_t>(rangeSize));
    extents.reserve(static_cast<size_t>(rangeSize));
    QTextBlock block = doc->findBlockByNumber(first);
    for (int number = first; number <= last && block.isValid(); ++number) {
        qreal extent = block.length();
        heights.push_back(layOutNow ? layoutBlock(block, spacing, indent, &extent, true)
                                    : estimatedHeight(extent, spacing));
        extents.push_back(extent);
        block = block.next();
    }
    const std::vector<char> pending(heights.size(), layOutNow ? 0 : 1);
    const std::vector<qreal> unplaced(heights.size(), FLOW_UNPLACED);
    const std::vector<qreal> noFlowHeights(heights.size(), 0.0);

    // Replace the range's cache entries. Positions only move when a height changed or
    // blocks were added/removed - typing within a line leaves them valid.
    const auto begin = static_cast<std::ptrdiff_t>(first);
    // oldLast + 1: oldLast is -1 when the cache is empty or blocks were inserted at the
    // very start, and begin() - 1 is not a valid iterator
    const auto oldEnd = static_cast<std::ptrdiff_t>(oldLast) + 1;
    m_pendingCount -= static_cast<int>(
        std::count(m_blockPending.begin() + begin, m_blockPending.begin() + oldEnd, 1));
    m_pendingCount += layOutNow ? 0 : static_cast<int>(pending.size());
    bool geometryChanged = true;
    if (static_cast<std::ptrdiff_t>(heights.size()) == oldEnd - begin) {
        geometryChanged = !std::equal(heights.begin(), heights.end(), m_blockHeights.begin() + begin);
        std::copy(heights.begin(), heights.end(), m_blockHeights.begin() + begin);
        std::copy(extents.begin(), extents.end(), m_blockExtents.begin() + begin);
        std::copy(pending.begin(), pending.end(), m_blockPending.begin() + begin);
        std::copy(unplaced.begin(), unplaced.end(), m_blockFlowOffsets.begin() + begin);
        std::copy(noFlowHeights.begin(), noFlowHeights.end(), m_blockFlowHeights.begin() + begin);
    } else {
        const auto replace = [begin, oldEnd](auto& cache, const auto& entries) {
            cache.erase(cache.begin() + begin, cache.begin() + oldEnd);
            cache.insert(cache.begin() + begin, entries.begin(), entries.end());
        };
        replace(m_blockHeights, heights);
        replace(m_blockExtents, extents);
        replace(m_blockPending, pending);
        replace(m_blockFlowOffsets, unplaced);
        replace(m_blockFlowHeights, noFlowHeights);
    }
    // With page flow, the range's new lines are placed on the pages with the positions;
    // whether that moves the blocks below is known only then
    geometryChanged |= m_pageFlow.enabled;
    if (geometryChanged) {
        m_positionsDirty = true;
    }

    if (layOutNow && !heights.empty()) {
        emit blocksLaidOut(first, static_cast<int>(heights.size()));
    }
    if (m_pendingCount > 0) {
        m_backgroundTimer->start();
    } else {
        m_backgroundTimer->stop();
    }

    // Repaint the changed blocks, and everything below them if they moved it
    notifyGeometryChanged(first, (geometryChanged || heights.empty())
                                     ? UNBOUNDED_EXTENT
                                     : blockY(last) + blockHeight(last));
}

bool KalahariTextDocumentLayout::layOutBlock(int number, const QTextBlock& block, qreal spacing,
                                             qreal indent) {
    const auto index = static_cast<size_t>(number);
    qreal extent = block.length();
    const qreal height = layoutBlock(block, spacing, indent, &extent, false);
    const bool changed = height != m_blockHeights[index];
    m_blockHeights[index] = height;
    m_blockExtents[index] = extent;
    m_blockFlowOffsets[index] = FLOW_UNPLACED;  // the new lines follow the block's top
    if (m_blockPending[index] != 0) {
        m_blockPending[index] = 0;
        --m_pendingCount;
    }
    return changed;
}

qreal KalahariTextDocumentLayout::layoutBlock(const QTextBlock& block, qreal spacing,
                                              qreal indent, qreal* extent,
                                              bool keepGlyphs) const {
    QTextLayout* layout = block.layout();
    if (!layout) {
        return QFontMetricsF(document()->defaultFont()).height() + spacing;
    }

    // Glyph fonts come from the document's character formats (resolved against the
    // document's default font), so the QTextLayout's own font is irrelevant here.

    // A paragraph without its own alignment is justified
    const Qt::Alignment alignment = effectiveAlignment(ownAlignment(block.blockFormat()));

    QTextOption textOption;
    textOption.setAlignment(alignment);
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    // Pages are zoomed by a scaled painter: with the font's design metrics the glyph
    // advances scale linearly, so lines break the same way and letters stay evenly
    // spaced at every zoom. The continuous view lays out at the zoomed font size, where
    // the screen metrics (hinted to whole pixels) are the sharper choice.
    textOption.setUseDesignMetrics(m_pageFlow.enabled);
    layout->setTextOption(textOption);

    // Blocks laid out on demand keep only the line breaks: their glyphs are shaped again
    // when a line is drawn (only the visible ones are). Cached glyphs of a whole chapter
    // took most of the editor's memory. A block laid out after an edit keeps its glyphs:
    // the cursor works there, and each cursor position query would shape the whole
    // paragraph again. Justified lines come out the same either way (tested).
    layout->setCacheEnabled(keepGlyphs);

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
    qreal textLength = 0;
    for (QTextLine line = layout->createLine(); line.isValid(); line = layout->createLine()) {
        const qreal lineIndent = line.lineNumber() == 0 ? firstIndent : 0.0;
        line.setLineWidth(lineWidth - lineIndent);
        const qreal extra = extraLeading(line.height(), m_typography.lineSpacing);
        line.setPosition(QPointF(rightToLeft ? 0.0 : lineIndent, y + leadingAbove(extra)));
        y += line.height() + extra;
        textLength += line.naturalTextWidth();
    }
    layout->endLayout();

    if (extent) {
        *extent = textLength / m_averageCharWidth;
    }
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

    const size_t count = m_blockHeights.size();
    m_blockYPositions.resize(count);
    qreal y = 0;
    if (!m_pageFlow.enabled) {
        for (size_t i = 0; i < count; ++i) {
            m_blockYPositions[i] = y;
            y += m_blockHeights[i];
        }
        m_cachedDocumentHeight = y;
        m_contentBottom = y;
        m_positionsDirty = false;
        return;
    }

    // Page flow: blocks are placed in order, each where the one above ends. A laid out
    // block whose place on its page is the same as when its lines were placed keeps
    // them (a block moved by whole pages does too); the others are placed again.
    //
    // Queried in the middle of a document change (Qt asks for block rects then), the
    // cache is out of step with the blocks: positions then come from the cached heights,
    // without touching any lines, and are computed again on the next query.
    const bool inStep = document()->blockCount() == static_cast<int>(count);
    const qreal spacing = paragraphSpacing();
    qreal lastSpacing = 0;
    QTextBlock block = document()->begin();
    for (size_t i = 0; i < count; ++i, block = block.next()) {
        m_blockYPositions[i] = y;
        const qreal offset = std::fmod(y, m_pageFlow.pitch);
        qreal height = 0;
        if (m_blockPending[i] != 0) {
            height = flowEstimate(m_blockHeights[i], y, spacing);
        } else if (std::abs(m_blockFlowOffsets[i] - offset) < FLOW_EPSILON) {
            height = m_blockFlowHeights[i];
        } else if (!inStep || !block.isValid() || !block.layout()) {
            height = m_blockFlowOffsets[i] == FLOW_UNPLACED ? m_blockHeights[i]
                                                             : m_blockFlowHeights[i];
        } else {
            height = paginateBlock(block.layout(), y, spacing);
            m_blockFlowOffsets[i] = offset;
            m_blockFlowHeights[i] = height;
        }
        y += height;
        lastSpacing = spacing;
    }
    m_cachedDocumentHeight = y;
    m_contentBottom = y - lastSpacing;
    m_positionsDirty = !inStep;
}

qreal KalahariTextDocumentLayout::flowLineTop(qreal top, qreal height) const {
    const qreal pitch = m_pageFlow.pitch;
    const qreal areaHeight = m_pageFlow.textHeight;
    const qreal page = std::floor((top + FLOW_EPSILON) / pitch);
    const qreal offset = top - page * pitch;
    const bool betweenAreas = offset >= areaHeight - FLOW_EPSILON;
    // A line at the top of its area stays even when it is taller than the area: moving
    // it on would not make it fit
    const bool crossesEnd = offset > FLOW_EPSILON && offset + height > areaHeight + FLOW_EPSILON;
    return (betweenAreas || crossesEnd) ? (page + 1.0) * pitch : top;
}

qreal KalahariTextDocumentLayout::paginateBlock(QTextLayout* layout, qreal top,
                                                qreal spacing) const {
    // Line boxes are stacked as layoutBlock() stacks them; a page break moves a line and
    // all lines after it down to the next text area
    const qreal lineSpacing = m_typography.lineSpacing;
    qreal natural = 0;  // box top within the block without page breaks
    qreal shift = 0;    // how far page breaks moved the lines so far
    for (int i = 0; i < layout->lineCount(); ++i) {
        QTextLine line = layout->lineAt(i);
        const qreal extra = extraLeading(line.height(), lineSpacing);
        const qreal boxHeight = line.height() + extra;
        const qreal boxTop = top + natural + shift;
        shift += flowLineTop(boxTop, boxHeight) - boxTop;
        line.setPosition(QPointF(line.x(), natural + shift + leadingAbove(extra)));
        natural += boxHeight;
    }
    if (layout->lineCount() == 0) {
        // No lines (not laid out by this layout): the block keeps its default height
        natural = QFontMetricsF(document()->defaultFont()).height();
        shift = flowLineTop(top, natural) - top;
    }
    return natural + shift + spacing;
}

qreal KalahariTextDocumentLayout::flowEstimate(qreal height, qreal top, qreal spacing) const {
    // The estimate is a whole number of estimated lines; they are placed as lines are
    const qreal lineHeight = m_estimatedLineHeight;
    const int lines = std::max(1, static_cast<int>(std::lround((height - spacing) / lineHeight)));
    qreal y = top;
    for (int i = 0; i < lines; ++i) {
        y = flowLineTop(y, lineHeight) + lineHeight;
    }
    return y - top + spacing;
}

qreal KalahariTextDocumentLayout::flowDocumentHeight() const {
    // The page holding the bottom of the last line ends the document
    const qreal bottom = std::max<qreal>(0.0, m_contentBottom - FLOW_EPSILON);
    const qreal lastPage = std::floor(bottom / m_pageFlow.pitch);
    return lastPage * m_pageFlow.pitch + m_pageFlow.textHeight;
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
    const auto index = static_cast<size_t>(blockNumber);
    if (blockNumber < 0 || index >= m_blockHeights.size()) {
        return 0;
    }
    if (!m_pageFlow.enabled) {
        return m_blockHeights[index];
    }
    // Page flow: from the block's top to the next block's (page breaks included)
    updateBlockPositions();
    const qreal bottom =
        index + 1 < m_blockYPositions.size() ? m_blockYPositions[index + 1] : m_cachedDocumentHeight;
    return bottom - m_blockYPositions[index];
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

    int number = clip.isEmpty() ? 0 : laidOutBlockAtY(clip.top());
    for (QTextBlock block = document()->findBlockByNumber(number); block.isValid();
         block = block.next(), ++number) {
        ensureLaidOut(number, number);
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
    const int number = laidOutBlockAtY(y);
    const QTextBlock block = document()->findBlockByNumber(number);
    QTextLayout* layout = block.isValid() ? block.layout() : nullptr;
    if (!layout || layout->lineCount() == 0) {
        return exact ? -1 : qMax(0, document()->characterCount() - 1);
    }

    // Line whose box covers the point (or the nearest line, for a fuzzy hit)
    const qreal localY = y - blockY(number);
    const QTextLine line = layout->lineAt(lineIndexAt(*layout, localY, m_typography.lineSpacing));
    if (exact) {
        // An exact hit is on the text of a line: not in the spacing after the paragraph,
        // nor before or after the text of the line
        const QRectF box = lineBox(line, m_typography.lineSpacing);
        const qreal startX = line.cursorToX(line.textStart());
        const qreal endX = line.cursorToX(line.textStart() + line.textLength());
        if (localY < box.top() || localY >= box.bottom() || point.x() < qMin(startX, endX) ||
            point.x() > qMax(startX, endX)) {
            return -1;
        }
    }
    const int pos = line.xToCursor(point.x(), exact ? QTextLine::CursorOnCharacter
                                                    : QTextLine::CursorBetweenCharacters);
    return block.position() + pos;
}

int KalahariTextDocumentLayout::pageCount() const {
    if (!m_pageFlow.enabled) {
        return 1;  // Continuous layout, single page
    }
    updateBlockPositions();
    return static_cast<int>(std::lround((flowDocumentHeight() - m_pageFlow.textHeight) /
                                        m_pageFlow.pitch)) + 1;
}

QSizeF KalahariTextDocumentLayout::documentSize() const {
    updateBlockPositions();
    return QSizeF(documentWidth(),
                  m_pageFlow.enabled ? flowDocumentHeight() : m_cachedDocumentHeight);
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
        ? blockHeight(number)
        : QFontMetricsF(document()->defaultFont()).height();
    return QRectF(0, blockY(number), documentWidth(), height);
}

}  // namespace kalahari::editor
