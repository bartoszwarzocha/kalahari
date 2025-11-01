/// @file art_provider.cpp
/// @brief Implementation of KalahariArtProvider

#include "kalahari/gui/art_provider.h"
#include "kalahari/gui/icon_registry.h"
#include "kalahari/core/logger.h"
#include <wx/bmpbndl.h>
#include <algorithm>

using namespace kalahari::gui;
using namespace kalahari::core;

// ============================================================================
// KalahariArtProvider Implementation
// ============================================================================

void KalahariArtProvider::Initialize() {
    Logger::getInstance().info("KalahariArtProvider: Initializing...");

    // Push our custom provider onto wxWidgets provider stack
    wxArtProvider::Push(new KalahariArtProvider);

    Logger::getInstance().info("KalahariArtProvider: Registered with wxWidgets");
}

wxBitmapBundle KalahariArtProvider::CreateBitmapBundle(
    const wxArtID& id,
    const wxArtClient& client,
    const wxSize& size)
{
    // DEBUG: Log every call to see if we're being invoked
    Logger::getInstance().debug("KalahariArtProvider::CreateBitmapBundle() called with id='{}', client='{}', size={}x{}",
        id.ToStdString(), client.ToStdString(), size.GetWidth(), size.GetHeight());

    auto& registry = IconRegistry::getInstance();

    // 1. Get effective SVG from registry (user override or default)
    std::string svg = registry.getEffectiveSVG(id);
    if (svg.empty()) {
        // Icon not registered in IconRegistry - return null (fallback to default provider)
        Logger::getInstance().debug("KalahariArtProvider: Icon '{}' not registered, returning wxNullBitmap (fallback to default provider)",
            id.ToStdString());
        return wxNullBitmap;
    }

    Logger::getInstance().debug("KalahariArtProvider: Found SVG for '{}', length={} bytes",
        id.ToStdString(), svg.length());

    // 2. Get effective color from registry (per-icon override or theme color)
    wxColour color = registry.getEffectiveColor(id);

    // 3. Replace {COLOR} placeholder with actual color
    svg = replaceColor(svg, color);
    Logger::getInstance().debug("KalahariArtProvider: After color replacement, SVG length={} bytes",
        svg.length());

    // 4. Determine size
    int iconSize = 16; // Default fallback
    if (size.GetWidth() > 0) {
        // Explicit size requested
        iconSize = size.GetWidth();
    } else {
        // Use context-aware size from registry
        iconSize = registry.getSizeForClient(client);
    }

    Logger::getInstance().debug("KalahariArtProvider: Creating bundle with size={}px, color=#{:02X}{:02X}{:02X}",
        iconSize, color.Red(), color.Green(), color.Blue());

    // 5. Create wxBitmapBundle from SVG
    //    wxBitmapBundle::FromSVG() uses nanosvg (embedded in wxWidgets 3.3.1)
    //    It automatically handles HiDPI (creates 1x, 2x, 3x versions)
    try {
        wxBitmapBundle bundle = wxBitmapBundle::FromSVG(
            svg.c_str(),
            wxSize(iconSize, iconSize)
        );

        if (bundle.IsOk()) {
            Logger::getInstance().info("KalahariArtProvider: ✓ Successfully created icon '{}' (size={}px)",
                id.ToStdString(), iconSize);
            return bundle;
        } else {
            Logger::getInstance().warn("KalahariArtProvider: ✗ wxBitmapBundle::FromSVG() returned invalid bundle for '{}' (SVG parsing error?)",
                id.ToStdString());
            // Log first 200 chars of SVG for debugging
            std::string svgPreview = svg.substr(0, std::min<size_t>(200, svg.length()));
            Logger::getInstance().warn("  SVG preview: {}", svgPreview);
        }
    } catch (const std::exception& e) {
        Logger::getInstance().error("KalahariArtProvider: Exception creating bundle for '{}': {}",
            id.ToStdString(), e.what());
    }

    // Fallback
    Logger::getInstance().warn("KalahariArtProvider: Returning wxNullBitmap for '{}' (fallback to default provider)",
        id.ToStdString());
    return wxNullBitmap;
}

// ============================================================================
// Helper Methods
// ============================================================================

std::string KalahariArtProvider::replaceColor(const std::string& svg, const wxColour& color) {
    std::string result = svg;

    // Convert wxColour to hex string (#RRGGBB)
    wxString colorHex = wxString::Format("#%02X%02X%02X",
        color.Red(), color.Green(), color.Blue());
    std::string colorStr = colorHex.ToStdString();

    // Replace all occurrences of {COLOR} with actual color
    size_t pos = result.find("{COLOR}");
    while (pos != std::string::npos) {
        result.replace(pos, 7, colorStr); // 7 = length of "{COLOR}"
        pos = result.find("{COLOR}", pos + colorStr.length());
    }

    return result;
}
