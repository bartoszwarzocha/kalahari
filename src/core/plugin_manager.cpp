/// @file plugin_manager.cpp
/// @brief Plugin manager implementation

#include <kalahari/core/plugin_manager.h>
#include <kalahari/core/logger.h>
#include <zip.h>
#include <fstream>
#include <sstream>
#include <vector>

#include <climits>    // PATH_MAX (cross-platform)

#ifdef _WIN32
#include <windows.h>  // GetModuleFileNameW
#else
#include <unistd.h>   // readlink
#endif

namespace kalahari {
namespace core {

PluginManager& PluginManager::getInstance() {
    static PluginManager instance;
    return instance;
}

std::filesystem::path PluginManager::getPluginsDirectory() const {
    // Fallback chain for robust plugin discovery
    // (handles development, tests, portable apps, system installations)

    std::vector<std::filesystem::path> search_paths;

    // Strategy 1: Current working directory (for development/tests)
    search_paths.push_back("plugins");

    // Strategy 2-3: Relative to executable (for production)
#ifdef _WIN32
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) != 0) {
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();

        // Strategy 2: exe_dir/plugins (portable app)
        search_paths.push_back(exeDir / "plugins");

        // Strategy 3: exe_dir/../plugins (if exe in bin/ subdirectory)
        search_paths.push_back(exeDir.parent_path() / "plugins");
    }
#else
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();

        // Strategy 2: exe_dir/plugins (portable app)
        search_paths.push_back(exeDir / "plugins");

        // Strategy 3: exe_dir/../plugins (Unix install: /usr/local/bin → /usr/local/plugins)
        search_paths.push_back(exeDir.parent_path() / "plugins");
    }
#endif

    // Try each path in order
    for (const auto& path : search_paths) {
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            Logger::getInstance().debug("PluginManager: Found plugins directory: {}", path.string());
            return path;
        }
    }

    // Not found - return empty path
    Logger::getInstance().warn("PluginManager: plugins/ directory not found in any search path");
    return std::filesystem::path();
}

size_t PluginManager::discoverPlugins() {
    std::lock_guard<std::mutex> lock(m_mutex);
    Logger::getInstance().info("PluginManager: Discovering plugins...");

    m_plugins.clear();

    // Find plugins directory (fallback chain: CWD → exe_dir → exe_dir/..)
    std::filesystem::path plugins_dir = getPluginsDirectory();

    // Check if plugins directory was found
    if (plugins_dir.empty()) {
        // Warning already logged in getPluginsDirectory()
        return 0;
    }

    size_t discovered = 0;

    // Iterate through plugins directory
    for (const auto& entry : std::filesystem::directory_iterator(plugins_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        if (entry.path().extension() != ".kplugin") {
            continue;
        }

        try {
            // Read manifest from .kplugin archive
            auto manifest_opt = readManifestFromArchive(entry.path());

            if (!manifest_opt.has_value()) {
                Logger::getInstance().warn("PluginManager: Failed to read manifest from: {}",
                                          entry.path().filename().string());
                continue;
            }

            PluginManifest manifest = manifest_opt.value();

            // Validate manifest
            if (!manifest.validate()) {
                Logger::getInstance().warn("PluginManager: Invalid manifest in: {}",
                                          entry.path().filename().string());
                continue;
            }

            // Store plugin metadata
            PluginMetadata metadata{
                .id = manifest.id,
                .name = manifest.name,
                .version = manifest.version,
                .path = entry.path(),
                .manifest = manifest
            };

            m_plugins[manifest.id] = metadata;
            discovered++;

            Logger::getInstance().info("PluginManager: Discovered plugin: {} v{} ({})",
                                      manifest.name, manifest.version, manifest.id);

        } catch (const std::exception& e) {
            Logger::getInstance().error("PluginManager: Error discovering plugin {}: {}",
                                       entry.path().filename().string(), e.what());
        }
    }

    Logger::getInstance().info("PluginManager: Plugin discovery complete. Found: {} plugins",
                               discovered);
    return discovered;
}

std::optional<PluginManifest> PluginManager::readManifestFromArchive(const std::filesystem::path& kpluginPath) {
    int zip_error = 0;

    // Open ZIP archive
    zip_t* archive = zip_open(kpluginPath.string().c_str(), ZIP_RDONLY, &zip_error);

    if (!archive) {
        zip_error_t error;
        zip_error_init_with_code(&error, zip_error);
        Logger::getInstance().error("PluginManager: Failed to open archive {}: {}",
                                   kpluginPath.filename().string(),
                                   zip_error_strerror(&error));
        zip_error_fini(&error);
        return std::nullopt;
    }

    // Locate manifest.json in archive
    zip_int64_t manifest_index = zip_name_locate(archive, "manifest.json", 0);

    if (manifest_index < 0) {
        Logger::getInstance().error("PluginManager: manifest.json not found in {}",
                                   kpluginPath.filename().string());
        zip_close(archive);
        return std::nullopt;
    }

    // Open manifest.json for reading
    zip_file_t* manifest_file = zip_fopen_index(archive, manifest_index, 0);

    if (!manifest_file) {
        Logger::getInstance().error("PluginManager: Failed to open manifest.json in {}",
                                   kpluginPath.filename().string());
        zip_close(archive);
        return std::nullopt;
    }

    // Read manifest.json content
    std::string manifest_content;
    char buffer[4096];
    zip_int64_t bytes_read = 0;

    while ((bytes_read = zip_fread(manifest_file, buffer, sizeof(buffer))) > 0) {
        manifest_content.append(buffer, static_cast<size_t>(bytes_read));
    }

    zip_fclose(manifest_file);
    zip_close(archive);

    if (bytes_read < 0) {
        Logger::getInstance().error("PluginManager: Error reading manifest.json from {}",
                                   kpluginPath.filename().string());
        return std::nullopt;
    }

    // Parse JSON
    try {
        json manifest_json = json::parse(manifest_content);
        PluginManifest manifest = PluginManifest::fromJson(manifest_json);
        return manifest;

    } catch (const json::exception& e) {
        Logger::getInstance().error("PluginManager: JSON parse error in {}: {}",
                                   kpluginPath.filename().string(), e.what());
        return std::nullopt;
    }
}

bool PluginManager::loadPlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Logger::getInstance().info("PluginManager: Loading plugin '{}'", pluginId);

    // Check if plugin was discovered
    auto plugin_it = m_plugins.find(pluginId);
    if (plugin_it == m_plugins.end()) {
        Logger::getInstance().error("PluginManager: Plugin '{}' not found (not discovered)", pluginId);
        return false;
    }

    // Check if already loaded
    if (m_loaded_plugins.find(pluginId) != m_loaded_plugins.end()) {
        Logger::getInstance().warn("PluginManager: Plugin '{}' is already loaded", pluginId);
        return true; // Already loaded is success
    }

    const PluginMetadata& metadata = plugin_it->second;

    // Create plugin instance
    PluginInstance instance;
    instance.id = pluginId;
    instance.state = PluginState::Loading;
    instance.manifest = metadata.manifest;

    try {
        // Step 1: Extract .kplugin archive
        Logger::getInstance().debug("PluginManager: Extracting plugin archive: {}", metadata.path.string());
        instance.archive = std::make_unique<PluginArchive>(metadata.path);

        if (!instance.archive->isValid()) {
            instance.state = PluginState::Error;
            instance.error_message = "Failed to extract plugin archive";
            Logger::getInstance().error("PluginManager: {}", instance.error_message);
            m_loaded_plugins[pluginId] = std::move(instance);
            return false;
        }

        auto extracted_path = instance.archive->getExtractedPath();
        Logger::getInstance().info("PluginManager: Plugin extracted to: {}", extracted_path.string());

        // Step 2: Add extracted directory to Python sys.path
        {
            py::gil_scoped_acquire gil;
            py::module_ sys = py::module_::import("sys");
            py::list path = sys.attr("path");
            path.append(extracted_path.string());
            Logger::getInstance().debug("PluginManager: Added to sys.path: {}", extracted_path.string());
        }

        // Step 3: Parse entry_point (module:class)
        auto [module_name, class_name] = parseEntryPoint(metadata.manifest.entry_point);
        Logger::getInstance().debug("PluginManager: Entry point: {}:{}", module_name, class_name);

        // Step 4: Import Python module
        {
            py::gil_scoped_acquire gil;
            Logger::getInstance().debug("PluginManager: Importing module: {}", module_name);
            instance.module = std::make_shared<py::object>(py::module_::import(module_name.c_str()));
        }

        // Step 5: Get plugin class and create instance
        {
            py::gil_scoped_acquire gil;
            Logger::getInstance().debug("PluginManager: Creating plugin instance: {}", class_name);

            if (!py::hasattr(*instance.module, class_name.c_str())) {
                throw std::runtime_error("Plugin class '" + class_name + "' not found in module '" + module_name + "'");
            }

            py::object plugin_class = instance.module->attr(class_name.c_str());
            instance.instance = std::make_shared<py::object>(plugin_class());
        }

        instance.state = PluginState::Loaded;
        Logger::getInstance().info("PluginManager: Plugin '{}' loaded successfully", pluginId);

        // Step 6: Call on_init() if exists
        {
            py::gil_scoped_acquire gil;

            if (py::hasattr(*instance.instance, "on_init")) {
                Logger::getInstance().debug("PluginManager: Calling on_init()");
                instance.instance->attr("on_init")();
                Logger::getInstance().debug("PluginManager: on_init() completed");
            }
        }

        // Step 7: Call on_activate() if exists
        {
            py::gil_scoped_acquire gil;

            if (py::hasattr(*instance.instance, "on_activate")) {
                Logger::getInstance().debug("PluginManager: Calling on_activate()");
                instance.instance->attr("on_activate")();
                Logger::getInstance().debug("PluginManager: on_activate() completed");
            }
        }

        instance.state = PluginState::Activated;
        Logger::getInstance().info("PluginManager: Plugin '{}' activated successfully", pluginId);

        // Store loaded plugin
        m_loaded_plugins[pluginId] = std::move(instance);

        return true;

    } catch (const py::error_already_set& e) {
        instance.state = PluginState::Error;
        instance.error_message = std::string("Python error: ") + e.what();
        Logger::getInstance().error("PluginManager: Failed to load plugin '{}': {}", pluginId, instance.error_message);
        m_loaded_plugins[pluginId] = std::move(instance);
        return false;

    } catch (const std::exception& e) {
        instance.state = PluginState::Error;
        instance.error_message = std::string("C++ error: ") + e.what();
        Logger::getInstance().error("PluginManager: Failed to load plugin '{}': {}", pluginId, instance.error_message);
        m_loaded_plugins[pluginId] = std::move(instance);
        return false;
    }
}

void PluginManager::unloadPlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Logger::getInstance().info("PluginManager: Unloading plugin '{}'", pluginId);

    // Check if plugin is loaded
    auto plugin_it = m_loaded_plugins.find(pluginId);
    if (plugin_it == m_loaded_plugins.end()) {
        Logger::getInstance().warn("PluginManager: Plugin '{}' is not loaded", pluginId);
        return;
    }

    PluginInstance& instance = plugin_it->second;

    try {
        instance.state = PluginState::Unloading;

        // Step 1: Call on_deactivate() if exists
        if (instance.instance && instance.instance->ptr() != nullptr) {
            py::gil_scoped_acquire gil;

            if (py::hasattr(*instance.instance, "on_deactivate")) {
                Logger::getInstance().debug("PluginManager: Calling on_deactivate()");
                instance.instance->attr("on_deactivate")();
                Logger::getInstance().debug("PluginManager: on_deactivate() completed");
            }
        }

        // Step 2: Remove from sys.path (if archive is valid)
        if (instance.archive && instance.archive->isValid()) {
            auto extracted_path = instance.archive->getExtractedPath();

            py::gil_scoped_acquire gil;
            py::module_ sys = py::module_::import("sys");
            py::list path = sys.attr("path");

            // Find and remove the path
            std::string path_str = extracted_path.string();
            for (size_t i = 0; i < py::len(path); ++i) {
                std::string item = py::str(path[i]);
                if (item == path_str) {
                    path.attr("pop")(i);
                    Logger::getInstance().debug("PluginManager: Removed from sys.path: {}", path_str);
                    break;
                }
            }
        }

        // Step 3: Clear Python objects (will decrement refcount)
        instance.instance.reset();
        instance.module.reset();

        // Step 4: Archive destructor will clean up temp files automatically

        Logger::getInstance().info("PluginManager: Plugin '{}' unloaded successfully", pluginId);

    } catch (const py::error_already_set& e) {
        Logger::getInstance().error("PluginManager: Python error during unload of '{}': {}", pluginId, e.what());
    } catch (const std::exception& e) {
        Logger::getInstance().error("PluginManager: Error during unload of '{}': {}", pluginId, e.what());
    }

    // Step 5: Remove from loaded plugins map
    m_loaded_plugins.erase(plugin_it);
}

std::vector<PluginMetadata> PluginManager::getDiscoveredPlugins() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<PluginMetadata> result;
    result.reserve(m_plugins.size());

    for (const auto& [id, metadata] : m_plugins) {
        result.push_back(metadata);
    }

    return result;
}

const PluginMetadata* PluginManager::getPluginMetadata(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_plugins.find(pluginId);
    if (it != m_plugins.end()) {
        return &it->second;
    }

    return nullptr;
}

const PluginInstance* PluginManager::getPluginInstance(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_loaded_plugins.find(pluginId);
    if (it != m_loaded_plugins.end()) {
        return &it->second;
    }

    return nullptr;
}

bool PluginManager::isPluginLoaded(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_loaded_plugins.find(pluginId);
    return (it != m_loaded_plugins.end() && it->second.state == PluginState::Activated);
}

std::pair<std::string, std::string> PluginManager::parseEntryPoint(const std::string& entry_point) {
    // Parse "module:class" format
    size_t colon_pos = entry_point.find(':');

    if (colon_pos == std::string::npos) {
        throw std::runtime_error("Invalid entry_point format (expected 'module:class'): " + entry_point);
    }

    std::string module_name = entry_point.substr(0, colon_pos);
    std::string class_name = entry_point.substr(colon_pos + 1);

    if (module_name.empty() || class_name.empty()) {
        throw std::runtime_error("Invalid entry_point format (empty module or class): " + entry_point);
    }

    return {module_name, class_name};
}

} // namespace core
} // namespace kalahari
