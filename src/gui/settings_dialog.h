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
/// Phase 0: Diagnostic mode fields.
/// Phase 1: Editor settings (Task #00019 - Settings Infrastructure).
/// Future: Will be expanded with language, theme, auto-save, etc.
struct SettingsState {
    // Phase 0: Diagnostic mode
    bool diagnosticModeEnabled = false;
    bool launchedWithDiagFlag = false;

    // ========================================================================
    // Phase 1: Editor Settings (Task #00019)
    // ========================================================================

    // Cursor & Caret
    bool caretBlinkEnabled = true;
    int caretBlinkRate = 500;      ///< Milliseconds (100-2000)
    int caretWidth = 1;             ///< Pixels (1-5)

    // Margins & Padding
    int marginLeft = 20;            ///< Pixels (0-100)
    int marginRight = 20;           ///< Pixels (0-100)
    int marginTop = 10;             ///< Pixels (0-100)
    int marginBottom = 10;          ///< Pixels (0-100)

    // Rendering
    double lineSpacing = 1.2;       ///< Multiplier (1.0-3.0)
    int selectionOpacity = 128;     ///< Alpha value (0-255), fixes bug #6
    wxColour selectionColor = wxColour(0, 120, 215);  ///< RGB selection color
    bool antialiasing = true;

    // Behavior
    bool autoFocus = true;          ///< Auto-focus editor on load, fixes bug #1
    bool wordWrap = true;
    int undoLimit = 100;            ///< Undo stack size (10-1000)

    // ========================================================================
    // Diagnostic Log Settings (Task #00020 - Phase 1)
    // ========================================================================

    int logBufferSize = 500;                                  ///< Ring buffer size (1-1000)
    wxColour logBackgroundColor = wxColour(60, 60, 60);       ///< Log background RGB
    wxColour logTextColor = wxColour(255, 255, 255);          ///< Log text RGB
    int logFontSize = 11;                                     ///< Log font size in points (6-20)

    // ========================================================================
    // Appearance Settings (Task #00020 - Option C)
    // ========================================================================

    wxString themeName = "System";           ///< Theme: "Light", "Dark", "System"
    int iconSize = 24;                       ///< Icon size in pixels (16, 24, 32, 48)
    double fontScaling = 1.0;                ///< Font scaling multiplier (0.8-1.5)

    // Future phases: Additional settings will go here
    // wxString interfaceLanguage = "en";
    // int autoSaveInterval = 5;
};

// ============================================================================
// Custom Event for Apply Button
// ============================================================================

/// @brief Event sent when user clicks Apply button (not OK)
///
/// Allows MainWindow to save and apply settings immediately
/// without closing the Settings Dialog.
class SettingsAppliedEvent : public wxCommandEvent {
public:
    SettingsAppliedEvent(wxEventType commandType = wxEVT_NULL, int id = 0)
        : wxCommandEvent(commandType, id) {}

    /// @brief Get the new settings state
    SettingsState getNewState() const { return m_newState; }

    /// @brief Set the new settings state
    void setNewState(const SettingsState& state) { m_newState = state; }

    /// @brief Clone event (required by wxWidgets)
    wxEvent* Clone() const override { return new SettingsAppliedEvent(*this); }

private:
    SettingsState m_newState;
};

// Declare custom event type
wxDECLARE_EVENT(EVT_SETTINGS_APPLIED, SettingsAppliedEvent);

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
