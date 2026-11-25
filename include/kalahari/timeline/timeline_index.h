// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file timeline_index.h
/// @brief What a timeline knows of its book (the characters, the places, the chapters) and the
/// words its views share

#ifndef KALAHARI_TIMELINE_TIMELINE_INDEX_H
#define KALAHARI_TIMELINE_TIMELINE_INDEX_H

#include <kalahari/timeline/timeline.h>

#include <QColor>
#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::timeline {

struct TimelineLook;

/// @brief The characters, the places and the chapters of the book of a timeline
struct TimelineIndex {
    /// @brief A card or a chapter
    struct Entry {
        QString id;
        QString name;  ///< Its title
        QColor color;  ///< Of a card: its color; invalid: none
    };

    QList<Entry> characters;  ///< The cards of characters, in the order of the Workshop
    QList<Entry> places;      ///< The cards of places
    QList<Entry> chapters;    ///< The texts of the book, in the order of reading

    /// @brief The index of @p book
    [[nodiscard]] static TimelineIndex of(const core::ProjectManager& book);

    [[nodiscard]] const Entry* character(const QString& id) const;
    [[nodiscard]] const Entry* place(const QString& id) const;
    [[nodiscard]] const Entry* chapter(const QString& id) const;

    /// @brief The name of character or place @p idOrName: the title of its card, else the text
    [[nodiscard]] QString nameOf(const QString& idOrName) const;

    /// @brief The names of @p idsOrNames, joined with commas
    [[nodiscard]] QString namesOf(const QStringList& idsOrNames) const;

    /// @brief The card of a character named @p name (in any case of letters), else the name
    [[nodiscard]] QString characterOf(const QString& name) const;

    /// @brief The card of a place named @p name, else the name
    [[nodiscard]] QString placeOf(const QString& name) const;

    /// @brief The characters of a text with commas: their cards, or their names
    [[nodiscard]] QStringList charactersOf(const QString& text) const;
};

/// @brief The words of the timelines that the axis, the list and the windows share
class TimelineWords {
    Q_DECLARE_TR_FUNCTIONS(TimelineWords)

public:
    /// @brief When @p event is, as the scale of the timeline shows it
    [[nodiscard]] static QString when(const Timeline& timeline, const TimelineEvent& event,
                                      const TimelineLook& look);

    /// @brief Its time as the field "When" shows it to change it: text that reads back
    [[nodiscard]] static QString whenToEdit(const Timeline& timeline, const TimelineEvent& event,
                                            const TimelineLook& look);

    /// @brief What the program read in the field "When" of @p event: "Recognized: ..."
    /// @param ok Gets whether the time was recognized
    [[nodiscard]] static QString recognized(const Timeline& timeline, const TimelineEvent& event,
                                            const TimelineLook& look, bool* ok);

    /// @brief Set the time of @p event to @p text, as the scale reads it: in the order of the
    /// story, its label
    /// @return Whether it has a time the program reads (always in the order of the story)
    static bool setWhen(const Timeline& timeline, TimelineEvent& event, const QString& text);

    /// @brief "No time"
    [[nodiscard]] static QString noTime();

    /// @brief "New event"
    [[nodiscard]] static QString newEvent();

    /// @brief The examples of times the program reads
    [[nodiscard]] static QString examples();

    /// @brief The name of the tray of the events without a time: "Not on the axis"
    [[nodiscard]] static QString trayName();

    /// @brief "Without a thread", "Without characters"... for the lane of @p lanes
    [[nodiscard]] static QString withoutLane(TimelineView::Lanes lanes);

    /// @brief "thread", "character", "place"
    [[nodiscard]] static QString laneKind(TimelineView::Lanes lanes);

    /// @brief The name of scale @p scale: "Dates"
    [[nodiscard]] static QString scaleName(TimelineView::Scale scale);

    /// @brief The name of lanes @p lanes: "Threads"
    [[nodiscard]] static QString lanesName(TimelineView::Lanes lanes);
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_TIMELINE_INDEX_H
