// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline.h
/// @brief A timeline of the Workshop: the events of a story in time, in threads, with the
/// characters, the place and the chapters of each, as its .ktl file keeps them

#ifndef KALAHARI_CORE_TIMELINE_H
#define KALAHARI_CORE_TIMELINE_H

#include <kalahari/core/story_time.h>

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace kalahari::core {

/// @brief A thread of a timeline: a line of the plot that events belong to
struct TimelineThread {
    QString id;         ///< Unique in the timeline
    QString name;       ///< "Wyprawa"
    QString color;      ///< Key of a color of the palette ("blue"...); empty: none
    QJsonObject extra;  ///< Fields this version does not know, saved back

    bool operator==(const TimelineThread&) const = default;
};

/// @brief An event of a timeline
struct TimelineEvent {
    QString id;                     ///< Unique in the timeline
    QString title;                  ///< "Amara znajduje mapę"
    std::optional<StoryTime> time;  ///< When it happens; none: it waits beside the axis
    QString timeText;               ///< Without a time: what the writer typed as one, which
                                    ///< the program does not read ("rok później")
    QString orderText;              ///< Its time as the order of the story shows it ("Tej
                                    ///< samej nocy"); empty: none
    int order = 0;                  ///< Its place in the order of the story, from 1
    QString thread;                 ///< Id of its thread; empty: none
    QStringList characters;         ///< Who is there: ids of the cards of characters, or
                                    ///< names of characters without a card
    QString place;                  ///< Where: id of the card of a place, or its name; empty:
                                    ///< not said
    QStringList chapters;           ///< Ids of the elements of the book that tell it
                                    ///< (chapters, scenes), in the order of the book
    QString description;            ///< What happens
    QString color;                  ///< Key of a color of the palette; empty: its thread's
    QJsonObject extra;              ///< Fields this version does not know, saved back

    bool operator==(const TimelineEvent&) const = default;
};

/// @brief How a timeline shows its events, kept with it
struct TimelineView {
    /// @brief What the axis measures
    enum class Scale {
        Dates,  ///< Days of the calendar
        Days,   ///< Days of the story, counted from day one
        Order   ///< The order of the story, without lengths of time
    };

    /// @brief What a band of the axis holds
    enum class Lanes {
        Threads,     ///< The events of a thread
        Characters,  ///< The events of a character; an event of several characters is in each
        Places,      ///< The events of a place
        One          ///< All events
    };

    /// @brief How the program shows the events on the axis
    enum class Look {
        Cards,  ///< Cards with the time and the chapters
        Labels  ///< Their titles alone
    };

    /// @brief What the window shows
    enum class Mode {
        Axis,  ///< The events on the axis
        List,  ///< The events in a table
        Book   ///< The chapters of the book against the time of their events
    };

    Scale scale = Scale::Dates;
    Lanes lanes = Lanes::Threads;
    Look look = Look::Cards;
    Mode mode = Mode::Axis;
    bool compress = true;  ///< Long gaps without events are short on the axis
    int zoom = 4;          ///< Step of the zoom of the axis, 0 (a year at a glance) to 6
    QJsonObject extra;     ///< Fields this version does not know, saved back

    bool operator==(const TimelineView&) const = default;
};

/// @brief A timeline of the Workshop, as its .ktl file keeps it
///
/// Its title and its description are those of its element in the book file. The file:
/// @code{.json}
/// {
///   "format": 1,
///   "dayOne": "1931-04-14",
///   "threads": [ { "id": "t1", "name": "Wyprawa", "color": "amber" } ],
///   "events": [
///     { "id": "e1", "title": "Amara znajduje mapę", "order": 1, "thread": "t1",
///       "time": { "from": "1931-04-16", "part": "evening" },
///       "characters": ["<card id>", "Ktoś"], "place": "<card id>",
///       "chapters": ["<chapter id>"], "description": "..." },
///     { "id": "e2", "title": "Selim rysuje mapę", "order": 2, "timeText": "przed wyprawą" }
///   ],
///   "view": { "scale": "dates", "lanes": "threads", "look": "cards", "mode": "axis",
///             "compress": true, "zoom": 4 }
/// }
/// @endcode
class Timeline {
public:
    static constexpr int FORMAT = 1;  ///< Version of the format of the file

    /// @brief Julian day of day one of the story: "Dzień 3" is two days after it
    [[nodiscard]] qint64 dayOne() const { return m_dayOne; }
    void setDayOne(qint64 julianDay) { m_dayOne = julianDay; }

    [[nodiscard]] const QList<TimelineThread>& threads() const { return m_threads; }
    [[nodiscard]] const QList<TimelineEvent>& events() const { return m_events; }
    [[nodiscard]] const TimelineView& view() const { return m_view; }
    void setView(const TimelineView& view) { m_view = view; }

    /// @brief Thread @p id, or nullptr
    [[nodiscard]] const TimelineThread* thread(const QString& id) const;

    /// @brief Event @p id, or nullptr
    [[nodiscard]] const TimelineEvent* event(const QString& id) const;

    /// @brief Add a thread named @p name in color @p color, after the others
    /// @return Its id
    QString addThread(const QString& name, const QString& color);

    /// @brief Put @p thread in place of the thread of its id
    /// @return False when the timeline has no thread of its id
    bool setThread(const TimelineThread& thread);

    /// @brief Take thread @p id out; its events stay, without a thread
    bool removeThread(const QString& id);

    /// @brief Add an event: a new id, and the last place in the order of the story when it
    /// has none; with @p after, the place after that event
    /// @return Its id
    QString addEvent(TimelineEvent event, const QString& after = QString());

    /// @brief Put @p event in place of the event of its id
    /// @return False when the timeline has no event of its id
    bool setEvent(const TimelineEvent& event);

    /// @brief Take event @p id out; the events after it in the order of the story move up
    bool removeEvent(const QString& id);

    /// @brief Set the time of @p event to what @p text says; text the program does not read
    /// as a time stays as the writer's text, and the event waits beside the axis
    /// @return Whether the text is a time
    bool setTimeOf(TimelineEvent& event, const QString& text) const;

    /// @brief Move event @p id to place @p order (from 1) of the order of the story
    bool moveInOrder(const QString& id, int order);

    /// @brief Put the order of the story in the order of time: events without a time keep
    /// their places after the events before them
    void orderByTime();

    /// @brief The events with a time, in the order of time (the order of the story for the
    /// same time)
    [[nodiscard]] QList<const TimelineEvent*> eventsInTime() const;

    /// @brief The events without a time, in the order of the story
    [[nodiscard]] QList<const TimelineEvent*> eventsWithoutTime() const;

    /// @brief All events in the order of the story
    [[nodiscard]] QList<const TimelineEvent*> eventsInOrder() const;

    /// @brief Forget element @p elementId of the book, which is gone: the events do not tell
    /// it in a chapter, have that character or happen in that place any more
    /// @param name The name a character or a place of the element keeps; empty: none
    /// @return Whether an event changed
    bool forgetElement(const QString& elementId, const QString& name = QString());

    /// @brief The timeline as its file keeps it
    [[nodiscard]] QJsonObject toJson() const;

    /// @brief Read the timeline from what its file keeps, in place of what it had
    /// @param error Gets what is wrong (in English, for the log)
    /// @return False, and an empty timeline, for a later format
    bool fromJson(const QJsonObject& json, QString* error = nullptr);

    /// @brief Read the timeline from its file
    /// @param error Gets what is wrong (in English, for the log)
    bool load(const QString& path, QString* error = nullptr);

    /// @brief Write the timeline to its file, replacing it in one step
    /// @param error Gets what is wrong (in English, for the log)
    bool save(const QString& path, QString* error = nullptr) const;

    bool operator==(const Timeline&) const = default;

private:
    /// @brief A new id with prefix @p prefix, unused in the timeline
    [[nodiscard]] QString newId(const QString& prefix) const;

    /// @brief Number the order of the story 1, 2, 3... in its order
    void renumber();

    TimelineEvent* findEvent(const QString& id);

    qint64 m_dayOne = 0;
    QList<TimelineThread> m_threads;
    QList<TimelineEvent> m_events;
    TimelineView m_view;
    QJsonObject m_extra;
};

/// @brief The names of the values of a view in the file, and back
struct TimelineViewNames {
    [[nodiscard]] static QString of(TimelineView::Scale scale);
    [[nodiscard]] static QString of(TimelineView::Lanes lanes);
    [[nodiscard]] static QString of(TimelineView::Look look);
    [[nodiscard]] static QString of(TimelineView::Mode mode);
    [[nodiscard]] static std::optional<TimelineView::Scale> scale(const QString& name);
    [[nodiscard]] static std::optional<TimelineView::Lanes> lanes(const QString& name);
    [[nodiscard]] static std::optional<TimelineView::Look> look(const QString& name);
    [[nodiscard]] static std::optional<TimelineView::Mode> mode(const QString& name);
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_TIMELINE_H
