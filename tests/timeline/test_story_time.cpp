// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_story_time.cpp
/// @brief The time of a story: what the writer types, its text in dates and in days of the
/// story, its place on a time axis and its object in a file

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <kalahari/timeline/story_time.h>

#include <QJsonObject>
#include <QString>

#include <optional>

using namespace kalahari::timeline;
using Catch::Matchers::WithinAbs;

namespace {

qint64 dayOf(int year, int month, int day) {
    const std::optional<qint64> julianDay = CalendarDay{year, month, day}.julianDay();
    REQUIRE(julianDay.has_value());
    return *julianDay;
}

/// Day one of the story in the tests: 14 April 1931
const qint64 DAY_ONE = CalendarDay{1931, 4, 14}.julianDay().value_or(0);

const StoryTimeStyle POLISH{TimeLanguage::Polish, DateStyle::Words, PartStyle::Words};
const StoryTimeStyle ENGLISH{TimeLanguage::English, DateStyle::Words, PartStyle::Words};

StoryTime parsed(const QString& text) {
    const std::optional<StoryTime> time = StoryTime::parse(text, DAY_ONE);
    INFO(text.toStdString());
    REQUIRE(time.has_value());
    return *time;
}

QString polishDates(const QString& text) {
    return parsed(text).dateText(POLISH);
}

}  // namespace

TEST_CASE("A calendar day has astronomical years", "[story_time]") {
    CHECK(CalendarDay::of(dayOf(1931, 4, 14)) == CalendarDay{1931, 4, 14});
    // 1 BC is year 0, 44 BC is year -43
    CHECK(dayOf(1, 1, 1) - dayOf(0, 12, 31) == 1);
    CHECK(CalendarDay::of(dayOf(-43, 3, 15)) == CalendarDay{-43, 3, 15});
    CHECK_FALSE(CalendarDay{1931, 2, 30}.julianDay().has_value());
    CHECK(CalendarDay{1932, 2, 29}.julianDay().has_value());
}

TEST_CASE("The writer types dates, months and years", "[story_time]") {
    SECTION("A day") {
        const StoryTime time = parsed(QStringLiteral("14 kwietnia 1931"));
        CHECK(time.first == DAY_ONE);
        CHECK_FALSE(time.isSpan());
        CHECK(time.precision == StoryTime::Precision::Day);
        CHECK(time.part == StoryTime::Part::None);
        CHECK(parsed(QStringLiteral("14.04.1931")) == time);
        CHECK(parsed(QStringLiteral("1931-04-14")) == time);
        CHECK(parsed(QStringLiteral("14 IV 1931")) == time);
        CHECK(parsed(QStringLiteral("14 April 1931")) == time);
        CHECK(parsed(QStringLiteral("April 14, 1931")) == time);
        CHECK(parsed(QStringLiteral("  14  Kwietnia   1931 ")) == time);
    }
    SECTION("A month and a year") {
        const StoryTime month = parsed(QStringLiteral("marzec 1931"));
        CHECK(month.precision == StoryTime::Precision::Month);
        CHECK(month.first == dayOf(1931, 3, 1));
        CHECK(parsed(QStringLiteral("w marcu 1931")) == month);
        CHECK(parsed(QStringLiteral("March 1931")) == month);
        CHECK(parsed(QStringLiteral("03.1931")) == month);
        const StoryTime year = parsed(QStringLiteral("1931"));
        CHECK(year.precision == StoryTime::Precision::Year);
        CHECK(year.first == dayOf(1931, 1, 1));
    }
    SECTION("Polish letters may be missing") {
        CHECK(parsed(QStringLiteral("1 pazdziernika 1931")).first == dayOf(1931, 10, 1));
        CHECK(parsed(QStringLiteral("Dzien 3, wieczor")) ==
              parsed(QStringLiteral("Dzień 3, wieczór")));
    }
    SECTION("A day that the month does not have is no time") {
        CHECK_FALSE(StoryTime::parse(QStringLiteral("30 lutego 1931"), DAY_ONE).has_value());
        CHECK_FALSE(StoryTime::parse(QStringLiteral("31.04.1931"), DAY_ONE).has_value());
    }
    SECTION("Words that say no time") {
        for (const char* text : {"", "rok później", "przed wyprawą", "12", "1 stycznia 50",
                                 "Dzień 0", "Dni 7–5", "marzec 1931, rano"}) {
            INFO(text);
            CHECK_FALSE(StoryTime::parse(QString::fromUtf8(text), DAY_ONE).has_value());
        }
    }
}

TEST_CASE("The writer types days in a row", "[story_time]") {
    const StoryTime days = parsed(QStringLiteral("16–17 kwietnia 1931"));
    CHECK(days.first == dayOf(1931, 4, 16));
    CHECK(days.last == dayOf(1931, 4, 17));
    CHECK(parsed(QStringLiteral("16-17 kwietnia 1931")) == days);
    CHECK(parsed(QStringLiteral("16 – 17 April 1931")) == days);
    CHECK(parsed(QStringLiteral("16.04.1931 – 17.04.1931")) == days);
    CHECK(parsed(QStringLiteral("1931-04-16 - 1931-04-17")) == days);

    const StoryTime months = parsed(QStringLiteral("2 marca – 5 kwietnia 1931"));
    CHECK(months.first == dayOf(1931, 3, 2));
    CHECK(months.last == dayOf(1931, 4, 5));

    const StoryTime years = parsed(QStringLiteral("30 grudnia 1931 – 2 stycznia 1932"));
    CHECK(years.first == dayOf(1931, 12, 30));
    CHECK(years.last == dayOf(1932, 1, 2));
}

TEST_CASE("The writer types days of the story", "[story_time]") {
    CHECK(parsed(QStringLiteral("Dzień 1")).first == DAY_ONE);
    CHECK(parsed(QStringLiteral("Dzień 3")).first == DAY_ONE + 2);
    // There is no day 0: the day before day one is day -1
    CHECK(parsed(QStringLiteral("Dzień −2")).first == DAY_ONE - 2);
    CHECK(parsed(QStringLiteral("Day -1")).first == DAY_ONE - 1);
    const StoryTime days = parsed(QStringLiteral("Dni 5–7"));
    CHECK(days.first == DAY_ONE + 4);
    CHECK(days.last == DAY_ONE + 6);
    CHECK(parsed(QStringLiteral("Days 5-7")) == days);
}

TEST_CASE("The writer types seasons, halves of a year and years in words", "[story_time]") {
    SECTION("A season with its year") {
        const StoryTime spring = parsed(QStringLiteral("wiosna 1931"));
        CHECK(spring.precision == StoryTime::Precision::Month);
        CHECK(spring.first == dayOf(1931, 3, 1));
        CHECK(spring.last == dayOf(1931, 5, 31));
        CHECK(spring.months() == 3);
        CHECK(parsed(QStringLiteral("Wiosną 1931")) == spring);
        CHECK(parsed(QStringLiteral("na wiosnę 1931")) == spring);
        CHECK(parsed(QStringLiteral("spring 1931")) == spring);
        CHECK(parsed(QStringLiteral("in the spring of 1931")) == spring);
        const StoryTime summer = parsed(QStringLiteral("lato 1931"));
        CHECK(summer.first == dayOf(1931, 6, 1));
        CHECK(summer.last == dayOf(1931, 8, 31));
        CHECK(parsed(QStringLiteral("latem 1931")) == summer);
        CHECK(parsed(QStringLiteral("w lecie 1931")) == summer);
        CHECK(parsed(QStringLiteral("jesień 1931")).first == dayOf(1931, 9, 1));
        CHECK(parsed(QStringLiteral("autumn 1931")).last == dayOf(1931, 11, 30));
    }
    SECTION("Winter has the year of its January") {
        const StoryTime winter = parsed(QStringLiteral("zima 1931"));
        CHECK(winter.first == dayOf(1930, 12, 1));
        CHECK(winter.last == dayOf(1931, 2, 28));
        CHECK(parsed(QStringLiteral("zimą 1931")) == winter);
        CHECK(parsed(QStringLiteral("winter 1931")) == winter);
        CHECK(parsed(QStringLiteral("zima 1930/31")) == winter);
        CHECK(parsed(QStringLiteral("zima 1930/1931")) == winter);
        CHECK_FALSE(StoryTime::parse(QStringLiteral("zima 1930/33"), DAY_ONE).has_value());
        // So it comes before spring of the same year
        CHECK(winter.start() < parsed(QStringLiteral("wiosna 1931")).start());
    }
    SECTION("Halves of a year") {
        const StoryTime first = parsed(QStringLiteral("pierwsza połowa 1931"));
        CHECK(first.first == dayOf(1931, 1, 1));
        CHECK(first.last == dayOf(1931, 6, 30));
        CHECK(first.months() == 6);
        CHECK(parsed(QStringLiteral("w pierwszej połowie 1931")) == first);
        CHECK(parsed(QStringLiteral("I połowa 1931")) == first);
        CHECK(parsed(QStringLiteral("pierwsza połowa roku 1931")) == first);
        CHECK(parsed(QStringLiteral("the first half of 1931")) == first);
        const StoryTime second = parsed(QStringLiteral("druga połowa 1931"));
        CHECK(second.first == dayOf(1931, 7, 1));
        CHECK(second.last == dayOf(1931, 12, 31));
        CHECK(parsed(QStringLiteral("II połowa 1931")) == second);
        CHECK(parsed(QStringLiteral("second half of 1931")) == second);
    }
    SECTION("A year in words") {
        const StoryTime year = parsed(QStringLiteral("1931"));
        CHECK(parsed(QStringLiteral("rok 1931")) == year);
        CHECK(parsed(QStringLiteral("w roku 1931")) == year);
        CHECK(parsed(QStringLiteral("the year 1931")) == year);
        CHECK(parsed(QStringLiteral("rok 44 p.n.e.")) == parsed(QStringLiteral("44 p.n.e.")));
        CHECK_FALSE(StoryTime::parse(QStringLiteral("rok"), DAY_ONE).has_value());
    }
    SECTION("Without its year: the first that does not end before day one") {
        // Day one is 14 April 1931: spring 1931 has it, winter comes after it
        CHECK(parsed(QStringLiteral("wiosna")) == parsed(QStringLiteral("wiosna 1931")));
        CHECK(parsed(QStringLiteral("lato")) == parsed(QStringLiteral("lato 1931")));
        CHECK(parsed(QStringLiteral("zima")) == parsed(QStringLiteral("zima 1932")));
        CHECK(parsed(QStringLiteral("pierwsza połowa")) ==
              parsed(QStringLiteral("pierwsza połowa 1931")));
        CHECK(parsed(QStringLiteral("druga połowa")) ==
              parsed(QStringLiteral("druga połowa 1931")));
        // Summer, then winter: in the order of the story
        CHECK(parsed(QStringLiteral("lato")).start() < parsed(QStringLiteral("zima")).start());
    }
    SECTION("Months from one to another") {
        const StoryTime months = parsed(QStringLiteral("marzec – maj 1931"));
        CHECK(months == parsed(QStringLiteral("wiosna 1931")));
        const StoryTime turn = parsed(QStringLiteral("listopad 1931 – luty 1932"));
        CHECK(turn.first == dayOf(1931, 11, 1));
        CHECK(turn.last == dayOf(1932, 2, 29));
        CHECK(turn.months() == 4);
        CHECK(parsed(QStringLiteral("April – July 1931")).months() == 4);
    }
    SECTION("The program writes them in words") {
        CHECK(polishDates(QStringLiteral("spring 1931")) == QStringLiteral("wiosna 1931"));
        CHECK(polishDates(QStringLiteral("zima 1930/31")) == QStringLiteral("zima 1931"));
        CHECK(polishDates(QStringLiteral("first half of 1931")) ==
              QStringLiteral("pierwsza połowa 1931"));
        CHECK(polishDates(QStringLiteral("kwiecień – lipiec 1931")) ==
              QStringLiteral("kwiecień – lipiec 1931"));
        CHECK(parsed(QStringLiteral("jesień 1931")).dateText(ENGLISH) ==
              QStringLiteral("autumn 1931"));
        CHECK(parsed(QStringLiteral("druga połowa 1931")).dateText(ENGLISH) ==
              QStringLiteral("second half of 1931"));
    }
}

TEST_CASE("The writer types days of the story in words", "[story_time]") {
    CHECK(parsed(QStringLiteral("Dzień pierwszy")).first == DAY_ONE);
    CHECK(parsed(QStringLiteral("dzień drugi")).first == DAY_ONE + 1);
    CHECK(parsed(QStringLiteral("Drugiego dnia")).first == DAY_ONE + 1);
    CHECK(parsed(QStringLiteral("dnia trzeciego")).first == DAY_ONE + 2);
    CHECK(parsed(QStringLiteral("trzeci dzień")).first == DAY_ONE + 2);
    CHECK(parsed(QStringLiteral("w piątym dniu")).first == DAY_ONE + 4);
    CHECK(parsed(QStringLiteral("dzień dwunasty")).first == DAY_ONE + 11);
    CHECK(parsed(QStringLiteral("dzień dwudziesty pierwszy")).first == DAY_ONE + 20);
    CHECK(parsed(QStringLiteral("Dzień drugi, wieczorem")) ==
          parsed(QStringLiteral("Dzień 2, wieczorem")));
    CHECK(parsed(QStringLiteral("day one")).first == DAY_ONE);
    CHECK(parsed(QStringLiteral("Day twenty-one")).first == DAY_ONE + 20);
    CHECK(parsed(QStringLiteral("the second day")).first == DAY_ONE + 1);
    CHECK(parsed(QStringLiteral("on the third day, at night")) ==
          parsed(QStringLiteral("Day 3, at night")));
    for (const char* text : {"dzień", "dzień zerowy", "the day", "dzień sto pierwszy"}) {
        INFO(text);
        CHECK_FALSE(StoryTime::parse(QString::fromUtf8(text), DAY_ONE).has_value());
    }
}

TEST_CASE("The writer types a part of the day", "[story_time]") {
    CHECK(parsed(QStringLiteral("14 kwietnia 1931, rano")).part == StoryTime::Part::Morning);
    CHECK(parsed(QStringLiteral("Dzień 3, noc")).part == StoryTime::Part::Night);
    CHECK(parsed(QStringLiteral("Dzień 3 w nocy")).part == StoryTime::Part::Night);
    CHECK(parsed(QStringLiteral("Dzień 4, świt")).part == StoryTime::Part::Dawn);
    CHECK(parsed(QStringLiteral("Dzień 3, południe")).part == StoryTime::Part::Noon);
    CHECK(parsed(QStringLiteral("Dzień 3, po południu")).part == StoryTime::Part::Afternoon);
    CHECK(parsed(QStringLiteral("Day 3, in the evening")).part == StoryTime::Part::Evening);
    const StoryTime clock = parsed(QStringLiteral("14 kwietnia 1931, 19:30"));
    CHECK(clock.part == StoryTime::Part::Clock);
    CHECK(clock.minutes == 19 * 60 + 30);
    CHECK(parsed(QStringLiteral("14.04.1931 o 19.30")) == clock);
    CHECK_FALSE(StoryTime::parse(QStringLiteral("Dzień 3, 25:00"), DAY_ONE).has_value());
}

TEST_CASE("The writer types about when and years before the Common Era", "[story_time]") {
    const StoryTime about = parsed(QStringLiteral("ok. 22 stycznia 1931"));
    CHECK(about.approximate);
    CHECK(about.first == dayOf(1931, 1, 22));
    CHECK(parsed(QStringLiteral("około 22 stycznia 1931")) == about);
    CHECK(parsed(QStringLiteral("c. 22 January 1931")) == about);
    CHECK(parsed(QStringLiteral("ok 1931")).approximate);

    const StoryTime ides = parsed(QStringLiteral("15 marca 44 p.n.e."));
    CHECK(ides.first == dayOf(-43, 3, 15));
    CHECK(parsed(QStringLiteral("15 March 44 BC")) == ides);
    CHECK(parsed(QStringLiteral("15.03.44 pne")) == ides);
    CHECK(parsed(QStringLiteral("44 p.n.e.")).first == dayOf(-43, 1, 1));
    // Two digits are a year only before the Common Era, so that "12" is not taken for one
    CHECK(parsed(QStringLiteral("1 stycznia 50 n.e.")).precision == StoryTime::Precision::Day);
}

TEST_CASE("The program writes a time in dates", "[story_time]") {
    CHECK(polishDates(QStringLiteral("14.04.1931, wieczorem")) == "14 kwietnia 1931, wieczorem");
    CHECK(polishDates(QStringLiteral("16-17.04.1931")) == QStringLiteral("16–17 kwietnia 1931"));
    CHECK(polishDates(QStringLiteral("2 marca – 5 kwietnia 1931")) ==
          QStringLiteral("2 marca – 5 kwietnia 1931"));
    CHECK(polishDates(QStringLiteral("30 grudnia 1931 – 2 stycznia 1932")) ==
          QStringLiteral("30 grudnia 1931 – 2 stycznia 1932"));
    CHECK(polishDates(QStringLiteral("w marcu 1931")) == QStringLiteral("marzec 1931"));
    CHECK(polishDates(QStringLiteral("ok. 1931")) == QStringLiteral("ok. 1931"));
    CHECK(polishDates(QStringLiteral("15 March 44 BC")) == QStringLiteral("15 marca 44 p.n.e."));

    const StoryTime morning = parsed(QStringLiteral("Dzień 1, rano"));
    CHECK(morning.dateText(ENGLISH) == QStringLiteral("14 April 1931, in the morning"));
    StoryTimeStyle numbers = POLISH;
    numbers.dates = DateStyle::Numbers;
    numbers.parts = PartStyle::Clock;
    CHECK(morning.dateText(numbers) == QStringLiteral("14.04.1931, 8:00"));
    StoryTimeStyle iso = ENGLISH;
    iso.dates = DateStyle::Iso;
    CHECK(morning.dateText(iso) == QStringLiteral("1931-04-14, in the morning"));
    CHECK(parsed(QStringLiteral("15 marca 44 p.n.e.")).dateText(iso) ==
          QStringLiteral("15 March 44 BC"));
}

TEST_CASE("What the program writes reads back as the same time", "[story_time]") {
    StoryTimeStyle numbers = POLISH;
    numbers.dates = DateStyle::Numbers;
    for (const char* text :
         {"14 kwietnia 1931, rano", "16–17 kwietnia 1931", "2 marca – 5 kwietnia 1931",
          "marzec 1931", "ok. 1931", "15 marca 44 p.n.e.", "Dzień 3, 19:30", "wiosna 1931",
          "zima 1931", "pierwsza połowa 1931", "druga połowa 44 p.n.e.",
          "listopad 1931 – luty 1932",
          "30 grudnia 1931 – 2 stycznia 1932, w nocy", "ok. 22 stycznia 1931, po południu"}) {
        INFO(text);
        const StoryTime time = parsed(QString::fromUtf8(text));
        for (const StoryTimeStyle& style : {POLISH, ENGLISH, numbers}) {
            CHECK(StoryTime::parse(time.dateText(style), DAY_ONE) == time);
        }
        if (time.precision == StoryTime::Precision::Day) {
            CHECK(StoryTime::parse(time.dayText(DAY_ONE, POLISH), DAY_ONE) == time);
            CHECK(StoryTime::parse(time.dayText(DAY_ONE, ENGLISH), DAY_ONE) == time);
        }
    }
}

TEST_CASE("The program writes a time in days of the story", "[story_time]") {
    CHECK(parsed(QStringLiteral("16 kwietnia 1931, wieczorem")).dayText(DAY_ONE, POLISH) ==
          QStringLiteral("Dzień 3, wieczorem"));
    CHECK(parsed(QStringLiteral("18–20 kwietnia 1931")).dayText(DAY_ONE, POLISH) ==
          QStringLiteral("Dni 5–7"));
    CHECK(parsed(QStringLiteral("12 kwietnia 1931")).dayText(DAY_ONE, ENGLISH) ==
          QStringLiteral("Day −2"));
    // A month is about its middle
    CHECK(parsed(QStringLiteral("kwiecień 1931")).dayText(DAY_ONE, POLISH) ==
          QStringLiteral("ok. Dzień 2"));
    CHECK(StoryTime::dayNumber(DAY_ONE, DAY_ONE) == 1);
    CHECK(StoryTime::dayNumber(DAY_ONE - 1, DAY_ONE) == -1);
    CHECK(StoryTime::dayOfNumber(-1, DAY_ONE) == DAY_ONE - 1);
}

TEST_CASE("A time has its place on a time axis", "[story_time]") {
    const auto day = static_cast<double>(DAY_ONE);
    CHECK_THAT(parsed(QStringLiteral("Dzień 1")).start(), WithinAbs(day + 0.5, 1e-9));
    CHECK_THAT(parsed(QStringLiteral("Dzień 1, rano")).start(), WithinAbs(day + 0.3, 1e-9));
    CHECK_THAT(parsed(QStringLiteral("Dzień 1, 18:00")).start(), WithinAbs(day + 0.75, 1e-9));
    const StoryTime days = parsed(QStringLiteral("Dni 1–3"));
    CHECK_THAT(days.start(), WithinAbs(day, 1e-9));
    CHECK_THAT(days.end(), WithinAbs(day + 3, 1e-9));
    CHECK_THAT(parsed(QStringLiteral("kwiecień 1931")).start(),
               WithinAbs(static_cast<double>(dayOf(1931, 4, 1)) + 14, 1e-9));
}

TEST_CASE("A time moves by days and by months", "[story_time]") {
    const StoryTime days = parsed(QStringLiteral("16–17 kwietnia 1931, rano"));
    CHECK(days.movedByDays(-2) == parsed(QStringLiteral("14–15 kwietnia 1931, rano")));
    CHECK(parsed(QStringLiteral("31 stycznia 1931")).movedByMonths(1) ==
          parsed(QStringLiteral("28 lutego 1931")));
    CHECK(parsed(QStringLiteral("marzec 1931")).movedByMonths(-3) ==
          parsed(QStringLiteral("grudzień 1930")));
    CHECK(parsed(QStringLiteral("1 stycznia 1 n.e.")).movedByMonths(-1) ==
          parsed(QStringLiteral("1 grudnia 1 p.n.e.")));
    // Months in a row keep how many they are
    CHECK(parsed(QStringLiteral("zima 1931")).movedByMonths(3) ==
          parsed(QStringLiteral("wiosna 1931")));
    CHECK(parsed(QStringLiteral("pierwsza połowa 1931")).movedByMonths(6) ==
          parsed(QStringLiteral("druga połowa 1931")));
    CHECK(parsed(QStringLiteral("wiosna 1931")).movedByDays(92) ==
          parsed(QStringLiteral("czerwiec – sierpień 1931")));
}

TEST_CASE("A file keeps a time as an object", "[story_time]") {
    const StoryTime days = parsed(QStringLiteral("ok. 16–17 kwietnia 1931, wieczorem"));
    const QJsonObject json = days.toJson();
    CHECK(json.value(QStringLiteral("from")).toString() == QStringLiteral("1931-04-16"));
    CHECK(json.value(QStringLiteral("to")).toString() == QStringLiteral("1931-04-17"));
    CHECK(json.value(QStringLiteral("part")).toString() == QStringLiteral("evening"));
    CHECK(json.value(QStringLiteral("approx")).toBool());
    CHECK(StoryTime::fromJson(json) == days);

    const QJsonObject spring = parsed(QStringLiteral("wiosna 1931")).toJson();
    CHECK(spring.value(QStringLiteral("from")).toString() == QStringLiteral("1931-03"));
    CHECK(spring.value(QStringLiteral("to")).toString() == QStringLiteral("1931-05"));
    for (const char* text : {"marzec 1931", "1931", "15 marca 44 p.n.e.", "Dzień 3, 19:05",
                             "zima 1931", "listopad 1931 – luty 1932"}) {
        INFO(text);
        const StoryTime time = parsed(QString::fromUtf8(text));
        CHECK(StoryTime::fromJson(time.toJson()) == time);
    }
    CHECK(parsed(QStringLiteral("15 marca 44 p.n.e.")).toJson().value(QStringLiteral("from")) ==
          QStringLiteral("-0043-03-15"));
    CHECK(parsed(QStringLiteral("marzec 1931")).toJson().value(QStringLiteral("from")) ==
          QStringLiteral("1931-03"));
    CHECK_FALSE(StoryTime::fromJson(QJsonObject{{QStringLiteral("from"), "wiosna"}}).has_value());
    CHECK_FALSE(StoryTime::fromJson(QJsonObject{{QStringLiteral("from"), "1931-02-30"}}));
}

TEST_CASE("The length of a gap without events", "[story_time]") {
    CHECK(StoryTime::lengthText(3, TimeLanguage::Polish) == QStringLiteral("3 dni"));
    CHECK(StoryTime::lengthText(1, TimeLanguage::Polish) == QStringLiteral("1 dzień"));
    CHECK(StoryTime::lengthText(21, TimeLanguage::Polish) == QStringLiteral("≈ 3 tyg."));
    CHECK(StoryTime::lengthText(80, TimeLanguage::Polish) == QStringLiteral("≈ 3 mies."));
    CHECK(StoryTime::lengthText(730, TimeLanguage::Polish) == QStringLiteral("≈ 2 lata"));
    CHECK(StoryTime::lengthText(1825, TimeLanguage::Polish) == QStringLiteral("≈ 5 lat"));
    CHECK(StoryTime::lengthText(80, TimeLanguage::English) == QStringLiteral("≈ 3 months"));
    CHECK(StoryTime::lengthText(1, TimeLanguage::English) == QStringLiteral("1 day"));
}
