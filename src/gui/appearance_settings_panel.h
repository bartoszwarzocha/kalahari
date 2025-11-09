/// @file appearance_settings_panel.h
/// @brief Settings panel for Appearance configuration (Task #00020 - Option C)

#pragma once

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/choice.h>

namespace kalahari {
namespace gui {

// Forward declaration
struct SettingsState;

/// @brief Settings panel for Appearance → Theme & Display
///
/// Provides controls to configure application appearance:
/// - Theme selection (Light/Dark/System)
/// - Icon size (16/24/32/48 pixels)
/// - Font scaling (0.8x - 1.5x)
///
/// These settings affect the entire application UI.
class AppearanceSettingsPanel : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (content panel in SettingsDialog)
    /// @param state Reference to working settings state
    AppearanceSettingsPanel(wxWindow* parent, SettingsState& state);

    /// @brief Save panel values back to state
    ///
    /// Called when user clicks OK or Apply.
    void saveToState();

private:
    SettingsState& m_state;

    // UI Controls
    wxChoice* m_themeChoice = nullptr;
    wxChoice* m_iconSizeChoice = nullptr;
    wxSpinCtrlDouble* m_fontScalingSpinner = nullptr;

    // Text controls that need dynamic wrapping
    wxStaticText* m_themeDescription = nullptr;
    wxStaticText* m_restartNote = nullptr;
    wxStaticText* m_iconDescription = nullptr;
    wxStaticText* m_typographyDescription = nullptr;
    wxStaticText* m_exampleText = nullptr;

    /// @brief Create theme settings section
    /// @param parent Parent sizer to add section to
    void createThemeSection(wxSizer* parent);

    /// @brief Create icon settings section
    /// @param parent Parent sizer to add section to
    void createIconSection(wxSizer* parent);

    /// @brief Create typography settings section
    /// @param parent Parent sizer to add section to
    void createTypographySection(wxSizer* parent);

    /// @brief Handle panel resize - dynamic text wrapping
    /// @param event Size event
    void onSize(wxSizeEvent& event);

    /// @brief Handle font scaling spinner value change
    /// @param event Spin event
    void onFontScalingChanged(wxSpinDoubleEvent& event);

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
