/// @file main_window.h
/// @brief Main application window (Qt6 QMainWindow subclass)
///
/// This file defines the MainWindow class, which is the primary GUI window
/// for Kalahari Writer's IDE. It manages menus, toolbars, status bar, and
/// dockable panels.

#pragma once

#include <QMainWindow>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <QTabWidget>
#include <optional>
#include <filesystem>
#include "kalahari/core/document.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/toolbar_manager.h"

class QDockWidget;
class QCloseEvent;
class QShowEvent;

namespace kalahari {

namespace gui {

// Forward declarations for panels
class DashboardPanel;
class EditorPanel;
class NavigatorPanel;
class PropertiesPanel;
class SearchPanel;
class AssistantPanel;
class LogPanel;

/// @brief Main application window
///
/// MainWindow is the top-level QMainWindow subclass for Kalahari.
/// It provides:
/// - File and Edit menus
/// - Toolbar with common actions
/// - Status bar
/// - Signal/slot connections for actions
///
/// Example usage:
/// @code
/// QApplication app(argc, argv);
/// MainWindow window;
/// window.show();
/// return app.exec();
/// @endcode
class MainWindow : public QMainWindow {
    Q_OBJECT  // Required for signals/slots!

public:
    /// @brief Constructor
    /// @param parent Parent widget (nullptr for top-level window)
    explicit MainWindow(QWidget* parent = nullptr);

    /// @brief Destructor
    ~MainWindow() override = default;

    /// @brief Enable diagnostic mode (show Diagnostics menu)
    void enableDiagnosticMode();

    /// @brief Disable diagnostic mode (hide Diagnostics menu)
    void disableDiagnosticMode();

    /// @brief Check if diagnostic mode is enabled
    /// @return true if diagnostic mode is active, false otherwise
    bool isDiagnosticMode() const { return m_diagnosticMode; }

private:
    /// @brief Register all commands in CommandRegistry
    ///
    /// Registers core commands (File, Edit, Help) with:
    /// - Command IDs (e.g., "file.new", "edit.undo")
    /// - Display names and tooltips
    /// - Keyboard shortcuts
    /// - Execute callbacks
    /// - Enable/disable state callbacks
    /// @note Must be called BEFORE createMenus() and createToolbars()
    void registerCommands();

    /// @brief Create menu bar from CommandRegistry
    ///
    /// Uses MenuBuilder to dynamically build menus from registered commands.
    void createMenus();

    /// @brief Create main toolbar from CommandRegistry
    ///
    /// Uses ToolbarBuilder to dynamically build toolbar from registered commands.
    void createToolbars();

    /// @brief Create status bar
    ///
    /// Shows "Ready" message on application start.
    void createStatusBar();

    /// @brief Create dockable panels
    ///
    /// Creates 6 dock widgets and sets up default layout.
    void createDocks();

    /// @brief Reset dock layout to default
    void resetLayout();

    /// @brief Create diagnostic menu (Task #00018)
    /// @note Only called when diagnostic mode is enabled
    void createDiagnosticMenu();

    /// @brief Remove diagnostic menu (Task #00018)
    void removeDiagnosticMenu();

protected:
    /// @brief Save perspective on close
    /// @param event Close event
    void closeEvent(QCloseEvent* event) override;

    /// @brief Restore perspective on show
    /// @param event Show event
    void showEvent(QShowEvent* event) override;

private slots:
    /// @brief Slot for File > New action
    void onNewDocument();

    /// @brief Slot for File > Open action
    void onOpenDocument();

    /// @brief Slot for File > Save action
    void onSaveDocument();

    /// @brief Slot for File > Save As action
    void onSaveAsDocument();

    /// @brief Slot for File > Exit action
    void onExit();

    /// @brief Slot for Edit > Undo action
    void onUndo();

    /// @brief Slot for Edit > Redo action
    void onRedo();

    /// @brief Slot for Edit > Cut action
    void onCut();

    /// @brief Slot for Edit > Copy action
    void onCopy();

    /// @brief Slot for Edit > Paste action
    void onPaste();

    /// @brief Slot for Edit > Select All action
    void onSelectAll();

    /// @brief Slot for Edit > Settings action
    void onSettings();

    /// @brief Slot for Help > About action
    void onAbout();

    /// @brief Slot for Help > About Qt action
    void onAboutQt();

    /// @brief Slot for Navigator double-click (Task #00015)
    /// @param chapterTitle Title of the chapter/item that was double-clicked
    /// @note Phase 0: Opens whole document in new editor tab
    /// @note Phase 1+: Opens specific chapter content
    void onNavigatorItemDoubleClicked(const QString& chapterTitle);

    /// @brief Slot for diagnostic mode changed from SettingsDialog (Task #00018)
    /// @param enabled true if diagnostic mode enabled, false otherwise
    void onDiagModeChanged(bool enabled);

    // Diagnostic tool slots (Task #00018) - only visible in diagnostic mode
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

private:
    // Actions removed - now managed by CommandRegistry
    // All actions are dynamically created from Command structs

    // Menus
    QMenu* m_fileMenu;
    QMenu* m_editMenu;
    QMenu* m_viewMenu;
    QMenu* m_helpMenu;

    // Toolbars (Task #00019)
    ToolbarManager* m_toolbarManager;

    // View actions (panel toggles)
    QAction* m_viewNavigatorAction;
    QAction* m_viewPropertiesAction;
    QAction* m_viewLogAction;
    QAction* m_viewSearchAction;
    QAction* m_viewAssistantAction;
    QAction* m_resetLayoutAction;

    // Dock widgets
    QDockWidget* m_navigatorDock;
    QDockWidget* m_propertiesDock;
    QDockWidget* m_logDock;
    QDockWidget* m_searchDock;
    QDockWidget* m_assistantDock;

    // Central tabbed workspace (Task #00015)
    QTabWidget* m_centralTabs;        ///< Central tabbed workspace container
    DashboardPanel* m_dashboardPanel; ///< Welcome/Dashboard panel (default first tab)

    // Panels (widgets inside docks or tabs)
    EditorPanel* m_editorPanel;       ///< DEPRECATED: Use getCurrentEditor() instead (Task #00015)
    NavigatorPanel* m_navigatorPanel;
    PropertiesPanel* m_propertiesPanel;
    SearchPanel* m_searchPanel;
    AssistantPanel* m_assistantPanel;
    LogPanel* m_logPanel;

    // First show flag (for geometry restore)
    bool m_firstShow;

    // Diagnostic mode (Task #00018)
    bool m_diagnosticMode;      ///< Diagnostic mode enabled flag
    QMenu* m_diagnosticMenu;    ///< Diagnostics menu (only visible when m_diagnosticMode=true)

    // Document management (Task #00008 - Phase 0)
    std::optional<core::Document> m_currentDocument;  ///< Current loaded document
    std::filesystem::path m_currentFilePath;          ///< Current .klh file path
    bool m_isDirty;                                   ///< Unsaved changes flag

    /// @brief Mark document as modified (add "*" to title)
    void setDirty(bool dirty);

    /// @brief Update window title with filename and dirty state
    void updateWindowTitle();

    /// @brief Get currently active EditorPanel tab (Task #00015)
    /// @return Active EditorPanel if current tab is an editor, nullptr otherwise
    /// @note Returns nullptr if current tab is Dashboard or other panel type
    EditorPanel* getCurrentEditor();

    /// @brief Get text from first chapter metadata (Phase 0 temporary hack)
    /// @param doc Document to extract text from
    /// @return Editor text content, or empty string if no content
    QString getPhase0Content(const core::Document& doc) const;

    /// @brief Set text in first chapter metadata (Phase 0 temporary hack)
    /// @param doc Document to update
    /// @param text Editor text content
    void setPhase0Content(core::Document& doc, const QString& text);
};

} // namespace gui
} // namespace kalahari
