/// @file editor_render_pipeline.cpp
/// @brief Implementation of unified rendering pipeline (OpenSpec #00043 Phase 12.1)

#include <kalahari/editor/editor_render_pipeline.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/viewport_manager.h>
#include <kalahari/editor/search_engine.h>
#include <kalahari/core/logger.h>
#include <QPainter>
#include <QPainterPath>
#include <QTextLayout>
#include <QTextLine>
#include <QTextBlock>
#include <algorithm>
#include <cmath>
#include <map>

namespace kalahari::editor {

namespace {

/// Size of the page numbers relative to the text font
constexpr double PAGE_NUMBER_FONT_SCALE = 0.8;

/// Shade of the desk around the pages, from the paper color (QColor::darker() for light
/// paper, QColor::lighter() for dark paper)
constexpr int DESK_DARKER_FACTOR = 118;
constexpr int DESK_LIGHTER_FACTOR = 160;

/// Size of the TODO and note marker icons in the margin, and the gap between them and
/// the text (pixels at 100% zoom)
constexpr double MARKER_ICON_SIZE = 8.0;
constexpr double MARKER_ICON_GAP = 4.0;

}  // anonymous namespace

// =============================================================================
// Constructor / Destructor
// =============================================================================

EditorRenderPipeline::EditorRenderPipeline(QObject* parent)
    : QObject(parent) {
    // Initialize with default context
    m_context.font = QFont("Segoe UI", 11);
}

EditorRenderPipeline::~EditorRenderPipeline() = default;

// =============================================================================
// Text Source (Stage 1)
// =============================================================================

void EditorRenderPipeline::setTextSource(std::unique_ptr<ITextSource> source) {
    m_textSource = std::move(source);
    m_heightDirty = true;
    markAllDirty();
}

// =============================================================================
// Render Context (Stage 2)
// =============================================================================

void EditorRenderPipeline::setContext(const RenderContext& context) {
    bool fontChanged = (m_context.font != context.font);
    bool widthChanged = (m_context.textWidth != context.textWidth);
    bool marginsChanged = (m_context.margins != context.margins);

    m_context = context;

    // Update text source if font, width, or margins changed
    if (m_textSource && (fontChanged || widthChanged || marginsChanged)) {
        m_textSource->setFont(m_context.font);
        m_textSource->setTextWidth(m_context.computed.textWidth);
        m_heightDirty = true;
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
    computeViewGeometry();
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

    computeViewGeometry();
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
        markAllDirty();
    }
}

void EditorRenderPipeline::setFont(const QFont& font) {
    if (m_context.font != font) {
        m_context.font = font;
        computeTypography();  // the base font size is the typography's reference size
        if (m_textSource) {
            m_textSource->setFont(font);
            m_textSource->setTypography(m_context.computed.typography);
            m_heightDirty = true;
        }
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
    updateVisibleRange();
}

void EditorRenderPipeline::setScreenDpi(double dpi) {
    if (dpi < 1.0) dpi = DEFAULT_DPI;  // Sanity check

    if (std::abs(m_context.screenDpi - dpi) < 0.001) {
        return;  // No change
    }

    // No relayout: the text is sized in points, which Qt converts with this same logical
    // DPI. Only the page geometry depends on it (recomputed by applyInitialConfig()).
    m_context.screenDpi = dpi;
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
    computeTypography();
    computePageLayout();
    computeMargins();
    computeTextWidth();
    computeViewGeometry();
    applyComputedToSource();

    m_heightDirty = true;
    markAllDirty();
}

void EditorRenderPipeline::computeDpiScaling() {
    // screenDpi is the logical DPI Qt already uses for the font's points, so only zoom
    // scales the text; the DPI converts page sizes (points) and margins (mm) to pixels.
    m_context.computed.mmToPixels = m_context.screenDpi / MM_PER_INCH;
    m_context.computed.totalScale = m_context.zoomFactor;

    // viewScale depends on zoom mode; pages at 100% have their size on paper
    if (m_context.zoomMode == ZoomMode::PageScaling) {
        m_context.computed.viewScale = m_context.computed.totalScale * m_context.paperScale;
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

void EditorRenderPipeline::computeTypography() {
    // Spacing and indent are pixels at 100% zoom, i.e. for the base font. The layout
    // scales them with the document font: font-scaling zoom grows them with the text
    // (in the same relayout), page-scaling zoom leaves them to the painter.
    LayoutTypography typography = m_context.typography;
    typography.referencePointSize = m_context.font.pointSizeF();
    m_context.computed.typography = typography;
}

void EditorRenderPipeline::computeMargins() {
    auto& computed = m_context.computed;
    if (m_context.viewMode == ViewMode::Page) {
        // The page's own margins (computePageLayout() fits them to the page)
        computed.marginLeft = computed.pageMargins.left();
        computed.marginTop = computed.pageMargins.top();
        computed.marginRight = computed.pageMargins.right();
        computed.marginBottom = computed.pageMargins.bottom();
    } else {
        // View margins (pixels)
        computed.marginLeft = m_context.margins.left;
        computed.marginTop = m_context.margins.top;
        computed.marginRight = m_context.margins.right;
        computed.marginBottom = m_context.margins.bottom;
    }
}

void EditorRenderPipeline::computePageLayout() {
    auto& computed = m_context.computed;

    // Page size in document units (the page size is in points, the margins in mm)
    const double pointsToPixels = m_context.screenDpi / POINTS_PER_INCH;
    computed.pageWidthPixels = m_context.pageMode.pageSize.width() * pointsToPixels;
    computed.pageHeightPixels = m_context.pageMode.pageSize.height() * pointsToPixels;

    // Margins taking more than the page leaves for text are scaled down together
    const auto fit = [](double first, double second, double extent) {
        first = std::max(0.0, first);
        second = std::max(0.0, second);
        const double available = extent * (1.0 - MIN_PAGE_TEXT_SHARE);
        const double total = first + second;
        const double factor = total > available && total > 0.0 ? available / total : 1.0;
        return std::pair{first * factor, second * factor};
    };
    const QMarginsF& mm = m_context.pageMode.marginsMm;
    const auto [left, right] = fit(mm.left() * computed.mmToPixels,
                                   mm.right() * computed.mmToPixels, computed.pageWidthPixels);
    const auto [top, bottom] = fit(mm.top() * computed.mmToPixels,
                                   mm.bottom() * computed.mmToPixels, computed.pageHeightPixels);
    computed.pageMargins = QMarginsF(left, top, right, bottom);

    // Whole pixels, as the layout places the lines (on the pixel grid): the sheets follow
    // the same pitch, or the text drifts off them page by page (A4 is 1122.52 px high at
    // 96 dpi)
    computed.textAreaHeight = std::max(1.0, std::round(computed.pageHeightPixels - top - bottom));
    computed.pagePitch = std::max(
        computed.textAreaHeight,
        std::round(computed.pageHeightPixels + std::max(0.0, m_context.pageMode.pageSpacing)));
}

void EditorRenderPipeline::computeTextWidth() {
    double width = 0.0;
    if (m_context.viewMode == ViewMode::Page) {
        // Page Mode: text width from page size minus margins
        width = m_context.computed.pageWidthPixels
              - m_context.computed.marginLeft
              - m_context.computed.marginRight;
    } else {
        // Scroll modes: viewport width minus margins
        width = m_context.viewportSize.width()
              - m_context.computed.marginLeft
              - m_context.computed.marginRight;
    }
    // A narrow (or not yet sized) viewport must not wrap the text a few glyphs per line
    m_context.computed.textWidth = std::max(MIN_TEXT_WIDTH, width);
}

void EditorRenderPipeline::computeViewGeometry() {
    auto& computed = m_context.computed;
    const double scale = computed.viewScale;
    const double viewWidth = m_context.viewportSize.width();
    const double viewHeight = m_context.viewportSize.height();

    // Typewriter scrolling: room below the last line, so that the last lines can be
    // scrolled up to the focus height. None above the first line: the text starts at the
    // top, and the cursor line stays at the focus height once it has come down to it.
    double typewriterBottom = 0.0;
    if (m_context.typewriter.enabled && viewHeight > 0.0) {
        const double focus = std::clamp(m_context.typewriter.focusPosition, 0.0, 1.0);
        typewriterBottom = (1.0 - focus) * viewHeight;
    }

    if (m_context.viewMode == ViewMode::Page) {
        // The pages are a column with the page gap around it. It is centred while it
        // fits the view and scrolls sideways when the zoom makes it wider.
        const double gap = std::max(0.0, m_context.pageMode.pageSpacing);
        const double pageWidth = computed.pageWidthPixels * scale;
        const double pagesViewWidth = std::max(0.0, viewWidth - m_context.scrollBarWidth);
        computed.contentWidth = pageWidth + 2.0 * gap * scale;
        const double maxX = std::max(0.0, computed.contentWidth - pagesViewWidth);
        m_context.scrollX = std::clamp(m_context.scrollX, 0.0, maxX);
        computed.pageCenterOffset =
            maxX > 0.0 ? gap * scale - m_context.scrollX : (pagesViewWidth - pageWidth) / 2.0;
        computed.originX = computed.pageCenterOffset + computed.marginLeft * scale;
        computed.originY = (gap + computed.marginTop) * scale;
        computed.scrollPaddingTop = gap + computed.marginTop;
        computed.scrollPaddingBottom = gap + computed.marginBottom + typewriterBottom / scale;
    } else {
        computed.contentWidth = viewWidth;
        m_context.scrollX = 0.0;
        computed.pageCenterOffset = 0.0;
        computed.originX = computed.marginLeft;
        computed.originY = computed.marginTop;
        computed.scrollPaddingTop = computed.marginTop;
        computed.scrollPaddingBottom = computed.marginBottom + typewriterBottom;
    }

    // The viewport manager works in document units: it needs the scale and where the
    // scroll position is shown to tell the visible paragraphs and the scroll range
    if (m_viewportManager) {
        m_viewportManager->setViewGeometry(scale, computed.originY);
        m_viewportManager->setTopScrollPadding(computed.scrollPaddingTop);
        m_viewportManager->setBottomScrollPadding(computed.scrollPaddingBottom);
    }
}

void EditorRenderPipeline::applyComputedToSource() {
    if (!m_textSource) return;

    m_textSource->setFont(m_context.computed.effectiveFont);
    m_textSource->setTextWidth(m_context.computed.textWidth);
    m_textSource->setTypography(m_context.computed.typography);
    applyPageFlowToSource();
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

void EditorRenderPipeline::applyTypographyToSource() {
    if (!m_textSource) return;
    m_textSource->setTypography(m_context.computed.typography);
}

void EditorRenderPipeline::applyPageFlowToSource() {
    if (!m_textSource) return;
    // The layout places the lines on the pages; the other modes have one long page
    PageFlow flow;
    if (m_context.viewMode == ViewMode::Page) {
        flow.enabled = true;
        flow.pitch = m_context.computed.pagePitch;
        flow.textHeight = m_context.computed.textAreaHeight;
    }
    m_textSource->setPageFlow(flow);
}

// =============================================================================
// Granular Configuration (Phase 15)
// =============================================================================

void EditorRenderPipeline::setConfigDpi(double dpi) {
    if (std::abs(m_context.screenDpi - dpi) < 0.01) return;  // No change

    m_context.screenDpi = dpi;
    computeDpiScaling();
    computePageLayout();
    computeMargins();
    computeTextWidth();
    applyWidthToSource();
    applyPageFlowToSource();
    computeViewGeometry();
    markAllDirty();
}

void EditorRenderPipeline::setConfigFont(const QFont& font) {
    if (m_context.font == font) return;  // No change

    m_context.font = font;
    computeEffectiveFont();
    computeTypography();  // the base font size is the typography's reference size
    applyFontToSource();
    applyTypographyToSource();
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
    // Font scaling lays the text out again at the new size (typography lengths follow the
    // font in the same relayout); page scaling keeps the font and only scales the painter
    applyFontToSource();
    computeViewGeometry();

    m_heightDirty = true;
    markAllDirty();
}

void EditorRenderPipeline::setConfigPaperScale(double scale) {
    scale = scale > 0.0 ? scale : 1.0;
    if (std::abs(m_context.paperScale - scale) < 1e-6) return;  // No change

    m_context.paperScale = scale;
    computeDpiScaling();  // The page view's scale
    computeViewGeometry();
    markAllDirty();
}

void EditorRenderPipeline::setConfigTypography(const LayoutTypography& typography) {
    if (m_context.typography == typography) return;  // No change

    m_context.typography = typography;
    computeTypography();
    applyTypographyToSource();
    m_heightDirty = true;
    markAllDirty();
}

void EditorRenderPipeline::setConfigViewportSize(const QSizeF& size) {
    if (m_context.viewportSize == size) return;  // No change

    m_context.viewportSize = size;

    if (m_context.viewMode != ViewMode::Page) {
        // Scroll modes: text width depends on viewport (pages keep theirs)
        computeTextWidth();
        applyWidthToSource();
        m_heightDirty = true;
    }
    computeViewGeometry();

    updateVisibleRange();
    markAllDirty();
}

void EditorRenderPipeline::setConfigMargins(double left, double top, double right, double bottom) {
    const RenderMargins margins{left, top, right, bottom};
    if (std::abs(m_context.margins.left - left) < 0.01 &&
        std::abs(m_context.margins.top - top) < 0.01 &&
        std::abs(m_context.margins.right - right) < 0.01 &&
        std::abs(m_context.margins.bottom - bottom) < 0.01) return;

    // View margins: the page mode uses the page's own (setConfigPageLayout())
    m_context.margins = margins;
    computeMargins();
    computeTextWidth();
    applyWidthToSource();
    computeViewGeometry();

    m_heightDirty = true;
    markAllDirty();
}

void EditorRenderPipeline::setConfigPageLayout(const QSizeF& pageSize, const QMarginsF& marginsMm,
                                               double pageGap) {
    PageModeConfig& page = m_context.pageMode;
    if (page.pageSize == pageSize && page.marginsMm == marginsMm &&
        std::abs(page.pageSpacing - pageGap) < 0.01) return;

    page.pageSize = pageSize;
    page.marginsMm = marginsMm;
    page.pageSpacing = pageGap;

    computePageLayout();
    computeMargins();
    computeTextWidth();
    applyWidthToSource();
    applyPageFlowToSource();
    computeViewGeometry();

    m_heightDirty = true;
    markAllDirty();
}

void EditorRenderPipeline::setConfigShowPageNumbers(bool show) {
    if (m_context.pageMode.showPageNumbers == show) return;

    m_context.pageMode.showPageNumbers = show;
    markRepaintOnly();
}

void EditorRenderPipeline::setConfigTypewriter(bool enabled, double focusPosition) {
    TypewriterConfig& typewriter = m_context.typewriter;
    if (typewriter.enabled == enabled &&
        std::abs(typewriter.focusPosition - focusPosition) < 0.0001) return;

    typewriter.enabled = enabled;
    typewriter.focusPosition = focusPosition;
    computeViewGeometry();
    updateVisibleRange();
    markAllDirty();
}

void EditorRenderPipeline::setConfigScrollX(double x) {
    const double oldScrollX = m_context.scrollX;
    m_context.scrollX = x;
    computeViewGeometry();  // clamps the offset
    if (std::abs(m_context.scrollX - oldScrollX) > 0.001) {
        markAllDirty();
    }
}

void EditorRenderPipeline::setConfigScrollBarWidth(double width) {
    if (std::abs(m_context.scrollBarWidth - width) < 0.01) return;

    m_context.scrollBarWidth = width;
    computeViewGeometry();
    markAllDirty();
}

void EditorRenderPipeline::setConfigColors(const RenderColors& colors) {
    m_context.colors = colors;
    // No layout recalculation needed - just repaint
    markRepaintOnly();
}

void EditorRenderPipeline::setConfigViewMode(ViewMode mode, ZoomMode zoomMode) {
    if (m_context.viewMode == mode && m_context.zoomMode == zoomMode) return;

    m_context.viewMode = mode;
    m_context.zoomMode = zoomMode;

    // View mode affects everything - full recalculation
    computeDpiScaling();
    computeEffectiveFont();
    computeTypography();
    computePageLayout();
    computeMargins();
    computeTextWidth();
    computeViewGeometry();
    applyComputedToSource();

    m_heightDirty = true;
    markAllDirty();
}

void EditorRenderPipeline::applyInitialConfig() {
    computeDpiScaling();
    computeEffectiveFont();
    computeTypography();
    computePageLayout();
    computeMargins();
    computeTextWidth();
    computeViewGeometry();
    applyComputedToSource();

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
        markDirty(cursorPaintRect().toAlignedRect());

        m_cursorPosition = position;

        // Mark new cursor position dirty
        markDirty(cursorPaintRect().toAlignedRect());

        // Focus mode: the paragraph left becomes dimmed, the one entered bright
        if (m_context.viewMode == ViewMode::Focus && oldParagraph != position.paragraph) {
            markParagraphDirty(static_cast<size_t>(oldParagraph));
            markParagraphDirty(static_cast<size_t>(position.paragraph));
        }
    }
}

void EditorRenderPipeline::setCursorVisible(bool visible) {
    if (m_context.cursor.visible != visible) {
        m_context.cursor.visible = visible;
        markDirty(cursorPaintRect().toAlignedRect());
    }
}

void EditorRenderPipeline::setCursorBlinkState(bool on) {
    if (m_context.cursor.blinkState != on) {
        m_context.cursor.blinkState = on;
        markDirty(cursorPaintRect().toAlignedRect());
    }
}

void EditorRenderPipeline::setCursorStyle(CursorStyle style) {
    if (m_cursorStyle != style) {
        markDirty(cursorPaintRect().toAlignedRect());
        m_cursorStyle = style;
        markDirty(cursorPaintRect().toAlignedRect());
    }
}

void EditorRenderPipeline::setCursorWidth(double width) {
    width = std::max(1.0, width);
    if (m_context.cursor.width != width) {
        markDirty(cursorPaintRect().toAlignedRect());
        m_context.cursor.width = width;
        markDirty(cursorPaintRect().toAlignedRect());
    }
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

void EditorRenderPipeline::setDropCaret(const std::optional<CursorPosition>& position) {
    if (m_dropCaret == position) {
        return;
    }
    // Repaint the old and the new caret line
    if (m_dropCaret) {
        markDirty(caretRect(*m_dropCaret).toAlignedRect());
    }
    m_dropCaret = position;
    if (m_dropCaret) {
        markDirty(caretRect(*m_dropCaret).toAlignedRect());
    }
}

QRectF EditorRenderPipeline::cursorRect() const {
    return caretRect(m_cursorPosition);
}

QRectF EditorRenderPipeline::caretRect(const CursorPosition& position) const {
    if (!m_textSource) {
        return QRectF();
    }

    int paraIndex = position.paragraph;
    if (paraIndex < 0 || static_cast<size_t>(paraIndex) >= m_textSource->paragraphCount()) {
        return QRectF();
    }

    const double scale = m_context.computed.viewScale;
    QTextLayout* layout = m_textSource->layout(static_cast<size_t>(paraIndex));
    if (!layout || layout->lineCount() == 0) {
        // Fallback: return default cursor rect
        double widgetY = paragraphWidgetY(static_cast<size_t>(paraIndex));
        QFontMetricsF fm(m_context.computed.effectiveFont);
        return QRectF(m_context.computed.originX, widgetY,
                      m_context.cursor.width, fm.height() * scale);
    }

    // Find line containing cursor - O(log n) using Qt's binary search
    int offset = position.offset;
    QTextLine line = layout->lineForTextPosition(offset);
    if (!line.isValid()) {
        line = layout->lineAt(layout->lineCount() - 1);
    }

    // Calculate cursor X position
    qreal cursorX = line.cursorToX(offset);

    // The cursor is as wide at every zoom, like the caret of a word processor
    const double docY = m_textSource->paragraphY(static_cast<size_t>(paraIndex)) + line.y();
    return QRectF(documentToWidget(QPointF(cursorX, docY)),
                  QSizeF(m_context.cursor.width, line.height() * scale));
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

void EditorRenderPipeline::setSpokenWord(int paragraph, int offset, int length) {
    std::optional<ParagraphHighlight> word;
    if (paragraph >= 0 && length > 0) {
        word = ParagraphHighlight{static_cast<size_t>(paragraph),
                                  TextHighlight{offset, length, HighlightKind::SpokenWord}};
    }
    if (m_spokenWord == word) {
        return;
    }
    // Repaint the old and the new word's paragraph
    if (m_spokenWord) {
        markParagraphDirty(m_spokenWord->first);
    }
    m_spokenWord = word;
    if (m_spokenWord) {
        markParagraphDirty(m_spokenWord->first);
    }
}

// =============================================================================
// Main Render Entry Point (Stage 3+4)
// =============================================================================

void EditorRenderPipeline::ensureVisibleLaidOut() {
    // Blocks waiting for layout have estimated heights; laying them out changes the
    // heights, and the viewport keeps the text at its top in place by moving the scroll
    // position. Repeat until the range shown is laid out.
    updateVisibleRange();
    if (!m_textSource) {
        return;
    }
    size_t first = 0;
    size_t last = 0;
    do {
        first = m_context.computed.firstVisibleParagraph;
        last = m_context.computed.lastVisibleParagraph;
        m_textSource->ensureLayouted(first, last);
        updateVisibleRange();
    } while (first != m_context.computed.firstVisibleParagraph ||
             last != m_context.computed.lastVisibleParagraph);
}

void EditorRenderPipeline::render(QPainter* painter, const QRect& clipRect) {
    if (!painter) return;

    painter->save();
    painter->setClipRect(clipRect);

    // Stage 1+2: Get visible range and ensure layouts
    ensureVisibleLaidOut();

    // Stage 4: Render. One path for every view mode: the layout has placed the lines on
    // the pages (page mode), so only the visible paragraphs are drawn, through the same
    // document-to-widget mapping.
    renderBackground(painter, clipRect);
    if (m_context.viewMode == ViewMode::Page) {
        renderPages(painter, clipRect);
    } else {
        renderTextFrameBorder(painter);
    }
    renderText(painter, clipRect);

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
    // Does NOT imply layout invalidation.
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

    double widgetY = paragraphWidgetY(paragraphIndex);
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
    if (m_context.viewMode != ViewMode::Page) {
        painter->fillRect(clipRect, m_context.colors.background);
        return;
    }
    // The desk around the pages: a shade of the paper, darker for light paper and
    // lighter for dark paper, so the sheets stand out in both color modes
    const QColor& paper = m_context.colors.background;
    painter->fillRect(clipRect, paper.lightness() > 127 ? paper.darker(DESK_DARKER_FACTOR)
                                                        : paper.lighter(DESK_LIGHTER_FACTOR));
}

void EditorRenderPipeline::renderTextFrameBorder(QPainter* painter) {
    if (!m_context.showTextFrameBorder || !m_textSource) return;

    // Calculate text area rectangle based on document content (not viewport)
    double docHeight = m_textSource->totalHeight();
    if (docHeight <= 0) return;

    // Frame surrounds the document content, scrolling with it
    const double scale = m_context.computed.viewScale;
    const QRectF textFrame(documentToWidget(QPointF(0.0, 0.0)),
                           QSizeF(m_context.computed.textWidth * scale, docHeight * scale));

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
        double widgetY = paragraphWidgetY(i);
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

    QPointF drawPos(m_context.computed.originX, widgetY);

    // Focus mode dims every paragraph but the cursor's
    const bool isDimmed = m_context.viewMode == ViewMode::Focus &&
                          static_cast<int>(index) != m_cursorPosition.paragraph;

    QColor textColor = isDimmed ? m_context.colors.inactiveText : m_context.colors.text;
    painter->setPen(textColor);

    // Apply transform
    painter->save();
    painter->translate(drawPos);

    // Page scaling zooms with the painter; font scaling has already laid out the text at
    // the zoomed font size (view scale 1)
    const double scale = m_context.computed.viewScale;
    if (scale != 1.0) {
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
            renderParagraphSelection(painter, static_cast<size_t>(para), startOffset, endOffset,
                                     paragraphWidgetY(static_cast<size_t>(para)));
        }
    }
}

void EditorRenderPipeline::renderParagraphSelection(QPainter* painter, size_t paraIndex,
                                                     int startOffset, int endOffset,
                                                     double widgetY) {
    // The band covers the whole line boxes, so the lines of a selection join up
    fillTextRange(painter, paraIndex, startOffset, endOffset, widgetY, m_context.colors.selection,
                  true);
}

std::vector<EditorRenderPipeline::LinePiece> EditorRenderPipeline::linePieces(
    size_t paraIndex, int startOffset, int endOffset) const {
    std::vector<LinePiece> pieces;
    QTextLayout* layout = m_textSource ? m_textSource->layout(paraIndex) : nullptr;
    if (!layout) return pieces;

    for (int i = 0; i < layout->lineCount(); ++i) {
        const QTextLine line = layout->lineAt(i);
        const int lineStart = line.textStart();
        const int lineEnd = lineStart + line.textLength();
        if (startOffset >= lineEnd || endOffset <= lineStart) continue;

        qreal x1 = line.cursorToX(std::max(startOffset, lineStart));
        qreal x2 = line.cursorToX(std::min(endOffset, lineEnd));
        if (x1 > x2) std::swap(x1, x2);
        pieces.push_back({line, x1, x2});
    }
    return pieces;
}

void EditorRenderPipeline::fillTextRange(QPainter* painter, size_t paraIndex, int startOffset,
                                         int endOffset, double widgetY, const QColor& color,
                                         bool lineBoxes) {
    const double scale = m_context.computed.viewScale;
    for (const LinePiece& piece : linePieces(paraIndex, startOffset, endOffset)) {
        const QRectF band = lineBoxes ? lineBox(piece.line)
                                      : QRectF(piece.line.x(), piece.line.y(), piece.line.width(),
                                               piece.line.height());
        const double x1 = m_context.computed.originX + piece.x1 * scale;
        const double x2 = m_context.computed.originX + piece.x2 * scale;
        painter->fillRect(QRectF(x1, widgetY + band.y() * scale, x2 - x1, band.height() * scale),
                          color);
    }
}

std::vector<EditorRenderPipeline::ParagraphHighlight> EditorRenderPipeline::visibleHighlights(
    const QRect& clipRect) const {
    std::vector<ParagraphHighlight> highlights;
    if (!m_textSource) return highlights;

    const size_t first = m_context.computed.firstVisibleParagraph;
    const size_t last = m_context.computed.lastVisibleParagraph;
    const size_t count = m_textSource->paragraphCount();

    // Annotations and check results of the paragraphs in the clip rect (a cursor blink
    // repaints one line)
    const double scale = m_context.computed.viewScale;
    for (size_t paragraph = first; paragraph <= last && paragraph < count; ++paragraph) {
        const QRectF paragraphRect(0.0, paragraphWidgetY(paragraph), m_context.viewportSize.width(),
                                   m_textSource->paragraphHeight(paragraph) * scale);
        if (!paragraphRect.intersects(clipRect)) continue;
        for (const TextHighlight& highlight : m_textSource->paragraphHighlights(paragraph)) {
            highlights.emplace_back(paragraph, highlight);
        }
    }

    // Search matches. They are sorted by position, so the ones in the visible paragraphs
    // are found by binary search instead of measuring all.
    if (m_searchEngine && m_searchEngine->isActive()) {
        const auto& matches = m_searchEngine->matches();
        const auto firstMatch = std::lower_bound(
            matches.begin(), matches.end(), static_cast<int>(first),
            [](const SearchMatch& match, int paragraph) { return match.paragraph < paragraph; });
        const int currentIdx = m_searchEngine->currentMatchIndex();
        for (auto it = firstMatch; it != matches.end() && it->paragraph <= static_cast<int>(last);
             ++it) {
            // A match is within one paragraph, but may wrap onto the next line
            const HighlightKind kind = static_cast<int>(it - matches.begin()) == currentIdx
                                           ? HighlightKind::CurrentSearchMatch
                                           : HighlightKind::SearchMatch;
            highlights.emplace_back(static_cast<size_t>(it->paragraph),
                                    TextHighlight{it->paragraphOffset,
                                                  static_cast<int>(it->length), kind});
        }
    }

    if (m_spokenWord && m_spokenWord->first >= first && m_spokenWord->first <= last &&
        m_spokenWord->first < count) {
        highlights.push_back(*m_spokenWord);
    }
    return highlights;
}

void EditorRenderPipeline::renderHighlightBackgrounds(
    QPainter* painter, const std::vector<ParagraphHighlight>& highlights) {
    const RenderColors& colors = m_context.colors;
    for (const auto& [paragraph, highlight] : highlights) {
        QColor color;
        switch (highlight.kind) {
            case HighlightKind::SearchMatch: color = colors.searchHighlight; break;
            case HighlightKind::CurrentSearchMatch: color = colors.currentMatch; break;
            case HighlightKind::Comment: color = colors.commentHighlight; break;
            case HighlightKind::Todo: color = colors.todoHighlight; break;
            case HighlightKind::CompletedTodo: color = colors.completedTodo; break;
            case HighlightKind::Note: color = colors.noteHighlight; break;
            case HighlightKind::SpokenWord: color = colors.spokenWord; break;
            // Drawn as marks only: a resolved comment stays without the background
            case HighlightKind::ResolvedComment:
            case HighlightKind::Spelling:
            case HighlightKind::Grammar: continue;
        }
        fillTextRange(painter, paragraph, highlight.start, highlight.start + highlight.length,
                      paragraphWidgetY(paragraph), color, false);
    }
}

void EditorRenderPipeline::renderHighlightMarks(
    QPainter* painter, const std::vector<ParagraphHighlight>& highlights) {
    const RenderColors& colors = m_context.colors;
    const double scale = m_context.computed.viewScale;
    const double originX = m_context.computed.originX;

    // Marker icons drawn on each line so far: a line's icons stand side by side
    std::map<std::pair<size_t, int>, int> lineIcons;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    for (const auto& [paragraph, highlight] : highlights) {
        if (highlight.kind == HighlightKind::SearchMatch ||
            highlight.kind == HighlightKind::CurrentSearchMatch ||
            highlight.kind == HighlightKind::SpokenWord) {
            continue;  // backgrounds only
        }
        const double widgetY = paragraphWidgetY(paragraph);
        const auto pieces =
            linePieces(paragraph, highlight.start, highlight.start + highlight.length);
        if (pieces.empty()) continue;

        switch (highlight.kind) {
            case HighlightKind::Comment:
            case HighlightKind::ResolvedComment: {
                // A line at the bottom of the text; dotted once the comment is resolved
                const bool resolved = highlight.kind == HighlightKind::ResolvedComment;
                QPen pen(colors.commentBorder, std::max(1.0, (resolved ? 1.0 : 2.0) * scale));
                pen.setStyle(resolved ? Qt::DotLine : Qt::SolidLine);
                pen.setCapStyle(Qt::FlatCap);
                painter->setPen(pen);
                for (const LinePiece& piece : pieces) {
                    const double y = widgetY + (piece.line.y() + piece.line.height()) * scale -
                                     pen.widthF() / 2.0;
                    painter->drawLine(QPointF(originX + piece.x1 * scale, y),
                                      QPointF(originX + piece.x2 * scale, y));
                }
                break;
            }
            case HighlightKind::Spelling:
            case HighlightKind::Grammar: {
                // A wave under the baseline
                const double amplitude = std::max(1.0, 1.5 * scale);
                const double step = 2.0 * amplitude;
                painter->setPen(QPen(highlight.kind == HighlightKind::Spelling
                                         ? colors.spellError
                                         : colors.grammarWarning,
                                     std::max(1.0, scale)));
                painter->setBrush(Qt::NoBrush);
                for (const LinePiece& piece : pieces) {
                    const double x1 = originX + piece.x1 * scale;
                    const double x2 = originX + piece.x2 * scale;
                    const double y = widgetY + (piece.line.y() + piece.line.ascent()) * scale +
                                     amplitude + 1.0;
                    QPainterPath wave(QPointF(x1, y));
                    const int steps = static_cast<int>(std::ceil((x2 - x1) / step));
                    for (int i = 1; i <= steps; ++i) {
                        wave.lineTo(QPointF(std::min(x1 + i * step, x2),
                                            i % 2 == 1 ? y - amplitude : y + amplitude));
                    }
                    painter->drawPath(wave);
                }
                break;
            }
            case HighlightKind::Todo:
            case HighlightKind::CompletedTodo:
            case HighlightKind::Note: {
                // An icon in the margin next to the text, level with the marker's first line
                const QColor color = highlight.kind == HighlightKind::Note ? colors.noteHighlight
                                     : highlight.kind == HighlightKind::Todo
                                         ? colors.todoHighlight
                                         : colors.completedTodo;
                const QColor solid(color.red(), color.green(), color.blue());
                const QTextLine& line = pieces.front().line;
                const int slot = lineIcons[{paragraph, line.lineNumber()}]++;
                const double iconSize = MARKER_ICON_SIZE * scale;
                const double pitch = iconSize + MARKER_ICON_GAP * scale;
                const double centerY = widgetY + (line.y() + line.height() / 2.0) * scale;
                const QRectF iconRect(originX - (slot + 1) * pitch, centerY - iconSize / 2.0,
                                      iconSize, iconSize);
                if (highlight.kind == HighlightKind::Note) {
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(solid);
                    painter->drawEllipse(iconRect);
                } else {
                    // A checkbox, ticked when the TODO is completed
                    painter->setPen(QPen(solid, std::max(1.0, scale)));
                    painter->setBrush(Qt::NoBrush);
                    painter->drawRect(iconRect);
                    if (highlight.kind == HighlightKind::CompletedTodo) {
                        painter->drawLine(
                            QPointF(iconRect.left() + 0.2 * iconSize, iconRect.center().y()),
                            QPointF(iconRect.center().x(), iconRect.bottom() - 0.2 * iconSize));
                        painter->drawLine(
                            QPointF(iconRect.center().x(), iconRect.bottom() - 0.2 * iconSize),
                            QPointF(iconRect.right() - 0.1 * iconSize, iconRect.top() + 0.2 * iconSize));
                    }
                }
                break;
            }
            case HighlightKind::SearchMatch:
            case HighlightKind::CurrentSearchMatch:
            case HighlightKind::SpokenWord:
                break;
        }
    }
    painter->restore();
}

QRectF EditorRenderPipeline::cursorPaintRect() const {
    QRectF rect = cursorRect();
    if (rect.isEmpty() || !m_textSource || m_cursorStyle == CursorStyle::Line) return rect;

    // Block and underline cursors are as wide as the character at the cursor, with its
    // format and the zoom
    const double scale = m_context.computed.viewScale;
    rect.setWidth(caretCharWidth(m_cursorPosition) * scale);
    if (m_cursorStyle == CursorStyle::Underline) {
        // A thin bar under the character
        rect.setTop(rect.bottom() - 2.0 * scale);
    }
    return rect;
}

double EditorRenderPipeline::caretCharWidth(const CursorPosition& position) const {
    const double averageWidth =
        QFontMetricsF(m_context.computed.effectiveFont).averageCharWidth();
    if (!m_textSource || position.paragraph < 0 ||
        static_cast<size_t>(position.paragraph) >= m_textSource->paragraphCount()) {
        return averageWidth;
    }
    QTextLayout* layout = m_textSource->layout(static_cast<size_t>(position.paragraph));
    if (!layout || layout->lineCount() == 0) {
        return averageWidth;
    }
    const QTextLine line = layout->lineForTextPosition(position.offset);
    if (!line.isValid() || position.offset >= line.textStart() + line.textLength()) {
        return averageWidth;
    }
    const int next = layout->nextCursorPosition(position.offset);
    const double width = std::abs(line.cursorToX(next) - line.cursorToX(position.offset));
    return width > 0.0 ? width : averageWidth;
}

void EditorRenderPipeline::renderCursor(QPainter* painter) {
    if (!m_context.cursor.visible || !m_context.cursor.blinkState) return;

    const QRectF rect = cursorPaintRect();
    if (rect.isEmpty()) return;

    painter->fillRect(rect, m_context.colors.cursor);
    if (m_cursorStyle != CursorStyle::Block) return;

    // The character under the block is drawn again in the background color, so it stays
    // readable
    const auto paragraph = static_cast<size_t>(m_cursorPosition.paragraph);
    QTextLayout* layout = m_textSource->layout(paragraph);
    const QTextLine line = layout ? layout->lineForTextPosition(m_cursorPosition.offset) : QTextLine();
    if (!line.isValid()) return;

    painter->save();
    painter->setClipRect(rect, Qt::IntersectClip);
    painter->setPen(m_context.colors.background);
    painter->translate(m_context.computed.originX, paragraphWidgetY(paragraph));
    if (m_context.computed.viewScale != 1.0) {
        painter->scale(m_context.computed.viewScale, m_context.computed.viewScale);
    }
    line.draw(painter, layout->position());
    painter->restore();
}

// =============================================================================
// Text and Pages
// =============================================================================

void EditorRenderPipeline::renderText(QPainter* painter, const QRect& clipRect) {
    // Only the visible paragraphs (firstVisibleParagraph..lastVisibleParagraph): O(visible)
    // instead of O(n) -- ~30 draw calls vs ~3000.

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
                renderParagraphSelection(painter, static_cast<size_t>(para), startOffset,
                                         endOffset, paragraphWidgetY(static_cast<size_t>(para)));
            }
        }
    }

    // Highlights: the backgrounds under the text like the selection, the marks over it
    const std::vector<ParagraphHighlight> highlights = visibleHighlights(clipRect);
    renderHighlightBackgrounds(painter, highlights);

    // Paragraph text (already viewport-culled internally)
    renderParagraphs(painter, clipRect);

    renderHighlightMarks(painter, highlights);

    // Cursor and drop caret (only if their paragraph is in visible range)
    const auto isVisible = [this](int paragraph) {
        return paragraph >= 0 &&
               static_cast<size_t>(paragraph) >= m_context.computed.firstVisibleParagraph &&
               static_cast<size_t>(paragraph) <= m_context.computed.lastVisibleParagraph;
    };
    if (m_context.cursor.visible && m_context.cursor.blinkState && m_textSource &&
        isVisible(m_cursorPosition.paragraph)) {
        renderCursor(painter);
    }
    if (m_dropCaret && isVisible(m_dropCaret->paragraph)) {
        painter->fillRect(caretRect(*m_dropCaret), m_context.colors.cursor);
    }
}

void EditorRenderPipeline::renderPages(QPainter* painter, const QRect& clipRect) {
    const auto& computed = m_context.computed;
    const double scale = computed.viewScale;

    // Pages whose sheets reach into the clip rect
    const double clipTop = widgetToDocument(QPointF(0.0, clipRect.top())).y();
    const double clipBottom = widgetToDocument(QPointF(0.0, clipRect.bottom() + 1.0)).y();
    const int firstPage = pageAtDocumentY(clipTop);
    const int lastPage = pageAtDocumentY(clipBottom);

    // Page styling from context
    constexpr double shadowOffsetX = 4.0;
    constexpr double shadowOffsetY = 4.0;
    constexpr double shadowBlur = 8.0;

    QColor borderColor = m_context.colors.text;
    borderColor.setAlpha(30);
    QColor numberColor = m_context.colors.text;
    numberColor.setAlpha(150);
    // Page numbers are drawn in page units, scaled with the page
    QFont numberFont = m_context.font;
    numberFont.setPointSizeF(m_context.font.pointSizeF() * PAGE_NUMBER_FONT_SCALE);

    for (int page = firstPage; page <= lastPage; ++page) {
        const double sheetTop = pageTextTop(page) - computed.marginTop;
        const QRectF sheet(documentToWidget(QPointF(-computed.marginLeft, sheetTop)),
                           QSizeF(computed.pageWidthPixels * scale,
                                  computed.pageHeightPixels * scale));

        // Page shadow
        if (m_context.pageMode.showPageBreaks) {
            const QRectF shadowRect = sheet.translated(shadowOffsetX, shadowOffsetY);
            for (int i = 0; i < 4; ++i) {
                QColor shadow = m_context.pageMode.pageShadow;
                shadow.setAlpha(shadow.alpha() / (i + 1));
                const double expand = shadowBlur * (i + 1) / 4.0;
                painter->fillRect(shadowRect.adjusted(-expand, -expand, expand, expand), shadow);
            }
        }

        // Paper and its edge
        painter->fillRect(sheet, m_context.colors.background);
        painter->save();
        QPen borderPen(borderColor);
        borderPen.setWidthF(1.0);
        painter->setPen(borderPen);
        painter->drawRect(sheet);
        painter->restore();

        // Text frame border if enabled
        if (m_context.showTextFrameBorder) {
            const QRectF textRect(documentToWidget(QPointF(0.0, pageTextTop(page))),
                                  QSizeF(computed.textWidth * scale,
                                         computed.textAreaHeight * scale));
            painter->save();
            QPen framePen(m_context.textFrameBorderColor);
            framePen.setWidth(m_context.textFrameBorderWidth);
            painter->setPen(framePen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(textRect);
            painter->restore();
        }

        // Page number at the bottom centre, numbered from 1 in each chapter
        if (m_context.pageMode.showPageNumbers) {
            painter->save();
            painter->translate(sheet.topLeft());
            painter->scale(scale, scale);
            painter->setFont(numberFont);
            painter->setPen(numberColor);
            const QRectF bottomMargin(0.0, computed.pageHeightPixels - computed.marginBottom,
                                      computed.pageWidthPixels, computed.marginBottom);
            painter->drawText(bottomMargin, Qt::AlignCenter, QString::number(page + 1));
            painter->restore();
        }
    }
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

    // Calculate visible range from the document y shown at the top and bottom edges
    const double viewTop = widgetToDocument(QPointF(0.0, 0.0)).y();
    const double viewBottom = widgetToDocument(QPointF(0.0, m_context.viewportSize.height())).y();

    m_context.computed.firstVisibleParagraph = m_textSource->paragraphAtY(viewTop);
    m_context.computed.lastVisibleParagraph = m_textSource->paragraphAtY(viewBottom);

    // Clamp to valid range
    size_t count = m_textSource->paragraphCount();
    if (m_context.computed.lastVisibleParagraph >= count && count > 0) {
        m_context.computed.lastVisibleParagraph = count - 1;
    }
}

QRectF EditorRenderPipeline::lineBox(const QTextLine& line) const {
    return KalahariTextDocumentLayout::lineBox(line,
                                               m_textSource ? m_textSource->lineSpacing() : 1.0);
}

double EditorRenderPipeline::paragraphWidgetY(size_t index) const {
    const double docY = m_textSource ? m_textSource->paragraphY(index) : 0.0;
    return documentToWidget(QPointF(0.0, docY)).y();
}

// =============================================================================
// Geometry: document <-> widget, pages
// =============================================================================

QPointF EditorRenderPipeline::documentToWidget(const QPointF& point) const {
    const auto& computed = m_context.computed;
    const double scale = computed.viewScale;
    return QPointF(computed.originX + point.x() * scale,
                   computed.originY + (point.y() - m_context.scrollY) * scale);
}

QPointF EditorRenderPipeline::widgetToDocument(const QPointF& point) const {
    const auto& computed = m_context.computed;
    const double scale = computed.viewScale;
    return QPointF((point.x() - computed.originX) / scale,
                   (point.y() - computed.originY) / scale + m_context.scrollY);
}

int EditorRenderPipeline::pageCount() const {
    if (m_context.viewMode != ViewMode::Page || !m_textSource) {
        return 1;
    }
    return std::max(1, m_textSource->pageCount());
}

int EditorRenderPipeline::pageAtDocumentY(double y) const {
    // Sheet i spans [i * pitch - top margin, i * pitch - top margin + page height); the
    // gap below a sheet counts to it
    const auto& computed = m_context.computed;
    if (computed.pagePitch <= 0.0) {
        return 0;
    }
    const double page = std::floor((y + computed.marginTop) / computed.pagePitch);
    return static_cast<int>(std::clamp(page, 0.0, static_cast<double>(pageCount() - 1)));
}

double EditorRenderPipeline::pageTextTop(int page) const {
    return page * m_context.computed.pagePitch;
}

double EditorRenderPipeline::maxScrollX() const {
    if (m_context.viewMode != ViewMode::Page) {
        return 0.0;
    }
    return std::max(0.0, m_context.computed.contentWidth -
                             std::max(0.0, m_context.viewportSize.width() - m_context.scrollBarWidth));
}

CursorPosition EditorRenderPipeline::positionFromPoint(const QPointF& point) const {
    if (!m_textSource || m_textSource->paragraphCount() == 0) {
        return CursorPosition{0, 0};
    }

    // The document position under the point; the layout has the lines where they are
    // shown, on their pages too
    const QPointF docPoint = widgetToDocument(point);
    size_t paraIndex = m_textSource->paragraphAtY(docPoint.y());
    if (paraIndex >= m_textSource->paragraphCount()) {
        paraIndex = m_textSource->paragraphCount() - 1;
    }

    QTextLayout* layout = m_textSource->layout(paraIndex);
    if (!layout || layout->lineCount() == 0) {
        return CursorPosition{static_cast<int>(paraIndex), 0};
    }

    // Line whose box covers the point (the nearest one above or below the lines)
    const double localY = docPoint.y() - m_textSource->paragraphY(paraIndex);
    const QTextLine line = layout->lineAt(
        KalahariTextDocumentLayout::lineIndexAt(*layout, localY, m_textSource->lineSpacing()));
    const int offset = line.xToCursor(docPoint.x(), QTextLine::CursorBetweenCharacters);

    return CursorPosition{static_cast<int>(paraIndex), offset};
}

}  // namespace kalahari::editor
