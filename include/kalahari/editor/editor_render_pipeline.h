/// @file editor_render_pipeline.h
/// @brief Unified rendering pipeline for BookEditor (OpenSpec #00043 Phase 12.1)
///
/// EditorRenderPipeline consolidates all rendering logic into a single class with
/// one entry point: render(painter, clipRect). This replaces the scattered rendering
/// paths in BookEditor, RenderEngine, and ViewportManager.
///
/// Pipeline stages:
/// 1. TEXT      - Get content from ITextSource (QTextDocument or KmlDocumentModel)
/// 2. ATTRIBUTES - Apply RenderContext (font, colors, margins, scale)
/// 3. LAYOUT    - Calculate block positions (using KalahariTextDocumentLayout)
/// 4. RENDER    - Draw to painter (text, cursor, selection, overlays)

#pragma once

#include <kalahari/editor/text_source_adapter.h>
#include <kalahari/editor/render_context.h>
#include <kalahari/editor/editor_types.h>
#include <QObject>
#include <QRect>
#include <memory>
#include <optional>

class QPainter;
class QTextDocument;

namespace kalahari::editor {

// Forward declarations
class ViewportManager;
class SearchEngine;
class KalahariTextDocumentLayout;

// =============================================================================
// Pagination Structures (Phase 13.3: moved from BookEditor)
// =============================================================================

/// @brief A slice of a paragraph that fits on a single page
///
/// When a paragraph spans multiple pages, it's split into slices.
/// Each slice contains a range of lines from the paragraph.
struct ParagraphSlice {
    size_t paraIndex;      ///< Index of the paragraph in the document
    int startLine;         ///< First line index (inclusive)
    int endLine;           ///< Last line index (exclusive)
    double yOffset;        ///< Y offset on the page (in screen pixels)
};

/// @brief Content of a single page in Page Mode
///
/// Contains the page geometry and all paragraph slices that appear on this page.
struct PageContent {
    double pageY;                           ///< Y position of page top in document coordinates
    QRectF pageRect;                        ///< Page rectangle (including margins) in widget coordinates
    QRectF textRect;                        ///< Text area rectangle (excluding margins) in widget coordinates
    std::vector<ParagraphSlice> slices;     ///< Paragraph slices on this page
};

/// @brief Unified rendering pipeline for the editor
///
/// EditorRenderPipeline provides a single entry point for all editor rendering.
/// It consolidates logic from RenderEngine, BookEditor::paintEvent, and ViewportManager
/// into a clean pipeline with defined stages.
///
/// Usage:
/// @code
/// // Setup pipeline
/// EditorRenderPipeline pipeline;
/// pipeline.setTextSource(std::make_unique<QTextDocumentSource>(doc));
///
/// RenderContext ctx;
/// ctx.margins = {50, 30, 50, 30};
/// ctx.colors.text = Qt::black;
/// pipeline.setContext(ctx);
///
/// // In paintEvent:
/// pipeline.render(&painter, event->rect());
/// @endcode
///
/// Key benefits:
/// - Single render() call replaces multiple painting paths
/// - All state centralized in RenderContext
/// - Clear separation of concerns (text source, attributes, layout, rendering)
/// - Easy to extend with new features (scale, margins, effects)
class EditorRenderPipeline : public QObject {
    Q_OBJECT

public:
    /// @brief Construct render pipeline
    /// @param parent Parent QObject
    explicit EditorRenderPipeline(QObject* parent = nullptr);

    /// @brief Destructor
    ~EditorRenderPipeline() override;

    // =========================================================================
    // Text Source (Stage 1)
    // =========================================================================

    /// @brief Set the text source
    /// @param source Text source adapter (takes ownership)
    void setTextSource(std::unique_ptr<ITextSource> source);

    /// @brief Get current text source
    /// @return Pointer to text source, or nullptr if not set
    ITextSource* textSource() const { return m_textSource.get(); }

    /// @brief Check if text source is set
    bool hasTextSource() const { return m_textSource != nullptr; }

    // =========================================================================
    // Render Context (Stage 2)
    // =========================================================================

    /// @brief Set the complete render context
    /// @param context New rendering configuration
    void setContext(const RenderContext& context);

    /// @brief Get current render context
    /// @return Reference to current context
    const RenderContext& context() const { return m_context; }

    /// @brief Get mutable context (for in-place modifications)
    /// @return Reference to context
    /// @note Call markDirty() after modifications
    RenderContext& context() { return m_context; }

    // =========================================================================
    // Context Shortcuts (commonly modified properties)
    // =========================================================================

    /// @brief Set margins
    void setMargins(double left, double top, double right, double bottom);
    void setMargins(const RenderMargins& margins);

    /// @brief Set zoom level and mode
    /// @param factor Zoom factor (1.0 = 100%, range 0.25-4.0)
    /// @param mode How zoom should be applied
    void setZoom(double factor, ZoomMode mode);

    /// @brief Get current zoom factor
    double zoomFactor() const { return m_context.zoomFactor; }

    /// @brief Get current zoom mode
    ZoomMode zoomMode() const { return m_context.zoomMode; }

    /// @brief Set text width
    /// @param width Available width for text (pixels)
    void setTextWidth(double width);

    /// @brief Set font
    void setFont(const QFont& font);

    /// @brief Set text color
    void setTextColor(const QColor& color);

    /// @brief Set background color
    void setBackgroundColor(const QColor& color);

    /// @brief Set view mode
    void setViewMode(ViewMode mode);

    /// @brief Set scroll position
    void setScrollY(double y);

    /// @brief Set viewport size
    void setViewportSize(const QSizeF& size);

    /// @brief Set screen DPI for WYSIWYG page geometry
    /// @param dpi Logical screen DPI (from screen()->logicalDotsPerInch())
    void setScreenDpi(double dpi);

    // =========================================================================
    // Configuration (Phase 14: called when settings change)
    // =========================================================================

    /// @brief Configure pipeline with new context
    /// Called when view mode, zoom, appearance, or viewport changes.
    /// Performs ALL calculations once (DPI, margins, text width, page layout).
    /// @deprecated Use granular setters (Phase 15) for better performance
    void configure(const RenderContext& context);

    // =========================================================================
    // Granular Configuration (Phase 15: targeted updates)
    // =========================================================================
    // These setters update static configuration and recalculate ONLY
    // the values that depend on what changed. Much faster than configure().

    /// @brief Set screen DPI (recalculates: mmToPixels, pageLayout, textWidth)
    /// @param dpi Logical screen DPI (from screen()->logicalDotsPerInch())
    void setConfigDpi(double dpi);

    /// @brief Set base font (recalculates: effectiveFont)
    /// @param font User's selected font
    void setConfigFont(const QFont& font);

    /// @brief Set zoom level (recalculates: effectiveFont, viewScale)
    /// @param factor Zoom factor (1.0 = 100%)
    /// @param mode How zoom is applied (FontScaling or PageScaling)
    void setConfigZoom(double factor, ZoomMode mode);

    /// @brief Set the view typography (recalculates: typography)
    /// @param typography Line spacing, paragraph spacing and first-line indent; lengths
    ///                   in pixels at 100% zoom
    void setConfigTypography(const LayoutTypography& typography);

    /// @brief Set viewport size (recalculates: textWidth or pageCenterOffset)
    /// @param size Widget size in pixels
    void setConfigViewportSize(const QSizeF& size);

    /// @brief Set margins in pixels (recalculates: textWidth)
    /// @param left Left margin in pixels
    /// @param top Top margin in pixels
    /// @param right Right margin in pixels
    /// @param bottom Bottom margin in pixels
    void setConfigMargins(double left, double top, double right, double bottom);

    /// @brief Set page layout parameters (recalculates: pageLayout, textWidth)
    /// @param pageSize Page size in points (72 DPI)
    /// @param pageGap Gap between pages in pixels
    void setConfigPageLayout(const QSizeF& pageSize, double pageGap);

    /// @brief Set colors (no recalculation, just marks dirty)
    /// @param colors Render colors (text, background, selection, etc.)
    void setConfigColors(const RenderColors& colors);

    /// @brief Set view mode (FULL reconfiguration - use sparingly)
    /// @param mode New view mode
    /// @note This triggers full reconfiguration because view mode affects everything
    void setConfigViewMode(ViewMode mode);

    /// @brief Apply initial configuration (called once after setup)
    /// Sets up initial state without full configure() overhead
    void applyInitialConfig();

    // =========================================================================
    // Lightweight updates (called frequently)
    // =========================================================================

    /// @brief Update cursor state only
    void updateCursor(const CursorPosition& pos, bool visible, bool blinkState);

    /// @brief Update selection only
    void updateSelection(const SelectionRange& selection);

    /// @brief Update scroll position only
    void updateScroll(double scrollY);

    // =========================================================================
    // Cursor & Selection
    // =========================================================================

    /// @brief Set cursor position
    void setCursorPosition(const CursorPosition& position);

    /// @brief Get cursor position
    const CursorPosition& cursorPosition() const { return m_cursorPosition; }

    /// @brief Set cursor visibility
    void setCursorVisible(bool visible);

    /// @brief Set cursor blink state (BookEditor's blink timer drives it)
    void setCursorBlinkState(bool on);

    /// @brief Set cursor style (Line, Block, Underline)
    void setCursorStyle(CursorStyle style);

    /// @brief Set the width of the line cursor and the drop caret, in pixels
    ///
    /// The same at every zoom, like the caret of a word processor.
    void setCursorWidth(double width);

    /// @brief Set selection range
    void setSelection(const SelectionRange& selection);

    /// @brief Get selection range
    const SelectionRange& selection() const { return m_selection; }

    /// @brief Check if there is an active selection
    bool hasSelection() const;

    /// @brief Clear selection
    void clearSelection();

    /// @brief Get cursor rectangle in widget coordinates
    QRectF cursorRect() const;

    /// @brief Rectangle of a caret line at a position, in widget coordinates
    ///
    /// Scroll-mode geometry, like cursorRect().
    QRectF caretRect(const CursorPosition& position) const;

    /// @brief Show where dragged text would be dropped (std::nullopt hides it)
    ///
    /// Painted as a line caret in the cursor color, independent of the cursor's
    /// visibility and blinking.
    void setDropCaret(const std::optional<CursorPosition>& position);

    /// @brief Position shown by the drop caret, if any
    const std::optional<CursorPosition>& dropCaret() const { return m_dropCaret; }

    /// @brief Area the cursor is painted in (cursorRect() adjusted to the cursor style)
    ///
    /// Scroll-mode geometry; also the area to repaint when the cursor blinks or moves.
    QRectF cursorPaintRect() const;

    // =========================================================================
    // Integration with other components
    // =========================================================================

    /// @brief Set viewport manager for scroll coordination
    /// @param viewport ViewportManager instance (not owned)
    void setViewportManager(ViewportManager* viewport);

    /// @brief Set search engine for highlight rendering
    /// @param engine SearchEngine instance (not owned)
    void setSearchEngine(SearchEngine* engine);

    // =========================================================================
    // Main Render Entry Point (Stage 3+4)
    // =========================================================================

    /// @brief Render the document
    /// @param painter QPainter to draw with
    /// @param clipRect Area to render (widget coordinates)
    ///
    /// This is the SINGLE entry point for all rendering.
    /// Pipeline stages:
    /// 1. Get visible paragraph range
    /// 2. Ensure layouts exist for visible paragraphs
    /// 3. Render background
    /// 4. Render paragraphs (text with formatting)
    /// 5. Render selection highlights
    /// 6. Render search highlights
    /// 7. Render cursor
    /// 8. Render overlays (focus mode, markers)
    void render(QPainter* painter, const QRect& clipRect);

    // =========================================================================
    // Dirty Region Tracking
    // =========================================================================

    /// @brief Mark entire viewport as needing repaint
    /// @note Also used when layout/pagination may have changed.
    ///       For color-only changes, use markRepaintOnly() instead.
    void markAllDirty();

    /// @brief Mark viewport for repaint without implying layout invalidation
    /// @note Use this for color-only changes where pagination cache and
    ///       layout remain valid. Semantically equivalent to markAllDirty()
    ///       but signals to callers that no cache rebuild is needed.
    void markRepaintOnly();

    /// @brief Mark specific region as needing repaint
    void markDirty(const QRect& region);

    /// @brief Mark paragraph as needing repaint
    void markParagraphDirty(size_t paragraphIndex);

    /// @brief Check if any region needs repaint
    bool isDirty() const { return !m_dirtyRegion.isEmpty(); }

    /// @brief Get dirty region
    QRegion dirtyRegion() const { return m_dirtyRegion; }

    /// @brief Clear dirty region (after painting)
    void clearDirtyRegion();

    // =========================================================================
    // Pagination (Phase 13.3: Page Mode support)
    // =========================================================================

    /// @brief Get cached pages for Page Mode rendering
    /// @return Vector of PageContent with page geometry and paragraph slices
    const std::vector<PageContent>& pages() const;

    /// @brief Check if pagination cache is valid
    bool isPaginationValid() const { return m_paginationCacheValid; }

    /// @brief Invalidate pagination cache (call when layout changes)
    void invalidatePagination();

    /// @brief Set page layout parameters for Page Mode
    /// @param pageSize Page size in points (e.g., A4 = 595x842)
    /// @param pageGap Gap between pages in pixels
    void setPageLayout(const QSizeF& pageSize, double pageGap = 20.0);

    /// @brief Find page index at given document Y coordinate
    /// @param docY Y coordinate in document space
    /// @return Page index (0-based), or -1 if not found
    int pageAtY(double docY) const;

    /// @brief Find position (paragraph, offset) at widget point
    /// @param point Point in widget coordinates
    /// @return CursorPosition at point, or invalid position if not found
    CursorPosition positionFromPoint(const QPointF& point) const;

signals:
    /// @brief Emitted when repaint is needed
    /// @param region Area to repaint
    void repaintRequested(const QRegion& region);

    /// @brief Emitted when document height changes
    /// @param newHeight New total height
    void documentHeightChanged(double newHeight);

private:
    // =========================================================================
    // Internal Render Methods (Stage 4)
    // =========================================================================

    /// @brief Render background
    void renderBackground(QPainter* painter, const QRect& clipRect);

    /// @brief Render text frame border
    void renderTextFrameBorder(QPainter* painter);

    /// @brief Render visible paragraphs
    void renderParagraphs(QPainter* painter, const QRect& clipRect);

    /// @brief Render single paragraph
    void renderParagraph(QPainter* painter, size_t index, double widgetY);

    /// @brief Render selection highlights
    void renderSelection(QPainter* painter, const QRect& clipRect);

    /// @brief Render selection for single paragraph
    void renderParagraphSelection(QPainter* painter, size_t paraIndex,
                                   int startOffset, int endOffset, double widgetY);

    /// @brief Fill the background of a text range of one paragraph, line by line
    /// @param lineBoxes Whole line boxes (lines join up) instead of the text height
    void fillTextRange(QPainter* painter, size_t paraIndex, int startOffset, int endOffset,
                       double widgetY, const QColor& color, bool lineBoxes);

    /// @brief Render search highlights of the visible paragraphs (scroll modes, under the text)
    void renderSearchHighlights(QPainter* painter);

    /// @brief Render cursor
    void renderCursor(QPainter* painter);

    /// @brief Render focus mode overlay
    void renderFocusOverlay(QPainter* painter, const QRect& clipRect);

    /// @brief Render marker highlights (comments, TODOs, notes)
    void renderMarkerHighlights(QPainter* painter, const QRect& clipRect);

    /// @brief Render comment highlights
    void renderCommentHighlights(QPainter* painter, const QRect& clipRect);

    // =========================================================================
    // Scroll Mode Rendering (Phase 15: Viewport-culled fast path)
    // =========================================================================

    /// @brief Render in Scroll Mode using viewport culling (O(visible) not O(n))
    ///
    /// For non-Page modes (Continuous, Focus, DistractionFree, Typewriter),
    /// renders only visible paragraphs using firstVisibleParagraph/lastVisibleParagraph
    /// instead of iterating ALL paragraph slices.
    /// This reduces draw calls from ~3000 to ~30 for large documents.
    void renderScrollMode(QPainter* painter, const QRect& clipRect);

    // =========================================================================
    // Page Mode Rendering (Phase 13.4: Unified rendering)
    // =========================================================================

    /// @brief Render page backgrounds, shadows, and borders (Page Mode only)
    void renderPageBackgrounds(QPainter* painter, const QRect& clipRect);

    /// @brief Render a single paragraph slice on a page
    /// @param painter QPainter to draw with
    /// @param slice The paragraph slice to render
    /// @param textRect Text area rectangle in widget coordinates
    void renderSlice(QPainter* painter, const ParagraphSlice& slice, const QRectF& textRect);

    /// @brief Render selection highlight for a slice
    /// @param painter QPainter to draw with
    /// @param slice The paragraph slice
    /// @param textRect Text area rectangle in widget coordinates
    void renderSliceSelection(QPainter* painter, const ParagraphSlice& slice, const QRectF& textRect);

    /// @brief Render the cursor and the drop caret within a slice
    /// @param painter QPainter to draw with
    /// @param slice The paragraph slice
    /// @param textRect Text area rectangle in widget coordinates
    void renderSliceCursor(QPainter* painter, const ParagraphSlice& slice, const QRectF& textRect);

    /// @brief Render a caret at a position within a slice (nothing if it is not in the slice)
    void renderSliceCaret(QPainter* painter, const ParagraphSlice& slice, const QRectF& textRect,
                          const CursorPosition& position, CursorStyle style, const QColor& color);

    /// @brief Width of the character at a position, in layout units
    ///
    /// Block and underline cursors are as wide. At the end of a line, where there is no
    /// character, the average character width.
    double caretCharWidth(const CursorPosition& position) const;

    // =========================================================================
    // Layout Helpers (Stage 3)
    // =========================================================================

    /// @brief Calculate visible paragraph range from scroll position
    void updateVisibleRange();

    /// @brief Get widget Y coordinate for paragraph
    double paragraphWidgetY(size_t index) const;

    /// @brief Get text rectangle for character range
    QRectF getTextRect(size_t paraIndex, int offset, int length) const;

    // =========================================================================
    // State
    // =========================================================================

    std::unique_ptr<ITextSource> m_textSource;  ///< Text content source
    RenderContext m_context;                     ///< All rendering configuration

    // Cursor and selection
    CursorPosition m_cursorPosition;             ///< Current cursor position
    SelectionRange m_selection;                  ///< Current selection
    std::optional<CursorPosition> m_dropCaret;   ///< Drop point of dragged text

    // External components (not owned)
    ViewportManager* m_viewportManager = nullptr;
    SearchEngine* m_searchEngine = nullptr;

    // Dirty tracking
    QRegion m_dirtyRegion;

    CursorStyle m_cursorStyle = CursorStyle::Line;  ///< Cursor style

    // Cached values
    mutable double m_cachedTotalHeight = 0.0;
    mutable bool m_heightDirty = true;

    // Pagination cache (Phase 13.3)
    mutable std::vector<PageContent> m_cachedPages;     ///< Cached page layout
    mutable bool m_paginationCacheValid = false;        ///< Is pagination cache valid?
    mutable double m_cachedScreenDpi = DEFAULT_DPI;     ///< Screen DPI used for cache
    mutable QSizeF m_cachedPageSize;                    ///< Page size used for cache
    double m_pageGap = 20.0;                            ///< Gap between pages in pixels

    /// @brief Box of a laid out line: the line plus its share of the line spacing
    QRectF lineBox(const QTextLine& line) const;

    /// @brief Top of a slice within its block (the box top of its first line)
    double sliceTopInBlock(const QTextLayout& layout, const ParagraphSlice& slice) const;

    /// @brief Rebuild pagination cache if needed
    void rebuildPaginationCache() const;

    // =========================================================================
    // Internal Calculation Methods (Phase 14: called by configure())
    // =========================================================================

    /// @brief Calculate DPI-derived values
    void computeDpiScaling();

    /// @brief Calculate effective margins for current view mode (SINGLE PLACE!)
    void computeMargins();

    /// @brief Calculate text width
    void computeTextWidth();

    /// @brief Calculate page layout for Page Mode
    void computePageLayout();

    /// @brief Calculate effective font
    void computeEffectiveFont();

    /// @brief Calculate the typography handed to the layout (reference font size)
    void computeTypography();

    /// @brief Apply computed values to text source
    void applyComputedToSource();

    // =========================================================================
    // Targeted Apply Methods (Phase 15: granular updates)
    // =========================================================================

    /// @brief Apply only font to text source (no width change)
    void applyFontToSource();

    /// @brief Apply only text width to text source (no font change)
    void applyWidthToSource();

    /// @brief Apply only the typography to the text source
    void applyTypographyToSource();

    /// @brief Recalculate only page center offset (for resize in Page mode)
    void recalcPageCenterOffset();
};

}  // namespace kalahari::editor
