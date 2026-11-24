// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file story_time.h
/// @brief When something happens in a story: a day or days of the calendar, a month or a year,
/// perhaps with a part of the day, as the writer types it and as the program shows it

#ifndef KALAHARI_TIMELINE_STORY_TIME_H
#define KALAHARI_TIMELINE_STORY_TIME_H

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

#include <optional>

namespace kalahari::timeline {

/// @brief A day of the calendar, with astronomical years: 0 is 1 BC, -43 is 44 BC
struct CalendarDay {
    int year = 1;   ///< Astronomical year
    int month = 1;  ///< 1-12
    int day = 1;    ///< 1-31

    /// @brief The Julian day number of the day, or nullopt when there is no such day
    [[nodiscard]] std::optional<qint64> julianDay() const;

    /// @brief The day of Julian day number @p julianDay
    [[nodiscard]] static CalendarDay of(qint64 julianDay);

    bool operator==(const CalendarDay&) const = default;
};

/// @brief The language of the words of a time the program writes
enum class TimeLanguage {
    English,  ///< "14 April 1931, in the morning", "Day 3"
    Polish    ///< "14 kwietnia 1931, rano", "Dzień 3"
};

/// @brief How the program writes the days of a time
enum class DateStyle {
    Words,    ///< "14 kwietnia 1931"
    Numbers,  ///< "14.04.1931"
    Iso       ///< "1931-04-14"; years before the Common Era in words
};

/// @brief How the program writes the part of the day of a time
enum class PartStyle {
    Words,  ///< "wieczorem"
    Clock   ///< "19:00"
};

/// @brief How the program writes a time
struct StoryTimeStyle {
    TimeLanguage language = TimeLanguage::English;
    DateStyle dates = DateStyle::Words;
    PartStyle parts = PartStyle::Words;
};

/// @brief When something happens in a story
///
/// The writer types it as a date, a month or a year ("14 kwietnia 1931, rano", "marzec 1931",
/// "ok. 1931", "44 p.n.e.", "rok 1931", "16–17 kwietnia 1931"), as months in a row: a season,
/// a half of a year or months from one to another ("wiosna 1931", "zima 1931/32", "pierwsza
/// połowa 1931", "marzec – maj 1931"), or as days of the story ("Dzień 3, noc", "Dzień
/// pierwszy", "drugiego dnia", "Dni 5–7"), which count from the first day of the story, day
/// one. English works as well ("14 April 1931, in the morning", "summer 1931", "first half of
/// 1931", "Day 3", "the second day"). A season or a half without its year is the first one
/// that does not end before day one ("lato", "zima"). Winter has the year of its January:
/// "zima 1931" is December 1930 to February 1931. The program keeps the days of the calendar,
/// so the same time shows as dates or as days of the story.
///
/// A file keeps it as an object:
/// @code{.json}
/// { "from": "1931-04-16", "to": "1931-04-17", "part": "evening", "approx": true }
/// @endcode
/// "from" says how exactly the time is known: "1931" a year, "1931-04" a month, "1931-04-16"
/// a day; a year before the Common Era is astronomical and has its sign: "-0043-03-15" is
/// 15 March 44 BC. "to" is the last day of days in a row, or the last month of months in a row
/// ("from": "1931-03", "to": "1931-05": spring 1931). "part" is a part of the day or a time of
/// the clock ("19:30").
struct StoryTime {
    /// @brief How exactly the time is known
    enum class Precision { Day, Month, Year };

    /// @brief The part of the day of a time
    enum class Part { None, Dawn, Morning, Noon, Afternoon, Evening, Night, Clock };

    qint64 first = 0;                       ///< Julian day of its first day; of a month or a
                                            ///< year, of their first day
    qint64 last = 0;                        ///< Julian day of its last day; first for one day,
                                            ///< one month or one year; of months in a row,
                                            ///< the last day of their last month
    Precision precision = Precision::Day;
    Part part = Part::None;
    int minutes = 0;                        ///< Part::Clock: minutes after midnight
    bool approximate = false;               ///< "ok. 1931": about then

    /// @brief One day @p julianDay
    [[nodiscard]] static StoryTime day(qint64 julianDay);

    /// @brief Whether it is days in a row, or months in a row
    [[nodiscard]] bool isSpan() const { return last != first; }

    /// @brief Months @p count in a row from month @p month of year @p year (one month for 1):
    /// a season, a half of a year; nullopt when there is no such month
    [[nodiscard]] static std::optional<StoryTime> monthsInRow(int year, int month, int count);

    /// @brief How many months it is: 1 for one month, 3 for a season; 0 for a day or a year
    [[nodiscard]] int months() const;

    /// @brief Where it starts on a time axis, in days: its first day and the part of the day
    /// (the middle of the day without one); the middle of a month or of a year
    [[nodiscard]] double start() const;

    /// @brief Where it ends on a time axis, in days: after the last day of days in a row, else
    /// start()
    [[nodiscard]] double end() const;

    /// @brief The same time @p days later (earlier when negative)
    [[nodiscard]] StoryTime movedByDays(qint64 days) const;

    /// @brief The same time @p months later (earlier when negative); a day that the month
    /// does not have becomes its last day
    [[nodiscard]] StoryTime movedByMonths(int months) const;

    /// @brief The time @p text says, or nullopt when it says none the program reads
    /// @param dayOne Julian day of day one of the story: "Dzień 3" is two days after it
    [[nodiscard]] static std::optional<StoryTime> parse(const QString& text, qint64 dayOne);

    /// @brief The time in dates: "14 kwietnia 1931, rano", "marzec 1931", "wiosna 1931",
    /// "pierwsza połowa 1931", "ok. 44 p.n.e."
    ///
    /// The text in words or numbers reads back as the same time (parse()).
    [[nodiscard]] QString dateText(const StoryTimeStyle& style) const;

    /// @brief The time in days of the story: "Dzień 3, rano", "Dni 5–7"; a month or a year is
    /// "ok. Dzień 40", its middle
    /// @param dayOne Julian day of day one of the story
    [[nodiscard]] QString dayText(qint64 dayOne, const StoryTimeStyle& style) const;

    /// @brief The number of the day of the story of Julian day @p julianDay: 1 for day one,
    /// -1 for the day before it; there is no day 0
    [[nodiscard]] static int dayNumber(qint64 julianDay, qint64 dayOne);

    /// @brief Julian day of day @p number of the story (not 0)
    [[nodiscard]] static qint64 dayOfNumber(int number, qint64 dayOne);

    /// @brief How long @p days is, roughly, for a gap without events: "3 dni", "≈ 2 tyg.",
    /// "≈ 5 mies.", "≈ 3 lata"
    [[nodiscard]] static QString lengthText(double days, TimeLanguage language);

    /// @brief The year as the program writes it: "1931", "44 p.n.e.", "44 BC"
    [[nodiscard]] static QString yearText(int year, TimeLanguage language);

    /// @brief The name of month @p month (1-12): "kwiecień", "April"
    [[nodiscard]] static QString monthName(int month, TimeLanguage language);

    /// @brief The time as a file keeps it
    [[nodiscard]] QJsonObject toJson() const;

    /// @brief The time a file keeps, or nullopt when the object is not one
    [[nodiscard]] static std::optional<StoryTime> fromJson(const QJsonObject& json);

    bool operator==(const StoryTime&) const = default;
};

}  // namespace kalahari::timeline

#endif  // KALAHARI_TIMELINE_STORY_TIME_H
