/// @file status_bar_writing.h
/// @brief The time of writing today and the days in a row, in the status bar

#pragma once

#include <QObject>
#include <QPointer>

class QLabel;
class QStatusBar;

namespace kalahari::gui {

class WritingStatsModel;

/// @brief Two labels on the right of the status bar, after the reading time: a clock with the
/// time of writing today and a flame with the days in a row
///
/// They follow the statistics, and show as the setting stats.statusBar says.
class StatusBarWriting : public QObject {
    Q_OBJECT

public:
    /// @brief The setting that shows them
    static constexpr const char* SETTING = "stats.statusBar";

    /// @brief Add the labels to the right of @p statusBar
    StatusBarWriting(QStatusBar* statusBar, WritingStatsModel* model, QObject* parent = nullptr);
    ~StatusBarWriting() override;

    /// @brief The label of the time of writing
    QLabel* timeLabel() const { return m_time; }

    /// @brief The label of the days in a row
    QLabel* streakLabel() const { return m_streak; }

    /// @brief Show what the statistics have now, as the setting says
    void update();

private:
    QPointer<WritingStatsModel> m_model;
    QLabel* m_time = nullptr;
    QLabel* m_streak = nullptr;
    int m_settingsListener = -1;
};

}  // namespace kalahari::gui
