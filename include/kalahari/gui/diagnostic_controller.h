/// @file diagnostic_controller.h
/// @brief Controller for diagnostic and dev mode functionality
///
/// Extracted from MainWindow as part of OpenSpec #00038 refactoring.
/// Manages diagnostic menu, dev tools menu, and all related functionality.

#pragma once

#include <QObject>

class QMainWindow;
class QMenu;
class QMenuBar;
class QDockWidget;
class QStatusBar;

namespace kalahari {
namespace gui {

class LogPanel;

/// @brief Controller for diagnostic and dev mode functionality
///
/// Manages:
/// - Diagnostic mode toggle and menu
/// - Dev mode toggle and menu
/// - All diagnostic tool actions
/// - Log panel visibility in diagnostic/dev modes
///
/// This controller was extracted from MainWindow to reduce the god object
/// and improve maintainability.
class DiagnosticController : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param mainWindow Parent main window (for menu bar access)
    /// @param logPanel Log panel instance (for diagnostic mode)
    /// @param logDock Log dock widget (for visibility control)
    /// @param statusBar Status bar (for status messages)
    /// @param parent Parent object
    explicit DiagnosticController(QMainWindow* mainWindow,
                                   LogPanel* logPanel,
                                   QDockWidget* logDock,
                                   QStatusBar* statusBar,
                                   QObject* parent = nullptr);

    /// @brief Destructor
    ~DiagnosticController() override = default;

    // =========================================================================
    // Mode Management (Public API for backward compatibility)
    // =========================================================================

    /// @brief Enable diagnostic mode (show Diagnostics menu)
    void enableDiagnosticMode();

    /// @brief Disable diagnostic mode (hide Diagnostics menu)
    void disableDiagnosticMode();

    /// @brief Check if diagnostic mode is enabled
    /// @return true if diagnostic mode is active, false otherwise
    [[nodiscard]] bool isDiagnosticMode() const { return m_diagnosticMode; }

    /// @brief Enable dev mode (show Dev Tools menu)
    void enableDevMode();

    /// @brief Disable dev mode (hide Dev Tools menu)
    void disableDevMode();

    /// @brief Check if dev mode is enabled
    /// @return true if dev mode is active, false otherwise
    [[nodiscard]] bool isDevMode() const { return m_devMode; }

public slots:
    /// @brief Slot for diagnostic mode changed from SettingsDialog
    /// @param enabled true if diagnostic mode enabled, false otherwise
    void onDiagModeChanged(bool enabled);

    // =========================================================================
    // Diagnostic Tools
    // =========================================================================

    void onDiagSystemInfo();
    void onDiagQtEnvironment();
    void onDiagFileSystemCheck();
    void onDiagSettingsDump();
    void onDiagMemoryStats();
    void onDiagOpenDocsStats();
    void onDiagLoggerTest();
    void onDiagEventBusTest();
    void onDiagPluginCheck();
    void onDiagCommandRegistryDump();
    void onDiagPythonEnvironment();
    void onDiagPythonImportTest();
    void onDiagPythonMemoryTest();
    void onDiagEmbeddedInterpreterStatus();
    void onDiagPerformanceBenchmark();
    void onDiagRenderStats();
    void onDiagClearLog();

#ifdef _DEBUG
    void onDiagForceCrash();
    void onDiagMemoryLeakTest();
#endif

    // =========================================================================
    // Dev Tools
    // =========================================================================

    void onDevToolsIconDownloader();

signals:
    /// @brief Emitted when diagnostic mode changes
    /// @param enabled New diagnostic mode state
    void diagnosticModeChanged(bool enabled);

    /// @brief Emitted when dev mode changes
    /// @param enabled New dev mode state
    void devModeChanged(bool enabled);

private:
    /// @brief Create diagnostic menu
    /// @note Only called when diagnostic mode is enabled
    void createDiagnosticMenu();

    /// @brief Remove diagnostic menu
    void removeDiagnosticMenu();

    /// @brief Create dev tools menu
    /// @note Only called when dev mode is enabled
    void createDevToolsMenu();

    /// @brief Remove dev tools menu
    void removeDevToolsMenu();

    // =========================================================================
    // Members
    // =========================================================================

    QMainWindow* m_mainWindow;      ///< Parent main window
    LogPanel* m_logPanel;           ///< Log panel (for diagnostic mode)
    QDockWidget* m_logDock;         ///< Log dock widget (for visibility)
    QStatusBar* m_statusBar;        ///< Status bar (for messages)

    bool m_diagnosticMode{false};   ///< Diagnostic mode enabled flag
    bool m_devMode{false};          ///< Dev mode enabled flag

    QMenu* m_diagnosticMenu{nullptr};  ///< Diagnostics menu
    QMenu* m_devToolsMenu{nullptr};    ///< Dev Tools menu
};

} // namespace gui
} // namespace kalahari
