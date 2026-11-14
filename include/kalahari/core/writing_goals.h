// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file writing_goals.h
/// @brief The goals of a book and its deadlines, and the reckoning of the words a deadline
/// needs and of the day the book will be written
///
/// The daily and weekly goals and the writing days are the writer's own (WritingRules, in the
/// settings); the length of a book, its deadline and the deadlines of its parts are the book's
/// own and are kept in its project.db (the setting BookGoals::SETTING_KEY).

#pragma once

#include "kalahari/core/writing_statistics.h"

#include <QDate>
#include <QList>
#include <QString>

namespace kalahari::core {

/// @brief A deadline of a book: a part of it, or all of it, due by a day
struct WritingDeadline {
    QString name;         ///< What is due, in the writer's words ("Chapters 1-10 for the editor")
    QDate date;           ///< The day it is due
    QString elementId;    ///< The part of the book it is about; empty: the whole book
    int targetWords = 0;  ///< The words that part should have by then; 0: none

    bool operator==(const WritingDeadline&) const = default;
};

/// @brief The goals of a book
struct BookGoals {
    /// @brief The setting of project.db that keeps them, as JSON
    static constexpr const char* SETTING_KEY = "writing.goals";

    int targetWords = 0;                ///< The length of the book in words; 0: none
    QDate deadline;                     ///< The day the book is due; invalid: none
    bool dailyGoalFromDeadline = false; ///< The daily goal is what the deadline needs
    QList<WritingDeadline> deadlines;   ///< The deadlines of the parts, by date

    /// @brief No goal and no deadline
    bool isEmpty() const;

    /// @brief The deadlines by date; those without a date last
    void sortDeadlines();

    /// @brief The index of the first deadline not passed on @p today; -1 when none
    int nextDeadline(const QDate& today) const;

    /// @brief The book has a length and a deadline not passed on @p today: the daily goal
    /// can come from them
    bool hasDeadlineAhead(const QDate& today) const;

    /// @brief As the setting keeps them
    QString toJson() const;

    /// @brief From the setting; empty goals from an empty or broken text
    static BookGoals fromJson(const QString& text);

    bool operator==(const BookGoals&) const = default;
};

/// @brief The writing days from @p from to @p to, both included; when the writer has no
/// writing days, every day is one
int writingDaysBetween(const WritingRules& rules, const QDate& from, const QDate& to);

/// @brief The words of each of @p writingDays days that write @p wordsLeft words, rounded up;
/// 0 when nothing is left, all of them when no days are left
int wordsPerWritingDay(int wordsLeft, int writingDays);

/// @brief The day the last of @p wordsLeft words is written, at @p averagePerDay words a
/// writing day from the day after @p today; @p today when nothing is left, invalid when the
/// average is not more than 0 or the day is more than a hundred years away
QDate forecastDate(const WritingRules& rules, const QDate& today, int wordsLeft,
                   double averagePerDay);

/// @brief The daily goal that the deadline of the book needs: the words left at the start of
/// @p today over the writing days from today to the deadline; 0 without a length and a
/// deadline, or after the deadline
int dailyGoalForDeadline(const WritingRules& rules, const BookGoals& goals,
                         int wordsAtStartOfToday, const QDate& today);

}  // namespace kalahari::core
