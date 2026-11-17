// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline_index.cpp
/// @brief What a timeline knows of its book and the words its views share

#include <kalahari/timeline/timeline_index.h>

#include <kalahari/core/book_project.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/timeline/timeline_look.h>

#include <functional>

namespace kalahari::timeline {

namespace {

constexpr const char* CHARACTER_KIND = "character";
constexpr const char* PLACE_KIND = "location";

const TimelineIndex::Entry* find(const QList<TimelineIndex::Entry>& entries, const QString& id) {
    for (const TimelineIndex::Entry& entry : entries) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

QString idOfName(const QList<TimelineIndex::Entry>& entries, const QString& name) {
    QString wanted = name.trimmed();
    for (const TimelineIndex::Entry& entry : entries) {
        if (entry.name.compare(wanted, Qt::CaseInsensitive) == 0 || entry.id == wanted) {
            return entry.id;
        }
    }
    return wanted;
}

}  // namespace

// =============================================================================
// The index
// =============================================================================

TimelineIndex TimelineIndex::of(const core::ProjectManager& book) {
    TimelineIndex index;
    const core::BookProject* project = book.project();
    if (project == nullptr) {
        return index;
    }
    const std::function<void(const QList<core::ProjectElement>&)> addCards =
        [&index, &addCards](const QList<core::ProjectElement>& elements) {
            for (const core::ProjectElement& element : elements) {
                const QColor color(element.card.color);
                if (element.kind.kindId == QLatin1String(CHARACTER_KIND)) {
                    index.characters.append(Entry{element.id, element.title, color});
                } else if (element.kind.kindId == QLatin1String(PLACE_KIND)) {
                    index.places.append(Entry{element.id, element.title, color});
                }
                addCards(element.elements);
            }
        };
    addCards(project->workshop.elements);
    for (const core::ProjectElement* element : project->readingOrder()) {
        if (book.formOf(*element) == core::ElementForm::Text) {
            index.chapters.append(Entry{element->id, element->title, QColor()});
        }
    }
    return index;
}

const TimelineIndex::Entry* TimelineIndex::character(const QString& id) const {
    return find(characters, id);
}

const TimelineIndex::Entry* TimelineIndex::place(const QString& id) const {
    return find(places, id);
}

const TimelineIndex::Entry* TimelineIndex::chapter(const QString& id) const {
    return find(chapters, id);
}

QString TimelineIndex::nameOf(const QString& idOrName) const {
    if (const Entry* entry = character(idOrName); entry != nullptr) {
        return entry->name;
    }
    if (const Entry* entry = place(idOrName); entry != nullptr) {
        return entry->name;
    }
    return idOrName;
}

QString TimelineIndex::namesOf(const QStringList& idsOrNames) const {
    QStringList names;
    for (const QString& idOrName : idsOrNames) {
        names.append(nameOf(idOrName));
    }
    return names.join(QStringLiteral(", "));
}

QString TimelineIndex::characterOf(const QString& name) const {
    return idOfName(characters, name);
}

QString TimelineIndex::placeOf(const QString& name) const {
    return idOfName(places, name);
}

QStringList TimelineIndex::charactersOf(const QString& text) const {
    QStringList result;
    for (const QString& part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString name = part.trimmed();
        if (name.isEmpty()) {
            continue;
        }
        const QString character = characterOf(name);
        if (!result.contains(character)) {
            result.append(character);
        }
    }
    return result;
}

// =============================================================================
// The words
// =============================================================================

QString TimelineWords::when(const Timeline& timeline, const TimelineEvent& event,
                            const TimelineLook& look) {
    const TimelineView::Scale scale = timeline.view().scale;
    if (scale == TimelineView::Scale::Order) {
        return event.orderText.isEmpty() ? QStringLiteral("–") : event.orderText;
    }
    if (!event.time) {
        return event.timeText.isEmpty() ? noTime() : event.timeText;
    }
    return scale == TimelineView::Scale::Days ? event.time->dayText(timeline.dayOne(), look.style)
                                              : event.time->dateText(look.style);
}

QString TimelineWords::whenToEdit(const Timeline& timeline, const TimelineEvent& event,
                                  const TimelineLook& look) {
    const TimelineView::Scale scale = timeline.view().scale;
    if (scale == TimelineView::Scale::Order) {
        return event.orderText;
    }
    if (!event.time) {
        return event.timeText;
    }
    StoryTimeStyle style = look.style;
    style.dates = DateStyle::Words;
    return scale == TimelineView::Scale::Days ? event.time->dayText(timeline.dayOne(), style)
                                              : event.time->dateText(style);
}

QString TimelineWords::recognized(const Timeline& timeline, const TimelineEvent& event,
                                  const TimelineLook& look, bool* ok) {
    const TimelineView::Scale scale = timeline.view().scale;
    if (scale == TimelineView::Scale::Order) {
        *ok = true;
        //: The note under the field "When" in the order of the story. In Polish: %1. w
        //: kolejności opowieści; podpis to dowolny tekst.
        return tr("%1. in the order of the story; the label is any text.").arg(event.order);
    }
    if (!event.time) {
        *ok = false;
        if (event.timeText.isEmpty()) {
            //: In Polish: Bez czasu – wydarzenie czeka na pasku „%1”.
            return tr("No time – the event waits in “%1”.").arg(trayName());
        }
        //: In Polish: Nie rozpoznano – wydarzenie czeka na pasku „%1”. Przykłady: %2.
        return tr("Not recognized – the event waits in “%1”. Examples: %2.")
            .arg(trayName(), examples());
    }
    *ok = true;
    StoryTimeStyle style = look.style;
    style.dates = DateStyle::Words;
    const bool days = scale == TimelineView::Scale::Days;
    const QString main =
        days ? event.time->dayText(timeline.dayOne(), style) : event.time->dateText(style);
    const QString other = days ? event.time->dateText(style)
                          : event.time->precision == StoryTime::Precision::Day
                              ? event.time->dayText(timeline.dayOne(), style)
                              : QString();
    //: The note under the field "When". %1: the time; %2: the same time in days of the story
    //: or in dates. In Polish: Rozpoznano: %1 – %2.
    return other.isEmpty() ? tr("Recognized: %1.").arg(main)
                           : tr("Recognized: %1 – %2.").arg(main, other);
}

bool TimelineWords::setWhen(const Timeline& timeline, TimelineEvent& event, const QString& text) {
    if (timeline.view().scale == TimelineView::Scale::Order) {
        event.orderText = text.trimmed();
        return true;
    }
    if (text.trimmed().isEmpty()) {
        event.time.reset();
        event.timeText.clear();
        return false;
    }
    return timeline.setTimeOf(event, text);
}

QString TimelineWords::noTime() {
    //: An event of a timeline without a time. In Polish: bez czasu
    return tr("no time");
}

QString TimelineWords::newEvent() {
    //: The title of a new event of a timeline. In Polish: Nowe wydarzenie
    return tr("New event");
}

QString TimelineWords::examples() {
    //: Times the program reads; keep the quotes. In Polish: „14 kwietnia 1931, rano”,
    //: „marzec 1931”, „16–17 kwietnia 1931”, „Dzień 3, noc”, „Dni 5–7”, „ok. 1931”
    return tr("“14 April 1931, in the morning”, “March 1931”, “16–17 April 1931”, "
              "“Day 3, night”, “Days 5–7”, “about 1931”");
}

QString TimelineWords::trayName() {
    //: The tray of the events of a timeline without a time. In Polish: Bez miejsca na osi
    return tr("Not on the axis");
}

QString TimelineWords::withoutLane(TimelineView::Lanes lanes) {
    switch (lanes) {
        case TimelineView::Lanes::Threads:
            //: A lane of a timeline. In Polish: Bez wątku
            return tr("Without a thread");
        case TimelineView::Lanes::Characters:
            //: A lane of a timeline. In Polish: Bez postaci
            return tr("Without characters");
        case TimelineView::Lanes::Places:
            //: A lane of a timeline. In Polish: Bez miejsca
            return tr("Without a place");
        case TimelineView::Lanes::One:
            break;
    }
    //: The one lane of a timeline. In Polish: Wszystkie wydarzenia
    return tr("All events");
}

QString TimelineWords::laneKind(TimelineView::Lanes lanes) {
    switch (lanes) {
        case TimelineView::Lanes::Threads:
            //: Under the name of a lane of a timeline. In Polish: wątek
            return tr("thread");
        case TimelineView::Lanes::Characters:
            //: Under the name of a lane of a timeline. In Polish: postać
            return tr("character");
        case TimelineView::Lanes::Places:
            //: Under the name of a lane of a timeline. In Polish: miejsce
            return tr("place");
        case TimelineView::Lanes::One:
            break;
    }
    return QString();
}

QString TimelineWords::scaleName(TimelineView::Scale scale) {
    switch (scale) {
        case TimelineView::Scale::Dates:
            //: A scale of a timeline: days of the calendar. In Polish: Daty
            return tr("Dates");
        case TimelineView::Scale::Days:
            //: A scale of a timeline: days counted from day one. In Polish: Dni opowieści
            return tr("Days of the story");
        case TimelineView::Scale::Order:
            break;
    }
    //: A scale of a timeline: the order of the story. In Polish: Kolejność
    return tr("Order");
}

QString TimelineWords::lanesName(TimelineView::Lanes lanes) {
    switch (lanes) {
        case TimelineView::Lanes::Threads:
            //: The lanes of a timeline. In Polish: Wątki
            return tr("Threads");
        case TimelineView::Lanes::Characters:
            //: The lanes of a timeline. In Polish: Postacie
            return tr("Characters");
        case TimelineView::Lanes::Places:
            //: The lanes of a timeline. In Polish: Miejsca
            return tr("Places");
        case TimelineView::Lanes::One:
            break;
    }
    //: The lanes of a timeline: one for all. In Polish: Jedno pasmo
    return tr("One lane");
}

}  // namespace kalahari::timeline
