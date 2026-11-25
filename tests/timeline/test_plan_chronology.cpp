// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_plan_chronology.cpp
/// @brief The chronology of the Book Plan without its painting: the columns of the times, the
/// gaps, the bands, the flashbacks, the periods of the writer and a time a step later

#include <catch2/catch_test_macros.hpp>
#include <kalahari/timeline/plan_chronology_layout.h>
#include <kalahari/timeline/plan_chronology_view.h>
#include <kalahari/timeline/story_time.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

#include <optional>
#include <string>

using namespace kalahari::timeline;

namespace {

const qint64 DAY_ONE = CalendarDay{1931, 4, 14}.julianDay().value_or(0);

/// A card of the book @p id with time @p time and point of view @p pov
ChronologyItem cardOf(const QString& id, const QString& time, const QString& pov = QString(),
                      const QList<PlanPeriod>& periods = {}) {
    ChronologyItem item;
    item.id = id;
    item.title = id;
    item.pov = pov;
    item.timeText = time;
    item.time = ChronologyLayout::timeOf(time, DAY_ONE, periods);
    return item;
}

ChronologyOptions optionsOf(TimelineView::Scale scale = TimelineView::Scale::Days,
                            bool compress = true) {
    ChronologyOptions options;
    options.scale = scale;
    options.lanes = TimelineView::Lanes::Characters;
    options.compress = compress;
    options.dayOne = DAY_ONE;
    return options;
}

/// The labels of the columns of @p layout, a gap in brackets: "Day 1, [≈ 4 weeks], Day 30"
std::string columnsOf(const ChronologyLayout& layout) {
    QStringList labels;
    for (const ChronologyColumn& column : layout.columns()) {
        labels << (column.gap ? QStringLiteral("[%1]").arg(column.label) : column.label);
    }
    return labels.join(QStringLiteral(", ")).toStdString();
}

/// The cards marked as flashbacks, by id
QStringList flashbacksOf(const ChronologyLayout& layout) {
    QStringList ids;
    for (const ChronologyItem& item : layout.items()) {
        if (item.flashback) {
            ids << item.id;
        }
    }
    return ids;
}

/// @p text a step later (@p steps > 0) or earlier; "-" for no time
std::string shifted(const QString& text, int steps, const QList<PlanPeriod>& periods = {}) {
    return PlanChronologyView::shiftedTime(text, steps, DAY_ONE, periods, StoryTimeStyle{})
        .value_or(QStringLiteral("-"))
        .toStdString();
}

}  // namespace

TEST_CASE("Book plan chronology: the times in columns, the gaps between them and the bands",
          "[timeline][chronology]") {
    const QList<ChronologyItem> items{
        cardOf(QStringLiteral("1"), QStringLiteral("Day 1"), QStringLiteral("Amara")),
        cardOf(QStringLiteral("2"), QStringLiteral("Day 3"), QStringLiteral("Selim")),
        cardOf(QStringLiteral("3"), QStringLiteral("Day 30"), QStringLiteral("amara")),
        cardOf(QStringLiteral("4"), QString(), QStringLiteral("Amara")),
        cardOf(QStringLiteral("5"), QStringLiteral("Day 2, evening")),
    };

    SECTION("gaps without cards are one narrow column that says how long they are") {
        const ChronologyLayout layout(items, optionsOf());
        CHECK(columnsOf(layout) == "Day 1, Day 2, Day 3, [≈ 4 weeks], Day 30");
        // The bands of the points of view in the order the book names them, a name once
        // whatever its letters; the cards without one in the last band
        REQUIRE(layout.lanes().size() == 3);
        CHECK(layout.lanes().at(0).name == QStringLiteral("Amara"));
        CHECK(layout.lanes().at(1).name == QStringLiteral("Selim"));
        CHECK(layout.lanes().at(2).key.isEmpty());
        CHECK(layout.laneOf(2) == 0);
        CHECK(layout.laneOf(4) == 2);
        CHECK(layout.columnOf(2) == 4);
        CHECK(layout.cell(0, 4) == QList<int>{2});
        // A card without a time is under the others
        CHECK(layout.untimed() == QList<int>{3});
        CHECK(layout.laneOf(3) == -1);
        CHECK(layout.columnOf(3) == -1);
        CHECK(layout.indexOf(QStringLiteral("5")) == 4);
        CHECK(layout.indexOf(QStringLiteral("none")) == -1);
    }
    SECTION("without Shorten Gaps the days of a short gap are empty columns") {
        const ChronologyLayout layout(items, optionsOf(TimelineView::Scale::Days, false));
        CHECK(layout.columns().size() == 30);
        CHECK(layout.columns().at(3).label == QStringLiteral("Day 4"));
        CHECK_FALSE(layout.columns().at(3).gap);
        CHECK(layout.columnOf(2) == 29);
    }
    SECTION("a gap longer than a month stays one column") {
        const ChronologyLayout layout(
            {cardOf(QStringLiteral("1"), QStringLiteral("Day 1")),
             cardOf(QStringLiteral("2"), QStringLiteral("Day 100"))},
            optionsOf(TimelineView::Scale::Days, false));
        CHECK(columnsOf(layout) == "Day 1, [≈ 3 months], Day 100");
    }
    SECTION("the dates say the days of the calendar") {
        const ChronologyLayout layout(items, optionsOf(TimelineView::Scale::Dates));
        CHECK(layout.columns().at(0).label == QStringLiteral("14 April 1931"));
    }
    SECTION("the order of the story has no gaps and says the times as the cards write them") {
        const ChronologyLayout layout(items, optionsOf(TimelineView::Scale::Order));
        CHECK(columnsOf(layout) == "Day 1, Day 2, evening, Day 3, Day 30");
    }
    SECTION("one band, or the bands of the places") {
        ChronologyOptions options = optionsOf();
        options.lanes = TimelineView::Lanes::One;
        const ChronologyLayout one(items, options);
        REQUIRE(one.lanes().size() == 1);
        CHECK(one.laneOf(4) == 0);
        options.lanes = TimelineView::Lanes::Places;
        const ChronologyLayout places(items, options);
        REQUIRE(places.lanes().size() == 1);
        CHECK(places.lanes().at(0).key.isEmpty());
    }
}

TEST_CASE("Book plan chronology: seasons, halves of a year and years are columns of their days",
          "[timeline][chronology]") {
    const ChronologyLayout layout(
        {cardOf(QStringLiteral("1"), QStringLiteral("spring 1931")),
         cardOf(QStringLiteral("2"), QStringLiteral("winter 1931")),
         cardOf(QStringLiteral("3"), QStringLiteral("second half of 1931")),
         cardOf(QStringLiteral("4"), QStringLiteral("1933"))},
        optionsOf());
    // Winter 1931 is December 1930 to February 1931
    CHECK(columnsOf(layout) ==
          "Winter 1931, Spring 1931, [≈ 4 weeks], Second half of 1931, [≈ 12 months], 1933");
    const auto [from, to] = ChronologyLayout::daysOf(*layout.items().at(0).time);
    CHECK(from == CalendarDay{1931, 3, 1}.julianDay().value_or(0));
    CHECK(to == CalendarDay{1931, 5, 31}.julianDay().value_or(0));
}

TEST_CASE("Book plan chronology: a card told after a card that happens later is a flashback",
          "[timeline][chronology]") {
    QList<ChronologyItem> items{
        cardOf(QStringLiteral("a"), QStringLiteral("Day 5")),
        cardOf(QStringLiteral("b"), QStringLiteral("Day 2")),
        cardOf(QStringLiteral("c"), QStringLiteral("Day 6")),
        cardOf(QStringLiteral("d"), QStringLiteral("Day 6, morning")),
        cardOf(QStringLiteral("e"), QStringLiteral("Day 6, evening")),
        cardOf(QStringLiteral("f"), QStringLiteral("Day 6, morning")),
        cardOf(QStringLiteral("g"), QString()),
        cardOf(QStringLiteral("h"), QStringLiteral("Day 6, evening")),
    };
    // An event outside the text is no flashback, and the book does not tell it before a card
    ChronologyItem outside = cardOf(QStringLiteral("event:1"), QStringLiteral("Day 1"));
    outside.outside = true;
    items.insert(3, outside);
    const ChronologyLayout layout(items, optionsOf());
    // The morning of a day told after the day is no flashback; after its evening it is; a card
    // without a time is passed over
    CHECK(flashbacksOf(layout) == QStringList{QStringLiteral("b"), QStringLiteral("f")});
}

TEST_CASE("Book plan chronology: the periods of the writer are times the cards may say",
          "[timeline][chronology]") {
    const std::optional<StoryTime> winter = StoryTime::parse(QStringLiteral("zima 1931"), DAY_ONE);
    const std::optional<StoryTime> back =
        StoryTime::parse(QStringLiteral("16 April 1931"), DAY_ONE);
    REQUIRE(winter);
    REQUIRE(back);
    const QList<PlanPeriod> periods{{QStringLiteral("Wyprawa"), *winter},
                                    {QStringLiteral("Powrót"), *back}};

    SECTION("a card says the name of a period, any letters, with a part of its day") {
        CHECK(ChronologyLayout::timeOf(QStringLiteral("Wyprawa"), DAY_ONE, periods) == winter);
        CHECK(ChronologyLayout::timeOf(QStringLiteral(" wyprawa "), DAY_ONE, periods) == winter);
        const std::optional<StoryTime> evening =
            ChronologyLayout::timeOf(QStringLiteral("Powrót, wieczorem"), DAY_ONE, periods);
        REQUIRE(evening);
        CHECK(evening->first == back->first);
        CHECK(evening->part == StoryTime::Part::Evening);
        // Another text is read as a time
        CHECK(ChronologyLayout::timeOf(QStringLiteral("Day 3"), DAY_ONE, periods) ==
              StoryTime::parse(QStringLiteral("Day 3"), DAY_ONE));
        CHECK_FALSE(ChronologyLayout::timeOf(QStringLiteral("Powrót"), DAY_ONE, {}));
    }
    SECTION("the file of the plan keeps them, without those it cannot read") {
        QJsonArray json = PlanPeriod::toJson(periods);
        json.append(QJsonObject{{QStringLiteral("name"), QStringLiteral("Bez czasu")}});
        json.append(QJsonObject{{QStringLiteral("time"), winter->toJson()}});
        CHECK(PlanPeriod::fromJson(json) == periods);
    }
    SECTION("a step later of a period is its time a step later") {
        CHECK(shifted(QStringLiteral("Powrót"), 1, periods) == "17 April 1931");
    }
}

TEST_CASE("Book plan chronology: a time a step earlier or later, as exactly as it is known",
          "[timeline][chronology]") {
    // Days of the story stay days of the story, dates stay dates
    CHECK(shifted(QStringLiteral("Day 3"), 1) == "Day 4");
    CHECK(shifted(QStringLiteral("Day 3, evening"), -1) == "Day 2, in the evening");
    CHECK(shifted(QStringLiteral("Days 5–7"), 1) == "Days 6–8");
    CHECK(shifted(QStringLiteral("14 April 1931"), 1) == "15 April 1931");
    // A month, a season, a half of a year, a year
    CHECK(shifted(QStringLiteral("March 1931"), -1) == "February 1931");
    CHECK(shifted(QStringLiteral("spring 1931"), 1) == "summer 1931");
    CHECK(shifted(QStringLiteral("autumn 1931"), 1) == "winter 1932");
    CHECK(shifted(QStringLiteral("first half of 1931"), 1) == "second half of 1931");
    CHECK(shifted(QStringLiteral("1931"), 1) == "1932");
    // A time the program does not read has no step
    CHECK(shifted(QStringLiteral("a year later"), 1) == "-");
    CHECK(shifted(QStringLiteral("Day 3"), 0) == "-");
}
