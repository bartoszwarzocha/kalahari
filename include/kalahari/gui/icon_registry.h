/// @file icon_registry.h
/// @brief Central icon management system with customization support
///
/// IconRegistry is the heart of Kalahari's icon system. It provides:
/// - Centralized icon registration and retrieval
/// - Per-icon and global color customization
/// - Context-aware sizing (toolbar, menu, panel, dialog)
/// - User customization (change icons, colors, sizes)
/// - Plugin icon registration
/// - Settings persistence (JSON)
/// - Change notification (EventBus integration)

#pragma once

#include <wx/wx.h>
#include <wx/artprov.h>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace kalahari {
namespace gui {

// ============================================================================
// IconDescriptor - Icon with Customization Options
// ============================================================================

/// @brief Descriptor for a single icon with customization options
struct IconDescriptor {
    std::string defaultSVG;              ///< Embedded SVG from resources
    std::optional<std::string> userSVG;  ///< User-provided custom SVG
    std::optional<wxColour> colorOverride; ///< Per-icon color (overrides theme)
    wxString label;                       ///< Human-readable name ("Save File")

    /// @brief Get effective SVG (user override or default)
    const std::string& getEffectiveSVG() const {
        return userSVG.has_value() ? *userSVG : defaultSVG;
    }

    /// @brief Check if icon has user customization
    bool isCustomized() const {
        return userSVG.has_value() || colorOverride.has_value();
    }

    /// @brief Serialize to JSON object
    wxString toJSON() const;

    /// @brief Deserialize from JSON object
    static IconDescriptor fromJSON(const wxString& json);
};

// ============================================================================
// IconSizeConfig - Size Configuration for Different Contexts
// ============================================================================

/// @brief Icon size configuration for different UI contexts
struct IconSizeConfig {
    int toolbar = 24;   ///< Toolbar icon size (px)
    int menu = 16;      ///< Menu icon size (px)
    int panel = 20;     ///< Panel caption icon size (px)
    int dialog = 32;    ///< Dialog icon size (px)

    /// @brief Get size for wxArtClient
    int getSizeForClient(const wxArtClient& client) const {
        if (client == wxART_TOOLBAR) return toolbar;
        if (client == wxART_MENU) return menu;
        if (client == wxART_OTHER) return panel;
        if (client == wxART_MESSAGE_BOX) return dialog;
        return 16; // fallback
    }

    /// @brief Serialize to JSON object
    wxString toJSON() const;

    /// @brief Deserialize from JSON object
    static IconSizeConfig fromJSON(const wxString& json);
};

// ============================================================================
// IconRegistry - Central Icon Management (Singleton)
// ============================================================================

/// @brief Central icon registry with customization support
///
/// Singleton managing all icon mappings, sizes, colors, and user customizations.
/// Integrates with SettingsManager for persistence and EventBus for synchronization.
///
/// Example usage:
/// @code
/// // At application startup
/// IconRegistry::getInstance().initialize();
///
/// // Register custom icon
/// IconRegistry::getInstance().registerIcon(
///     "MY_ACTION",
///     "<svg>...</svg>",
///     "My Action"
/// );
///
/// // Change theme
/// IconRegistry::getInstance().setTheme(IconRegistry::ColorTheme::Dark);
///
/// // Customize icon color
/// IconRegistry::getInstance().setIconColor("wxID_SAVE", wxColour(255, 0, 0));
///
/// // In ArtProvider
/// std::string svg = IconRegistry::getInstance().getEffectiveSVG("wxID_SAVE");
/// wxColour color = IconRegistry::getInstance().getEffectiveColor("wxID_SAVE");
/// @endcode
class IconRegistry {
public:
    /// @brief Color theme enumeration
    enum class ColorTheme {
        Light,   ///< #212121 (dark icons on light background)
        Dark,    ///< #E0E0E0 (light icons on dark background)
        Custom   ///< User-defined global color
    };

    /// @brief Event fired when icon mapping/color/size changes
    /// Payload: wxString actionId (or "*" for global change)
    static constexpr const char* EVENT_ICON_CHANGED = "kalahari.icon.changed";

    /// @brief Get singleton instance
    static IconRegistry& getInstance();

    /// @brief Initialize registry with default icon mappings
    /// @note Called once at application startup
    void initialize();

    // ========================================================================
    // Icon Registration (called at startup)
    // ========================================================================

    /// @brief Register icon with default SVG
    /// @param actionId wxWidgets action ID (wxID_NEW, wxID_OPEN, custom IDs)
    /// @param defaultSVG Embedded SVG string from resources
    /// @param label Human-readable label ("New File")
    void registerIcon(const wxString& actionId,
                      const std::string& defaultSVG,
                      const wxString& label);

    /// @brief Check if icon is registered
    bool hasIcon(const wxString& actionId) const;

    /// @brief Get all registered action IDs
    std::vector<wxString> getAllActionIds() const;

    // ========================================================================
    // Icon Retrieval (called by ArtProvider)
    // ========================================================================

    /// @brief Get icon descriptor for action
    /// @return IconDescriptor or nullptr if not found
    const IconDescriptor* getIcon(const wxString& actionId) const;

    /// @brief Get effective SVG for action (with user override)
    std::string getEffectiveSVG(const wxString& actionId) const;

    /// @brief Get effective color for action (theme + per-icon override)
    wxColour getEffectiveColor(const wxString& actionId) const;

    /// @brief Get icon label
    wxString getIconLabel(const wxString& actionId) const;

    // ========================================================================
    // User Customization (called by Settings Dialog)
    // ========================================================================

    /// @brief Set custom SVG for action (user override)
    void setCustomIcon(const wxString& actionId, const std::string& svgData);

    /// @brief Clear custom SVG (revert to default)
    void clearCustomIcon(const wxString& actionId);

    /// @brief Set per-icon color override
    void setIconColor(const wxString& actionId, const wxColour& color);

    /// @brief Clear per-icon color (use theme color)
    void clearIconColor(const wxString& actionId);

    /// @brief Reset ALL customizations (factory defaults)
    void resetAllCustomizations();

    // ========================================================================
    // Size Configuration
    // ========================================================================

    /// @brief Set icon sizes for all contexts
    void setSizes(const IconSizeConfig& sizes);

    /// @brief Get current size configuration
    const IconSizeConfig& getSizes() const { return m_sizes; }

    /// @brief Get size for specific client
    int getSizeForClient(const wxArtClient& client) const {
        return m_sizes.getSizeForClient(client);
    }

    // ========================================================================
    // Theme Configuration
    // ========================================================================

    /// @brief Set global color theme
    void setTheme(ColorTheme theme, wxColour customColor = wxNullColour);

    /// @brief Get current theme
    ColorTheme getTheme() const { return m_theme; }

    /// @brief Get theme color (for icons without per-icon override)
    wxColour getThemeColor() const;

    // ========================================================================
    // Persistence (integration with SettingsManager)
    // ========================================================================

    /// @brief Load customizations from settings
    void loadFromSettings();

    /// @brief Save customizations to settings
    void saveToSettings();

private:
    IconRegistry() = default;
    ~IconRegistry() = default;
    IconRegistry(const IconRegistry&) = delete;
    IconRegistry& operator=(const IconRegistry&) = delete;

    /// @brief Fire icon changed event
    void notifyIconChanged(const wxString& actionId);

    // Icon mappings
    std::map<wxString, IconDescriptor> m_icons;

    // Configuration
    IconSizeConfig m_sizes;
    ColorTheme m_theme = ColorTheme::Light;
    wxColour m_customThemeColor;
};

} // namespace gui
} // namespace kalahari
