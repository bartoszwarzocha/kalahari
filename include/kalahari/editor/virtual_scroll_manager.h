/// @file virtual_scroll_manager.h
/// @brief Virtual scroll manager for efficient large document scrolling (OpenSpec #00042 Phase 2.8-2.9)
///
/// VirtualScrollManager tracks which paragraphs are visible in the viewport
/// and provides efficient scrolling for documents with many paragraphs.
/// It maintains a buffer of paragraphs above and below the visible area
/// to ensure smooth scrolling without layout recalculation delays.
///
/// Key responsibilities:
/// - Track visible paragraph range based on viewport position
/// - Include buffer paragraphs for smooth scrolling
/// - Calculate visible range from scroll offset and viewport height
/// - Integrate with KmlDocument for paragraph access
/// - Track paragraph heights for accurate scroll position calculation (Phase 2.9)

#pragma once

#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/kml_document.h>
#include <QPair>
#include <memory>
#include <vector>

namespace kalahari::editor {

/// @brief Number of paragraphs to keep as buffer above and below visible area
///
/// Buffer paragraphs are pre-laid-out to ensure smooth scrolling.
/// When the user scrolls, these paragraphs are immediately ready
/// to be displayed without layout computation delay.
constexpr int BUFFER_PARAGRAPHS = 10;

/// @brief Estimated height for paragraphs whose actual height is not yet known
///
/// This is used for paragraphs that haven't been laid out yet.
/// A typical paragraph with one line of text at 12-14pt font is around 20-25px.
/// Using a slightly conservative estimate ensures we don't miss paragraphs
/// that might be taller than expected.
constexpr qreal ESTIMATED_LINE_HEIGHT = 20.0;

/// @brief Information about a paragraph's position and size for virtual scrolling
///
/// ParagraphInfo stores the Y position and height of a paragraph.
/// When the actual height is not known (heightKnown == false),
/// the height field contains an estimated value based on ESTIMATED_LINE_HEIGHT.
struct ParagraphInfo {
    /// @brief Y position of the paragraph (top edge) relative to document start
    qreal y = 0.0;

    /// @brief Height of the paragraph in pixels
    qreal height = ESTIMATED_LINE_HEIGHT;

    /// @brief Whether the height has been measured (true) or is estimated (false)
    bool heightKnown = false;

    /// @brief Default constructor with estimated height
    ParagraphInfo() = default;

    /// @brief Constructor with specific values
    /// @param yPos Y position of paragraph
    /// @param h Height of paragraph
    /// @param known Whether height is measured or estimated
    ParagraphInfo(qreal yPos, qreal h, bool known)
        : y(yPos), height(h), heightKnown(known) {}

    /// @brief Equality comparison
    bool operator==(const ParagraphInfo& other) const {
        return y == other.y && height == other.height && heightKnown == other.heightKnown;
    }

    /// @brief Inequality comparison
    bool operator!=(const ParagraphInfo& other) const {
        return !(*this == other);
    }
};

/// @brief Virtual scroll manager for efficient large document scrolling
///
/// VirtualScrollManager optimizes rendering of large documents by tracking
/// which paragraphs are visible in the current viewport. Only visible
/// paragraphs (plus a buffer) need to be laid out and rendered.
///
/// The manager works with a viewport concept:
/// - Viewport top: The Y position of the top of the visible area (scroll offset)
/// - Viewport height: The height of the visible area
///
/// Usage:
/// @code
/// VirtualScrollManager scrollManager;
/// scrollManager.setDocument(&document);
/// scrollManager.setViewport(0, 600);  // Viewport starts at top, 600px tall
///
/// auto [first, last] = scrollManager.visibleRange();
/// for (int i = first; i <= last; ++i) {
///     // Layout and render paragraph i
/// }
/// @endcode
///
/// Thread safety: Not thread-safe. Use from GUI thread only.
class VirtualScrollManager {
public:
    /// @brief Construct an empty virtual scroll manager
    VirtualScrollManager();

    /// @brief Destructor
    ~VirtualScrollManager();

    /// @brief Copy constructor
    VirtualScrollManager(const VirtualScrollManager& other);

    /// @brief Move constructor
    VirtualScrollManager(VirtualScrollManager&& other) noexcept;

    /// @brief Copy assignment
    VirtualScrollManager& operator=(const VirtualScrollManager& other);

    /// @brief Move assignment
    VirtualScrollManager& operator=(VirtualScrollManager&& other) noexcept;

    // =========================================================================
    // Document Management
    // =========================================================================

    /// @brief Set the document to manage scrolling for
    /// @param document Pointer to the document (not owned, must outlive manager)
    ///
    /// When a new document is set, paragraph info is reinitialized.
    /// All paragraph heights are marked as unknown until updated.
    void setDocument(KmlDocument* document);

    /// @brief Get the current document
    /// @return Pointer to the document, or nullptr if not set
    KmlDocument* document() const;

    // =========================================================================
    // Viewport Management
    // =========================================================================

    /// @brief Set the viewport position and size
    /// @param top The Y position of the top of the viewport (scroll offset)
    /// @param height The height of the viewport in pixels
    ///
    /// The viewport defines the visible area of the document.
    /// After setting the viewport, call visibleRange() to get
    /// which paragraphs need to be rendered.
    void setViewport(qreal top, qreal height);

    /// @brief Get the current viewport top position
    /// @return The Y position of the top of the viewport
    qreal viewportTop() const;

    /// @brief Get the current viewport height
    /// @return The height of the viewport
    qreal viewportHeight() const;

    /// @brief Set just the viewport top position (scroll offset)
    /// @param top The Y position of the top of the viewport
    void setViewportTop(qreal top);

    /// @brief Set just the viewport height
    /// @param height The height of the viewport
    void setViewportHeight(qreal height);

    // =========================================================================
    // Visible Range Calculation
    // =========================================================================

    /// @brief Get the range of visible paragraphs (including buffer)
    /// @return Pair of (first visible index, last visible index)
    ///
    /// The returned range includes BUFFER_PARAGRAPHS above and below
    /// the actually visible paragraphs to allow smooth scrolling.
    ///
    /// If no document is set or the document is empty, returns (-1, -1).
    /// The returned indices are always valid for the document:
    /// - first >= 0
    /// - last < document->paragraphCount()
    ///
    /// @note This is the "extended" visible range including buffer.
    ///       For the exact visible range, use exactVisibleRange().
    QPair<int, int> visibleRange() const;

    /// @brief Get the exact range of visible paragraphs (no buffer)
    /// @return Pair of (first visible index, last visible index)
    ///
    /// Returns only paragraphs that are actually visible in the viewport,
    /// without the buffer paragraphs. Useful for rendering optimizations
    /// where you need to know exactly what the user can see.
    ///
    /// If no document is set or the document is empty, returns (-1, -1).
    QPair<int, int> exactVisibleRange() const;

    /// @brief Check if a paragraph is visible (including buffer)
    /// @param paragraphIndex The paragraph index to check
    /// @return true if the paragraph is in the visible range (including buffer)
    bool isParagraphVisible(int paragraphIndex) const;

    /// @brief Check if a paragraph is exactly visible (no buffer)
    /// @param paragraphIndex The paragraph index to check
    /// @return true if the paragraph is in the exact visible range
    bool isParagraphExactlyVisible(int paragraphIndex) const;

    // =========================================================================
    // Buffer Configuration
    // =========================================================================

    /// @brief Get the number of buffer paragraphs
    /// @return The number of paragraphs kept as buffer above and below
    ///
    /// This returns the configured buffer size, which may differ from
    /// BUFFER_PARAGRAPHS if setBufferParagraphs() was called.
    int bufferParagraphs() const;

    /// @brief Set the number of buffer paragraphs
    /// @param count Number of paragraphs to keep as buffer (must be >= 0)
    ///
    /// More buffer paragraphs mean smoother scrolling but more memory/CPU
    /// used for layout. The default is BUFFER_PARAGRAPHS (10).
    void setBufferParagraphs(int count);

    // =========================================================================
    // Height Management (Phase 2.9)
    // =========================================================================

    /// @brief Update the height of a specific paragraph
    /// @param index The paragraph index (0-based)
    /// @param height The measured height in pixels
    ///
    /// Call this after laying out a paragraph to provide accurate height data.
    /// This improves scroll position accuracy compared to using estimated heights.
    /// If index is out of range, the call is ignored.
    ///
    /// After updating heights, subsequent calls to paragraphY() and totalHeight()
    /// will reflect the new measured height.
    void updateParagraphHeight(int index, qreal height);

    /// @brief Get the total height of all paragraphs
    /// @return Total document height in pixels
    ///
    /// Uses known heights for measured paragraphs and ESTIMATED_LINE_HEIGHT
    /// for paragraphs that haven't been laid out yet.
    qreal totalHeight() const;

    /// @brief Get the Y position of a paragraph
    /// @param index The paragraph index (0-based)
    /// @return Y position of the paragraph's top edge, or 0.0 if index is invalid
    ///
    /// The Y position is the sum of heights of all preceding paragraphs.
    qreal paragraphY(int index) const;

    /// @brief Get paragraph info for a specific index
    /// @param index The paragraph index (0-based)
    /// @return ParagraphInfo with position, height, and known flag
    ///
    /// Returns a default ParagraphInfo if index is out of range.
    ParagraphInfo paragraphInfo(int index) const;

    /// @brief Check if a paragraph's height is known (measured)
    /// @param index The paragraph index (0-based)
    /// @return true if height has been measured, false if estimated
    bool isHeightKnown(int index) const;

    /// @brief Get the number of paragraphs with known heights
    /// @return Count of paragraphs that have been measured
    int knownHeightCount() const;

    /// @brief Reset all paragraph heights to estimated values
    ///
    /// Call this when the document changes significantly (e.g., font change)
    /// and all heights need to be re-measured.
    void resetHeights();

    // =========================================================================
    // Scrolling Support (Phase 2.10)
    // =========================================================================

    /// @brief Get the current scroll offset
    /// @return The Y position of the top of the viewport (same as viewportTop())
    ///
    /// This is an alias for viewportTop() for semantic clarity when used
    /// for scrolling operations.
    qreal scrollOffset() const;

    /// @brief Set the scroll offset
    /// @param offset The Y position to scroll to (clamped to valid range)
    ///
    /// The offset is clamped to [0, max scroll], where max scroll ensures
    /// the viewport doesn't extend past the document content.
    /// This is an alias for setViewportTop() with clamping.
    void setScrollOffset(qreal offset);

    /// @brief Get the paragraph index at a given Y position
    /// @param y The Y position in document coordinates
    /// @return The index of the paragraph at that Y position, or -1 if invalid
    ///
    /// Finds the paragraph whose Y range (y to y+height) contains the given position.
    /// If y is past the end of the document, returns the last paragraph index.
    /// If y is negative, returns 0.
    int paragraphAtY(qreal y) const;

    /// @brief Scroll to ensure a paragraph is visible in the viewport
    /// @param index The paragraph index to make visible
    /// @return The new scroll offset after adjustment
    ///
    /// If the paragraph is already fully visible, no scrolling occurs.
    /// If the paragraph is above the viewport, scrolls up to show it at the top.
    /// If the paragraph is below the viewport, scrolls down to show it at the bottom.
    /// Returns the scroll offset after adjustment (may be unchanged if already visible).
    qreal ensureParagraphVisible(int index);

    /// @brief Scroll to ensure a cursor position is visible in the viewport
    /// @param position The cursor position to make visible
    /// @return The new scroll offset after adjustment
    ///
    /// This is a convenience method that calls ensureParagraphVisible()
    /// with the paragraph index from the cursor position.
    qreal ensurePositionVisible(const CursorPosition& position);

    /// @brief Get the maximum valid scroll offset
    /// @return Maximum scroll offset that keeps content visible
    ///
    /// The maximum scroll ensures the viewport can still show content
    /// at the end of the document. Returns 0 if document fits in viewport.
    qreal maxScrollOffset() const;

private:
    /// @brief Calculate the first visible paragraph index
    /// @return First paragraph index that is at least partially visible
    int calculateFirstVisibleParagraph() const;

    /// @brief Calculate the last visible paragraph index
    /// @return Last paragraph index that is at least partially visible
    int calculateLastVisibleParagraph() const;

    /// @brief Ensure paragraph info vector matches document size
    ///
    /// Called automatically when document changes. Adds or removes
    /// ParagraphInfo entries as needed, and recalculates Y positions.
    /// This is const because it only modifies the mutable m_paragraphInfo.
    void syncParagraphInfo() const;

    /// @brief Recalculate Y positions for all paragraphs
    ///
    /// Called after height updates to ensure Y positions are consistent.
    void recalculateYPositions();

    /// @brief Recalculate Y positions starting from a specific index
    /// @param fromIndex Starting index for recalculation
    ///
    /// More efficient than full recalculation when only one paragraph changed.
    /// Used internally for incremental updates.
    /// @deprecated Use Fenwick tree operations instead for O(log N) performance
    void recalculateYPositionsFrom(int fromIndex);

    // =========================================================================
    // Fenwick Tree (Binary Indexed Tree) for O(log N) height operations
    // =========================================================================

    /// @brief Initialize Fenwick tree with given size
    /// @param size Number of paragraphs
    ///
    /// Allocates the tree array and initializes all values to 0.
    /// Call rebuildFenwick() after this to populate from paragraph heights.
    void initFenwick(size_t size);

    /// @brief Rebuild Fenwick tree from current paragraph heights
    ///
    /// Called after document changes that affect paragraph count.
    /// O(N) operation but only needed when paragraphs are added/removed.
    void rebuildFenwick() const;

    /// @brief Update height at index by delta value
    /// @param index Paragraph index (0-based)
    /// @param delta Change in height (can be positive or negative)
    ///
    /// O(log N) operation - updates the tree to reflect height change.
    void updateFenwick(size_t index, qreal delta) const;

    /// @brief Get sum of heights for paragraphs [0, index)
    /// @param index Upper bound (exclusive)
    /// @return Sum of heights of paragraphs 0 through index-1
    ///
    /// O(log N) operation. This gives the Y position of paragraph at index.
    qreal prefixSum(size_t index) const;

    /// @brief Get total height using Fenwick tree
    /// @return Total height of all paragraphs
    ///
    /// O(log N) operation - equivalent to prefixSum(paragraphCount).
    qreal totalHeightFenwick() const;

    /// @brief Find paragraph index at given Y position using binary search
    /// @param y Y position in document coordinates
    /// @return Paragraph index containing that Y position
    ///
    /// O(log^2 N) operation using binary search with prefix sum queries.
    int findParagraphAtY(qreal y) const;

    /// @brief Ensure Fenwick tree is valid and up-to-date
    ///
    /// Rebuilds the tree if it has been invalidated.
    void ensureFenwickValid() const;

    KmlDocument* m_document;        ///< Document being scrolled (not owned)
    qreal m_viewportTop;            ///< Y position of viewport top
    qreal m_viewportHeight;         ///< Height of the viewport
    int m_bufferParagraphs;         ///< Number of buffer paragraphs

    /// @brief Paragraph position and height information
    ///
    /// Mutable because it's lazily updated during const operations
    /// when the document paragraph count changes.
    mutable std::vector<ParagraphInfo> m_paragraphInfo;

    /// @brief Fenwick tree for O(log N) prefix sum queries
    ///
    /// Tree is 1-indexed internally: index i in tree corresponds to
    /// paragraph i-1 in the document. Size is m_paragraphInfo.size() + 1.
    mutable std::vector<qreal> m_fenwickTree;

    /// @brief Flag indicating if Fenwick tree needs rebuilding
    ///
    /// Set to true when paragraph count changes or heights are reset.
    mutable bool m_fenwickDirty;

    /// @brief Cached total document height for O(1) access
    ///
    /// Updated when paragraph heights change. Avoids summing all heights.
    mutable qreal m_cachedTotalHeight;

    /// @brief Flag indicating if cached total height is valid
    mutable bool m_totalHeightValid;
};

}  // namespace kalahari::editor
