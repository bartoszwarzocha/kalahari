/// @file appearance_settings_panel.h
/// @brief Settings panel for Appearance configuration (Task #00020 - Option C)

#pragma once

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/choice.h>
#include <bwx_sdk/bwx_gui/bwx_managed.h>  // Task #00043: BWX SDK Reactive System

namespace kalahari {
namespace gui {

// Forward declaration
struct SettingsState;

/// @brief Settings panel for Appearance → Theme & Display
///
/// Provides controls to configure application appearance:
/// - Theme selection (Light/Dark/System)
/// - Icon size (16/24/32/48 pixels)
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

    // UI Controls (Task #00043: BWX Reactive System)
    bwx::gui::Choice* m_themeChoice = nullptr;
    bwx::gui::Choice* m_iconSizeChoice = nullptr;

    // Text controls that need dynamic wrapping (Task #00043: BWX Reactive System)
    bwx::gui::StaticText* m_themeDescription = nullptr;
    bwx::gui::StaticText* m_restartNote = nullptr;
    bwx::gui::StaticText* m_iconDescription = nullptr;

    /// @brief Create theme settings section
    /// @param parent Parent sizer to add section to
    void createThemeSection(wxSizer* parent);

    /// @brief Create icon settings section
    /// @param parent Parent sizer to add section to
    void createIconSection(wxSizer* parent);

    /// @brief Handle panel resize - dynamic text wrapping
    /// @param event Size event
    void onSize(wxSizeEvent& event);

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
