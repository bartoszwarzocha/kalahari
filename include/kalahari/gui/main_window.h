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
#include <QMap>
#include <optional>
#include <filesystem>
#include "kalahari/core/document.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/toolbar_manager.h"
#include "kalahari/gui/settings_data.h"
#include "kalahari/gui/widgets/standalone_info_bar.h"

class QDockWidget;
class QCloseEvent;
class QShowEvent;
class QLabel;
class QToolButton;

namespace kalahari {

namespace core {
    struct Theme;  // Forward declaration for Theme (Task #00023)
}

namespace gui {

// Forward declarations for panels
class DashboardPanel;
class EditorPanel;
class NavigatorPanel;
class PropertiesPanel;
class SearchPanel;
class AssistantPanel;
class LogPanel;
class MenuBuilder;      // Task #00025
class BusyIndicator;    // Reusable spinner overlay

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

    /// @brief Enable dev mode (show Dev Tools menu) - Task #00020
    void enableDevMode();

    /// @brief Disable dev mode (hide Dev Tools menu) - Task #00020
    void disableDevMode();

    /// @brief Check if dev mode is enabled - Task #00020
    /// @return true if dev mode is active, false otherwise
    bool isDevMode() const { return m_devMode; }

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

    /// @brief Create dev tools menu (Task #00020)
    /// @note Only called when dev mode is enabled
    void createDevToolsMenu();

    /// @brief Remove dev tools menu (Task #00020)
    void removeDevToolsMenu();

    /// @brief Setup custom title bar for dock widget (Task #00028)
    /// @param dock The dock widget to customize
    /// @param iconId Icon command ID (e.g., "view.navigator")
    /// @param title Translated title text
    /// @note Creates horizontal layout with icon label + title label + float/close buttons
    /// @note Stores icon label in m_dockIconLabels for theme refresh
    void setupDockTitleBar(QDockWidget* dock, const QString& iconId, const QString& title);

    /// @brief Refresh all dock title bar icons (Task #00028)
    /// @note Called when theme changes to update icon colors
    void refreshDockIcons();

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

    /// @brief Slot for File > New Project action (OpenSpec #00033)
    void onNewProject();

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

    /// @brief Slot for Navigator element selection (Task #00015, OpenSpec #00033)
    /// @param elementId Unique ID of the selected element (BookElement::getId())
    /// @param elementTitle Display title of the element
    /// @note Phase 0: Opens whole document in new editor tab
    /// @note Phase 1+: Opens specific element content based on elementId
    void onNavigatorElementSelected(const QString& elementId, const QString& elementTitle);

    /// @brief Slot for opening recent file (OpenSpec #00030)
    /// @param filePath Path to the file to open
    void onOpenRecentFile(const QString& filePath);

    /// @brief Slot for diagnostic mode changed from SettingsDialog (Task #00018)
    /// @param enabled true if diagnostic mode enabled, false otherwise
    void onDiagModeChanged(bool enabled);

    /// @brief Slot for theme changed (Task #00023)
    /// @param theme New theme to apply to IconRegistry
    void onThemeChanged(const kalahari::core::Theme& theme);

    /// @brief Slot for project opened (OpenSpec #00033 Phase D)
    /// @param projectPath Path to the opened project
    void onProjectOpened(const QString& projectPath);

    /// @brief Slot for project closed (OpenSpec #00033 Phase D)
    void onProjectClosed();

    /// @brief Slot for Save All action (OpenSpec #00033 Phase E)
    /// Saves all dirty chapters to their respective files.
    void onSaveAll();

    /// @brief Slot for settings apply requested from SettingsDialog
    /// @param settings Settings data to apply
    /// @param fromOkButton true if triggered by OK (dialog closed), false if Apply
    void onApplySettings(const SettingsData& settings, bool fromOkButton);

    /// @brief Slot for File > Open > File... action (OpenSpec #00033 Phase F)
    /// Shows file dialog and opens standalone file
    void onOpenStandaloneFile();

    /// @brief Slot for "Add to Project" button in info bar (OpenSpec #00033 Phase F)
    /// Shows dialog to add standalone file to current project
    void onAddToProject();

    /// @brief Slot for File > Export > Project Archive... action (OpenSpec #00033 Phase H)
    /// Exports current project as .klh.zip archive
    void onExportArchive();

    /// @brief Slot for File > Import > Project Archive... action (OpenSpec #00033 Phase H)
    /// Imports project from .klh.zip archive
    void onImportArchive();

    /// @brief Open a standalone file by path (OpenSpec #00033 Phase F)
    /// @param path Absolute path to the file
    void openStandaloneFile(const QString& path);

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

    // Dev Tools slots (Task #00020) - only visible in dev mode
    void onDevToolsIconDownloader();

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

    // Menu builder (Task #00025 - centralized icon refresh)
    MenuBuilder* m_menuBuilder;

    // View actions (panel toggles)
    QAction* m_viewNavigatorAction;
    QAction* m_viewPropertiesAction;
    QAction* m_viewLogAction;
    QAction* m_viewSearchAction;
    QAction* m_viewAssistantAction;

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

    // Dev mode (Task #00020)
    bool m_devMode;             ///< Dev mode enabled flag
    QMenu* m_devToolsMenu;      ///< Dev Tools menu (only visible when m_devMode=true)

    // Dock title bar icons (Task #00028, OpenSpec #00032)
    QList<QLabel*> m_dockIconLabels;  ///< Icon labels in dock title bars (for theme refresh)
    QList<QToolButton*> m_dockToolButtons;  ///< Tool buttons in dock title bars (for theme refresh)

    // Document management (Task #00008 - Phase 0)
    std::optional<core::Document> m_currentDocument;  ///< Current loaded document
    std::filesystem::path m_currentFilePath;          ///< Current .klh file path
    bool m_isDirty;                                   ///< Unsaved changes flag

    // Chapter editing (OpenSpec #00033 Phase E)
    QMap<QString, bool> m_dirtyChapters;  ///< Tracks dirty state per elementId
    QString m_currentElementId;           ///< Currently active element in editor

    // Standalone file support (OpenSpec #00033 Phase F)
    StandaloneInfoBar* m_standaloneInfoBar;   ///< Info bar for standalone files
    QList<QString> m_standaloneFilePaths;     ///< List of open standalone file paths
    QWidget* m_centralWrapper;                ///< Wrapper widget for info bar + tabs

    /// @brief Mark document as modified (add "*" to title)
    void setDirty(bool dirty);

    /// @brief Update window title with filename and dirty state
    void updateWindowTitle();

    /// @brief Get currently active EditorPanel tab (Task #00015)
    /// @return Active EditorPanel if current tab is an editor, nullptr otherwise
    /// @note Returns nullptr if current tab is Dashboard or other panel type
    EditorPanel* getCurrentEditor();

    /// @brief Collect current application settings into SettingsData
    /// @return Current settings from SettingsManager and runtime state
    SettingsData collectCurrentSettings() const;

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
