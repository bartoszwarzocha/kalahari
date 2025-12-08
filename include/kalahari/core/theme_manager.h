/// @file theme_manager.h
/// @brief Theme management system for Kalahari

#pragma once

#include "kalahari/core/theme.h"
#include <QObject>
#include <QString>
#include <QColor>
#include <memory>
#include <map>
#include <optional>

namespace kalahari {
namespace core {

/// @brief Manages application themes (load, switch, persist)
///
/// Singleton class responsible for:
/// - Loading themes from JSON files in resources/themes/
/// - Switching between themes
/// - Applying user color overrides
/// - Notifying UI components of theme changes via signals
class ThemeManager : public QObject {
    Q_OBJECT

public:
    /// @brief Get ThemeManager singleton instance
    static ThemeManager& getInstance();

    // Delete copy/move constructors (singleton)
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    ThemeManager(ThemeManager&&) = delete;
    ThemeManager& operator=(ThemeManager&&) = delete;

    /// @brief Load theme from JSON file
    /// @param themeName Theme name (without .json extension)
    /// @return Loaded Theme object
    /// @throws std::runtime_error if theme file not found or malformed
    Theme loadTheme(const QString& themeName);

    /// @brief Get currently active theme
    /// @return Current theme (or default Light theme if none loaded)
    const Theme& getCurrentTheme() const;

    /// @brief Get list of available themes in resources/themes/
    /// @return List of theme names (without .json extension)
    QStringList getAvailableThemes() const;

    /// @brief Apply theme to application
    /// @param theme Theme to apply
    /// Emits themeChanged() signal for UI updates
    void applyTheme(const Theme& theme);

    /// @brief Switch to theme by name
    /// @param themeName Theme name to load and apply
    /// @return true if successful, false if theme not found
    bool switchTheme(const QString& themeName);

    /// @brief Apply user color overrides to current theme
    /// @param overrides Map of color keys to custom colors
    /// Example: {"primary": "#FF0000", "secondary": "#00FF00"}
    void applyColorOverrides(const std::map<std::string, QColor>& overrides);

    /// @brief Set a single color override
    /// @param key Color key (e.g., "primary", "secondary", "palette.window", "log.info")
    /// @param color Color value to set
    /// Supported keys:
    /// - "primary", "secondary", "accent", "background", "text" - Theme colors
    /// - "palette.*" - QPalette colors (window, windowText, base, etc.)
    /// - "log.*" - Log panel colors (trace, debug, info, etc.)
    void setColorOverride(const QString& key, const QColor& color);

    /// @brief Reset all color overrides (restore theme defaults)
    void resetColorOverrides();

    /// @brief Refresh theme application (reapply palette and stylesheet)
    /// Call this after multiple setColorOverride() calls to apply changes
    void refreshTheme();

signals:
    /// @brief Emitted when theme changes
    /// @param theme New active theme (with overrides applied)
    void themeChanged(const Theme& theme);

    /// @brief Emitted after stylesheet applied (for additional widget refresh)
    void themeStyleChanged();

private:
    ThemeManager(); // Private constructor (singleton)
    ~ThemeManager() override = default;

    Theme m_currentTheme;         ///< Currently active theme
    Theme m_baseTheme;            ///< Base theme (before overrides)
    std::map<std::string, QColor> m_overrides; ///< User color overrides

    /// @brief Load theme JSON file from resources/themes/
    /// @param themeName Theme name
    /// @return JSON content
    std::optional<nlohmann::json> loadThemeFile(const QString& themeName);
};

} // namespace core
} // namespace kalahari
