// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file plugin_manager.h
/// @brief Singleton managing plugin lifecycle

#ifndef KALAHARI_CORE_PLUGIN_MANAGER_H
#define KALAHARI_CORE_PLUGIN_MANAGER_H

#include <kalahari/core/plugin_manifest.h>
#include <kalahari/core/plugin_archive.h>
#include <kalahari/core/python_plugin.h>
// Qt's "slots" keyword would break Python's headers when Qt's come first
#pragma push_macro("slots")
#undef slots
#include <pybind11/pybind11.h>
#pragma pop_macro("slots")
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <filesystem>
#include <functional>
#include <optional>
#include <memory>

namespace py = pybind11;

namespace kalahari::core {

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
    std::string moduleName;                         ///< The name its entry module is imported
                                                    ///< under (unique to the plugin)
    std::shared_ptr<PythonPlugin> extensions;       ///< Its extension points, registered in
                                                    ///< ExtensionPointRegistry while it is loaded
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

    /// @brief The folders the plugins are looked for in, in order: a plugin found in an earlier
    ///        folder hides one with the same id in a later folder
    ///
    /// Empty (the default): the user's folder (userPluginsFolder()) and the program's
    /// (plugins/ in the current folder, next to the program or one level up; the first found).
    void setPluginFolders(std::vector<std::filesystem::path> folders);

    /// @brief The folders the plugins are looked for in, in order
    [[nodiscard]] std::vector<std::filesystem::path> pluginFolders() const;

    /// @brief The folder of the plugins the writer installs: plugins/ next to the settings
    [[nodiscard]] static std::filesystem::path userPluginsFolder();

    /// @brief Discover plugins in the plugin folders
    /// @return Number of plugins discovered
    size_t discoverPlugins();

    /// @brief Discover the plugins and load every enabled one, starting Python only when a
    ///        plugin is to be loaded
    /// @return Number of plugins loaded
    size_t loadEnabledPlugins();

    /// @brief Unload every loaded plugin (the end of the program)
    void unloadAll();

    /// @brief Whether the writer has not turned the plugin off (plugins.disabled)
    [[nodiscard]] static bool isPluginEnabled(const std::string& pluginId);

    /// @brief Turn the plugin on (loads it) or off (unloads it), and remember it
    /// @return true when the plugin is in the state asked for (an enabled one is loaded)
    bool setPluginEnabled(const std::string& pluginId, bool enabled);

    /// @brief Install the plugin file @p file (and its signature next to it) into the user's
    ///        folder, in place of an older version, and load it when it is enabled
    /// @param error Why it was not installed
    /// @return The id of the plugin installed
    std::optional<std::string> installPlugin(const std::filesystem::path& file,
                                             std::string* error = nullptr);

    /// @brief Unload the plugin and delete it from the user's folder; a plugin of the
    ///        program's folder cannot be removed
    /// @param removeSettings Also forget its settings (plugins.settings)
    /// @param error Why it was not removed
    bool removePlugin(const std::string& pluginId, bool removeSettings = false,
                      std::string* error = nullptr);

    /// @brief Whether the plugin is in the user's folder, so it can be removed
    [[nodiscard]] bool isUserPlugin(const std::string& pluginId) const;

    /// @brief What the loaded plugin added to the program (empty when it is not loaded)
    [[nodiscard]] PluginContributions contributionsOf(const std::string& pluginId) const;

    /// @brief The licence check of a plugin before it is loaded
    ///
    /// Gives why the plugin may not be loaded, or nothing when it may. The default lets every
    /// plugin load; the shape of the licences of paid add-ons is not decided yet.
    using LicenseCheck = std::function<std::optional<std::string>(const PluginManifest&)>;

    /// @brief Set the licence check (none: every plugin may load)
    void setLicenseCheck(LicenseCheck check);

    /// @brief Called after a plugin was loaded or unloaded (its id), on the thread that did it
    using ChangeListener = std::function<void(const std::string&)>;

    /// @brief Hear of the plugins loaded and unloaded; gives the id to stop with
    int addChangeListener(ChangeListener listener);

    /// @brief Stop hearing of the plugins
    void removeChangeListener(int id);

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
    PluginManager();

    /// @brief Private destructor
    ///
    /// Releases the Python objects of loaded plugins safely. The destruction
    /// order of this singleton and PythonInterpreter is not fixed, so the
    /// interpreter may already be finalized here.
    ~PluginManager();

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

    /// @brief Discover the plugins of one folder; one already found is not replaced
    size_t discoverIn(const std::filesystem::path& folder);

    /// @brief Read and check one plugin file
    /// @param error Why it is not a plugin to load
    std::optional<PluginMetadata> examine(const std::filesystem::path& file, std::string* error);

    /// @brief Put @p file and its signature in place of the installed @p target and its
    ///        signature, checked before; the installed pair stays when a step fails
    /// @return Why it was not replaced; empty when it was
    std::string replaceInstalled(const std::filesystem::path& file,
                                 const std::filesystem::path& target,
                                 const std::string& pluginId);

    /// @brief Import the entry module of an extracted plugin under a name of its own
    static py::object importEntryModule(const std::filesystem::path& folder,
                                        const std::string& module, const std::string& uniqueName);

    /// @brief Tell the listeners of a change of the plugin
    void notifyChanged(const std::string& pluginId);

    std::map<std::string, PluginMetadata> m_plugins;          ///< Discovered plugins (id -> metadata)
    std::map<std::string, PluginInstance> m_loaded_plugins;   ///< Loaded plugins (id -> instance)
    std::vector<std::filesystem::path> m_folders;             ///< setPluginFolders()
    LicenseCheck m_licenseCheck;                              ///< setLicenseCheck()
    std::map<int, ChangeListener> m_listeners;                ///< addChangeListener()
    int m_nextListener = 1;
    mutable std::mutex m_mutex;                               ///< Thread safety
};

} // namespace kalahari::core

#endif  // KALAHARI_CORE_PLUGIN_MANAGER_H
