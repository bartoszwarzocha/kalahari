// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_axis_view_paint.cpp
/// @brief The axis of a timeline: its painting

#include <kalahari/timeline/timeline_axis_view.h>

#include <kalahari/timeline/timeline_document.h>
#include <kalahari/timeline/timeline_edits.h>

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>

#include <algorithm>
#include <cmath>

namespace kalahari::timeline {

namespace {

constexpr double CARD_RADIUS = 4.0;
constexpr double BAR_HEIGHT = 3.0;
constexpr double PADDING_X = 7.0;
constexpr double PADDING_TOP = 6.0;
constexpr double DOT = 8.0;
constexpr double DOT_LEFT = 6.0;
constexpr double LABEL_TEXT_LEFT = 18.0;
constexpr double DIM_OPACITY = 0.25;
constexpr double DRAGGED_OPACITY = 0.35;
constexpr double GHOST_OPACITY = 0.85;
constexpr double CHIP_GAP = 4.0;
constexpr double CHIP_PADDING = 4.0;
constexpr double CHIP_WIDEST = 90.0;
constexpr double GUIDE_Y = 14.0;
constexpr double STRIPE = 8.0;
constexpr double TICK_MAJOR = 7.0;
constexpr double TICK_MINOR = 4.0;
constexpr double LABEL_BOTTOM = 8.0;
constexpr double UPPER_BOTTOM = 23.0;
constexpr double HEAD_PADDING = 8.0;
constexpr double HEAD_DOT = 9.0;
constexpr double TRAY_TINT = 0.12;
constexpr double LINK_OPACITY = 0.6;
constexpr double LINK_LEFT = 10.0;
constexpr double DROP_WIDTH = 2.0;
constexpr double DROP_LABEL_PADDING = 6.0;
constexpr double SELECTION_WIDTH = 2.0;
constexpr double FLASH = 0.5;
constexpr double GAP_LABEL_TOP = 6.0;      ///< The length of a gap: below the top of the lanes
constexpr double GAP_LABEL_PADDING = 4.0;

QColor mixed(const QColor& a, const QColor& b, double share) {
    return QColor::fromRgbF(static_cast<float>(a.redF() * share + b.redF() * (1.0 - share)),
                            static_cast<float>(a.greenF() * share + b.greenF() * (1.0 - share)),
                            static_cast<float>(a.blueF() * share + b.blueF() * (1.0 - share)));
}

/// Diagonal stripes in @p rect, as a gap drawn short shows
void stripes(QPainter& painter, const QRectF& rect, const QColor& color, const QColor& edge) {
    painter.save();
    painter.setClipRect(rect, Qt::IntersectClip);
    painter.setPen(QPen(color, STRIPE / 2.0));
    const double first = rect.left() - rect.height();
    for (int i = 0; i * STRIPE < rect.width() + rect.height(); ++i) {
        const double x = first + i * STRIPE;
        painter.drawLine(QPointF(x, rect.bottom()), QPointF(x + rect.height(), rect.top()));
    }
    painter.restore();
    painter.setPen(QPen(edge, 1.0));
    painter.drawLine(rect.topLeft(), rect.bottomLeft());
    painter.drawLine(rect.topRight(), rect.bottomRight());
}

}  // namespace

void TimelineAxisView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(viewport()->rect(), m_look.lane);
    paintLanes(painter);
    paintHeads(painter);
    paintTray(painter);
    paintHeader(painter);
    paintDrag(painter);
}

// =============================================================================
// The axis above the lanes
// =============================================================================

void TimelineAxisView::paintHeader(QPainter& painter) const {
    const QRectF band(0.0, 0.0, viewport()->width(), TimelineScene::AXIS_HEIGHT);
    painter.fillRect(band, m_look.window);
    painter.setPen(m_look.mid);
    painter.drawLine(band.bottomLeft(), band.bottomRight());

    // The corner: what the axis measures
    painter.setFont(m_scene->smallFont());
    painter.setPen(m_look.mutedText);
    const TimelineView::Scale scale = m_document.timeline().view().scale;
    //: The corner of the axis of a timeline. In Polish: kolejność / dni opowieści / daty
    const QString corner = scale == TimelineView::Scale::Order  ? tr("order")
                           : scale == TimelineView::Scale::Days ? tr("days of the story")
                                                                : tr("dates");
    painter.drawText(QRectF(HEAD_PADDING, 0.0, TimelineScene::HEAD_WIDTH - 2 * HEAD_PADDING,
                            TimelineScene::AXIS_HEIGHT),
                     Qt::AlignLeft | Qt::AlignVCenter, corner);
    painter.drawLine(QPointF(TimelineScene::HEAD_WIDTH, 0.0),
                     QPointF(TimelineScene::HEAD_WIDTH, TimelineScene::AXIS_HEIGHT));

    painter.save();
    painter.setClipRect(QRectF(TimelineScene::HEAD_WIDTH, 0.0,
                               viewport()->width() - TimelineScene::HEAD_WIDTH,
                               TimelineScene::AXIS_HEIGHT));
    painter.translate(TimelineScene::HEAD_WIDTH - horizontalScrollBar()->value(), 0.0);
    const double bottom = TimelineScene::AXIS_HEIGHT;
    const QFontMetricsF metrics(m_scene->smallFont());
    if (m_scene->ordered()) {
        painter.setPen(m_look.mutedText);
        for (qsizetype i = 0; i < m_scene->onAxis().size(); ++i) {
            const QString label = QString::number(i + 1) + QLatin1Char('.');
            const double x = m_scene->xOfSlot(static_cast<int>(i)) + 12.0;
            const double width = metrics.horizontalAdvance(label);
            painter.drawText(QPointF(x - width / 2.0, bottom - LABEL_BOTTOM), label);
        }
    } else {
        for (const TimelineTick& tick : m_scene->ticks()) {
            switch (tick.kind) {
                case TimelineTick::Kind::Minor:
                    painter.setPen(mixed(m_look.mutedText, m_look.window, FLASH));
                    painter.drawLine(QPointF(tick.x, bottom - TICK_MINOR),
                                     QPointF(tick.x, bottom));
                    break;
                case TimelineTick::Kind::Major:
                    painter.setPen(m_look.mutedText);
                    painter.drawLine(QPointF(tick.x, bottom - TICK_MAJOR),
                                     QPointF(tick.x, bottom));
                    painter.setPen(m_look.text);
                    painter.drawText(QPointF(tick.x + 3.0, bottom - LABEL_BOTTOM), tick.label);
                    break;
                case TimelineTick::Kind::Middle: {
                    painter.setPen(m_look.text);
                    const double width = metrics.horizontalAdvance(tick.label);
                    painter.drawText(QPointF(tick.x - width / 2.0, bottom - LABEL_BOTTOM),
                                     tick.label);
                    break;
                }
                case TimelineTick::Kind::Upper: {
                    QFont bold = m_scene->smallFont();
                    bold.setWeight(QFont::DemiBold);
                    painter.setFont(bold);
                    painter.setPen(m_look.mutedText);
                    painter.drawText(QPointF(tick.x, bottom - UPPER_BOTTOM), tick.label);
                    painter.setFont(m_scene->smallFont());
                    break;
                }
            }
        }
        for (const TimelineGapMark& gap : m_scene->gaps()) {
            const QRectF rect(gap.x, 0.0, gap.width, bottom);
            painter.fillRect(rect, m_look.window);
            stripes(painter, rect, m_look.gap, m_look.tick);
        }
    }
    painter.restore();
}

// =============================================================================
// The tray of the events without a time
// =============================================================================

void TimelineAxisView::paintTray(QPainter& painter) const {
    if (m_scene->trayHeight() <= 0.0) {
        return;
    }
    const QRectF band(0.0, TimelineScene::AXIS_HEIGHT, viewport()->width(),
                      m_scene->trayHeight());
    painter.fillRect(band, mixed(m_look.palette.at(1), m_look.window, TRAY_TINT));
    painter.setPen(m_look.mid);
    painter.drawLine(band.bottomLeft(), band.bottomRight());
    painter.setFont(m_scene->smallFont());
    painter.setPen(m_look.mutedText);
    painter.drawText(m_scene->trayTextRect().translated(0.0, TimelineScene::AXIS_HEIGHT),
                     Qt::AlignLeft | Qt::AlignVCenter, m_scene->trayText());
    painter.save();
    painter.translate(0.0, TimelineScene::AXIS_HEIGHT);
    painter.setClipRect(QRectF(0.0, 0.0, band.width(), band.height()));
    for (const TimelineItem& item : m_scene->trayItems()) {
        paintItem(painter, item, true);
    }
    painter.restore();
}

// =============================================================================
// The lanes
// =============================================================================

void TimelineAxisView::paintLanes(QPainter& painter) const {
    const double top = m_scene->lanesTop();
    painter.save();
    painter.setClipRect(QRectF(TimelineScene::HEAD_WIDTH, top,
                               viewport()->width() - TimelineScene::HEAD_WIDTH,
                               viewport()->height() - top));
    painter.translate(TimelineScene::HEAD_WIDTH - horizontalScrollBar()->value(),
                      top - verticalScrollBar()->value());
    const double width = std::max(m_scene->width(), horizontalScrollBar()->value() +
                                                        static_cast<double>(viewport()->width()));
    const QList<TimelineLane>& lanes = m_scene->lanes();
    for (qsizetype i = 0; i < lanes.size(); ++i) {
        const TimelineLane& lane = lanes.at(i);
        const QRectF rect(0.0, lane.top, width, lane.height);
        painter.fillRect(rect, i % 2 == 0 ? m_look.lane : m_look.laneAlternate);
        painter.setPen(QPen(m_look.tick, 1.0, Qt::DashLine));
        painter.drawLine(QPointF(0.0, lane.top + GUIDE_Y), QPointF(width, lane.top + GUIDE_Y));
        painter.setPen(mixed(m_look.mid, m_look.lane, FLASH));
        painter.drawLine(rect.bottomLeft(), rect.bottomRight());
    }
    // The gaps drawn short, each with its length down its stripes, at the top of what shows
    const QFontMetricsF small(m_scene->smallFont());
    painter.setFont(m_scene->smallFont());
    for (const TimelineGapMark& gap : m_scene->gaps()) {
        stripes(painter, QRectF(gap.x, 0.0, gap.width, m_scene->lanesHeight()), m_look.gap,
                m_look.tick);
        const double length = small.horizontalAdvance(gap.length) + 2 * GAP_LABEL_PADDING;
        const double height = small.height();
        painter.save();
        painter.translate(gap.x + gap.width / 2.0,
                          verticalScrollBar()->value() + GAP_LABEL_TOP + length / 2.0);
        painter.rotate(-90.0);
        const QRectF label(-length / 2.0, -height / 2.0, length, height);
        painter.fillRect(label, m_look.window);
        painter.setPen(m_look.mutedText);
        painter.drawText(label, Qt::AlignCenter, gap.length);
        painter.restore();
    }

    // An event of several characters: a line through its lanes
    if (m_document.timeline().view().lanes == TimelineView::Lanes::Characters) {
        for (const TimelineEvent* event : m_scene->onAxis()) {
            const QList<const TimelineItem*> items = m_scene->itemsOf(event->id);
            if (items.size() < 2) {
                continue;
            }
            double from = items.constFirst()->rect.top();
            double to = from;
            for (const TimelineItem* item : items) {
                from = std::min(from, item->rect.top());
                to = std::max(to, item->rect.top());
            }
            QColor color = m_scene->colorOf(*event);
            color.setAlphaF(static_cast<float>(LINK_OPACITY));
            painter.setPen(QPen(color, 2.0));
            const double x = items.constFirst()->rect.left() + LINK_LEFT;
            painter.drawLine(QPointF(x, from + PADDING_TOP), QPointF(x, to + PADDING_TOP));
        }
    }

    for (const TimelineItem& item : m_scene->items()) {
        if (m_drag && m_drag->started && m_drag->id == item.id) {
            painter.setOpacity(DRAGGED_OPACITY);
        }
        paintItem(painter, item, false);
        painter.setOpacity(1.0);
    }

    // Where a dragged event goes
    if (m_drag && m_drag->started && m_drag->valid) {
        painter.setPen(QPen(m_look.drop, DROP_WIDTH));
        painter.drawLine(QPointF(m_drag->dropX, 0.0),
                         QPointF(m_drag->dropX, m_scene->lanesHeight()));
    }
    painter.restore();
}

void TimelineAxisView::paintHeads(QPainter& painter) const {
    const double top = m_scene->lanesTop();
    const QRectF band(0.0, top, TimelineScene::HEAD_WIDTH, viewport()->height() - top);
    painter.fillRect(band, m_look.window);
    painter.save();
    painter.setClipRect(band);
    painter.translate(0.0, top - verticalScrollBar()->value());
    const QFontMetricsF titles(m_scene->titleFont());
    const double nameLeft = HEAD_PADDING + HEAD_DOT + 6.0;
    const double nameWidth = TimelineScene::HEAD_WIDTH - nameLeft - HEAD_PADDING;
    for (const TimelineLane& lane : m_scene->lanes()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(lane.color);
        painter.drawEllipse(QRectF(HEAD_PADDING, lane.top + HEAD_PADDING + 2.0, HEAD_DOT,
                                   HEAD_DOT));
        painter.setFont(m_scene->titleFont());
        painter.setPen(m_look.text);
        painter.drawText(QRectF(nameLeft, lane.top + HEAD_PADDING - 2.0, nameWidth,
                                titles.height()),
                         Qt::AlignLeft | Qt::AlignTop,
                         titles.elidedText(lane.name, Qt::ElideRight, nameWidth));
        //: How many events a lane of a timeline has. In Polish: %n wydarzenie / wydarzenia /
        //: wydarzeń
        const QString count = tr("%n event(s)", nullptr, lane.count);
        // "thread · 3 events", or the number alone where both do not fit
        const QFontMetricsF smallMetrics(m_scene->smallFont());
        const QString both = lane.kind + QStringLiteral(" · ") + count;
        const QString under =
            lane.kind.isEmpty() || smallMetrics.horizontalAdvance(both) > nameWidth ? count
                                                                                     : both;
        painter.setFont(m_scene->smallFont());
        painter.setPen(m_look.mutedText);
        painter.drawText(QRectF(nameLeft, lane.top + HEAD_PADDING - 2.0 + titles.height(),
                                nameWidth, m_scene->smallHeight()),
                         Qt::AlignLeft | Qt::AlignTop,
                         smallMetrics.elidedText(under, Qt::ElideRight, nameWidth));
        painter.setPen(m_look.mid);
        painter.drawLine(QPointF(0.0, lane.top + lane.height),
                         QPointF(TimelineScene::HEAD_WIDTH, lane.top + lane.height));
    }
    painter.restore();
    painter.setPen(m_look.mid);
    painter.drawLine(band.topRight(), band.bottomRight());
}

// =============================================================================
// An event
// =============================================================================

void TimelineAxisView::paintItem(QPainter& painter, const TimelineItem& item, bool tray) const {
    const TimelineEvent* event = m_document.timeline().event(item.id);
    if (event == nullptr) {
        return;
    }
    const QRectF& rect = item.rect;
    const bool cards = m_scene->cards();
    const bool selected = m_document.selected() == item.id &&
                          (tray || m_document.selectedLane().isEmpty() ||
                           m_document.selectedLane() == item.lane ||
                           m_scene->itemOf(item.id, m_document.selectedLane()) == &item);
    const QColor color = m_scene->colorOf(*event);
    const double opacity = painter.opacity();
    if (!TimelineEdits::matches(*event, m_highlight)) {
        painter.setOpacity(opacity * DIM_OPACITY);
    }
    const double radius = cards ? CARD_RADIUS : rect.height() / 2.0;
    QPainterPath shape;
    shape.addRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    QPen frame(m_look.mid, 1.0);
    if (TimelineScene::rough(*event)) {
        frame.setStyle(Qt::DashLine);
    }
    painter.setPen(frame);
    painter.setBrush(m_look.card);
    painter.drawPath(shape);

    const QFontMetricsF small(m_scene->smallFont());
    if (cards) {
        painter.save();
        painter.setClipPath(shape);
        const double bar = item.span > 0.0 ? item.span : rect.width();
        painter.fillRect(QRectF(rect.left(), rect.top(), bar, BAR_HEIGHT), color);
        double y = rect.top() + PADDING_TOP;
        const double width = rect.width() - 2 * PADDING_X;
        painter.setFont(m_scene->titleFont());
        painter.setPen(m_look.text);
        painter.drawText(QRectF(rect.left() + PADDING_X, y, width, m_scene->titleHeight()),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetricsF(m_scene->titleFont())
                             .elidedText(event->title, Qt::ElideRight, width));
        y += m_scene->titleHeight() + 1.0;
        painter.setFont(m_scene->smallFont());
        painter.setPen(m_look.mutedText);
        if (m_look.showTime) {
            painter.drawText(QRectF(rect.left() + PADDING_X, y, width, m_scene->smallHeight()),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             small.elidedText(tray && !event->time
                                                  ? (event->timeText.isEmpty()
                                                         ? TimelineWords::noTime()
                                                         : event->timeText)
                                                  : m_scene->whenOf(*event),
                                              Qt::ElideRight, width));
            y += m_scene->smallHeight() + 1.0;
        }
        if (m_look.showCharacters || m_look.showPlace) {
            painter.drawText(QRectF(rect.left() + PADDING_X, y, width, m_scene->smallHeight()),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             small.elidedText(m_scene->detailsOf(*event), Qt::ElideRight, width));
            y += m_scene->smallHeight() + 1.0;
        }
        if (m_look.showChapters) {
            double x = rect.left() + PADDING_X;
            const double chipHeight = m_scene->smallHeight() + 2.0;
            const auto chip = [&](const QString& text, bool off) {
                const double room = rect.right() - PADDING_X - x;
                if (room < 2 * CHIP_PADDING + small.averageCharWidth()) {
                    return;
                }
                // A chip of a deleted chapter or of none is in italics, measured as it is drawn
                QFont font = m_scene->smallFont();
                font.setItalic(off);
                const QFontMetricsF chipMetrics(font);
                const double textWidth = std::min(
                    {chipMetrics.horizontalAdvance(text), CHIP_WIDEST, room - 2 * CHIP_PADDING});
                const QRectF box(x, y + 1.0, textWidth + 2 * CHIP_PADDING, chipHeight);
                QPen pen(m_look.mid, 1.0);
                if (off) {
                    pen.setStyle(Qt::DashLine);
                }
                painter.setPen(pen);
                painter.setBrush(m_look.window);
                painter.drawRoundedRect(box, 3.0, 3.0);
                painter.setFont(font);
                painter.setPen(m_look.mutedText);
                painter.drawText(box.adjusted(CHIP_PADDING, 0.0, -CHIP_PADDING, 0.0),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 chipMetrics.elidedText(text, Qt::ElideRight, textWidth));
                x = box.right() + CHIP_GAP;
            };
            if (event->chapters.isEmpty()) {
                //: An event that no chapter tells. In Polish: poza książką
                chip(tr("not in the book"), true);
            }
            for (const QString& chapter : event->chapters) {
                const TimelineIndex::Entry* entry = m_scene->index().chapter(chapter);
                //: A chapter that was deleted. In Polish: (usunięty)
                chip(entry != nullptr ? entry->name : tr("(deleted)"), false);
            }
        }
        painter.restore();
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QRectF(rect.left() + DOT_LEFT, rect.center().y() - DOT / 2.0, DOT,
                                   DOT));
        painter.setFont(m_scene->font());
        painter.setPen(m_look.text);
        const double width = rect.width() - LABEL_TEXT_LEFT - PADDING_X;
        painter.drawText(QRectF(rect.left() + LABEL_TEXT_LEFT, rect.top(), width, rect.height()),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetricsF(m_scene->font())
                             .elidedText(event->title, Qt::ElideRight, width));
    }
    if (selected) {
        painter.setPen(QPen(m_look.selection, SELECTION_WIDTH));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect.adjusted(-1.5, -1.5, 1.5, 1.5), radius + 1.0, radius + 1.0);
    }
    painter.setOpacity(opacity);
}

void TimelineAxisView::paintDrag(QPainter& painter) const {
    if (!m_drag || !m_drag->started) {
        return;
    }
    const TimelineItem* item = m_scene->itemOf(m_drag->id, m_drag->lane);
    if (item == nullptr) {
        return;
    }
    // The card follows the mouse
    TimelineItem ghost = *item;
    ghost.rect = QRectF(QPointF(m_drag->at) - m_drag->grip, m_drag->size);
    painter.setOpacity(GHOST_OPACITY);
    paintItem(painter, ghost, true);
    painter.setOpacity(1.0);
    if (!m_drag->valid || m_drag->label.isEmpty()) {
        return;
    }
    // What the drop does, beside the line
    painter.setFont(m_scene->smallFont());
    const QFontMetricsF metrics(m_scene->smallFont());
    const QPointF line = fromLanes(QPointF(m_drag->dropX, 0.0));
    const double width = metrics.horizontalAdvance(m_drag->label) + 2 * DROP_LABEL_PADDING;
    const double height = metrics.height() + 4.0;
    const QRectF box(std::min(line.x() + 4.0, viewport()->width() - width - 2.0),
                     m_scene->lanesTop() + 4.0, width, height);
    painter.setPen(m_look.drop);
    painter.setBrush(m_look.window);
    painter.drawRoundedRect(box, height / 2.0, height / 2.0);
    painter.setPen(m_look.text);
    painter.drawText(box, Qt::AlignCenter, m_drag->label);
}

}  // namespace kalahari::timeline
