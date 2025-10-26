/// @file main_window.h
/// @brief Main application window for Kalahari Writer's IDE
///
/// This file contains the main window class (wxFrame-derived) that provides
/// the primary user interface including menu bar, toolbar, status bar,
/// and placeholder content area for the future editor.

#pragma once

#include <wx/wx.h>

namespace kalahari {
namespace gui {

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

    /// @brief Handle File -> Exit menu item
    /// @param event Command event from menu
    void onFileExit(wxCommandEvent& event);

    /// @brief Handle Edit -> Undo menu item
    /// @param event Command event from menu/toolbar
    void onEditUndo(wxCommandEvent& event);

    /// @brief Handle Edit -> Redo menu item
    /// @param event Command event from menu/toolbar
    void onEditRedo(wxCommandEvent& event);

    /// @brief Handle Help -> About menu item
    /// @param event Command event from menu
    void onHelpAbout(wxCommandEvent& event);

    /// @brief Handle window close event
    /// @param event Close event (from window manager or File->Exit)
    void onClose(wxCloseEvent& event);

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

    // ========================================================================
    // Event Table Declaration
    // ========================================================================

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
