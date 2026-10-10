/// @file book_editor_view.cpp
/// @brief BookEditor: scrolling, view modes, zoom, pages, typewriter scrolling, Focus, Distraction-Free mode

#include <kalahari/editor/book_editor.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/text_statistics.h>
#include <kalahari/editor/render_context.h>
#include <QAbstractTextDocumentLayout>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <QDateTime>
#include <QTextLine>
#include <QEasingCurve>
#include <QMouseEvent>
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

// The factor of one zoom step (Ctrl+wheel notch, Zoom In/Out; the range is
// MIN_ZOOM_FACTOR..MAX_ZOOM_FACTOR)
constexpr double ZOOM_STEP = 1.1;

// Rounds of placing the cursor line at a height of the view (the typewriter focus height
// after a jump, its place on the screen after a change of view), each laying out the
// paragraphs it brought into view
constexpr int MAX_CURSOR_PLACING_ROUNDS = 4;

// Distance of the Distraction-Free texts (word count, hint, clock) from the view's edges
constexpr qreal DISTRACTION_FREE_TEXT_MARGIN = 20.0;

namespace {

/// @brief A scroll bar of the editor: every press on it stays with it
///
/// Qt 6.9 passes a press a scroll bar does not use (one of the right button, or one on a
/// bar without a range) on to the widget under it: the editor would take it for a click in
/// the text, moving the cursor or selecting a word
class EditorScrollBar : public QScrollBar {
public:
    using QScrollBar::QScrollBar;

protected:
    void mousePressEvent(QMouseEvent* event) override {
        QScrollBar::mousePressEvent(event);
        event->accept();
    }
};

}  // namespace

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

        emit viewModeChanged(mode);

        // Every view has the page's width, margins and zoom, so the line breaks stay: the
        // pipeline places the lines on the pages or one under another and lays the text out
        // again. Scroll anchoring keeps the text at the top of the view in place; the
        // cursor line, while it is in the view, keeps its place on the screen instead.
        if (m_renderPipeline && m_viewportManager) {
            const QRectF caretBefore = m_renderPipeline->caretRect(m_cursorPosition);
            const bool caretShown = !caretBefore.isNull() && caretBefore.bottom() > 0.0 &&
                                    caretBefore.top() < static_cast<double>(height());
            m_renderPipeline->setConfigViewMode(mode);
            updateScrollBarRange();
            // Placing the line can bring paragraphs with estimated heights into view, and
            // laying them out moves it, so a few rounds
            for (int round = 0; caretShown && round < MAX_CURSOR_PLACING_ROUNDS; ++round) {
                m_renderPipeline->ensureVisibleLaidOut();
                const QRectF caret = m_renderPipeline->caretRect(m_cursorPosition);
                const double shift =
                    (caret.top() - caretBefore.top()) / m_viewportManager->viewScale();
                if (caret.isNull() || std::abs(shift) < 0.5) {
                    break;
                }
                setScrollOffset(scrollOffset() + shift);
            }
        }
        updateScrollBarRange();
        updatePageInfo();

        // The view stays where the writer is: no jump to a cursor scrolled out of view
        resetCursorBlink();
        update();
    }
}

// =============================================================================
// Zoom Control
// =============================================================================

double BookEditor::zoomFactor() const {
    if (m_renderPipeline) {
        return m_renderPipeline->zoomFactor();
    }
    return 1.0;
}

void BookEditor::setZoomFactor(double factor) {
    // A zoom asked for stays (see shrinkToPageWidthOnFirstShow())
    m_shrinkOnFirstShow = false;
    // Zooming keeps the middle of the view on the same text
    applyZoom(factor, QPointF(width() / 2.0, height() / 2.0));
}

void BookEditor::applyZoom(double factor, const QPointF& fixedPoint) {
    if (!m_renderPipeline) {
        return;
    }
    factor = qBound(MIN_ZOOM_FACTOR, factor, MAX_ZOOM_FACTOR);
    const QPointF docPoint = m_renderPipeline->widgetToDocument(fixedPoint);

    // Every view zooms the page as a whole: only the painter scale changes, the line breaks
    // stay
    m_renderPipeline->setConfigZoom(factor);
    updateScrollBarRange();

    // The document point under fixedPoint stays there
    const RenderContext& ctx = m_renderPipeline->context();
    const double scale = ctx.computed.viewScale;
    m_renderPipeline->setConfigScrollX(
        (ctx.pageMode.pageSpacing + ctx.computed.marginLeft + docPoint.x()) * scale -
        fixedPoint.x());
    setScrollOffset(docPoint.y() - (fixedPoint.y() - ctx.computed.originY) / scale);
    updateHorizontalScrollBar();

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
    updateScrollBarRange();
    updateHorizontalScrollBar();
    updateTypewriterScroll(false);
    applyFirstShowShrink();  // the page's new width, until the first paint
    update();
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

double BookEditor::pageWidthZoom() const
{
    // The page (or the endless page) with the gap on both sides fills the width left of
    // the scroll bar
    const RenderContext& ctx = m_renderPipeline->context();
    const double pagesWidth =
        (ctx.computed.pageWidthPixels + 2.0 * ctx.pageMode.pageSpacing) * ctx.paperScale;
    return pagesWidth > 0.0 ? std::max(0.0, width() - ctx.scrollBarWidth) / pagesWidth : 0.0;
}

void BookEditor::zoomToPageWidth()
{
    if (!m_renderPipeline) {
        return;
    }
    m_shrinkOnFirstShow = false;
    const double zoom = pageWidthZoom();
    if (zoom > 0.0) {
        applyZoom(zoom, QPointF(width() / 2.0, height() / 2.0));
    }
}

void BookEditor::shrinkToPageWidthOnFirstShow()
{
    m_shrinkOnFirstShow = true;
    applyFirstShowShrink();
}

void BookEditor::applyFirstShowShrink()
{
    if (!m_shrinkOnFirstShow || !m_renderPipeline) {
        return;
    }
    const double widthZoom = pageWidthZoom();
    if (widthZoom <= 0.0) {
        return;
    }
    // At most the zoom of the settings: a page that fits the view keeps it
    const double factor = qBound(MIN_ZOOM_FACTOR,
                                 std::min(m_appearance.pageLayout.zoomLevel, widthZoom),
                                 MAX_ZOOM_FACTOR);
    if (std::abs(factor - zoomFactor()) >= 0.001) {
        // The text at the top of the view stays there: the start of a new text
        applyZoom(factor, QPointF(width() / 2.0, 0.0));
    }
}

void BookEditor::zoomToWholePage()
{
    if (!m_renderPipeline) {
        return;
    }
    m_shrinkOnFirstShow = false;
    // The page with the gaps around it fits the view; the page view shows the cursor's
    // page, the continuous views take the same zoom around the middle of the view
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
        // Ctrl+scroll = zoom, applied at every notch, around the mouse pointer
        if (event->modifiers() & Qt::ControlModifier) {
            if (angleDelta.y() != 0) {
                const qreal zoomDelta = angleDelta.y() > 0 ? ZOOM_STEP : (1.0 / ZOOM_STEP);
                m_shrinkOnFirstShow = false;
                applyZoom(zoomFactor() * zoomDelta, event->position());
            }
            event->accept();
            return;
        }

        // Sideways (a horizontal wheel, or Shift with a vertical one): a page wider than
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
        // (document units divide by the view scale)
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
    m_verticalScrollBar = new EditorScrollBar(Qt::Vertical, this);
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

    // The page keeps clear of the scroll bar over the right edge
    if (m_renderPipeline) {
        m_renderPipeline->setConfigScrollBarWidth(m_verticalScrollBar->sizeHint().width());
    }

    // Horizontal scrollbar: while the zoomed page is wider than the view
    m_horizontalScrollBar = new EditorScrollBar(Qt::Horizontal, this);
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

    // Scroll range in document units: the view shows height / zoom of the document
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

    // Shown while the zoomed page is wider than the view
    const double maxX = m_renderPipeline->maxScrollX();
    const bool needed = maxX >= 1.0;
    m_updatingScrollBar = true;
    m_horizontalScrollBar->setRange(0, needed ? static_cast<int>(std::ceil(maxX)) : 0);
    m_horizontalScrollBar->setPageStep(std::max(1, width()));
    m_horizontalScrollBar->setValue(static_cast<int>(std::lround(m_renderPipeline->context().scrollX)));
    m_updatingScrollBar = false;

    const int barHeight = m_horizontalScrollBar->sizeHint().height();
    const int barRight = m_verticalScrollBar ? m_verticalScrollBar->sizeHint().width() : 0;
    m_horizontalScrollBar->setGeometry(0, height() - barHeight, std::max(0, width() - barRight),
                                       barHeight);
    m_horizontalScrollBar->setVisible(needed && !m_distractionFree);
}

void BookEditor::setHorizontalScrollOffset(double x)
{
    if (!m_renderPipeline) {
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

    // Page size in points, margins in millimetres
    const QSizeF sizeMm = m_appearance.pageLayout.pageSizeMm();
    const QSizeF sizePoints(sizeMm.width() * POINTS_PER_INCH / MM_PER_INCH,
                            sizeMm.height() * POINTS_PER_INCH / MM_PER_INCH);
    const PageMarginsConfig& margins = m_appearance.pageMargins;
    m_renderPipeline->setConfigPageLayout(
        sizePoints, QMarginsF(margins.left, margins.top, margins.right, margins.bottom),
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
    for (int round = 0; round < MAX_CURSOR_PLACING_ROUNDS; ++round) {
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

// =============================================================================
// Focus
// =============================================================================

bool BookEditor::isFocusModeEnabled() const
{
    return m_appearance.focusMode.enabled;
}

void BookEditor::setFocusModeEnabled(bool enabled)
{
    if (m_appearance.focusMode.enabled == enabled) {
        return;
    }
    m_appearance.focusMode.enabled = enabled;
    if (m_renderPipeline) {
        m_renderPipeline->setConfigFocus(enabled);
    }
    update();
    emit focusModeChanged(enabled);
}

// =============================================================================
// Distraction-Free writing
// =============================================================================

bool BookEditor::isDistractionFree() const
{
    return m_distractionFree;
}

void BookEditor::setDistractionFree(bool enabled)
{
    if (m_distractionFree == enabled) {
        return;
    }
    m_distractionFree = enabled;

    // The scroll bars hide with the window's bars (the wheel and the keys still scroll)
    if (m_verticalScrollBar != nullptr) {
        m_verticalScrollBar->setVisible(!enabled);
    }
    updateHorizontalScrollBar();

    if (enabled) {
        // The texts at the edges show at first, then fade out
        m_uiOpacity = 1.0;
        startUiFade();
    } else {
        if (m_uiFadeTimer != nullptr) {
            m_uiFadeTimer->stop();
        }
        m_uiOpacity = 0.0;
    }
    update();
    emit distractionFreeModeChanged(enabled);
}

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
    if (!m_distractionFree) {
        return;
    }

    // The whole editor: its scroll bars are hidden meanwhile
    const QRectF view(rect());

    // The sides of the view darken toward its edges, in the color of the sheets' shadow,
    // outside the middle part as wide as appearance().distractionFree.textWidth
    const qreal middle = std::clamp(m_appearance.distractionFree.textWidth, 0.0, 1.0);
    const qreal sideWidth = view.width() * (1.0 - middle) / 2.0;
    if (sideWidth > 0.0) {
        const QColor shade = m_appearance.colors.pageShadow;
        QLinearGradient leftGradient(view.left(), 0.0, view.left() + sideWidth, 0.0);
        leftGradient.setColorAt(0.0, shade);
        leftGradient.setColorAt(1.0, Qt::transparent);
        painter.fillRect(QRectF(view.left(), view.top(), sideWidth, view.height()), leftGradient);

        QLinearGradient rightGradient(view.right() - sideWidth, 0.0, view.right(), 0.0);
        rightGradient.setColorAt(0.0, Qt::transparent);
        rightGradient.setColorAt(1.0, shade);
        painter.fillRect(QRectF(view.right() - sideWidth, view.top(), sideWidth, view.height()),
                         rightGradient);
    }

    // The texts at the edges, while they have not faded out
    if (m_uiOpacity <= 0.0) {
        return;
    }

    // In the dimmed text color of the paper (the color of the paragraphs Focus dims)
    QColor textColor = m_appearance.colors.focusInactiveColor(m_appearance.colorMode);
    textColor.setAlphaF(static_cast<float>(textColor.alphaF() * m_uiOpacity));
    painter.setFont(m_appearance.typography.uiFont);
    painter.setPen(textColor);

    const QRectF area = view.adjusted(DISTRACTION_FREE_TEXT_MARGIN, DISTRACTION_FREE_TEXT_MARGIN,
                                      -DISTRACTION_FREE_TEXT_MARGIN, -DISTRACTION_FREE_TEXT_MARGIN);
    if (m_appearance.distractionFree.showWordCount) {
        painter.drawText(area, Qt::AlignHCenter | Qt::AlignBottom,
                         tr("Words: %1").arg(wordCount()));
    }
    if (m_appearance.distractionFree.showClock) {
        painter.drawText(area, Qt::AlignRight | Qt::AlignTop,
                         QDateTime::currentDateTime().toString(QStringLiteral("HH:mm")));
    }
}

}  // namespace kalahari::editor
