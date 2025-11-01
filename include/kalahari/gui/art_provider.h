/// @file art_provider.h
/// @brief Custom wxArtProvider with SVG support and IconRegistry integration
///
/// KalahariArtProvider queries IconRegistry for icons and renders them
/// using wxBitmapBundle::FromSVG() with color/size customization.

#pragma once

#include <wx/artprov.h>
#include <wx/bmpbndl.h>

namespace kalahari {
namespace gui {

/// @brief Custom ArtProvider with SVG support and IconRegistry integration
///
/// This class bridges wxWidgets' icon system (wxArtProvider) with our
/// custom IconRegistry. It:
/// - Queries IconRegistry for SVG data
/// - Applies color theme from IconRegistry
/// - Applies size configuration from IconRegistry
/// - Creates wxBitmapBundle from SVG (HiDPI automatic)
///
/// Usage:
/// @code
/// // At application startup (called from MainWindow constructor)
/// KalahariArtProvider::Initialize();
/// @endcode
class KalahariArtProvider : public wxArtProvider {
public:
    /// @brief Initialize and register provider with wxWidgets
    ///
    /// Pushes KalahariArtProvider onto wxWidgets' provider stack.
    /// Icons will be queried from IconRegistry when needed.
    static void Initialize();

protected:
    /// @brief Create bitmap bundle from SVG (override from wxArtProvider)
    ///
    /// This method is called by wxWidgets when an icon is requested via:
    /// - wxArtProvider::GetBitmap()
    /// - wxArtProvider::GetBitmapBundle()
    /// - wxToolBar->AddTool()
    /// - wxMenuItem->SetBitmap()
    ///
    /// @param id Action ID (e.g., wxID_NEW, wxID_OPEN, custom IDs)
    /// @param client Context where icon is used (wxART_TOOLBAR, wxART_MENU, etc.)
    /// @param size Requested size (may be wxDefaultSize for context default)
    /// @return wxBitmapBundle or wxNullBitmap if icon not found
    wxBitmapBundle CreateBitmapBundle(const wxArtID& id,
                                      const wxArtClient& client,
                                      const wxSize& size) override;

private:
    /// @brief Replace {COLOR} placeholder in SVG with actual color
    /// @param svg SVG string with {COLOR} placeholder
    /// @param color Actual color to use
    /// @return SVG with color replaced
    static std::string replaceColor(const std::string& svg, const wxColour& color);
};

} // namespace gui
} // namespace kalahari
