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
#include <QTimer>
#include <memory>

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

    /// @brief Set scale factor
    /// @param scale Zoom factor (1.0 = 100%, 1.25 = 125%)
    void setScaleFactor(double scale);

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

    // =========================================================================
    // Cursor & Selection
    // =========================================================================

    /// @brief Set cursor position
    void setCursorPosition(const CursorPosition& position);

    /// @brief Get cursor position
    const CursorPosition& cursorPosition() const { return m_cursorPosition; }

    /// @brief Set cursor visibility
    void setCursorVisible(bool visible);

    /// @brief Set cursor blink state
    void setCursorBlinkState(bool on);

    /// @brief Set cursor style (Line, Block, Underline)
    void setCursorStyle(int style);

    /// @brief Start cursor blink timer
    void startCursorBlink();

    /// @brief Stop cursor blink timer
    void stopCursorBlink();

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
    void markAllDirty();

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

signals:
    /// @brief Emitted when repaint is needed
    /// @param region Area to repaint
    void repaintRequested(const QRegion& region);

    /// @brief Emitted when cursor blink state changes
    /// @param visible Current blink state
    void cursorBlinkChanged(bool visible);

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

    /// @brief Render search highlights
    void renderSearchHighlights(QPainter* painter, const QRect& clipRect);

    /// @brief Render cursor
    void renderCursor(QPainter* painter);

    /// @brief Render focus mode overlay
    void renderFocusOverlay(QPainter* painter, const QRect& clipRect);

    /// @brief Render marker highlights (comments, TODOs, notes)
    void renderMarkerHighlights(QPainter* painter, const QRect& clipRect);

    /// @brief Render comment highlights
    void renderCommentHighlights(QPainter* painter, const QRect& clipRect);

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

    // External components (not owned)
    ViewportManager* m_viewportManager = nullptr;
    SearchEngine* m_searchEngine = nullptr;

    // Dirty tracking
    QRegion m_dirtyRegion;

    // Cursor blinking
    QTimer m_cursorBlinkTimer;
    int m_cursorStyle = 0;  ///< 0=Line, 1=Block, 2=Underline

    // Cached values
    mutable double m_cachedTotalHeight = 0.0;
    mutable bool m_heightDirty = true;

private slots:
    /// @brief Handle cursor blink timer timeout
    void onCursorBlinkTimeout();
};

}  // namespace kalahari::editor
