// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file writing_plan.h
/// @brief The schedule of a book: when each of its texts is written and when it is revised
///
/// The plan is the book's own and is kept in its project.db, next to its goals (the setting
/// WritingPlan::settingKeyOf(the key of the goals)).

#ifndef KALAHARI_CORE_WRITING_PLAN_H
#define KALAHARI_CORE_WRITING_PLAN_H

#include <QDate>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <utility>

namespace kalahari::core {

struct WritingRules;

/// @brief When a text of a book is written and when it is revised
struct TextPlan {
    QString elementId;    ///< The text (a chapter, a scene) of the book
    QDate start;          ///< The first day of writing; invalid: writing not planned
    QDate end;            ///< The last day of writing
    QDate revisionStart;  ///< The first day of revisions; invalid: revisions not planned
    QDate revisionEnd;    ///< The last day of revisions
    int targetWords = 0;  ///< The words the text should have when written; 0: none

    /// @brief Writing is planned: both of its days are known, the end not before the start
    [[nodiscard]] bool hasWriting() const;

    /// @brief Revisions are planned: both of their days are known, the end not before the start
    [[nodiscard]] bool hasRevision() const;

    /// @brief Nothing is planned
    [[nodiscard]] bool isEmpty() const;

    /// @brief The first planned day; invalid when nothing is planned
    [[nodiscard]] QDate firstDay() const;

    /// @brief The last planned day; invalid when nothing is planned
    [[nodiscard]] QDate lastDay() const;

    /// @brief Moves every planned day by @p days
    void shift(int days);

    bool operator==(const TextPlan&) const = default;
};

/// @brief The schedule of a book
struct WritingPlan {
    /// @brief The setting of project.db that keeps the plan of the book the project began with
    static constexpr const char* SETTING_KEY = "writing.plan";

    /// @brief The setting that keeps the plan of the book whose goals the setting
    ///        @p goalsKey keeps: SETTING_KEY for BookGoals::SETTING_KEY, "writing.plan.<id>"
    ///        for "writing.goals.<id>"
    [[nodiscard]] static QString settingKeyOf(const QString& goalsKey);

    QList<TextPlan> texts;  ///< The plans of the texts, in no order

    /// @brief The plan of @p elementId; nullptr when it has none
    [[nodiscard]] const TextPlan* find(const QString& elementId) const;

    /// @brief Keeps @p plan as the plan of its text; an empty plan removes the text's plan
    void set(const TextPlan& plan);

    /// @brief Removes the plan of @p elementId; false when it had none
    bool remove(const QString& elementId);

    /// @brief The first and the last planned day of the book; invalid when nothing is planned
    [[nodiscard]] QDate firstDay() const;
    [[nodiscard]] QDate lastDay() const;

    /// @brief As the setting keeps it
    [[nodiscard]] QString toJson() const;

    /// @brief From the setting; an empty plan from an empty or broken text
    static WritingPlan fromJson(const QString& text);

    bool operator==(const WritingPlan&) const = default;
};

/// @brief A text of a book as the daily goal of its schedule counts it
struct PlannedText {
    int words = 0;              ///< Its words when today's writing day started
    bool beingWritten = true;   ///< In writing still: not in revision, not final
};

/// @brief The daily goal that @p plan needs on @p today: for each text being written whose
///        bar of writing has started by today and that lacks words of its target length, the
///        words it lacked at the start of today over the writing days left in its bar (all of
///        them once its bar has ended); 0 on a day off
/// @param texts The texts of the book; a plan of a text not among them counts nothing
/// @param counted When given, gets the ids of the texts that make up the goal, in the order
///        of the plan
int dailyGoalForPlan(const WritingRules& rules, const WritingPlan& plan,
                     const QHash<QString, PlannedText>& texts, const QDate& today,
                     QStringList* counted = nullptr);

/// @brief Bars of writing for texts one after another, in the order of @p wordsLeft, on the
///        writing days from @p from to @p to (every day when the writer has none): each text
///        has its share of the days by the words it still needs, and texts too short for a
///        day of their own share one; when no text needs words, each has the same share
/// @return The first and the last day of each text, in the same order; empty without texts
///         or without a writing day between the days
QList<std::pair<QDate, QDate>> spreadOverWritingDays(const WritingRules& rules,
                                                    const QList<int>& wordsLeft,
                                                    const QDate& from, const QDate& to);

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_WRITING_PLAN_H
