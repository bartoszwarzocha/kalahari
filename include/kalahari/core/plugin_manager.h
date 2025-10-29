/// @file plugin_manager.h
/// @brief Singleton managing plugin lifecycle

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <filesystem>

namespace kalahari {
namespace core {

/// @brief Metadata for a discovered plugin (future use)
struct PluginMetadata {
    std::string id;
    std::string name;
    std::string version;
    std::filesystem::path path;
};

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

private:
    /// @brief Private constructor (singleton)
    PluginManager() = default;

    /// @brief Private destructor
    ~PluginManager() = default;

    std::vector<PluginMetadata> m_plugins;  ///< Discovered plugins
    mutable std::mutex m_mutex;              ///< Thread safety
};

} // namespace core
} // namespace kalahari
