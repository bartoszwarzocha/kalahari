/// @file writing_goals_tab.h
/// @brief The tab Writing Goals and Deadlines in the middle of the window: the daily and weekly
/// goal of the writer, the length and the deadline of the book, and the deadlines of its parts
///
/// The daily goal, the writing days and the weekly goal are the writer's own, the same for all
/// the books (the settings stats.dailyGoal, stats.writingDays and stats.weeklyGoal); the length
/// of the book, its deadline and the deadlines of its parts are the book's own (BookGoals, in
/// its project.db). A change works at once: there is no OK or Apply.

#pragma once

#include "kalahari/gui/dialogs/deadline_dialog.h"

#include <QList>
#include <QString>
#include <QWidget>

#include <array>
#include <functional>

class QBoxLayout;
class QCheckBox;
class QDateEdit;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QToolButton;
class QTreeWidget;

namespace kalahari::gui {

class WritingStatsModel;

/// @brief The tab Writing Goals and Deadlines (Tools > Writing Goals and Deadlines)
///
/// It shows what the model has and follows it: a goal changed in another place (Settings, a
/// book opened or closed) shows here at once. Without a book the goals of the book are off.
class WritingGoalsTab : public QWidget {
    Q_OBJECT

public:
    /// @brief The icon of the tab (ArtProvider)
    static constexpr const char* ICON_ID = "tools.writingGoals";

    /// @brief The book the goals are about, as the program has it now
    struct BookSource {
        std::function<QString()> title;                       ///< Its title
        std::function<QList<dialogs::BookPart>()> parts;      ///< Its parts, in reading order
        std::function<int(const QString& elementId)> words;   ///< The words of a part now
    };

    /// @param model The statistics and the goals; it outlives the tab
    explicit WritingGoalsTab(WritingStatsModel* model, QWidget* parent = nullptr);

    /// @brief Where the title of the book, its parts and their words come from
    void setBookSource(BookSource source);

    /// @brief The title of the tab
    static QString tabTitle();

    /// @brief Show what the model has now
    void refresh();

    /// @brief Add a deadline, in its window
    void addDeadline();

    /// @brief Change the chosen deadline, in its window
    void changeDeadline();

    /// @brief Remove the chosen deadline, after asking
    void removeDeadline();

signals:
    /// @brief Statistics Settings was pressed: Settings > Statistics and Goals > Counting
    void settingsRequested();

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The texts in the language of the program
    void retranslate();

    /// @brief The colors of the notes in the theme
    void updateColors();

    /// @brief The two columns side by side when they fit, else one under the other
    void updateColumns();

    /// @brief The words of the book, how many are left and what the deadline needs
    void showCalculation();

    /// @brief The list of the deadlines, with the one chosen before
    void showDeadlines();

    /// @brief Change and Remove only with a deadline chosen
    void updateDeadlineButtons();

    /// @brief Keep @p goals in the book
    void storeGoals(const core::BookGoals& goals);

    /// @brief The writing days of the buttons, as the setting keeps them
    int writingDaysOfButtons() const;

    /// @brief "%1 words" in the writer's language
    static QString words(qint64 count);

    WritingStatsModel* m_model = nullptr;
    BookSource m_source;

    QLabel* m_title = nullptr;
    QLabel* m_intro = nullptr;
    QBoxLayout* m_columns = nullptr;
    QWidget* m_left = nullptr;
    QWidget* m_right = nullptr;

    QGroupBox* m_dailyGroup = nullptr;
    QLabel* m_dailyLabel = nullptr;
    QSpinBox* m_dailyGoal = nullptr;
    QLabel* m_dailyWords = nullptr;
    QLabel* m_daysLabel = nullptr;
    std::array<QToolButton*, 7> m_dayButtons{};
    QLabel* m_daysNote = nullptr;
    QCheckBox* m_fromDeadline = nullptr;
    QLabel* m_fromDeadlineNote = nullptr;

    QGroupBox* m_weeklyGroup = nullptr;
    QRadioButton* m_weeklySum = nullptr;
    QRadioButton* m_weeklyOwn = nullptr;
    QSpinBox* m_weeklyGoal = nullptr;
    QLabel* m_weeklyWords = nullptr;

    QGroupBox* m_bookGroup = nullptr;
    QLabel* m_lengthLabel = nullptr;
    QSpinBox* m_length = nullptr;
    QLabel* m_lengthWords = nullptr;
    QCheckBox* m_hasDeadline = nullptr;
    QDateEdit* m_deadline = nullptr;
    QLabel* m_calculation = nullptr;

    QGroupBox* m_deadlinesGroup = nullptr;
    QTreeWidget* m_deadlines = nullptr;
    QPushButton* m_add = nullptr;
    QPushButton* m_change = nullptr;
    QPushButton* m_remove = nullptr;
    QLabel* m_deadlinesNote = nullptr;

    QPushButton* m_settings = nullptr;
    QList<QLabel*> m_notes;
};

}  // namespace kalahari::gui
