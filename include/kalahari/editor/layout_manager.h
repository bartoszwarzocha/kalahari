/// @file layout_manager.h
/// @brief Layout manager for document paragraphs (OpenSpec #00042 Phase 2.11)
///
/// LayoutManager coordinates layout of document paragraphs, working with
/// VirtualScrollManager to only layout visible paragraphs (plus buffer).
/// It uses lazy creation to minimize memory usage and computation.
///
/// Key responsibilities:
/// - Manage ParagraphLayout instances for document paragraphs
/// - Lazy creation (only create layouts when needed)
/// - Connect to VirtualScrollManager for visible range
/// - Layout only visible paragraphs + buffer
/// - Invalidate layouts on content changes
/// - React to document changes via IDocumentObserver

#pragma once

#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/paragraph_layout.h>
#include <kalahari/editor/virtual_scroll_manager.h>
#include <QFont>
#include <QRectF>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace kalahari::editor {

/// @brief Manages paragraph layouts for efficient document rendering
///
/// LayoutManager provides efficient layout management for large documents
/// by only creating and maintaining layouts for visible paragraphs.
/// It implements IDocumentObserver to automatically invalidate layouts
/// when document content changes.
///
/// Usage:
/// @code
/// LayoutManager manager;
/// manager.setDocument(&document);
/// manager.setScrollManager(&scrollManager);
/// manager.setWidth(800.0);
/// manager.setFont(QFont("Serif", 12));
///
/// // Layout visible paragraphs
/// manager.layoutVisibleParagraphs();
///
/// // Get layout for a specific paragraph (may be nullptr if not visible)
/// ParagraphLayout* layout = manager.paragraphLayout(5);
/// @endcode
///
/// Thread safety: Not thread-safe. Use from GUI thread only.
class LayoutManager : public IDocumentObserver {
public:
    /// @brief Construct an empty layout manager
    LayoutManager();

    /// @brief Destructor
    ~LayoutManager() override;

    /// @brief Copy constructor (deleted - observer registration)
    LayoutManager(const LayoutManager&) = delete;

    /// @brief Move constructor
    LayoutManager(LayoutManager&& other) noexcept;

    /// @brief Copy assignment (deleted - observer registration)
    LayoutManager& operator=(const LayoutManager&) = delete;

    /// @brief Move assignment
    LayoutManager& operator=(LayoutManager&& other) noexcept;

    // =========================================================================
    // Document and Scroll Manager
    // =========================================================================

    /// @brief Set the document to manage layouts for
    /// @param document Pointer to the document (not owned, must outlive manager)
    ///
    /// The manager registers as an observer on the document to receive
    /// change notifications. When a new document is set, all existing
    /// layouts are cleared.
    void setDocument(KmlDocument* document);

    /// @brief Get the current document
    /// @return Pointer to the document, or nullptr if not set
    KmlDocument* document() const;

    /// @brief Set the virtual scroll manager
    /// @param scrollManager Pointer to scroll manager (not owned)
    ///
    /// The scroll manager determines which paragraphs are visible
    /// and need to be laid out.
    void setScrollManager(VirtualScrollManager* scrollManager);

    /// @brief Get the current scroll manager
    /// @return Pointer to scroll manager, or nullptr if not set
    VirtualScrollManager* scrollManager() const;

    // =========================================================================
    // Layout Configuration
    // =========================================================================

    /// @brief Set the layout width for all paragraphs
    /// @param width The available width for text wrapping
    /// @note Marks all layouts as needing recalculation
    void setWidth(qreal width);

    /// @brief Get the current layout width
    /// @return The width used for layout
    qreal width() const;

    /// @brief Set the font for all paragraphs
    /// @param font The font to use for text layout
    /// @note Marks all layouts as needing recalculation
    void setFont(const QFont& font);

    /// @brief Get the current font
    /// @return The font used for layout
    QFont font() const;

    // =========================================================================
    // Layout Operations
    // =========================================================================

    /// @brief Layout all visible paragraphs (from scroll manager)
    /// @return Total height of laid-out visible paragraphs
    ///
    /// This method:
    /// 1. Gets visible range from scroll manager
    /// 2. Creates layouts for paragraphs that don't have them
    /// 3. Performs layout calculation for dirty layouts
    /// 4. Updates scroll manager with measured heights
    qreal layoutVisibleParagraphs();

    /// @brief Layout a specific paragraph
    /// @param index The paragraph index (0-based)
    /// @return Height of the laid-out paragraph, or 0 if invalid
    ///
    /// Creates the layout if it doesn't exist and performs layout
    /// calculation. Also updates scroll manager with the height.
    qreal layoutParagraph(int index);

    /// @brief Get layout for a specific paragraph
    /// @param index The paragraph index (0-based)
    /// @return Pointer to the layout, or nullptr if not created
    ///
    /// Does NOT create the layout if it doesn't exist.
    /// Use layoutParagraph() to ensure the layout exists.
    ParagraphLayout* paragraphLayout(int index);

    /// @brief Get const layout for a specific paragraph
    /// @param index The paragraph index (0-based)
    /// @return Const pointer to the layout, or nullptr if not created
    const ParagraphLayout* paragraphLayout(int index) const;

    /// @brief Check if a paragraph has a layout
    /// @param index The paragraph index (0-based)
    /// @return true if layout exists for this paragraph
    bool hasLayout(int index) const;

    /// @brief Get the number of active layouts
    /// @return Number of layouts currently in memory
    int layoutCount() const;

    // =========================================================================
    // Cache Management
    // =========================================================================

    /// @brief Invalidate layout for a specific paragraph
    /// @param index The paragraph index to invalidate
    ///
    /// Marks the layout as dirty so it will be recalculated on next access.
    /// If the layout doesn't exist, this has no effect.
    void invalidateLayout(int index);

    /// @brief Invalidate all layouts
    ///
    /// Marks all existing layouts as dirty.
    void invalidateAllLayouts();

    /// @brief Clear all layouts
    ///
    /// Removes all cached layouts from memory.
    void clearLayouts();

    /// @brief Release layouts for paragraphs outside visible range
    ///
    /// Frees memory by removing layouts for paragraphs that are
    /// no longer visible (not in scroll manager's extended range).
    /// This should be called periodically to prevent memory growth.
    void releaseInvisibleLayouts();

    // =========================================================================
    // Geometry Queries
    // =========================================================================

    /// @brief Get the Y position of a paragraph
    /// @param index The paragraph index (0-based)
    /// @return Y position from scroll manager, or 0 if invalid
    ///
    /// This delegates to the scroll manager for paragraph positions.
    qreal paragraphY(int index) const;

    /// @brief Get the height of a paragraph
    /// @param index The paragraph index (0-based)
    /// @return Height from layout if available, else estimated height
    qreal paragraphHeight(int index) const;

    /// @brief Get the total document height
    /// @return Total height from scroll manager
    qreal totalHeight() const;

    /// @brief Get bounding rect for a paragraph
    /// @param index The paragraph index (0-based)
    /// @return Bounding rectangle (x=0, y=paragraphY, width, height)
    QRectF paragraphRect(int index) const;

    // =========================================================================
    // IDocumentObserver Implementation
    // =========================================================================

    /// @brief Called when document content changes
    void onContentChanged() override;

    /// @brief Called when a paragraph is inserted
    /// @param index The index where paragraph was inserted
    void onParagraphInserted(int index) override;

    /// @brief Called when a paragraph is removed
    /// @param index The index where paragraph was removed
    void onParagraphRemoved(int index) override;

    /// @brief Called when a paragraph is modified
    /// @param index The index of modified paragraph
    void onParagraphModified(int index) override;

private:
    /// @brief Create a layout for a paragraph
    /// @param index The paragraph index
    /// @return Pointer to the created layout
    ParagraphLayout* createLayout(int index);

    /// @brief Get or create a layout for a paragraph
    /// @param index The paragraph index
    /// @return Pointer to the layout
    ParagraphLayout* getOrCreateLayout(int index);

    /// @brief Update paragraph text in layout from document
    /// @param index The paragraph index
    /// @param layout The layout to update
    void updateLayoutText(int index, ParagraphLayout* layout);

    /// @brief Shift layout indices after insert/remove
    /// @param fromIndex Starting index
    /// @param delta Amount to shift (+1 for insert, -1 for remove)
    void shiftLayoutIndices(int fromIndex, int delta);

    KmlDocument* m_document;                        ///< Document being managed (not owned)
    VirtualScrollManager* m_scrollManager;          ///< Scroll manager (not owned)
    qreal m_width;                                  ///< Layout width
    QFont m_font;                                   ///< Layout font

    /// @brief Cache of paragraph layouts
    ///
    /// Uses unordered_map for O(1) access. Key is paragraph index.
    /// Only visible paragraphs (and buffer) have layouts.
    std::unordered_map<int, std::unique_ptr<ParagraphLayout>> m_layouts;

    /// @brief Set of paragraph indices that need re-layout
    ///
    /// Tracks which paragraphs have been invalidated and need
    /// their layout recalculated. More efficient than marking
    /// all layouts dirty when only one paragraph changes.
    std::unordered_set<int> m_dirtyParagraphs;
};

}  // namespace kalahari::editor
