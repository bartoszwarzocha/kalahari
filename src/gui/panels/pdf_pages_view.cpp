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
#include <QPolygonF>
#include <QScrollBar>
#include <QTransform>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>

namespace kalahari::gui {

namespace {

/// How far the shade of a page falls, in pixels
constexpr int SHADE = 2;

/// The pages drawn that are kept, in kilobytes: about 192 MB
constexpr int KEPT_KILOBYTES = 192 * 1024;

/// A page drawn larger than this many bytes is not kept: only the part shown is drawn
constexpr qint64 LARGEST_KEPT = 24LL * 1024 * 1024;

/// Bytes of a pixel of a page drawn
constexpr qint64 PIXEL_BYTES = 4;

/// Bytes in a kilobyte, the unit of the pages kept
constexpr qint64 KILOBYTE = 1024;

/// How much of the pages shows through the colors of the selection and of the hits at first
constexpr int SELECTION_ALPHA = 80;

/// How much of the light text of inverted pages shows through a highlight (0-255)
constexpr int INVERTED_HIGHLIGHT_ALPHA = 110;

/// Dimmed pages: their white is this light
constexpr float DIMMED_LIGHTNESS = 0.62F;

/// Pages with their colors turned: their black is this light
constexpr float INVERTED_LIGHTNESS = 0.12F;

/// Pixels the arrows and the wheel move the pages by
constexpr int SCROLL_STEP = 40;

/// A page without a size in its file is taken as A4, in points
constexpr double A4_WIDTH = 595.0;
constexpr double A4_HEIGHT = 842.0;

/// Positions closer than this, in points, select nothing
constexpr double SAME_POSITION = 0.5;

/// @brief textBetween(): the steps in which the places beside the text go toward each other
constexpr int APPROACH_STEPS = 8;

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
    m_colors.selection.setAlpha(SELECTION_ALPHA);
    m_colors.searchMatch = m_colors.selection;
    m_colors.currentSearchMatch = m_colors.selection;
}

void PdfPagesView::setDocument(PdfDocument* document) {
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

void PdfPagesView::setSearchModel(PdfSearchModel* model) {
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

void PdfPagesView::showPlace(int page, const QRectF& points) {
    setCurrentPage(page);
    const QRect place = toViewport(page, points).toAlignedRect();
    if (place.isEmpty() || viewport()->rect().contains(place)) {
        return;
    }
    // The place in the middle of the window, as far as the pages go
    m_scrolling = true;
    QScrollBar* vertical = verticalScrollBar();
    vertical->setValue(vertical->value() + place.center().y() - (viewport()->height() / 2));
    if (!viewport()->rect().contains(place.left(), place.top()) ||
        !viewport()->rect().contains(place.right(), place.top())) {
        QScrollBar* horizontal = horizontalScrollBar();
        horizontal->setValue(horizontal->value() + place.center().x() - (viewport()->width() / 2));
    }
    m_scrolling = false;
    viewport()->update();
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
    std::optional<PdfSelection> selection = textBetween(page, start, end);
    if (!selection) {
        clearSelection();
        return;
    }
    takeSelection(page, std::move(*selection));
}

void PdfPagesView::takeSelection(int page, PdfSelection selection) {
    m_selection = std::move(selection);
    m_selectionPage = page;
    viewport()->update();
    emit selectionChanged();
}

std::optional<PdfSelection> PdfPagesView::textBetween(int page, const QPointF& start,
                                                      const QPointF& end) {
    const auto found = [](const PdfSelection& selection) {
        return selection.isValid() && !selection.text().isEmpty();
    };
    PdfSelection selection = m_document->getSelection(page, start, end);
    if (found(selection)) {
        return selection;
    }
    // PdfDocument finds a letter only near the place: from the margin, or between two
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
    const PdfSelection all = m_document->getAllText(page);
    const QRectF bounds = all.isValid() ? all.boundingRectangle() : QRectF();
    m_textBounds.insert(page, bounds);
    return bounds;
}

void PdfPagesView::selectHit(const PdfHit& hit) {
    if (m_document == nullptr || hit.page < 0 || hit.page >= pageCount()) {
        return;
    }
    PdfSelection selection =
        m_document->getSelectionAtIndex(hit.page, hit.startIndex, hit.length);
    if (!selection.isValid() || selection.text().isEmpty()) {
        clearSelection();
        return;
    }
    takeSelection(hit.page, std::move(selection));
}

void PdfPagesView::selectPage(int page) {
    if (m_document == nullptr || page < 0 || page >= pageCount()) {
        return;
    }
    PdfSelection selection = m_document->getAllText(page);
    if (!selection.isValid() || selection.text().isEmpty()) {
        clearSelection();
        return;
    }
    takeSelection(page, std::move(selection));
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

QList<QRectF> PdfPagesView::selectionAreas() const {
    QList<QRectF> areas;
    if (!m_selection || m_selection->text().isEmpty()) {
        return areas;
    }
    // One area a line: the parts of a line, word by word, are one band
    for (const QPolygonF& polygon : m_selection->bounds()) {
        const QRectF part = polygon.boundingRect();
        if (part.isEmpty()) {
            continue;
        }
        // Of one line: at least half of the lower one beside the other, as a full stop
        const auto line = std::ranges::find_if(areas, [&part](const QRectF& area) {
            const double beside = std::min(area.bottom(), part.bottom()) -
                                  std::max(area.top(), part.top());
            return beside >= std::min(area.height(), part.height()) / 2;
        });
        if (line != areas.end()) {
            *line = line->united(part);
        } else {
            areas << part;
        }
    }
    return areas;
}

void PdfPagesView::setHighlights(const QList<Highlight>& highlights) {
    m_highlights = highlights;
    viewport()->update();
}

int PdfPagesView::highlightAt(const QPoint& position) const {
    const int page = pageAt(position);
    if (page < 0) {
        return -1;
    }
    const QPointF point = toPoints(page, position);
    // The one drawn last, over the others
    for (qsizetype index = m_highlights.size() - 1; index >= 0; --index) {
        const Highlight& highlight = m_highlights.at(index);
        if (highlight.page == page &&
            std::ranges::any_of(highlight.areas,
                                [&point](const QRectF& area) { return area.contains(point); })) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

void PdfPagesView::setCurrentHit(int page, const QList<QRectF>& rectangles) {
    m_hitPage = page;
    m_hitRectangles = rectangles;
    viewport()->update();
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
        paintMarks(painter, page, rect);
    }
}

void PdfPagesView::paintMarks(QPainter& painter, int page, const QRect& rect) const {
    // A highlight colors the paper and leaves the text dark, as a highlighter; on the dark
    // paper of inverted pages it lets the light text through
    painter.save();
    if (m_look != Look::Inverted) {
        painter.setCompositionMode(QPainter::CompositionMode_Multiply);
    }
    for (const Highlight& highlight : m_highlights) {
        if (highlight.page != page) {
            continue;
        }
        QColor color = highlight.color;
        if (m_look == Look::Inverted) {
            color.setAlpha(INVERTED_HIGHLIGHT_ALPHA);
        }
        for (const QRectF& area : highlight.areas) {
            painter.fillRect(toViewport(page, area), color);
        }
    }
    painter.restore();
    if (m_search != nullptr) {
        const QList<PdfHit> hits = m_search->resultsOnPage(page);
        for (const PdfHit& hit : hits) {
            for (const QRectF& rectangle : hit.rectangles) {
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
    if (m_dragPage >= 0 && event->buttons().testFlag(Qt::LeftButton)) {
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
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        if (delta > 0) {
            zoomIn();
        } else if (delta < 0) {
            zoomOut();
        }
        event->accept();
        return;
    }
    if (wheelToNextRow(delta)) {
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

bool PdfPagesView::wheelToNextRow(int delta) {
    if (m_layout == Layout::Continuous || delta == 0) {
        return false;
    }
    QScrollBar* bar = verticalScrollBar();
    const int row = rowOf(m_page);
    if (delta < 0 && bar->value() >= bar->maximum() && row + 1 < rows().size()) {
        moveRows(1);
        return true;
    }
    if (delta > 0 && bar->value() <= bar->minimum() && row > 0) {
        // The page before, from its end
        moveRows(-1);
        m_scrolling = true;
        bar->setValue(bar->maximum());
        m_scrolling = false;
        return true;
    }
    return false;
}

void PdfPagesView::keyPressEvent(QKeyEvent* event) {
    Qt::KeyboardModifiers modifiers = event->modifiers();
    modifiers.setFlag(Qt::KeypadModifier, false);
    if (handleKey(event->key(), modifiers)) {
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        // Esc takes the selection away; without one it goes to the panel
        if (modifiers == Qt::NoModifier && hasSelection()) {
            clearSelection();
        } else {
            event->ignore();
        }
        return;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

bool PdfPagesView::handleKey(int key, Qt::KeyboardModifiers modifiers) {
    if (modifiers == Qt::ShiftModifier && key == Qt::Key_F10) {
        emit selectionMenuRequested(selectionMenuPlace());
        return true;
    }
    // Ctrl+Home and Ctrl+End as Home and End
    if (modifiers == Qt::ControlModifier && (key == Qt::Key_Home || key == Qt::Key_End)) {
        return moveKey(key);
    }
    if (modifiers != Qt::NoModifier) {
        return false;
    }
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        emit activated();
        return true;
    }
    return moveKey(key);
}

bool PdfPagesView::moveKey(int key) {
    switch (key) {
    case Qt::Key_PageDown:
        moveRows(1);
        return true;
    case Qt::Key_PageUp:
        moveRows(-1);
        return true;
    case Qt::Key_Home:
        setCurrentPage(0);
        return true;
    case Qt::Key_End:
        setCurrentPage(pageCount() - 1);
        return true;
    default:
        return false;
    }
}

QPoint PdfPagesView::selectionMenuPlace() const {
    const QRect selection = selectionRect();
    return viewport()->mapToGlobal(selection.isEmpty() ? viewport()->rect().center()
                                                       : selection.bottomLeft());
}

void PdfPagesView::contextMenuEvent(QContextMenuEvent* event) {
    emit selectionMenuRequested(event->reason() == QContextMenuEvent::Mouse ? event->globalPos()
                                                                            : selectionMenuPlace());
    event->accept();
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
        QImage image = m_document->render(page, pixels, part);
        applyLook(image);
        image.setDevicePixelRatio(ratio);
        return image;
    }
    if (const QImage* kept = m_images.object(page); kept != nullptr && kept->size() == pixels) {
        return *kept;
    }
    QImage image = m_document->render(page, pixels);
    if (image.isNull()) {
        return image;
    }
    applyLook(image);
    image.setDevicePixelRatio(ratio);
    m_images.insert(page, new QImage(image),
                    static_cast<int>(std::max<qint64>(1, image.sizeInBytes() / KILOBYTE)));
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

}  // namespace kalahari::gui
