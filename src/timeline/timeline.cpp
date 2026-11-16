// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file timeline.cpp
/// @brief A timeline of the Workshop: its threads and events, their order, and its file

#include <kalahari/timeline/timeline.h>

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace kalahari::timeline {

namespace {

// Keys of the file
const QString ID = QStringLiteral("id");
const QString NAME = QStringLiteral("name");
const QString COLOR = QStringLiteral("color");
const QString TITLE = QStringLiteral("title");
const QString TIME = QStringLiteral("time");
const QString TIME_TEXT = QStringLiteral("timeText");
const QString ORDER_TEXT = QStringLiteral("orderText");
const QString ORDER = QStringLiteral("order");
const QString THREAD = QStringLiteral("thread");
const QString CHARACTERS = QStringLiteral("characters");
const QString PLACE = QStringLiteral("place");
const QString CHAPTERS = QStringLiteral("chapters");
const QString DESCRIPTION = QStringLiteral("description");
const QString FORMAT_KEY = QStringLiteral("format");
const QString DAY_ONE = QStringLiteral("dayOne");
const QString THREADS = QStringLiteral("threads");
const QString EVENTS = QStringLiteral("events");
const QString VIEW = QStringLiteral("view");
const QString SCALE = QStringLiteral("scale");
const QString LANES = QStringLiteral("lanes");
const QString LOOK = QStringLiteral("look");
const QString MODE = QStringLiteral("mode");
const QString COMPRESS = QStringLiteral("compress");
const QString ZOOM = QStringLiteral("zoom");

constexpr int MAX_ZOOM = 6;

/// The names of the values of an enum in the file, in the order of the enum
template <typename Enum, std::size_t N>
QString nameOf(Enum value, const std::array<const char*, N>& names) {
    return QString::fromLatin1(names.at(static_cast<std::size_t>(value)));
}

template <typename Enum, std::size_t N>
std::optional<Enum> valueOf(const QString& name, const std::array<const char*, N>& names) {
    for (std::size_t i = 0; i < N; ++i) {
        if (name == QLatin1String(names.at(i))) {
            return static_cast<Enum>(i);
        }
    }
    return std::nullopt;
}

constexpr std::array<const char*, 3> SCALE_NAMES{"dates", "days", "order"};
constexpr std::array<const char*, 4> LANE_NAMES{"threads", "characters", "places", "one"};
constexpr std::array<const char*, 2> LOOK_NAMES{"cards", "labels"};
constexpr std::array<const char*, 3> MODE_NAMES{"axis", "list", "book"};

/// The fields of @p json that the object of this version has not
QJsonObject unknownOf(QJsonObject json, std::initializer_list<QString> known) {
    for (const QString& key : known) {
        json.remove(key);
    }
    return json;
}

/// Put @p value in @p json as @p key unless it is empty
void putText(QJsonObject& json, const QString& key, const QString& value) {
    if (!value.isEmpty()) {
        json.insert(key, value);
    }
}

void putList(QJsonObject& json, const QString& key, const QStringList& values) {
    if (!values.isEmpty()) {
        json.insert(key, QJsonArray::fromStringList(values));
    }
}

QStringList listOf(const QJsonValue& value) {
    QStringList list;
    for (const auto& item : value.toArray()) {
        if (item.isString() && !item.toString().isEmpty()) {
            list.append(item.toString());
        }
    }
    return list;
}

QJsonObject threadToJson(const TimelineThread& thread) {
    QJsonObject json = thread.extra;
    json.insert(ID, thread.id);
    json.insert(NAME, thread.name);
    putText(json, COLOR, thread.color);
    return json;
}

TimelineThread threadFromJson(const QJsonObject& json) {
    TimelineThread thread;
    thread.id = json.value(ID).toString();
    thread.name = json.value(NAME).toString();
    thread.color = json.value(COLOR).toString();
    thread.extra = unknownOf(json, {ID, NAME, COLOR});
    return thread;
}

QJsonObject eventToJson(const TimelineEvent& event) {
    QJsonObject json = event.extra;
    json.insert(ID, event.id);
    json.insert(TITLE, event.title);
    json.insert(ORDER, event.order);
    if (event.time) {
        json.insert(TIME, event.time->toJson());
    } else {
        putText(json, TIME_TEXT, event.timeText);
    }
    putText(json, ORDER_TEXT, event.orderText);
    putText(json, THREAD, event.thread);
    putList(json, CHARACTERS, event.characters);
    putText(json, PLACE, event.place);
    putList(json, CHAPTERS, event.chapters);
    putText(json, DESCRIPTION, event.description);
    putText(json, COLOR, event.color);
    return json;
}

TimelineEvent eventFromJson(const QJsonObject& json) {
    TimelineEvent event;
    event.id = json.value(ID).toString();
    event.title = json.value(TITLE).toString();
    event.order = json.value(ORDER).toInt();
    event.time = StoryTime::fromJson(json.value(TIME).toObject());
    // A time this version cannot read waits beside the axis, and is kept
    if (!event.time && json.value(TIME).isObject()) {
        event.extra.insert(TIME, json.value(TIME));
    }
    event.timeText = event.time ? QString() : json.value(TIME_TEXT).toString();
    event.orderText = json.value(ORDER_TEXT).toString();
    event.thread = json.value(THREAD).toString();
    event.characters = listOf(json.value(CHARACTERS));
    event.place = json.value(PLACE).toString();
    event.chapters = listOf(json.value(CHAPTERS));
    event.description = json.value(DESCRIPTION).toString();
    event.color = json.value(COLOR).toString();
    const QJsonObject unknown =
        unknownOf(json, {ID, TITLE, ORDER, TIME, TIME_TEXT, ORDER_TEXT, THREAD, CHARACTERS,
                         PLACE, CHAPTERS, DESCRIPTION, COLOR});
    for (auto it = unknown.begin(); it != unknown.end(); ++it) {
        event.extra.insert(it.key(), it.value());
    }
    return event;
}

QJsonObject viewToJson(const TimelineView& view) {
    QJsonObject json = view.extra;
    json.insert(SCALE, TimelineViewNames::of(view.scale));
    json.insert(LANES, TimelineViewNames::of(view.lanes));
    json.insert(LOOK, TimelineViewNames::of(view.look));
    json.insert(MODE, TimelineViewNames::of(view.mode));
    json.insert(COMPRESS, view.compress);
    json.insert(ZOOM, view.zoom);
    return json;
}

TimelineView viewFromJson(const QJsonObject& json) {
    TimelineView view;
    view.scale = TimelineViewNames::scale(json.value(SCALE).toString()).value_or(view.scale);
    view.lanes = TimelineViewNames::lanes(json.value(LANES).toString()).value_or(view.lanes);
    view.look = TimelineViewNames::look(json.value(LOOK).toString()).value_or(view.look);
    view.mode = TimelineViewNames::mode(json.value(MODE).toString()).value_or(view.mode);
    view.compress = json.value(COMPRESS).toBool(view.compress);
    view.zoom = std::clamp(json.value(ZOOM).toInt(view.zoom), 0, MAX_ZOOM);
    view.extra = unknownOf(json, {SCALE, LANES, LOOK, MODE, COMPRESS, ZOOM});
    return view;
}

void setError(QString* error, const QString& text) {
    if (error != nullptr) {
        *error = text;
    }
}

/// Whether event @p a comes before @p b in time; the same time keeps the order of the story
bool earlier(const TimelineEvent* a, const TimelineEvent* b) {
    const double startA = a->time ? a->time->start() : 0.0;
    const double startB = b->time ? b->time->start() : 0.0;
    if (startA != startB) {
        return startA < startB;
    }
    return a->order < b->order;
}

}  // namespace

// =============================================================================
// Threads and events
// =============================================================================

const TimelineThread* Timeline::thread(const QString& id) const {
    const auto it = std::find_if(m_threads.cbegin(), m_threads.cend(),
                                 [&id](const TimelineThread& thread) { return thread.id == id; });
    return it != m_threads.cend() ? &*it : nullptr;
}

const TimelineEvent* Timeline::event(const QString& id) const {
    const auto it = std::find_if(m_events.cbegin(), m_events.cend(),
                                 [&id](const TimelineEvent& event) { return event.id == id; });
    return it != m_events.cend() ? &*it : nullptr;
}

TimelineEvent* Timeline::findEvent(const QString& id) {
    const auto it = std::find_if(m_events.begin(), m_events.end(),
                                 [&id](const TimelineEvent& event) { return event.id == id; });
    return it != m_events.end() ? &*it : nullptr;
}

QString Timeline::newId(const QString& prefix) const {
    for (int number = 1;; ++number) {
        const QString id = prefix + QString::number(number);
        if (thread(id) == nullptr && event(id) == nullptr) {
            return id;
        }
    }
}

QString Timeline::addThread(const QString& name, const QString& color) {
    TimelineThread thread;
    thread.id = newId(QStringLiteral("t"));
    thread.name = name;
    thread.color = color;
    m_threads.append(thread);
    return thread.id;
}

bool Timeline::setThread(const TimelineThread& thread) {
    for (TimelineThread& own : m_threads) {
        if (own.id == thread.id) {
            own = thread;
            return true;
        }
    }
    return false;
}

bool Timeline::removeThread(const QString& id) {
    const qsizetype removed = m_threads.removeIf(
        [&id](const TimelineThread& thread) { return thread.id == id; });
    for (TimelineEvent& event : m_events) {
        if (event.thread == id) {
            event.thread.clear();
        }
    }
    return removed > 0;
}

QString Timeline::addEvent(TimelineEvent event, const QString& after) {
    event.id = newId(QStringLiteral("e"));
    const TimelineEvent* before = after.isEmpty() ? nullptr : this->event(after);
    if (before != nullptr) {
        event.order = before->order + 1;
    } else if (event.order <= 0) {
        event.order = static_cast<int>(m_events.size()) + 1;
    }
    // The events from its place on move down
    for (TimelineEvent& other : m_events) {
        if (other.order >= event.order) {
            ++other.order;
        }
    }
    m_events.append(event);
    renumber();
    return event.id;
}

bool Timeline::setEvent(const TimelineEvent& event) {
    TimelineEvent* own = findEvent(event.id);
    if (own == nullptr) {
        return false;
    }
    const int order = own->order;
    *own = event;
    if (event.order != order) {
        own->order = order;
        moveInOrder(event.id, event.order);
    }
    return true;
}

bool Timeline::removeEvent(const QString& id) {
    const qsizetype removed =
        m_events.removeIf([&id](const TimelineEvent& event) { return event.id == id; });
    renumber();
    return removed > 0;
}

bool Timeline::setTimeOf(TimelineEvent& event, const QString& text) const {
    event.time = StoryTime::parse(text, m_dayOne);
    event.timeText = event.time ? QString() : text.trimmed();
    return event.time.has_value();
}

bool Timeline::moveInOrder(const QString& id, int order) {
    TimelineEvent* moved = findEvent(id);
    if (moved == nullptr) {
        return false;
    }
    const int to = std::clamp(order, 1, static_cast<int>(m_events.size()));
    const int from = moved->order;
    for (TimelineEvent& event : m_events) {
        if (from < to && event.order > from && event.order <= to) {
            --event.order;
        } else if (to < from && event.order >= to && event.order < from) {
            ++event.order;
        }
    }
    moved->order = to;
    renumber();
    return true;
}

void Timeline::orderByTime() {
    QList<const TimelineEvent*> inOrder = eventsInOrder();
    QList<const TimelineEvent*> timed = eventsInTime();
    // The events with a time take the places of the events with a time, in the order of time
    qsizetype next = 0;
    for (const TimelineEvent*& event : inOrder) {
        if (event->time) {
            event = timed.at(next++);
        }
    }
    QHash<QString, int> places;
    for (qsizetype i = 0; i < inOrder.size(); ++i) {
        places.insert(inOrder.at(i)->id, static_cast<int>(i) + 1);
    }
    for (TimelineEvent& event : m_events) {
        event.order = places.value(event.id);
    }
}

void Timeline::renumber() {
    QList<TimelineEvent*> events;
    for (TimelineEvent& event : m_events) {
        events.append(&event);
    }
    std::stable_sort(events.begin(), events.end(),
                     [](const TimelineEvent* a, const TimelineEvent* b) {
                         return a->order < b->order;
                     });
    int order = 0;
    for (TimelineEvent* event : events) {
        event->order = ++order;
    }
}

QList<const TimelineEvent*> Timeline::eventsInOrder() const {
    QList<const TimelineEvent*> events;
    for (const TimelineEvent& event : m_events) {
        events.append(&event);
    }
    std::stable_sort(events.begin(), events.end(),
                     [](const TimelineEvent* a, const TimelineEvent* b) {
                         return a->order < b->order;
                     });
    return events;
}

QList<const TimelineEvent*> Timeline::eventsInTime() const {
    QList<const TimelineEvent*> events;
    for (const TimelineEvent& event : m_events) {
        if (event.time) {
            events.append(&event);
        }
    }
    std::stable_sort(events.begin(), events.end(), earlier);
    return events;
}

QList<const TimelineEvent*> Timeline::eventsWithoutTime() const {
    QList<const TimelineEvent*> events = eventsInOrder();
    events.removeIf([](const TimelineEvent* event) { return event->time.has_value(); });
    return events;
}

bool Timeline::forgetElement(const QString& elementId, const QString& name) {
    bool changed = false;
    for (TimelineEvent& event : m_events) {
        const qsizetype chapters = event.chapters.removeAll(elementId);
        const qsizetype at = event.characters.indexOf(elementId);
        if (at >= 0) {
            // A character keeps its name when the writer asks for it
            if (name.isEmpty() || event.characters.contains(name)) {
                event.characters.removeAt(at);
            } else {
                event.characters[at] = name;
            }
        }
        const bool place = event.place == elementId;
        if (place) {
            event.place = name;
        }
        changed = changed || chapters > 0 || at >= 0 || place;
    }
    return changed;
}

// =============================================================================
// File
// =============================================================================

QJsonObject Timeline::toJson() const {
    QJsonArray threads;
    for (const TimelineThread& thread : m_threads) {
        threads.append(threadToJson(thread));
    }
    QJsonArray events;
    for (const TimelineEvent* event : eventsInOrder()) {
        events.append(eventToJson(*event));
    }
    QJsonObject json = m_extra;
    json.insert(FORMAT_KEY, FORMAT);
    json.insert(DAY_ONE, StoryTime::day(m_dayOne).toJson().value(QStringLiteral("from")));
    json.insert(THREADS, threads);
    json.insert(EVENTS, events);
    json.insert(VIEW, viewToJson(m_view));
    return json;
}

bool Timeline::fromJson(const QJsonObject& json, QString* error) {
    *this = Timeline();
    const int format = json.value(FORMAT_KEY).toInt(FORMAT);
    if (format > FORMAT) {
        setError(error, QStringLiteral("the timeline has format %1, this version of Kalahari "
                                       "reads format %2")
                            .arg(format)
                            .arg(FORMAT));
        return false;
    }
    const std::optional<StoryTime> dayOne =
        StoryTime::fromJson(QJsonObject{{QStringLiteral("from"), json.value(DAY_ONE)}});
    if (dayOne) {
        m_dayOne = dayOne->first;
    }
    for (const auto& value : json.value(THREADS).toArray()) {
        TimelineThread thread = threadFromJson(value.toObject());
        if (!thread.id.isEmpty() && this->thread(thread.id) == nullptr) {
            m_threads.append(thread);
        }
    }
    for (const auto& value : json.value(EVENTS).toArray()) {
        TimelineEvent event = eventFromJson(value.toObject());
        if (event.id.isEmpty() || this->event(event.id) != nullptr) {
            event.id = newId(QStringLiteral("e"));
        }
        if (event.order <= 0) {
            event.order = static_cast<int>(m_events.size()) + 1;
        }
        m_events.append(event);
    }
    renumber();
    m_view = viewFromJson(json.value(VIEW).toObject());
    m_extra = unknownOf(json, {FORMAT_KEY, DAY_ONE, THREADS, EVENTS, VIEW});
    return true;
}

bool Timeline::load(const QString& path, QString* error) {
    *this = Timeline();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("%1: %2").arg(path, file.errorString()));
        return false;
    }
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) {
        setError(error, QStringLiteral("%1: %2").arg(path, parseError.errorString()));
        return false;
    }
    return fromJson(document.object(), error);
}

bool Timeline::save(const QString& path, QString* error) const {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("%1: %2").arg(path, file.errorString()));
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setError(error, QStringLiteral("%1: %2").arg(path, file.errorString()));
        return false;
    }
    return true;
}

// =============================================================================
// Names of the view
// =============================================================================

QString TimelineViewNames::of(TimelineView::Scale scale) {
    return nameOf(scale, SCALE_NAMES);
}

QString TimelineViewNames::of(TimelineView::Lanes lanes) {
    return nameOf(lanes, LANE_NAMES);
}

QString TimelineViewNames::of(TimelineView::Look look) {
    return nameOf(look, LOOK_NAMES);
}

QString TimelineViewNames::of(TimelineView::Mode mode) {
    return nameOf(mode, MODE_NAMES);
}

std::optional<TimelineView::Scale> TimelineViewNames::scale(const QString& name) {
    return valueOf<TimelineView::Scale>(name, SCALE_NAMES);
}

std::optional<TimelineView::Lanes> TimelineViewNames::lanes(const QString& name) {
    return valueOf<TimelineView::Lanes>(name, LANE_NAMES);
}

std::optional<TimelineView::Look> TimelineViewNames::look(const QString& name) {
    return valueOf<TimelineView::Look>(name, LOOK_NAMES);
}

std::optional<TimelineView::Mode> TimelineViewNames::mode(const QString& name) {
    return valueOf<TimelineView::Mode>(name, MODE_NAMES);
}

}  // namespace kalahari::timeline
