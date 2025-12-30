/// @file lazy_layout_manager.cpp
/// @brief LazyLayoutManager implementation (OpenSpec #00043 Phase 2)

#include <kalahari/editor/lazy_layout_manager.h>
#include <QFontMetrics>
#include <algorithm>
#include <vector>

namespace kalahari::editor {

// =============================================================================
// Constructor / Destructor
// =============================================================================

LazyLayoutManager::LazyLayoutManager(TextBuffer* buffer)
    : m_buffer(buffer) {
    if (m_buffer) {
        m_buffer->addObserver(this);
    }
}

LazyLayoutManager::~LazyLayoutManager() {
    if (m_buffer) {
        m_buffer->removeObserver(this);
    }
}

LazyLayoutManager::LazyLayoutManager(LazyLayoutManager&& other) noexcept
    : m_buffer(other.m_buffer)
    , m_width(other.m_width)
    , m_font(std::move(other.m_font))
    , m_viewportY(other.m_viewportY)
    , m_viewportHeight(other.m_viewportHeight)
    , m_firstVisible(other.m_firstVisible)
    , m_lastVisible(other.m_lastVisible)
    , m_layouts(std::move(other.m_layouts))
    , m_dirtyParagraphs(std::move(other.m_dirtyParagraphs))
    , m_accessCounter(other.m_accessCounter) {
    // Re-register observer
    if (m_buffer) {
        m_buffer->removeObserver(&other);
        m_buffer->addObserver(this);
    }
    other.m_buffer = nullptr;
}

LazyLayoutManager& LazyLayoutManager::operator=(LazyLayoutManager&& other) noexcept {
    if (this != &other) {
        // Unregister from current buffer
        if (m_buffer) {
            m_buffer->removeObserver(this);
        }

        // Move data
        m_buffer = other.m_buffer;
        m_width = other.m_width;
        m_font = std::move(other.m_font);
        m_viewportY = other.m_viewportY;
        m_viewportHeight = other.m_viewportHeight;
        m_firstVisible = other.m_firstVisible;
        m_lastVisible = other.m_lastVisible;
        m_layouts = std::move(other.m_layouts);
        m_dirtyParagraphs = std::move(other.m_dirtyParagraphs);
        m_accessCounter = other.m_accessCounter;

        // Re-register observer
        if (m_buffer) {
            m_buffer->removeObserver(&other);
            m_buffer->addObserver(this);
        }
        other.m_buffer = nullptr;
    }
    return *this;
}

// =============================================================================
// Configuration
// =============================================================================

void LazyLayoutManager::setWidth(double width) {
    if (m_width != width) {
        m_width = width;
        invalidateAllLayouts();
    }
}

void LazyLayoutManager::setFont(const QFont& font) {
    if (m_font != font) {
        m_font = font;
        // Update font in existing layouts
        for (auto& [index, info] : m_layouts) {
            if (info.layout) {
                info.layout->setFont(m_font);
                info.dirty = true;
            }
        }
        // Update estimated line height in buffer
        if (m_buffer) {
            QFontMetrics fm(m_font);
            m_buffer->setEstimatedLineHeight(fm.lineSpacing());
        }
        invalidateAllLayouts();
    }
}

// =============================================================================
// Viewport Management
// =============================================================================

void LazyLayoutManager::setViewport(double y, double height) {
    m_viewportY = y;
    m_viewportHeight = height;
    updateVisibleRange();
}

size_t LazyLayoutManager::bufferStart() const {
    if (m_firstVisible < LAZY_BUFFER_SIZE) {
        return 0;
    }
    return m_firstVisible - LAZY_BUFFER_SIZE;
}

size_t LazyLayoutManager::bufferEnd() const {
    if (!m_buffer) return 0;
    size_t count = m_buffer->paragraphCount();
    size_t end = m_lastVisible + LAZY_BUFFER_SIZE;
    return std::min(end, count > 0 ? count - 1 : 0);
}

void LazyLayoutManager::updateVisibleRange() {
    if (!m_buffer || m_buffer->paragraphCount() == 0) {
        m_firstVisible = 0;
        m_lastVisible = 0;
        return;
    }

    // Find first visible paragraph using binary search (Fenwick tree)
    m_firstVisible = m_buffer->getParagraphAtY(m_viewportY);

    // Find last visible paragraph
    double viewportBottom = m_viewportY + m_viewportHeight;
    m_lastVisible = m_buffer->getParagraphAtY(viewportBottom);

    // Ensure within bounds
    size_t count = m_buffer->paragraphCount();
    if (m_lastVisible >= count) {
        m_lastVisible = count > 0 ? count - 1 : 0;
    }
}

// =============================================================================
// Layout Operations
// =============================================================================

double LazyLayoutManager::layoutVisibleParagraphs() {
    if (!m_buffer || m_buffer->paragraphCount() == 0) {
        return 0.0;
    }

    // Update visible range
    updateVisibleRange();

    // Calculate layout for visible + buffer paragraphs
    size_t start = bufferStart();
    size_t end = bufferEnd();

    double totalVisibleHeight = 0.0;

    for (size_t i = start; i <= end; ++i) {
        double height = layoutParagraph(i);

        // Track height only for visible paragraphs
        if (i >= m_firstVisible && i <= m_lastVisible) {
            totalVisibleHeight += height;
        }
    }

    // Release distant layouts to bound memory
    releaseDistantLayouts();

    return totalVisibleHeight;
}

double LazyLayoutManager::layoutParagraph(size_t index) {
    if (!m_buffer || index >= m_buffer->paragraphCount()) {
        return 0.0;
    }

    // Get or create layout
    QTextLayout* layout = getOrCreateLayout(index);
    if (!layout) {
        return m_buffer->getParagraphHeight(index);
    }

    // Check if layout needs recalculation
    auto it = m_layouts.find(index);
    bool needsLayout = (it != m_layouts.end() && it->second.dirty) ||
                       m_dirtyParagraphs.count(index) > 0;

    if (needsLayout) {
        // Update text if needed
        updateLayoutText(index, layout);

        // Perform layout calculation
        double height = performLayout(layout);

        // Update height in TextBuffer
        m_buffer->setParagraphHeight(index, height);

        // Clear dirty flags
        if (it != m_layouts.end()) {
            it->second.dirty = false;
        }
        m_dirtyParagraphs.erase(index);

        return height;
    }

    // Return cached height
    return m_buffer->getParagraphHeight(index);
}

QTextLayout* LazyLayoutManager::getLayout(size_t index) {
    auto it = m_layouts.find(index);
    if (it != m_layouts.end() && it->second.layout) {
        // Return cached layout - caller should ensure layoutVisibleParagraphs()
        // was called before rendering to refresh dirty layouts.
        // Do NOT call layoutParagraph() here - it's expensive and getLayout()
        // may be called multiple times per frame (cursorRect, paintParagraph, etc.)
        touchLayout(index);
        return it->second.layout.get();
    }
    return nullptr;
}

const QTextLayout* LazyLayoutManager::getLayout(size_t index) const {
    auto it = m_layouts.find(index);
    if (it != m_layouts.end() && it->second.layout) {
        return it->second.layout.get();
    }
    return nullptr;
}

bool LazyLayoutManager::hasLayout(size_t index) const {
    auto it = m_layouts.find(index);
    return it != m_layouts.end() && it->second.layout && !it->second.dirty;
}

// =============================================================================
// Height Queries
// =============================================================================

double LazyLayoutManager::paragraphHeight(size_t index) const {
    if (!m_buffer) return 0.0;
    return m_buffer->getParagraphHeight(index);
}

double LazyLayoutManager::paragraphY(size_t index) const {
    if (!m_buffer) return 0.0;
    return m_buffer->getParagraphY(index);
}

double LazyLayoutManager::totalHeight() const {
    if (!m_buffer) return 0.0;
    return m_buffer->totalHeight();
}

size_t LazyLayoutManager::findParagraphAtY(double y) const {
    if (!m_buffer) return 0;
    return m_buffer->getParagraphAtY(y);
}

QRectF LazyLayoutManager::paragraphRect(size_t index) const {
    double y = paragraphY(index);
    double h = paragraphHeight(index);
    return QRectF(0.0, y, m_width, h);
}

// =============================================================================
// Cache Management
// =============================================================================

void LazyLayoutManager::invalidateLayout(size_t index) {
    auto it = m_layouts.find(index);
    if (it != m_layouts.end()) {
        it->second.dirty = true;
    }
    m_dirtyParagraphs.insert(index);

    // Also invalidate height in buffer
    if (m_buffer) {
        m_buffer->invalidateParagraphHeight(index);
    }
}

void LazyLayoutManager::invalidateAllLayouts() {
    for (auto& [index, info] : m_layouts) {
        info.dirty = true;
        m_dirtyParagraphs.insert(index);
    }
}

void LazyLayoutManager::clearLayouts() {
    m_layouts.clear();
    m_dirtyParagraphs.clear();
    m_accessCounter = 0;
}

void LazyLayoutManager::releaseDistantLayouts() {
    if (m_layouts.size() <= LAZY_MAX_CACHED_LAYOUTS) {
        return;
    }

    // First pass: remove layouts outside buffer zone
    size_t start = bufferStart();
    size_t end = bufferEnd();

    auto it = m_layouts.begin();
    while (it != m_layouts.end()) {
        if (it->first < start || it->first > end) {
            it = m_layouts.erase(it);
        } else {
            ++it;
        }
    }

    // Second pass: if still over limit, evict oldest
    if (m_layouts.size() > LAZY_MAX_CACHED_LAYOUTS) {
        evictOldestLayouts(LAZY_MAX_CACHED_LAYOUTS);
    }
}

void LazyLayoutManager::evictOldestLayouts(size_t keepCount) {
    if (m_layouts.size() <= keepCount) {
        return;
    }

    // Collect all indices with access times
    std::vector<std::pair<size_t, uint64_t>> accessTimes;
    accessTimes.reserve(m_layouts.size());

    for (const auto& [index, info] : m_layouts) {
        accessTimes.emplace_back(index, info.lastAccess);
    }

    // Sort by access time (oldest first)
    std::sort(accessTimes.begin(), accessTimes.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    // Remove oldest
    size_t toRemove = m_layouts.size() - keepCount;
    for (size_t i = 0; i < toRemove && i < accessTimes.size(); ++i) {
        m_layouts.erase(accessTimes[i].first);
    }
}

void LazyLayoutManager::touchLayout(size_t index) {
    auto it = m_layouts.find(index);
    if (it != m_layouts.end()) {
        it->second.lastAccess = ++m_accessCounter;
    }
}

// =============================================================================
// ITextBufferObserver Implementation
// =============================================================================

void LazyLayoutManager::onTextChanged() {
    // Full text change - invalidate all layouts
    invalidateAllLayouts();
}

void LazyLayoutManager::onParagraphInserted(size_t index) {
    // Shift existing layout indices
    shiftLayoutIndices(index, 1);
    // New paragraph doesn't have layout yet (lazy creation)
}

void LazyLayoutManager::onParagraphRemoved(size_t index) {
    // Remove layout for deleted paragraph
    m_layouts.erase(index);
    m_dirtyParagraphs.erase(index);
    // Shift remaining indices
    shiftLayoutIndices(index, -1);
}

void LazyLayoutManager::onParagraphChanged(size_t index) {
    invalidateLayout(index);
}

void LazyLayoutManager::onHeightChanged(size_t /*index*/, double /*oldHeight*/, double /*newHeight*/) {
    // Height changed in buffer - update visible range
    updateVisibleRange();
}

// =============================================================================
// Private Methods
// =============================================================================

QTextLayout* LazyLayoutManager::createLayout(size_t index) {
    if (!m_buffer || index >= m_buffer->paragraphCount()) {
        return nullptr;
    }

    auto layout = std::make_unique<QTextLayout>();
    layout->setFont(m_font);
    layout->setText(m_buffer->paragraphText(index));

    QTextLayout* ptr = layout.get();

    LayoutInfo info;
    info.layout = std::move(layout);
    info.lastAccess = ++m_accessCounter;
    info.dirty = true;

    m_layouts[index] = std::move(info);

    return ptr;
}

QTextLayout* LazyLayoutManager::getOrCreateLayout(size_t index) {
    auto it = m_layouts.find(index);
    if (it != m_layouts.end() && it->second.layout) {
        touchLayout(index);
        return it->second.layout.get();
    }
    return createLayout(index);
}

void LazyLayoutManager::updateLayoutText(size_t index, QTextLayout* layout) {
    if (!m_buffer || !layout) {
        return;
    }

    QString text = m_buffer->paragraphText(index);
    if (layout->text() != text) {
        layout->setText(text);
    }
}

double LazyLayoutManager::performLayout(QTextLayout* layout) {
    if (!layout || m_width <= 0) {
        return 0.0;
    }

    layout->beginLayout();

    double y = 0.0;
    while (true) {
        QTextLine line = layout->createLine();
        if (!line.isValid()) {
            break;
        }

        line.setLineWidth(m_width);
        line.setPosition(QPointF(0, y));
        y += line.height();
    }

    layout->endLayout();

    return y;
}

void LazyLayoutManager::shiftLayoutIndices(size_t fromIndex, int delta) {
    if (delta == 0) {
        return;
    }

    // Rebuild map with shifted indices
    std::unordered_map<size_t, LayoutInfo> newLayouts;
    std::unordered_set<size_t> newDirty;

    for (auto& [index, info] : m_layouts) {
        if (index >= fromIndex) {
            // Shift this index
            size_t newIndex = static_cast<size_t>(static_cast<int>(index) + delta);
            newLayouts[newIndex] = std::move(info);
            if (m_dirtyParagraphs.count(index) > 0) {
                newDirty.insert(newIndex);
            }
        } else {
            // Keep as is
            newLayouts[index] = std::move(info);
            if (m_dirtyParagraphs.count(index) > 0) {
                newDirty.insert(index);
            }
        }
    }

    m_layouts = std::move(newLayouts);
    m_dirtyParagraphs = std::move(newDirty);
}

}  // namespace kalahari::editor
