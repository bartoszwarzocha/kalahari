/// @file log_settings_panel.h
/// @brief Settings panel for Diagnostic Log configuration (Task #00020)

#pragma once

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/clrpicker.h>

namespace kalahari {
namespace gui {

// Forward declaration
struct SettingsState;

/// @brief Settings panel for Diagnostic Log → Configuration
///
/// Provides controls to configure the Diagnostic Log panel:
/// - Ring buffer size (1-1000 entries)
/// - Background color (RGB)
/// - Text color (RGB)
/// - Font size (6-20 points)
///
/// Only visible when diagnostic mode is enabled.
class LogSettingsPanel : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (content panel in SettingsDialog)
    /// @param state Reference to working settings state
    LogSettingsPanel(wxWindow* parent, SettingsState& state);

    /// @brief Save panel values back to state
    ///
    /// Called when user clicks OK or Apply.
    void saveToState();

private:
    SettingsState& m_state;

    // UI Controls
    wxSpinCtrl* m_bufferSizeSpinner = nullptr;
    wxColourPickerCtrl* m_backgroundColorPicker = nullptr;
    wxColourPickerCtrl* m_textColorPicker = nullptr;
    wxSpinCtrl* m_fontSizeSpinner = nullptr;

    /// @brief Create buffer settings section
    /// @param parent Parent sizer to add section to
    void createBufferSection(wxSizer* parent);

    /// @brief Create appearance settings section
    /// @param parent Parent sizer to add section to
    void createAppearanceSection(wxSizer* parent);

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
