/// @file main_window.h
/// @brief Main application window (Qt6 QMainWindow subclass)
///
/// This file defines the MainWindow class, which is the primary GUI window
/// for Kalahari Writer's IDE. It manages menus, toolbars, status bar, and
/// dockable panels.
///
/// OpenSpec #00038 Phase 4: Dock/panel management delegated to DockCoordinator.
/// OpenSpec #00038 Phase 7: Document operations delegated to DocumentCoordinator.

#pragma once

#include <QMainWindow>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <QTabWidget>
#include <QLabel>
#include <QMap>
#include <optional>
#include <filesystem>
#include "kalahari/core/document.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/toolbar_manager.h"
#include "kalahari/gui/settings_data.h"

class QDockWidget;
class QCloseEvent;
class QShowEvent;
class QTimer;

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
class LogPanel;
class MenuBuilder;      // Task #00025
class BusyIndicator;    // Reusable spinner overlay
class DiagnosticController;  // OpenSpec #00038 - Diagnostic/dev mode controller
class DockCoordinator;  // OpenSpec #00038 Phase 4 - Dock/panel management
class SettingsCoordinator;  // OpenSpec #00038 Phase 5 - Settings management
class NavigatorCoordinator;  // OpenSpec #00038 Phase 6 - Navigator handlers
class DocumentCoordinator;  // OpenSpec #00038 Phase 7 - Document operations

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

    /// @brief Destructor - disconnects signals before children are destroyed
    ~MainWindow() override;

    /// @brief Enable diagnostic mode (show Diagnostics menu)
    /// @note Delegates to DiagnosticController (OpenSpec #00038)
    void enableDiagnosticMode();

    /// @brief Disable diagnostic mode (hide Diagnostics menu)
    /// @note Delegates to DiagnosticController (OpenSpec #00038)
    void disableDiagnosticMode();

    /// @brief Check if diagnostic mode is enabled
    /// @return true if diagnostic mode is active, false otherwise
    /// @note Delegates to DiagnosticController (OpenSpec #00038)
    [[nodiscard]] bool isDiagnosticMode() const;

    /// @brief Enable dev mode (show Dev Tools menu) - Task #00020
    /// @note Delegates to DiagnosticController (OpenSpec #00038)
    void enableDevMode();

    /// @brief Disable dev mode (hide Dev Tools menu) - Task #00020
    /// @note Delegates to DiagnosticController (OpenSpec #00038)
    void disableDevMode();

    /// @brief Check if dev mode is enabled - Task #00020
    /// @return true if dev mode is active, false otherwise
    /// @note Delegates to DiagnosticController (OpenSpec #00038)
    [[nodiscard]] bool isDevMode() const;

    /// @brief Apply editor settings to all open EditorPanels
    /// @note Called when editor font, colors, or other settings change
    void applyEditorSettingsToAllPanels();

    /// @brief Get current active editor panel
    /// @return Active EditorPanel if current tab is an editor, nullptr otherwise
    /// @note Returns nullptr if current tab is Dashboard or other panel type
    /// @note Made public for diagnostic/benchmark access (OpenSpec #00043)
    EditorPanel* getCurrentEditor();

    /// @brief Open a chapter/element by ID (OpenSpec #00043 - Benchmark CLI)
    /// @param elementId Element ID to open (from BookElement::getId())
    /// @param elementTitle Display title of the element
    /// @note Public wrapper for onNavigatorElementSelected for CLI/benchmark use
    void openChapter(const QString& elementId, const QString& elementTitle);

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
    /// Delegates to DockCoordinator (OpenSpec #00038 Phase 4).
    void createDocks();

    /// @brief Reset dock layout to default
    /// @note Delegates to DockCoordinator (OpenSpec #00038 Phase 4).
    void resetLayout();

    // NOTE: createDiagnosticMenu, removeDiagnosticMenu, createDevToolsMenu, removeDevToolsMenu
    // moved to DiagnosticController (OpenSpec #00038)

    // NOTE: setupDockTitleBar, refreshDockIcons moved to DockCoordinator (OpenSpec #00038 Phase 4)

protected:
    /// @brief Save perspective on close
    /// @param event Close event
    void closeEvent(QCloseEvent* event) override;

    /// @brief Restore perspective on show
    /// @param event Show event
    void showEvent(QShowEvent* event) override;

private slots:
    // NOTE: Document operations (onNewDocument, onNewProject, onOpenDocument, onSaveDocument,
    // onSaveAsDocument, onSaveAll, onOpenRecentFile, onOpenStandaloneFile, openStandaloneFile,
    // onAddToProject, onExportArchive, onImportArchive, onProjectOpened, onProjectClosed)
    // moved to DocumentCoordinator (OpenSpec #00038 Phase 7)

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

    // =========================================================================
    // Format Actions (OpenSpec #00042 Phase 7.2)
    // =========================================================================

    /// @brief Slot for Format > Bold action
    void onFormatBold();

    /// @brief Slot for Format > Italic action
    void onFormatItalic();

    /// @brief Slot for Format > Underline action
    void onFormatUnderline();

    /// @brief Slot for Format > Strikethrough action
    void onFormatStrikethrough();

    /// @brief Slot for Format > Align Left action
    void onAlignLeft();

    /// @brief Slot for Format > Align Center action
    void onAlignCenter();

    /// @brief Slot for Format > Align Right action
    void onAlignRight();

    /// @brief Slot for Format > Justify action
    void onAlignJustify();

    // =========================================================================
    // Insert Actions (OpenSpec #00042 Phase 7.9)
    // =========================================================================

    /// @brief Slot for Insert > Comment action
    void onInsertComment();

    // =========================================================================
    // View Mode Actions (OpenSpec #00042 Phase 7.3)
    // =========================================================================

    /// @brief Set editor view mode to Continuous
    void onViewModeContinuous();

    /// @brief Set editor view mode to Page Layout
    void onViewModePage();

    /// @brief Set editor view mode to Typewriter
    void onViewModeTypewriter();

    /// @brief Set editor view mode to Focus
    void onViewModeFocus();

    /// @brief Set editor view mode to Distraction-Free
    void onViewModeDistFree();

    /// @brief Update action states based on editor state
    ///
    /// Called when cursor position or selection changes in BookEditor.
    /// Updates enabled/checked state for Edit and Format menu actions.
    void updateEditorActionStates();

    /// @brief Update status bar statistics display
    /// @param words Word count
    /// @param chars Character count
    /// @param paragraphs Paragraph count
    /// @note Connected to StatisticsCollector::statisticsChanged() signal (OpenSpec #00042 Task 6.13)
    void updateStatusBarStatistics(int words, int chars, int paragraphs);

    /// @brief Slot for Help > About action
    void onAbout();

    /// @brief Slot for Help > About Qt action
    void onAboutQt();

    /// @brief Slot for Navigator element selection (Task #00015, OpenSpec #00033)
    /// @param elementId Unique ID of the selected element (BookElement::getId())
    /// @param elementTitle Display title of the element
    /// @note Delegates to NavigatorCoordinator (OpenSpec #00038 Phase 6)
    void onNavigatorElementSelected(const QString& elementId, const QString& elementTitle);

    // NOTE: onDiagModeChanged moved to DiagnosticController (OpenSpec #00038)
    // MainWindow forwards to controller

    /// @brief Slot for theme changed (Task #00023)
    /// @param theme New theme to apply to IconRegistry
    void onThemeChanged(const kalahari::core::Theme& theme);

    // NOTE: onApplySettings moved to SettingsCoordinator (OpenSpec #00038 Phase 5)
    // NOTE: Navigator context menu handlers moved to NavigatorCoordinator (OpenSpec #00038 Phase 6)
    // NOTE: All diagnostic tool slots (onDiag*) moved to DiagnosticController (OpenSpec #00038)
    // NOTE: All dev tools slots (onDevTools*) moved to DiagnosticController (OpenSpec #00038)

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

    // Dock/panel coordinator (OpenSpec #00038 Phase 4)
    DockCoordinator* m_dockCoordinator;  ///< Manages dock widgets and panels

    // First show flag (for geometry restore)
    bool m_firstShow;

    // Diagnostic/Dev mode controller (OpenSpec #00038)
    DiagnosticController* m_diagnosticController;  ///< Manages diagnostic/dev mode and menus

    // Settings coordinator (OpenSpec #00038 Phase 5)
    SettingsCoordinator* m_settingsCoordinator;  ///< Manages settings dialog and application

    // Navigator coordinator (OpenSpec #00038 Phase 6)
    NavigatorCoordinator* m_navigatorCoordinator;  ///< Manages navigator panel interactions

    // Document coordinator (OpenSpec #00038 Phase 7)
    DocumentCoordinator* m_documentCoordinator;  ///< Manages document lifecycle and file operations

    // Document dirty state (kept in MainWindow, shared with DocumentCoordinator via callbacks)
    bool m_isDirty;                                   ///< Unsaved changes flag

    // Fullscreen mode (OpenSpec #00040)
    QByteArray m_savedGeometryBeforeFullscreen;       ///< Saved geometry before entering fullscreen

    // Status bar statistics labels (OpenSpec #00042 Task 6.13)
    QLabel* m_wordCountLabel{nullptr};                ///< Word count display
    QLabel* m_charCountLabel{nullptr};                ///< Character count display
    QLabel* m_readingTimeLabel{nullptr};              ///< Reading time display

    // OpenSpec #00043: Debounce timer for action state updates
    QTimer* m_actionStateDebounceTimer{nullptr};      ///< Debounce rapid cursor changes

    // NOTE: m_currentDocument, m_currentFilePath, m_standaloneFilePaths moved to DocumentCoordinator (OpenSpec #00038 Phase 7)
    // NOTE: m_dirtyChapters and m_currentElementId moved to NavigatorCoordinator (OpenSpec #00038 Phase 6)

    /// @brief Mark document as modified (add "*" to title)
    void setDirty(bool dirty);

    /// @brief Update window title with filename and dirty state
    void updateWindowTitle();

    /// @brief Toggle fullscreen mode (OpenSpec #00040)
    void toggleFullScreen();

    // NOTE: collectCurrentSettings moved to SettingsCoordinator (OpenSpec #00038 Phase 5)
    // NOTE: getPhase0Content, setPhase0Content moved to DocumentCoordinator (OpenSpec #00038 Phase 7)
};

} // namespace gui
} // namespace kalahari
