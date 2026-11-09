/// @file writing_stats_coordinator.h
/// @brief The statistics and goals of writing in the main window: the statistics bar above the
/// tabs of the texts, its commands and the record of the open book

#pragma once

#include <QObject>
#include <QString>

#include <functional>

class QWidget;

namespace kalahari::gui {

class DockCoordinator;
class DocumentCoordinator;
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
/// the tab of the goals in the middle of the window, beside the tabs of the texts.
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

    /// @param docks The window's docks and its middle (centralWrapper())
    /// @param documents Its collector records the writing of the book that is open
    /// @param openSettings Opens the Settings dialog at a page (SettingsDialog::openPage())
    /// @param journalPath The writer's journal; empty: WritingJournal::defaultPath()
    WritingStatsCoordinator(DockCoordinator* docks, DocumentCoordinator* documents,
                            std::function<void(const QString&)> openSettings,
                            QObject* parent = nullptr, const QString& journalPath = QString());
    ~WritingStatsCoordinator() override;

    /// @brief Hook the commands up: View > Show Statistics Bar, Tools > Writing Goals and
    /// Deadlines, Tools > Statistics Settings
    void connectCommands();

    /// @brief Bring the tab Writing Goals and Deadlines to the front, made when it is not open
    /// @return The tab; nullptr without the middle of a window
    WritingGoalsTab* openGoals();

    /// @brief The model of the statistics
    WritingStatsModel* model() const { return m_model; }

    /// @brief The statistics bar
    WritingStatsBar* bar() const { return m_bar; }

    /// @brief Turn the statistics bar on or off (the setting SHOW_BAR_SETTING)
    void toggleBar();

private:
    /// @brief A book was opened: its record, its goals and the collector of its editors
    void onProjectOpened();
    /// @brief The book is closing: the record goes on in the journal only
    void onProjectClosing();
    /// @brief Show or hide the bar as the setting says
    void applyBarVisibility();

    DockCoordinator* m_docks{nullptr};
    DocumentCoordinator* m_documents{nullptr};
    std::function<void(const QString&)> m_openSettings;
    WritingStatsModel* m_model{nullptr};
    WritingStatsBar* m_bar{nullptr};
    int m_settingsListener{-1};
};

}  // namespace kalahari::gui
