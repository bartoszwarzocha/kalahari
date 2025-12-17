/// @file plugin_manager.h
/// @brief Singleton managing plugin lifecycle

#pragma once

#include <kalahari/core/plugin_manifest.h>
#include <kalahari/core/plugin_archive.h>
#include <pybind11/pybind11.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <filesystem>
#include <optional>
#include <memory>

namespace py = pybind11;

namespace kalahari {
namespace core {

/// @brief Plugin lifecycle states
enum class PluginState {
    Discovered,    ///< Found during discovery, not loaded
    Loading,       ///< Currently being loaded
    Loaded,        ///< Successfully loaded and initialized
    Activated,     ///< Plugin is active and ready
    Error,         ///< Plugin failed to load or encountered an error
    Unloading      ///< Currently being unloaded
};

/// @brief Metadata for a discovered plugin
struct PluginMetadata {
    std::string id;               ///< Unique plugin ID
    std::string name;             ///< Human-readable name
    std::string version;          ///< Plugin version
    std::filesystem::path path;   ///< Path to .kplugin file
    PluginManifest manifest;      ///< Full manifest data
};

/// @brief Runtime instance of a loaded plugin
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif
struct PluginInstance {
    std::string id;                                 ///< Plugin ID
    PluginState state;                              ///< Current lifecycle state
    PluginManifest manifest;                        ///< Plugin manifest
    std::unique_ptr<PluginArchive> archive;         ///< Extracted archive (RAII)
    std::shared_ptr<py::object> module;             ///< Imported Python module
    std::shared_ptr<py::object> instance;           ///< Plugin class instance
    std::string error_message;                      ///< Error description (if state == Error)
};
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

/// @brief Singleton manager for plugins
class PluginManager {
public:
    /// @brief Get singleton instance
    /// @return Reference to PluginManager instance
    static PluginManager& getInstance();

    // Prevent copying/moving
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /// @brief Discover plugins in plugins/ directory
    /// @return Number of plugins discovered
    size_t discoverPlugins();

    /// @brief Load a plugin by ID
    /// @param pluginId Unique plugin identifier
    /// @return true if loaded successfully
    bool loadPlugin(const std::string& pluginId);

    /// @brief Unload a plugin
    /// @param pluginId Plugin to unload
    void unloadPlugin(const std::string& pluginId);

    /// @brief Get list of discovered plugins
    /// @return Vector of plugin metadata
    std::vector<PluginMetadata> getDiscoveredPlugins() const;

    /// @brief Get a specific plugin's metadata by ID
    /// @param pluginId Plugin identifier
    /// @return Pointer to metadata if found, nullptr otherwise
    const PluginMetadata* getPluginMetadata(const std::string& pluginId) const;

    /// @brief Get loaded plugin instance
    /// @param pluginId Plugin identifier
    /// @return Pointer to PluginInstance if loaded, nullptr otherwise
    const PluginInstance* getPluginInstance(const std::string& pluginId) const;

    /// @brief Check if plugin is loaded
    /// @param pluginId Plugin identifier
    /// @return true if plugin is loaded and activated
    bool isPluginLoaded(const std::string& pluginId) const;

private:
    /// @brief Private constructor (singleton)
    PluginManager() = default;

    /// @brief Private destructor
    ~PluginManager() = default;

    /// @brief Read and parse manifest.json from .kplugin archive
    /// @param kpluginPath Path to .kplugin file
    /// @return PluginManifest if successful, std::nullopt otherwise
    std::optional<PluginManifest> readManifestFromArchive(const std::filesystem::path& kpluginPath);

    /// @brief Parse entry_point string (module:class format)
    /// @param entry_point Entry point string from manifest
    /// @return Pair of (module_name, class_name)
    std::pair<std::string, std::string> parseEntryPoint(const std::string& entry_point);

    /// @brief Find plugins directory (fallback chain for production robustness)
    /// @return Path to plugins directory if found, empty path otherwise
    std::filesystem::path getPluginsDirectory() const;

    /// @brief Check if unsigned plugins are allowed via settings
    /// @return true if plugins.allowUnsigned setting is "true"
    [[nodiscard]] bool allowUnsignedPlugins() const;

    std::map<std::string, PluginMetadata> m_plugins;          ///< Discovered plugins (id -> metadata)
    std::map<std::string, PluginInstance> m_loaded_plugins;   ///< Loaded plugins (id -> instance)
    mutable std::mutex m_mutex;                               ///< Thread safety
};

} // namespace core
} // namespace kalahari
