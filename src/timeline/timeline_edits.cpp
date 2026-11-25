// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file timeline_edits.cpp
/// @brief The changes of a timeline that its views share

#include <kalahari/timeline/timeline_edits.h>

#include <kalahari/timeline/timeline_document.h>
#include <kalahari/timeline/timeline_index.h>
#include <kalahari/timeline/timeline_look.h>

#include <QAction>
#include <QMenu>
#include <QPainter>
#include <QPixmap>

#include <cmath>

namespace kalahari::timeline {

namespace {

constexpr int SWATCH = 12;
constexpr double DAYS_FROM = 12.0;   ///< Pixels per day from which a step is a day
constexpr double MONTHS_FROM = 2.0;  ///< From which a step is a month, else a year
constexpr int MONTHS_PER_YEAR = 12;

/// The lane of @p event in lanes @p lanes: its first
QString firstLaneOf(const TimelineEvent& event, TimelineView::Lanes lanes) {
    switch (lanes) {
        case TimelineView::Lanes::Threads:
            return event.thread;
        case TimelineView::Lanes::Characters:
            return event.characters.value(0);
        case TimelineView::Lanes::Places:
            return event.place;
        case TimelineView::Lanes::One:
            break;
    }
    return QString();
}

/// A new event in lane @p lane, with the thread of @p base (or the first thread)
TimelineEvent newEventIn(const Timeline& timeline, const QString& lane,
                         const TimelineEvent* base) {
    TimelineEvent event;
    event.title = TimelineWords::newEvent();
    event.thread = base != nullptr                 ? base->thread
                   : timeline.threads().isEmpty() ? QString()
                                                  : timeline.threads().constFirst().id;
    switch (timeline.view().lanes) {
        case TimelineView::Lanes::Threads:
            event.thread = timeline.thread(lane) != nullptr ? lane : QString();
            break;
        case TimelineView::Lanes::Characters:
            if (!lane.isEmpty()) {
                event.characters = {lane};
            }
            break;
        case TimelineView::Lanes::Places:
            event.place = lane;
            break;
        case TimelineView::Lanes::One:
            break;
    }
    return event;
}

int monthsBetween(const CalendarDay& from, const CalendarDay& to) {
    return (to.year - from.year) * MONTHS_PER_YEAR + (to.month - from.month);
}

}  // namespace

bool TimelineEdits::matches(const TimelineEvent& event, const QString& filter) {
    if (filter.isEmpty()) {
        return true;
    }
    if (filter == QLatin1String("unbooked")) {
        return event.chapters.isEmpty();
    }
    const qsizetype colon = filter.indexOf(QLatin1Char(':'));
    const QString kind = filter.left(colon);
    const QString value = filter.mid(colon + 1);
    if (kind == QLatin1String("thread")) {
        return event.thread == value;
    }
    if (kind == QLatin1String("character")) {
        return event.characters.contains(value);
    }
    if (kind == QLatin1String("place")) {
        return event.place == value;
    }
    return true;
}

QString TimelineEdits::addAfter(TimelineDocument& document, const QString& after,
                                const std::optional<QString>& lane, QString* message) {
    const Timeline& timeline = document.timeline();
    const TimelineEvent* base = timeline.event(after);
    if (base == nullptr) {
        const QList<const TimelineEvent*> inTime = timeline.eventsInTime();
        const QList<const TimelineEvent*> inOrder = timeline.eventsInOrder();
        base = !inTime.isEmpty() ? inTime.constFirst()
               : !inOrder.isEmpty() ? inOrder.constFirst()
                                    : nullptr;
    }
    const QString laneOf = lane.value_or(
        base != nullptr ? firstLaneOf(*base, timeline.view().lanes) : QString());
    TimelineEvent event = newEventIn(timeline, laneOf, base);
    if (base != nullptr && base->time) {
        const StoryTime& time = *base->time;
        const qint64 day = time.isSpan()                                 ? time.last
                           : time.precision == StoryTime::Precision::Day ? time.first
                               : static_cast<qint64>(std::floor(time.start()));
        event.time = StoryTime::day(day + 1);
    }
    const QString baseId = base != nullptr ? base->id : QString();
    QString added;
    //: What Undo undoes in a timeline. In Polish: dodanie wydarzenia
    document.change(tr("adding an event"), [&event, &baseId, &added](Timeline& changed) {
        added = changed.addEvent(event, baseId);
        return true;
    });
    if (!added.isEmpty()) {
        document.select(added, laneOf);
        //: In Polish: Dodano wydarzenie. Wpisz tytuł i naciśnij Enter; czas, postacie i
        //: rozdziały ustawisz w oknie „Wydarzenie” (Enter).
        *message = tr("Added an event. Type its title and press Enter; its time, characters "
                      "and chapters are in the window “Event” (Enter).");
    }
    return added;
}

QString TimelineEdits::addAt(TimelineDocument& document, double day, int slot,
                             const QString& lane, const TimelineLook& look, QString* message) {
    const Timeline& timeline = document.timeline();
    TimelineEvent event = newEventIn(timeline, lane, nullptr);
    const bool ordered = timeline.view().scale == TimelineView::Scale::Order;
    if (ordered) {
        event.order = slot + 1;
    } else {
        event.time = StoryTime::day(static_cast<qint64>(std::floor(day)));
        int order = 0;
        for (const TimelineEvent* other : timeline.eventsInTime()) {
            if (other->time->start() <= day) {
                order = std::max(order, other->order);
            }
        }
        event.order = order + 1;
    }
    QString added;
    document.change(tr("adding an event"), [&event, &added](Timeline& changed) {
        added = changed.addEvent(event);
        return true;
    });
    if (!added.isEmpty()) {
        document.select(added, lane);
        const TimelineEvent* made = document.timeline().event(added);
        const QString when = made != nullptr && !ordered
                                 ? TimelineWords::when(document.timeline(), *made, look)
                                 : QString();
        //: %1: when it is. In Polish: Dodano wydarzenie – %1. Wpisz tytuł i naciśnij Enter;
        //: resztę pól ma okno „Wydarzenie” (Enter).
        *message = when.isEmpty()
                       ? tr("Added an event. Type its title and press Enter; the window "
                            "“Event” (Enter) has the other fields.")
                       : tr("Added an event – %1. Type its title and press Enter; the window "
                            "“Event” (Enter) has the other fields.")
                             .arg(when);
    }
    return added;
}

bool TimelineEdits::remove(TimelineDocument& document, const QString& id, QString* message) {
    const Timeline& timeline = document.timeline();
    const TimelineEvent* event = timeline.event(id);
    if (event == nullptr) {
        return false;
    }
    const QString title = event->title;
    QList<const TimelineEvent*> sequence = timeline.view().scale == TimelineView::Scale::Order
                                               ? timeline.eventsInOrder()
                                               : timeline.eventsInTime();
    sequence.append(timeline.eventsWithoutTime());
    const qsizetype at = sequence.indexOf(event);
    const TimelineEvent* next = at + 1 < sequence.size() ? sequence.at(at + 1)
                                : at > 0                 ? sequence.at(at - 1)
                                                         : nullptr;
    const QString nextId = next != nullptr ? next->id : QString();
    //: What Undo undoes in a timeline. In Polish: usunięcie „%1”
    const bool done = document.change(tr("deleting %1").arg(quoted(title)),
                                      [&id](Timeline& changed) { return changed.removeEvent(id); });
    if (done) {
        document.select(nextId);
        //: In Polish: Usunięto wydarzenie „%1”.
        *message = tr("Deleted the event %1.").arg(quoted(title));
    }
    return done;
}

QString TimelineEdits::stepOf(const Timeline& timeline, double pixelsPerDay) {
    if (timeline.view().scale == TimelineView::Scale::Order) {
        //: Ctrl+Left and Ctrl+Right move an event by... In Polish: jedno miejsce
        return tr("a place");
    }
    if (timeline.view().scale == TimelineView::Scale::Days || pixelsPerDay >= DAYS_FROM) {
        //: In Polish: dzień
        return tr("a day");
    }
    //: In Polish: miesiąc / rok
    return pixelsPerDay >= MONTHS_FROM ? tr("a month") : tr("a year");
}

bool TimelineEdits::shift(TimelineDocument& document, const QString& id, int direction,
                          double pixelsPerDay, const TimelineLook& look, QString* message) {
    const Timeline& timeline = document.timeline();
    const TimelineEvent* event = timeline.event(id);
    if (event == nullptr) {
        return false;
    }
    const QString title = event->title;
    //: What Undo undoes in a timeline. In Polish: przesunięcie „%1”
    const QString what = tr("moving %1").arg(quoted(title));
    if (timeline.view().scale == TimelineView::Scale::Order) {
        const QList<const TimelineEvent*> inOrder = timeline.eventsInOrder();
        const qsizetype to = inOrder.indexOf(event) + direction;
        if (to < 0 || to >= inOrder.size()) {
            //: In Polish: To już pierwsze wydarzenie. / To już ostatnie wydarzenie.
            *message =
                direction < 0 ? tr("This is the first event.") : tr("This is the last event.");
            return false;
        }
        const bool done = document.change(what, [&id, to](Timeline& changed) {
            return changed.moveInOrder(id, static_cast<int>(to) + 1);
        });
        if (done) {
            //: In Polish: „%1” jest teraz %2. w kolejności.
            *message = tr("%1 is now %2. in the order.").arg(quoted(title)).arg(to + 1);
        }
        return done;
    }
    if (!event->time) {
        //: In Polish: To wydarzenie nie ma jeszcze czasu. Wpisz go w oknie „Wydarzenie”
        //: (Enter) albo przeciągnij wydarzenie na oś.
        *message = tr("This event has no time yet. Give it one in the window “Event” (Enter), "
                      "or drag the event onto the axis.");
        return false;
    }
    // A step of months: those of its time when it is a month or a year, else those the zoom
    // shows; 0: a day
    const StoryTime& time = *event->time;
    const bool byDays =
        pixelsPerDay >= DAYS_FROM || timeline.view().scale == TimelineView::Scale::Days;
    int months = 0;
    if (time.precision == StoryTime::Precision::Year) {
        months = MONTHS_PER_YEAR;
    } else if (time.precision == StoryTime::Precision::Month) {
        months = 1;
    } else if (!byDays) {
        months = pixelsPerDay >= MONTHS_FROM ? 1 : MONTHS_PER_YEAR;
    }
    const StoryTime moved =
        months == 0 ? time.movedByDays(direction) : time.movedByMonths(months * direction);
    const bool done =
        document.changeEvent(id, what, [&moved](TimelineEvent& changed) { changed.time = moved; });
    if (done) {
        const TimelineEvent* now = document.timeline().event(id);
        //: In Polish: Przesunięto „%1”: %2.
        const QString when =
            now != nullptr ? TimelineWords::when(document.timeline(), *now, look) : QString();
        *message = tr("Moved %1: %2.").arg(quoted(title), when);
    }
    return done;
}

bool TimelineEdits::setLane(TimelineEvent& event, TimelineView::Lanes lanes, const QString& from,
                            const QString& to) {
    switch (lanes) {
        case TimelineView::Lanes::Threads:
            event.thread = to;
            return true;
        case TimelineView::Lanes::Characters:
            if (!to.isEmpty() && to != from && event.characters.contains(to)) {
                return false;
            }
            if (!from.isEmpty()) {
                event.characters.removeAll(from);
            }
            if (!to.isEmpty() && !event.characters.contains(to)) {
                event.characters.append(to);
            }
            return true;
        case TimelineView::Lanes::Places:
            event.place = to;
            return true;
        case TimelineView::Lanes::One:
            break;
    }
    return true;
}

bool TimelineEdits::moveToLane(TimelineDocument& document, const QString& id,
                               const QString& from, const QString& to, const QString& name,
                               QString* message) {
    const Timeline& timeline = document.timeline();
    const TimelineEvent* event = timeline.event(id);
    if (event == nullptr || from == to) {
        return false;
    }
    const TimelineView::Lanes lanes = timeline.view().lanes;
    if (lanes == TimelineView::Lanes::One) {
        //: In Polish: Przy jednym paśmie wydarzenie nie ma dokąd przejść. Wybierz pasma
        //: według wątków, postaci albo miejsc.
        *message = tr("With one lane an event has nowhere to go. Choose lanes of threads, "
                      "characters or places.");
        return false;
    }
    TimelineEvent changed = *event;
    if (!setLane(changed, lanes, from, to)) {
        //: In Polish: %1 już jest w tym wydarzeniu.
        *message = tr("%1 is already in this event.").arg(name);
        return false;
    }
    const QString title = event->title;
    //: What Undo undoes in a timeline. In Polish: przeniesienie „%1” do innego pasma
    const bool done = document.change(tr("moving %1 to another lane").arg(quoted(title)),
                                      [&changed](Timeline& timelineOf) {
                                          return timelineOf.setEvent(changed);
                                      });
    if (done) {
        document.select(id, to);
        //: In Polish: „%1” – teraz w paśmie „%2”.
        *message = tr("%1 is now in the lane %2.").arg(quoted(title), quoted(name));
    }
    return done;
}

StoryTime TimelineEdits::movedTo(const TimelineEvent& event, double day, double pixelsPerDay,
                                 TimelineView::Scale scale) {
    const auto target = static_cast<qint64>(std::floor(day));
    if (!event.time) {
        return StoryTime::day(target);
    }
    const StoryTime& time = *event.time;
    const CalendarDay from = CalendarDay::of(time.first);
    const CalendarDay to = CalendarDay::of(target);
    if (time.precision == StoryTime::Precision::Year) {
        return time.movedByMonths((to.year - from.year) * MONTHS_PER_YEAR);
    }
    if (time.precision == StoryTime::Precision::Month ||
        (scale == TimelineView::Scale::Dates && pixelsPerDay < MONTHS_FROM)) {
        return time.movedByMonths(monthsBetween(from, to));
    }
    return time.movedByDays(target - time.first);
}

void TimelineEdits::addColors(QMenu& menu, TimelineDocument& document, const TimelineLook& look,
                              const QString& id) {
    const TimelineEvent* event = document.timeline().event(id);
    //: In the menu of an event of a timeline. In Polish: Kolor
    QMenu* colors = menu.addMenu(tr("Color"));
    //: The color of an event: that of its thread. In Polish: Kolor wątku
    QAction* thread = colors->addAction(tr("The color of its thread"));
    thread->setCheckable(true);
    thread->setChecked(event != nullptr && event->color.isEmpty());
    //: What Undo undoes in a timeline. In Polish: zmiana koloru wydarzenia
    const QString what = tr("changing the color of an event");
    QObject::connect(thread, &QAction::triggered, &document, [&document, id, what]() {
        document.changeEvent(id, what, [](TimelineEvent& changed) { changed.color.clear(); });
    });
    colors->addSeparator();
    for (std::size_t i = 0; i < look.palette.size(); ++i) {
        const QString key = QString::number(i + 1);
        //: A color of the palette of the threads. In Polish: Kolor %1
        QAction* action =
            colors->addAction(swatch(look.palette.at(i), look.mid), tr("Color %1").arg(key));
        action->setCheckable(true);
        action->setChecked(event != nullptr && event->color == key);
        QObject::connect(action, &QAction::triggered, &document, [&document, id, key, what]() {
            document.changeEvent(id, what, [&key](TimelineEvent& changed) { changed.color = key; });
        });
    }
}

QIcon TimelineEdits::swatch(const QColor& color, const QColor& frame) {
    QPixmap pixmap(SWATCH, SWATCH);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(frame);
    painter.setBrush(color);
    painter.drawEllipse(QRectF(0.5, 0.5, SWATCH - 1.0, SWATCH - 1.0));
    painter.end();
    return QIcon(pixmap);
}

QString TimelineEdits::quoted(const QString& title) {
    //: A title in quotes. In Polish: „%1”
    return tr("“%1”").arg(title);
}

}  // namespace kalahari::timeline
