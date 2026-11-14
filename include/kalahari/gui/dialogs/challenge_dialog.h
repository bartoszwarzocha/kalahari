// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file challenge_dialog.h
/// @brief The small window of Add Challenge: a goal of the writer's own for a time, e.g.
/// 50,000 words in a month

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <kalahari/core/writing_awards.h>

#include <QDate>

class QComboBox;
class QDateEdit;
class QLabel;
class QSpinBox;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief Add Challenge: what to reach (words, days in a row or writing days), how much of it
/// and by which day
///
/// The challenge counts from today to the end of this week, of this month or to a day of the
/// writer's choice. The days of a challenge of days cannot be more than the days it has. Enter
/// adds it, Esc gives up.
class ChallengeDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param today The writing day of now: the first day of the challenge
    /// @param parent Parent widget
    explicit ChallengeDialog(const QDate& today, QWidget* parent = nullptr);

    /// @brief The challenge of the fields (without its number)
    core::WritingChallenge challenge() const;

    /// @brief The last day of the challenge, as chosen
    QDate endDay() const;

private:
    /// @brief The unit and the range of the target, as the kind needs them
    void updateTarget();
    /// @brief The day of the writer's choice only with that choice
    void updateEnd();
    /// @brief Add only with a target that the days of the challenge can reach
    void updateAcceptButton();

    QDate m_today;
    QComboBox* m_kindBox = nullptr;
    QSpinBox* m_targetBox = nullptr;
    QComboBox* m_endBox = nullptr;
    QDateEdit* m_endEdit = nullptr;
    QLabel* m_note = nullptr;
    core::ChallengeKind m_shownKind = core::ChallengeKind::Words;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
