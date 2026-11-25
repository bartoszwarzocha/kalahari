// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file spread_plan_dialog.h
/// @brief The small window Plan the Texts of the tab Writing Schedule: the bars of writing of
/// many texts at once, one after another, from a day to the deadline of the book

#ifndef KALAHARI_GUI_DIALOGS_SPREAD_PLAN_DIALOG_H
#define KALAHARI_GUI_DIALOGS_SPREAD_PLAN_DIALOG_H

#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/widgets/schedule_view.h"

#include <kalahari/core/writing_goals.h>
#include <kalahari/core/writing_plan.h>
#include <kalahari/core/writing_statistics.h>

#include <QDate>
#include <QList>
#include <QStringList>

class QDateEdit;
class QLabel;
class QRadioButton;
class QSpinBox;

namespace kalahari::gui::dialogs {

/// @brief Plans the writing of many texts at once
///
/// The texts are those without a plan of writing, or all those in Draft; Final texts and
/// texts in Revision are written already. They follow each other in the order of the book,
/// on the writing days from the first day to the last (the deadline of the book when it has
/// one), each with its share of the days by the words it still needs (core::
/// spreadOverWritingDays()). A text without a length of its own takes the length the window
/// gives, and keeps it in its plan. The revisions planned stay as they are. The note says how
/// many words a writing day needs before anything changes.
class SpreadPlanDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Which texts are planned
    enum class Texts {
        WithoutPlan,   ///< Those in Draft without a plan of writing
        BeingWritten,  ///< All those in Draft, their bars of writing laid again
    };

    /// @param rows The rows of the schedule, in the order of the book (only the texts count)
    /// @param plan The schedule now
    /// @param goals The goals of the book: its deadline is the last day, its length gives the
    ///        length of a text without its own
    /// @param rules The writing days of the writer
    /// @param today The writing day of now: the first day
    /// @param parent Parent widget
    SpreadPlanDialog(const QList<ScheduleRow>& rows, const core::WritingPlan& plan,
                     const core::BookGoals& goals, const core::WritingRules& rules,
                     const QDate& today, QWidget* parent = nullptr);

    /// @brief The schedule with the texts planned as the fields say
    [[nodiscard]] core::WritingPlan plan() const;

    /// @brief The ids of the texts @p texts plans, in the order of @p rows
    [[nodiscard]] static QStringList textsToPlan(const QList<ScheduleRow>& rows,
                                                 const core::WritingPlan& plan, Texts texts);

    /// @brief @p plan with the bars of writing of @p texts from @p from to @p to; a text
    ///        without a length takes @p defaultLength; unchanged when there is no writing day
    ///        between the days
    [[nodiscard]] static core::WritingPlan spread(const QList<ScheduleRow>& rows,
                                                  const core::WritingPlan& plan, Texts texts,
                                                  const QDate& from, const QDate& to,
                                                  int defaultLength,
                                                  const core::WritingRules& rules);

    /// @brief The length of a text without its own when the window opens: the length of the
    ///        book over its texts, rounded to a hundred; without a length of the book, 3000
    [[nodiscard]] static int defaultLengthOf(const QList<ScheduleRow>& rows,
                                             const core::BookGoals& goals);

private:
    [[nodiscard]] Texts chosenTexts() const;

    /// @brief The last day not before the first; the note and the accept button
    void updateFields();

    QList<ScheduleRow> m_rows;
    core::WritingPlan m_plan;
    core::WritingRules m_rules;
    QRadioButton* m_withoutPlan = nullptr;
    QRadioButton* m_beingWritten = nullptr;
    QDateEdit* m_from = nullptr;
    QDateEdit* m_to = nullptr;
    QSpinBox* m_length = nullptr;
    QLabel* m_note = nullptr;
};

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_SPREAD_PLAN_DIALOG_H
