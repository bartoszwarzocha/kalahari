/// @file editor_settings_panel.h
/// @brief Settings panel for Editor configuration
///
/// Provides UI for configuring text editor behavior, appearance, and layout.
/// Part of Task #00019 Settings Infrastructure - enables testing different
/// configurations without rebuilding.

#pragma once

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/clrpicker.h>

namespace kalahari {
namespace gui {

// Forward declaration
struct SettingsState;

// ============================================================================
// Editor Settings Panel
// ============================================================================

/// @brief Settings panel for Editor configuration
///
/// Provides comprehensive editor settings organized into 4 categories:
/// 1. **Cursor & Caret** - Blink rate, width, visibility
/// 2. **Margins & Padding** - Left/right/top/bottom margins
/// 3. **Rendering** - Line spacing, selection opacity/color, antialiasing
/// 4. **Behavior** - Auto-focus, word wrap, undo limit
///
/// **SVG Icon:** resources/icons/material_design/settings.svg
class EditorSettingsPanel : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (content panel in SettingsDialog)
    /// @param state Reference to working settings state
    EditorSettingsPanel(wxWindow* parent, SettingsState& state);

    /// @brief Save panel values back to state
    ///
    /// Called when user clicks OK or Apply.
    /// Reads all spinners, checkboxes, and color pickers.
    void saveToState();

private:
    SettingsState& m_state;

    // ========================================================================
    // Cursor & Caret Controls
    // ========================================================================

    wxCheckBox* m_caretBlinkCheckbox = nullptr;
    wxSpinCtrl* m_caretBlinkRateSpinner = nullptr;  // ms
    wxSpinCtrl* m_caretWidthSpinner = nullptr;      // px

    // ========================================================================
    // Margins & Padding Controls
    // ========================================================================

    wxSpinCtrl* m_marginLeftSpinner = nullptr;      // px
    wxSpinCtrl* m_marginRightSpinner = nullptr;     // px
    wxSpinCtrl* m_marginTopSpinner = nullptr;       // px
    wxSpinCtrl* m_marginBottomSpinner = nullptr;    // px

    // ========================================================================
    // Rendering Controls
    // ========================================================================

    wxSpinCtrlDouble* m_lineSpacingSpinner = nullptr;  // multiplier (1.0-3.0)
    wxSlider* m_selectionOpacitySlider = nullptr;       // 0-255
    wxColourPickerCtrl* m_selectionColorPicker = nullptr;
    wxCheckBox* m_antialiasingCheckbox = nullptr;

    // ========================================================================
    // Behavior Controls
    // ========================================================================

    wxCheckBox* m_autoFocusCheckbox = nullptr;
    wxCheckBox* m_wordWrapCheckbox = nullptr;
    wxSpinCtrl* m_undoLimitSpinner = nullptr;

    // ========================================================================
    // Text controls that need dynamic wrapping
    // ========================================================================

    wxStaticText* m_marginsDescription = nullptr;

    // ========================================================================
    // Event Handlers
    // ========================================================================

    /// @brief Handle caret blink checkbox toggle
    /// @param event Checkbox event
    void onCaretBlinkChanged(wxCommandEvent& event);

    /// @brief Handle panel resize - dynamic text wrapping
    /// @param event Size event
    void onSize(wxSizeEvent& event);

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /// @brief Create Cursor & Caret section
    /// @param parent Parent sizer
    void createCursorSection(wxSizer* parent);

    /// @brief Create Margins & Padding section
    /// @param parent Parent sizer
    void createMarginsSection(wxSizer* parent);

    /// @brief Create Rendering section
    /// @param parent Parent sizer
    void createRenderingSection(wxSizer* parent);

    /// @brief Create Behavior section
    /// @param parent Parent sizer
    void createBehaviorSection(wxSizer* parent);

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
