/// @file editor_render_pipeline.h
/// @brief Unified rendering pipeline for BookEditor (OpenSpec #00043 Phase 12.1)
///
/// EditorRenderPipeline consolidates all rendering logic into a single class with
/// one entry point: render(painter, clipRect). This replaces the scattered rendering
/// paths in BookEditor, RenderEngine, and ViewportManager.
///
/// Pipeline stages:
/// 1. TEXT      - Get content from ITextSource (the QTextDocument)
/// 2. ATTRIBUTES - Apply RenderContext (font, colors, margins, scale)
/// 3. LAYOUT    - Calculate block positions (using KalahariTextDocumentLayout)
/// 4. RENDER    - Draw to painter (text, cursor, selection, overlays)

#pragma once

#include <kalahari/editor/text_source_adapter.h>
#include <kalahari/editor/render_context.h>
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/text_highlight.h>
#include <QObject>
#include <QRect>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class QPainter;
class QTextDocument;

namespace kalahari::editor {

// Forward declarations
class ViewportManager;
class SearchEngine;
class KalahariTextDocumentLayout;

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
/// - One mapping between document and widget coordinates for every view mode: the text
///   layout places the lines on the pages (page flow), the pipeline only offsets and
///   scales them (widget = origin + (document - scroll) * viewScale)
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

    /// @brief Set the page view's widget pixels per layout pixel at zoom 100% (recalculates:
    ///        viewScale; see RenderContext::paperScale)
    void setConfigPaperScale(double scale);

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
    /// @param marginsMm Page margins in millimetres
    /// @param pageGap Gap between and around pages, in pixels at 100% zoom
    void setConfigPageLayout(const QSizeF& pageSize, const QMarginsF& marginsMm, double pageGap);

    /// @brief Show or hide the page numbers (repaint only)
    void setConfigShowPageNumbers(bool show);

    /// @brief Set typewriter scrolling (recalculates: origin and scroll padding)
    /// @param enabled Keep the cursor line at a fixed height of the view
    /// @param focusPosition That height as a share of the view height (0 = top)
    void setConfigTypewriter(bool enabled, double focusPosition);

    /// @brief Turn Focus on or off: every paragraph but the cursor's is dimmed (repaint only)
    void setConfigFocus(bool enabled);

    /// @brief Set the horizontal scroll offset in pixels (page mode, zoomed page wider
    ///        than the view; clamped to [0, maxScrollX()])
    void setConfigScrollX(double x);

    /// @brief Set the width of the vertical scroll bar over the view's right edge; the
    ///        pages are centred, and scrolled sideways, in the width left of it
    void setConfigScrollBarWidth(double width);

    /// @brief Set colors (no recalculation, just marks dirty)
    /// @param colors Render colors (text, background, selection, etc.)
    void setConfigColors(const RenderColors& colors);

    /// @brief Set view mode (FULL reconfiguration - use sparingly)
    /// @param mode New view mode
    /// @param zoomMode How the zoom applies in that mode (one relayout for both)
    /// @note This triggers full reconfiguration because view mode affects everything
    void setConfigViewMode(ViewMode mode, ZoomMode zoomMode);

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
    /// Also the area to repaint when the cursor blinks or moves.
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

    /// @brief Highlight the word being read aloud
    /// @param paragraph Paragraph of the word
    /// @param offset First character of the word in the paragraph
    /// @param length Length of the word; 0 clears the highlight
    void setSpokenWord(int paragraph, int offset, int length);

    // =========================================================================
    // Main Render Entry Point (Stage 3+4)
    // =========================================================================

    /// @brief Lay out the paragraphs in view (the others keep estimated heights)
    ///
    /// The scroll position may move while it runs: scroll anchoring keeps the text at the
    /// top of the view in place.
    void ensureVisibleLaidOut();

    /// @brief Render the document
    /// @param painter QPainter to draw with
    /// @param clipRect Area to render (widget coordinates)
    ///
    /// This is the SINGLE entry point for all rendering.
    /// Pipeline stages:
    /// 1. Get visible paragraph range
    /// 2. Ensure layouts exist for visible paragraphs
    /// 3. Render background (and the pages in page mode)
    /// 4. Render the selection and the highlight backgrounds (search matches, spoken word)
    /// 5. Render paragraphs (text with formatting)
    /// 6. Render the highlight marks (spelling and grammar waves)
    /// 7. Render cursor
    void render(QPainter* painter, const QRect& clipRect);

    // =========================================================================
    // Dirty Region Tracking
    // =========================================================================

    /// @brief Mark entire viewport as needing repaint
    /// @note Also used when the layout may have changed.
    ///       For color-only changes, use markRepaintOnly() instead.
    void markAllDirty();

    /// @brief Mark viewport for repaint without implying layout invalidation
    /// @note Use this for color-only changes where the layout remains valid.
    ///       Semantically equivalent to markAllDirty() but tells callers that
    ///       nothing needs to be laid out again.
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
    // Geometry: document <-> widget, pages
    // =========================================================================

    /// @brief Widget position of a document position (scroll and zoom applied)
    QPointF documentToWidget(const QPointF& point) const;

    /// @brief Document position shown at a widget position
    QPointF widgetToDocument(const QPointF& point) const;

    /// @brief Number of pages (page mode; 1 in the other modes)
    int pageCount() const;

    /// @brief Page (0-based) whose sheet holds document y, or the nearest page
    int pageAtDocumentY(double y) const;

    /// @brief Document y of the top of a page's text area
    double pageTextTop(int page) const;

    /// @brief Largest horizontal scroll offset (pixels; 0 when the pages fit the view)
    double maxScrollX() const;

    /// @brief Find position (paragraph, offset) at widget point
    /// @param point Point in widget coordinates
    /// @return CursorPosition at point (the nearest one for a point off the text)
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

    /// @brief Part of a text range on one line: the line and the range's x extent on it
    ///        (layout coordinates of the paragraph)
    struct LinePiece {
        QTextLine line;
        qreal x1 = 0.0;
        qreal x2 = 0.0;
    };

    /// @brief Parts of a text range of one paragraph, one per line it runs on
    std::vector<LinePiece> linePieces(size_t paraIndex, int startOffset, int endOffset) const;

    /// @brief Fill the background of a text range of one paragraph, line by line
    /// @param lineBoxes Whole line boxes (lines join up) instead of the text height
    void fillTextRange(QPainter* painter, size_t paraIndex, int startOffset, int endOffset,
                       double widgetY, const QColor& color, bool lineBoxes);

    /// @brief Highlight of a paragraph (paragraph index, highlighted range)
    using ParagraphHighlight = std::pair<size_t, TextHighlight>;

    /// @brief Highlights of the visible paragraphs: the text source's check results (of the
    ///        paragraphs in the clip rect), search matches, spoken word
    std::vector<ParagraphHighlight> visibleHighlights(const QRect& clipRect) const;

    /// @brief Fill the highlight backgrounds (under the text)
    void renderHighlightBackgrounds(QPainter* painter,
                                    const std::vector<ParagraphHighlight>& highlights);

    /// @brief Draw the highlight marks (over the text): spelling and grammar waves
    void renderHighlightMarks(QPainter* painter,
                              const std::vector<ParagraphHighlight>& highlights);

    /// @brief Render cursor
    void renderCursor(QPainter* painter);

    /// @brief Render the visible text: selection, highlights, paragraphs, cursor and drop
    ///        caret (only the visible paragraphs: O(visible), not O(n))
    void renderText(QPainter* painter, const QRect& clipRect);

    /// @brief Render the sheets of the pages in the clip rect: shadow, paper, border,
    ///        text frame and page number (page mode)
    void renderPages(QPainter* painter, const QRect& clipRect);

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

    // =========================================================================
    // State
    // =========================================================================

    std::unique_ptr<ITextSource> m_textSource;  ///< Text content source
    RenderContext m_context;                     ///< All rendering configuration

    // Cursor and selection
    CursorPosition m_cursorPosition;             ///< Current cursor position
    SelectionRange m_selection;                  ///< Current selection
    std::optional<CursorPosition> m_dropCaret;   ///< Drop point of dragged text
    std::optional<ParagraphHighlight> m_spokenWord;  ///< Word being read aloud

    // External components (not owned)
    ViewportManager* m_viewportManager = nullptr;
    SearchEngine* m_searchEngine = nullptr;

    // Dirty tracking
    QRegion m_dirtyRegion;

    CursorStyle m_cursorStyle = CursorStyle::Line;  ///< Cursor style

    // Cached values
    mutable double m_cachedTotalHeight = 0.0;
    mutable bool m_heightDirty = true;

    /// @brief Box of a laid out line: the line plus its share of the line spacing
    QRectF lineBox(const QTextLine& line) const;

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

    /// @brief Apply the page flow (page mode: the text areas of the pages) to the source
    void applyPageFlowToSource();

    /// @brief Calculate the view mapping: origin, page position, scroll padding; also
    ///        handed to the viewport manager (view scale and top inset)
    void computeViewGeometry();
};

}  // namespace kalahari::editor
