/// @file lazy_layout_manager.h
/// @brief Lazy layout manager for viewport-only layout (OpenSpec #00043 Phase 2)
///
/// LazyLayoutManager provides Word/Writer-style lazy layout calculation.
/// It only calculates layout for visible paragraphs, using estimates for
/// off-screen content. This enables 60fps scrolling in 150k+ word documents.
///
/// Key features:
/// - Viewport-only layout calculation
/// - Height estimation for off-screen paragraphs
/// - LRU cache for ParagraphLayout instances
/// - Integration with TextBuffer's Fenwick tree
/// - Observer pattern for height updates

#pragma once

#include <kalahari/editor/text_buffer.h>
#include <kalahari/editor/paragraph_layout.h>
#include <QFont>
#include <QRectF>
#include <QTextLayout>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace kalahari::editor {

/// @brief Maximum number of paragraph layouts to cache
constexpr size_t LAZY_MAX_CACHED_LAYOUTS = 150;

/// @brief Buffer zone around visible paragraphs (paragraphs to pre-layout)
constexpr size_t LAZY_BUFFER_SIZE = 50;

/// @brief Layout state for a paragraph
enum class LayoutState {
    Estimated,   ///< Height is estimated, no layout calculated
    Calculating, ///< Layout is being calculated (background)
    Calculated   ///< Layout has been calculated, height is accurate
};

/// @brief Lazy layout manager for viewport-only rendering
///
/// LazyLayoutManager works with TextBuffer to provide efficient layout
/// for large documents. It only calculates layout for visible paragraphs
/// plus a buffer zone, using height estimates for everything else.
///
/// Usage:
/// @code
/// TextBuffer buffer;
/// buffer.setPlainText(largeDocument);
///
/// LazyLayoutManager manager(&buffer);
/// manager.setWidth(800.0);
/// manager.setFont(QFont("Serif", 12));
///
/// // Set viewport (visible area)
/// manager.setViewport(0, 600);  // Y range
///
/// // Layout visible paragraphs
/// manager.layoutVisibleParagraphs();
///
/// // Get layout for a specific paragraph
/// QTextLayout* layout = manager.getLayout(5);
/// @endcode
///
/// Thread safety: Not thread-safe. Use from GUI thread only.
class LazyLayoutManager : public ITextBufferObserver {
public:
    /// @brief Construct with TextBuffer
    /// @param buffer TextBuffer to manage layouts for (must outlive manager)
    explicit LazyLayoutManager(TextBuffer* buffer);

    /// @brief Destructor
    ~LazyLayoutManager() override;

    /// @brief Copy constructor (deleted)
    LazyLayoutManager(const LazyLayoutManager&) = delete;

    /// @brief Move constructor
    LazyLayoutManager(LazyLayoutManager&& other) noexcept;

    /// @brief Copy assignment (deleted)
    LazyLayoutManager& operator=(const LazyLayoutManager&) = delete;

    /// @brief Move assignment
    LazyLayoutManager& operator=(LazyLayoutManager&& other) noexcept;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// @brief Set the layout width
    /// @param width Available width for text wrapping
    void setWidth(double width);

    /// @brief Get the layout width
    double width() const { return m_width; }

    /// @brief Set the font for all paragraphs
    /// @param font Font to use for layout
    void setFont(const QFont& font);

    /// @brief Get the current font
    const QFont& font() const { return m_font; }

    /// @brief Get the TextBuffer
    TextBuffer* buffer() const { return m_buffer; }

    // =========================================================================
    // Viewport Management
    // =========================================================================

    /// @brief Set the visible viewport range
    /// @param y Y coordinate of viewport top
    /// @param height Height of the viewport
    void setViewport(double y, double height);

    /// @brief Get first visible paragraph index
    size_t firstVisibleParagraph() const { return m_firstVisible; }

    /// @brief Get last visible paragraph index
    size_t lastVisibleParagraph() const { return m_lastVisible; }

    /// @brief Get buffer zone start (firstVisible - BUFFER_SIZE)
    size_t bufferStart() const;

    /// @brief Get buffer zone end (lastVisible + BUFFER_SIZE)
    size_t bufferEnd() const;

    // =========================================================================
    // Layout Operations
    // =========================================================================

    /// @brief Layout all visible paragraphs + buffer zone
    /// @return Total height of visible paragraphs
    ///
    /// This method:
    /// 1. Determines visible paragraphs from viewport
    /// 2. Calculates layout for visible + buffer paragraphs
    /// 3. Updates TextBuffer with measured heights
    /// 4. Returns total height of laid-out visible paragraphs
    double layoutVisibleParagraphs();

    /// @brief Layout a specific paragraph
    /// @param index Paragraph index
    /// @return Height of the paragraph
    double layoutParagraph(size_t index);

    /// @brief Get layout for a paragraph (may be nullptr if not calculated)
    /// @param index Paragraph index
    /// @return Pointer to QTextLayout, or nullptr
    ///
    /// Does NOT create layout if it doesn't exist.
    /// Use layoutParagraph() first to ensure layout exists.
    QTextLayout* getLayout(size_t index);

    /// @brief Get const layout for a paragraph
    const QTextLayout* getLayout(size_t index) const;

    /// @brief Check if paragraph has a calculated layout
    /// @param index Paragraph index
    /// @return true if layout is calculated
    bool hasLayout(size_t index) const;

    /// @brief Get number of cached layouts
    size_t layoutCount() const { return m_layouts.size(); }

    // =========================================================================
    // Height Queries (delegates to TextBuffer)
    // =========================================================================

    /// @brief Get paragraph height (calculated or estimated)
    /// @param index Paragraph index
    /// @return Height in pixels
    double paragraphHeight(size_t index) const;

    /// @brief Get Y position of paragraph
    /// @param index Paragraph index
    /// @return Y position in pixels
    double paragraphY(size_t index) const;

    /// @brief Get total document height
    double totalHeight() const;

    /// @brief Find paragraph at Y coordinate
    /// @param y Y coordinate
    /// @return Paragraph index
    size_t findParagraphAtY(double y) const;

    /// @brief Get bounding rect for paragraph
    /// @param index Paragraph index
    /// @return Bounding rectangle
    QRectF paragraphRect(size_t index) const;

    // =========================================================================
    // Cache Management
    // =========================================================================

    /// @brief Invalidate layout for a paragraph
    /// @param index Paragraph index
    void invalidateLayout(size_t index);

    /// @brief Invalidate all layouts
    void invalidateAllLayouts();

    /// @brief Clear all cached layouts
    void clearLayouts();

    /// @brief Release layouts outside visible + buffer zone
    void releaseDistantLayouts();

    /// @brief Get maximum cached layouts
    static constexpr size_t maxCachedLayouts() { return LAZY_MAX_CACHED_LAYOUTS; }

    /// @brief Get buffer size
    static constexpr size_t bufferSize() { return LAZY_BUFFER_SIZE; }

    // =========================================================================
    // ITextBufferObserver Implementation
    // =========================================================================

    void onTextChanged() override;
    void onParagraphInserted(size_t index) override;
    void onParagraphRemoved(size_t index) override;
    void onParagraphChanged(size_t index) override;
    void onHeightChanged(size_t index, double oldHeight, double newHeight) override;

private:
    /// @brief Paragraph layout info
    struct LayoutInfo {
        std::unique_ptr<QTextLayout> layout;
        uint64_t lastAccess = 0;  ///< For LRU eviction
        bool dirty = true;        ///< Needs recalculation
    };

    /// @brief Create layout for a paragraph
    QTextLayout* createLayout(size_t index);

    /// @brief Get or create layout for a paragraph
    QTextLayout* getOrCreateLayout(size_t index);

    /// @brief Update layout text from buffer
    void updateLayoutText(size_t index, QTextLayout* layout);

    /// @brief Perform layout calculation
    double performLayout(QTextLayout* layout);

    /// @brief Touch layout (update access time for LRU)
    void touchLayout(size_t index);

    /// @brief Evict oldest layouts to stay under max
    void evictOldestLayouts(size_t keepCount);

    /// @brief Shift layout indices after insert/remove
    void shiftLayoutIndices(size_t fromIndex, int delta);

    /// @brief Update visible range from viewport
    void updateVisibleRange();

    TextBuffer* m_buffer;                              ///< Text buffer (not owned)
    double m_width = 0.0;                              ///< Layout width
    QFont m_font;                                      ///< Layout font

    // Viewport state
    double m_viewportY = 0.0;                          ///< Viewport top
    double m_viewportHeight = 0.0;                     ///< Viewport height
    size_t m_firstVisible = 0;                         ///< First visible paragraph
    size_t m_lastVisible = 0;                          ///< Last visible paragraph

    // Layout cache
    std::unordered_map<size_t, LayoutInfo> m_layouts;  ///< Cached layouts
    std::unordered_set<size_t> m_dirtyParagraphs;      ///< Paragraphs needing re-layout
    mutable uint64_t m_accessCounter = 0;              ///< For LRU tracking
};

}  // namespace kalahari::editor
