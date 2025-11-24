/// @file icon_registry.h
/// @brief Central icon management system with runtime theming and customization
///
/// IconRegistry is the heart of Kalahari's icon system. It provides:
/// - Centralized icon registration and retrieval
/// - Per-icon and global color customization (PRIMARY + SECONDARY)
/// - Context-aware sizing (toolbar, menu, panel, dialog)
/// - User customization (change icons, colors, sizes)
/// - Settings persistence (JSON via SettingsManager)
/// - QPixmap caching for performance
///
/// Ported from wxWidgets IconRegistry and extended for Qt6 + TwoTone support.

#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <map>
#include <optional>

namespace kalahari {
namespace core {

// ============================================================================
// IconDescriptor - Icon with Customization Options
// ============================================================================

/// @brief Descriptor for a single icon with customization options
struct IconDescriptor {
    QString defaultSVGPath;                       ///< Default SVG file path (e.g., "resources/icons/twotone/save.svg")
    std::optional<QString> userSVGPath;           ///< User-provided custom SVG path (optional override)
    std::optional<QColor> primaryOverride;        ///< Per-icon PRIMARY color override (optional)
    std::optional<QColor> secondaryOverride;      ///< Per-icon SECONDARY color override (optional)
    QString label;                                ///< Human-readable label ("Save File")

    /// @brief Get effective SVG path (user override or default)
    QString getEffectiveSVGPath() const {
        return userSVGPath.has_value() ? *userSVGPath : defaultSVGPath;
    }

    /// @brief Check if icon has user customization (SVG or colors)
    bool isCustomized() const {
        return userSVGPath.has_value() || primaryOverride.has_value() || secondaryOverride.has_value();
    }
};

// ============================================================================
// ThemeConfig - Theme Configuration (PRIMARY + SECONDARY Colors)
// ============================================================================

/// @brief Theme configuration with PRIMARY and SECONDARY colors
struct ThemeConfig {
    QColor primaryColor;   ///< PRIMARY color (main icon shape, default: #424242)
    QColor secondaryColor; ///< SECONDARY color (TwoTone accent, default: #757575)
    QString name;          ///< Theme name ("Light", "Dark", "Custom")

    /// @brief Default Light theme (dark icons on light background)
    static const ThemeConfig DEFAULT_LIGHT;

    /// @brief Default Dark theme (light icons on dark background)
    static const ThemeConfig DEFAULT_DARK;
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

    /// @brief Default size configuration
    static const IconSizeConfig DEFAULT_SIZES;
};

// ============================================================================
// IconRegistry - Central Icon Management (Singleton)
// ============================================================================

/// @brief Central icon registry with runtime theming and customization
///
/// Singleton managing all icon mappings, sizes, colors, and user customizations.
/// Integrates with SettingsManager for persistence and QPixmap caching for performance.
///
/// Example usage:
/// @code
/// // At application startup
/// IconRegistry::getInstance().initialize();
///
/// // Register icons
/// IconRegistry::getInstance().registerIcon(
///     "file.save",
///     "resources/icons/twotone/save.svg",
///     "Save File"
/// );
///
/// // Get icon with current theme
/// QIcon icon = IconRegistry::getInstance().getIcon("file.save", "twotone", 24);
///
/// // Change theme
/// IconRegistry::getInstance().setThemeColors(QColor("#2196F3"), QColor("#90CAF9"), "Blue");
///
/// // Customize icon color
/// IconRegistry::getInstance().setIconPrimaryColor("file.save", QColor("#FF0000"));
/// @endcode
class IconRegistry {
public:
    /// @brief Get singleton instance
    static IconRegistry& getInstance();

    /// @brief Initialize registry (load settings, log initialization)
    /// @note Called once at application startup
    void initialize();

    // ========================================================================
    // Icon Registration (called at startup)
    // ========================================================================

    /// @brief Register icon with default SVG path
    /// @param actionId Unique action ID (e.g., "file.save", "edit.copy")
    /// @param defaultSVGPath Default SVG file path (e.g., "resources/icons/twotone/save.svg")
    /// @param label Human-readable label ("Save File")
    void registerIcon(const QString& actionId,
                      const QString& defaultSVGPath,
                      const QString& label);

    /// @brief Check if icon is registered
    bool hasIcon(const QString& actionId) const;

    /// @brief Get all registered action IDs (sorted alphabetically)
    QStringList getAllIconIds() const;

    // ========================================================================
    // Icon Retrieval (called by Command Registry, UI)
    // ========================================================================

    /// @brief Get icon with color replacement and caching
    /// @param actionId Action ID (e.g., "file.save")
    /// @param theme Theme name (e.g., "twotone", "rounded", "outlined")
    /// @param size Icon size in pixels (e.g., 24)
    /// @return QIcon with colors applied, or empty QIcon if not found
    QIcon getIcon(const QString& actionId, const QString& theme, int size);

    /// @brief Get icon descriptor for action
    /// @return IconDescriptor pointer or nullptr if not found
    const IconDescriptor* getIconDescriptor(const QString& actionId) const;

    /// @brief Get icon label
    QString getIconLabel(const QString& actionId) const;

    // ========================================================================
    // User Customization (called by Settings Dialog, future Icon Manager)
    // ========================================================================

    /// @brief Set custom SVG path for action (user override)
    void setCustomIconPath(const QString& actionId, const QString& svgPath);

    /// @brief Clear custom SVG path (revert to default)
    void clearCustomIconPath(const QString& actionId);

    /// @brief Set per-icon PRIMARY color override
    void setIconPrimaryColor(const QString& actionId, const QColor& color);

    /// @brief Set per-icon SECONDARY color override
    void setIconSecondaryColor(const QString& actionId, const QColor& color);

    /// @brief Clear per-icon color overrides (revert to theme colors)
    void clearIconColors(const QString& actionId);

    /// @brief Reset ALL customizations (factory defaults)
    void resetAllCustomizations();

    // ========================================================================
    // Size Configuration
    // ========================================================================

    /// @brief Set icon sizes for all contexts
    void setSizes(const IconSizeConfig& sizes);

    /// @brief Get current size configuration
    const IconSizeConfig& getSizes() const { return m_sizes; }

    /// @brief Reset sizes to defaults
    void resetSizes();

    // ========================================================================
    // Theme Configuration
    // ========================================================================

    /// @brief Set global theme colors
    /// @param primary PRIMARY color (main icon shape)
    /// @param secondary SECONDARY color (TwoTone accent)
    /// @param name Theme name ("Light", "Dark", "Custom")
    void setThemeColors(const QColor& primary, const QColor& secondary, const QString& name);

    /// @brief Get current theme configuration
    const ThemeConfig& getThemeConfig() const { return m_theme; }

    /// @brief Reset theme to default Light
    void resetTheme();

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

    // ========================================================================
    // Internal Helpers
    // ========================================================================

    /// @brief Load SVG file content from disk
    /// @param filePath Absolute or relative SVG file path
    /// @return SVG content as QString, or empty string if error
    QString loadSVGFromFile(const QString& filePath) const;

    /// @brief Replace {COLOR_PRIMARY} and {COLOR_SECONDARY} placeholders
    /// @param svgContent Original SVG content with placeholders
    /// @param primary PRIMARY color (QColor::name() hex string)
    /// @param secondary SECONDARY color (QColor::name() hex string)
    /// @return Modified SVG content with colors applied
    QString replaceColorPlaceholders(const QString& svgContent,
                                     const QColor& primary,
                                     const QColor& secondary) const;

    /// @brief Render SVG to QPixmap
    /// @param svgContent SVG content (with colors already replaced)
    /// @param size Pixel size (width = height)
    /// @return Rendered QPixmap, or empty QPixmap if error
    QPixmap renderSVGToPixmap(const QString& svgContent, int size) const;

    /// @brief Get effective PRIMARY color for icon (per-icon override or theme)
    QColor getEffectivePrimaryColor(const QString& actionId) const;

    /// @brief Get effective SECONDARY color for icon (per-icon override or theme)
    QColor getEffectiveSecondaryColor(const QString& actionId) const;

    /// @brief Construct cache key for QPixmap cache
    /// @param actionId Action ID
    /// @param theme Theme name
    /// @param size Icon size
    /// @param primary PRIMARY color
    /// @param secondary SECONDARY color
    /// @return Cache key (e.g., "file.save_twotone_24_#424242_#757575")
    QString constructCacheKey(const QString& actionId,
                              const QString& theme,
                              int size,
                              const QColor& primary,
                              const QColor& secondary) const;

    /// @brief Clear cache entries matching pattern (e.g., "file.save_*")
    void clearCachePattern(const QString& pattern);

    /// @brief Clear entire cache (e.g., on theme change)
    void clearCache();

    // ========================================================================
    // Member Variables
    // ========================================================================

    /// @brief Icon registry (actionId → IconDescriptor)
    std::map<QString, IconDescriptor> m_icons;

    /// @brief Current theme configuration
    ThemeConfig m_theme = ThemeConfig::DEFAULT_LIGHT;

    /// @brief Current size configuration
    IconSizeConfig m_sizes = IconSizeConfig::DEFAULT_SIZES;

    /// @brief QPixmap cache (cacheKey → QPixmap)
    /// Cache key format: "{actionId}_{theme}_{size}_{primaryColor}_{secondaryColor}"
    std::map<QString, QPixmap> m_pixmapCache;
};

} // namespace core
} // namespace kalahari
