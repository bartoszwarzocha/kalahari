/// @file editor_render_pipeline.cpp
/// @brief Implementation of unified rendering pipeline (OpenSpec #00043 Phase 12.1)

#include <kalahari/editor/editor_render_pipeline.h>
#include <kalahari/editor/viewport_manager.h>
#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/kml_format_registry.h>
#include <kalahari/core/logger.h>
#include <QPainter>
#include <QTextLayout>
#include <QTextLine>
#include <QTextBlock>
#include <QTextFragment>
#include <algorithm>
#include <cmath>

namespace kalahari::editor {

// =============================================================================
// Constructor / Destructor
// =============================================================================

EditorRenderPipeline::EditorRenderPipeline(QObject* parent)
    : QObject(parent) {
    // Initialize with default context
    m_context.font = QFont("Segoe UI", 11);

    // Setup cursor blink timer
    connect(&m_cursorBlinkTimer, &QTimer::timeout,
            this, &EditorRenderPipeline::onCursorBlinkTimeout);
}

EditorRenderPipeline::~EditorRenderPipeline() {
    m_cursorBlinkTimer.stop();
}

// =============================================================================
// Text Source (Stage 1)
// =============================================================================

void EditorRenderPipeline::setTextSource(std::unique_ptr<ITextSource> source) {
    m_textSource = std::move(source);
    m_heightDirty = true;
    m_paginationCacheValid = false;
    markAllDirty();
}

// =============================================================================
// Render Context (Stage 2)
// =============================================================================

void EditorRenderPipeline::setContext(const RenderContext& context) {
    bool fontChanged = (m_context.font != context.font);
    bool widthChanged = (m_context.textWidth != context.textWidth);
    bool marginsChanged = (m_context.margins != context.margins);
    bool viewModeChanged = (m_context.viewMode != context.viewMode);
    bool dpiChanged = (std::abs(m_context.computed.dpiScale - context.computed.dpiScale) > 0.001);

    m_context = context;

    // Update text source if font, width, or margins changed
    if (m_textSource && (fontChanged || widthChanged || marginsChanged)) {
        m_textSource->setFont(m_context.font);
        m_textSource->setTextWidth(m_context.computed.textWidth);
        m_heightDirty = true;
    }

    // Invalidate pagination if relevant settings changed
    if (fontChanged || widthChanged || marginsChanged || viewModeChanged || dpiChanged) {
        m_paginationCacheValid = false;
    }

    markAllDirty();
}

void EditorRenderPipeline::setMargins(double left, double top, double right, double bottom) {
    RenderMargins newMargins{left, top, right, bottom};
    if (m_context.margins == newMargins) {
        return;  // No change, skip expensive relayout
    }
    m_context.margins = newMargins;
    computeMargins();
    computeTextWidth();
    if (m_textSource) {
        m_textSource->setTextWidth(m_context.computed.textWidth);
        m_heightDirty = true;
    }
    m_paginationCacheValid = false;
    markAllDirty();
}

void EditorRenderPipeline::setMargins(const RenderMargins& margins) {
    setMargins(margins.left, margins.top, margins.right, margins.bottom);
}

void EditorRenderPipeline::setZoom(double factor, ZoomMode mode) {
    factor = qBound(0.25, factor, 4.0);  // Limit 25% to 400%

    bool modeChanged = (m_context.zoomMode != mode);
    bool factorChanged = (std::abs(m_context.zoomFactor - factor) > 0.001);

    if (!modeChanged && !factorChanged) {
        return;  // No change
    }

    m_context.zoomFactor = factor;
    m_context.zoomMode = mode;

    // Recalculate computed values
    computeDpiScaling();
    computeEffectiveFont();
    computeTextWidth();

    if (m_textSource) {
        if (mode == ZoomMode::FontScaling) {
            // Font scaling: change font size, trigger layout recalculation
            m_textSource->setFont(m_context.computed.effectiveFont);
        } else {
            // Page scaling: keep base font
            m_textSource->setFont(m_context.font);
        }
        m_textSource->setTextWidth(m_context.computed.textWidth);
        m_heightDirty = true;
    }

    m_paginationCacheValid = false;
    markAllDirty();
}

void EditorRenderPipeline::setTextWidth(double width) {
    if (m_context.textWidth != width) {
        m_context.textWidth = width;
        computeTextWidth();
        if (m_textSource) {
            m_textSource->setTextWidth(m_context.computed.textWidth);
            m_heightDirty = true;
        }
        m_paginationCacheValid = false;
        markAllDirty();
    }
}

void EditorRenderPipeline::setFont(const QFont& font) {
    if (m_context.font != font) {
        m_context.font = font;
        if (m_textSource) {
            m_textSource->setFont(font);
            m_heightDirty = true;
        }
        m_paginationCacheValid = false;
        markAllDirty();
    }
}

void EditorRenderPipeline::setTextColor(const QColor& color) {
    if (m_context.colors.text != color) {
        m_context.colors.text = color;
        markRepaintOnly();  // Color-only change, no layout invalidation needed
    }
}

void EditorRenderPipeline::setBackgroundColor(const QColor& color) {
    if (m_context.colors.background != color) {
        m_context.colors.background = color;
        markRepaintOnly();  // Color-only change, no layout invalidation needed
    }
}

void EditorRenderPipeline::setViewMode(ViewMode mode) {
    if (m_context.viewMode != mode) {
        m_context.viewMode = mode;
        m_paginationCacheValid = false;
        markAllDirty();
    }
}

void EditorRenderPipeline::setScrollY(double y) {
    m_context.scrollY = y;
    updateVisibleRange();
}

void EditorRenderPipeline::setViewportSize(const QSizeF& size) {
    if (m_context.viewportSize == size) {
        return;  // No change
    }
    m_context.viewportSize = size;
    m_paginationCacheValid = false;  // Page centering changes with viewport
    updateVisibleRange();
}

void EditorRenderPipeline::setScreenDpi(double dpi) {
    if (dpi < 1.0) dpi = DEFAULT_DPI;  // Sanity check

    if (std::abs(m_context.screenDpi - dpi) < 0.001) {
        return;  // No change
    }

    m_context.screenDpi = dpi;

    // Relayout with new DPI scale
    if (m_textSource) {
        m_textSource->setFont(m_context.computed.effectiveFont);
        m_heightDirty = true;
    }

    m_paginationCacheValid = false;
    markAllDirty();
}

// =============================================================================
// Configuration (Phase 14)
// =============================================================================

void EditorRenderPipeline::configure(const RenderContext& context) {
    m_context = context;

    // Perform ALL calculations in order
    computeDpiScaling();
    computeEffectiveFont();
    computeMargins();
    computePageLayout();
    computeTextWidth();
    applyComputedToSource();

    // Invalidate caches
    m_paginationCacheValid = false;
    m_heightDirty = true;
    markAllDirty();
}

void EditorRenderPipeline::computeDpiScaling() {
    m_context.computed.dpiScale = m_context.screenDpi / DEFAULT_DPI;
    m_context.computed.mmToPixels = m_context.screenDpi / 25.4;
    m_context.computed.totalScale = m_context.computed.dpiScale * m_context.zoomFactor;

    // viewScale depends on zoom mode
    if (m_context.zoomMode == ZoomMode::PageScaling) {
        m_context.computed.viewScale = m_context.computed.totalScale;
    } else {
        m_context.computed.viewScale = 1.0;  // FontScaling: scale in font, not painter
    }
}

void EditorRenderPipeline::computeEffectiveFont() {
    if (m_context.zoomMode == ZoomMode::FontScaling) {
        m_context.computed.effectiveFont = m_context.font;
        m_context.computed.effectiveFont.setPointSizeF(
            m_context.font.pointSizeF() * m_context.computed.totalScale);
    } else {
        m_context.computed.effectiveFont = m_context.font;
    }
}

void EditorRenderPipeline::computeMargins() {
    // Margins are passed pre-calculated in pixels from BookEditor
    switch (m_context.viewMode) {
        case ViewMode::Page:
        case ViewMode::Typewriter: {
            // Page margins from pageMode config (already in pixels from BookEditor)
            m_context.computed.marginLeft = m_context.margins.left;
            m_context.computed.marginRight = m_context.margins.right;
            m_context.computed.marginTop = m_context.margins.top;
            m_context.computed.marginBottom = m_context.margins.bottom;
            break;
        }
        default:
            // View margins (already in pixels)
            m_context.computed.marginLeft = m_context.margins.left;
            m_context.computed.marginRight = m_context.margins.right;
            m_context.computed.marginTop = m_context.margins.top;
            m_context.computed.marginBottom = m_context.margins.bottom;
            break;
    }
}

void EditorRenderPipeline::computePageLayout() {
    if (m_context.viewMode != ViewMode::Page) {
        return;
    }

    double dpiScale = m_context.computed.dpiScale;

    // Page size in pixels
    m_context.computed.pageWidthPixels = m_context.pageMode.pageSize.width() * dpiScale;
    m_context.computed.pageHeightPixels = m_context.pageMode.pageSize.height() * dpiScale;

    // Text area height
    m_context.computed.textAreaHeight = m_context.computed.pageHeightPixels
                                       - m_context.computed.marginTop
                                       - m_context.computed.marginBottom;

    // Center offset for page
    double viewportWidth = m_context.viewportSize.width();
    m_context.computed.pageCenterOffset =
        (viewportWidth > m_context.computed.pageWidthPixels)
        ? (viewportWidth - m_context.computed.pageWidthPixels) / 2.0
        : 0.0;
}

void EditorRenderPipeline::computeTextWidth() {
    if (m_context.viewMode == ViewMode::Page) {
        // Page Mode: text width from page size minus margins
        m_context.computed.textWidth = m_context.computed.pageWidthPixels
                                      - m_context.computed.marginLeft
                                      - m_context.computed.marginRight;
    } else {
        // Scroll modes: viewport width minus margins
        m_context.computed.textWidth = m_context.viewportSize.width()
                                      - m_context.computed.marginLeft
                                      - m_context.computed.marginRight;
    }
}

void EditorRenderPipeline::applyComputedToSource() {
    if (!m_textSource) return;

    m_textSource->setFont(m_context.computed.effectiveFont);
    m_textSource->setTextWidth(m_context.computed.textWidth);
}

// =============================================================================
// Targeted Apply Methods (Phase 15)
// =============================================================================

void EditorRenderPipeline::applyFontToSource() {
    if (!m_textSource) return;
    m_textSource->setFont(m_context.computed.effectiveFont);
}

void EditorRenderPipeline::applyWidthToSource() {
    if (!m_textSource) return;
    m_textSource->setTextWidth(m_context.computed.textWidth);
}

void EditorRenderPipeline::recalcPageCenterOffset() {
    double viewportWidth = m_context.viewportSize.width();
    m_context.computed.pageCenterOffset =
        (viewportWidth > m_context.computed.pageWidthPixels)
        ? (viewportWidth - m_context.computed.pageWidthPixels) / 2.0
        : 0.0;
}

// =============================================================================
// Granular Configuration (Phase 15)
// =============================================================================

void EditorRenderPipeline::setConfigDpi(double dpi) {
    if (std::abs(m_context.screenDpi - dpi) < 0.01) return;  // No change

    m_context.screenDpi = dpi;
    computeDpiScaling();

    if (m_context.viewMode == ViewMode::Page) {
        computePageLayout();
    }
    computeTextWidth();

    if (m_context.zoomMode == ZoomMode::PageScaling) {
        computeEffectiveFont();
        applyFontToSource();
    }

    applyWidthToSource();
    m_paginationCacheValid = false;
    markAllDirty();
}

void EditorRenderPipeline::setConfigFont(const QFont& font) {
    if (m_context.font == font) return;  // No change

    m_context.font = font;
    computeEffectiveFont();
    applyFontToSource();
    m_paginationCacheValid = false;
    m_heightDirty = true;
    markAllDirty();
}

void EditorRenderPipeline::setConfigZoom(double factor, ZoomMode mode) {
    if (std::abs(m_context.zoomFactor - factor) < 0.001 &&
        m_context.zoomMode == mode) return;  // No change

    m_context.zoomFactor = factor;
    m_context.zoomMode = mode;

    computeDpiScaling();  // Recalculates totalScale, viewScale
    computeEffectiveFont();
    applyFontToSource();

    m_paginationCacheValid = false;
    m_heightDirty = true;
    markAllDirty();
}

void EditorRenderPipeline::setConfigViewportSize(const QSizeF& size) {
    if (m_context.viewportSize == size) return;  // No change

    m_context.viewportSize = size;

    if (m_context.viewMode == ViewMode::Page) {
        // Page mode: only center offset changes, text width stays same
        recalcPageCenterOffset();
    } else {
        // Scroll modes: text width depends on viewport
        computeTextWidth();
        applyWidthToSource();
        m_paginationCacheValid = false;
        m_heightDirty = true;
    }

    updateVisibleRange();
    markAllDirty();
}

void EditorRenderPipeline::setConfigMargins(double left, double top, double right, double bottom) {
    if (std::abs(m_context.computed.marginLeft - left) < 0.01 &&
        std::abs(m_context.computed.marginTop - top) < 0.01 &&
        std::abs(m_context.computed.marginRight - right) < 0.01 &&
        std::abs(m_context.computed.marginBottom - bottom) < 0.01) return;

    // Update BOTH input and computed to keep them in sync
    m_context.margins.left = left;
    m_context.margins.top = top;
    m_context.margins.right = right;
    m_context.margins.bottom = bottom;
    m_context.computed.marginLeft = left;
    m_context.computed.marginTop = top;
    m_context.computed.marginRight = right;
    m_context.computed.marginBottom = bottom;

    computeTextWidth();
    applyWidthToSource();

    if (m_context.viewMode == ViewMode::Page) {
        // Text area height changes with margins
        m_context.computed.textAreaHeight = m_context.computed.pageHeightPixels
                                           - top - bottom;
    }

    m_paginationCacheValid = false;
    m_heightDirty = true;
    markAllDirty();
}

void EditorRenderPipeline::setConfigPageLayout(const QSizeF& pageSize, double pageGap) {
    if (m_context.pageMode.pageSize == pageSize &&
        std::abs(m_pageGap - pageGap) < 0.01) return;

    m_context.pageMode.pageSize = pageSize;
    m_pageGap = pageGap;

    computePageLayout();
    computeTextWidth();
    applyWidthToSource();

    m_paginationCacheValid = false;
    m_heightDirty = true;
    markAllDirty();
}

void EditorRenderPipeline::setConfigColors(const RenderColors& colors) {
    m_context.colors = colors;
    // No layout recalculation needed - just repaint
    markRepaintOnly();
}

void EditorRenderPipeline::setConfigViewMode(ViewMode mode) {
    if (m_context.viewMode == mode) return;

    m_context.viewMode = mode;

    // View mode affects everything - full recalculation
    computeDpiScaling();
    computeEffectiveFont();
    computeMargins();
    computePageLayout();
    computeTextWidth();
    applyComputedToSource();

    m_paginationCacheValid = false;
    m_heightDirty = true;
    markAllDirty();
}

void EditorRenderPipeline::applyInitialConfig() {
    computeDpiScaling();
    computeEffectiveFont();
    computeMargins();
    computePageLayout();
    computeTextWidth();
    applyComputedToSource();

    m_paginationCacheValid = false;
    m_heightDirty = true;
    markAllDirty();
}

// Lightweight updates
void EditorRenderPipeline::updateCursor(const CursorPosition& pos, bool visible, bool blinkState) {
    m_cursorPosition = pos;
    m_context.cursor.visible = visible;
    m_context.cursor.blinkState = blinkState;
}

void EditorRenderPipeline::updateSelection(const SelectionRange& selection) {
    m_selection = selection;
}

void EditorRenderPipeline::updateScroll(double scrollY) {
    m_context.scrollY = scrollY;
}

// =============================================================================
// Cursor & Selection
// =============================================================================

void EditorRenderPipeline::setCursorPosition(const CursorPosition& position) {
    if (m_cursorPosition != position) {
        // Track old paragraph for focus mode optimization
        int oldParagraph = m_cursorPosition.paragraph;

        // Mark old cursor position dirty
        markDirty(cursorRect().toAlignedRect());

        m_cursorPosition = position;

        // Mark new cursor position dirty
        markDirty(cursorRect().toAlignedRect());

        // Update focus mode if enabled
        if (m_context.focusMode.enabled) {
            int newParagraph = position.paragraph;
            m_context.focusMode.focusedParagraph = newParagraph;

            // Only mark affected paragraphs dirty, not entire viewport
            if (oldParagraph != newParagraph) {
                // Mark old paragraph (now dimmed) and new paragraph (now focused)
                markParagraphDirty(static_cast<size_t>(oldParagraph));
                markParagraphDirty(static_cast<size_t>(newParagraph));
            }
            // If same paragraph, cursor dirty regions are already marked
        }
    }
}

void EditorRenderPipeline::setCursorVisible(bool visible) {
    if (m_context.cursor.visible != visible) {
        m_context.cursor.visible = visible;
        markDirty(cursorRect().toAlignedRect());
    }
}

void EditorRenderPipeline::setCursorBlinkState(bool on) {
    if (m_context.cursor.blinkState != on) {
        m_context.cursor.blinkState = on;
        markDirty(cursorRect().toAlignedRect());
        emit cursorBlinkChanged(on);
    }
}

void EditorRenderPipeline::setCursorStyle(CursorStyle style) {
    if (m_cursorStyle != style) {
        markDirty(cursorRect().toAlignedRect());
        m_cursorStyle = style;
        markDirty(cursorRect().toAlignedRect());
    }
}

void EditorRenderPipeline::startCursorBlink() {
    if (m_context.cursor.blinkInterval > 0) {
        m_context.cursor.blinkState = true;
        m_cursorBlinkTimer.start(m_context.cursor.blinkInterval);
    }
}

void EditorRenderPipeline::stopCursorBlink() {
    m_cursorBlinkTimer.stop();
    m_context.cursor.blinkState = true;  // Keep cursor visible when not blinking
    markDirty(cursorRect().toAlignedRect());
}

void EditorRenderPipeline::onCursorBlinkTimeout() {
    m_context.cursor.blinkState = !m_context.cursor.blinkState;
    markDirty(cursorRect().toAlignedRect());
    emit cursorBlinkChanged(m_context.cursor.blinkState);
}

void EditorRenderPipeline::setSelection(const SelectionRange& selection) {
    if (m_selection.start != selection.start || m_selection.end != selection.end) {
        // Mark old selection dirty
        SelectionRange oldSel = m_selection.normalized();
        for (int i = oldSel.start.paragraph; i <= oldSel.end.paragraph; ++i) {
            markParagraphDirty(static_cast<size_t>(i));
        }

        m_selection = selection;

        // Mark new selection dirty
        SelectionRange newSel = m_selection.normalized();
        for (int i = newSel.start.paragraph; i <= newSel.end.paragraph; ++i) {
            markParagraphDirty(static_cast<size_t>(i));
        }
    }
}

bool EditorRenderPipeline::hasSelection() const {
    return m_selection.start != m_selection.end;
}

void EditorRenderPipeline::clearSelection() {
    setSelection(SelectionRange{});
}

QRectF EditorRenderPipeline::cursorRect() const {
    if (!m_textSource) {
        return QRectF();
    }

    int paraIndex = m_cursorPosition.paragraph;
    if (paraIndex < 0 || static_cast<size_t>(paraIndex) >= m_textSource->paragraphCount()) {
        return QRectF();
    }

    QTextLayout* layout = m_textSource->layout(static_cast<size_t>(paraIndex));
    if (!layout || layout->lineCount() == 0) {
        // Fallback: return default cursor rect
        double widgetY = paragraphWidgetY(static_cast<size_t>(paraIndex));
        QFontMetricsF fm(m_context.font);
        return QRectF(m_context.computed.marginLeft, widgetY,
                      m_context.cursor.width, fm.height());
    }

    // Find line containing cursor - O(log n) using Qt's binary search
    int offset = m_cursorPosition.offset;
    QTextLine line = layout->lineForTextPosition(offset);
    if (!line.isValid()) {
        line = layout->lineAt(layout->lineCount() - 1);
    }

    // Calculate cursor X position
    qreal cursorX = line.cursorToX(offset);

    // Convert to widget coordinates - Phase 14: use computed values
    double docY = m_textSource->paragraphY(static_cast<size_t>(paraIndex));
    double widgetY = m_context.computed.marginTop + (docY - m_context.scrollY) * m_context.computed.viewScale + line.y() * m_context.computed.viewScale;
    double widgetX = m_context.computed.marginLeft + cursorX * m_context.computed.viewScale;

    return QRectF(widgetX, widgetY,
                  m_context.cursor.width,
                  line.height() * m_context.computed.viewScale);
}

// =============================================================================
// Integration
// =============================================================================

void EditorRenderPipeline::setViewportManager(ViewportManager* viewport) {
    m_viewportManager = viewport;
}

void EditorRenderPipeline::setSearchEngine(SearchEngine* engine) {
    m_searchEngine = engine;
}

// =============================================================================
// Main Render Entry Point (Stage 3+4)
// =============================================================================

void EditorRenderPipeline::render(QPainter* painter, const QRect& clipRect) {
    if (!painter) return;

    painter->save();
    painter->setClipRect(clipRect);

    // Stage 1+2: Get visible range and ensure layouts
    updateVisibleRange();

    if (m_textSource) {
        m_textSource->ensureLayouted(
            m_context.computed.firstVisibleParagraph,
            m_context.computed.lastVisibleParagraph
        );
    }

    // Stage 4: Render
    //
    // Two rendering paths (Phase 15: viewport culling):
    // - Page Mode: uses pagination cache with slices (page breaks, clipping)
    // - Scroll Modes: uses viewport-culled paragraph rendering (O(visible) not O(n))

    renderBackground(painter, clipRect);

    if (m_context.viewMode != ViewMode::Page) {
        // =====================================================================
        // SCROLL MODE FAST PATH: render only visible paragraphs
        // Uses firstVisibleParagraph/lastVisibleParagraph for O(~30) draw calls
        // instead of iterating ALL paragraph slices (O(n))
        // =====================================================================
        renderScrollMode(painter, clipRect);
    } else {
        // =====================================================================
        // PAGE MODE: uses pagination cache with slices (unchanged)
        // =====================================================================
        rebuildPaginationCache();
        renderPageBackgrounds(painter, clipRect);

        const auto& pageList = pages();
        double scrollY = m_context.scrollY;

        for (const PageContent& page : pageList) {
            // Convert to widget coordinates
            QRectF textRect = page.textRect;
            double widgetY = textRect.top() - scrollY;
            textRect.moveTop(widgetY);

            // Skip if page text area is not visible
            if (textRect.bottom() < 0 || textRect.top() > m_context.viewportSize.height()) {
                continue;
            }

            // Set clip to text area for Page Mode
            painter->save();
            painter->setClipRect(textRect.toRect());

            // Render each slice in this page
            for (const ParagraphSlice& slice : page.slices) {
                renderSliceSelection(painter, slice, textRect);
                renderSlice(painter, slice, textRect);
                renderSliceCursor(painter, slice, textRect);
            }

            painter->restore();
        }
    }

    // Overlays (work in all modes, already viewport-culled)
    renderCommentHighlights(painter, clipRect);
    renderMarkerHighlights(painter, clipRect);
    renderSearchHighlights(painter, clipRect);
    renderFocusOverlay(painter, clipRect);

    painter->restore();

    // Clear dirty region after paint
    clearDirtyRegion();
}

// =============================================================================
// Dirty Region Tracking
// =============================================================================

void EditorRenderPipeline::markAllDirty() {
    int w = static_cast<int>(m_context.viewportSize.width());
    int h = static_cast<int>(m_context.viewportSize.height());
    if (w > 0 && h > 0) {
        m_dirtyRegion = QRegion(0, 0, w, h);
        emit repaintRequested(m_dirtyRegion);
    }
}

void EditorRenderPipeline::markRepaintOnly() {
    // Lightweight repaint request for color-only changes.
    // Does NOT imply pagination or layout invalidation.
    // Callers use this instead of markAllDirty() when only visual
    // appearance changed (colors, highlights) without affecting geometry.
    int w = static_cast<int>(m_context.viewportSize.width());
    int h = static_cast<int>(m_context.viewportSize.height());
    if (w > 0 && h > 0) {
        m_dirtyRegion = QRegion(0, 0, w, h);
        emit repaintRequested(m_dirtyRegion);
    }
}

void EditorRenderPipeline::markDirty(const QRect& region) {
    if (!region.isEmpty()) {
        m_dirtyRegion = m_dirtyRegion.united(region);
        emit repaintRequested(QRegion(region));
    }
}

void EditorRenderPipeline::markParagraphDirty(size_t paragraphIndex) {
    if (!m_textSource) return;

    double docY = m_textSource->paragraphY(paragraphIndex);
    double widgetY = m_context.computed.marginTop + (docY - m_context.scrollY) * m_context.computed.viewScale;
    double height = m_textSource->paragraphHeight(paragraphIndex) * m_context.computed.viewScale;

    QRect rect(0, static_cast<int>(widgetY),
               static_cast<int>(m_context.viewportSize.width()),
               static_cast<int>(height + 1));

    markDirty(rect);
}

void EditorRenderPipeline::clearDirtyRegion() {
    m_dirtyRegion = QRegion();
}

// =============================================================================
// Internal Render Methods
// =============================================================================

void EditorRenderPipeline::renderBackground(QPainter* painter, const QRect& clipRect) {
    painter->fillRect(clipRect, m_context.colors.background);
}

void EditorRenderPipeline::renderTextFrameBorder(QPainter* painter) {
    if (!m_context.showTextFrameBorder || !m_textSource) return;

    // Calculate text area rectangle based on document content (not viewport)
    double docHeight = m_textSource->totalHeight();
    if (docHeight <= 0) return;

    // Frame surrounds the document content, scrolling with it
    double left = m_context.computed.marginLeft;
    double top = m_context.computed.marginTop + (0 - m_context.scrollY) * m_context.computed.viewScale;  // Top of document in widget coords
    double width = m_context.computed.textWidth;
    double height = docHeight * m_context.computed.viewScale;

    QRectF textFrame(left, top, width, height);

    painter->save();
    painter->setPen(QPen(m_context.textFrameBorderColor, m_context.textFrameBorderWidth));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(textFrame);
    painter->restore();
}

void EditorRenderPipeline::renderParagraphs(QPainter* painter, const QRect& clipRect) {
    if (!m_textSource) return;

    size_t first = m_context.computed.firstVisibleParagraph;
    size_t last = m_context.computed.lastVisibleParagraph;
    size_t count = m_textSource->paragraphCount();

    for (size_t i = first; i <= last && i < count; ++i) {
        double docY = m_textSource->paragraphY(i);
        double widgetY = m_context.computed.marginTop + (docY - m_context.scrollY) * m_context.computed.viewScale;
        double height = m_textSource->paragraphHeight(i) * m_context.computed.viewScale;

        // Check if paragraph intersects clip rect
        QRectF paraRect(0, widgetY, m_context.viewportSize.width(), height);
        if (paraRect.intersects(clipRect)) {
            renderParagraph(painter, i, widgetY);
        }
    }
}

void EditorRenderPipeline::renderParagraph(QPainter* painter, size_t index, double widgetY) {
    QTextLayout* layout = m_textSource->layout(index);
    if (!layout) return;

    QPointF drawPos(m_context.computed.marginLeft, widgetY);

    // Determine text color (focus mode dimming)
    bool isDimmed = m_context.focusMode.enabled &&
                    static_cast<int>(index) != m_context.focusMode.focusedParagraph;

    QColor textColor = isDimmed ? m_context.colors.inactiveText : m_context.colors.text;
    painter->setPen(textColor);

    // Apply transform
    painter->save();
    painter->translate(drawPos);

    // Apply scale only for PageScaling mode
    // For FontScaling: no painter scale needed, font is already scaled via effectiveFont()
    if (m_context.zoomMode == ZoomMode::PageScaling) {
        double scale = m_context.computed.viewScale;
        painter->scale(scale, scale);
    }

    // Draw the layout
    layout->draw(painter, QPointF(0, 0));

    painter->restore();
}

void EditorRenderPipeline::renderSelection(QPainter* painter, [[maybe_unused]] const QRect& clipRect) {
    if (!hasSelection() || !m_textSource) return;

    SelectionRange sel = m_selection.normalized();
    size_t count = m_textSource->paragraphCount();

    for (int para = sel.start.paragraph; para <= sel.end.paragraph; ++para) {
        if (para < 0 || static_cast<size_t>(para) >= count) continue;

        // Calculate selection bounds for this paragraph
        QString text = m_textSource->paragraphText(static_cast<size_t>(para));
        int textLen = text.length();

        int startOffset = (para == sel.start.paragraph) ? sel.start.offset : 0;
        int endOffset = (para == sel.end.paragraph)
                            ? std::min(sel.end.offset, textLen)
                            : textLen;

        if (startOffset < endOffset) {
            double docY = m_textSource->paragraphY(static_cast<size_t>(para));
            double widgetY = m_context.computed.marginTop + (docY - m_context.scrollY) * m_context.computed.viewScale;

            renderParagraphSelection(painter, static_cast<size_t>(para),
                                     startOffset, endOffset, widgetY);
        }
    }
}

void EditorRenderPipeline::renderParagraphSelection(QPainter* painter, size_t paraIndex,
                                                     int startOffset, int endOffset,
                                                     double widgetY) {
    QTextLayout* layout = m_textSource->layout(paraIndex);
    if (!layout) return;

    // Find lines containing selection
    for (int i = 0; i < layout->lineCount(); ++i) {
        QTextLine line = layout->lineAt(i);
        int lineStart = line.textStart();
        int lineEnd = lineStart + line.textLength();

        // Check if selection intersects this line
        if (startOffset < lineEnd && endOffset > lineStart) {
            int selStart = std::max(startOffset, lineStart);
            int selEnd = std::min(endOffset, lineEnd);

            qreal x1 = line.cursorToX(selStart);
            qreal x2 = line.cursorToX(selEnd);
            if (x1 > x2) std::swap(x1, x2);

            // Convert to widget coordinates - Phase 14: use computed values
            double wx1 = m_context.computed.marginLeft + x1 * m_context.computed.viewScale;
            double wx2 = m_context.computed.marginLeft + x2 * m_context.computed.viewScale;
            double wy = widgetY + line.y() * m_context.computed.viewScale;
            double wh = line.height() * m_context.computed.viewScale;

            QRectF selRect(wx1, wy, wx2 - wx1, wh);
            painter->fillRect(selRect, m_context.colors.selection);
        }
    }
}

void EditorRenderPipeline::renderSearchHighlights(QPainter* painter, const QRect& clipRect) {
    if (!m_searchEngine || !m_searchEngine->isActive()) return;

    const auto& matches = m_searchEngine->matches();
    int currentIdx = m_searchEngine->currentMatchIndex();

    for (size_t i = 0; i < matches.size(); ++i) {
        const auto& match = matches[i];

        QRectF matchRect = getTextRect(static_cast<size_t>(match.paragraph),
                                       match.paragraphOffset,
                                       static_cast<int>(match.length));

        if (matchRect.isEmpty() || !matchRect.intersects(clipRect)) continue;

        QColor color = (static_cast<int>(i) == currentIdx)
                           ? m_context.colors.currentMatch
                           : m_context.colors.searchHighlight;

        painter->fillRect(matchRect, color);
    }
}

void EditorRenderPipeline::renderCursor(QPainter* painter) {
    if (!m_context.cursor.visible || !m_context.cursor.blinkState) return;

    QRectF rect = cursorRect();
    if (rect.isEmpty()) return;

    // Adjust rect based on cursor style
    if (m_cursorStyle == CursorStyle::Block) {
        // Block cursor: use character width
        if (m_textSource) {
            QTextLayout* layout = m_textSource->layout(
                static_cast<size_t>(m_cursorPosition.paragraph));
            if (layout && layout->lineCount() > 0) {
                QString text = m_textSource->paragraphText(
                    static_cast<size_t>(m_cursorPosition.paragraph));
                if (m_cursorPosition.offset < text.length()) {
                    QFontMetricsF fm(m_context.font);
                    QChar ch = text.at(m_cursorPosition.offset);
                    double charWidth = fm.horizontalAdvance(ch) * m_context.computed.viewScale;
                    if (charWidth > 0) {
                        rect.setWidth(charWidth);
                    }
                }
            }
        }
        // Semi-transparent block to show character underneath
        QColor blockColor = m_context.colors.cursor;
        blockColor.setAlpha(180);
        painter->fillRect(rect, blockColor);
    } else if (m_cursorStyle == CursorStyle::Underline) {
        // Underline cursor
        double underlineHeight = 2.0 * m_context.computed.viewScale;
        QFontMetricsF fm(m_context.font);
        double charWidth = fm.averageCharWidth() * m_context.computed.viewScale;
        rect.setTop(rect.bottom() - underlineHeight);
        rect.setHeight(underlineHeight);
        rect.setWidth(charWidth);
        painter->fillRect(rect, m_context.colors.cursor);
    } else {
        // Line cursor (default)
        painter->fillRect(rect, m_context.colors.cursor);
    }
}

void EditorRenderPipeline::renderFocusOverlay([[maybe_unused]] QPainter* painter,
                                              [[maybe_unused]] const QRect& clipRect) {
    if (!m_context.focusMode.enabled || !m_textSource) return;

    // Focus overlay is handled in renderParagraph via dimming
    // This method can be extended for more complex focus effects
}

void EditorRenderPipeline::renderMarkerHighlights(QPainter* painter, const QRect& clipRect) {
    if (!m_textSource) return;

    // Get visible paragraph range - Phase 14: use computed values
    size_t firstPara = m_context.computed.firstVisibleParagraph;
    size_t lastPara = m_context.computed.lastVisibleParagraph;
    size_t count = m_textSource->paragraphCount();

    // Check each visible paragraph for TODO/NOTE markers
    for (size_t para = firstPara; para <= lastPara && para < count; ++para) {
        // Check if paragraph has TODO marker in first fragment
        // Markers are stored in QTextCharFormat properties via KmlPropTodo
        bool hasTodo = false;
        bool isCompleted = false;
        bool isNote = false;

        // Get paragraph text to check for TODO/NOTE markers
        QString text = m_textSource->paragraphText(para);

        // Simple marker detection: look for [TODO], [NOTE], [DONE] at start
        if (text.startsWith("[TODO]") || text.startsWith("TODO:")) {
            hasTodo = true;
        } else if (text.startsWith("[DONE]") || text.startsWith("[x]")) {
            hasTodo = true;
            isCompleted = true;
        } else if (text.startsWith("[NOTE]") || text.startsWith("NOTE:")) {
            isNote = true;
            hasTodo = true;  // Treat as marker
        }

        if (!hasTodo) continue;

        // Get line rect for this paragraph - Phase 14: inline coordinate conversion
        double docY = m_textSource->paragraphY(para);
        double widgetY = m_context.computed.marginTop + (docY - m_context.scrollY) * m_context.computed.viewScale;
        double height = m_textSource->paragraphHeight(para) * m_context.computed.viewScale;

        QRectF lineRect(0, widgetY, m_context.viewportSize.width(), height);

        if (!lineRect.toRect().intersects(clipRect)) continue;

        // Choose color based on type and completion state
        QColor highlightColor;
        if (!isNote) {
            highlightColor = isCompleted ? m_context.colors.completedTodo
                                        : m_context.colors.todoHighlight;
        } else {
            highlightColor = m_context.colors.noteHighlight;
        }

        // Draw small indicator in left margin - Phase 14: use computed values
        qreal iconSize = 8.0 * m_context.computed.viewScale;
        qreal marginX = lineRect.left() + 2;
        qreal centerY = lineRect.center().y();

        QRectF iconRect(marginX, centerY - iconSize / 2, iconSize, iconSize);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        if (!isNote) {
            // Draw checkbox for TODO
            QPen pen(highlightColor.darker(150));
            pen.setWidth(1);
            painter->setPen(pen);
            painter->setBrush(isCompleted ? highlightColor : Qt::NoBrush);
            painter->drawRect(iconRect);

            if (isCompleted) {
                // Draw checkmark
                painter->setPen(QPen(Qt::white, 1.5));
                painter->drawLine(
                    QPointF(iconRect.left() + 2, iconRect.center().y()),
                    QPointF(iconRect.center().x(), iconRect.bottom() - 2));
                painter->drawLine(
                    QPointF(iconRect.center().x(), iconRect.bottom() - 2),
                    QPointF(iconRect.right() - 1, iconRect.top() + 2));
            }
        } else {
            // Draw info circle for NOTE
            painter->setPen(Qt::NoPen);
            painter->setBrush(highlightColor);
            painter->drawEllipse(iconRect);

            // Draw 'i' in center
            painter->setPen(QPen(highlightColor.darker(200), 1));
            QFont font = painter->font();
            font.setPixelSize(static_cast<int>(iconSize - 2));
            font.setBold(true);
            painter->setFont(font);
            painter->drawText(iconRect, Qt::AlignCenter, "i");
        }

        painter->restore();

        // Draw subtle line highlight
        QColor lineHighlight = highlightColor;
        lineHighlight.setAlpha(30);
        painter->fillRect(lineRect, lineHighlight);
    }
}

void EditorRenderPipeline::renderCommentHighlights(QPainter* painter, const QRect& clipRect) {
    if (!m_textSource) return;

    // Get visible paragraph range - Phase 14: use computed values
    size_t firstPara = m_context.computed.firstVisibleParagraph;
    size_t lastPara = m_context.computed.lastVisibleParagraph;
    size_t count = m_textSource->paragraphCount();

    // Comment detection is based on KML format properties
    // For now, we scan for comment patterns in text
    for (size_t para = firstPara; para <= lastPara && para < count; ++para) {
        QString text = m_textSource->paragraphText(para);

        // Look for comment markers: /* ... */ or <!-- ... -->
        int commentStart = -1;
        int commentEnd = -1;

        // HTML-style comments
        int htmlStart = text.indexOf("<!--");
        if (htmlStart >= 0) {
            commentStart = htmlStart;
            int htmlEnd = text.indexOf("-->", htmlStart + 4);
            commentEnd = (htmlEnd >= 0) ? htmlEnd + 3 : text.length();
        }

        // C-style comments
        int cStart = text.indexOf("/*");
        if (cStart >= 0 && (commentStart < 0 || cStart < commentStart)) {
            commentStart = cStart;
            int cEnd = text.indexOf("*/", cStart + 2);
            commentEnd = (cEnd >= 0) ? cEnd + 2 : text.length();
        }

        if (commentStart < 0) continue;

        // Get visual rectangle for comment range
        QRectF commentRect = getTextRect(para, commentStart, commentEnd - commentStart);

        if (!commentRect.isEmpty() && commentRect.toRect().intersects(clipRect)) {
            // Fill background
            painter->fillRect(commentRect, m_context.colors.commentHighlight);

            // Draw underline
            QPen pen(m_context.colors.commentBorder);
            pen.setWidth(2);
            painter->setPen(pen);
            painter->drawLine(
                QPointF(commentRect.left(), commentRect.bottom() - 1),
                QPointF(commentRect.right(), commentRect.bottom() - 1));
        }
    }
}

// =============================================================================
// Scroll Mode Rendering (Phase 15: Viewport-culled fast path)
// =============================================================================

void EditorRenderPipeline::renderScrollMode(QPainter* painter, const QRect& clipRect) {
    // Scroll Mode fast path: renders only visible paragraphs using
    // firstVisibleParagraph/lastVisibleParagraph.
    // This is O(visible) instead of O(n) -- ~30 draw calls vs ~3000.

    // Text frame border (if enabled)
    renderTextFrameBorder(painter);

    // Selection highlights (only visible paragraphs)
    if (hasSelection() && m_textSource) {
        SelectionRange sel = m_selection.normalized();
        size_t first = m_context.computed.firstVisibleParagraph;
        size_t last = m_context.computed.lastVisibleParagraph;
        size_t count = m_textSource->paragraphCount();

        // Clamp selection to visible range
        int selFirst = std::max(sel.start.paragraph, static_cast<int>(first));
        int selLast = std::min(sel.end.paragraph, static_cast<int>(last));

        for (int para = selFirst; para <= selLast && static_cast<size_t>(para) < count; ++para) {
            QString text = m_textSource->paragraphText(static_cast<size_t>(para));
            int textLen = text.length();

            int startOffset = (para == sel.start.paragraph) ? sel.start.offset : 0;
            int endOffset = (para == sel.end.paragraph)
                                ? std::min(sel.end.offset, textLen)
                                : textLen;

            if (startOffset < endOffset) {
                double docY = m_textSource->paragraphY(static_cast<size_t>(para));
                double widgetY = m_context.computed.marginTop +
                                 (docY - m_context.scrollY) * m_context.computed.viewScale;

                renderParagraphSelection(painter, static_cast<size_t>(para),
                                         startOffset, endOffset, widgetY);
            }
        }
    }

    // Paragraph text (already viewport-culled internally)
    renderParagraphs(painter, clipRect);

    // Cursor (only if cursor paragraph is in visible range)
    if (m_context.cursor.visible && m_context.cursor.blinkState && m_textSource) {
        int cursorPara = m_cursorPosition.paragraph;
        if (cursorPara >= 0 &&
            static_cast<size_t>(cursorPara) >= m_context.computed.firstVisibleParagraph &&
            static_cast<size_t>(cursorPara) <= m_context.computed.lastVisibleParagraph) {
            renderCursor(painter);
        }
    }
}

// =============================================================================
// Page Mode Rendering (Phase 13.4: Unified rendering)
// =============================================================================

void EditorRenderPipeline::renderPageBackgrounds(QPainter* painter, [[maybe_unused]] const QRect& clipRect) {
    // Page Mode: render page backgrounds, shadows, and borders
    if (m_context.viewMode != ViewMode::Page) return;

    const auto& pageList = pages();
    double scrollY = m_context.scrollY;

    // Page styling from context
    constexpr double shadowOffsetX = 4.0;
    constexpr double shadowOffsetY = 4.0;
    constexpr double shadowBlur = 8.0;

    QColor borderColor = m_context.colors.text;
    borderColor.setAlpha(30);

    for (const PageContent& page : pageList) {
        // Convert to widget coordinates
        QRectF pageRect = page.pageRect;
        double widgetY = pageRect.top() - scrollY;
        pageRect.moveTop(widgetY);

        // Skip if page is not visible
        if (pageRect.bottom() < 0 || pageRect.top() > m_context.viewportSize.height()) {
            continue;
        }

        // Draw page shadow
        if (m_context.pageMode.showPageBreaks) {  // Use existing flag for shadows
            QRectF shadowRect = pageRect.translated(shadowOffsetX, shadowOffsetY);

            for (int i = 0; i < 4; ++i) {
                QColor shadow = m_context.pageMode.pageShadow;
                shadow.setAlpha(shadow.alpha() / (i + 1));
                double expand = shadowBlur * (i + 1) / 4.0;
                QRectF blurRect = shadowRect.adjusted(-expand, -expand, expand, expand);
                painter->fillRect(blurRect, shadow);
            }
        }

        // Draw page background (white)
        painter->fillRect(pageRect, m_context.colors.background);

        // Draw page border
        painter->save();
        QPen borderPen(borderColor);
        borderPen.setWidthF(1.0);
        painter->setPen(borderPen);
        painter->drawRect(pageRect);
        painter->restore();

        // Draw text frame border if enabled
        if (m_context.showTextFrameBorder) {
            QRectF textRect = page.textRect;
            textRect.moveTop(textRect.top() - scrollY);

            painter->save();
            QPen framePen(m_context.textFrameBorderColor);
            framePen.setWidth(m_context.textFrameBorderWidth);
            painter->setPen(framePen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(textRect);
            painter->restore();
        }
    }
}

void EditorRenderPipeline::renderSlice(QPainter* painter, const ParagraphSlice& slice,
                                        const QRectF& textRect) {
    if (!m_textSource) return;

    QTextLayout* layout = m_textSource->layout(slice.paraIndex);
    if (!layout || layout->lineCount() == 0) return;

    // Calculate scale factor for this mode - Phase 14: use computed values
    double scale = m_context.computed.viewScale;

    // Calculate Y offset: shift lines so first line of slice starts at Y=0
    double firstLineY = (slice.startLine < layout->lineCount())
        ? layout->lineAt(slice.startLine).y() : 0.0;

    // Set up painter transform
    painter->save();
    painter->translate(textRect.left(), textRect.top() + slice.yOffset);

    // Apply scale only for PageScaling mode or Page Mode
    if (m_context.zoomMode == ZoomMode::PageScaling || m_context.viewMode == ViewMode::Page) {
        painter->scale(scale, scale);
    }

    // Determine text color (focus mode dimming)
    bool isDimmed = m_context.focusMode.enabled &&
                    static_cast<int>(slice.paraIndex) != m_context.focusMode.focusedParagraph;
    QColor textColor = isDimmed ? m_context.colors.inactiveText : m_context.colors.text;
    painter->setPen(textColor);

    // Draw text lines for this slice
    for (int lineIdx = slice.startLine; lineIdx < slice.endLine && lineIdx < layout->lineCount(); ++lineIdx) {
        QTextLine line = layout->lineAt(lineIdx);
        // Offset is -firstLineY for all lines (shifts the slice to start at Y=0)
        line.draw(painter, QPointF(0, -firstLineY));
    }

    painter->restore();
}

void EditorRenderPipeline::renderSliceSelection(QPainter* painter, const ParagraphSlice& slice,
                                                  const QRectF& textRect) {
    if (!hasSelection() || !m_textSource) return;

    SelectionRange sel = m_selection.normalized();
    int paraIdx = static_cast<int>(slice.paraIndex);

    // Check if this paragraph is part of the selection
    if (paraIdx < sel.start.paragraph || paraIdx > sel.end.paragraph) return;

    QTextLayout* layout = m_textSource->layout(slice.paraIndex);
    if (!layout || layout->lineCount() == 0) return;

    // Calculate scale factor - Phase 14: use computed values
    double scale = m_context.computed.viewScale;
    if (m_context.viewMode == ViewMode::Page) {
        scale = m_context.computed.totalScale;
    }

    // Calculate firstLineY for position adjustment
    double firstLineY = (slice.startLine < layout->lineCount())
        ? layout->lineAt(slice.startLine).y() : 0.0;

    // Set up painter transform
    painter->save();
    painter->translate(textRect.left(), textRect.top() + slice.yOffset);

    if (m_context.zoomMode == ZoomMode::PageScaling || m_context.viewMode == ViewMode::Page) {
        painter->scale(scale, scale);
    }

    QColor selColor = m_context.colors.selection;

    for (int lineIdx = slice.startLine; lineIdx < slice.endLine && lineIdx < layout->lineCount(); ++lineIdx) {
        QTextLine line = layout->lineAt(lineIdx);
        int lineStart = line.textStart();
        int lineEnd = lineStart + line.textLength();

        // Determine selection bounds for this line
        int selStartInLine = 0;
        int selEndInLine = lineEnd - lineStart;

        if (paraIdx == sel.start.paragraph) {
            selStartInLine = std::max(0, sel.start.offset - lineStart);
        }
        if (paraIdx == sel.end.paragraph) {
            selEndInLine = std::min(lineEnd - lineStart, sel.end.offset - lineStart);
        }

        // Only draw if there's something selected on this line
        if (selStartInLine < selEndInLine &&
            sel.start.offset < lineEnd &&
            (paraIdx < sel.end.paragraph || sel.end.offset > lineStart)) {

            // Clamp to valid range
            int actualStart = std::max(0, std::min(selStartInLine, line.textLength()));
            int actualEnd = std::max(0, std::min(selEndInLine, line.textLength()));

            if (actualStart < actualEnd) {
                double selX1 = line.cursorToX(lineStart + actualStart);
                double selX2 = line.cursorToX(lineStart + actualEnd);
                double selY = line.y() - firstLineY;
                double selHeight = line.height();

                // For full-line selection (middle paragraphs), extend to line width
                if (paraIdx > sel.start.paragraph && paraIdx < sel.end.paragraph) {
                    selX1 = 0;
                    selX2 = line.naturalTextWidth();
                }

                painter->fillRect(QRectF(selX1, selY, selX2 - selX1, selHeight), selColor);
            }
        }
    }

    painter->restore();
}

void EditorRenderPipeline::renderSliceCursor(QPainter* painter, const ParagraphSlice& slice,
                                               const QRectF& textRect) {
    // Check if cursor should be visible
    if (!m_context.cursor.visible || !m_context.cursor.blinkState) return;
    if (!m_textSource) return;

    // Check if cursor is in this paragraph
    if (m_cursorPosition.paragraph != static_cast<int>(slice.paraIndex)) return;

    QTextLayout* layout = m_textSource->layout(slice.paraIndex);
    if (!layout || layout->lineCount() == 0) return;

    QString text = m_textSource->paragraphText(slice.paraIndex);
    int textLen = static_cast<int>(text.length());
    int offsetInBlock = m_cursorPosition.offset;
    if (offsetInBlock < 0) offsetInBlock = 0;
    if (offsetInBlock > textLen && textLen > 0) offsetInBlock = textLen;

    // Find which line the cursor is on
    QTextLine cursorLine = layout->lineForTextPosition(offsetInBlock);
    if (!cursorLine.isValid()) return;

    int cursorLineNum = cursorLine.lineNumber();

    // Only draw cursor if the line is in this slice
    if (cursorLineNum < slice.startLine || cursorLineNum >= slice.endLine) return;

    // Calculate scale factor - Phase 14: use computed values
    double scale = m_context.computed.viewScale;
    if (m_context.viewMode == ViewMode::Page) {
        scale = m_context.computed.totalScale;
    }

    // Calculate firstLineY for position adjustment
    double firstLineY = (slice.startLine < layout->lineCount())
        ? layout->lineAt(slice.startLine).y() : 0.0;

    // Set up painter transform
    painter->save();
    painter->translate(textRect.left(), textRect.top() + slice.yOffset);

    if (m_context.zoomMode == ZoomMode::PageScaling || m_context.viewMode == ViewMode::Page) {
        painter->scale(scale, scale);
    }

    double cursorX = cursorLine.cursorToX(offsetInBlock);
    double cursorY = cursorLine.y() - firstLineY;
    double cursorWidth = m_context.cursor.width;
    double cursorHeight = cursorLine.height();

    QColor cursorColor = m_context.colors.cursor;

    // Adjust based on cursor style
    if (m_cursorStyle == CursorStyle::Block) {
        // Block cursor: use character width
        if (offsetInBlock < textLen) {
            QFontMetricsF fm(m_context.computed.effectiveFont);
            QChar ch = text.at(offsetInBlock);
            double charWidth = fm.horizontalAdvance(ch);
            if (charWidth > 0) {
                cursorWidth = charWidth;
            }
        }
        // Semi-transparent block
        cursorColor.setAlpha(180);
        painter->fillRect(QRectF(cursorX, cursorY, cursorWidth, cursorHeight), cursorColor);
    } else if (m_cursorStyle == CursorStyle::Underline) {
        // Underline cursor
        double underlineHeight = 2.0;
        QFontMetricsF fm(m_context.computed.effectiveFont);
        double charWidth = fm.averageCharWidth();
        painter->fillRect(QRectF(cursorX, cursorY + cursorHeight - underlineHeight,
                                  charWidth, underlineHeight), cursorColor);
    } else {
        // Line cursor (default)
        painter->fillRect(QRectF(cursorX, cursorY, cursorWidth, cursorHeight), cursorColor);
    }

    painter->restore();
}

// =============================================================================
// Layout Helpers
// =============================================================================

void EditorRenderPipeline::updateVisibleRange() {
    if (!m_textSource || m_context.viewportSize.isEmpty()) {
        m_context.computed.firstVisibleParagraph = 0;
        m_context.computed.lastVisibleParagraph = 0;
        return;
    }

    // Use viewport manager if available
    if (m_viewportManager) {
        m_context.computed.firstVisibleParagraph = m_viewportManager->firstVisibleParagraph();
        m_context.computed.lastVisibleParagraph = m_viewportManager->lastVisibleParagraph();
        return;
    }

    // Calculate visible range from scroll position
    double viewTop = m_context.scrollY;
    double viewBottom = viewTop + m_context.viewportSize.height() / m_context.computed.viewScale;

    m_context.computed.firstVisibleParagraph = m_textSource->paragraphAtY(viewTop);
    m_context.computed.lastVisibleParagraph = m_textSource->paragraphAtY(viewBottom);

    // Clamp to valid range
    size_t count = m_textSource->paragraphCount();
    if (m_context.computed.lastVisibleParagraph >= count && count > 0) {
        m_context.computed.lastVisibleParagraph = count - 1;
    }
}

double EditorRenderPipeline::paragraphWidgetY(size_t index) const {
    if (!m_textSource) return m_context.computed.marginTop;

    double docY = m_textSource->paragraphY(index);
    return m_context.computed.marginTop + (docY - m_context.scrollY) * m_context.computed.viewScale;
}

QRectF EditorRenderPipeline::getTextRect(size_t paraIndex, int offset, int length) const {
    if (!m_textSource) return QRectF();

    QTextLayout* layout = m_textSource->layout(paraIndex);
    if (!layout || layout->lineCount() == 0) return QRectF();

    // Find line containing offset - O(log n) using Qt's binary search
    QTextLine line = layout->lineForTextPosition(offset);
    if (!line.isValid()) {
        line = layout->lineAt(layout->lineCount() - 1);  // Use last line if offset is beyond
    }
    if (!line.isValid()) return QRectF();

    // Get x positions
    qreal x1 = line.cursorToX(offset);
    qreal x2 = line.cursorToX(offset + length);
    if (x1 > x2) std::swap(x1, x2);

    // Convert to widget coordinates - Phase 14: use computed values
    double docY = m_textSource->paragraphY(paraIndex);
    double widgetY = m_context.computed.marginTop + (docY - m_context.scrollY) * m_context.computed.viewScale + line.y() * m_context.computed.viewScale;
    double widgetX = m_context.computed.marginLeft + x1 * m_context.computed.viewScale;
    double width = (x2 - x1) * m_context.computed.viewScale;
    double height = line.height() * m_context.computed.viewScale;

    return QRectF(widgetX, widgetY, width, height);
}

// =============================================================================
// Pagination (Phase 13.3)
// =============================================================================

const std::vector<PageContent>& EditorRenderPipeline::pages() const {
    rebuildPaginationCache();
    return m_cachedPages;
}

void EditorRenderPipeline::invalidatePagination() {
    m_paginationCacheValid = false;
}

void EditorRenderPipeline::setPageLayout(const QSizeF& pageSize, double pageGap) {
    if (m_context.pageMode.pageSize != pageSize || m_pageGap != pageGap) {
        m_context.pageMode.pageSize = pageSize;
        m_pageGap = pageGap;
        m_paginationCacheValid = false;
    }
}

int EditorRenderPipeline::pageAtY(double docY) const {
    rebuildPaginationCache();

    if (m_cachedPages.empty()) return -1;

    // Binary search for page containing docY - Phase 14: use computed dpiScale
    for (size_t i = 0; i < m_cachedPages.size(); ++i) {
        const PageContent& page = m_cachedPages[i];
        double pageTop = page.pageY;
        double pageBottom = pageTop + m_context.pageMode.pageSize.height() * m_context.computed.dpiScale + m_pageGap;

        if (docY >= pageTop && docY < pageBottom) {
            return static_cast<int>(i);
        }

        // Check if we're past this page
        if (i == m_cachedPages.size() - 1) {
            // Last page - return it if docY is beyond
            return static_cast<int>(i);
        }
    }

    return 0;
}

CursorPosition EditorRenderPipeline::positionFromPoint(const QPointF& point) const {
    if (!m_textSource) {
        return CursorPosition{0, 0};
    }

    rebuildPaginationCache();

    // For Scroll Mode (Continuous), use simple paragraph lookup
    if (m_context.viewMode == ViewMode::Continuous || m_cachedPages.empty()) {
        // Convert widget Y to document Y - Phase 14: inline coordinate conversion
        double docY = (point.y() - m_context.computed.marginTop) / m_context.computed.viewScale + m_context.scrollY;
        size_t paraIndex = m_textSource->paragraphAtY(docY);

        if (paraIndex >= m_textSource->paragraphCount()) {
            paraIndex = m_textSource->paragraphCount() > 0 ? m_textSource->paragraphCount() - 1 : 0;
        }

        QTextLayout* layout = m_textSource->layout(paraIndex);
        if (!layout || layout->lineCount() == 0) {
            return CursorPosition{static_cast<int>(paraIndex), 0};
        }

        // Find Y within paragraph
        double paraY = m_textSource->paragraphY(paraIndex);
        double localY = (docY - paraY);

        // Find line at localY
        QTextLine line;
        for (int i = 0; i < layout->lineCount(); ++i) {
            QTextLine l = layout->lineAt(i);
            if (localY >= l.y() && localY < l.y() + l.height()) {
                line = l;
                break;
            }
        }
        if (!line.isValid()) {
            line = layout->lineAt(layout->lineCount() - 1);
        }

        // Find X position - Phase 14: inline coordinate conversion
        double docX = (point.x() - m_context.computed.marginLeft) / m_context.computed.viewScale;
        int offset = line.xToCursor(docX, QTextLine::CursorBetweenCharacters);

        return CursorPosition{static_cast<int>(paraIndex), offset};
    }

    // Page Mode: use cached pages
    double scrollY = m_context.scrollY;
    double widgetY = point.y();

    // Find which page contains this point
    int pageIndex = -1;
    for (size_t i = 0; i < m_cachedPages.size(); ++i) {
        const PageContent& page = m_cachedPages[i];
        QRectF pageRect = page.pageRect;
        pageRect.moveTop(pageRect.top() - scrollY);

        if (pageRect.contains(point)) {
            pageIndex = static_cast<int>(i);
            break;
        }
    }

    if (pageIndex < 0) {
        // Click outside any page - find closest page
        for (size_t i = 0; i < m_cachedPages.size(); ++i) {
            const PageContent& page = m_cachedPages[i];
            QRectF pageRect = page.pageRect;
            pageRect.moveTop(pageRect.top() - scrollY);

            if (widgetY < pageRect.bottom()) {
                pageIndex = static_cast<int>(i);
                break;
            }
        }
        if (pageIndex < 0 && !m_cachedPages.empty()) {
            pageIndex = static_cast<int>(m_cachedPages.size() - 1);
        }
    }

    if (pageIndex < 0 || m_cachedPages.empty()) {
        return CursorPosition{0, 0};
    }

    const PageContent& page = m_cachedPages[static_cast<size_t>(pageIndex)];
    QRectF textRect = page.textRect;
    textRect.moveTop(textRect.top() - scrollY);

    // Find Y within text area
    double localY = point.y() - textRect.top();

    // Find which slice contains this Y - Phase 14: use computed values
    for (const ParagraphSlice& slice : page.slices) {
        QTextLayout* layout = m_textSource->layout(slice.paraIndex);
        if (!layout) continue;

        // Calculate slice height
        double sliceHeight = 0.0;
        for (int lineIdx = slice.startLine; lineIdx < slice.endLine && lineIdx < layout->lineCount(); ++lineIdx) {
            sliceHeight += layout->lineAt(lineIdx).height() * m_context.computed.viewScale;
        }

        if (localY >= slice.yOffset && localY < slice.yOffset + sliceHeight) {
            // Found the slice - find exact line and offset
            double lineY = slice.yOffset;
            for (int lineIdx = slice.startLine; lineIdx < slice.endLine && lineIdx < layout->lineCount(); ++lineIdx) {
                QTextLine line = layout->lineAt(lineIdx);
                double lineHeight = line.height() * m_context.computed.viewScale;

                if (localY >= lineY && localY < lineY + lineHeight) {
                    // Found the line - find X offset
                    double localX = (point.x() - textRect.left()) / m_context.computed.viewScale;
                    int offset = line.xToCursor(localX, QTextLine::CursorBetweenCharacters);
                    return CursorPosition{static_cast<int>(slice.paraIndex), offset};
                }
                lineY += lineHeight;
            }
        }
    }

    // Fallback: return position in last slice of this page
    if (!page.slices.empty()) {
        const ParagraphSlice& lastSlice = page.slices.back();
        QString text = m_textSource->paragraphText(lastSlice.paraIndex);
        return CursorPosition{static_cast<int>(lastSlice.paraIndex), static_cast<int>(text.length())};
    }

    return CursorPosition{0, 0};
}

void EditorRenderPipeline::rebuildPaginationCache() const {
    // Check if cache is still valid - Phase 14: use computed dpiScale
    bool cacheValid = m_paginationCacheValid &&
                      std::abs(m_cachedDpiScale - m_context.computed.dpiScale) < 0.001 &&
                      m_cachedPageSize == m_context.pageMode.pageSize;

    if (cacheValid) {
        return;
    }

    m_cachedPages.clear();

    if (!m_textSource) {
        m_paginationCacheValid = true;
        return;
    }

    // For Scroll Mode (Continuous, Focus, DistractionFree, Typewriter):
    // Create a single "virtual page" covering the entire document
    if (m_context.viewMode != ViewMode::Page) {
        // Calculate total document height - Phase 14: use computed values
        double totalHeight = 0.0;
        size_t paraCount = m_textSource->paragraphCount();
        for (size_t i = 0; i < paraCount; ++i) {
            totalHeight += m_textSource->paragraphHeight(i);
        }
        totalHeight *= m_context.computed.viewScale;

        // Create single page covering entire document
        PageContent scrollPage;
        scrollPage.pageY = 0.0;
        // For Scroll Mode, pageRect = textRect = full viewport width
        scrollPage.pageRect = QRectF(0, 0, m_context.viewportSize.width(),
                                      totalHeight + m_context.computed.marginTop + m_context.computed.marginBottom);
        scrollPage.textRect = QRectF(m_context.computed.marginLeft, m_context.computed.marginTop,
                                      m_context.computed.textWidth, totalHeight);

        // Add all paragraphs as slices (each paragraph = one slice with all lines)
        double currentY = 0.0;
        for (size_t i = 0; i < paraCount; ++i) {
            QTextLayout* layout = m_textSource->layout(i);
            if (!layout) continue;

            ParagraphSlice slice;
            slice.paraIndex = i;
            slice.startLine = 0;
            slice.endLine = layout->lineCount();
            slice.yOffset = currentY;
            scrollPage.slices.push_back(slice);

            currentY += m_textSource->paragraphHeight(i) * m_context.computed.viewScale;
        }

        m_cachedPages.push_back(scrollPage);
        m_paginationCacheValid = true;
        m_cachedDpiScale = m_context.computed.dpiScale;
        return;
    }

    // Page Mode: calculate page breaks - Phase 14: use ALL computed values
    const auto& c = m_context.computed;
    double pageWidth = c.pageWidthPixels;
    double pageHeight = c.pageHeightPixels;
    double marginLeft = c.marginLeft;
    double marginTop = c.marginTop;
    double textAreaWidth = c.textWidth;
    double textAreaHeight = c.textAreaHeight;
    double centerOffset = c.pageCenterOffset;
    double dpiScale = c.dpiScale;

    size_t paraCount = m_textSource->paragraphCount();

    if (paraCount == 0) {
        // Empty document: create one empty page
        PageContent page;
        page.pageY = 0.0;
        page.pageRect = QRectF(centerOffset, 0.0, pageWidth, pageHeight);
        page.textRect = QRectF(centerOffset + marginLeft, marginTop, textAreaWidth, textAreaHeight);
        m_cachedPages.push_back(page);
    } else {
        double currentPageY = 0.0;
        double currentY = 0.0;  // Y within current page's text area

        PageContent currentPage;
        currentPage.pageY = currentPageY;
        currentPage.pageRect = QRectF(centerOffset, currentPageY, pageWidth, pageHeight);
        currentPage.textRect = QRectF(centerOffset + marginLeft, currentPageY + marginTop,
                                       textAreaWidth, textAreaHeight);

        // Lambda to start a new page
        auto startNewPage = [&]() {
            m_cachedPages.push_back(currentPage);
            currentPageY += pageHeight + m_pageGap;
            currentY = 0.0;

            currentPage = PageContent();
            currentPage.pageY = currentPageY;
            currentPage.pageRect = QRectF(centerOffset, currentPageY, pageWidth, pageHeight);
            currentPage.textRect = QRectF(centerOffset + marginLeft, currentPageY + marginTop,
                                           textAreaWidth, textAreaHeight);
        };

        for (size_t paraIndex = 0; paraIndex < paraCount; ++paraIndex) {
            QTextLayout* layout = m_textSource->layout(paraIndex);
            if (!layout) continue;

            int lineCount = layout->lineCount();
            if (lineCount == 0) continue;

            int startLine = 0;
            double sliceStartY = currentY;

            // Process each line in the paragraph
            for (int lineIdx = 0; lineIdx < lineCount; ++lineIdx) {
                QTextLine line = layout->lineAt(lineIdx);
                double lineHeight = line.height() * dpiScale;

                // Check if this line fits on current page
                if (currentY + lineHeight > textAreaHeight && currentY > 0) {
                    // Save current slice if we have any lines
                    if (lineIdx > startLine) {
                        ParagraphSlice slice;
                        slice.paraIndex = paraIndex;
                        slice.startLine = startLine;
                        slice.endLine = lineIdx;
                        slice.yOffset = sliceStartY;
                        currentPage.slices.push_back(slice);
                    }

                    // Start new page
                    startNewPage();

                    // Start new slice from this line
                    startLine = lineIdx;
                    sliceStartY = currentY;
                }

                currentY += lineHeight;
            }

            // Add final slice for this paragraph
            if (startLine < lineCount) {
                ParagraphSlice slice;
                slice.paraIndex = paraIndex;
                slice.startLine = startLine;
                slice.endLine = lineCount;
                slice.yOffset = sliceStartY;
                currentPage.slices.push_back(slice);
            }
        }

        // Add final page
        if (!currentPage.slices.empty() || m_cachedPages.empty()) {
            m_cachedPages.push_back(currentPage);
        }
    }

    // Update cache parameters - Phase 14: use computed dpiScale
    m_cachedDpiScale = m_context.computed.dpiScale;
    m_cachedPageSize = m_context.pageMode.pageSize;
    m_paginationCacheValid = true;
}

}  // namespace kalahari::editor
