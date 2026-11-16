// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_edits.h
/// @brief The changes of a timeline that its axis, its list and the time and the book share

#ifndef KALAHARI_TIMELINE_TIMELINE_EDITS_H
#define KALAHARI_TIMELINE_TIMELINE_EDITS_H

#include <kalahari/timeline/timeline.h>

#include <QColor>
#include <QCoreApplication>
#include <QIcon>
#include <QString>
#include <QStringList>

#include <optional>

class QMenu;

namespace kalahari::timeline {

class TimelineDocument;
struct TimelineIndex;
struct TimelineLook;

/// @brief The changes of a timeline its views share; each is a step of Undo, and says what it
/// did in @p message for the bar under the timeline
class TimelineEdits {
    Q_DECLARE_TR_FUNCTIONS(TimelineEdits)

public:
    /// @brief Whether @p event is one that @p filter highlights: "" all, "thread:<id>",
    /// "character:<id or name>", "place:<id or name>", "unbooked" (not in the book)
    [[nodiscard]] static bool matches(const TimelineEvent& event, const QString& filter);

    /// @brief A new event after event @p after (empty: after the first in time): the day after
    /// it, in lane @p lane (empty: its lane); it is selected
    /// @return Its id; empty: none
    static QString addAfter(TimelineDocument& document, const QString& after,
                            const std::optional<QString>& lane, QString* message);

    /// @brief A new event at @p day (in the order of the story: at place @p slot, from 0) in
    /// lane @p lane; it is selected
    static QString addAt(TimelineDocument& document, double day, int slot, const QString& lane,
                         const TimelineLook& look, QString* message);

    /// @brief Delete event @p id; the event after it in time is selected
    static bool remove(TimelineDocument& document, const QString& id, QString* message);

    /// @brief Move event @p id a step earlier (@p direction -1) or later (1): in the order of
    /// the story a place, else a day, a month or a year, as its time and @p pixelsPerDay say
    static bool shift(TimelineDocument& document, const QString& id, int direction,
                      double pixelsPerDay, const TimelineLook& look, QString* message);

    /// @brief What a step of shift() is: "a day", "a month", "a year", "a place"
    [[nodiscard]] static QString stepOf(const Timeline& timeline, double pixelsPerDay);

    /// @brief Event @p id from lane @p from to lane @p to (keys of TimelineLane)
    /// @param name The name of lane @p to, for the message
    static bool moveToLane(TimelineDocument& document, const QString& id, const QString& from,
                           const QString& to, const QString& name, QString* message);

    /// @brief Give @p event lane @p to in place of lane @p from, as @p lanes go
    /// @return False when it cannot be there: a character it already has
    static bool setLane(TimelineEvent& event, TimelineView::Lanes lanes, const QString& from,
                        const QString& to);

    /// @brief The time of @p event moved to @p day, kept as exactly as it was known; an event
    /// without a time gets the day
    [[nodiscard]] static StoryTime movedTo(const TimelineEvent& event, double day,
                                           double pixelsPerDay, TimelineView::Scale scale);

    /// @brief Add the colors of the palette to @p menu: the color of event @p id
    static void addColors(QMenu& menu, TimelineDocument& document, const TimelineLook& look,
                          const QString& id);

    /// @brief A swatch of color @p color, framed with @p frame, for the menus and the lists
    [[nodiscard]] static QIcon swatch(const QColor& color, const QColor& frame);

    /// @brief "“Kompas”" in the quotes of the language
    [[nodiscard]] static QString quoted(const QString& title);
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_TIMELINE_EDITS_H
