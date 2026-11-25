// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file length_meter.h
/// @brief The length of a text against its target: its counts, the target and a bar

#ifndef KALAHARI_GUI_WIDGETS_LENGTH_METER_H
#define KALAHARI_GUI_WIDGETS_LENGTH_METER_H

#include <QWidget>

class QLabel;
class QProgressBar;

namespace kalahari::gui {

/// @brief The characters with spaces and the words of a text, its target length and a bar of
/// the length against it; past the target the bar and its words take the warning color of the
/// theme. A synopsis also has its standard pages.
class LengthMeter : public QWidget {
    Q_OBJECT

public:
    /// @param parent Its parent
    explicit LengthMeter(QWidget* parent = nullptr);

    /// @brief Show the length of a text of @p characters characters with spaces and @p words
    /// words against target @p target in the unit @p inWords (0: no target, without the bar)
    /// @param pages Whether the standard pages are shown too
    void setLength(int characters, int words, int target, bool inWords, bool pages = false);

    /// @brief The counts: "320 characters with spaces · 50 words", with the pages
    [[nodiscard]] QLabel* countsLabel() const { return m_counts; }

    /// @brief The target: "target: up to 600 characters", "12 characters too many"
    [[nodiscard]] QLabel* targetLabel() const { return m_target; }

    /// @brief The bar of the length against the target
    [[nodiscard]] QProgressBar* bar() const { return m_bar; }

private:
    QLabel* m_counts = nullptr;
    QLabel* m_target = nullptr;
    QProgressBar* m_bar = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_LENGTH_METER_H
