// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file writing_statistics.h
/// @brief The statistics of writing counted from the record of writing: the writing days,
/// the graph of a day, the days in a row, the hours of writing and the temperature of work
///
/// A writing day is not a day of the calendar: it ends at the time the writer chose in the
/// morning (4:00 by default), so the words written after midnight count to the evening the
/// work started in. A change of that time counts the whole record again, since the record
/// keeps the slots of the days of the calendar.

#pragma once

#include "kalahari/core/writing_activity.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMap>
#include <QString>

#include <array>
#include <utility>

namespace kalahari::core {

/// @brief What counts toward the goals and the days in a row
enum class GoalCounting {
    Net,      ///< The growth of the text: the words written minus the words deleted
    Written,  ///< All the words written
};

/// @brief Which writing day keeps the days in a row going
enum class StreakRule {
    MinimumWords,  ///< A day with at least WritingRules::streakMinimumWords words
    DailyGoal,     ///< A day with the daily goal reached
};

/// @brief The temperature of work, shown to the writer as the pace of work: how much and how
/// fast the writer writes now, compared with the writer's own usual work
enum class WritingTemperature {
    Unknown,     ///< Too little record of the usual work to compare with
    Idle,        ///< Nothing written or deleted lately
    MuchFast,    ///< More words than usual, written faster than usual: work at full speed
    MuchSlow,    ///< More words than usual, written slower than usual: calm writing
    LittleFast,  ///< Fewer words than usual, changed faster than usual: corrections
    LittleSlow,  ///< Fewer words than usual, slower than usual: a standstill
};

/// @brief The choices of the writer for counting the statistics and the goals
struct WritingRules {
    int dayStartMinutes = 4 * 60;  ///< A writing day ends at this time of the next morning
    int idleMinutes = 5;           ///< The pause after which the time of writing stops
    int largeChangeWords = 500;    ///< A change of more words at once is not writing; 0: off
    bool countPasted = false;      ///< The words pasted count as written (and undone, deleted)
    GoalCounting counting = GoalCounting::Net;
    bool allBooks = true;          ///< The daily goals count the words of all the books
    int writingDays = 0x1f;        ///< Bit 0 is Monday, ... bit 6 Sunday; Monday to Friday
    int dailyGoal = 1000;          ///< Words of a writing day; 0: no daily goal
    int weeklyGoal = 0;            ///< Words of a week; 0: the daily goals of its writing days
    StreakRule streakRule = StreakRule::MinimumWords;
    int streakMinimumWords = 100;  ///< The words of a day that keep the days in a row going

    /// @brief The writer writes on @p day (the other days are days off)
    bool isWritingDay(const QDate& day) const;

    /// @brief The daily goal of @p day: 0 on a day off
    int dailyGoalOn(const QDate& day) const;

    /// @brief The goal of a week: the weekly goal, or the daily goals of its writing days
    int weeklyGoalWords() const;

    /// @brief The rules the settings of the writer give (Settings > Statistics and Goals)
    static WritingRules fromSettings();
};

/// @brief The words written and deleted and the time of writing, added up
struct WritingTotals {
    int written = 0;
    int deleted = 0;
    int pasted = 0;  ///< The words pasted, less the pastes undone; in written when they count
    qint64 activeMs = 0;

    /// @brief The growth of the text
    int net() const { return written - deleted; }

    /// @brief The words that count toward the goals under @p counting
    int counted(GoalCounting counting) const {
        return counting == GoalCounting::Net ? net() : written;
    }

    /// @brief The words that count, in an hour of writing; 0 without time of writing
    double wordsPerHour(GoalCounting counting) const;

    bool isEmpty() const {
        return written == 0 && deleted == 0 && pasted == 0 && activeMs == 0;
    }

    void add(const WritingActivity& activity);
    WritingTotals& operator+=(const WritingTotals& other);
};

/// @brief The average words written in each hour of each day of the week
///
/// The day of the week is the writing day's, the hour is the clock's: the words written at
/// 1:00 on Thursday night are in the hour 1 of Wednesday, after its hour 23.
struct WritingHourMap {
    /// Index 0 is Monday, ... 6 Sunday; then the hours 0 to 23 of the clock
    std::array<std::array<double, 24>, 7> words{};

    /// @brief The day of the week with the most words: 1 Monday, ... 7 Sunday; 0 when none
    int bestDayOfWeek() const;

    /// @brief The first hour of the @p width hours in a row with the most words on all the
    /// days of the week (23, 0 and 1 are in a row); -1 when none
    int bestHours(int width = 2) const;

    /// @brief The day of the week (1 Monday, ... 7 Sunday) and the first of the @p width
    /// hours in a row with the most words on that day; {0, -1} when none
    std::pair<int, int> bestTime(int width = 2) const;

    /// @brief The share of the words written from @p fromHour to @p toHour (24 is midnight;
    /// a range that passes midnight, like 18 to 4, is fine), 0 to 1
    double shareOfHours(int fromHour, int toHour) const;

    /// @brief The share of the words written on the days from @p fromDay to @p toDay (1 Monday,
    /// ... 7 Sunday, both included; Saturday to Monday is fine), 0 to 1
    double shareOfDays(int fromDay, int toDay) const;
};

/// @brief The statistics of a record of writing under the rules of the writer
///
/// It is given the record of the days it counts (of all the books, or of one), and the
/// record not saved yet may be added to it.
class WritingStatistics {
public:
    explicit WritingStatistics(const WritingRules& rules = {});

    /// @brief The rules it counts by
    const WritingRules& rules() const { return m_rules; }

    /// @brief Counts by @p rules from now on; the end of a writing day is taken to a whole
    /// slot (4:03 is 4:00)
    void setRules(const WritingRules& rules);

    /// @brief Counts this record instead of the one it had
    void setActivity(const QList<WritingActivity>& activity);

    /// @brief Counts this record too (the record not saved yet, or just saved)
    void addActivity(const QList<WritingActivity>& activity);

    /// @brief The whole record it counts, by writing day, as it was kept: the words pasted
    /// apart (counted() gives an entry as the rules count it)
    const QMap<QDate, QList<WritingActivity>>& record() const { return m_byDay; }

    /// @brief An entry of the record as the rules count it: with WritingRules::countPasted
    /// the words pasted are written (the pastes undone, deleted), otherwise they are not
    /// counted; the words pasted stay in WritingActivity::pasted either way
    WritingActivity counted(const WritingActivity& entry) const;

    /// @brief The writing day of a slot of a day of the calendar
    QDate writingDayOf(const QDate& date, int slot) const;

    /// @brief The writing day of a moment
    QDate writingDayOf(const QDateTime& when) const;

    /// @brief The first writing day of the record; invalid when it is empty
    QDate firstDay() const;

    /// @brief The totals of a writing day
    WritingTotals day(const QDate& writingDay) const;

    /// @brief How much the texts grew on a writing day, whatever the rules count: the words
    /// pasted too, and the words written and deleted again cancel out
    int textGrowth(const QDate& writingDay) const;

    /// @brief The totals of the writing days from @p from to @p to, both included
    WritingTotals period(const QDate& from, const QDate& to) const;

    /// @brief The words that count on a writing day, in pieces of @p minutes from its start
    /// (a multiple of ACTIVITY_SLOT_MINUTES that divides a day; others are made one)
    /// @return One number for each piece of the day, 24 * 60 / minutes of them
    QList<int> dayGraph(const QDate& writingDay, int minutes) const;

    /// @brief The totals of each text (element id) in the writing days from @p from to @p to
    QHash<QString, WritingTotals> texts(const QDate& from, const QDate& to) const;

    /// @brief A writing day keeps the days in a row going
    bool countsToStreak(const QDate& writingDay) const;

    /// @brief The words a writing day needs to keep the days in a row going: the minimum of
    /// the rules, or the daily goal (the minimum when there is no daily goal); on a day off too
    int streakWords() const;

    /// @brief The days in a row up to the writing day @p today: today counts when it already
    /// keeps them going, and does not break them before it is over; a day off that does not
    /// keep them going neither breaks them nor adds a day
    int streak(const QDate& today) const;

    /// @brief The words that count, on average in a writing day from @p from to @p to; the
    /// words of the days off count, the days off do not, and neither do the days before the
    /// first day of the record
    double averagePerWritingDay(const QDate& from, const QDate& to) const;

    /// @brief The average words written in each hour of each day of the week from @p from to
    /// @p to; the days before the first day of the record do not count
    WritingHourMap hourMap(const QDate& from, const QDate& to) const;

    /// @brief The temperature of work at @p now: the last @p windowMinutes compared with the
    /// usual work of the @p historyDays days before today
    ///
    /// "Much": more words that count than in the same time of usual writing. "Fast": more
    /// words written and deleted in a minute of writing than usually. Unknown with less than
    /// TEMPERATURE_HISTORY_MINUTES of writing in those days.
    WritingTemperature temperature(const QDateTime& now, int windowMinutes = 30,
                                   int historyDays = 28) const;

    /// @brief The time of writing in the days before today that the temperature needs
    static constexpr int TEMPERATURE_HISTORY_MINUTES = 60;

private:
    void regroup();
    void addToDays(const WritingActivity& activity);

    // A copy shares the record until one of them changes, and then copies only the map of
    // the days, so a copy with the record not saved yet added to it is cheap
    WritingRules m_rules;
    QMap<QDate, QList<WritingActivity>> m_byDay;  ///< The record by writing day
    QMap<QDate, WritingTotals> m_days;             ///< The totals by writing day
};

}  // namespace kalahari::core
