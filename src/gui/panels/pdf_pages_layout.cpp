// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pdf_pages_layout.cpp
/// @brief How PdfPagesView lays out the pages: the rows, the zoom, the page shown and the
/// places of the pages in the view

#include "kalahari/gui/panels/pdf_pages_view.h"

#include <QScrollBar>

#include <algorithm>
#include <cmath>
#include <functional>
#include <ranges>
#include <utility>

namespace kalahari::gui {

namespace {

/// Room around the pages, in pixels
constexpr int MARGIN = 12;

/// Room between the pages, in pixels
constexpr int GAP = 12;

/// Two zooms closer than this are the same
constexpr double SAME_ZOOM = 0.001;

/// Points in an inch, the unit of a PDF page
constexpr double POINTS_PER_INCH = 72.0;

}  // namespace

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
    const auto next =
        std::ranges::find_if(steps, [this](double step) { return step > m_zoom + SAME_ZOOM; });
    setZoom(next != steps.cend() ? *next : steps.last());
}

void PdfPagesView::zoomOut() {
    const QList<double>& steps = zoomSteps();
    const auto down = std::views::reverse(steps);
    const auto next =
        std::ranges::find_if(down, [this](double step) { return step < m_zoom - SAME_ZOOM; });
    setZoom(next != down.end() ? *next : steps.first());
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

void PdfPagesView::relayout() {
    const double before = m_zoom;
    const double down = shownDown();
    const QList<QList<int>> shown = shownRows();
    if (m_fit != Fit::None) {
        m_zoom = fittedZoom(shown);
    }
    placePages(shown);

    m_scrolling = true;
    const QSize room = viewport()->size();
    QScrollBar* vertical = verticalScrollBar();
    vertical->setRange(0, std::max(0, m_size.height() - room.height()));
    vertical->setPageStep(room.height());
    QScrollBar* horizontal = horizontalScrollBar();
    horizontal->setRange(0, std::max(0, m_size.width() - room.width()));
    horizontal->setPageStep(room.width());
    // The place shown stays in the window
    if (m_layout == Layout::Continuous && m_page < m_rects.size() &&
        !m_rects.at(m_page).isEmpty()) {
        const QRect rect = m_rects.at(m_page);
        vertical->setValue(rect.top() + static_cast<int>(std::lround(down * rect.height())));
    }
    m_scrolling = false;
    viewport()->update();
    if (std::abs(before - m_zoom) > SAME_ZOOM) {
        emit zoomChanged(m_zoom);
    }
}

double PdfPagesView::shownDown() const {
    if (m_layout != Layout::Continuous || m_page >= m_rects.size() ||
        m_rects.at(m_page).isEmpty()) {
        return 0.0;
    }
    const QRect rect = m_rects.at(m_page);
    return static_cast<double>(verticalScrollBar()->value() - rect.top()) / rect.height();
}

QList<QList<int>> PdfPagesView::shownRows() const {
    QList<QList<int>> all = rows();
    if (m_layout == Layout::Continuous || all.isEmpty()) {
        return all;
    }
    return {all.at(std::clamp(rowOf(m_page), 0, static_cast<int>(all.size()) - 1))};
}

void PdfPagesView::placePages(const QList<QList<int>>& shownRows) {
    const double scale = pixelsAtFullSize() * m_zoom;
    const auto pixels = [this, scale](int page) {
        const QSizeF points = m_pointSizes.at(page);
        return QSize(std::max(1, static_cast<int>(std::lround(points.width() * scale))),
                     std::max(1, static_cast<int>(std::lround(points.height() * scale))));
    };
    m_rects = QVector<QRect>(pageCount());
    // Each row in the middle of the widest one
    QVector<int> widths;
    int widest = 0;
    for (const QList<int>& row : shownRows) {
        int width = GAP * (static_cast<int>(row.size()) - 1);
        for (const int page : row) {
            width += pixels(page).width();
        }
        widths.append(width);
        widest = std::max(widest, width);
    }
    // The pages of a row side by side, each in the middle of the height of the row
    int top = MARGIN;
    for (int index = 0; index < shownRows.size(); ++index) {
        const QList<int>& row = shownRows.at(index);
        int height = 0;
        for (const int page : row) {
            height = std::max(height, pixels(page).height());
        }
        int left = MARGIN + (widest - widths.at(index)) / 2;
        for (const int page : row) {
            const QSize size = pixels(page);
            m_rects[page] = QRect(QPoint(left, top + (height - size.height()) / 2), size);
            left += size.width() + GAP;
        }
        top += height + GAP;
    }
    m_size = shownRows.isEmpty() ? QSize() : QSize(widest + 2 * MARGIN, top - GAP + MARGIN);
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
        for (const int page : row) {
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
        for (const int page : *row) {
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

void PdfPagesView::followScroll() {
    if (m_scrolling || m_layout != Layout::Continuous || m_rects.isEmpty()) {
        return;
    }
    // The last page that begins above the middle of the window
    const int middle = verticalScrollBar()->value() + viewport()->height() / 2;
    const auto after =
        std::ranges::upper_bound(std::as_const(m_rects), middle, std::less<>(), &QRect::top);
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
