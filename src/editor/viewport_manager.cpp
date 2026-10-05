/// @file viewport_manager.cpp
/// @brief ViewportManager implementation (OpenSpec #00043 Phase 11.8)

#include <kalahari/editor/viewport_manager.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <QAbstractTextDocumentLayout>
#include <QTextLayout>
#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

/// The editor's own layout, which answers position queries from cached block positions
const KalahariTextDocumentLayout* kalahariLayout(const QTextDocument* doc) {
    return qobject_cast<const KalahariTextDocumentLayout*>(doc->documentLayout());
}

}  // anonymous namespace

// =============================================================================
// Constructor / Destructor
// =============================================================================

ViewportManager::ViewportManager(QObject* parent)
    : QObject(parent) {
}

ViewportManager::~ViewportManager() {
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
    }
}

// =============================================================================
// Component Integration
// =============================================================================

void ViewportManager::setDocument(QTextDocument* doc) {
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
        disconnect(m_document->documentLayout(), nullptr, this, nullptr);
    }

    m_document = doc;
    m_anchor = ScrollAnchor{};

    if (m_document) {
        connect(m_document, &QTextDocument::contentsChanged,
                this, &ViewportManager::onDocumentChanged);
        // Re-wrapping after a width or font change alters block heights without a
        // content change, so contentsChanged is not emitted for it.
        connect(m_document->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
                this, &ViewportManager::onDocumentSizeChanged);
        if (const auto* layout = kalahariLayout(m_document)) {
            connect(layout, &KalahariTextDocumentLayout::blockGeometryChanged,
                    this, &ViewportManager::onBlockGeometryChanged);
        }
        updateVisibleRange();
        captureAnchor();
    }
}

void ViewportManager::onDocumentChanged() {
    updateVisibleRange();
    // The content moved under the viewport: the text now at its top is the anchor
    captureAnchor();
    emit documentHeightChanged(totalDocumentHeight());
    emit viewportChanged();
}

void ViewportManager::onDocumentSizeChanged() {
    // No repaint here: a content change repaints through onDocumentChanged(), a new
    // width or font through whoever set it, and blocks laid out on demand leave the
    // visible text where it is (scroll anchoring)
    updateVisibleRange();
    emit documentHeightChanged(totalDocumentHeight());
}

void ViewportManager::onBlockGeometryChanged(int firstBlock) {
    // Blocks at or above the anchor changed height or were wrapped again: scroll so that
    // the anchored text is where it was. Blocks below it do not move it.
    const bool anchorMoved = m_anchoringEnabled && m_anchor.block >= firstBlock &&
                             m_anchor.block < m_document->blockCount();
    const double oldScrollY = m_scrollY;
    if (anchorMoved) {
        m_scrollY = clampScrollPosition(anchoredPosition());
    }
    updateVisibleRange();
    if (std::abs(m_scrollY - oldScrollY) > 0.001) {
        emit scrollPositionAnchored(m_scrollY);
    }
}

// =============================================================================
// Viewport Configuration
// =============================================================================

void ViewportManager::setViewportSize(const QSize& size) {
    if (m_viewportSize != size) {
        m_viewportSize = size;
        updateVisibleRange();
        emit viewportChanged();
    }
}

void ViewportManager::setBufferSize(size_t paragraphs) {
    m_bufferSize = paragraphs;
}

void ViewportManager::setTopScrollPadding(double padding) {
    if (std::abs(m_topScrollPadding - padding) > 0.001) {
        m_topScrollPadding = std::max(0.0, padding);
        emit viewportChanged();
    }
}

void ViewportManager::setBottomScrollPadding(double padding) {
    if (std::abs(m_bottomScrollPadding - padding) > 0.001) {
        m_bottomScrollPadding = std::max(0.0, padding);
        emit viewportChanged();
    }
}

// =============================================================================
// Scroll Position
// =============================================================================

void ViewportManager::setScrollPosition(double y) {
    double clamped = clampScrollPosition(y);
    if (std::abs(m_scrollY - clamped) > 0.001) {
        m_scrollY = clamped;
        updateVisibleRange();
        captureAnchor();
        emit scrollPositionChanged(m_scrollY);
        emit viewportChanged();
    }
}

void ViewportManager::scrollBy(double delta) {
    setScrollPosition(m_scrollY + delta);
}

double ViewportManager::scrollToMakeParagraphVisible(size_t index) {
    if (!m_document || static_cast<int>(index) >= m_document->blockCount()) {
        return m_scrollY;
    }

    double paraY = paragraphY(index);
    double paraHeight = paragraphHeight(index);
    double viewHeight = static_cast<double>(m_viewportSize.height());

    // Already visible?
    if (paraY >= m_scrollY && paraY + paraHeight <= m_scrollY + viewHeight) {
        return m_scrollY;
    }

    // Need to scroll
    double newScrollY;
    if (paraY < m_scrollY) {
        // Paragraph is above viewport - scroll up
        newScrollY = paraY;
    } else {
        // Paragraph is below viewport - scroll down
        newScrollY = paraY + paraHeight - viewHeight;
    }

    setScrollPosition(newScrollY);
    return m_scrollY;
}

double ViewportManager::maxScrollPosition() const {
    double totalHeight = totalDocumentHeight();
    double viewHeight = static_cast<double>(m_viewportSize.height());

    // Include both top and bottom padding so user can see margins
    // Top padding: allows content to be scrolled up to show top margin
    // Bottom padding: allows scrolling past content to show bottom margin
    double scrollableHeight = totalHeight + m_topScrollPadding + m_bottomScrollPadding;

    if (scrollableHeight <= viewHeight) {
        return 0.0;
    }

    return scrollableHeight - viewHeight;
}

double ViewportManager::clampScrollPosition(double y) const {
    if (y < 0.0) return 0.0;

    double maxY = maxScrollPosition();
    if (y > maxY) return maxY;

    return y;
}

// =============================================================================
// Scroll Anchoring
// =============================================================================

void ViewportManager::setScrollAnchoringEnabled(bool enabled) {
    m_anchoringEnabled = enabled;
    captureAnchor();
}

void ViewportManager::captureAnchor() {
    m_anchor = ScrollAnchor{};
    const KalahariTextDocumentLayout* layout = m_document ? kalahariLayout(m_document) : nullptr;
    const int block = layout ? layout->blockNumberAtY(m_scrollY) : -1;
    if (block < 0) {
        return;
    }
    m_anchor.block = block;
    m_anchor.offset = m_scrollY - layout->blockY(block);
    m_anchor.height = layout->blockHeight(block);

    // A laid out block also gives the line at the top, which stays the anchor when the
    // block is wrapped again
    if (layout->isLaidOut(block)) {
        const QTextLayout* lines = m_document->findBlockByNumber(block).layout();
        const qreal lineSpacing = layout->typography().lineSpacing;
        const int index = KalahariTextDocumentLayout::lineIndexAt(*lines, m_anchor.offset, lineSpacing);
        if (index >= 0) {
            const QTextLine line = lines->lineAt(index);
            const QRectF box = KalahariTextDocumentLayout::lineBox(line, lineSpacing);
            m_anchor.lineStart = line.textStart();
            m_anchor.lineShare = box.height() > 0.0 ? (m_anchor.offset - box.top()) / box.height() : 0.0;
        }
    }
}

double ViewportManager::anchoredPosition() const {
    const KalahariTextDocumentLayout* layout = kalahariLayout(m_document);
    const int block = m_anchor.block;
    const double top = layout->blockY(block);
    const double height = layout->blockHeight(block);

    // The block has its lines: the line holding the text that was at the top, also when
    // the block was wrapped again at a new width or font size
    if (m_anchor.lineStart >= 0 && layout->isLaidOut(block)) {
        const QTextLayout* lines = m_document->findBlockByNumber(block).layout();
        const QTextLine line = lines->lineForTextPosition(m_anchor.lineStart);
        if (line.isValid()) {
            const QRectF box =
                KalahariTextDocumentLayout::lineBox(line, layout->typography().lineSpacing);
            return top + box.top() + m_anchor.lineShare * box.height();
        }
    }
    // The block kept its height: the same offset into it
    if (height == m_anchor.height) {
        return top + m_anchor.offset;
    }
    // Estimated heights: the same share of the block
    return top + (m_anchor.height > 0.0 ? m_anchor.offset * height / m_anchor.height : 0.0);
}

// =============================================================================
// Visible Range
// =============================================================================

size_t ViewportManager::bufferStart() const {
    if (m_firstVisible < m_bufferSize) {
        return 0;
    }
    return m_firstVisible - m_bufferSize;
}

size_t ViewportManager::bufferEnd() const {
    if (!m_document) return 0;

    size_t count = static_cast<size_t>(m_document->blockCount());
    size_t end = m_lastVisible + m_bufferSize;

    return std::min(end, count > 0 ? count - 1 : 0);
}

bool ViewportManager::isParagraphVisible(size_t index) const {
    return index >= m_firstVisible && index <= m_lastVisible;
}

bool ViewportManager::isParagraphInBuffer(size_t index) const {
    return index >= bufferStart() && index <= bufferEnd();
}

void ViewportManager::updateVisibleRange() {
    if (!m_document || m_document->blockCount() == 0) {
        m_firstVisible = 0;
        m_lastVisible = 0;
        return;
    }

    const size_t oldFirst = m_firstVisible;
    const size_t oldLast = m_lastVisible;

    // First block reaching into the viewport, last block starting above its bottom edge
    const double viewTop = m_scrollY;
    const double viewBottom = m_scrollY + static_cast<double>(m_viewportSize.height());
    m_firstVisible = paragraphAtY(viewTop);
    m_lastVisible = std::max(m_firstVisible, paragraphAtY(viewBottom));

    // Notify if range changed
    if (oldFirst != m_firstVisible || oldLast != m_lastVisible) {
        notifyRangeChanged();
    }
}

void ViewportManager::notifyRangeChanged() {
    emit visibleRangeChanged(m_firstVisible, m_lastVisible);
    emit layoutRequested(bufferStart(), bufferEnd());
}

// =============================================================================
// Scrollbar
// =============================================================================

double ViewportManager::scrollbarPosition() const {
    double maxY = maxScrollPosition();
    if (maxY <= 0.0) return 0.0;
    return m_scrollY / maxY;
}

double ViewportManager::scrollbarThumbSize() const {
    double totalHeight = totalDocumentHeight();
    if (totalHeight <= 0.0) return 1.0;

    double viewHeight = static_cast<double>(m_viewportSize.height());
    double thumbSize = viewHeight / totalHeight;

    return std::min(1.0, std::max(0.05, thumbSize));  // At least 5% visible
}

void ViewportManager::setScrollbarPosition(double position) {
    position = std::clamp(position, 0.0, 1.0);
    double maxY = maxScrollPosition();
    setScrollPosition(position * maxY);
}

bool ViewportManager::isScrollbarNeeded() const {
    return totalDocumentHeight() > static_cast<double>(m_viewportSize.height());
}

// =============================================================================
// Geometry Queries
// =============================================================================

QRectF ViewportManager::viewportRect() const {
    return QRectF(0.0, m_scrollY,
                  static_cast<double>(m_viewportSize.width()),
                  static_cast<double>(m_viewportSize.height()));
}

double ViewportManager::totalDocumentHeight() const {
    if (!m_document) return 0.0;
    return m_document->documentLayout()->documentSize().height();
}

size_t ViewportManager::paragraphAtY(double y) const {
    if (!m_document) return 0;

    // The editor's layout keeps cumulative block positions: binary search there
    if (const auto* layout = kalahariLayout(m_document)) {
        return static_cast<size_t>(std::max(0, layout->blockNumberAtY(y)));
    }

    // Any other layout: blocks are stacked in order, so binary search on their tops
    const QAbstractTextDocumentLayout* docLayout = m_document->documentLayout();
    int low = 0;
    int high = std::max(0, m_document->blockCount() - 1);
    while (low < high) {
        const int middle = low + (high - low + 1) / 2;
        if (docLayout->blockBoundingRect(m_document->findBlockByNumber(middle)).y() <= y) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    return static_cast<size_t>(low);
}

double ViewportManager::paragraphY(size_t index) const {
    if (!m_document) return 0.0;

    const QTextBlock block = m_document->findBlockByNumber(static_cast<int>(index));
    if (!block.isValid()) {
        return totalDocumentHeight();  // Past the last paragraph
    }
    return m_document->documentLayout()->blockBoundingRect(block).y();
}

double ViewportManager::paragraphHeight(size_t index) const {
    if (!m_document) return 0.0;

    const QTextBlock block = m_document->findBlockByNumber(static_cast<int>(index));
    if (!block.isValid()) return 0.0;
    return m_document->documentLayout()->blockBoundingRect(block).height();
}

}  // namespace kalahari::editor
