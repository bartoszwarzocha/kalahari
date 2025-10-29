/// @file settings_dialog.h
/// @brief Settings dialog with tree-based navigation
///
/// Provides comprehensive configuration UI organized hierarchically.
/// Phase 0: Only Advanced → Diagnostics implemented.
/// Future phases will add more categories systematically.

#pragma once

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/splitter.h>
#include <map>

namespace kalahari {
namespace gui {

// ============================================================================
// Settings State Structure
// ============================================================================

/// @brief Settings state container
///
/// Holds current application configuration state.
/// Phase 0: Only diagnostic mode fields.
/// Phase 1+: Will be expanded with language, theme, auto-save, etc.
struct SettingsState {
    // Phase 0: Diagnostic mode
    bool diagnosticModeEnabled = false;
    bool launchedWithDiagFlag = false;

    // Phase 1+: Additional settings will go here
    // wxString interfaceLanguage = "en";
    // wxString themeName = "Light";
    // int autoSaveInterval = 5;
};

// ============================================================================
// Diagnostics Panel (Phase 0)
// ============================================================================

/// @brief Settings panel for Advanced → Diagnostics
///
/// Provides checkbox to enable/disable diagnostic mode at runtime.
/// Runtime-only (not persisted to settings.json).
/// Grayed out if application launched with --diag flag.
class DiagnosticsPanel : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (content panel in SettingsDialog)
    /// @param state Reference to working settings state
    DiagnosticsPanel(wxWindow* parent, SettingsState& state);

    /// @brief Save panel values back to state
    ///
    /// Called when user clicks OK or Apply.
    /// Respects launchedWithDiagFlag (won't change if CLI flag used).
    void saveToState();

private:
    SettingsState& m_state;
    wxCheckBox* m_diagnosticCheckbox = nullptr;

    /// @brief Handle checkbox value change
    /// @param event Checkbox event
    void onCheckboxChanged(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE();
};

// ============================================================================
// Settings Dialog (Main)
// ============================================================================

/// @brief Main settings dialog with tree navigation
///
/// **Layout:**
/// - Left: wxTreeCtrl (280px, resizable)
/// - Right: wxScrolledWindow (dynamic content)
/// - Bottom: OK, Cancel, Apply buttons
///
/// **Phase 0 tree:**
/// ```
/// 🔧 Advanced
/// └─ 🐛 Diagnostics
/// ```
///
/// **Future phases:** Will expand tree systematically (see 08_gui_design.md).
class SettingsDialog : public wxDialog {
public:
    /// @brief Constructor
    /// @param parent Parent window (MainWindow)
    /// @param currentState Current application settings state
    SettingsDialog(wxWindow* parent, const SettingsState& currentState);

    /// @brief Get modified state after user clicks OK
    /// @return New settings state with user changes
    SettingsState getNewState() const { return m_workingState; }

private:
    // ========================================================================
    // UI Components
    // ========================================================================

    wxSplitterWindow* m_splitter = nullptr;
    wxTreeCtrl* m_tree = nullptr;
    wxScrolledWindow* m_contentPanel = nullptr;
    wxImageList* m_iconList = nullptr;

    // Panel management
    std::map<wxTreeItemId, wxPanel*> m_panels;
    wxPanel* m_currentPanel = nullptr;

    // ========================================================================
    // State
    // ========================================================================

    SettingsState m_originalState;  ///< State when dialog opened
    SettingsState m_workingState;   ///< Modified state (before OK)

    // ========================================================================
    // Event Handlers
    // ========================================================================

    /// @brief Handle tree selection change
    /// @param event Tree event
    void onTreeSelectionChanged(wxTreeEvent& event);

    /// @brief Handle OK button click
    /// @param event Command event
    void onOK(wxCommandEvent& event);

    /// @brief Handle Cancel button click
    /// @param event Command event
    void onCancel(wxCommandEvent& event);

    /// @brief Handle Apply button click
    /// @param event Command event
    void onApply(wxCommandEvent& event);

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /// @brief Build tree structure
    ///
    /// Phase 0: Only Advanced → Diagnostics.
    /// Future phases: Add more branches systematically.
    void buildTree();

    /// @brief Show panel for selected tree item
    /// @param item Tree item ID
    void showPanel(wxTreeItemId item);

    /// @brief Validate all settings
    /// @return true if all settings valid, false otherwise
    bool validateSettings();

    /// @brief Apply changes from working state
    ///
    /// Saves all panel states to m_workingState.
    /// Shows confirmation dialog if enabling diagnostics.
    void applyChanges();

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
