/// @file viewport_manager.cpp
/// @brief ViewportManager implementation (OpenSpec #00043 Phase 4)

#include <kalahari/editor/viewport_manager.h>
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
    if (m_buffer) {
        m_buffer->removeObserver(this);
    }
}

// =============================================================================
// Component Integration
// =============================================================================

void ViewportManager::setBuffer(TextBuffer* buffer) {
    if (m_buffer) {
        m_buffer->removeObserver(this);
    }

    m_buffer = buffer;

    if (m_buffer) {
        m_buffer->addObserver(this);
        m_totalHeightDirty = true;
        updateVisibleRange();
    }
}

void ViewportManager::setLayoutManager(LazyLayoutManager* manager) {
    m_layoutManager = manager;
    syncLayoutManagerViewport();
}

// =============================================================================
// Viewport Configuration
// =============================================================================

void ViewportManager::setViewportSize(const QSize& size) {
    if (m_viewportSize != size) {
        m_viewportSize = size;
        updateVisibleRange();
        syncLayoutManagerViewport();
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
        syncLayoutManagerViewport();
        emit scrollPositionChanged(m_scrollY);
        emit viewportChanged();
    }
}

void ViewportManager::scrollBy(double delta) {
    setScrollPosition(m_scrollY + delta);
}

double ViewportManager::scrollToMakeParagraphVisible(size_t index) {
    if (!m_buffer || index >= m_buffer->paragraphCount()) {
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
    if (!m_buffer) return 0;

    size_t count = m_buffer->paragraphCount();
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
    if (!m_buffer || m_buffer->paragraphCount() == 0) {
        m_firstVisible = 0;
        m_lastVisible = 0;
        return;
    }

    size_t oldFirst = m_firstVisible;
    size_t oldLast = m_lastVisible;

    // Find first visible paragraph using binary search
    m_firstVisible = m_buffer->getParagraphAtY(m_scrollY);

    // Find last visible paragraph
    double viewBottom = m_scrollY + static_cast<double>(m_viewportSize.height());
    m_lastVisible = m_buffer->getParagraphAtY(viewBottom);

    // Clamp to valid range
    size_t count = m_buffer->paragraphCount();
    if (m_lastVisible >= count) {
        m_lastVisible = count > 0 ? count - 1 : 0;
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
    if (!m_buffer) return 0.0;

    if (m_totalHeightDirty) {
        m_cachedTotalHeight = m_buffer->totalHeight();
        m_totalHeightDirty = false;
    }

    return m_cachedTotalHeight;
}

size_t ViewportManager::paragraphAtY(double y) const {
    if (!m_buffer) return 0;
    return m_buffer->getParagraphAtY(y);
}

double ViewportManager::paragraphY(size_t index) const {
    if (!m_buffer) return 0.0;
    return m_buffer->getParagraphY(index);
}

double ViewportManager::paragraphHeight(size_t index) const {
    if (!m_buffer) return 0.0;
    return m_buffer->getParagraphHeight(index);
}

// =============================================================================
// Layout Coordination
// =============================================================================

void ViewportManager::requestLayout() {
    if (!m_layoutManager) return;

    syncLayoutManagerViewport();
    m_layoutManager->layoutVisibleParagraphs();
}

void ViewportManager::syncLayoutManagerViewport() {
    if (!m_layoutManager) return;

    m_layoutManager->setViewport(m_scrollY, static_cast<double>(m_viewportSize.height()));
}

// =============================================================================
// ITextBufferObserver Implementation
// =============================================================================

void ViewportManager::onTextChanged() {
    m_totalHeightDirty = true;
    updateVisibleRange();
    emit documentHeightChanged(totalDocumentHeight());
    emit viewportChanged();
}

void ViewportManager::onParagraphInserted(size_t /*index*/) {
    m_totalHeightDirty = true;
    updateVisibleRange();
    emit documentHeightChanged(totalDocumentHeight());
}

void ViewportManager::onParagraphRemoved(size_t /*index*/) {
    m_totalHeightDirty = true;
    updateVisibleRange();
    emit documentHeightChanged(totalDocumentHeight());
}

void ViewportManager::onParagraphChanged(size_t /*index*/) {
    m_totalHeightDirty = true;
    updateVisibleRange();
}

void ViewportManager::onHeightChanged(size_t /*index*/, double /*oldHeight*/, double /*newHeight*/) {
    m_totalHeightDirty = true;
    updateVisibleRange();
    emit documentHeightChanged(totalDocumentHeight());
}

}  // namespace kalahari::editor
