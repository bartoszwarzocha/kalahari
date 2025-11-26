/// @file fallback_theme.cpp
/// @brief Implementation of emergency fallback themes
///
/// Part of Task #00023: Advanced Theme System with Plugin Architecture

#include "kalahari/core/fallback_theme.h"
#include "kalahari/core/logger.h"

namespace kalahari::core {

// Static member initialization
bool FallbackTheme::s_usingFallback = true;

Theme FallbackTheme::getLightTheme() {
    Theme theme;

    // Meta information
    theme.name = "Light (Fallback)";
    theme.version = "1.0";
    theme.author = "Kalahari";
    theme.description = "Emergency fallback light theme";

    // Main colors (essential QPalette colors)
    // Light theme: darker primary for icons, lighter secondary for twotone accents
    theme.colors.primary = QColor("#333333");
    theme.colors.secondary = QColor("#999999");
    theme.colors.accent = QColor("#0078D4");
    theme.colors.background = QColor("#FFFFFF");
    theme.colors.text = QColor("#000000");

    // Log panel colors (note: warning/error will be added in Phase 2)
    theme.log.info = QColor("#000000");
    theme.log.debug = QColor("#666666");
    theme.log.background = QColor("#F8F9FA");

    Logger::getInstance().debug("FallbackTheme: Created Light fallback theme");
    return theme;
}

Theme FallbackTheme::getDarkTheme() {
    Theme theme;

    // Meta information
    theme.name = "Dark (Fallback)";
    theme.version = "1.0";
    theme.author = "Kalahari";
    theme.description = "Emergency fallback dark theme";

    // Main colors (essential QPalette colors)
    // Dark theme: lighter primary for icons, darker secondary for twotone accents
    theme.colors.primary = QColor("#999999");
    theme.colors.secondary = QColor("#333333");
    theme.colors.accent = QColor("#0078D4");
    theme.colors.background = QColor("#1E1E1E");
    theme.colors.text = QColor("#FFFFFF");

    // Log panel colors (note: warning/error will be added in Phase 2)
    theme.log.info = QColor("#FFFFFF");
    theme.log.debug = QColor("#808080");
    theme.log.background = QColor("#252526");

    Logger::getInstance().debug("FallbackTheme: Created Dark fallback theme");
    return theme;
}

std::optional<Theme> FallbackTheme::getThemeByName(const QString& name) {
    QString lowerName = name.toLower();

    if (lowerName == "light" || lowerName == "light (fallback)") {
        return getLightTheme();
    }
    if (lowerName == "dark" || lowerName == "dark (fallback)") {
        return getDarkTheme();
    }

    return std::nullopt;
}

QStringList FallbackTheme::getAvailableThemes() {
    return {"Light (Fallback)", "Dark (Fallback)"};
}

bool FallbackTheme::isUsingFallback() {
    return s_usingFallback;
}

void FallbackTheme::setUsingFallback(bool usingFallback) {
    s_usingFallback = usingFallback;
    if (usingFallback) {
        Logger::getInstance().warn("FallbackTheme: Using emergency fallback themes");
    } else {
        Logger::getInstance().info("FallbackTheme: Plugin themes available");
    }
}

} // namespace kalahari::core
