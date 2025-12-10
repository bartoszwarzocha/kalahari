/// @file resource_paths.h
/// @brief Platform-aware resource locator for cross-platform resource discovery
///
/// ResourcePaths is a singleton that provides centralized resource path resolution.
/// It searches for resources in multiple platform-specific locations to support:
/// - Development builds (resources next to executable)
/// - macOS bundles (Resources folder inside .app)
/// - Linux FHS installations (/usr/share, /usr/local/share, ~/.local/share)
/// - Windows installed applications
///
/// @example
/// @code
/// auto& paths = ResourcePaths::getInstance();
/// QString iconPath = paths.getIconPath("twotone", "save.svg");
/// QString themePath = paths.getThemePath("Dark");
/// if (!paths.resourcesFound()) {
///     // Handle missing resources
/// }
/// @endcode

#pragma once

#include <QString>
#include <QStringList>

namespace kalahari {
namespace core {

/// @brief Platform-aware resource path resolver singleton
///
/// Provides centralized resource discovery across all supported platforms.
/// Searches multiple locations in priority order and caches the first valid path.
///
/// Search order:
/// 1. Next to executable: <appDir>/resources
/// 2. macOS Bundle: <appDir>/../Resources/resources and <appDir>/../Resources
/// 3. Linux FHS: /usr/share/kalahari/resources, /usr/local/share/kalahari/resources, ~/.local/share/kalahari/resources
/// 4. Development fallbacks: <appDir>/../../resources, <appDir>/../../../resources
class ResourcePaths {
public:
    /// @brief Get the singleton instance
    /// @return Reference to the global ResourcePaths instance
    static ResourcePaths& getInstance();

    // Non-copyable, non-movable
    ResourcePaths(const ResourcePaths&) = delete;
    ResourcePaths& operator=(const ResourcePaths&) = delete;
    ResourcePaths(ResourcePaths&&) = delete;
    ResourcePaths& operator=(ResourcePaths&&) = delete;

    /// @brief Get the base resources directory
    /// @return Absolute path to the resources directory (first valid path found, cached)
    /// @note Returns empty string if no valid resources directory was found
    QString getResourcesDir() const;

    /// @brief Get path to a specific icon file
    /// @param iconTheme Icon theme name (e.g., "twotone", "filled", "outlined", "rounded")
    /// @param iconName Icon file name with extension (e.g., "save.svg", "file_new.svg")
    /// @return Absolute path to the icon file, or empty string if resources not found
    /// @note Does not check if the icon file actually exists
    QString getIconPath(const QString& iconTheme, const QString& iconName) const;

    /// @brief Get path to a theme JSON file
    /// @param themeName Theme name without extension (e.g., "Dark", "Light", "Savanna")
    /// @return Absolute path to the theme JSON file, or empty string if resources not found
    /// @note Does not check if the theme file actually exists
    QString getThemePath(const QString& themeName) const;

    /// @brief Get path to the themes directory
    /// @return Absolute path to <resources>/themes, or empty string if resources not found
    QString getThemesDir() const;

    /// @brief Get path to the icons directory
    /// @return Absolute path to <resources>/icons, or empty string if resources not found
    QString getIconsDir() const;

    /// @brief Check if resources were found during initialization
    /// @return true if a valid resources directory was found, false otherwise
    bool resourcesFound() const;

    /// @brief Get all search paths that were checked during initialization
    /// @return List of all paths that were searched, in order of priority
    /// @note Useful for debugging resource loading issues
    QStringList getSearchPaths() const;

private:
    /// @brief Private constructor (singleton pattern)
    ResourcePaths();

    /// @brief Initialize search paths based on current platform
    void initSearchPaths();

    /// @brief Find the first valid resources directory from search paths
    /// @return Absolute path to valid resources directory, or empty string if none found
    QString findResourcesDir() const;

    /// @brief All paths to search for resources (platform-specific)
    QStringList m_searchPaths;

    /// @brief Cached path to the first valid resources directory found
    QString m_foundResourcesDir;
};

} // namespace core
} // namespace kalahari
