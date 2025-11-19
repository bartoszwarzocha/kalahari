/// @file theme_manager.cpp
/// @brief Implementation of ThemeManager singleton

#include "theme_manager.h"
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/logger.h>
#include <bwx_sdk/bwx_gui/bwx_reactive.h>
#include <cmath>
#include <algorithm>

namespace kalahari {
namespace gui {

// ============================================================================
// Singleton Access
// ============================================================================

ThemeManager& ThemeManager::getInstance() {
    static ThemeManager instance;
    return instance;
}

// ============================================================================
// Initialization
// ============================================================================

void ThemeManager::initialize(core::SettingsManager& settingsManager) {
    m_settingsManager = &settingsManager;

    // Read current preset from settings (default: Normal = 2)
    int presetValue = m_settingsManager->get<int>("appearance.font_size_preset", 2);

    // Validate range (0-5)
    if (presetValue < 0 || presetValue > 5) {
        core::Logger::getInstance().warn(
            "ThemeManager: Invalid font size preset {} in settings, using Normal",
            presetValue
        );
        presetValue = 2;
    }

    m_currentPreset = static_cast<FontSizePreset>(presetValue);

    core::Logger::getInstance().info(
        "ThemeManager: Initialized with preset '{}' (scale {:.2f})",
        presetToString(m_currentPreset),
        presetToScale(m_currentPreset)
    );

    // Apply current preset on startup
    applyFontSizePreset(m_currentPreset);
}

// ============================================================================
// Font Size Preset Management
// ============================================================================

void ThemeManager::applyFontSizePreset(FontSizePreset preset) {
    m_currentPreset = preset;

    // Save to SettingsManager
    if (m_settingsManager) {
        m_settingsManager->set("appearance.font_size_preset", static_cast<int>(preset));
    }

    // Calculate scale factor
    double scale = presetToScale(preset);

    // Broadcast to all bwx controls
    bwx::gui::bwxReactive::broadcastFontScaleChange(scale);

    core::Logger::getInstance().info(
        "ThemeManager: Applied font size preset '{}' (scale {:.2f})",
        presetToString(preset),
        scale
    );
}

// ============================================================================
// Conversion: Preset <-> Scale
// ============================================================================

double ThemeManager::presetToScale(FontSizePreset preset) {
    switch (preset) {
        case FontSizePreset::ExtraSmall: return 0.7;
        case FontSizePreset::Small:      return 0.85;
        case FontSizePreset::Normal:     return 1.0;
        case FontSizePreset::Medium:     return 1.15;
        case FontSizePreset::Large:      return 1.3;
        case FontSizePreset::ExtraLarge: return 1.5;
        default:                         return 1.0;
    }
}

FontSizePreset ThemeManager::scaleToPreset(double scale) {
    // Find closest matching preset
    // Scale values: 0.7, 0.85, 1.0, 1.15, 1.3, 1.5

    // Define thresholds (midpoints between scale values)
    const double threshold_XS_S  = (0.7 + 0.85) / 2.0;  // 0.775
    const double threshold_S_N   = (0.85 + 1.0) / 2.0;  // 0.925
    const double threshold_N_M   = (1.0 + 1.15) / 2.0;  // 1.075
    const double threshold_M_L   = (1.15 + 1.3) / 2.0;  // 1.225
    const double threshold_L_XL  = (1.3 + 1.5) / 2.0;   // 1.4

    if (scale < threshold_XS_S) {
        return FontSizePreset::ExtraSmall;
    } else if (scale < threshold_S_N) {
        return FontSizePreset::Small;
    } else if (scale < threshold_N_M) {
        return FontSizePreset::Normal;
    } else if (scale < threshold_M_L) {
        return FontSizePreset::Medium;
    } else if (scale < threshold_L_XL) {
        return FontSizePreset::Large;
    } else {
        return FontSizePreset::ExtraLarge;
    }
}

// ============================================================================
// Conversion: Preset <-> String
// ============================================================================

std::string ThemeManager::presetToString(FontSizePreset preset) {
    switch (preset) {
        case FontSizePreset::ExtraSmall: return "Extra Small";
        case FontSizePreset::Small:      return "Small";
        case FontSizePreset::Normal:     return "Normal";
        case FontSizePreset::Medium:     return "Medium";
        case FontSizePreset::Large:      return "Large";
        case FontSizePreset::ExtraLarge: return "Extra Large";
        default:                         return "Normal";
    }
}

FontSizePreset ThemeManager::stringToPreset(const std::string& str) {
    if (str == "Extra Small") {
        return FontSizePreset::ExtraSmall;
    } else if (str == "Small") {
        return FontSizePreset::Small;
    } else if (str == "Normal") {
        return FontSizePreset::Normal;
    } else if (str == "Medium") {
        return FontSizePreset::Medium;
    } else if (str == "Large") {
        return FontSizePreset::Large;
    } else if (str == "Extra Large") {
        return FontSizePreset::ExtraLarge;
    } else {
        // Invalid string, return default
        return FontSizePreset::Normal;
    }
}

} // namespace gui
} // namespace kalahari
