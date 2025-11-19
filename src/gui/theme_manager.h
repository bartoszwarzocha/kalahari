/// @file theme_manager.h
/// @brief Central theme management singleton

#pragma once

#include <string>

// Forward declarations
namespace kalahari {
namespace core {
    class SettingsManager;
}
}

namespace kalahari {
namespace gui {

// ============================================================================
// FontSizePreset Enum
// ============================================================================

/// @brief Font size preset for application-wide font scaling
///
/// Maps to scale factors used by bwxReactive controls:
/// - ExtraSmall: 0.7 (70%)
/// - Small: 0.85 (85%)
/// - Normal: 1.0 (100%) - DEFAULT
/// - Medium: 1.15 (115%)
/// - Large: 1.3 (130%)
/// - ExtraLarge: 1.5 (150%)
enum class FontSizePreset {
    ExtraSmall = 0,  ///< 70% of base size (0.7 scale)
    Small = 1,       ///< 85% of base size (0.85 scale)
    Normal = 2,      ///< 100% of base size (1.0 scale) - DEFAULT
    Medium = 3,      ///< 115% of base size (1.15 scale)
    Large = 4,       ///< 130% of base size (1.3 scale)
    ExtraLarge = 5   ///< 150% of base size (1.5 scale)
};

// ============================================================================
// ThemeManager Singleton
// ============================================================================

/// @brief Central theme management singleton
///
/// Coordinates appearance settings (fonts, colors, icons) across the application.
/// Integrates with SettingsManager and broadcasts changes to bwx::gui::bwxReactive controls.
///
/// **Responsibilities:**
/// - Read `appearance.font_size_preset` from SettingsManager
/// - Calculate scale factor from preset
/// - Broadcast font scale changes to all bwx controls
/// - (Future Phase 2) Manage color themes, icon sets
///
/// **Usage:**
/// ```cpp
/// // Initialize on startup (MainWindow constructor)
/// ThemeManager::getInstance().initialize(settingsManager);
///
/// // Apply changes when user saves settings
/// ThemeManager::getInstance().applyFontSizePreset(FontSizePreset::Large);
/// ```
///
/// **Thread Safety:**
/// - Singleton instance: Thread-safe (C++11 magic static)
/// - Methods: Single-threaded (GUI thread only)
class ThemeManager {
public:
    /// @brief Get singleton instance
    /// @return Reference to singleton instance
    static ThemeManager& getInstance();

    /// @brief Initialize with SettingsManager reference
    ///
    /// Must be called once on application startup (MainWindow constructor).
    /// Reads current font size preset from settings and applies it.
    ///
    /// @param settingsManager Reference to SettingsManager singleton
    void initialize(core::SettingsManager& settingsManager);

    /// @brief Apply font size preset (updates all bwx controls)
    ///
    /// Saves preset to SettingsManager, calculates scale factor, and broadcasts
    /// change to all bwxReactive controls.
    ///
    /// @param preset Font size preset (ExtraSmall...ExtraLarge)
    void applyFontSizePreset(FontSizePreset preset);

    /// @brief Get current font size preset
    /// @return Current preset
    FontSizePreset getCurrentFontSizePreset() const { return m_currentPreset; }

    /// @brief Get current font scale factor
    /// @return Current scale factor (0.7-1.5)
    double getCurrentScale() const { return presetToScale(m_currentPreset); }

    /// @brief Convert preset to scale factor
    /// @param preset Font size preset
    /// @return Scale factor (0.7, 0.85, 1.0, 1.15, 1.3, 1.5)
    static double presetToScale(FontSizePreset preset);

    /// @brief Convert scale to preset (reverse mapping)
    ///
    /// Finds closest matching preset for given scale factor.
    /// Used when loading settings with custom scale values.
    ///
    /// @param scale Scale factor
    /// @return Closest matching preset
    static FontSizePreset scaleToPreset(double scale);

    /// @brief Convert preset to display string
    /// @param preset Font size preset
    /// @return Display string ("Extra Small", "Small", "Normal", "Medium", "Large", "Extra Large")
    static std::string presetToString(FontSizePreset preset);

    /// @brief Convert display string to preset
    /// @param str Display string
    /// @return Preset (Normal if invalid string)
    static FontSizePreset stringToPreset(const std::string& str);

private:
    // Singleton pattern
    ThemeManager() = default;
    ~ThemeManager() = default;

    // Disable copy/move
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    ThemeManager(ThemeManager&&) = delete;
    ThemeManager& operator=(ThemeManager&&) = delete;

    core::SettingsManager* m_settingsManager = nullptr;  ///< Settings reference (non-owning)
    FontSizePreset m_currentPreset = FontSizePreset::Normal;  ///< Current preset
};

} // namespace gui
} // namespace kalahari
