// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file text_plan_dialog.h
/// @brief The small window of the plan of a text, on the tab Writing Schedule: the days of
/// writing, the days of revisions and the length of the text

#ifndef KALAHARI_GUI_DIALOGS_TEXT_PLAN_DIALOG_H
#define KALAHARI_GUI_DIALOGS_TEXT_PLAN_DIALOG_H

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <kalahari/core/writing_plan.h>
#include <kalahari/core/writing_statistics.h>

#include <QDate>
#include <QString>

class QCheckBox;
class QDateEdit;
class QLabel;
class QSpinBox;

namespace kalahari::gui::dialogs {

/// @brief The plan of a text: when it is written, when it is revised, how long it should be
///
/// Each of the two stages is planned or not; a stage planned has its first and its last day.
/// The note under the fields says how many writing days that is and what a day needs. Enter
/// keeps the plan, Remove Plan takes the text out of the schedule, Esc gives up.
class TextPlanDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param title The title of the text
    /// @param plan The plan the fields start with; without one, writing from @p today for two
    ///        weeks
    /// @param words The words of the text now
    /// @param rules The writing days of the writer
    /// @param today The writing day of now
    /// @param parent Parent widget
    TextPlanDialog(const QString& title, const core::TextPlan& plan, int words,
                   const core::WritingRules& rules, const QDate& today,
                   QWidget* parent = nullptr);

    /// @brief The plan of the fields; empty after Remove Plan
    [[nodiscard]] core::TextPlan plan() const;

    /// @brief Remove Plan was pressed
    [[nodiscard]] bool isRemoved() const { return m_removed; }

private:
    /// @brief The fields of a stage on only when it is planned; the end not before the start
    void updateFields();

    /// @brief The writing days and the words a day needs
    void updateNote();

    QString m_elementId;
    int m_words = 0;
    core::WritingRules m_rules;
    QDate m_today;
    bool m_removed = false;

    QCheckBox* m_writing = nullptr;
    QDateEdit* m_start = nullptr;
    QDateEdit* m_end = nullptr;
    QCheckBox* m_revision = nullptr;
    QDateEdit* m_revisionStart = nullptr;
    QDateEdit* m_revisionEnd = nullptr;
    QSpinBox* m_length = nullptr;
    QLabel* m_note = nullptr;
};

} // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_TEXT_PLAN_DIALOG_H
