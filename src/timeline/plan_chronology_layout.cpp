// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plan_chronology_layout.cpp
/// @brief The chronology of the Book Plan without its painting: its columns, bands and
/// flashbacks

#include <kalahari/timeline/plan_chronology_layout.h>

#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <map>
#include <utility>

namespace kalahari::timeline {

namespace {

constexpr int MONTHS = 12;

/// The name of a period or a band as it is compared: "Amara" is "amara"
QString folded(const QString& text) {
    return text.simplified().toCaseFolded();
}

QString capitalized(QString text) {
    if (!text.isEmpty()) {
        text[0] = text.at(0).toUpper();
    }
    return text;
}

/// Julian day of the day before the first day of the month after @p day's
qint64 lastDayOfMonth(const CalendarDay& day) {
    const CalendarDay next =
        day.month == MONTHS ? CalendarDay{day.year + 1, 1, 1} : CalendarDay{day.year, day.month + 1, 1};
    return next.julianDay().value_or(day.julianDay().value_or(0)) - 1;
}

/// Where @p time starts and ends, in days: a moment of a day with a part of the day, else its
/// whole days
std::pair<double, double> spanOf(const StoryTime& time) {
    if (time.precision == StoryTime::Precision::Day && !time.isSpan() &&
        time.part != StoryTime::Part::None) {
        const double at = time.start();
        return {at, at};
    }
    const auto [from, to] = ChronologyLayout::daysOf(time);
    return {static_cast<double>(from), static_cast<double>(to) + 1.0};
}

/// The time of a column without what only a card says: its part of the day, "about"
StoryTime columnTime(StoryTime time) {
    time.part = StoryTime::Part::None;
    time.minutes = 0;
    time.approximate = false;
    return time;
}

/// What a column of time @p time says on scale @p options.scale
QString labelOf(const StoryTime& time, const ChronologyOptions& options) {
    const StoryTime shown = columnTime(time);
    if (shown.precision == StoryTime::Precision::Day &&
        options.scale == TimelineView::Scale::Days) {
        return shown.dayText(options.dayOne, options.style);
    }
    return capitalized(shown.dateText(options.style));
}

}  // namespace

// =============================================================================
// PlanPeriod
// =============================================================================

QList<PlanPeriod> PlanPeriod::fromJson(const QJsonArray& json) {
    QList<PlanPeriod> periods;
    for (const QJsonValue& value : json) {
        const QJsonObject object = value.toObject();
        const QString name = object.value(QStringLiteral("name")).toString().simplified();
        const std::optional<StoryTime> time =
            StoryTime::fromJson(object.value(QStringLiteral("time")).toObject());
        if (!name.isEmpty() && time) {
            periods.append(PlanPeriod{name, *time});
        }
    }
    return periods;
}

QJsonArray PlanPeriod::toJson(const QList<PlanPeriod>& periods) {
    QJsonArray json;
    for (const PlanPeriod& period : periods) {
        json.append(QJsonObject{{QStringLiteral("name"), period.name},
                                {QStringLiteral("time"), period.time.toJson()}});
    }
    return json;
}

// =============================================================================
// ChronologyLayout
// =============================================================================

std::optional<StoryTime> ChronologyLayout::timeOf(const QString& text, qint64 dayOne,
                                                  const QList<PlanPeriod>& periods) {
    const QString said = folded(text);
    if (said.isEmpty()) {
        return std::nullopt;
    }
    for (const PlanPeriod& period : periods) {
        const QString name = folded(period.name);
        if (said == name) {
            return period.time;
        }
        // "Wyprawa, wieczorem": a day of the writer with a part of the day
        if (said.startsWith(name + QLatin1Char(',')) &&
            period.time.precision == StoryTime::Precision::Day && !period.time.isSpan()) {
            const std::optional<StoryTime> part = StoryTime::parse(
                QStringLiteral("Day 1, ") + said.mid(name.size() + 1).trimmed(), dayOne);
            if (part && part->part != StoryTime::Part::None) {
                StoryTime time = period.time;
                time.part = part->part;
                time.minutes = part->minutes;
                return time;
            }
        }
    }
    return StoryTime::parse(text, dayOne);
}

std::pair<qint64, qint64> ChronologyLayout::daysOf(const StoryTime& time) {
    switch (time.precision) {
    case StoryTime::Precision::Month:
        return {time.first,
                time.isSpan() ? time.last : lastDayOfMonth(CalendarDay::of(time.first))};
    case StoryTime::Precision::Year: {
        const CalendarDay day = CalendarDay::of(time.first);
        return {time.first, lastDayOfMonth(CalendarDay{day.year, MONTHS, 1})};
    }
    case StoryTime::Precision::Day:
        break;
    }
    return {time.first, time.last};
}

ChronologyLayout::ChronologyLayout(QList<ChronologyItem> items, const ChronologyOptions& options)
    : m_items(std::move(items)) {
    m_itemLane.fill(-1, m_items.size());
    m_itemColumn.fill(-1, m_items.size());
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items.at(i).time) {
            m_untimed.append(i);
        }
    }
    layOutColumns(options);
    layOutLanes(options);
    markFlashbacks();
}

void ChronologyLayout::layOutColumns(const ChronologyOptions& options) {
    // The times of the cards, each once, in the order of time
    std::map<std::pair<qint64, qint64>, int> first;  // the days of a time -> its first card
    for (int i = 0; i < m_items.size(); ++i) {
        if (const std::optional<StoryTime>& time = m_items.at(i).time) {
            first.try_emplace(daysOf(*time), i);
        }
    }
    const bool ordered = options.scale == TimelineView::Scale::Order;
    std::map<std::pair<qint64, qint64>, int> columnOfDays;
    qint64 reached = 0;  // the last day of the times so far
    bool started = false;
    bool lastWasDay = false;
    const auto addDay = [this, &options](qint64 day) {
        ChronologyColumn column;
        column.label = labelOf(StoryTime::day(day), options);
        column.from = day;
        column.to = day;
        m_columns.append(column);
    };
    for (const auto& [days, item] : first) {
        const StoryTime& time = *m_items.at(item).time;
        const bool day = time.precision == StoryTime::Precision::Day && !time.isSpan();
        if (started && !ordered && days.first > reached + 1) {
            const qint64 gap = days.first - reached - 1;
            if (!options.compress && gap <= MOST_EMPTY_DAYS && day && lastWasDay) {
                for (qint64 empty = reached + 1; empty < days.first; ++empty) {
                    addDay(empty);
                }
            } else {
                ChronologyColumn column;
                column.gap = true;
                column.label =
                    StoryTime::lengthText(static_cast<double>(gap), options.style.language);
                column.from = reached + 1;
                column.to = days.first - 1;
                m_columns.append(column);
            }
        }
        ChronologyColumn column;
        column.label = ordered ? capitalized(m_items.at(item).timeText.simplified())
                               : labelOf(time, options);
        column.from = days.first;
        column.to = days.second;
        columnOfDays.emplace(days, static_cast<int>(m_columns.size()));
        m_columns.append(column);
        reached = started ? std::max(reached, days.second) : days.second;
        started = true;
        lastWasDay = day;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        if (const std::optional<StoryTime>& time = m_items.at(i).time) {
            m_itemColumn[i] = columnOfDays.at(daysOf(*time));
        }
    }
}

void ChronologyLayout::layOutLanes(const ChronologyOptions& options) {
    const auto keyOf = [&options](const ChronologyItem& item) {
        switch (options.lanes) {
        case TimelineView::Lanes::One:
            return QStringLiteral("*");
        case TimelineView::Lanes::Places:
            return folded(item.place);
        case TimelineView::Lanes::Characters:
        case TimelineView::Lanes::Threads:
            break;
        }
        return folded(item.pov);
    };
    const auto nameOf = [&options](const ChronologyItem& item) {
        return (options.lanes == TimelineView::Lanes::Places ? item.place : item.pov).simplified();
    };
    bool none = false;
    for (int i = 0; i < m_items.size(); ++i) {
        const ChronologyItem& item = m_items.at(i);
        if (!item.time) {
            continue;
        }
        const QString key = keyOf(item);
        if (key.isEmpty()) {
            none = true;
            continue;
        }
        if (std::ranges::none_of(m_lanes,
                                 [&key](const ChronologyLane& lane) { return lane.key == key; })) {
            m_lanes.append(ChronologyLane{
                key, options.lanes == TimelineView::Lanes::One ? QString() : nameOf(item)});
        }
    }
    // The cards without a point of view (a place) in the last band
    if (none) {
        m_lanes.append(ChronologyLane{});
    }
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items.at(i).time) {
            continue;
        }
        const QString key = keyOf(m_items.at(i));
        const auto lane = std::ranges::find_if(
            m_lanes, [&key](const ChronologyLane& found) { return found.key == key; });
        m_itemLane[i] = static_cast<int>(std::distance(m_lanes.begin(), lane));
    }
}

void ChronologyLayout::markFlashbacks() {
    std::optional<std::pair<double, double>> before;
    for (ChronologyItem& item : m_items) {
        if (item.outside || !item.time) {
            continue;
        }
        const std::pair<double, double> span = spanOf(*item.time);
        if (before) {
            // Ends before the time of the card told before it starts; the same moment is no
            // flashback
            item.flashback = span.second < before->first ||
                             (span.second == before->first && span.second > span.first);
        }
        before = span;
    }
}

QList<int> ChronologyLayout::cell(int lane, int column) const {
    QList<int> found;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_itemLane.at(i) == lane && m_itemColumn.at(i) == column) {
            found.append(i);
        }
    }
    std::ranges::stable_sort(found, [this](int left, int right) {
        return m_items.at(left).time->start() < m_items.at(right).time->start();
    });
    return found;
}

int ChronologyLayout::laneOf(int item) const {
    return item >= 0 && item < m_itemLane.size() ? m_itemLane.at(item) : -1;
}

int ChronologyLayout::columnOf(int item) const {
    return item >= 0 && item < m_itemColumn.size() ? m_itemColumn.at(item) : -1;
}

int ChronologyLayout::indexOf(const QString& id) const {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

}  // namespace kalahari::timeline
