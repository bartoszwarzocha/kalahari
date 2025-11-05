/// @file main_window.h
/// @brief Main application window for Kalahari Writer's IDE
///
/// This file contains the main window class (wxFrame-derived) that provides
/// the primary user interface including menu bar, toolbar, status bar,
/// and placeholder content area for the future editor.

#pragma once

#include <wx/wx.h>
#include <wx/aui/aui.h>
#include <thread>
#include <functional>
#include <vector>
#include <map>
#include <algorithm>

namespace kalahari {
namespace gui {

// Forward declarations
class NavigatorPanel;
class EditorPanel;
class PropertiesPanel;
class StatisticsPanel;
class SearchPanel;
class AssistantPanel;

// ============================================================================
// Custom Event Types (KALAHARI convention)
// ============================================================================

/// @brief Event fired when a background task completes successfully
wxDECLARE_EVENT(wxEVT_KALAHARI_TASK_COMPLETED, wxThreadEvent);

/// @brief Event fired when a background task fails with an exception
wxDECLARE_EVENT(wxEVT_KALAHARI_TASK_FAILED, wxThreadEvent);

/// @brief Main application window
///
/// Provides the primary UI chrome for Kalahari Writer's IDE:
/// - Menu bar (File, Edit, View, Help)
/// - Toolbar (New, Open, Save, etc.)
/// - Status bar (3 panes: status text, cursor position, session time)
/// - Main panel (placeholder for future editor - Phase 1)
///
/// Uses event tables for menu/toolbar event handling (type-safe, compile-time).
class MainWindow : public wxFrame {
public:
    /// @brief Constructor
    ///
    /// Creates the main window with default size and position.
    /// Initializes all UI components (menu, toolbar, status bar, main panel).
    MainWindow();

    /// @brief Destructor
    ///
    /// Cleanup is handled automatically by wxWidgets for child windows.
    virtual ~MainWindow();

private:
    // ========================================================================
    // UI Components
    // ========================================================================

    /// @brief Menu bar (File, Edit, View, Help menus)
    wxMenuBar* m_menuBar = nullptr;

    /// @brief Toolbar (New, Open, Save buttons with stock icons)
    wxToolBar* m_toolBar = nullptr;

    /// @brief Status bar (3 panes: status | position | time)
    wxStatusBar* m_statusBar = nullptr;

    /// @brief Main content panel (placeholder for future editor)
    wxPanel* m_mainPanel = nullptr;

    // ========================================================================
    // wxAUI Docking System (Phase 1 Task #00013)
    // ========================================================================

    /// @brief wxAUI manager for dockable panels
    wxAuiManager* m_auiManager = nullptr;

    /// @brief Navigator panel (document tree structure)
    NavigatorPanel* m_navigatorPanel = nullptr;

    /// @brief Editor panel (rich text editing)
    EditorPanel* m_editorPanel = nullptr;

    /// @brief Properties panel (chapter/document metadata)
    PropertiesPanel* m_propertiesPanel = nullptr;

    /// @brief Statistics panel (writing metrics)
    StatisticsPanel* m_statisticsPanel = nullptr;

    /// @brief Search panel (find and replace)
    SearchPanel* m_searchPanel = nullptr;

    /// @brief Assistant panel (AI writing help)
    AssistantPanel* m_assistantPanel = nullptr;

    /// @brief View menu items (for checkbox synchronization)
    wxMenuItem* m_viewNavigatorItem = nullptr;
    wxMenuItem* m_viewPropertiesItem = nullptr;
    wxMenuItem* m_viewStatisticsItem = nullptr;
    wxMenuItem* m_viewSearchItem = nullptr;
    wxMenuItem* m_viewAssistantItem = nullptr;

    /// @brief Editor Mode menu items (Task #00019)
    wxMenuItem* m_viewModeFullItem = nullptr;
    wxMenuItem* m_viewModePageItem = nullptr;
    wxMenuItem* m_viewModeTypewriterItem = nullptr;
    wxMenuItem* m_viewModePublisherItem = nullptr;

    /// @brief Perspectives submenu (for dynamic custom perspectives)
    wxMenu* m_perspectivesMenu = nullptr;

    /// @brief Dynamic custom perspective menu items
    std::vector<wxMenuItem*> m_customPerspectiveItems;

    /// @brief Mapping from menu ID to custom perspective name
    std::map<int, std::string> m_customPerspectiveNames;

    // ========================================================================
    // Threading Infrastructure (Phase 0 Week 2)
    // ========================================================================

    /// @brief Mutex protecting access to thread pool vector
    wxMutex m_threadMutex;

    /// @brief Semaphore limiting max concurrent background tasks (max 4)
    wxSemaphore m_threadSemaphore;

    /// @brief Vector tracking active background thread IDs
    ///
    /// Protected by m_threadMutex. Used for graceful shutdown.
    std::vector<std::thread::id> m_activeThreads;

    // ========================================================================
    // Diagnostic Mode (Phase 0 Week 3)
    // ========================================================================

    /// @brief Current diagnostic mode state (runtime only, not persisted)
    bool m_diagnosticMode = false;

    /// @brief Whether app was launched with --diag CLI flag
    ///
    /// If true, diagnostic mode cannot be changed via Settings dialog.
    bool m_launchedWithDiagFlag = false;

    // ========================================================================
    // Event Handlers (On prefix, camelCase)
    // ========================================================================

    /// @brief Handle File -> New menu item
    /// @param event Command event from menu/toolbar
    void onFileNew(wxCommandEvent& event);

    /// @brief Handle File -> Open menu item
    /// @param event Command event from menu/toolbar
    void onFileOpen(wxCommandEvent& event);

    /// @brief Handle File -> Save menu item
    /// @param event Command event from menu/toolbar
    void onFileSave(wxCommandEvent& event);

    /// @brief Handle File -> Save As menu item
    /// @param event Command event from menu/toolbar
    void onFileSaveAs(wxCommandEvent& event);

    /// @brief Handle File -> Settings menu item
    /// @param event Command event from menu
    void onFileSettings(wxCommandEvent& event);

    /// @brief Handle File -> Exit menu item
    /// @param event Command event from menu
    void onFileExit(wxCommandEvent& event);

    /// @brief Handle Edit -> Undo menu item
    /// @param event Command event from menu/toolbar
    void onEditUndo(wxCommandEvent& event);

    /// @brief Handle Edit -> Redo menu item
    /// @param event Command event from menu/toolbar
    void onEditRedo(wxCommandEvent& event);

    /// @brief Handle Edit -> Cut menu item (Task #00019)
    /// @param event Command event from menu/toolbar
    void onEditCut(wxCommandEvent& event);

    /// @brief Handle Edit -> Copy menu item (Task #00019)
    /// @param event Command event from menu/toolbar
    void onEditCopy(wxCommandEvent& event);

    /// @brief Handle Edit -> Paste menu item (Task #00019)
    /// @param event Command event from menu/toolbar
    void onEditPaste(wxCommandEvent& event);

    /// @brief Handle Edit -> Select All menu item (Task #00019)
    /// @param event Command event from menu/toolbar
    void onEditSelectAll(wxCommandEvent& event);

    /// @brief Handle Format -> Bold menu item (Task #00014)
    /// @param event Command event from Format menu
    void onFormatBold(wxCommandEvent& event);

    /// @brief Handle Format -> Italic menu item (Task #00014)
    /// @param event Command event from Format menu
    void onFormatItalic(wxCommandEvent& event);

    /// @brief Handle Format -> Underline menu item (Task #00014)
    /// @param event Command event from Format menu
    void onFormatUnderline(wxCommandEvent& event);

    /// @brief Handle Format -> Font menu item (Task #00014)
    /// @param event Command event from Format menu
    void onFormatFont(wxCommandEvent& event);

    /// @brief Handle Format -> Clear Formatting menu item (Task #00014)
    /// @param event Command event from Format menu
    void onFormatClear(wxCommandEvent& event);

    /// @brief Handle Help -> About menu item
    /// @param event Command event from menu
    void onHelpAbout(wxCommandEvent& event);

    /// @brief Handle Diagnostics -> Test Python Integration menu item
    /// @param event Command event from menu
    void onDiagnosticsTestPython(wxCommandEvent& event);

    /// @brief Handle Diagnostics -> Test Python Bindings (pybind11) menu item
    /// @param event Command event from menu
    void onDiagnosticsTestPyBind11(wxCommandEvent& event);

    /// @brief Handle Diagnostics -> Open Log Folder menu item
    /// @param event Command event from menu
    void onDiagnosticsOpenLogs(wxCommandEvent& event);

    /// @brief Handle Diagnostics -> System Information menu item
    /// @param event Command event from menu
    void onDiagnosticsSystemInfo(wxCommandEvent& event);

    /// @brief Handle Diagnostics -> Test Plugin System menu item
    /// @param event Command event from menu
    void onDiagnosticsTestPlugins(wxCommandEvent& event);

    /// @brief Handle window close event
    /// @param event Close event (from window manager or File->Exit)
    void onClose(wxCloseEvent& event);

    /// @brief Handle background task completion event
    /// @param event Thread event with task results (from wxQueueEvent)
    void onTaskCompleted(wxThreadEvent& event);

    /// @brief Handle background task failure event
    /// @param event Thread event with error message (from wxQueueEvent)
    void onTaskFailed(wxThreadEvent& event);

    // ========================================================================
    // View Menu Event Handlers (Task #00013)
    // ========================================================================

    /// @brief Handle View -> Navigator menu item
    /// @param event Command event from menu
    void onViewNavigator(wxCommandEvent& event);

    /// @brief Handle View -> Properties menu item
    /// @param event Command event from menu
    void onViewProperties(wxCommandEvent& event);

    /// @brief Handle View -> Statistics menu item
    /// @param event Command event from menu
    void onViewStatistics(wxCommandEvent& event);

    /// @brief Handle View -> Search menu item
    /// @param event Command event from menu
    void onViewSearch(wxCommandEvent& event);

    /// @brief Handle View -> Assistant menu item
    /// @param event Command event from menu
    void onViewAssistant(wxCommandEvent& event);

    /// @brief Handle View -> Editor Mode menu items (Task #00019)
    /// @param event Command event from Editor Mode submenu
    void onViewMode(wxCommandEvent& event);

    /// @brief Handle View -> Perspectives -> Load menu items
    /// @param event Command event from menu
    void onLoadPerspective(wxCommandEvent& event);

    /// @brief Handle View -> Perspectives -> Save Perspective...
    /// @param event Command event from menu
    void onSavePerspective(wxCommandEvent& event);

    /// @brief Handle View -> Perspectives -> Manage Perspectives...
    /// @param event Command event from menu
    void onManagePerspectives(wxCommandEvent& event);

    /// @brief Refresh the Perspectives submenu with custom perspectives
    ///
    /// Removes old dynamic menu items and adds up to 5 most recent
    /// custom perspectives (excluding default ones) to the menu.
    /// Called after save/delete/rename operations.
    void refreshPerspectivesMenu();

    // ========================================================================
    // Threading Methods (Phase 0 Week 2)
    // ========================================================================

    /// @brief Submit a background task for execution
    ///
    /// Runs the provided task function in a worker thread (std::thread).
    /// Thread pool is limited to 4 concurrent tasks via semaphore.
    /// Task results/errors are communicated back to GUI via wxQueueEvent.
    ///
    /// @param task Function to execute in background thread
    /// @return true if task submitted, false if thread limit reached
    ///
    /// @note Thread-safe. Can be called from any thread.
    /// @note Task exceptions are caught and sent as wxEVT_KALAHARI_TASK_FAILED events.
    ///
    /// Example:
    /// @code
    /// submitBackgroundTask([]() {
    ///     // Heavy computation here
    ///     auto result = processData();
    ///
    ///     // Notify GUI when done (will be queued to GUI thread)
    ///     wxThreadEvent* evt = new wxThreadEvent(wxEVT_KALAHARI_TASK_COMPLETED);
    ///     evt->SetPayload(result);
    ///     wxQueueEvent(this, evt);
    /// });
    /// @endcode
    bool submitBackgroundTask(std::function<void()> task);

    // ========================================================================
    // Helper Methods (camelCase, private)
    // ========================================================================

    /// @brief Create and populate the menu bar
    ///
    /// Creates File, Edit, View, and Help menus with appropriate items.
    /// Uses standard wxID_* constants for platform integration (e.g., Cmd+Q on macOS).
    void createMenuBar();

    /// @brief Create and populate the toolbar
    ///
    /// Creates toolbar with New, Open, Save buttons using stock icons
    /// (wxArtProvider provides platform-native icons).
    void createToolBar();

    /// @brief Create and configure the status bar
    ///
    /// Creates 3-pane status bar:
    /// - Pane 0: Status text (flexible width)
    /// - Pane 1: Cursor position ("Line X, Col Y")
    /// - Pane 2: Session time or other info
    void createStatusBar();

    /// @brief Setup main content panel
    ///
    /// Creates placeholder panel with centered text explaining that
    /// the editor will be implemented in Phase 1.
    void setupMainPanel();

    /// @brief Set diagnostic mode (show/hide Diagnostics menu)
    ///
    /// Rebuilds menu bar to show or hide the Diagnostics menu.
    /// @param enabled true to show Diagnostics menu, false to hide
    void setDiagnosticMode(bool enabled);

    // ========================================================================
    // wxAUI Helper Methods (Task #00013)
    // ========================================================================

    /// @brief Initialize wxAUI manager and create panels
    ///
    /// Creates all 6 panels and adds them to wxAuiManager with default layout.
    void initializeAUI();

    /// @brief Update View menu checkboxes to match panel visibility
    ///
    /// Synchronizes View menu checkboxes with actual wxAuiPaneInfo states.
    /// Called after loading perspectives or toggling panels.
    void updateViewMenu();

    /// @brief Load a perspective by name
    ///
    /// Loads panel layout from PerspectiveManager.
    /// @param name Perspective name ("Default", "Writing", etc.)
    void loadPerspective(const std::string& name);

    /// @brief Save current layout as a named perspective
    ///
    /// Saves current wxAuiManager layout to PerspectiveManager.
    /// @param name Perspective name
    void savePerspective(const std::string& name);

    // ========================================================================
    // Event Table Declaration
    // ========================================================================

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
