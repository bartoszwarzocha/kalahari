/// @file virtual_scroll_manager.cpp
/// @brief Virtual scroll manager implementation (OpenSpec #00042 Phase 2.8-2.9)

#include <kalahari/editor/virtual_scroll_manager.h>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// Constructor / Destructor
// =============================================================================

VirtualScrollManager::VirtualScrollManager()
    : m_document(nullptr)
    , m_viewportTop(0.0)
    , m_viewportHeight(0.0)
    , m_bufferParagraphs(BUFFER_PARAGRAPHS)
    , m_cachedTotalHeight(0.0)
    , m_totalHeightValid(false)
{
}

VirtualScrollManager::~VirtualScrollManager() = default;

VirtualScrollManager::VirtualScrollManager(const VirtualScrollManager& other)
    : m_document(other.m_document)
    , m_viewportTop(other.m_viewportTop)
    , m_viewportHeight(other.m_viewportHeight)
    , m_bufferParagraphs(other.m_bufferParagraphs)
    , m_paragraphInfo(other.m_paragraphInfo)
    , m_cachedTotalHeight(other.m_cachedTotalHeight)
    , m_totalHeightValid(other.m_totalHeightValid)
{
}

VirtualScrollManager::VirtualScrollManager(VirtualScrollManager&& other) noexcept
    : m_document(other.m_document)
    , m_viewportTop(other.m_viewportTop)
    , m_viewportHeight(other.m_viewportHeight)
    , m_bufferParagraphs(other.m_bufferParagraphs)
    , m_paragraphInfo(std::move(other.m_paragraphInfo))
    , m_cachedTotalHeight(other.m_cachedTotalHeight)
    , m_totalHeightValid(other.m_totalHeightValid)
{
    other.m_document = nullptr;
    other.m_viewportTop = 0.0;
    other.m_viewportHeight = 0.0;
    other.m_bufferParagraphs = BUFFER_PARAGRAPHS;
    other.m_cachedTotalHeight = 0.0;
    other.m_totalHeightValid = false;
}

VirtualScrollManager& VirtualScrollManager::operator=(const VirtualScrollManager& other) {
    if (this != &other) {
        m_document = other.m_document;
        m_viewportTop = other.m_viewportTop;
        m_viewportHeight = other.m_viewportHeight;
        m_bufferParagraphs = other.m_bufferParagraphs;
        m_paragraphInfo = other.m_paragraphInfo;
        m_cachedTotalHeight = other.m_cachedTotalHeight;
        m_totalHeightValid = other.m_totalHeightValid;
    }
    return *this;
}

VirtualScrollManager& VirtualScrollManager::operator=(VirtualScrollManager&& other) noexcept {
    if (this != &other) {
        m_document = other.m_document;
        m_viewportTop = other.m_viewportTop;
        m_viewportHeight = other.m_viewportHeight;
        m_bufferParagraphs = other.m_bufferParagraphs;
        m_paragraphInfo = std::move(other.m_paragraphInfo);
        m_cachedTotalHeight = other.m_cachedTotalHeight;
        m_totalHeightValid = other.m_totalHeightValid;

        other.m_document = nullptr;
        other.m_viewportTop = 0.0;
        other.m_viewportHeight = 0.0;
        other.m_bufferParagraphs = BUFFER_PARAGRAPHS;
        other.m_cachedTotalHeight = 0.0;
        other.m_totalHeightValid = false;
    }
    return *this;
}

// =============================================================================
// Document Management
// =============================================================================

void VirtualScrollManager::setDocument(KmlDocument* document) {
    m_document = document;
    // Reset paragraph info and cache when document changes
    m_paragraphInfo.clear();
    m_cachedTotalHeight = 0.0;
    m_totalHeightValid = false;
    syncParagraphInfo();
}

KmlDocument* VirtualScrollManager::document() const {
    return m_document;
}

// =============================================================================
// Viewport Management
// =============================================================================

void VirtualScrollManager::setViewport(qreal top, qreal height) {
    m_viewportTop = std::max(0.0, top);
    m_viewportHeight = std::max(0.0, height);
}

qreal VirtualScrollManager::viewportTop() const {
    return m_viewportTop;
}

qreal VirtualScrollManager::viewportHeight() const {
    return m_viewportHeight;
}

void VirtualScrollManager::setViewportTop(qreal top) {
    m_viewportTop = std::max(0.0, top);
}

void VirtualScrollManager::setViewportHeight(qreal height) {
    m_viewportHeight = std::max(0.0, height);
}

// =============================================================================
// Visible Range Calculation
// =============================================================================

QPair<int, int> VirtualScrollManager::visibleRange() const {
    // Return invalid range if no document or empty document
    if (!m_document || m_document->paragraphCount() == 0) {
        return {-1, -1};
    }

    // Get exact visible range first
    auto [exactFirst, exactLast] = exactVisibleRange();

    // If exact range is invalid, return invalid
    if (exactFirst < 0 || exactLast < 0) {
        return {-1, -1};
    }

    // Expand by buffer paragraphs
    int first = std::max(0, exactFirst - m_bufferParagraphs);
    int last = std::min(m_document->paragraphCount() - 1, exactLast + m_bufferParagraphs);

    return {first, last};
}

QPair<int, int> VirtualScrollManager::exactVisibleRange() const {
    // Return invalid range if no document or empty document
    if (!m_document || m_document->paragraphCount() == 0) {
        return {-1, -1};
    }

    // Return invalid range if viewport has no size
    if (m_viewportHeight <= 0) {
        return {-1, -1};
    }

    int first = calculateFirstVisibleParagraph();
    int last = calculateLastVisibleParagraph();

    // Validate range
    if (first < 0 || last < 0 || first > last) {
        return {-1, -1};
    }

    return {first, last};
}

bool VirtualScrollManager::isParagraphVisible(int paragraphIndex) const {
    auto [first, last] = visibleRange();
    if (first < 0 || last < 0) {
        return false;
    }
    return paragraphIndex >= first && paragraphIndex <= last;
}

bool VirtualScrollManager::isParagraphExactlyVisible(int paragraphIndex) const {
    auto [first, last] = exactVisibleRange();
    if (first < 0 || last < 0) {
        return false;
    }
    return paragraphIndex >= first && paragraphIndex <= last;
}

// =============================================================================
// Buffer Configuration
// =============================================================================

int VirtualScrollManager::bufferParagraphs() const {
    return m_bufferParagraphs;
}

void VirtualScrollManager::setBufferParagraphs(int count) {
    m_bufferParagraphs = std::max(0, count);
}

// =============================================================================
// Height Management (Phase 2.9)
// =============================================================================

void VirtualScrollManager::updateParagraphHeight(int index, qreal height) {
    syncParagraphInfo();

    if (index < 0 || index >= static_cast<int>(m_paragraphInfo.size())) {
        return;
    }

    // Clamp height to reasonable minimum
    height = std::max(1.0, height);

    // Check if height actually changed
    qreal oldHeight = m_paragraphInfo[index].height;
    if (qFuzzyCompare(oldHeight, height) && m_paragraphInfo[index].heightKnown) {
        return;  // No change needed
    }

    // Update the height and mark as known
    m_paragraphInfo[index].height = height;
    m_paragraphInfo[index].heightKnown = true;

    // Update cached total height incrementally
    if (m_totalHeightValid) {
        m_cachedTotalHeight += (height - oldHeight);
    }

    // Recalculate Y positions only for this and subsequent paragraphs (incremental update)
    recalculateYPositionsFrom(index + 1);
}

qreal VirtualScrollManager::totalHeight() const {
    syncParagraphInfo();

    if (m_paragraphInfo.empty()) {
        return 0.0;
    }

    // Use cached total height if valid
    if (m_totalHeightValid) {
        return m_cachedTotalHeight;
    }

    // Calculate and cache total height
    const auto& last = m_paragraphInfo.back();
    m_cachedTotalHeight = last.y + last.height;
    m_totalHeightValid = true;
    return m_cachedTotalHeight;
}

qreal VirtualScrollManager::paragraphY(int index) const {
    syncParagraphInfo();

    if (index < 0 || index >= static_cast<int>(m_paragraphInfo.size())) {
        return 0.0;
    }

    return m_paragraphInfo[index].y;
}

ParagraphInfo VirtualScrollManager::paragraphInfo(int index) const {
    syncParagraphInfo();

    if (index < 0 || index >= static_cast<int>(m_paragraphInfo.size())) {
        return ParagraphInfo();
    }

    return m_paragraphInfo[index];
}

bool VirtualScrollManager::isHeightKnown(int index) const {
    syncParagraphInfo();

    if (index < 0 || index >= static_cast<int>(m_paragraphInfo.size())) {
        return false;
    }

    return m_paragraphInfo[index].heightKnown;
}

int VirtualScrollManager::knownHeightCount() const {
    syncParagraphInfo();

    int count = 0;
    for (const auto& info : m_paragraphInfo) {
        if (info.heightKnown) {
            ++count;
        }
    }
    return count;
}

void VirtualScrollManager::resetHeights() {
    for (auto& info : m_paragraphInfo) {
        info.height = ESTIMATED_LINE_HEIGHT;
        info.heightKnown = false;
    }
    m_totalHeightValid = false;
    recalculateYPositions();
}

// =============================================================================
// Private Methods
// =============================================================================

int VirtualScrollManager::calculateFirstVisibleParagraph() const {
    if (!m_document || m_document->paragraphCount() == 0) {
        return -1;
    }

    syncParagraphInfo();

    // Binary search for the first paragraph that ends after viewport top
    // A paragraph is visible if its bottom edge (y + height) is > viewportTop
    int left = 0;
    int right = static_cast<int>(m_paragraphInfo.size()) - 1;
    int result = 0;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        qreal paragraphBottom = m_paragraphInfo[mid].y + m_paragraphInfo[mid].height;

        if (paragraphBottom <= m_viewportTop) {
            // This paragraph ends before viewport, look later
            left = mid + 1;
        } else {
            // This paragraph might be visible, but there might be earlier ones
            result = mid;
            right = mid - 1;
        }
    }

    return std::clamp(result, 0, m_document->paragraphCount() - 1);
}

int VirtualScrollManager::calculateLastVisibleParagraph() const {
    if (!m_document || m_document->paragraphCount() == 0) {
        return -1;
    }

    syncParagraphInfo();

    qreal viewportBottom = m_viewportTop + m_viewportHeight;

    // Binary search for the last paragraph that starts before viewport bottom
    // A paragraph is visible if its top edge (y) is < viewportBottom
    int left = 0;
    int right = static_cast<int>(m_paragraphInfo.size()) - 1;
    int result = right;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        qreal paragraphTop = m_paragraphInfo[mid].y;

        if (paragraphTop >= viewportBottom) {
            // This paragraph starts after viewport, look earlier
            right = mid - 1;
        } else {
            // This paragraph might be visible, but there might be later ones
            result = mid;
            left = mid + 1;
        }
    }

    return std::clamp(result, 0, m_document->paragraphCount() - 1);
}

void VirtualScrollManager::syncParagraphInfo() const {
    if (!m_document) {
        m_paragraphInfo.clear();
        m_totalHeightValid = false;
        return;
    }

    int docCount = m_document->paragraphCount();
    int currentSize = static_cast<int>(m_paragraphInfo.size());

    if (currentSize == docCount) {
        return;  // Already synced
    }

    // Invalidate cached total height when paragraph count changes
    m_totalHeightValid = false;

    if (currentSize < docCount) {
        // Need to add paragraphs
        m_paragraphInfo.reserve(docCount);
        for (int i = currentSize; i < docCount; ++i) {
            qreal y = 0.0;
            if (i > 0) {
                y = m_paragraphInfo[i - 1].y + m_paragraphInfo[i - 1].height;
            }
            m_paragraphInfo.emplace_back(y, ESTIMATED_LINE_HEIGHT, false);
        }
    } else {
        // Need to remove paragraphs
        m_paragraphInfo.resize(docCount);
    }
}

void VirtualScrollManager::recalculateYPositions() {
    qreal y = 0.0;
    for (auto& info : m_paragraphInfo) {
        info.y = y;
        y += info.height;
    }
    // Invalidate cached total height - will be recalculated on next access
    m_totalHeightValid = false;
}

void VirtualScrollManager::recalculateYPositionsFrom(int fromIndex) {
    if (m_paragraphInfo.empty() || fromIndex <= 0) {
        // If starting from beginning, just do full recalculation
        if (fromIndex <= 0) {
            recalculateYPositions();
        }
        return;
    }

    // Clamp to valid range
    fromIndex = std::min(fromIndex, static_cast<int>(m_paragraphInfo.size()));

    // Start from the previous paragraph's end position
    qreal y = m_paragraphInfo[fromIndex - 1].y + m_paragraphInfo[fromIndex - 1].height;

    // Update only from fromIndex onward
    for (int i = fromIndex; i < static_cast<int>(m_paragraphInfo.size()); ++i) {
        m_paragraphInfo[i].y = y;
        y += m_paragraphInfo[i].height;
    }
    // Note: total height cache is updated incrementally in updateParagraphHeight
}

// =============================================================================
// Scrolling Support (Phase 2.10)
// =============================================================================

qreal VirtualScrollManager::scrollOffset() const {
    return m_viewportTop;
}

void VirtualScrollManager::setScrollOffset(qreal offset) {
    // Clamp to valid range
    qreal maxOffset = maxScrollOffset();
    offset = std::clamp(offset, 0.0, maxOffset);
    m_viewportTop = offset;
}

int VirtualScrollManager::paragraphAtY(qreal y) const {
    // Handle edge cases
    if (!m_document || m_document->paragraphCount() == 0) {
        return -1;
    }

    syncParagraphInfo();

    // Handle negative Y - return first paragraph
    if (y < 0.0) {
        return 0;
    }

    // Handle Y past end of document - return last paragraph
    qreal total = totalHeight();
    if (y >= total) {
        return m_document->paragraphCount() - 1;
    }

    // Binary search for paragraph containing Y
    int left = 0;
    int right = static_cast<int>(m_paragraphInfo.size()) - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        qreal paragraphTop = m_paragraphInfo[mid].y;
        qreal paragraphBottom = paragraphTop + m_paragraphInfo[mid].height;

        if (y < paragraphTop) {
            // Y is before this paragraph
            right = mid - 1;
        } else if (y >= paragraphBottom) {
            // Y is after this paragraph
            left = mid + 1;
        } else {
            // Y is within this paragraph
            return mid;
        }
    }

    // Fallback: return clamped left (should not normally reach here)
    return std::clamp(left, 0, m_document->paragraphCount() - 1);
}

qreal VirtualScrollManager::ensureParagraphVisible(int index) {
    // Handle edge cases
    if (!m_document || m_document->paragraphCount() == 0) {
        return m_viewportTop;
    }

    // Clamp index to valid range
    index = std::clamp(index, 0, m_document->paragraphCount() - 1);

    syncParagraphInfo();

    // Get paragraph position and size
    qreal paragraphTop = m_paragraphInfo[index].y;
    qreal paragraphHeight = m_paragraphInfo[index].height;
    qreal paragraphBottom = paragraphTop + paragraphHeight;

    // Calculate viewport bounds
    qreal viewportBottom = m_viewportTop + m_viewportHeight;

    // Check if paragraph is already fully visible
    if (paragraphTop >= m_viewportTop && paragraphBottom <= viewportBottom) {
        // Already visible, no scrolling needed
        return m_viewportTop;
    }

    // Calculate new scroll position
    qreal newOffset = m_viewportTop;

    if (paragraphTop < m_viewportTop) {
        // Paragraph is above viewport - scroll up to show at top
        newOffset = paragraphTop;
    } else if (paragraphBottom > viewportBottom) {
        // Paragraph is below viewport - scroll down to show at bottom
        // But ensure paragraph top is visible if it's taller than viewport
        if (paragraphHeight >= m_viewportHeight) {
            // Large paragraph - show top
            newOffset = paragraphTop;
        } else {
            // Normal paragraph - show at bottom of viewport
            newOffset = paragraphBottom - m_viewportHeight;
        }
    }

    // Apply the new scroll offset (with clamping)
    setScrollOffset(newOffset);

    return m_viewportTop;
}

qreal VirtualScrollManager::ensurePositionVisible(const CursorPosition& position) {
    return ensureParagraphVisible(position.paragraph);
}

qreal VirtualScrollManager::maxScrollOffset() const {
    // If no document or no viewport, max scroll is 0
    if (!m_document || m_viewportHeight <= 0.0) {
        return 0.0;
    }

    qreal total = totalHeight();

    // If content fits in viewport, no scrolling needed
    if (total <= m_viewportHeight) {
        return 0.0;
    }

    // Max scroll is total height minus viewport height
    // This ensures we can still see the last line at the bottom
    return total - m_viewportHeight;
}

}  // namespace kalahari::editor
