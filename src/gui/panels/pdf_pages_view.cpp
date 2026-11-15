// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pdf_pages_view.cpp
/// @brief The pages of a PDF file as the preview shows them

#include "kalahari/gui/panels/pdf_pages_view.h"

#include <QClipboard>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfDocumentRenderOptions>
#include <QPdfLink>
#include <QPdfSearchModel>
#include <QScrollBar>
#include <QTransform>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace kalahari::gui {

namespace {

/// Room around the pages, in pixels
constexpr int MARGIN = 12;

/// Room between the pages, in pixels
constexpr int GAP = 12;

/// How far the shade of a page falls, in pixels
constexpr int SHADE = 2;

/// Two zooms closer than this are the same
constexpr double SAME_ZOOM = 0.001;

/// The pages drawn that are kept, in kilobytes: about 192 MB
constexpr int KEPT_KILOBYTES = 192 * 1024;

/// A page drawn larger than this many bytes is not kept: only the part shown is drawn
constexpr qint64 LARGEST_KEPT = 24LL * 1024 * 1024;

/// Bytes of a pixel of a page drawn
constexpr qint64 PIXEL_BYTES = 4;

/// Dimmed pages: their white is this light
constexpr float DIMMED_LIGHTNESS = 0.62F;

/// Pages with their colors turned: their black is this light
constexpr float INVERTED_LIGHTNESS = 0.12F;

/// Pixels the arrows and the wheel move the pages by
constexpr int SCROLL_STEP = 40;

/// Points in an inch, the unit of a PDF page
constexpr double POINTS_PER_INCH = 72.0;

/// A page without a size in its file is taken as A4, in points
constexpr double A4_WIDTH = 595.0;
constexpr double A4_HEIGHT = 842.0;

/// Positions closer than this, in points, select nothing
constexpr double SAME_POSITION = 0.5;

/// @brief textBetween(): the steps in which the places beside the text go toward each other
constexpr int APPROACH_STEPS = 8;

/// How the pages are drawn: with the comments and the forms of the file
QPdfDocumentRenderOptions renderOptions() {
    QPdfDocumentRenderOptions options;
    options.setRenderFlags(QPdfDocumentRenderOptions::RenderFlag::Annotations);
    return options;
}

}  // namespace

const QList<double>& PdfPagesView::zoomSteps() {
    static const QList<double> steps = {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0};
    return steps;
}

PdfPagesView::PdfPagesView(QWidget* parent) : QAbstractScrollArea(parent) {
    setObjectName(QStringLiteral("pdfPagesView"));
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::NoFrame);
    viewport()->setMouseTracking(true);
    verticalScrollBar()->setSingleStep(SCROLL_STEP);
    horizontalScrollBar()->setSingleStep(SCROLL_STEP);
    m_images.setMaxCost(KEPT_KILOBYTES);
    m_colors.desk = palette().color(QPalette::Mid);
    m_colors.selection = palette().color(QPalette::Highlight);
    m_colors.selection.setAlpha(80);
    m_colors.searchMatch = m_colors.selection;
    m_colors.currentSearchMatch = m_colors.selection;
}

void PdfPagesView::setDocument(QPdfDocument* document) {
    // clang-analyzer takes the shared reference count of the QPointer for a use after free
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDelete)
    m_document = document;
    m_images.clear();
    m_textBounds.clear();
    m_selection.reset();
    m_selectionPage = -1;
    m_hitPage = -1;
    m_hitRectangles.clear();
    m_dragPage = -1;
    m_pointSizes.clear();
    if (m_document != nullptr) {
        for (int page = 0; page < m_document->pageCount(); ++page) {
            const QSizeF points = m_document->pagePointSize(page);
            m_pointSizes.append(points.isEmpty() ? QSizeF(A4_WIDTH, A4_HEIGHT) : points);
        }
    }
    m_page = 0;
    relayout();
    m_scrolling = true;
    verticalScrollBar()->setValue(0);
    horizontalScrollBar()->setValue(0);
    m_scrolling = false;
    emit selectionChanged();
    emit currentPageChanged(m_page);
}

void PdfPagesView::setSearchModel(QPdfSearchModel* model) {
    if (m_search != nullptr) {
        disconnect(m_search, nullptr, this, nullptr);
    }
    m_search = model;
    if (m_search != nullptr) {
        const auto repaint = [this]() { viewport()->update(); };
        connect(m_search, &QAbstractItemModel::rowsInserted, this, repaint);
        connect(m_search, &QAbstractItemModel::modelReset, this, repaint);
    }
    viewport()->update();
}

int PdfPagesView::pageCount() const {
    return static_cast<int>(m_pointSizes.size());
}

void PdfPagesView::setCurrentPage(int page) {
    const int count = pageCount();
    if (count == 0) {
        return;
    }
    page = std::clamp(page, 0, count - 1);
    if (m_layout == Layout::Continuous) {
        m_scrolling = true;
        verticalScrollBar()->setValue(m_rects.at(page).top() - MARGIN);
        m_scrolling = false;
        setPage(page);
        viewport()->update();
        return;
    }
    // One page or one spread at a time: the other row is laid out in its place
    const int before = m_page;
    const bool otherRow = rowOf(page) != rowOf(m_page);
    m_page = page;
    if (otherRow) {
        relayout();
    }
    m_scrolling = true;
    verticalScrollBar()->setValue(0);
    m_scrolling = false;
    if (page != before) {
        emit currentPageChanged(page);
    }
    viewport()->update();
}

void PdfPagesView::showPlace(int page, const QRectF& points) {
    setCurrentPage(page);
    const QRect place = toViewport(page, points).toAlignedRect();
    if (place.isEmpty() || viewport()->rect().contains(place)) {
        return;
    }
    // The place in the middle of the window, as far as the pages go
    m_scrolling = true;
    QScrollBar* vertical = verticalScrollBar();
    vertical->setValue(vertical->value() + place.center().y() - viewport()->height() / 2);
    if (!viewport()->rect().contains(place.left(), place.top()) ||
        !viewport()->rect().contains(place.right(), place.top())) {
        QScrollBar* horizontal = horizontalScrollBar();
        horizontal->setValue(horizontal->value() + place.center().x() - viewport()->width() / 2);
    }
    m_scrolling = false;
    viewport()->update();
}

void PdfPagesView::setPageLayout(Layout layout) {
    if (layout == m_layout) {
        return;
    }
    m_layout = layout;
    const int page = m_page;
    relayout();
    setCurrentPage(page);
}

void PdfPagesView::setZoom(double zoom) {
    const QList<double>& steps = zoomSteps();
    const double before = m_zoom;
    m_fit = Fit::None;
    m_zoom = std::clamp(zoom, steps.first(), steps.last());
    relayout();
    if (std::abs(before - m_zoom) > SAME_ZOOM) {
        emit zoomChanged(m_zoom);
    }
}

void PdfPagesView::setFit(Fit fit) {
    m_fit = fit;
    relayout();
}

void PdfPagesView::zoomIn() {
    const QList<double>& steps = zoomSteps();
    const auto next = std::find_if(steps.cbegin(), steps.cend(),
                                   [this](double step) { return step > m_zoom + SAME_ZOOM; });
    setZoom(next != steps.cend() ? *next : steps.last());
}

void PdfPagesView::zoomOut() {
    const QList<double>& steps = zoomSteps();
    const auto next = std::find_if(steps.crbegin(), steps.crend(),
                                   [this](double step) { return step < m_zoom - SAME_ZOOM; });
    setZoom(next != steps.crend() ? *next : steps.first());
}

void PdfPagesView::setLook(Look look) {
    if (look == m_look) {
        return;
    }
    m_look = look;
    m_images.clear();
    viewport()->update();
}

void PdfPagesView::setColors(const Colors& colors) {
    m_colors = colors;
    viewport()->update();
}

bool PdfPagesView::hasSelection() const {
    return m_selection.has_value() && !m_selection->text().isEmpty();
}

QString PdfPagesView::selectedText() const {
    return m_selection ? m_selection->text() : QString();
}

void PdfPagesView::select(int page, const QPointF& start, const QPointF& end) {
    if (m_document == nullptr || page < 0 || page >= pageCount()) {
        return;
    }
    const QPointF distance = end - start;
    if (std::abs(distance.x()) < SAME_POSITION && std::abs(distance.y()) < SAME_POSITION) {
        clearSelection();
        return;
    }
    std::optional<QPdfSelection> selection = textBetween(page, start, end);
    if (!selection) {
        clearSelection();
        return;
    }
    m_selection = std::move(selection);
    m_selectionPage = page;
    viewport()->update();
    emit selectionChanged();
}

std::optional<QPdfSelection> PdfPagesView::textBetween(int page, const QPointF& start,
                                                       const QPointF& end) {
    const auto found = [](const QPdfSelection& selection) {
        return selection.isValid() && !selection.text().isEmpty();
    };
    QPdfSelection selection = m_document->getSelection(page, start, end);
    if (found(selection)) {
        return selection;
    }
    // QPdfDocument finds a letter only near the place: from the margin, or between two
    // paragraphs, the places go to the text and toward each other
    const QRectF bounds = textBounds(page);
    if (bounds.isEmpty()) {
        return std::nullopt;
    }
    const auto within = [&bounds](const QPointF& place) {
        return QPointF(std::clamp(place.x(), bounds.left(), bounds.right()),
                       std::clamp(place.y(), bounds.top(), bounds.bottom()));
    };
    const QPointF from = within(start);
    const QPointF to = within(end);
    const QPointF step = (to - from) / APPROACH_STEPS;
    for (int steps = 0; steps < APPROACH_STEPS / 2; ++steps) {
        selection = m_document->getSelection(page, from + step * steps, to - step * steps);
        if (found(selection)) {
            return selection;
        }
    }
    return std::nullopt;
}

QRectF PdfPagesView::textBounds(int page) {
    if (const auto known = m_textBounds.constFind(page); known != m_textBounds.cend()) {
        return *known;
    }
    const QPdfSelection all = m_document->getAllText(page);
    const QRectF bounds = all.isValid() ? all.boundingRectangle() : QRectF();
    m_textBounds.insert(page, bounds);
    return bounds;
}

void PdfPagesView::selectHit(int page, const QList<QRectF>& rectangles) {
    if (rectangles.isEmpty()) {
        return;
    }
    // From the left edge of its first letter to the right edge of its last one, in the
    // middle of their lines
    const QRectF& first = rectangles.first();
    const QRectF& last = rectangles.last();
    select(page, QPointF(first.left() + SAME_POSITION, first.center().y()),
           QPointF(last.right() - SAME_POSITION, last.center().y()));
}

void PdfPagesView::selectPage(int page) {
    if (m_document == nullptr || page < 0 || page >= pageCount()) {
        return;
    }
    QPdfSelection selection = m_document->getAllText(page);
    if (!selection.isValid() || selection.text().isEmpty()) {
        clearSelection();
        return;
    }
    m_selection = std::move(selection);
    m_selectionPage = page;
    viewport()->update();
    emit selectionChanged();
}

void PdfPagesView::clearSelection() {
    if (!m_selection) {
        return;
    }
    m_selection.reset();
    m_selectionPage = -1;
    viewport()->update();
    emit selectionChanged();
}

QRect PdfPagesView::selectionRect() const {
    if (!m_selection || pageRect(m_selectionPage).isEmpty()) {
        return {};
    }
    return toViewport(m_selectionPage, m_selection->boundingRectangle()).toAlignedRect();
}

void PdfPagesView::setCurrentHit(int page, const QList<QRectF>& rectangles) {
    m_hitPage = page;
    m_hitRectangles = rectangles;
    viewport()->update();
}

QRect PdfPagesView::pageRect(int page) const {
    if (page < 0 || page >= m_rects.size() || m_rects.at(page).isEmpty()) {
        return {};
    }
    return m_rects.at(page).translated(origin());
}

int PdfPagesView::pageAt(const QPoint& position) const {
    const QPoint at = position - origin();
    for (int page = 0; page < m_rects.size(); ++page) {
        if (m_rects.at(page).contains(at)) {
            return page;
        }
    }
    return -1;
}

bool PdfPagesView::canCopyHere() const {
    return hasSelection();
}

void PdfPagesView::copyHere() {
    if (hasSelection()) {
        QGuiApplication::clipboard()->setText(selectedText());
    }
}

bool PdfPagesView::canSelectAllHere() const {
    return pageCount() > 0;
}

void PdfPagesView::selectAllHere() {
    selectPage(m_page);
}

QMetaObject::Connection PdfPagesView::whenSelectionChanges(QObject* context,
                                                           std::function<void()> update) {
    return connect(this, &PdfPagesView::selectionChanged, context, std::move(update));
}

void PdfPagesView::paintEvent(QPaintEvent* event) {
    QPainter painter(viewport());
    painter.fillRect(event->rect(), m_colors.desk);
    if (m_document == nullptr) {
        return;
    }
    const QColor shade = m_colors.desk.darker(150);
    for (int page = 0; page < m_rects.size(); ++page) {
        const QRect rect = pageRect(page);
        if (rect.isEmpty() || !rect.adjusted(0, 0, SHADE, SHADE).intersects(event->rect())) {
            continue;
        }
        painter.fillRect(rect.translated(SHADE, SHADE), shade);
        const QRect visible = event->rect().intersected(rect).translated(-rect.topLeft());
        const QImage image = pageImage(page, rect.size(), visible);
        if (image.isNull()) {
            painter.fillRect(rect, palette().color(QPalette::Base));
        } else if (image.deviceIndependentSize().toSize() == rect.size()) {
            painter.drawImage(rect.topLeft(), image);
        } else {
            painter.drawImage(rect.topLeft() + visible.topLeft(), image);
        }

        // The hits of the search, the hit gone to and the selection, over the text
        if (m_search != nullptr) {
            const QList<QPdfLink> hits = m_search->resultsOnPage(page);
            for (const QPdfLink& hit : hits) {
                for (const QRectF& rectangle : hit.rectangles()) {
                    painter.fillRect(toViewport(page, rectangle), m_colors.searchMatch);
                }
            }
        }
        if (page == m_hitPage) {
            for (const QRectF& rectangle : std::as_const(m_hitRectangles)) {
                painter.fillRect(toViewport(page, rectangle), m_colors.currentSearchMatch);
            }
        }
        if (m_selection && page == m_selectionPage) {
            const QSizeF points = m_pointSizes.at(page);
            const QTransform transform(rect.width() / points.width(), 0, 0,
                                       rect.height() / points.height(), rect.x(), rect.y());
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_colors.selection);
            for (const QPolygonF& polygon : m_selection->bounds()) {
                painter.drawPolygon(transform.map(polygon));
            }
        }
    }
}

void PdfPagesView::resizeEvent(QResizeEvent* event) {
    QAbstractScrollArea::resizeEvent(event);
    relayout();
}

void PdfPagesView::scrollContentsBy(int dx, int dy) {
    Q_UNUSED(dx)
    Q_UNUSED(dy)
    viewport()->update();
    followScroll();
}

void PdfPagesView::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }
    const QPoint position = event->position().toPoint();
    m_dragPage = pageAt(position);
    if (m_dragPage >= 0) {
        m_dragStart = toPoints(m_dragPage, position);
    }
    clearSelection();
    event->accept();
}

void PdfPagesView::mouseMoveEvent(QMouseEvent* event) {
    const QPoint position = event->position().toPoint();
    if (m_dragPage >= 0 && (event->buttons() & Qt::LeftButton)) {
        select(m_dragPage, m_dragStart, toPoints(m_dragPage, position));
    }
    const Qt::CursorShape shape = pageAt(position) >= 0 ? Qt::IBeamCursor : Qt::ArrowCursor;
    if (viewport()->cursor().shape() != shape) {
        viewport()->setCursor(shape);
    }
    event->accept();
}

void PdfPagesView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QAbstractScrollArea::mouseReleaseEvent(event);
        return;
    }
    const bool dragging = m_dragPage >= 0;
    m_dragPage = -1;
    if (dragging && hasSelection()) {
        emit selectionMade();
    }
    event->accept();
}

void PdfPagesView::wheelEvent(QWheelEvent* event) {
    const int delta = event->angleDelta().y();
    // Ctrl with the wheel zooms
    if (event->modifiers() & Qt::ControlModifier) {
        if (delta > 0) {
            zoomIn();
        } else if (delta < 0) {
            zoomOut();
        }
        event->accept();
        return;
    }
    // One page or one spread at a time: past its end the wheel goes to the next one
    if (m_layout != Layout::Continuous && delta != 0) {
        QScrollBar* bar = verticalScrollBar();
        const int row = rowOf(m_page);
        if (delta < 0 && bar->value() >= bar->maximum() && row + 1 < rows().size()) {
            moveRows(1);
            event->accept();
            return;
        }
        if (delta > 0 && bar->value() <= bar->minimum() && row > 0) {
            moveRows(-1);
            m_scrolling = true;
            bar->setValue(bar->maximum());
            m_scrolling = false;
            event->accept();
            return;
        }
    }
    QAbstractScrollArea::wheelEvent(event);
}

void PdfPagesView::keyPressEvent(QKeyEvent* event) {
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    const bool plain = modifiers == Qt::NoModifier;
    switch (event->key()) {
    case Qt::Key_PageDown:
        if (plain) {
            moveRows(1);
            return;
        }
        break;
    case Qt::Key_PageUp:
        if (plain) {
            moveRows(-1);
            return;
        }
        break;
    case Qt::Key_Home:
        if (plain || modifiers == Qt::ControlModifier) {
            setCurrentPage(0);
            return;
        }
        break;
    case Qt::Key_End:
        if (plain || modifiers == Qt::ControlModifier) {
            setCurrentPage(pageCount() - 1);
            return;
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (plain) {
            emit activated();
            return;
        }
        break;
    case Qt::Key_Escape:
        if (plain && hasSelection()) {
            clearSelection();
            return;
        }
        event->ignore();
        return;
    case Qt::Key_F10:
        if (modifiers == Qt::ShiftModifier) {
            const QRect selection = selectionRect();
            const QPoint at = selection.isEmpty() ? viewport()->rect().center()
                                                  : selection.bottomLeft();
            emit selectionMenuRequested(viewport()->mapToGlobal(at));
            return;
        }
        break;
    default:
        break;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

void PdfPagesView::contextMenuEvent(QContextMenuEvent* event) {
    QPoint at = event->globalPos();
    if (event->reason() != QContextMenuEvent::Mouse) {
        const QRect selection = selectionRect();
        at = viewport()->mapToGlobal(selection.isEmpty() ? viewport()->rect().center()
                                                         : selection.bottomLeft());
    }
    emit selectionMenuRequested(at);
    event->accept();
}

void PdfPagesView::relayout() {
    const double before = m_zoom;
    // The place shown: how far down the page shown the window begins
    double down = 0.0;
    const bool continuous = m_layout == Layout::Continuous;
    if (continuous && m_page < m_rects.size() && !m_rects.at(m_page).isEmpty()) {
        const QRect rect = m_rects.at(m_page);
        down = static_cast<double>(verticalScrollBar()->value() - rect.top()) / rect.height();
    }

    const QList<QList<int>> all = rows();
    QList<QList<int>> shown;
    if (continuous) {
        shown = all;
    } else if (!all.isEmpty()) {
        shown.append(all.at(std::clamp(rowOf(m_page), 0, static_cast<int>(all.size()) - 1)));
    }
    if (m_fit != Fit::None) {
        m_zoom = fittedZoom(shown);
    }

    const double scale = pixelsAtFullSize() * m_zoom;
    const auto pixels = [scale](double points) {
        return std::max(1, static_cast<int>(std::lround(points * scale)));
    };
    m_rects = QVector<QRect>(pageCount());
    QVector<int> widths;
    int widest = 0;
    for (const QList<int>& row : std::as_const(shown)) {
        int width = GAP * (static_cast<int>(row.size()) - 1);
        for (int page : row) {
            width += pixels(m_pointSizes.at(page).width());
        }
        widths.append(width);
        widest = std::max(widest, width);
    }
    int top = MARGIN;
    for (int index = 0; index < shown.size(); ++index) {
        const QList<int>& row = shown.at(index);
        int height = 0;
        for (int page : row) {
            height = std::max(height, pixels(m_pointSizes.at(page).height()));
        }
        int left = MARGIN + (widest - widths.at(index)) / 2;
        for (int page : row) {
            const QSize size(pixels(m_pointSizes.at(page).width()),
                             pixels(m_pointSizes.at(page).height()));
            m_rects[page] = QRect(QPoint(left, top + (height - size.height()) / 2), size);
            left += size.width() + GAP;
        }
        top += height + GAP;
    }
    m_size = shown.isEmpty() ? QSize() : QSize(widest + 2 * MARGIN, top - GAP + MARGIN);

    m_scrolling = true;
    const QSize room = viewport()->size();
    QScrollBar* vertical = verticalScrollBar();
    vertical->setRange(0, std::max(0, m_size.height() - room.height()));
    vertical->setPageStep(room.height());
    QScrollBar* horizontal = horizontalScrollBar();
    horizontal->setRange(0, std::max(0, m_size.width() - room.width()));
    horizontal->setPageStep(room.width());
    if (continuous && m_page < m_rects.size() && !m_rects.at(m_page).isEmpty()) {
        const QRect rect = m_rects.at(m_page);
        vertical->setValue(rect.top() + static_cast<int>(std::lround(down * rect.height())));
    }
    m_scrolling = false;
    viewport()->update();
    if (std::abs(before - m_zoom) > SAME_ZOOM) {
        emit zoomChanged(m_zoom);
    }
}

QList<QList<int>> PdfPagesView::rows() const {
    QList<QList<int>> result;
    const int count = pageCount();
    if (m_layout != Layout::Two) {
        for (int page = 0; page < count; ++page) {
            result.append(QList<int>{page});
        }
        return result;
    }
    // As in a book: the first page alone on the right, then the even pages on the left
    if (count > 0) {
        result.append(QList<int>{0});
    }
    for (int page = 1; page < count; page += 2) {
        QList<int> row{page};
        if (page + 1 < count) {
            row.append(page + 1);
        }
        result.append(row);
    }
    return result;
}

int PdfPagesView::rowOf(int page) const {
    if (m_layout != Layout::Two) {
        return page;
    }
    return page <= 0 ? 0 : (page + 1) / 2;
}

double PdfPagesView::pixelsAtFullSize() const {
    return logicalDpiX() / POINTS_PER_INCH;
}

double PdfPagesView::fittedZoom(const QList<QList<int>>& shownRows) const {
    const QSize room = viewport()->size() - QSize(2 * MARGIN, 2 * MARGIN);
    if (shownRows.isEmpty() || room.width() <= 0 || room.height() <= 0) {
        return m_zoom;
    }
    const double full = pixelsAtFullSize();
    const auto widthZoom = [this, full, room](const QList<int>& row) {
        double points = 0.0;
        for (int page : row) {
            points += m_pointSizes.at(page).width();
        }
        const int gaps = GAP * (static_cast<int>(row.size()) - 1);
        return (room.width() - gaps) / (points * full);
    };
    double zoom = 0.0;
    if (m_fit == Fit::PageWidth) {
        // The widest row fits
        zoom = widthZoom(shownRows.first());
        for (const QList<int>& row : shownRows) {
            zoom = std::min(zoom, widthZoom(row));
        }
    } else {
        // The row of the page shown fits whole
        const QList<int>* row = &shownRows.first();
        for (const QList<int>& each : shownRows) {
            if (each.contains(m_page)) {
                row = &each;
            }
        }
        double height = 0.0;
        for (int page : *row) {
            height = std::max(height, m_pointSizes.at(page).height());
        }
        zoom = std::min(widthZoom(*row), room.height() / (height * full));
    }
    const QList<double>& steps = zoomSteps();
    return std::clamp(zoom, steps.first(), steps.last());
}

QPoint PdfPagesView::origin() const {
    const QSize room = viewport()->size();
    const int x = m_size.width() < room.width() ? (room.width() - m_size.width()) / 2
                                                : -horizontalScrollBar()->value();
    const int y = m_size.height() < room.height() ? (room.height() - m_size.height()) / 2
                                                  : -verticalScrollBar()->value();
    return {x, y};
}

QRectF PdfPagesView::toViewport(int page, const QRectF& points) const {
    const QRect rect = pageRect(page);
    if (rect.isEmpty()) {
        return {};
    }
    const QSizeF size = m_pointSizes.at(page);
    const double x = rect.width() / size.width();
    const double y = rect.height() / size.height();
    return {rect.x() + points.x() * x, rect.y() + points.y() * y, points.width() * x,
            points.height() * y};
}

QPointF PdfPagesView::toPoints(int page, const QPoint& position) const {
    const QRect rect = pageRect(page);
    if (rect.isEmpty()) {
        return {};
    }
    const QSizeF size = m_pointSizes.at(page);
    const double x = (position.x() - rect.x()) * size.width() / rect.width();
    const double y = (position.y() - rect.y()) * size.height() / rect.height();
    return {std::clamp(x, 0.0, size.width()), std::clamp(y, 0.0, size.height())};
}

QImage PdfPagesView::pageImage(int page, const QSize& size, const QRect& visible) {
    const qreal ratio = devicePixelRatioF();
    const QSize pixels = (QSizeF(size) * ratio).toSize();
    if (static_cast<qint64>(pixels.width()) * pixels.height() * PIXEL_BYTES > LARGEST_KEPT) {
        // Too large to keep: the part shown only
        const QRect part = QRectF(QPointF(visible.topLeft()) * ratio, QSizeF(visible.size()) * ratio)
                               .toAlignedRect()
                               .intersected(QRect(QPoint(0, 0), pixels));
        if (part.isEmpty()) {
            return {};
        }
        QPdfDocumentRenderOptions options = renderOptions();
        options.setScaledSize(pixels);
        options.setScaledClipRect(part);
        QImage image = m_document->render(page, part.size(), options);
        applyLook(image);
        image.setDevicePixelRatio(ratio);
        return image;
    }
    if (const QImage* kept = m_images.object(page); kept != nullptr && kept->size() == pixels) {
        return *kept;
    }
    QImage image = m_document->render(page, pixels, renderOptions());
    if (image.isNull()) {
        return image;
    }
    applyLook(image);
    image.setDevicePixelRatio(ratio);
    m_images.insert(page, new QImage(image),
                    static_cast<int>(std::max<qint64>(1, image.sizeInBytes() / 1024)));
    return image;
}

void PdfPagesView::applyLook(QImage& image) const {
    if (m_look == Look::AsFile || image.isNull()) {
        return;
    }
    if (image.format() != QImage::Format_ARGB32_Premultiplied) {
        image.convertTo(QImage::Format_ARGB32_Premultiplied);
    }
    if (m_look == Look::Inverted) {
        image.invertPixels();
    }
    QPainter painter(&image);
    if (m_look == Look::Dimmed) {
        // The white of the paper made gray, the black of the text stays
        painter.setCompositionMode(QPainter::CompositionMode_Multiply);
        painter.fillRect(image.rect(), QColor::fromHslF(0.0F, 0.0F, DIMMED_LIGHTNESS));
    } else {
        // The black of the paper made dark gray, the white of the text stays
        painter.setCompositionMode(QPainter::CompositionMode_Screen);
        painter.fillRect(image.rect(), QColor::fromHslF(0.0F, 0.0F, INVERTED_LIGHTNESS));
    }
}

void PdfPagesView::followScroll() {
    if (m_scrolling || m_layout != Layout::Continuous || m_rects.isEmpty()) {
        return;
    }
    // The last page that begins above the middle of the window
    const int middle = verticalScrollBar()->value() + viewport()->height() / 2;
    const auto after = std::upper_bound(
        m_rects.cbegin(), m_rects.cend(), middle,
        [](int value, const QRect& rect) { return value < rect.top(); });
    const int page = std::max(0, static_cast<int>(after - m_rects.cbegin()) - 1);
    setPage(page);
}

void PdfPagesView::setPage(int page) {
    if (page != m_page) {
        m_page = page;
        emit currentPageChanged(page);
    }
}

void PdfPagesView::moveRows(int count) {
    const QList<QList<int>> all = rows();
    if (all.isEmpty()) {
        return;
    }
    const int row = std::clamp(rowOf(m_page) + count, 0, static_cast<int>(all.size()) - 1);
    setCurrentPage(all.at(row).first());
}

}  // namespace kalahari::gui
