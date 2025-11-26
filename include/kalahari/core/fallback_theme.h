/// @file fallback_theme.h
/// @brief Emergency fallback themes for guaranteed application startup
///
/// This module provides hardcoded minimal Light and Dark themes that are
/// available even when no plugins are loaded. These themes ensure the
/// application can always start and be usable.
///
/// Part of Task #00023: Advanced Theme System with Plugin Architecture

#pragma once

#include "kalahari/core/theme.h"
#include <optional>
#include <QString>
#include <QStringList>

namespace kalahari::core {

/// @brief Emergency fallback theme provider
///
/// Provides minimal hardcoded themes that guarantee application startup
/// even when all plugins fail to load. These themes contain only essential
/// QPalette colors and basic icon colors.
///
/// Usage hierarchy:
/// - Level 1 (Fallback): Used when no plugins available
/// - Level 2 (Core Plugins): Used when core plugins loaded
/// - Level 3 (User Plugins): Used when user plugins installed
class FallbackTheme {
public:
    /// @brief Get minimal Light fallback theme
    /// @return Theme with light color palette
    static Theme getLightTheme();

    /// @brief Get minimal Dark fallback theme
    /// @return Theme with dark color palette
    static Theme getDarkTheme();

    /// @brief Get fallback theme by name
    /// @param name Theme name ("Light" or "Dark")
    /// @return Theme if name matches, nullopt otherwise
    static std::optional<Theme> getThemeByName(const QString& name);

    /// @brief Get list of available fallback theme names
    /// @return List containing "Light (Fallback)" and "Dark (Fallback)"
    static QStringList getAvailableThemes();

    /// @brief Check if currently using a fallback theme
    /// @return true if no plugin themes were loaded
    static bool isUsingFallback();

    /// @brief Set fallback usage flag
    /// @param usingFallback true if using fallback themes
    static void setUsingFallback(bool usingFallback);

private:
    static bool s_usingFallback;
};

} // namespace kalahari::core
