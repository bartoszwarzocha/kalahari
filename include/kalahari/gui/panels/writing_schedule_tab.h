// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file writing_schedule_tab.h
/// @brief The tab Writing Schedule in the middle of the window: the texts of the book on a
/// time axis, when each is written and when it is revised, next to the goals and the
/// statistics of writing
///
/// The plan is the book's own (core::WritingPlan, in its project.db, each volume of a series
/// its own). A change works at once: there is no OK or Apply; Edit > Undo takes it back.

#ifndef KALAHARI_GUI_PANELS_WRITING_SCHEDULE_TAB_H
#define KALAHARI_GUI_PANELS_WRITING_SCHEDULE_TAB_H

#include "kalahari/gui/edit_target.h"
#include "kalahari/gui/widgets/schedule_view.h"

#include <QList>
#include <QString>
#include <QWidget>

#include <functional>

class QCheckBox;
class QLabel;
class QPushButton;
class QToolButton;

namespace kalahari::gui {

class WritingStatsModel;

/// @brief The tab Writing Schedule (Tools > Writing Schedule)
///
/// It follows the model: the plan, the goals and the record of writing of the open book, a
/// book opened or closed. Edit > Undo, Redo and Delete work on its plan (EditTarget).
class WritingScheduleTab : public QWidget, public EditTarget {
    Q_OBJECT

public:
    /// @brief The icon of the tab and of its command (ArtProvider)
    static constexpr const char* ICON_ID = "tools.writingSchedule";

    /// @brief The scale shown last: "days", "weeks" or "months"
    static constexpr const char* SCALE_SETTING = "stats.schedule.scale";

    /// @brief Whether the days off are shaded
    static constexpr const char* DAYS_OFF_SETTING = "stats.schedule.showDaysOff";

    /// @brief Changes of the plan Undo can take back
    static constexpr int UNDO_LIMIT = 100;

    /// @param model The statistics, the goals and the plan; it outlives the tab
    explicit WritingScheduleTab(WritingStatsModel* model, QWidget* parent = nullptr);

    /// @brief Where the rows of the chart come from: the book, its parts and texts now
    void setRowsSource(std::function<QList<ScheduleRow>()> source);

    /// @brief The title of the tab
    static QString tabTitle();

    /// @brief Show what the book and the model have now
    void refresh();

    /// @brief The chart
    [[nodiscard]] ScheduleView* view() const { return m_view; }

    /// @brief The plan of @p elementId in its window
    void editPlan(const QString& elementId);

    /// @brief Plan the Texts...: the writing of many texts at once (dialogs::SpreadPlanDialog),
    ///        one step to undo
    void spreadPlan();

    /// @brief Undo and Redo of the changes of the plan
    [[nodiscard]] bool canUndo() const { return !m_undo.isEmpty(); }
    [[nodiscard]] bool canRedo() const { return !m_redo.isEmpty(); }
    void undo();
    void redo();

    // EditTarget
    [[nodiscard]] bool takesEdit(EditCommand command) const override;
    [[nodiscard]] bool canEditHere(EditCommand command) const override;
    void editHere(EditCommand command) override;
    QMetaObject::Connection followEditStates(QObject* receiver,
                                             std::function<void()> changed) override;

signals:
    /// @brief Goals and Deadlines was pressed
    void goalsRequested();

    /// @brief The gear was pressed: Settings > Statistics and Goals > Colors
    void settingsRequested();

    /// @brief What Undo, Redo and Delete can do changed
    void editStatesChanged();

protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    /// @brief The texts in the language of the program
    void retranslate();

    /// @brief Today, the deadlines, the forecast and the days written, from the model
    void showFacts();

    /// @brief The line under the chart: what the chosen row has
    void showDetails();

    /// @brief Keep @p plan in the book, with the plan before it for Undo
    void storePlan(const core::WritingPlan& plan);

    /// @brief The plan of the model, as the book has it
    void takePlan();

    /// @brief The scale, kept for the next time
    void chooseScale(ScheduleView::Scale scale);

    /// @brief The book cannot change: open read-only, or none
    [[nodiscard]] bool isReadOnly() const;

    WritingStatsModel* m_model = nullptr;
    std::function<QList<ScheduleRow>()> m_rowsSource;
    ScheduleView* m_view = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_intro = nullptr;
    QLabel* m_details = nullptr;
    QToolButton* m_days = nullptr;
    QToolButton* m_weeks = nullptr;
    QToolButton* m_months = nullptr;
    QPushButton* m_today = nullptr;
    QCheckBox* m_daysOff = nullptr;
    QPushButton* m_spread = nullptr;
    QPushButton* m_goals = nullptr;
    QToolButton* m_settings = nullptr;
    QList<core::WritingPlan> m_undo;  ///< The plans before the changes, the last one last
    QList<core::WritingPlan> m_redo;  ///< The plans undone, the last one undone last
    bool m_storing = false;           ///< The plan changes in the model by this tab
    bool m_stale = true;              ///< Something changed while it was hidden
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_WRITING_SCHEDULE_TAB_H
