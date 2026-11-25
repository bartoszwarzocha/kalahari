// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file timeline_scene.cpp
/// @brief Where the axis of a timeline draws each lane, event, tick and gap

#include <kalahari/timeline/timeline_scene.h>

#include <QFontMetricsF>
#include <QHash>

#include <algorithm>
#include <cmath>

namespace kalahari::timeline {

namespace {

constexpr double LABEL_HEIGHT = 22.0;
constexpr double LABEL_ROW = 28.0;
constexpr double LABEL_ROOM = 190.0;
constexpr double LABEL_PADDING = 30.0;
constexpr double ROW_GAP = 8.0;
constexpr double CARD_TOP = 22.0;
constexpr double LABEL_TOP = 18.0;
constexpr double LANE_BOTTOM = 4.0;
constexpr double ROOM_AFTER = 26.0;
constexpr double ORDER_LABEL_WIDTH = 150.0;
constexpr double END_ROOM = 70.0;
constexpr double LEAST_SPAN = 8.0;
constexpr double SMALL_FONT = 0.9;
constexpr double TRAY_PADDING = 8.0;
constexpr double TRAY_LABEL_HEIGHT = 24.0;
constexpr double EMPTY_ROOM = 40.0;

// The ticks: how many pixels a day needs for each kind of them
constexpr double DAYS_FROM = 12.0;     ///< A tick for each day
constexpr double EVERY_DAY_FROM = 25.0;  ///< A label for each day, else every other day
constexpr double WEEKS_FROM = 2.0;     ///< Labels of weeks, months in the upper row
constexpr double MINOR_FROM = 4.0;     ///< Minor ticks between the weeks
constexpr double LEAST_SEGMENT = 30.0;
constexpr double UPPER_ROOM = 96.0;
constexpr double UPPER_END = 20.0;
constexpr int DAYS_PER_WEEK = 7;

QString dayNumberText(int number) {
    return number < 0 ? QString(QChar(0x2212)) + QString::number(-number) : QString::number(number);
}

}  // namespace

TimelineScene::TimelineScene(const Timeline& timeline, const TimelineIndex& index,
                             const TimelineLook& look, const QFont& font)
    : m_timeline(&timeline),
      m_index(&index),
      m_look(look),
      m_font(font),
      m_titleFont(font),
      m_smallFont(font) {
    m_titleFont.setWeight(QFont::DemiBold);
    if (font.pointSizeF() > 0) {
        m_smallFont.setPointSizeF(font.pointSizeF() * SMALL_FONT);
    } else if (font.pixelSize() > 0) {
        m_smallFont.setPixelSize(std::max(1, static_cast<int>(font.pixelSize() * SMALL_FONT)));
    }
    m_titleHeight = QFontMetricsF(m_titleFont).height();
    m_smallHeight = QFontMetricsF(m_smallFont).height();
    m_cardHeight = 6.0 + m_titleHeight + 5.0;
    if (m_look.showTime) {
        m_cardHeight += m_smallHeight + 1.0;
    }
    if (m_look.showCharacters || m_look.showPlace) {
        m_cardHeight += m_smallHeight + 1.0;
    }
    if (m_look.showChapters) {
        m_cardHeight += m_smallHeight + 7.0;
    }
}

bool TimelineScene::ordered() const {
    return m_timeline->view().scale == TimelineView::Scale::Order;
}

bool TimelineScene::cards() const {
    return m_timeline->view().look == TimelineView::Look::Cards;
}

// =============================================================================
// The layout
// =============================================================================

void TimelineScene::layout(double trayWidth) {
    m_onAxis = ordered() ? m_timeline->eventsInOrder() : m_timeline->eventsInTime();
    m_axis.reset();
    m_step = 0.0;
    if (ordered()) {
        m_step = (cards() ? CARD_WIDTH : ORDER_LABEL_WIDTH) + ROOM_AFTER;
        m_width = ORIGIN + static_cast<double>(m_onAxis.size()) * m_step + EMPTY_ROOM;
    } else {
        QList<TimeAxis::Span> spans;
        for (const TimelineEvent* event : m_onAxis) {
            if (event->time) {
                spans.append(TimeAxis::Span{event->time->start(), event->time->end()});
            }
        }
        TimeAxis::Options options;
        options.pixelsPerDay = TimeAxis::pixelsPerDayOf(m_timeline->view().zoom);
        options.roomBefore = ORIGIN;
        options.roomAfter = (cards() ? CARD_WIDTH : LABEL_ROOM) + ROOM_AFTER;
        options.compress = m_timeline->view().compress;
        m_axis.emplace(spans, options, static_cast<double>(m_timeline->dayOne()) + 0.5);
        m_width = ORIGIN + m_axis->width() + END_ROOM;
    }
    layoutLanes();
    layoutTicks();
    layoutTray(trayWidth);
}

double TimelineScene::spanOf(const TimelineEvent& event) const {
    if (ordered() || !event.time || !event.time->isSpan() || !m_axis) {
        return 0.0;
    }
    return std::max(LEAST_SPAN,
                    m_axis->x(event.time->end()) - m_axis->x(event.time->start()));
}

double TimelineScene::widthOf(const TimelineEvent& event, double span) const {
    if (cards()) {
        return std::max(CARD_WIDTH, span);
    }
    const double text = QFontMetricsF(m_font).horizontalAdvance(event.title);
    return std::max(std::ceil(text) + LABEL_PADDING, span);
}

double TimelineScene::xOf(const TimelineEvent& event) const {
    if (ordered()) {
        return xOfSlot(static_cast<int>(m_onAxis.indexOf(&event)));
    }
    return event.time && m_axis ? ORIGIN + m_axis->x(event.time->start()) : ORIGIN;
}

QStringList TimelineScene::laneKeysOf(const TimelineEvent& event) const {
    switch (m_timeline->view().lanes) {
        case TimelineView::Lanes::Threads:
            return {m_timeline->thread(event.thread) != nullptr ? event.thread : QString()};
        case TimelineView::Lanes::Characters:
            return event.characters.isEmpty() ? QStringList{QString()} : event.characters;
        case TimelineView::Lanes::Places:
            return {event.place};
        case TimelineView::Lanes::One:
            break;
    }
    return {QStringLiteral("*")};
}

void TimelineScene::layoutLanes() {
    m_lanes.clear();
    m_items.clear();
    const TimelineView::Lanes kind = m_timeline->view().lanes;
    const QString kindName = TimelineWords::laneKind(kind);
    std::size_t next = 0;
    const auto nextColor = [this, &next]() {
        const QColor color = m_look.palette.at(next % m_look.palette.size());
        ++next;
        return color;
    };
    switch (kind) {
        case TimelineView::Lanes::Threads:
            for (const TimelineThread& thread : m_timeline->threads()) {
                const QColor color = m_look.paletteColor(thread.color);
                m_lanes.append(TimelineLane{thread.id, thread.name, kindName,
                                            color.isValid() ? color : m_look.none});
            }
            break;
        case TimelineView::Lanes::Characters:
        case TimelineView::Lanes::Places: {
            const bool characters = kind == TimelineView::Lanes::Characters;
            const QList<TimelineIndex::Entry>& cards =
                characters ? m_index->characters : m_index->places;
            for (const TimelineIndex::Entry& card : cards) {
                const QColor own = card.color.isValid() ? card.color
                                   : characters         ? nextColor()
                                                        : m_look.mutedText;
                m_lanes.append(TimelineLane{card.id, card.name, kindName, own});
            }
            // The names without a card that the events give, in the order of the story
            for (const TimelineEvent* event : m_timeline->eventsInOrder()) {
                const QStringList names =
                    characters ? event->characters : QStringList{event->place};
                for (const QString& name : names) {
                    if (!name.isEmpty() && lane(name) == nullptr) {
                        m_lanes.append(TimelineLane{name, name, kindName,
                                                    characters ? nextColor() : m_look.mutedText});
                    }
                }
            }
            break;
        }
        case TimelineView::Lanes::One:
            m_lanes.append(TimelineLane{QStringLiteral("*"), TimelineWords::withoutLane(kind),
                                        QString(), m_look.mutedText});
            break;
    }
    if (kind != TimelineView::Lanes::One) {
        m_lanes.append(TimelineLane{QString(), TimelineWords::withoutLane(kind), QString(),
                                    m_look.none});
    }

    // The events of each lane, in rows that do not overlap
    QHash<QString, QList<TimelineItem>> byLane;
    for (const TimelineEvent* event : m_onAxis) {
        const double span = spanOf(*event);
        const double x = xOf(*event);
        for (const QString& key : laneKeysOf(*event)) {
            byLane[key].append(TimelineItem{event->id, key, QRectF(x, 0.0, widthOf(*event, span),
                                                                   cards() ? m_cardHeight
                                                                           : LABEL_HEIGHT),
                                            span});
        }
    }
    const double rowHeight = cards() ? m_cardHeight + ROW_GAP : LABEL_ROW;
    const double rowsTop = cards() ? CARD_TOP : LABEL_TOP;
    double y = 0.0;
    QList<TimelineLane> shown;
    for (TimelineLane laneOf : m_lanes) {
        QList<TimelineItem> items = byLane.value(laneOf.key);
        if (items.isEmpty() && laneOf.key.isEmpty()) {
            continue;  // The lane of the events without a thread, while there are none
        }
        std::stable_sort(items.begin(), items.end(),
                         [](const TimelineItem& a, const TimelineItem& b) {
                             return a.rect.x() < b.rect.x();
                         });
        QList<double> ends;
        for (TimelineItem& item : items) {
            qsizetype row = 0;
            while (row < ends.size() && ends.at(row) + ROW_GAP > item.rect.x()) {
                ++row;
            }
            if (row == ends.size()) {
                ends.append(0.0);
            }
            ends[row] = item.rect.right();
            item.rect.moveTop(y + rowsTop + static_cast<double>(row) * rowHeight);
            m_items.append(item);
        }
        laneOf.top = y;
        laneOf.height =
            rowsTop + static_cast<double>(std::max<qsizetype>(1, ends.size())) * rowHeight +
            LANE_BOTTOM;
        laneOf.count = static_cast<int>(items.size());
        y += laneOf.height;
        shown.append(laneOf);
    }
    m_lanes = shown;
    m_lanesHeight = y;
}

void TimelineScene::layoutTicks() {
    m_ticks.clear();
    m_gaps.clear();
    if (!m_axis) {
        return;
    }
    const TimeLanguage language = m_look.style.language;
    const bool days = m_timeline->view().scale == TimelineView::Scale::Days;
    const double perDay = m_axis->pixelsPerDay();
    const auto monthText = [language](const CalendarDay& day) {
        return StoryTime::monthName(day.month, language) + QLatin1Char(' ') +
               StoryTime::yearText(day.year, language);
    };
    for (const TimeAxis::Segment& segment : m_axis->segments()) {
        const double x0 = ORIGIN + segment.x;
        const double x1 = x0 + (segment.to - segment.from) * perDay;
        if (x1 - x0 < LEAST_SEGMENT) {
            continue;
        }
        const auto first = static_cast<qint64>(std::ceil(segment.from));
        const auto last = static_cast<qint64>(std::floor(segment.to));
        double lastUpper = -1e9;
        const auto upper = [this, &lastUpper, x1](double x, const QString& label) {
            if (x - lastUpper > UPPER_ROOM && x < x1 - UPPER_END) {
                m_ticks.append(TimelineTick{x, label, TimelineTick::Kind::Upper});
                lastUpper = x;
            }
        };
        const qint64 dayOne = m_timeline->dayOne();
        if (days) {
            //: The upper row of the axis of a timeline in days of the story. In Polish:
            //: dni %1 … %2
            upper(x0 + 4.0, tr("days %1 … %2")
                                .arg(dayNumberText(StoryTime::dayNumber(first, dayOne)),
                                     dayNumberText(StoryTime::dayNumber(last, dayOne))));
        } else {
            const CalendarDay start = CalendarDay::of(first);
            upper(x0 + 4.0, perDay < WEEKS_FROM ? StoryTime::yearText(start.year, language)
                                                : monthText(start));
        }
        for (qint64 day = first; day <= last; ++day) {
            const double x = x0 + (static_cast<double>(day) - segment.from) * perDay;
            const CalendarDay date = CalendarDay::of(day);
            const int number = StoryTime::dayNumber(day, dayOne);
            if (!days && date.day == 1 && perDay >= WEEKS_FROM) {
                upper(x + 2.0, monthText(date));
            }
            if (!days && date.day == 1 && date.month == 1 && perDay < WEEKS_FROM) {
                upper(x + 2.0, StoryTime::yearText(date.year, language));
            }
            const QString label = days ? dayNumberText(number) : QString::number(date.day);
            if (perDay >= DAYS_FROM) {
                const bool everyDay = perDay >= EVERY_DAY_FROM;
                const bool show = days ? (everyDay || number % 2 != 0)
                                       : (everyDay || (date.day - 1) % 2 == 0);
                m_ticks.append(TimelineTick{x, QString(), TimelineTick::Kind::Minor});
                if (show && static_cast<double>(day + 1) <= segment.to + 1e-9) {
                    m_ticks.append(TimelineTick{x + perDay / 2.0, label,
                                                TimelineTick::Kind::Middle});
                }
            } else if (perDay >= WEEKS_FROM) {
                const bool major = days ? (number - 1) % DAYS_PER_WEEK == 0
                                        : (date.day == 1 || date.day == 8 || date.day == 15 ||
                                           date.day == 22);
                if (major) {
                    m_ticks.append(TimelineTick{x, label, TimelineTick::Kind::Major});
                } else if (perDay >= MINOR_FROM) {
                    m_ticks.append(TimelineTick{x, QString(), TimelineTick::Kind::Minor});
                }
            } else if (date.day == 1) {
                m_ticks.append(TimelineTick{x, StoryTime::monthName(date.month, language).left(3),
                                            TimelineTick::Kind::Major});
            }
        }
    }
    for (const TimeAxis::Gap& gap : m_axis->gaps()) {
        m_gaps.append(TimelineGapMark{ORIGIN + gap.x, gap.width,
                                      StoryTime::lengthText(gap.days, language)});
    }
}

void TimelineScene::layoutTray(double trayWidth) {
    m_tray.clear();
    m_trayText.clear();
    m_trayTextRect = QRectF();
    m_trayHeight = 0.0;
    if (ordered()) {
        return;
    }
    const QList<const TimelineEvent*> waiting = m_timeline->eventsWithoutTime();
    if (waiting.isEmpty()) {
        return;
    }
    //: Before the events of a timeline without a time. %1: the tray; %2: how many. In
    //: Polish: %1 (%2) – przeciągnij na oś albo wpisz czas:
    m_trayText = tr("%1 (%2) – drag onto the axis or give a time:")
                     .arg(TimelineWords::trayName())
                     .arg(waiting.size());
    const QFontMetricsF metrics(m_smallFont);
    const double height = cards() ? m_cardHeight : TRAY_LABEL_HEIGHT;
    const double textWidth = std::ceil(metrics.horizontalAdvance(m_trayText));
    m_trayTextRect = QRectF(TRAY_PADDING, TRAY_PADDING, textWidth, height);
    double x = TRAY_PADDING + textWidth + TRAY_PADDING;
    double y = TRAY_PADDING;
    const double right = std::max(trayWidth, TRAY_CARD_WIDTH * 2.0) - TRAY_PADDING;
    for (const TimelineEvent* event : waiting) {
        const double width =
            cards() ? TRAY_CARD_WIDTH
                    : std::ceil(QFontMetricsF(m_font).horizontalAdvance(event->title)) +
                          LABEL_PADDING;
        if (x + width > right && x > TRAY_PADDING) {
            x = TRAY_PADDING;
            y += height + TRAY_PADDING;
        }
        m_tray.append(TimelineItem{event->id, QString(), QRectF(x, y, width, height), 0.0});
        x += width + TRAY_PADDING;
    }
    m_trayHeight = y + height + TRAY_PADDING;
}

// =============================================================================
// Where things are
// =============================================================================

double TimelineScene::xOfDay(double day) const {
    return m_axis ? ORIGIN + m_axis->x(day) : ORIGIN;
}

double TimelineScene::dayAt(double x) const {
    return m_axis ? m_axis->dayAt(x - ORIGIN) : static_cast<double>(m_timeline->dayOne());
}

int TimelineScene::slotAt(double x) const {
    if (m_step <= 0.0) {
        return 0;
    }
    const auto slot = static_cast<int>(std::lround((x - ORIGIN) / m_step));
    return std::clamp(slot, 0, std::max(0, static_cast<int>(m_onAxis.size()) - 1));
}

double TimelineScene::xOfSlot(int slot) const {
    return ORIGIN + static_cast<double>(slot) * m_step;
}

double TimelineScene::pixelsPerDay() const {
    return m_axis ? m_axis->pixelsPerDay() : 0.0;
}

const TimelineItem* TimelineScene::itemAt(const QPointF& point) const {
    // The last drawn is on top
    for (auto it = m_items.crbegin(); it != m_items.crend(); ++it) {
        if (it->rect.contains(point)) {
            return &*it;
        }
    }
    return nullptr;
}

const TimelineItem* TimelineScene::trayItemAt(const QPointF& point) const {
    for (const TimelineItem& item : m_tray) {
        if (item.rect.contains(point)) {
            return &item;
        }
    }
    return nullptr;
}

const TimelineLane* TimelineScene::laneAt(double y) const {
    for (const TimelineLane& laneOf : m_lanes) {
        if (y >= laneOf.top && y < laneOf.top + laneOf.height) {
            return &laneOf;
        }
    }
    return nullptr;
}

const TimelineLane* TimelineScene::lane(const QString& key) const {
    for (const TimelineLane& laneOf : m_lanes) {
        if (laneOf.key == key) {
            return &laneOf;
        }
    }
    return nullptr;
}

const TimelineItem* TimelineScene::itemOf(const QString& id, const QString& lane) const {
    const TimelineItem* first = nullptr;
    for (const TimelineItem& item : m_items) {
        if (item.id == id) {
            if (item.lane == lane) {
                return &item;
            }
            if (first == nullptr) {
                first = &item;
            }
        }
    }
    if (first != nullptr) {
        return first;
    }
    for (const TimelineItem& item : m_tray) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}

QList<const TimelineItem*> TimelineScene::itemsOf(const QString& id) const {
    QList<const TimelineItem*> found;
    for (const TimelineItem& item : m_items) {
        if (item.id == id) {
            found.append(&item);
        }
    }
    return found;
}

QColor TimelineScene::colorOf(const TimelineEvent& event) const {
    const QColor own = m_look.paletteColor(event.color);
    if (own.isValid()) {
        return own;
    }
    if (const TimelineThread* thread = m_timeline->thread(event.thread); thread != nullptr) {
        const QColor color = m_look.paletteColor(thread->color);
        if (color.isValid()) {
            return color;
        }
    }
    return m_look.none;
}

bool TimelineScene::rough(const TimelineEvent& event) {
    return event.time &&
           (event.time->approximate || event.time->precision != StoryTime::Precision::Day);
}

QString TimelineScene::whenOf(const TimelineEvent& event) const {
    return TimelineWords::when(*m_timeline, event, m_look);
}

QString TimelineScene::detailsOf(const TimelineEvent& event) const {
    QStringList parts;
    if (m_look.showCharacters && !event.characters.isEmpty()) {
        parts.append(m_index->namesOf(event.characters));
    }
    if (m_look.showPlace && !event.place.isEmpty()) {
        parts.append(m_index->nameOf(event.place));
    }
    return parts.join(QStringLiteral(" · "));
}

}  // namespace kalahari::timeline
