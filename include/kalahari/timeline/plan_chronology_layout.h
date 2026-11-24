// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plan_chronology_layout.h
/// @brief The chronology of the Book Plan without its painting: which time each card has, its
/// column and its band, the gaps between the columns and the flashbacks

#ifndef KALAHARI_TIMELINE_PLAN_CHRONOLOGY_LAYOUT_H
#define KALAHARI_TIMELINE_PLAN_CHRONOLOGY_LAYOUT_H

#include <kalahari/timeline/story_time.h>
#include <kalahari/timeline/timeline.h>

#include <QColor>
#include <QJsonArray>
#include <QList>
#include <QString>

#include <optional>
#include <utility>

namespace kalahari::timeline {

/// @brief A period the writer names: "Wyprawa" is "zima 1931"; a time of the action that says
/// its name has its time
struct PlanPeriod {
    QString name;    ///< "Wyprawa"
    StoryTime time;  ///< When it is

    bool operator==(const PlanPeriod&) const = default;

    /// @brief The periods of a plan file ("periods": [{ "name": ..., "time": {...} }])
    [[nodiscard]] static QList<PlanPeriod> fromJson(const QJsonArray& json);
    [[nodiscard]] static QJsonArray toJson(const QList<PlanPeriod>& periods);
};

/// @brief A card of the chronology: a scene or a text of the book, or an event outside the text
struct ChronologyItem {
    QString id;            ///< Of a card of the book its id; of an event "event:" and its id
    bool outside = false;  ///< An event outside the text
    bool planned = false;  ///< In the plan: no text yet
    QString number;        ///< Its place in the order of reading ("3.2"); empty outside the text
    QString title;
    QString description;
    QString pov;           ///< Point of view; of an event its first character
    QString place;
    QString timeText;      ///< The time of the action as the writer typed it
    QString textTitle;     ///< The title of its text
    QColor color;          ///< Its own color; invalid: none
    std::optional<StoryTime> time;  ///< What the time says; none: no time the program reads
    bool textTime = false;   ///< A scene without a time of its own: timeText is its text's
    bool flashback = false;  ///< Told after a card whose time is later
};

/// @brief A column of the chronology: a time, or a gap without cards between two times
struct ChronologyColumn {
    bool gap = false;
    QString label;       ///< "Dzień 3", "Wiosna 1931"; of a gap "≈ 27 lat"
    qint64 from = 0;     ///< Julian day of its first day
    qint64 to = 0;       ///< Julian day of its last day
};

/// @brief A band of the chronology: the cards of a point of view, of a place, or all
struct ChronologyLane {
    QString key;   ///< Folded name; empty: none said
    QString name;  ///< As the first card writes it
};

/// @brief How the chronology shows the cards
struct ChronologyOptions {
    TimelineView::Scale scale = TimelineView::Scale::Days;
    TimelineView::Lanes lanes = TimelineView::Lanes::Characters;  ///< Characters: points of view
    bool compress = true;  ///< Gaps without cards are one narrow column
    qint64 dayOne = 0;
    StoryTimeStyle style;
};

/// @brief Where the chronology puts each card
///
/// The columns are the times of the cards in their order: a day ("Dzień 3", "16 kwietnia 1931"),
/// days in a row, a month, a season, a year. Between two times with days between them stands a
/// gap that says how long it is ("4 dni", "≈ 27 lat"); without compress() the days of a gap of
/// at most MOST_EMPTY_DAYS days are empty columns. The order of the story (Scale::Order) has no
/// gaps, and its columns say the time as the first card writes it. A card the time of which
/// ends before the time of the card told before it starts is a flashback.
class ChronologyLayout {
public:
    /// The most days of a gap shown as empty columns without compress()
    static constexpr qint64 MOST_EMPTY_DAYS = 31;

    /// @param items The cards of the book in the order of reading, then the events outside the
    ///        text; their times are read already (timeOf())
    ChronologyLayout(QList<ChronologyItem> items, const ChronologyOptions& options);

    [[nodiscard]] const QList<ChronologyItem>& items() const { return m_items; }
    [[nodiscard]] const QList<ChronologyColumn>& columns() const { return m_columns; }
    [[nodiscard]] const QList<ChronologyLane>& lanes() const { return m_lanes; }

    /// @brief The cards in band @p lane and column @p column, in the order of their time
    [[nodiscard]] QList<int> cell(int lane, int column) const;

    /// @brief The cards without a time the program reads, in the order of reading
    [[nodiscard]] const QList<int>& untimed() const { return m_untimed; }

    /// @brief The band and the column of card @p item; -1 for a card without a time
    [[nodiscard]] int laneOf(int item) const;
    [[nodiscard]] int columnOf(int item) const;

    /// @brief The card of id @p id; -1: none
    [[nodiscard]] int indexOf(const QString& id) const;

    /// @brief The time that @p text says: that of a period of the writer it names ("Wyprawa",
    /// "Wyprawa, wieczorem" is its day with the part of the day), else what StoryTime reads
    [[nodiscard]] static std::optional<StoryTime> timeOf(const QString& text, qint64 dayOne,
                                                         const QList<PlanPeriod>& periods);

    /// @brief The days of @p time, whole: of a month from its first to its last day
    [[nodiscard]] static std::pair<qint64, qint64> daysOf(const StoryTime& time);

private:
    void layOutColumns(const ChronologyOptions& options);
    void layOutLanes(const ChronologyOptions& options);
    void markFlashbacks();

    QList<ChronologyItem> m_items;
    QList<ChronologyColumn> m_columns;
    QList<ChronologyLane> m_lanes;
    QList<int> m_itemLane;    ///< Of each card; -1 without a time
    QList<int> m_itemColumn;  ///< Of each card; -1 without a time
    QList<int> m_untimed;
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_PLAN_CHRONOLOGY_LAYOUT_H
