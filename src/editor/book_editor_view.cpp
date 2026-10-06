/// @file book_editor_view.cpp
/// @brief BookEditor: scrolling, view modes, zoom, pages, typewriter scrolling, Focus and Distraction-Free modes

#include <kalahari/editor/book_editor.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/text_statistics.h>
#include <kalahari/editor/render_context.h>
#include <QAbstractTextDocumentLayout>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <QDateTime>
#include <QTextLine>
#include <QEasingCurve>
#include <QPainter>
#include <QVariantAnimation>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>
#include <algorithm>

namespace kalahari::editor {

// Wheel scroll step in pixels (approximate line height)
constexpr qreal WHEEL_SCROLL_STEP = 60.0;

// View pixels one mouse wheel notch scrolls
constexpr qreal WHEEL_PIXELS_PER_NOTCH = 40.0;

// Zoom range and the factor of one zoom step (Ctrl+wheel notch, Zoom In/Out)
constexpr double MIN_ZOOM_FACTOR = 0.25;
constexpr double MAX_ZOOM_FACTOR = 4.0;
constexpr double ZOOM_STEP = 1.1;

// Rounds of placing the cursor line at the typewriter focus height after a jump, each
// laying out the paragraphs it brought into view
constexpr int MAX_TYPEWRITER_ROUNDS = 4;

// Phase 11: Old architecture methods removed (setDocument, document, layoutManager, scrollManager)
// Use fromKml()/toKml() for document operations
// Use ViewportManager for scroll operations

// =============================================================================
// Scrolling
// =============================================================================

QScrollBar* BookEditor::verticalScrollBar() const
{
    return m_verticalScrollBar;
}

qreal BookEditor::scrollOffset() const
{
    return m_viewportManager ? m_viewportManager->scrollPosition() : 0.0;
}

void BookEditor::setScrollOffset(qreal offset)
{
    if (!m_viewportManager) return;

    qreal oldOffset = m_viewportManager->scrollPosition();
    m_viewportManager->setScrollPosition(offset);
    qreal newOffset = m_viewportManager->scrollPosition();

    if (oldOffset != newOffset) {
        syncScrollBarValue();
        emit scrollOffsetChanged(newOffset);
        updatePipelineScroll();  // Phase 14: lightweight scroll only
        update();  // Request repaint
        // Wherever the view stops, the cursor shows at once instead of in the middle of a
        // blink
        resetCursorBlink();
    }
}

void BookEditor::scrollBy(qreal delta, bool animated)
{
    qreal targetOffset = scrollOffset() + delta;
    scrollTo(targetOffset, animated);
}

void BookEditor::scrollTo(qreal offset, bool animated)
{
    // Phase 11: Use ViewportManager for max scroll
    qreal maxScroll = m_viewportManager ? m_viewportManager->maxScrollPosition() : 0.0;
    offset = qBound(0.0, offset, maxScroll);

    if (animated && m_smoothScrollingEnabled) {
        startScrollAnimation(offset);
    } else {
        stopScrollAnimation();
        setScrollOffset(offset);
    }
}

bool BookEditor::isSmoothScrollingEnabled() const
{
    return m_smoothScrollingEnabled;
}

void BookEditor::setSmoothScrollingEnabled(bool enabled)
{
    m_smoothScrollingEnabled = enabled;
    if (!enabled) {
        stopScrollAnimation();
    }
}

int BookEditor::smoothScrollDuration() const
{
    return m_smoothScrollDuration;
}

void BookEditor::setSmoothScrollDuration(int duration)
{
    m_smoothScrollDuration = qMax(0, duration);
}

// =============================================================================
// View Mode (Phase 5.1)
// =============================================================================

ViewMode BookEditor::viewMode() const
{
    return m_viewMode;
}

void BookEditor::setViewMode(ViewMode mode)
{
    auto& logger = core::Logger::getInstance();

    if (m_viewMode != mode) {
        ViewMode oldMode = m_viewMode;
        m_viewMode = mode;

        // Log view mode transition (OpenSpec #00042 Task 7.19 Issue #6)
        logger.info("BookEditor::setViewMode: {} -> {}",
                    static_cast<int>(oldMode), static_cast<int>(mode));

        // When entering Distraction-Free mode, show UI initially
        if (mode == ViewMode::DistractionFree) {
            m_uiOpacity = 1.0;
            startUiFade();
            emit distractionFreeModeChanged(true);
        } else if (oldMode == ViewMode::DistractionFree) {
            // Leaving distraction-free mode
            if (m_uiFadeTimer != nullptr) {
                m_uiFadeTimer->stop();
            }
            emit distractionFreeModeChanged(false);
        }

        emit viewModeChanged(mode);

        // The pipeline recomputes the view (margins, pages, zoom mode, scroll padding) and
        // lays the text out again; scroll anchoring keeps the text at the top of the view,
        // also between the scroll modes and the pages
        if (m_renderPipeline) {
            m_renderPipeline->setConfigViewMode(mode, getZoomModeForViewMode());
        }
        updateScrollBarRange();
        updatePageInfo();

        // Ensure cursor is visible after view mode change
        ensureCursorVisible();
        update();
    }
}

// =============================================================================
// Zoom Control
// =============================================================================

ZoomMode BookEditor::getZoomModeForViewMode() const {
    // Pages zoom as a whole (their line breaks stay); the scroll modes lay the text out
    // again at the zoomed font size, wrapped to the view
    return m_viewMode == ViewMode::Page ? ZoomMode::PageScaling : ZoomMode::FontScaling;
}

double BookEditor::zoomFactor() const {
    if (m_renderPipeline) {
        return m_renderPipeline->zoomFactor();
    }
    return 1.0;
}

void BookEditor::setZoomFactor(double factor) {
    // Zooming keeps the middle of the view on the same text
    applyZoom(factor, QPointF(width() / 2.0, height() / 2.0));
}

void BookEditor::applyZoom(double factor, const QPointF& fixedPoint) {
    if (!m_renderPipeline) {
        return;
    }
    factor = qBound(MIN_ZOOM_FACTOR, factor, MAX_ZOOM_FACTOR);
    const ZoomMode mode = getZoomModeForViewMode();
    const QPointF docPoint = m_renderPipeline->widgetToDocument(fixedPoint);

    // Font scaling wraps only the visible paragraphs before the next paint (scroll
    // anchoring keeps the text at the top of the view), page scaling only changes the
    // painter scale
    m_renderPipeline->setConfigZoom(factor, mode);
    updateScrollBarRange();

    if (mode == ZoomMode::PageScaling) {
        // The document point under fixedPoint stays there
        const RenderContext& ctx = m_renderPipeline->context();
        const double scale = ctx.computed.viewScale;
        m_renderPipeline->setConfigScrollX(
            (ctx.pageMode.pageSpacing + ctx.computed.marginLeft + docPoint.x()) * scale -
            fixedPoint.x());
        setScrollOffset(docPoint.y() - (fixedPoint.y() - ctx.computed.originY) / scale);
        updateHorizontalScrollBar();
    }
    // Typewriter scrolling keeps the cursor line at the focus height instead
    updateTypewriterScroll(false);
    update();
    emit zoomChanged(factor);
}

void BookEditor::zoomIn() {
    setZoomFactor(zoomFactor() * ZOOM_STEP);
}

void BookEditor::zoomOut() {
    setZoomFactor(zoomFactor() / ZOOM_STEP);
}

void BookEditor::zoomReset() {
    setZoomFactor(1.0);
}

// =============================================================================
// Page Navigation (Phase 5.3-5.5)
// =============================================================================

int BookEditor::currentPage() const
{
    // The page holding the cursor, as word processors count it
    if (!m_textBuffer || m_viewMode != ViewMode::Page || !m_renderPipeline) {
        return 0;
    }
    return m_renderPipeline->pageAtDocumentY(getCursorDocumentY()) + 1;
}

int BookEditor::totalPages() const
{
    if (!m_textBuffer || m_viewMode != ViewMode::Page || !m_renderPipeline) {
        return 0;
    }
    return m_renderPipeline->pageCount();
}

void BookEditor::goToPage(int page)
{
    if (!m_textBuffer || m_viewMode != ViewMode::Page || !m_renderPipeline) {
        return;
    }
    if (page < 1 || page > totalPages()) {
        return;
    }

    // Estimated heights above a page shift its text: the blocks down to the page are laid
    // out first, so the page is where its text will stay
    if (auto* layout = qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())) {
        layout->ensureLaidOutTo(m_renderPipeline->pageTextTop(page - 1));
    }
    page = std::min(page, totalPages());

    // The cursor goes to the first line of the page, and the sheet's top to the top of the
    // view
    const double textTop = m_renderPipeline->pageTextTop(page - 1);
    const int position = m_textBuffer->documentLayout()->hitTest(QPointF(0.0, textTop),
                                                                   Qt::FuzzyHit);
    const QTextBlock block = m_textBuffer->findBlock(std::max(0, position));
    if (block.isValid()) {
        setCursorPosition({block.blockNumber(), std::max(0, position - block.position())});
    }
    // Sheet top at the top of the view (typewriter scrolling has put the cursor line at
    // its focus height instead)
    if (!m_appearance.typewriter.enabled) {
        scrollToPageTop(page);
    }
    updatePageInfo();
    update();
}

void BookEditor::scrollToPageTop(int page)
{
    if (!m_renderPipeline || m_viewMode != ViewMode::Page) {
        return;
    }
    // The sheet's top at the gap below the view's top edge, as the first page shows at the
    // start of the chapter
    const RenderContext& ctx = m_renderPipeline->context();
    setScrollOffset(m_renderPipeline->pageTextTop(page - 1) - ctx.computed.marginTop -
                    ctx.pageMode.pageSpacing + ctx.computed.originY / ctx.computed.viewScale);
}

void BookEditor::setPaperScale(double scale)
{
    if (!m_renderPipeline) {
        return;
    }
    m_renderPipeline->setConfigPaperScale(scale);
    if (m_viewMode == ViewMode::Page) {
        updateScrollBarRange();
        updateHorizontalScrollBar();
        updateTypewriterScroll(false);
        update();
    }
}

double BookEditor::paperScale() const
{
    return m_renderPipeline ? m_renderPipeline->context().paperScale : 1.0;
}

double BookEditor::paperScaleOf(const QScreen* screen)
{
    return screen ? paperScaleFor(screen->physicalDotsPerInch(), screen->logicalDotsPerInch())
                  : 1.0;
}

double BookEditor::paperScaleFor(double physicalDpi, double logicalDpi)
{
    // A screen that reports no size, or a made-up one, gives a ratio far from any real
    // screen's (from about 70 to 300 pixels per inch at 100% to 300% display scaling)
    if (physicalDpi <= 0.0 || logicalDpi <= 0.0) {
        return 1.0;
    }
    const double ratio = physicalDpi / logicalDpi;
    return ratio >= 0.5 && ratio <= 3.0 ? ratio : 1.0;
}

void BookEditor::zoomToPageWidth()
{
    if (!m_renderPipeline || m_viewMode != ViewMode::Page) {
        return;
    }
    // The page with the gap on both sides fills the width left of the scroll bar
    const RenderContext& ctx = m_renderPipeline->context();
    const double pagesWidth =
        (ctx.computed.pageWidthPixels + 2.0 * ctx.pageMode.pageSpacing) * ctx.paperScale;
    if (pagesWidth > 0.0) {
        applyZoom((width() - ctx.scrollBarWidth) / pagesWidth,
                  QPointF(width() / 2.0, height() / 2.0));
    }
}

void BookEditor::zoomToWholePage()
{
    if (!m_renderPipeline || m_viewMode != ViewMode::Page) {
        return;
    }
    // The page with the gaps around it fits the view; the cursor's page is shown
    const RenderContext& ctx = m_renderPipeline->context();
    const double gaps = 2.0 * ctx.pageMode.pageSpacing;
    const double pageWidth = (ctx.computed.pageWidthPixels + gaps) * ctx.paperScale;
    const double pageHeight = (ctx.computed.pageHeightPixels + gaps) * ctx.paperScale;
    if (pageWidth <= 0.0 || pageHeight <= 0.0) {
        return;
    }
    const int page = std::max(1, currentPage());
    applyZoom(std::min((width() - ctx.scrollBarWidth) / pageWidth, height() / pageHeight),
              QPointF(width() / 2.0, height() / 2.0));
    if (!m_appearance.typewriter.enabled) {
        scrollToPageTop(page);
    }
}

void BookEditor::updatePageInfo()
{
    const int current = currentPage();
    const int total = totalPages();
    if (total != m_lastTotalPages) {
        m_lastTotalPages = total;
        emit totalPagesChanged(total);
    }
    if (current != m_lastCurrentPage) {
        m_lastCurrentPage = current;
        emit currentPageChanged(current);
    }
}

void BookEditor::nextPage()
{
    int current = currentPage();
    int total = totalPages();
    if (current < total) {
        goToPage(current + 1);
    }
}

void BookEditor::previousPage()
{
    int current = currentPage();
    if (current > 1) {
        goToPage(current - 1);
    }
}

void BookEditor::wheelEvent(QWheelEvent* event)
{
    QPoint angleDelta = event->angleDelta();
    if (!angleDelta.isNull()) {
        // Ctrl+scroll = zoom, applied at every notch; pages zoom around the mouse pointer
        if (event->modifiers() & Qt::ControlModifier) {
            if (angleDelta.y() != 0) {
                const qreal zoomDelta = angleDelta.y() > 0 ? ZOOM_STEP : (1.0 / ZOOM_STEP);
                applyZoom(zoomFactor() * zoomDelta, event->position());
            }
            event->accept();
            return;
        }

        // Sideways (a horizontal wheel, or Shift with a vertical one): pages wider than
        // the view
        const int sideways = angleDelta.x() != 0 ? angleDelta.x()
                             : (event->modifiers() & Qt::ShiftModifier) ? angleDelta.y()
                                                                       : 0;
        if (sideways != 0) {
            if (m_renderPipeline && m_renderPipeline->maxScrollX() > 0.0) {
                setHorizontalScrollOffset(m_renderPipeline->context().scrollX -
                                          sideways / 8.0 / 15.0 * WHEEL_PIXELS_PER_NOTCH);
            }
            event->accept();
            return;
        }

        // Standard wheel scroll: 1 notch = 15 degrees, a fixed number of view pixels
        // (document units divide by the view scale: page mode zooms)
        const qreal scale = m_viewportManager ? m_viewportManager->viewScale() : 1.0;
        const qreal delta = -angleDelta.y() / 8.0 / 15.0 * WHEEL_PIXELS_PER_NOTCH / scale;
        setScrollOffset(scrollOffset() + delta);
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

// =============================================================================
// Private Slots
// =============================================================================

void BookEditor::onScrollBarValueChanged(int value)
{
    // Avoid recursive updates
    if (m_updatingScrollBar) {
        return;
    }

    // Stop any running animation when user manually scrolls
    stopScrollAnimation();

    // Update scroll manager with new offset
    setScrollOffset(static_cast<qreal>(value));
}

void BookEditor::onScrollAnimationValueChanged(const QVariant& value)
{
    setScrollOffset(value.toReal());
}

void BookEditor::setupScrollBar()
{
    // Create vertical scrollbar (with an arrow pointer, not the editor's I-beam)
    m_verticalScrollBar = new QScrollBar(Qt::Vertical, this);
    m_verticalScrollBar->setCursor(Qt::ArrowCursor);
    m_verticalScrollBar->setMinimum(0);
    m_verticalScrollBar->setMaximum(0);
    m_verticalScrollBar->setSingleStep(static_cast<int>(WHEEL_SCROLL_STEP));
    m_verticalScrollBar->setPageStep(height());

    // Position scrollbar on right edge
    // Note: Position will be updated in resizeEvent
    m_verticalScrollBar->setGeometry(
        width() - m_verticalScrollBar->sizeHint().width(),
        0,
        m_verticalScrollBar->sizeHint().width(),
        height()
    );

    // Connect scrollbar value changes
    connect(m_verticalScrollBar, &QScrollBar::valueChanged,
            this, &BookEditor::onScrollBarValueChanged);

    // The pages keep clear of the scroll bar over the right edge
    if (m_renderPipeline) {
        m_renderPipeline->setConfigScrollBarWidth(m_verticalScrollBar->sizeHint().width());
    }

    // Horizontal scrollbar: page mode, while the zoomed pages are wider than the view
    m_horizontalScrollBar = new QScrollBar(Qt::Horizontal, this);
    m_horizontalScrollBar->setCursor(Qt::ArrowCursor);
    m_horizontalScrollBar->setRange(0, 0);
    m_horizontalScrollBar->setSingleStep(static_cast<int>(WHEEL_SCROLL_STEP));
    m_horizontalScrollBar->hide();
    connect(m_horizontalScrollBar, &QScrollBar::valueChanged, this, [this](int value) {
        if (!m_updatingScrollBar) {
            setHorizontalScrollOffset(value);
        }
    });

    // Note: Scroll animation is created lazily in startScrollAnimation()
}

void BookEditor::updateScrollBarRange()
{
    if (m_verticalScrollBar == nullptr) {
        return;
    }

    // Scroll range in document units: page mode shows height / zoom of the document
    double maxOffset = 0.0;
    double pageStep = static_cast<double>(height());
    if (m_viewportManager) {
        maxOffset = m_viewportManager->maxScrollPosition();
        pageStep = m_viewportManager->visibleDocumentHeight();
    }

    // Update scrollbar without triggering signals
    m_updatingScrollBar = true;
    m_verticalScrollBar->setMaximum(static_cast<int>(std::ceil(maxOffset)));
    m_verticalScrollBar->setPageStep(std::max(1, static_cast<int>(pageStep)));
    m_updatingScrollBar = false;

    // Position scrollbar on right edge (also update position on resize)
    int scrollBarWidth = m_verticalScrollBar->sizeHint().width();
    m_verticalScrollBar->setGeometry(
        width() - scrollBarWidth,
        0,
        scrollBarWidth,
        height()
    );

    // A shorter range (zoom out, a taller window) leaves no space below the end of the text
    if (m_textBuffer && scrollOffset() > maxOffset) {
        setScrollOffset(maxOffset);
    }
    syncScrollBarValue();
    updateHorizontalScrollBar();
}

void BookEditor::updateHorizontalScrollBar()
{
    if (m_horizontalScrollBar == nullptr || !m_renderPipeline) {
        return;
    }

    // Shown while the zoomed pages are wider than the view
    const double maxX = m_renderPipeline->maxScrollX();
    const bool needed = m_viewMode == ViewMode::Page && maxX >= 1.0;
    m_updatingScrollBar = true;
    m_horizontalScrollBar->setRange(0, needed ? static_cast<int>(std::ceil(maxX)) : 0);
    m_horizontalScrollBar->setPageStep(std::max(1, width()));
    m_horizontalScrollBar->setValue(static_cast<int>(std::lround(m_renderPipeline->context().scrollX)));
    m_updatingScrollBar = false;

    const int barHeight = m_horizontalScrollBar->sizeHint().height();
    const int barRight = m_verticalScrollBar ? m_verticalScrollBar->sizeHint().width() : 0;
    m_horizontalScrollBar->setGeometry(0, height() - barHeight, std::max(0, width() - barRight),
                                       barHeight);
    m_horizontalScrollBar->setVisible(needed);
}

void BookEditor::setHorizontalScrollOffset(double x)
{
    if (!m_renderPipeline || m_viewMode != ViewMode::Page) {
        return;
    }
    const double oldX = m_renderPipeline->context().scrollX;
    m_renderPipeline->setConfigScrollX(x);  // clamps it
    if (std::abs(m_renderPipeline->context().scrollX - oldX) > 0.001) {
        updateHorizontalScrollBar();
        update();
    }
}

void BookEditor::syncScrollBarValue()
{
    if (m_verticalScrollBar == nullptr) {
        return;
    }

    m_updatingScrollBar = true;
    m_verticalScrollBar->setValue(static_cast<int>(scrollOffset()));
    m_updatingScrollBar = false;
}

void BookEditor::applyPageLayout()
{
    if (!m_renderPipeline) {
        return;
    }

    // Page size in points, margins in millimetres. With mirror margins every page has the
    // first page's margins for now: the layout gives all pages one text width.
    const QSizeF sizeMm = m_appearance.pageLayout.pageSizeMm();
    const QSizeF sizePoints(sizeMm.width() * POINTS_PER_INCH / MM_PER_INCH,
                            sizeMm.height() * POINTS_PER_INCH / MM_PER_INCH);
    const PageMarginsConfig& margins = m_appearance.pageMargins;
    m_renderPipeline->setConfigPageLayout(
        sizePoints,
        QMarginsF(margins.effectiveLeft(1), margins.top, margins.effectiveRight(1), margins.bottom),
        m_appearance.pageLayout.pageGap);
    m_renderPipeline->setConfigShowPageNumbers(m_appearance.pageLayout.showPageNumbers);
}

void BookEditor::startScrollAnimation(qreal targetOffset, int durationMs)
{
    // Lazily create the animation on first use
    if (m_scrollAnimation == nullptr) {
        // A plain value animation: a QPropertyAnimation needs a target object and
        // asserts in debug Qt builds without one
        m_scrollAnimation = new QVariantAnimation(this);
        m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);

        // Connect animation value changes
        connect(m_scrollAnimation, &QVariantAnimation::valueChanged,
                this, &BookEditor::onScrollAnimationValueChanged);
    }

    // Clamp target to valid range
    targetOffset = qBound(0.0, targetOffset, m_viewportManager->maxScrollPosition());

    // Stop any existing animation
    m_scrollAnimation->stop();

    // Configure animation
    m_scrollAnimation->setDuration(durationMs >= 0 ? durationMs : m_smoothScrollDuration);
    m_scrollAnimation->setStartValue(scrollOffset());
    m_scrollAnimation->setEndValue(targetOffset);

    // Start animation
    m_scrollAnimation->start();
}

void BookEditor::stopScrollAnimation()
{
    if (m_scrollAnimation != nullptr) {
        m_scrollAnimation->stop();
    }
}

// =============================================================================
// Typewriter Mode (Phase 5.2)
// =============================================================================

qreal BookEditor::getCursorDocumentY() const
{
    // Phase 11: Use QTextLayout for cursor position
    if (!m_textBuffer) {
        return 0.0;
    }

    QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    if (!block.isValid()) {
        return 0.0;
    }

    QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);
    if (!layout) {
        return 0.0;
    }

    // Use document layout for correct Y position
    QRectF blockRect = m_textBuffer->documentLayout()->blockBoundingRect(block);
    qreal paraY = blockRect.y();

    int offsetInBlock = qMin(m_cursorPosition.offset, block.length() - 1);
    if (offsetInBlock < 0) offsetInBlock = 0;

    QTextLine line = layout->lineForTextPosition(offsetInBlock);
    if (line.isValid()) {
        paraY += line.y();
    }

    return paraY;
}

void BookEditor::updateTypewriterScroll(bool animate)
{
    if (!m_appearance.typewriter.enabled || !m_textBuffer ||
        !m_renderPipeline || !m_viewportManager) {
        return;
    }

    // The middle of the cursor line goes to the focus height of the view. The scroll
    // padding has room for it below the last line; near the start of the text the view
    // stops at the top and the line stays above the focus height.
    const double focusY =
        std::clamp(m_appearance.typewriter.focusPosition, 0.0, 1.0) * static_cast<double>(height());
    const auto targetScroll = [&]() -> std::optional<double> {
        const QRectF caret = m_renderPipeline->caretRect(m_cursorPosition);
        if (caret.isNull()) {
            return std::nullopt;
        }
        return std::clamp(
            scrollOffset() + (caret.center().y() - focusY) / m_viewportManager->viewScale(), 0.0,
            m_viewportManager->maxScrollPosition());
    };
    const std::optional<double> target = targetScroll();
    if (!target) {
        return;
    }
    const double distance = std::abs(*target - scrollOffset());
    if (distance < 0.5) {
        return;
    }

    // A short move glides, a jump (search, Ctrl+End) is immediate
    if (animate && m_appearance.typewriter.smoothScroll &&
        distance <= m_viewportManager->visibleDocumentHeight()) {
        startScrollAnimation(*target, m_appearance.typewriter.scrollDuration);
        return;
    }
    stopScrollAnimation();
    setScrollOffset(*target);

    // A jump may land among paragraphs with estimated heights: once the view there is laid
    // out, the line is placed again. Placing it can bring more of them into view, so a few
    // rounds.
    for (int round = 0; round < MAX_TYPEWRITER_ROUNDS; ++round) {
        m_renderPipeline->ensureVisibleLaidOut();
        const std::optional<double> corrected = targetScroll();
        if (!corrected || std::abs(*corrected - scrollOffset()) < 0.5) {
            break;
        }
        setScrollOffset(*corrected);
    }
}

bool BookEditor::isTypewriterEnabled() const
{
    return m_appearance.typewriter.enabled;
}

void BookEditor::setTypewriterEnabled(bool enabled)
{
    if (m_appearance.typewriter.enabled == enabled) {
        return;
    }
    m_appearance.typewriter.enabled = enabled;
    applyTypewriter();
    emit typewriterChanged(enabled);
}

void BookEditor::applyTypewriter()
{
    if (!m_renderPipeline) {
        return;
    }

    // The room below the text changes with it (the pipeline gives the viewport the new
    // scroll padding), the text stays where it is. Then the cursor line goes to the focus
    // height at once, as far as the text above it allows.
    m_renderPipeline->setConfigTypewriter(m_appearance.typewriter.enabled,
                                          m_appearance.typewriter.focusPosition);
    updateScrollBarRange();
    updateTypewriterScroll(false);
    update();
}

// Phase 13.5: paintPageMode() removed - rendering now handled by EditorRenderPipeline
// See render() method in editor_render_pipeline.cpp

// =============================================================================
// Focus Mode (Phase 5.6)
// =============================================================================

BookEditor::FocusedRange BookEditor::getFocusedRange() const
{
    FocusedRange range;

    // Phase 11.6: Use QTextDocument directly instead of LazyLayoutManager
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return range;
    }
    QTextDocument* doc = m_textBuffer.get();
    if (!doc) {
        return range;
    }

    int paraIndex = m_cursorPosition.paragraph;
    size_t paraCount = m_textBuffer->blockCount();

    if (paraIndex < 0 || paraIndex >= static_cast<int>(paraCount)) {
        paraIndex = 0;
    }

    // Default to paragraph scope
    range.startParagraph = paraIndex;
    range.endParagraph = paraIndex;
    range.startLine = 0;
    range.endLine = -1;  // Will be set below

    // Get layout from QTextBlock
    QTextBlock block = doc->findBlockByNumber(paraIndex);
    QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);

    // Determine range based on focus scope
    switch (m_appearance.focusMode.scope) {
        case FocusModeSettings::FocusScope::Line: {
            // Focus on the specific line containing the cursor
            if (layout && layout->lineCount() > 0) {
                int lineIndex = 0;
                if (m_cursorPosition.offset > 0) {
                    QTextLine line = layout->lineForTextPosition(m_cursorPosition.offset);
                    if (line.isValid()) {
                        lineIndex = line.lineNumber();
                    }
                }
                range.startLine = lineIndex;
                range.endLine = lineIndex;
            } else {
                range.startLine = 0;
                range.endLine = 0;
            }
            break;
        }

        case FocusModeSettings::FocusScope::Sentence:
            // Sentence detection is complex - treat as paragraph for now
            [[fallthrough]];

        case FocusModeSettings::FocusScope::Paragraph:
        default:
            // Focus on the entire paragraph
            if (layout && layout->lineCount() > 0) {
                range.endLine = layout->lineCount() - 1;
            } else {
                range.endLine = 0;
            }
            break;
    }

    return range;
}

void BookEditor::paintFocusOverlay(QPainter& painter)
{
    // Only draw overlay in Focus view mode
    if (m_viewMode != ViewMode::Focus) {
        return;
    }

    // Phase 11.6: Use QTextDocument directly
    if (!m_textBuffer || !m_viewportManager) {
        return;
    }
    QTextDocument* doc = m_textBuffer.get();
    if (!doc || m_textBuffer->blockCount() == 0) {
        return;
    }

    FocusedRange focusedRange = getFocusedRange();

    // Get viewport info
    int viewportHeight = height();
    double scrollY = m_viewportManager->scrollPosition();
    // Phase 12.6: Use computed margins (not input)
    const auto& ctx = m_renderPipeline->context();
    double marginTop = ctx.computed.marginTop;

    // Y position of the focused paragraph, from the document layout
    const double focusY = m_viewportManager->paragraphY(
        static_cast<size_t>(std::max(0, focusedRange.startParagraph)));

    // Get focused paragraph height (or line height if Line scope)
    double focusHeight = 0.0;
    double focusTop = focusY;

    if (focusedRange.startParagraph >= 0 &&
        focusedRange.startParagraph < static_cast<int>(m_textBuffer->blockCount())) {

        QTextBlock block = doc->findBlockByNumber(focusedRange.startParagraph);
        QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);

        if (m_appearance.focusMode.scope == FocusModeSettings::FocusScope::Line) {
            // For line scope, calculate specific line bounds
            if (layout && focusedRange.startLine >= 0 && focusedRange.startLine < layout->lineCount()) {
                QTextLine line = layout->lineAt(focusedRange.startLine);
                if (line.isValid()) {
                    const QRectF box = KalahariTextDocumentLayout::lineBox(
                        line, m_renderPipeline->textSource()->lineSpacing());
                    focusTop = focusY + box.top();
                    focusHeight = box.height();
                }
            }
        } else {
            // For paragraph scope, use entire paragraph
            focusHeight = m_viewportManager->paragraphHeight(
                static_cast<size_t>(focusedRange.startParagraph));
        }
    }

    // Calculate screen positions (apply margins and scroll offset)
    int widgetFocusTop = static_cast<int>(marginTop + focusTop - scrollY);
    int widgetFocusBottom = static_cast<int>(marginTop + focusTop + focusHeight - scrollY);

    // Calculate overlay opacity (inverted: high dimOpacity = more dimming)
    int overlayAlpha = static_cast<int>((1.0 - m_appearance.focusMode.dimOpacity) * 255.0);
    overlayAlpha = qBound(0, overlayAlpha, 255);

    // Create overlay color based on editor background
    QColor overlayColor = m_appearance.colors.editorBackground;
    overlayColor.setAlpha(overlayAlpha);

    // Draw overlay above focused area
    if (widgetFocusTop > 0) {
        QRectF topOverlay(0, 0, width(), widgetFocusTop);
        painter.fillRect(topOverlay, overlayColor);
    }

    // Draw overlay below focused area
    if (widgetFocusBottom < viewportHeight) {
        QRectF bottomOverlay(0, widgetFocusBottom, width(), viewportHeight - widgetFocusBottom);
        painter.fillRect(bottomOverlay, overlayColor);
    }

    // Draw highlight behind focused area (optional)
    if (m_appearance.focusMode.highlightBackground) {
        // Use cached margins from ctx - no DPI query needed
        QColor highlightColor = m_appearance.colors.accent;
        highlightColor.setAlpha(25);  // Very subtle

        QRectF focusRect(static_cast<qreal>(ctx.computed.marginLeft),
                         static_cast<qreal>(widgetFocusTop),
                         static_cast<qreal>(width() - ctx.computed.marginLeft),
                         static_cast<qreal>(widgetFocusBottom - widgetFocusTop));
        painter.fillRect(focusRect, highlightColor);
    }
}

// =============================================================================
// Distraction-Free Mode (Phase 5.7)
// =============================================================================

void BookEditor::startUiFade()
{
    if (m_uiFadeTimer == nullptr) {
        return;
    }

    // Stop any existing timer
    m_uiFadeTimer->stop();

    // Start fade timer with configured timeout
    int timeout = m_appearance.distractionFree.uiFadeTimeout;
    if (timeout > 0) {
        m_uiFadeTimer->start(timeout);
    }
}

void BookEditor::paintDistractionFreeOverlay(QPainter& painter)
{
    // Only draw overlay in Distraction-Free view mode
    if (m_viewMode != ViewMode::DistractionFree) {
        return;
    }

    // Calculate content area based on textWidth setting
    // (This is preparation for Phase 7 - actual text centering will be done there)
    qreal viewportWidth = static_cast<qreal>(width());
    qreal textWidth = viewportWidth * m_appearance.distractionFree.textWidth;
    qreal sideMargin = (viewportWidth - textWidth) / 2.0;

    // Draw subtle gradient/vignette on sides (optional visual touch)
    if (sideMargin > 0) {
        // Create a subtle vignette effect on the sides
        QColor vignetteColor = m_appearance.colors.editorBackground;
        vignetteColor.setAlpha(30);  // Very subtle

        // Left vignette
        QLinearGradient leftGradient(0, 0, sideMargin, 0);
        leftGradient.setColorAt(0.0, vignetteColor);
        leftGradient.setColorAt(1.0, Qt::transparent);
        painter.fillRect(QRectF(0, 0, sideMargin, height()), leftGradient);

        // Right vignette
        QLinearGradient rightGradient(width() - sideMargin, 0, width(), 0);
        rightGradient.setColorAt(0.0, Qt::transparent);
        rightGradient.setColorAt(1.0, vignetteColor);
        painter.fillRect(QRectF(width() - sideMargin, 0, sideMargin, height()), rightGradient);
    }

    // Only draw overlays if UI is visible (opacity > 0)
    if (m_uiOpacity <= 0.0) {
        return;
    }

    // Set up text color with opacity
    QColor textColor = m_appearance.colors.textSecondary;
    textColor.setAlphaF(m_uiOpacity);

    // Draw word count at bottom center
    if (m_appearance.distractionFree.showWordCount) {
        QString countText = tr("%1 words").arg(wordCount());

        // Use UI font, slightly smaller
        QFont countFont = m_appearance.typography.uiFont;
        countFont.setPointSize(10);
        painter.setFont(countFont);
        painter.setPen(textColor);

        // Calculate position - bottom center with some padding
        QFontMetrics countFm(countFont);
        int countTextWidth = countFm.horizontalAdvance(countText);
        int countTextHeight = countFm.height();
        int countPadding = 20;

        QRectF countRect(
            (width() - countTextWidth) / 2.0,
            height() - countTextHeight - countPadding,
            countTextWidth,
            countTextHeight
        );

        painter.drawText(countRect, Qt::AlignCenter, countText);
    }

    // Draw clock at top right
    if (m_appearance.distractionFree.showClock) {
        QString timeText = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));

        // Use UI font
        QFont clockFont = m_appearance.typography.uiFont;
        clockFont.setPointSize(10);
        painter.setFont(clockFont);
        painter.setPen(textColor);

        // Calculate position - top right with some padding
        QFontMetrics clockFm(clockFont);
        int clockTextWidth = clockFm.horizontalAdvance(timeText);
        int clockTextHeight = clockFm.height();
        int clockPadding = 20;

        QRectF clockRect(
            width() - clockTextWidth - clockPadding,
            clockPadding,
            clockTextWidth,
            clockTextHeight
        );

        painter.drawText(clockRect, Qt::AlignCenter, timeText);
    }
}

}  // namespace kalahari::editor
