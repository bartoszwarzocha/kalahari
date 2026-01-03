/// @file viewport_manager.cpp
/// @brief ViewportManager implementation (OpenSpec #00043 Phase 11.8)

#include <kalahari/editor/viewport_manager.h>
#include <QTextLayout>
#include <QAbstractTextDocumentLayout>
#include <algorithm>
#include <cmath>

namespace kalahari::editor {

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
    }

    m_document = doc;

    if (m_document) {
        connect(m_document, &QTextDocument::contentsChanged,
                this, &ViewportManager::onDocumentChanged);
        m_totalHeightDirty = true;
        updateVisibleRange();
    }
}

void ViewportManager::onDocumentChanged() {
    m_totalHeightDirty = true;
    updateVisibleRange();
    emit documentHeightChanged(totalDocumentHeight());
    emit viewportChanged();
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

// =============================================================================
// Scroll Position
// =============================================================================

void ViewportManager::setScrollPosition(double y) {
    double clamped = clampScrollPosition(y);
    if (std::abs(m_scrollY - clamped) > 0.001) {
        m_scrollY = clamped;
        updateVisibleRange();
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

    if (totalHeight <= viewHeight) {
        return 0.0;
    }

    return totalHeight - viewHeight;
}

double ViewportManager::clampScrollPosition(double y) const {
    if (y < 0.0) return 0.0;

    double maxY = maxScrollPosition();
    if (y > maxY) return maxY;

    return y;
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

    size_t oldFirst = m_firstVisible;
    size_t oldLast = m_lastVisible;

    double viewTop = m_scrollY;
    double viewBottom = m_scrollY + static_cast<double>(m_viewportSize.height());

    // Iterate blocks to find visible range
    m_firstVisible = 0;
    m_lastVisible = 0;
    bool foundFirst = false;

    // Use cumulative line heights (consistent with paragraphY and blockHeight)
    QTextBlock block = m_document->begin();
    size_t blockIndex = 0;
    double cumulativeY = 0.0;

    while (block.isValid()) {
        double height = this->blockHeight(block);

        double blockTop = cumulativeY;
        double blockBottom = cumulativeY + height;

        // Check if this block intersects viewport
        if (blockBottom > viewTop && !foundFirst) {
            m_firstVisible = blockIndex;
            foundFirst = true;
        }

        // Update last visible as long as block starts within viewport
        if (blockTop <= viewBottom) {
            m_lastVisible = blockIndex;
        } else {
            // We've passed the viewport, can stop early
            break;
        }

        cumulativeY += height;
        block = block.next();
        ++blockIndex;
    }

    // If no blocks found visible, set to 0
    if (!foundFirst) {
        m_firstVisible = 0;
        m_lastVisible = 0;
    }

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

    if (m_totalHeightDirty) {
        // Calculate from block layouts (NOT documentSize which may include margins)
        // This ensures consistency with paragraphY() and RenderEngine
        m_cachedTotalHeight = 0.0;
        QTextBlock block = m_document->begin();
        while (block.isValid()) {
            m_cachedTotalHeight += blockHeight(block);
            block = block.next();
        }
        m_totalHeightDirty = false;
    }

    return m_cachedTotalHeight;
}

size_t ViewportManager::paragraphAtY(double y) const {
    if (!m_document) return 0;

    // Iterate blocks using cumulative layout heights (consistent with paragraphY())
    // NOT using blockBoundingRect or hitTest which use document coordinate system with margins
    QTextBlock block = m_document->begin();
    size_t blockIndex = 0;
    double cumulativeY = 0.0;

    while (block.isValid()) {
        double height = blockHeight(block);
        double blockTop = cumulativeY;
        double blockBottom = cumulativeY + height;

        if (y >= blockTop && y < blockBottom) {
            return blockIndex;
        }

        cumulativeY += height;
        block = block.next();
        ++blockIndex;
    }

    // If y is beyond document, return last block
    int count = m_document->blockCount();
    return count > 0 ? static_cast<size_t>(count - 1) : 0;
}

double ViewportManager::paragraphY(size_t index) const {
    if (!m_document) return 0.0;

    // Calculate cumulative Y using blockHeight() for consistency
    // We manually position blocks when rendering (not using Qt's document layout)
    // so we need our own continuous positioning without Qt's extra margins/spacing
    double cumulativeY = 0.0;
    QTextBlock block = m_document->begin();
    size_t blockIndex = 0;

    while (block.isValid() && blockIndex < index) {
        cumulativeY += blockHeight(block);
        block = block.next();
        ++blockIndex;
    }

    return cumulativeY;
}

double ViewportManager::paragraphHeight(size_t index) const {
    if (!m_document) return 0.0;

    QTextBlock block = m_document->findBlockByNumber(static_cast<int>(index));
    return blockHeight(block);
}

double ViewportManager::blockHeight(const QTextBlock& block) const {
    if (!block.isValid()) return m_estimatedLineHeight;

    // Use boundingRect().height() - same as KmlDocumentModel (view mode)
    // This gives just the text height without double-counting leading
    if (auto* layout = block.layout()) {
        double height = layout->boundingRect().height();
        if (height > 0) {
            return height;
        }
    }

    // Fallback: use blockBoundingRect (layout not prepared yet)
    if (m_document) {
        if (auto* docLayout = m_document->documentLayout()) {
            double h = docLayout->blockBoundingRect(block).height();
            if (h > 0) return h;
        }
    }

    return m_estimatedLineHeight;
}

}  // namespace kalahari::editor
