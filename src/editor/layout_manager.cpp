/// @file layout_manager.cpp
/// @brief Layout manager implementation (OpenSpec #00042 Phase 2.11)

#include <kalahari/editor/layout_manager.h>
#include <algorithm>

namespace kalahari::editor {

// =============================================================================
// Constructor / Destructor
// =============================================================================

LayoutManager::LayoutManager()
    : m_document(nullptr)
    , m_scrollManager(nullptr)
    , m_width(0.0)
    , m_font()
{
}

LayoutManager::~LayoutManager() {
    // Unregister from document if we were observing
    if (m_document) {
        m_document->removeObserver(this);
    }
}

LayoutManager::LayoutManager(LayoutManager&& other) noexcept
    : m_document(other.m_document)
    , m_scrollManager(other.m_scrollManager)
    , m_width(other.m_width)
    , m_font(std::move(other.m_font))
    , m_layouts(std::move(other.m_layouts))
{
    // Re-register observer on the document
    if (m_document) {
        m_document->removeObserver(&other);
        m_document->addObserver(this);
    }

    // Clear other's state
    other.m_document = nullptr;
    other.m_scrollManager = nullptr;
    other.m_width = 0.0;
}

LayoutManager& LayoutManager::operator=(LayoutManager&& other) noexcept {
    if (this != &other) {
        // Unregister from current document
        if (m_document) {
            m_document->removeObserver(this);
        }

        // Move data
        m_document = other.m_document;
        m_scrollManager = other.m_scrollManager;
        m_width = other.m_width;
        m_font = std::move(other.m_font);
        m_layouts = std::move(other.m_layouts);

        // Re-register observer
        if (m_document) {
            m_document->removeObserver(&other);
            m_document->addObserver(this);
        }

        // Clear other's state
        other.m_document = nullptr;
        other.m_scrollManager = nullptr;
        other.m_width = 0.0;
    }
    return *this;
}

// =============================================================================
// Document and Scroll Manager
// =============================================================================

void LayoutManager::setDocument(KmlDocument* document) {
    // Unregister from old document
    if (m_document) {
        m_document->removeObserver(this);
    }

    m_document = document;

    // Clear all layouts and dirty tracking when document changes
    m_layouts.clear();
    m_dirtyParagraphs.clear();

    // Register with new document
    if (m_document) {
        m_document->addObserver(this);
    }
}

KmlDocument* LayoutManager::document() const {
    return m_document;
}

void LayoutManager::setScrollManager(VirtualScrollManager* scrollManager) {
    m_scrollManager = scrollManager;
}

VirtualScrollManager* LayoutManager::scrollManager() const {
    return m_scrollManager;
}

// =============================================================================
// Layout Configuration
// =============================================================================

void LayoutManager::setWidth(qreal width) {
    if (m_width != width) {
        m_width = width;
        // Invalidate all layouts when width changes
        invalidateAllLayouts();
    }
}

qreal LayoutManager::width() const {
    return m_width;
}

void LayoutManager::setFont(const QFont& font) {
    if (m_font != font) {
        m_font = font;
        // Invalidate all layouts when font changes
        invalidateAllLayouts();
        // Also update font in existing layouts
        for (auto& [index, layout] : m_layouts) {
            layout->setFont(m_font);
        }
    }
}

QFont LayoutManager::font() const {
    return m_font;
}

// =============================================================================
// Layout Operations
// =============================================================================

qreal LayoutManager::layoutVisibleParagraphs() {
    if (!m_document || !m_scrollManager) {
        return 0.0;
    }

    auto [first, last] = m_scrollManager->visibleRange();
    if (first < 0 || last < 0) {
        return 0.0;
    }

    qreal totalHeight = 0.0;

    // Layout each visible paragraph - only if dirty or not yet laid out
    for (int i = first; i <= last; ++i) {
        auto it = m_layouts.find(i);
        bool needsLayout = (it == m_layouts.end()) ||
                           (it->second->isDirty()) ||
                           (m_dirtyParagraphs.count(i) > 0);

        if (needsLayout) {
            qreal height = layoutParagraph(i);
            totalHeight += height;
        } else {
            // Use cached height
            totalHeight += it->second->height();
        }
    }

    return totalHeight;
}

qreal LayoutManager::layoutParagraph(int index) {
    if (!m_document || index < 0 || index >= m_document->paragraphCount()) {
        return 0.0;
    }

    // Get or create the layout
    ParagraphLayout* layout = getOrCreateLayout(index);
    if (!layout) {
        return 0.0;
    }

    // Update text from document if needed (only if dirty or text changed)
    if (m_dirtyParagraphs.count(index) > 0) {
        updateLayoutText(index, layout);
    }

    // Perform layout
    qreal height = layout->doLayout(m_width);

    // Clear dirty flag after successful layout
    m_dirtyParagraphs.erase(index);

    // Update scroll manager with measured height
    if (m_scrollManager) {
        m_scrollManager->updateParagraphHeight(index, height);
    }

    return height;
}

ParagraphLayout* LayoutManager::paragraphLayout(int index) {
    auto it = m_layouts.find(index);
    if (it != m_layouts.end()) {
        return it->second.get();
    }
    return nullptr;
}

const ParagraphLayout* LayoutManager::paragraphLayout(int index) const {
    auto it = m_layouts.find(index);
    if (it != m_layouts.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool LayoutManager::hasLayout(int index) const {
    return m_layouts.find(index) != m_layouts.end();
}

int LayoutManager::layoutCount() const {
    return static_cast<int>(m_layouts.size());
}

// =============================================================================
// Cache Management
// =============================================================================

void LayoutManager::invalidateLayout(int index) {
    auto it = m_layouts.find(index);
    if (it != m_layouts.end()) {
        it->second->invalidate();
    }
    // Track which paragraphs need re-layout
    m_dirtyParagraphs.insert(index);
}

void LayoutManager::invalidateAllLayouts() {
    for (auto& [index, layout] : m_layouts) {
        layout->invalidate();
        m_dirtyParagraphs.insert(index);
    }
}

void LayoutManager::clearLayouts() {
    m_layouts.clear();
    m_dirtyParagraphs.clear();
}

void LayoutManager::releaseInvisibleLayouts() {
    if (!m_scrollManager) {
        // Without scroll manager, we can't determine visibility - clear all
        m_layouts.clear();
        return;
    }

    auto [first, last] = m_scrollManager->visibleRange();
    if (first < 0 || last < 0) {
        // No valid visible range - clear all
        m_layouts.clear();
        return;
    }

    // Remove layouts outside the visible range
    auto it = m_layouts.begin();
    while (it != m_layouts.end()) {
        if (it->first < first || it->first > last) {
            it = m_layouts.erase(it);
        } else {
            ++it;
        }
    }
}

// =============================================================================
// Geometry Queries
// =============================================================================

qreal LayoutManager::paragraphY(int index) const {
    if (m_scrollManager) {
        return m_scrollManager->paragraphY(index);
    }
    return 0.0;
}

qreal LayoutManager::paragraphHeight(int index) const {
    // First check if we have a layout with measured height
    auto it = m_layouts.find(index);
    if (it != m_layouts.end() && !it->second->isDirty()) {
        return it->second->height();
    }

    // Fall back to scroll manager (estimated or measured)
    if (m_scrollManager) {
        return m_scrollManager->paragraphInfo(index).height;
    }

    // Default estimated height
    return ESTIMATED_LINE_HEIGHT;
}

qreal LayoutManager::totalHeight() const {
    if (m_scrollManager) {
        return m_scrollManager->totalHeight();
    }
    return 0.0;
}

QRectF LayoutManager::paragraphRect(int index) const {
    qreal y = paragraphY(index);
    qreal h = paragraphHeight(index);
    return QRectF(0.0, y, m_width, h);
}

// =============================================================================
// IDocumentObserver Implementation
// =============================================================================

void LayoutManager::onContentChanged() {
    // General content change - invalidate all layouts
    invalidateAllLayouts();
}

void LayoutManager::onParagraphInserted(int index) {
    // Shift existing layout indices up
    shiftLayoutIndices(index, 1);

    // The new paragraph doesn't have a layout yet (lazy creation)
    // Layouts for affected paragraphs need invalidation since
    // content may have shifted
}

void LayoutManager::onParagraphRemoved(int index) {
    // Remove the layout for the deleted paragraph
    m_layouts.erase(index);

    // Shift remaining layout indices down
    shiftLayoutIndices(index, -1);
}

void LayoutManager::onParagraphModified(int index) {
    // Invalidate the layout for the modified paragraph
    invalidateLayout(index);
}

// =============================================================================
// Private Methods
// =============================================================================

ParagraphLayout* LayoutManager::createLayout(int index) {
    auto layout = std::make_unique<ParagraphLayout>();
    layout->setFont(m_font);

    // Get text from document
    if (m_document) {
        const KmlParagraph* para = m_document->paragraph(index);
        if (para) {
            layout->setText(para->plainText());
        }
    }

    ParagraphLayout* ptr = layout.get();
    m_layouts[index] = std::move(layout);
    return ptr;
}

ParagraphLayout* LayoutManager::getOrCreateLayout(int index) {
    auto it = m_layouts.find(index);
    if (it != m_layouts.end()) {
        return it->second.get();
    }
    return createLayout(index);
}

void LayoutManager::updateLayoutText(int index, ParagraphLayout* layout) {
    if (!m_document || !layout) {
        return;
    }

    const KmlParagraph* para = m_document->paragraph(index);
    if (!para) {
        return;
    }

    QString text = para->plainText();
    if (layout->text() != text) {
        layout->setText(text);
    }
}

void LayoutManager::shiftLayoutIndices(int fromIndex, int delta) {
    if (delta == 0) {
        return;
    }

    // We need to rebuild the map with shifted indices
    // This is necessary because unordered_map doesn't allow key modification
    std::unordered_map<int, std::unique_ptr<ParagraphLayout>> newLayouts;

    for (auto& [index, layout] : m_layouts) {
        if (index >= fromIndex) {
            // Shift this index
            int newIndex = index + delta;
            if (newIndex >= 0) {
                newLayouts[newIndex] = std::move(layout);
            }
        } else {
            // Keep as is
            newLayouts[index] = std::move(layout);
        }
    }

    m_layouts = std::move(newLayouts);
}

}  // namespace kalahari::editor
