/// @file about_dialog.h
/// @brief About dialog with application information and third-party credits
///
/// Structure inspired by Fretboard Master:
/// - wxNotebook with multiple tabs
/// - App info tab with banner image
/// - Third-party components tab with licenses
/// - Credits and version information

#pragma once

#include <wx/dialog.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statbmp.h>
#include <wx/button.h>
#include <wx/textctrl.h>

namespace kalahari {
namespace gui {

/// @brief About dialog showing application information
///
/// Multi-tab dialog with:
/// - Main tab: App name, version, banner image, brief description
/// - Third-Party tab: Components and licenses
/// - License tab: Kalahari license (MIT)
/// - Credits tab: Development team and contributors
class AboutDialog : public wxDialog {
public:
    /// @brief Constructor
    /// @param parent Parent window
    explicit AboutDialog(wxWindow* parent);

private:
    /// @brief Create main info panel (first tab)
    /// @param notebook Parent notebook
    /// @return Created panel
    wxPanel* createMainPanel(wxNotebook* notebook);

    /// @brief Create third-party components panel
    /// @param notebook Parent notebook
    /// @return Created panel
    wxPanel* createThirdPartyPanel(wxNotebook* notebook);

    /// @brief Create license panel (MIT license text)
    /// @param notebook Parent notebook
    /// @return Created panel
    wxPanel* createLicensePanel(wxNotebook* notebook);

    /// @brief Create credits panel (development team)
    /// @param notebook Parent notebook
    /// @return Created panel
    wxPanel* createCreditsPanel(wxNotebook* notebook);

    /// @brief Create placeholder banner bitmap (black rectangle)
    /// @param width Banner width
    /// @param height Banner height
    /// @return Temporary black bitmap
    wxBitmap createPlaceholderBanner(int width, int height);

    // Event handlers
    void onClose(wxCommandEvent& event);
};

} // namespace gui
} // namespace kalahari
