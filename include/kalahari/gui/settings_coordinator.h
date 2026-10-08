/// @file settings_coordinator.h
/// @brief Settings dialog coordination for MainWindow
///
/// OpenSpec #00038 - Phase 5: Extract Settings Management from MainWindow
/// This class manages settings dialog integration and settings application.

#pragma once

#include <QObject>
#include <QStringList>
#include <functional>

class QMainWindow;
class QStatusBar;

namespace kalahari {
namespace gui {

class DashboardPanel;
class LogPanel;
class DockCoordinator;

/// @brief Coordinates settings dialog and application
///
/// Manages:
/// - Opening settings dialog
/// - Refreshing the panels the changed settings affect
/// - Emitting signals for diagnostic mode, log buffer, dashboard refresh
///
/// Example usage:
/// @code
/// auto coordinator = new SettingsCoordinator(this, dockCoordinator, statusBar(), this);
/// coordinator->openSettingsDialog();
/// @endcode
class SettingsCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param mainWindow Parent QMainWindow
    /// @param dockCoordinator Dock coordinator for panel access
    /// @param statusBar Status bar for feedback messages
    /// @param parent Parent QObject
    explicit SettingsCoordinator(QMainWindow* mainWindow,
                                  DockCoordinator* dockCoordinator,
                                  QStatusBar* statusBar,
                                  QObject* parent = nullptr);

    /// @brief Destructor
    ~SettingsCoordinator() override = default;

    /// @brief Open the settings dialog
    /// @note Creates modal SettingsDialog, connects signals, handles OK/Cancel
    void openSettingsDialog();

    /// @brief Set callback for checking diagnostic mode
    /// @param callback Function returning current diagnostic mode state
    void setDiagnosticModeGetter(std::function<bool()> callback);

public slots:
    /// @brief React to settings applied by the dialog, refreshing only what changed
    /// @param changedKeys Keys of the settings written
    void onApplySettings(const QStringList& changedKeys);

signals:
    /// @brief Emitted when diagnostic mode should be enabled
    void enableDiagnosticModeRequested();

    /// @brief Emitted when diagnostic mode should be disabled
    void disableDiagnosticModeRequested();

private:
    QMainWindow* m_mainWindow;
    DockCoordinator* m_dockCoordinator;
    QStatusBar* m_statusBar;
    std::function<bool()> m_diagnosticModeGetter;
    bool m_languageChanged = false;  ///< UI language changed while the dialog was open

    /// @brief Offer to restart Kalahari so a new UI language takes effect
    void offerRestartForLanguage();
};

} // namespace gui
} // namespace kalahari
