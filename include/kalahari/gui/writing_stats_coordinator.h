/// @file writing_stats_coordinator.h
/// @brief The statistics and goals of writing in the main window: the statistics bar above the
/// tabs of the texts, its commands, the badges and challenges, the messages at the bottom and
/// the record of the open book

#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class QStatusBar;
class QTimer;
class QWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core
namespace kalahari::gui {

class CommandRegistry;

class DockCoordinator;
class DocumentCoordinator;
class InfoBar;
class StatusBarWriting;
class WritingAwardsTab;
class WritingAwardsTracker;
class WritingGoalsTab;
class WritingStatsBar;
class WritingStatsModel;

/// @brief The statistics and goals of writing in the main window
///
/// Keeps the writer's journal open (WritingJournal::defaultPath()) and gives the model the
/// record of the book that opens, and the editor's collector that records the writing. Puts the
/// statistics bar on top of the middle of the window, above the tabs of the texts, shown as
/// the setting "stats.bar.show" says; View > Show Statistics Bar and its button on the Writing
/// Statistics toolbar turn that setting on and off. Tools > Writing Goals and Deadlines opens
/// the tab of the goals in the middle of the window, beside the tabs of the texts, and Tools >
/// Challenges and Badges the tab of the badges. The goals reached, the deadlines coming, the
/// badges won and the challenges done are said in a bar at the bottom of the middle of the
/// window, with a button that copies the message, as the settings stats.notify.* say.
class WritingStatsCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief The setting that shows the statistics bar
    static constexpr const char* SHOW_BAR_SETTING = "stats.bar.show";
    /// @brief The page of the Settings dialog that the bar's gear opens
    static constexpr const char* SETTINGS_PAGE = "stats.bar";
    /// @brief The page that Statistics Settings on the tab of the goals opens: how the words
    /// count toward them
    static constexpr const char* GOALS_SETTINGS_PAGE = "stats.counting";

    /// @param book The book of its window
    /// @param commands The commands of its window
    /// @param docks The window's docks and its middle (centralWrapper())
    /// @param documents Its collector records the writing of the book that is open
    /// @param openSettings Opens the Settings dialog at a page (SettingsDialog::openPage())
    /// @param journalPath The writer's journal; empty: WritingJournal::defaultPath()
    WritingStatsCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                            DockCoordinator* docks, DocumentCoordinator* documents,
                            std::function<void(const QString&)> openSettings,
                            QObject* parent = nullptr, const QString& journalPath = QString());
    ~WritingStatsCoordinator() override;

    /// @brief Hook the commands up: View > Show Statistics Bar, Tools > Writing Goals and
    /// Deadlines, Tools > Challenges and Badges, Tools > Add Challenge, Tools > Statistics
    /// Settings
    void connectCommands();

    /// @brief Bring the tab Writing Goals and Deadlines to the front, made when it is not open
    /// @return The tab; nullptr without the middle of a window
    WritingGoalsTab* openGoals();

    /// @brief Bring the tab Challenges and Badges to the front, made when it is not open
    /// @return The tab; nullptr without the middle of a window
    WritingAwardsTab* openAwards();

    /// @brief Add a challenge, in its window, and show it on the tab Challenges and Badges
    /// @return Its number; 0 when none was added
    int addChallenge();

    /// @brief The model of the statistics
    WritingStatsModel* model() const { return m_model; }

    /// @brief The badges and the challenges
    WritingAwardsTracker* tracker() const { return m_tracker; }

    /// @brief The statistics bar
    WritingStatsBar* bar() const { return m_bar; }

    /// @brief The bar of the messages at the bottom of the middle of the window
    InfoBar* messageBar() const { return m_messageBar; }

    /// @brief Say @p message in the bar at the bottom, with the others of the same moment
    void showMessage(const QString& message);

    /// @brief Put the time of writing and the days in a row in @p statusBar, as the setting
    /// stats.statusBar says
    void attachStatusBar(QStatusBar* statusBar);

    /// @brief The time of writing and the days in a row in the status bar; nullptr before
    /// attachStatusBar()
    StatusBarWriting* statusBarWriting() const { return m_statusBarWriting; }

    /// @brief The window over which the summary of the session shows; without one it does not
    void setSummaryWindow(QWidget* window);

    /// @brief The summary of the session, when the setting stats.sessionSummary says so and
    /// something was written since the book opened; once for each session
    void showSessionSummary();

    /// @brief Turn the statistics bar on or off (the setting SHOW_BAR_SETTING)
    void toggleBar();

private slots:
    /// @brief Show or hide the bar as the setting says (called by name from the listener of
    /// the settings)
    void applyBarVisibility();

private:
    /// @brief A book was opened: its record, its goals and the collector of its editors
    void onProjectOpened();
    /// @brief The book is closing: the summary of the session, then the record goes on in the
    /// journal only
    void onProjectClosing();
    /// @brief The messages of the tracker, as the settings say
    void connectMessages();
    /// @brief Show the messages of this moment in the bar
    void flushMessages();

    DockCoordinator* m_docks{nullptr};
    DocumentCoordinator* m_documents{nullptr};
    std::function<void(const QString&)> m_openSettings;
    WritingStatsModel* m_model{nullptr};
    WritingAwardsTracker* m_tracker{nullptr};
    WritingStatsBar* m_bar{nullptr};
    InfoBar* m_messageBar{nullptr};
    QTimer* m_messageTimer{nullptr};
    QStringList m_pendingMessages;
    StatusBarWriting* m_statusBarWriting{nullptr};
    // Plain, cleared when the window goes: clang-tidy's analyzer reports a QPointer as a use
    // of freed memory
    QWidget* m_summaryWindow{nullptr};
    QDateTime m_sessionStart;     ///< The session started then: the book opened
    bool m_summaryShown{false};   ///< The summary of this session was shown
    int m_settingsListener{-1};
    core::ProjectManager& m_book;  ///< The book of its window
    CommandRegistry& m_commands;  ///< The commands of its window
};

}  // namespace kalahari::gui
