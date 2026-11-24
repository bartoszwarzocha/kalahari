// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plan_chronology_grid.cpp
/// @brief The cards of the chronology of the Book Plan: where each is, its painting, the keys
/// and the mouse that choose it

#include <kalahari/timeline/plan_chronology_view.h>

#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>

#include <algorithm>
#include <cmath>
#include <limits>

namespace kalahari::timeline {

namespace {

constexpr double STRIP = 4.0;          ///< The color of a card at its top
constexpr double RADIUS = 4.0;
constexpr double RING = 3.0;           ///< The ring of a flashback, outside the card
constexpr double LINE_SPACING = 2.0;
constexpr double SCROLL_STEP = 24.0;
constexpr int FOOT_SMALLER = 1;        ///< The foot of a card: a point smaller

}  // namespace

PlanChronologyGrid::PlanChronologyGrid(QWidget* parent) : QAbstractScrollArea(parent) {
    setObjectName(QStringLiteral("planChronologyGrid"));
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setAutoFillBackground(false);
    horizontalScrollBar()->setSingleStep(static_cast<int>(SCROLL_STEP));
    verticalScrollBar()->setSingleStep(static_cast<int>(SCROLL_STEP));
    //: The cards of the chronology of the Book Plan, for the screen reader. In Polish:
    //: Chronologia
    setAccessibleName(tr("Chronology"));
}

void PlanChronologyGrid::setChronology(std::shared_ptr<const ChronologyLayout> layout,
                                       const TimelineLook& look, const QString& noLaneName,
                                       const QString& oneLaneName) {
    m_layout = std::move(layout);
    m_look = look;
    m_noLaneName = noLaneName;
    m_oneLaneName = oneLaneName;
    if (m_current >= static_cast<int>(m_layout->items().size())) {
        m_current = -1;
    }
    layOut();
    viewport()->update();
}

void PlanChronologyGrid::setCurrent(int item) {
    if (m_layout == nullptr || item < -1 || item >= static_cast<int>(m_layout->items().size())) {
        item = -1;
    }
    if (item == m_current) {
        return;
    }
    m_current = item;
    if (item >= 0) {
        // Scroll so that the card shows
        const QRectF rect = rectOf(item);
        const QPointF at = offset();
        const double left = rect.left() - HEAD_WIDTH;
        const double right = rect.right() + PADDING - viewport()->width();
        if (left < at.x()) {
            horizontalScrollBar()->setValue(static_cast<int>(std::max(0.0, left)));
        } else if (right > at.x()) {
            horizontalScrollBar()->setValue(static_cast<int>(std::ceil(right)));
        }
        const double top = rect.top() - m_headerHeight;
        const double bottom = rect.bottom() + PADDING - viewport()->height();
        if (top < at.y()) {
            verticalScrollBar()->setValue(static_cast<int>(std::max(0.0, top)));
        } else if (bottom > at.y()) {
            verticalScrollBar()->setValue(static_cast<int>(std::ceil(bottom)));
        }
    }
    viewport()->update();
    emit currentChanged();
}

QRectF PlanChronologyGrid::rectOf(int item) const {
    return item >= 0 && item < m_rects.size() ? m_rects.at(item) : QRectF();
}

int PlanChronologyGrid::itemAt(const QPointF& point) const {
    const QPointF at = point + offset();
    for (int i = 0; i < m_rects.size(); ++i) {
        if (m_rects.at(i).contains(at)) {
            return i;
        }
    }
    return -1;
}

QPointF PlanChronologyGrid::offset() const {
    return {static_cast<double>(horizontalScrollBar()->value()),
            static_cast<double>(verticalScrollBar()->value())};
}

// =============================================================================
// Where each card is
// =============================================================================

void PlanChronologyGrid::layOut() {
    m_columnX.clear();
    m_columnWidth.clear();
    m_laneY.clear();
    m_laneHeight.clear();
    m_rects.clear();
    if (m_layout == nullptr) {
        return;
    }
    const QFontMetricsF metrics(font());
    QFont small = font();
    small.setPointSizeF(std::max(1.0, small.pointSizeF() - FOOT_SMALLER));
    const QFontMetricsF smallMetrics(small);
    m_headerHeight = metrics.height() + 2 * PADDING;
    // The title, the time and the place, the foot
    m_cardHeight = STRIP + PADDING / 2 + 2 * (metrics.height() + LINE_SPACING) +
                   smallMetrics.height() + PADDING;
    m_rects.fill(QRectF(), m_layout->items().size());

    double x = HEAD_WIDTH;
    for (const ChronologyColumn& column : m_layout->columns()) {
        m_columnX.append(x);
        const double width = column.gap ? GAP_WIDTH : CARD_WIDTH + 2 * PADDING;
        m_columnWidth.append(width);
        x += width;
    }
    m_width = x + PADDING;

    double y = m_headerHeight;
    const auto lanes = static_cast<int>(m_layout->lanes().size());
    const auto columns = static_cast<int>(m_layout->columns().size());
    for (int lane = 0; lane < lanes; ++lane) {
        m_laneY.append(y);
        int most = 1;
        for (int column = 0; column < columns; ++column) {
            const QList<int> cards = m_layout->cell(lane, column);
            most = std::max(most, static_cast<int>(cards.size()));
            for (int i = 0; i < cards.size(); ++i) {
                m_rects[cards.at(i)] =
                    QRectF(m_columnX.at(column) + PADDING,
                           y + PADDING + i * (m_cardHeight + CARD_SPACING), CARD_WIDTH,
                           m_cardHeight);
            }
        }
        const double height = 2 * PADDING + most * m_cardHeight + (most - 1) * CARD_SPACING;
        m_laneHeight.append(height);
        y += height;
    }

    // The cards without a time, in rows as wide as the window or the columns
    const QList<int>& untimed = m_layout->untimed();
    if (!untimed.isEmpty()) {
        y += PADDING;
        m_untimedTitleRect = QRectF(PADDING, y, std::max(m_width, 1.0), metrics.height());
        y += metrics.height() + PADDING / 2;
        const double right = std::max(m_width, static_cast<double>(viewport()->width()));
        double left = PADDING;
        for (int item : untimed) {
            if (left > PADDING && left + CARD_WIDTH > right - PADDING) {
                left = PADDING;
                y += m_cardHeight + CARD_SPACING;
            }
            m_rects[item] = QRectF(left, y, CARD_WIDTH, m_cardHeight);
            left += CARD_WIDTH + CARD_SPACING;
        }
        y += m_cardHeight;
        m_width = std::max(m_width, left);
    } else {
        m_untimedTitleRect = QRectF();
    }
    m_height = y + PADDING;
    updateScrollBars();
}

void PlanChronologyGrid::updateScrollBars() {
    const QSize area = viewport()->size();
    horizontalScrollBar()->setPageStep(area.width());
    verticalScrollBar()->setPageStep(area.height());
    horizontalScrollBar()->setRange(0, std::max(0, static_cast<int>(std::ceil(m_width)) - area.width()));
    verticalScrollBar()->setRange(0,
                                  std::max(0, static_cast<int>(std::ceil(m_height)) - area.height()));
}

void PlanChronologyGrid::resizeEvent(QResizeEvent* event) {
    QAbstractScrollArea::resizeEvent(event);
    layOut();
}

// =============================================================================
// Painting
// =============================================================================

void PlanChronologyGrid::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(viewport()->rect(), m_look.window);
    if (m_layout == nullptr) {
        return;
    }
    const QPointF at = offset();
    painter.save();
    painter.translate(-at);
    // The bands, every other one in its own color, and the gaps
    for (int lane = 0; lane < m_laneY.size(); ++lane) {
        const QRectF band(HEAD_WIDTH, m_laneY.at(lane), m_width - HEAD_WIDTH,
                          m_laneHeight.at(lane));
        painter.fillRect(band, lane % 2 == 0 ? m_look.lane : m_look.laneAlternate);
    }
    const QList<ChronologyColumn>& columns = m_layout->columns();
    const double lanesBottom =
        m_laneY.isEmpty() ? m_headerHeight : m_laneY.constLast() + m_laneHeight.constLast();
    for (int column = 0; column < columns.size(); ++column) {
        const QRectF strip(m_columnX.at(column), m_headerHeight, m_columnWidth.at(column),
                           lanesBottom - m_headerHeight);
        if (columns.at(column).gap) {
            painter.fillRect(strip, m_look.gap);
        } else {
            painter.setPen(QPen(m_look.tick, 1.0));
            painter.drawLine(strip.topRight(), strip.bottomRight());
        }
    }
    // The cards
    if (!m_untimedTitleRect.isNull()) {
        QFont bold = font();
        bold.setBold(true);
        painter.setFont(bold);
        painter.setPen(m_look.mutedText);
        painter.drawText(m_untimedTitleRect, Qt::AlignLeft | Qt::AlignVCenter, m_untimedTitle);
        painter.setFont(font());
    }
    for (int item = 0; item < m_rects.size(); ++item) {
        if (!m_rects.at(item).isNull()) {
            paintCard(painter, item, m_rects.at(item));
        }
    }
    painter.restore();
    paintHeads(painter);
}

void PlanChronologyGrid::paintHeads(QPainter& painter) const {
    const QPointF at = offset();
    const QList<ChronologyColumn>& columns = m_layout->columns();
    const QFontMetricsF metrics(font());
    // The times over the columns stay at the top
    painter.fillRect(QRectF(0, 0, viewport()->width(), m_headerHeight), m_look.window);
    QFont bold = font();
    bold.setBold(true);
    for (int column = 0; column < columns.size(); ++column) {
        const QRectF head(m_columnX.at(column) - at.x(), 0, m_columnWidth.at(column),
                          m_headerHeight);
        const ChronologyColumn& shown = columns.at(column);
        if (shown.gap) {
            QFont small = font();
            small.setPointSizeF(std::max(1.0, small.pointSizeF() - FOOT_SMALLER));
            painter.setFont(small);
            painter.setPen(m_look.mutedText);
            painter.drawText(head, Qt::AlignCenter | Qt::TextWordWrap, shown.label);
            continue;
        }
        painter.setFont(bold);
        painter.setPen(m_look.text);
        painter.drawText(head.adjusted(PADDING, 0, -PADDING, 0), Qt::AlignCenter,
                         metrics.elidedText(shown.label, Qt::ElideRight,
                                            head.width() - 2 * PADDING));
    }
    painter.setPen(QPen(m_look.mid, 1.0));
    painter.drawLine(QPointF(0, m_headerHeight), QPointF(viewport()->width(), m_headerHeight));
    // The names of the bands stay at the left
    painter.setFont(font());
    const QList<ChronologyLane>& lanes = m_layout->lanes();
    painter.save();
    painter.setClipRect(QRectF(0, m_headerHeight, HEAD_WIDTH, viewport()->height()));
    for (int lane = 0; lane < lanes.size(); ++lane) {
        const QRectF head(0, m_laneY.at(lane) - at.y(), HEAD_WIDTH, m_laneHeight.at(lane));
        painter.fillRect(head, m_look.window);
        const ChronologyLane& shown = lanes.at(lane);
        QString name = shown.name;
        if (shown.key == QLatin1String("*")) {
            name = m_oneLaneName;
        } else if (shown.key.isEmpty()) {
            name = m_noLaneName;
        }
        painter.setPen(shown.key.isEmpty() ? m_look.mutedText : m_look.text);
        painter.drawText(head.adjusted(PADDING, PADDING, -PADDING, -PADDING),
                         Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, name);
    }
    painter.restore();
    painter.fillRect(QRectF(0, 0, HEAD_WIDTH, m_headerHeight), m_look.window);
    painter.setPen(QPen(m_look.mid, 1.0));
    painter.drawLine(QPointF(HEAD_WIDTH, 0), QPointF(HEAD_WIDTH, m_headerHeight));
}

void PlanChronologyGrid::paintCard(QPainter& painter, int item, const QRectF& rect) const {
    const ChronologyItem& card = m_layout->items().at(item);
    const bool selected = item == m_current;
    if (card.flashback) {
        painter.setPen(QPen(m_look.flashback, 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect.adjusted(-RING, -RING, RING, RING), RADIUS + RING,
                                RADIUS + RING);
    }
    QPainterPath shape;
    shape.addRoundedRect(rect, RADIUS, RADIUS);
    painter.fillPath(shape, m_look.card);
    if (card.color.isValid()) {
        painter.save();
        painter.setClipPath(shape);
        painter.fillRect(QRectF(rect.left(), rect.top(), rect.width(), STRIP), card.color);
        painter.restore();
    }
    // A card in the plan and an event outside the text: a dashed frame
    QPen frame(selected ? m_look.selection : m_look.mid, selected ? 2.0 : 1.0);
    if (card.planned || card.outside) {
        frame.setStyle(Qt::DashLine);
    }
    painter.setPen(frame);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);

    const QRectF inside = rect.adjusted(PADDING, STRIP + PADDING / 2, -PADDING, -PADDING / 2);
    const QFontMetricsF metrics(font());
    QFont bold = font();
    bold.setBold(true);
    const QFontMetricsF boldMetrics(bold);
    double y = inside.top();
    // The number in the order of reading, then the title
    double x = inside.left();
    if (!card.number.isEmpty()) {
        painter.setFont(bold);
        painter.setPen(m_look.mutedText);
        const QString number = card.number + QLatin1Char(' ');
        painter.drawText(QPointF(x, y + boldMetrics.ascent()), number);
        x += boldMetrics.horizontalAdvance(number);
    }
    painter.setFont(bold);
    painter.setPen(m_look.text);
    painter.drawText(QPointF(x, y + boldMetrics.ascent()),
                     boldMetrics.elidedText(card.title, Qt::ElideRight, inside.right() - x));
    y += metrics.height() + LINE_SPACING;
    // When and where
    painter.setFont(font());
    painter.setPen(m_look.mutedText);
    QStringList when;
    when << (card.timeText.isEmpty() ? tr("no time") : card.timeText);
    if (!card.place.isEmpty()) {
        when << card.place;
    }
    painter.drawText(QPointF(inside.left(), y + metrics.ascent()),
                     metrics.elidedText(when.join(QStringLiteral(" · ")), Qt::ElideRight,
                                        inside.width()));
    y += metrics.height() + LINE_SPACING;
    // What kind of card it is
    QString foot;
    QColor footColor = m_look.mutedText;
    if (card.outside) {
        //: The foot of an event of the chronology that the book does not tell. In Polish: Poza
        //: tekstem
        foot = tr("Outside the text");
    } else if (card.flashback) {
        //: The foot of a card told later than it happens. In Polish: Retrospekcja
        foot = tr("Flashback");
        footColor = m_look.flashback;
    } else if (card.planned) {
        //: The foot of a card in the plan. In Polish: W planie
        foot = tr("In the plan");
    } else if (!card.time && !card.timeText.isEmpty()) {
        //: The foot of a card whose time the program does not read. In Polish: Czas
        //: nierozpoznany
        foot = tr("Time not recognized");
        footColor = m_look.flashback;
    }
    if (!foot.isEmpty()) {
        QFont small = font();
        small.setPointSizeF(std::max(1.0, small.pointSizeF() - FOOT_SMALLER));
        const QFontMetricsF smallMetrics(small);
        painter.setFont(small);
        painter.setPen(footColor);
        painter.drawText(QPointF(inside.left(), y + smallMetrics.ascent()),
                         smallMetrics.elidedText(foot, Qt::ElideRight, inside.width()));
    }
}

// =============================================================================
// The keys and the mouse
// =============================================================================

QList<QList<int>> PlanChronologyGrid::rows() const {
    QList<QList<int>> rows;
    if (m_layout == nullptr) {
        return rows;
    }
    const auto columns = static_cast<int>(m_layout->columns().size());
    for (int lane = 0; lane < m_layout->lanes().size(); ++lane) {
        QList<int> row;
        for (int column = 0; column < columns; ++column) {
            row << m_layout->cell(lane, column);
        }
        rows.append(row);
    }
    if (!m_layout->untimed().isEmpty()) {
        rows.append(m_layout->untimed());
    }
    return rows;
}

QList<int> PlanChronologyGrid::rowOf(int item) const {
    for (const QList<int>& row : rows()) {
        if (row.contains(item)) {
            return row;
        }
    }
    return {};
}

void PlanChronologyGrid::moveInRow(int step, bool toEnd) {
    const QList<QList<int>> all = rows();
    if (all.isEmpty()) {
        return;
    }
    if (m_current < 0) {
        setCurrent(all.constFirst().constFirst());
        return;
    }
    const QList<int> row = rowOf(m_current);
    if (toEnd) {
        setCurrent(step < 0 ? row.constFirst() : row.constLast());
        return;
    }
    const auto at = row.indexOf(m_current);
    const auto next = std::clamp<qsizetype>(at + step, 0, row.size() - 1);
    setCurrent(row.at(next));
}

void PlanChronologyGrid::moveToRow(int step) {
    const QList<QList<int>> all = rows();
    if (all.isEmpty()) {
        return;
    }
    if (m_current < 0) {
        setCurrent(all.constFirst().constFirst());
        return;
    }
    // In a column of cards first, then to the nearest card of the band above or below
    const QRectF here = rectOf(m_current);
    if (const int lane = m_layout->laneOf(m_current); lane >= 0) {
        const QList<int> cards = m_layout->cell(lane, m_layout->columnOf(m_current));
        const auto at = cards.indexOf(m_current);
        if (at + step >= 0 && at + step < cards.size()) {
            setCurrent(cards.at(at + step));
            return;
        }
    }
    qsizetype rowIndex = -1;
    for (qsizetype i = 0; i < all.size(); ++i) {
        if (all.at(i).contains(m_current)) {
            rowIndex = i;
        }
    }
    const qsizetype target = rowIndex + step;
    if (rowIndex < 0 || target < 0 || target >= all.size()) {
        return;
    }
    int nearest = -1;
    double best = std::numeric_limits<double>::max();
    for (int item : all.at(target)) {
        const double distance = std::abs(rectOf(item).center().x() - here.center().x()) +
                                std::abs(rectOf(item).center().y() - here.center().y()) / 4;
        if (distance < best) {
            best = distance;
            nearest = item;
        }
    }
    if (nearest >= 0) {
        setCurrent(nearest);
    }
}

void PlanChronologyGrid::keyPressEvent(QKeyEvent* event) {
    const Qt::KeyboardModifiers modifiers =
        event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);
    if (modifiers != Qt::NoModifier) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Left:
        moveInRow(-1, false);
        break;
    case Qt::Key_Right:
        moveInRow(1, false);
        break;
    case Qt::Key_Up:
        moveToRow(-1);
        break;
    case Qt::Key_Down:
        moveToRow(1);
        break;
    case Qt::Key_Home:
        moveInRow(-1, true);
        break;
    case Qt::Key_End:
        moveInRow(1, true);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_current >= 0) {
            emit activated(m_current);
        }
        break;
    default:
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    event->accept();
}

void PlanChronologyGrid::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        const int item = itemAt(event->position());
        if (item >= 0) {
            setCurrent(item);
        }
    }
    QAbstractScrollArea::mousePressEvent(event);
}

void PlanChronologyGrid::mouseDoubleClickEvent(QMouseEvent* event) {
    const int item = itemAt(event->position());
    if (event->button() == Qt::LeftButton && item >= 0) {
        setCurrent(item);
        emit activated(item);
        return;
    }
    QAbstractScrollArea::mouseDoubleClickEvent(event);
}

void PlanChronologyGrid::contextMenuEvent(QContextMenuEvent* event) {
    if (event->reason() == QContextMenuEvent::Mouse) {
        const int item = itemAt(QPointF(event->pos()));
        if (item >= 0) {
            setCurrent(item);
        }
        emit menuRequested(item, event->globalPos());
        return;
    }
    // Shift+F10 and the key of the menu: under the card chosen
    const QRectF rect = rectOf(m_current);
    const QPoint below = rect.isNull()
                             ? viewport()->rect().center()
                             : (rect.bottomLeft() - offset()).toPoint() + QPoint(0, 2);
    emit menuRequested(m_current, viewport()->mapToGlobal(below));
}

}  // namespace kalahari::timeline
