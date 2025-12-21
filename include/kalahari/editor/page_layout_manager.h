/// @file page_layout_manager.h
/// @brief Page layout manager for Page Mode (OpenSpec #00042 Phase 5.3-5.5)
///
/// PageLayoutManager calculates page breaks and manages document pagination
/// for Page Mode rendering. It works with LayoutManager to determine how
/// paragraphs and lines are distributed across pages.
///
/// Key responsibilities:
/// - Calculate page breaks based on PageLayout settings
/// - Track content distribution across pages (which lines on which page)
/// - Widow/orphan control (minimum lines at top/bottom of page)
/// - Provide geometry queries for page rendering
/// - Invalidate on layout changes

#pragma once

#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/editor_types.h>
#include <QRectF>
#include <vector>

namespace kalahari::editor {

// Forward declarations
class KmlDocument;
class LayoutManager;

/// @brief Range of lines from a paragraph that appear on a page
///
/// When a paragraph spans multiple pages, each page contains a ContentRange
/// that specifies which lines of that paragraph appear on that page.
struct ContentRange {
    int paragraphIndex = 0;     ///< Index of the paragraph in the document
    int lineStart = 0;          ///< First line index (0-based, within paragraph)
    int lineEnd = 0;            ///< Last line index (exclusive)
    qreal offsetY = 0.0;        ///< Y offset of this content within the page's text area

    /// @brief Get the number of lines in this range
    /// @return Number of lines (lineEnd - lineStart)
    int lineCount() const { return lineEnd - lineStart; }
};

/// @brief Information about a single page in the document
///
/// PageInfo contains all the information needed to render and navigate
/// a page in Page Mode, including its geometry and content ranges.
struct PageInfo {
    int pageNumber = 0;                 ///< Page number (1-based for display)
    qreal pageY = 0.0;                  ///< Y position of page top in document coordinates
    QRectF pageRect;                    ///< Full page rectangle (including margins)
    QRectF textRect;                    ///< Text area rectangle (page minus margins)
    std::vector<ContentRange> content;  ///< Content ranges on this page

    /// @brief Check if the page is empty (no content)
    /// @return true if no content ranges on this page
    bool isEmpty() const { return content.empty(); }
};

/// @brief Settings for pagination (widow/orphan control)
///
/// Controls how lines are distributed across page breaks to avoid
/// typographic problems like widows and orphans.
struct PaginationSettings {
    int minLinesAtTop = 2;      ///< Minimum lines at top of page (orphan control)
    int minLinesAtBottom = 2;   ///< Minimum lines at bottom of page (widow control)
};

/// @brief Manages page layout and pagination for Page Mode
///
/// PageLayoutManager calculates how document content is distributed across
/// pages based on the PageLayout settings (page size, margins, zoom).
/// It works with LayoutManager to get paragraph/line heights and determine
/// page breaks.
///
/// The manager uses lazy calculation - pages are only computed when
/// calculatePages() is called or a query method requires valid layout.
///
/// Usage:
/// @code
/// PageLayoutManager pageManager;
/// pageManager.setPageLayout(appearance.pageLayout);
/// pageManager.setDocument(&document);
/// pageManager.setLayoutManager(&layoutManager);
/// pageManager.setViewportWidth(800.0);
///
/// // Calculate pagination
/// int numPages = pageManager.calculatePages();
///
/// // Query page information
/// for (int i = 1; i <= numPages; ++i) {
///     const PageInfo* info = pageManager.pageInfo(i);
///     // Render page at pageY(i)
/// }
/// @endcode
///
/// Thread safety: Not thread-safe. Use from GUI thread only.
class PageLayoutManager {
public:
    /// @brief Construct an empty page layout manager
    PageLayoutManager();

    /// @brief Destructor
    ~PageLayoutManager();

    /// @brief Copy constructor (deleted - references to document/layout)
    PageLayoutManager(const PageLayoutManager&) = delete;

    /// @brief Move constructor
    PageLayoutManager(PageLayoutManager&& other) noexcept;

    /// @brief Copy assignment (deleted)
    PageLayoutManager& operator=(const PageLayoutManager&) = delete;

    /// @brief Move assignment
    PageLayoutManager& operator=(PageLayoutManager&& other) noexcept;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// @brief Set the page layout configuration
    /// @param layout The page layout settings (page size, margins, zoom)
    /// @note Invalidates current pagination
    void setPageLayout(const PageLayout& layout);

    /// @brief Get the current page layout
    /// @return The page layout configuration
    const PageLayout& pageLayout() const;

    /// @brief Set the document for pagination
    /// @param document Pointer to the document (not owned, must outlive manager)
    /// @note Invalidates current pagination
    void setDocument(KmlDocument* document);

    /// @brief Get the current document
    /// @return Pointer to the document, or nullptr if not set
    KmlDocument* document() const;

    /// @brief Set the layout manager for paragraph heights
    /// @param layoutManager Pointer to the layout manager (not owned)
    /// @note Invalidates current pagination
    void setLayoutManager(LayoutManager* layoutManager);

    /// @brief Get the current layout manager
    /// @return Pointer to the layout manager, or nullptr if not set
    LayoutManager* layoutManager() const;

    /// @brief Set the viewport width for page centering calculations
    /// @param width The viewport width in pixels
    /// @note Invalidates current pagination
    void setViewportWidth(qreal width);

    /// @brief Get the current viewport width
    /// @return The viewport width in pixels
    qreal viewportWidth() const;

    /// @brief Set pagination settings (widow/orphan control)
    /// @param settings The pagination settings
    /// @note Invalidates current pagination
    void setPaginationSettings(const PaginationSettings& settings);

    /// @brief Get the current pagination settings
    /// @return The pagination settings
    const PaginationSettings& paginationSettings() const;

    // =========================================================================
    // Pagination
    // =========================================================================

    /// @brief Calculate pages for the current document
    /// @return Number of pages, or 0 if document is empty or not set
    ///
    /// This method performs the main pagination algorithm:
    /// 1. Iterates through all paragraphs and their lines
    /// 2. Fills pages up to the text area height
    /// 3. Applies widow/orphan control at page breaks
    /// 4. Calculates page and text area rectangles
    ///
    /// Results are cached until invalidate() is called.
    int calculatePages();

    /// @brief Invalidate the current pagination
    ///
    /// Call this when document content, layout, or page settings change.
    /// The next call to calculatePages() or query methods will recalculate.
    void invalidate();

    /// @brief Check if pagination is valid (calculated and not invalidated)
    /// @return true if pagination is valid
    bool isValid() const;

    /// @brief Get the total number of pages
    /// @return Number of pages, or 0 if not calculated
    ///
    /// If not valid, automatically calls calculatePages().
    int totalPages() const;

    // =========================================================================
    // Page Information
    // =========================================================================

    /// @brief Get information about a specific page
    /// @param pageNumber The page number (1-based)
    /// @return Pointer to PageInfo, or nullptr if invalid page number
    ///
    /// If not valid, automatically calls calculatePages().
    const PageInfo* pageInfo(int pageNumber) const;

    /// @brief Find the page containing a cursor position
    /// @param position The cursor position in the document
    /// @return Page number (1-based), or 0 if position is invalid
    ///
    /// If not valid, automatically calls calculatePages().
    int pageForPosition(const CursorPosition& position) const;

    /// @brief Find the page at a given Y coordinate (document coordinates)
    /// @param y The Y coordinate in document space
    /// @return Page number (1-based), or 0 if y is before first page
    ///
    /// If not valid, automatically calls calculatePages().
    int pageAtY(qreal y) const;

    // =========================================================================
    // Geometry
    // =========================================================================

    /// @brief Get the Y position of a page's top edge
    /// @param pageNumber The page number (1-based)
    /// @return Y position in document coordinates, or 0 if invalid
    ///
    /// If not valid, automatically calls calculatePages().
    qreal pageY(int pageNumber) const;

    /// @brief Get the total height of all pages including gaps
    /// @return Total document height in pixels
    ///
    /// If not valid, automatically calls calculatePages().
    qreal totalHeight() const;

    /// @brief Get the full page rectangle for a page
    /// @param pageNumber The page number (1-based)
    /// @return Page rectangle, or empty rect if invalid
    ///
    /// The page rectangle includes margins. It is positioned
    /// at the page's Y coordinate, centered horizontally.
    ///
    /// If not valid, automatically calls calculatePages().
    QRectF pageRect(int pageNumber) const;

    /// @brief Get the text area rectangle for a page
    /// @param pageNumber The page number (1-based)
    /// @return Text area rectangle, or empty rect if invalid
    ///
    /// The text area is the page rectangle minus margins.
    /// This is where content is actually rendered.
    ///
    /// If not valid, automatically calls calculatePages().
    QRectF textAreaRect(int pageNumber) const;

    /// @brief Get the horizontal offset to center pages in viewport
    /// @return X offset to add to page positions for centering
    ///
    /// When the viewport is wider than the page, pages should be
    /// centered. This method returns the offset to achieve that.
    qreal pageCenterOffset() const;

private:
    /// @brief Ensure pagination is valid, calculating if necessary
    void ensureValid() const;

    /// @brief Calculate page dimensions from layout settings
    void calculatePageDimensions();

    /// @brief Add content to the current page, handling page breaks
    /// @param paraIndex Paragraph index
    /// @param lineStart First line to add
    /// @param lineEnd Last line (exclusive)
    /// @param lineHeight Height of each line
    /// @param currentY Current Y position in text area
    /// @param remainingHeight Remaining height on current page
    void addContentToPages(int paraIndex, int lineStart, int lineEnd,
                           qreal lineHeight, qreal& currentY, qreal& remainingHeight);

    /// @brief Start a new page
    void startNewPage();

    /// @brief Finalize the current page (set final geometry)
    void finalizeCurrentPage();

    PageLayout m_pageLayout;                    ///< Page layout configuration
    PaginationSettings m_paginationSettings;    ///< Pagination settings
    KmlDocument* m_document;                    ///< Document (not owned)
    LayoutManager* m_layoutManager;             ///< Layout manager (not owned)
    qreal m_viewportWidth;                      ///< Viewport width for centering

    mutable std::vector<PageInfo> m_pages;      ///< Calculated page information
    mutable bool m_valid;                       ///< Is pagination valid?
    mutable qreal m_totalHeight;                ///< Total document height

    // Cached page dimensions (in pixels at current zoom)
    qreal m_pageWidth;                          ///< Page width in pixels
    qreal m_pageHeight;                         ///< Page height in pixels
    qreal m_textAreaWidth;                      ///< Text area width in pixels
    qreal m_textAreaHeight;                     ///< Text area height in pixels
    QMarginsF m_marginsPixels;                  ///< Margins in pixels
};

}  // namespace kalahari::editor
