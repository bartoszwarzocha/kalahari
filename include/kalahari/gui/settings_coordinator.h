/// @file settings_coordinator.h
/// @brief Settings dialog coordination for MainWindow
///
/// OpenSpec #00038 - Phase 5: Extract Settings Management from MainWindow
/// This class manages settings dialog integration and settings application.

#pragma once

#include <QObject>
#include <functional>

class QMainWindow;
class QStatusBar;

namespace kalahari {
namespace gui {

struct SettingsData;
class DashboardPanel;
class LogPanel;
class DockCoordinator;

/// @brief Coordinates settings dialog and application
///
/// Manages:
/// - Opening settings dialog
/// - Collecting current settings
/// - Applying changed settings
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

    /// @brief Collect current application settings into SettingsData
    /// @return Current settings from SettingsManager and runtime state
    [[nodiscard]] SettingsData collectCurrentSettings() const;

    /// @brief Set callback for checking diagnostic mode
    /// @param callback Function returning current diagnostic mode state
    void setDiagnosticModeGetter(std::function<bool()> callback);

public slots:
    /// @brief Apply settings from dialog
    /// @param settings Settings data to apply
    /// @param fromOkButton true if triggered by OK (dialog closed), false if Apply
    void onApplySettings(const SettingsData& settings, bool fromOkButton);

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
};

} // namespace gui
} // namespace kalahari
