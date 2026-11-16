// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_timeline.cpp
/// @brief A timeline of the Workshop: its threads and events, the order of the story, its
/// file, and its axis with long gaps drawn short

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <kalahari/core/time_axis.h>
#include <kalahari/core/timeline.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QTemporaryDir>

using namespace kalahari::core;
using Catch::Matchers::WithinAbs;

namespace {

const qint64 DAY_ONE = CalendarDay{1931, 4, 14}.julianDay().value_or(0);

/// A timeline of three events: two with a time in the wrong order of the story, one without
Timeline expedition() {
    Timeline timeline;
    timeline.setDayOne(DAY_ONE);
    const QString journey = timeline.addThread(QStringLiteral("Wyprawa"), QStringLiteral("amber"));
    TimelineEvent storm;
    storm.title = QStringLiteral("Burza piaskowa");
    storm.thread = journey;
    timeline.setTimeOf(storm, QStringLiteral("Dni 5–7"));
    timeline.addEvent(storm);
    TimelineEvent map;
    map.title = QStringLiteral("Amara znajduje mapę");
    map.thread = journey;
    map.characters = {QStringLiteral("card-amara"), QStringLiteral("Ktoś")};
    map.place = QStringLiteral("card-cairo");
    map.chapters = {QStringLiteral("prologue")};
    timeline.setTimeOf(map, QStringLiteral("Dzień 3, wieczorem"));
    timeline.addEvent(map);
    TimelineEvent drawing;
    drawing.title = QStringLiteral("Selim rysuje mapę");
    timeline.setTimeOf(drawing, QStringLiteral("przed wyprawą"));
    timeline.addEvent(drawing);
    return timeline;
}

QStringList titles(const QList<const TimelineEvent*>& events) {
    QStringList list;
    for (const TimelineEvent* event : events) {
        list.append(event->title);
    }
    return list;
}

}  // namespace

TEST_CASE("A timeline has threads and events in the order of the story", "[timeline]") {
    Timeline timeline = expedition();
    REQUIRE(timeline.events().size() == 3);
    CHECK(titles(timeline.eventsInOrder()) ==
          QStringList{"Burza piaskowa", "Amara znajduje mapę", "Selim rysuje mapę"});
    CHECK(titles(timeline.eventsInTime()) ==
          QStringList{"Amara znajduje mapę", "Burza piaskowa"});
    CHECK(titles(timeline.eventsWithoutTime()) == QStringList{"Selim rysuje mapę"});
    const TimelineEvent* drawing = timeline.eventsWithoutTime().first();
    CHECK(drawing->timeText == QStringLiteral("przed wyprawą"));

    SECTION("A new event after another takes the place after it") {
        TimelineEvent camp;
        camp.title = QStringLiteral("Pierwszy postój");
        const QString id = timeline.addEvent(camp, timeline.eventsInOrder().first()->id);
        CHECK(timeline.event(id)->order == 2);
        CHECK(titles(timeline.eventsInOrder()).at(2) == QStringLiteral("Amara znajduje mapę"));
    }
    SECTION("An event moves in the order of the story") {
        const QString last = timeline.eventsInOrder().last()->id;
        REQUIRE(timeline.moveInOrder(last, 1));
        CHECK(titles(timeline.eventsInOrder()) ==
              QStringList{"Selim rysuje mapę", "Burza piaskowa", "Amara znajduje mapę"});
    }
    SECTION("The order of the story in the order of time") {
        timeline.orderByTime();
        // The event without a time keeps its place
        CHECK(titles(timeline.eventsInOrder()) ==
              QStringList{"Amara znajduje mapę", "Burza piaskowa", "Selim rysuje mapę"});
    }
    SECTION("A removed event leaves no hole in the order") {
        REQUIRE(timeline.removeEvent(timeline.eventsInOrder().first()->id));
        CHECK(timeline.eventsInOrder().first()->order == 1);
        CHECK(timeline.eventsInOrder().last()->order == 2);
    }
    SECTION("A removed thread leaves its events without one") {
        const QString journey = timeline.threads().first().id;
        REQUIRE(timeline.removeThread(journey));
        CHECK(timeline.threads().isEmpty());
        CHECK(timeline.eventsInOrder().first()->thread.isEmpty());
    }
    SECTION("The time of an event the program does not read waits beside the axis") {
        TimelineEvent event = *timeline.eventsInOrder().first();
        CHECK_FALSE(timeline.setTimeOf(event, QStringLiteral("rok później")));
        CHECK_FALSE(event.time.has_value());
        CHECK(event.timeText == QStringLiteral("rok później"));
        CHECK(timeline.setTimeOf(event, QStringLiteral("Dzień 1")));
        CHECK(event.timeText.isEmpty());
        CHECK(event.time->first == DAY_ONE);
    }
}

TEST_CASE("A timeline forgets an element that is gone", "[timeline]") {
    Timeline timeline = expedition();
    CHECK(timeline.forgetElement(QStringLiteral("prologue")));
    CHECK(timeline.forgetElement(QStringLiteral("card-amara"), QStringLiteral("Amara")));
    CHECK(timeline.forgetElement(QStringLiteral("card-cairo")));
    const TimelineEvent* map = timeline.eventsInTime().first();
    CHECK(map->chapters.isEmpty());
    CHECK(map->characters == QStringList{"Amara", "Ktoś"});
    CHECK(map->place.isEmpty());
    CHECK_FALSE(timeline.forgetElement(QStringLiteral("card-amara")));
}

TEST_CASE("A timeline file keeps all it has", "[timeline]") {
    Timeline timeline = expedition();
    TimelineView view;
    view.scale = TimelineView::Scale::Days;
    view.lanes = TimelineView::Lanes::Characters;
    view.look = TimelineView::Look::Labels;
    view.mode = TimelineView::Mode::Book;
    view.compress = false;
    view.zoom = 2;
    timeline.setView(view);

    const QJsonObject json = timeline.toJson();
    CHECK(json.value(QStringLiteral("format")).toInt() == Timeline::FORMAT);
    CHECK(json.value(QStringLiteral("dayOne")).toString() == QStringLiteral("1931-04-14"));
    CHECK(json.value(QStringLiteral("view")).toObject().value(QStringLiteral("lanes")) ==
          QStringLiteral("characters"));

    Timeline read;
    REQUIRE(read.fromJson(json));
    CHECK(read == timeline);

    SECTION("Fields this version does not know are saved back") {
        QJsonObject later = json;
        later.insert(QStringLiteral("calendar"), QStringLiteral("shire"));
        QJsonArray events = later.value(QStringLiteral("events")).toArray();
        QJsonObject first = events.at(0).toObject();
        first.insert(QStringLiteral("mood"), QStringLiteral("dark"));
        events.replace(0, first);
        later.insert(QStringLiteral("events"), events);
        Timeline kept;
        REQUIRE(kept.fromJson(later));
        const QJsonObject again = kept.toJson();
        CHECK(again.value(QStringLiteral("calendar")).toString() == QStringLiteral("shire"));
        CHECK(again.value(QStringLiteral("events")).toArray().at(0).toObject().value(
                  QStringLiteral("mood")) == QStringLiteral("dark"));
    }
    SECTION("A file of a later format is not read") {
        QJsonObject later = json;
        later.insert(QStringLiteral("format"), Timeline::FORMAT + 1);
        Timeline notRead;
        QString error;
        CHECK_FALSE(notRead.fromJson(later, &error));
        CHECK_FALSE(error.isEmpty());
        CHECK(notRead.events().isEmpty());
    }
    SECTION("On disk") {
        QTemporaryDir folder;
        REQUIRE(folder.isValid());
        const QString path = folder.filePath(QStringLiteral("timeline_001.ktl"));
        REQUIRE(timeline.save(path));
        Timeline loaded;
        REQUIRE(loaded.load(path));
        CHECK(loaded == timeline);
        QString error;
        CHECK_FALSE(loaded.load(folder.filePath(QStringLiteral("none.ktl")), &error));
        CHECK_FALSE(error.isEmpty());
    }
}

TEST_CASE("A time axis draws long gaps short", "[timeline]") {
    TimeAxis::Options options;
    options.pixelsPerDay = 10.0;
    options.roomBefore = 20.0;
    options.roomAfter = 40.0;
    options.shortFrom = 90.0;
    options.gapWidth = 44.0;
    // Two events a day apart, and one 100 days later
    const QList<TimeAxis::Span> spans{{100.5, 100.5}, {101.5, 101.5}, {201.5, 201.5}};

    const TimeAxis axis(spans, options, 0.0);
    REQUIRE(axis.segments().size() == 2);
    REQUIRE(axis.gaps().size() == 1);
    CHECK_THAT(axis.gaps().first().days, WithinAbs(100.0, 1e-9));
    // The first event stands after its room
    CHECK_THAT(axis.x(100.5), WithinAbs(20.0, 1e-9));
    CHECK_THAT(axis.x(101.5), WithinAbs(30.0, 1e-9));
    // After the room of the second event (4 days) comes the gap, then the room of the third
    const double gapStart = 20.0 + 10.0 + 40.0;
    CHECK_THAT(axis.gaps().first().x, WithinAbs(gapStart, 1e-9));
    CHECK_THAT(axis.x(201.5), WithinAbs(gapStart + 44.0 + 20.0, 1e-9));
    CHECK_THAT(axis.width(), WithinAbs(gapStart + 44.0 + 20.0 + 40.0, 1e-9));
    for (const double day : {100.5, 101.5, 150.0, 201.5}) {
        CHECK_THAT(axis.dayAt(axis.x(day)), WithinAbs(day, 1e-6));
    }

    SECTION("Without compressing, the gap is drawn to scale") {
        options.compress = false;
        const TimeAxis full(spans, options, 0.0);
        CHECK(full.gaps().isEmpty());
        CHECK_THAT(full.x(201.5), WithinAbs(20.0 + 1010.0, 1e-9));
    }
    SECTION("An axis without events is around its empty day") {
        const TimeAxis empty({}, options, 50.0);
        CHECK(empty.gaps().isEmpty());
        CHECK_THAT(empty.x(50.0), WithinAbs(50.0, 1e-9));
    }
    SECTION("The zoom steps") {
        CHECK(TimeAxis::zoomSteps() == 7);
        CHECK(TimeAxis::pixelsPerDayOf(-3) == TimeAxis::pixelsPerDayOf(0));
        CHECK(TimeAxis::pixelsPerDayOf(4) == 30.0);
    }
}
