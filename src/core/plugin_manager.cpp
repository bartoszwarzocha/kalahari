// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plugin_manager.cpp
/// @brief Plugin manager implementation

#include <kalahari/core/plugin_manager.h>
#include <kalahari/core/plugin_signature.h>
#include <kalahari/core/trusted_keys.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/unicode_paths.h>
#include <kalahari/core/python_interpreter.h>
#include <kalahari/core/extension_points.h>
#include <zip.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <random>
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

namespace {

/// The signature file of a plugin file: <file>.sig (without the narrow string of the path,
/// which loses the letters outside the code page on Windows)
std::filesystem::path signatureOf(const std::filesystem::path& file) {
    std::filesystem::path signature = file;
    signature += ".sig";
    return signature;
}

/// The name the entry module of a plugin is imported under: its id as a name of Python, each
/// character other than a letter or a digit written as _ and its code in hex, so that two ids
/// never give one name ("a.b-c" and "a.b_c" would with a plain _)
std::string uniqueModuleName(const std::string& pluginId) {
    constexpr std::string_view HEX = "0123456789abcdef";
    std::string name = "kalahari_plugin_";
    for (const char each : pluginId) {
        const auto code = static_cast<unsigned char>(each);
        if (std::isalnum(code) != 0) {
            name += each;
        } else {
            name += '_';
            name += HEX[code >> 4U];
            name += HEX[code & 0x0FU];
        }
    }
    return name;
}

/// A copy of a plugin file and its signature, in a folder of its own, to check and unpack: the
/// file in the plugins' folder may change after it is checked, the copy does not. The folder
/// goes with it.
class PluginSnapshot {
public:
    explicit PluginSnapshot(const std::filesystem::path& file) {
        std::error_code error;
        const std::filesystem::path temporary = std::filesystem::temp_directory_path(error);
        if (error) {
            m_error = "No temporary folder: " + error.message();
            return;
        }
        std::random_device random;
        std::ostringstream name;
        name << std::hex << ((static_cast<std::uint64_t>(random()) << 32U) | random());
        m_folder = temporary / "Kalahari" / "plugin-checks" / name.str();
        std::filesystem::create_directories(m_folder, error);
        const std::filesystem::path copy = m_folder / file.filename();
        if (!error) {
            std::filesystem::copy_file(file, copy, error);
        }
        std::error_code missing;
        if (!error && std::filesystem::exists(signatureOf(file), missing)) {
            std::filesystem::copy_file(signatureOf(file), signatureOf(copy), error);
        }
        if (error) {
            m_error = "Cannot copy " + pathText(file) + ": " + error.message();
            return;
        }
        m_file = copy;
    }

    ~PluginSnapshot() {
        std::error_code ignored;
        if (!m_folder.empty()) {
            std::filesystem::remove_all(m_folder, ignored);
        }
    }

    PluginSnapshot(const PluginSnapshot&) = delete;
    PluginSnapshot& operator=(const PluginSnapshot&) = delete;
    PluginSnapshot(PluginSnapshot&&) = delete;
    PluginSnapshot& operator=(PluginSnapshot&&) = delete;

    /// The copy of the plugin file; empty when it could not be made
    [[nodiscard]] const std::filesystem::path& file() const { return m_file; }
    /// Why there is no copy
    [[nodiscard]] const std::string& error() const { return m_error; }

private:
    std::filesystem::path m_folder;
    std::filesystem::path m_file;
    std::string m_error;
};

/// Move @p from to @p to (in one folder); false when it cannot
bool moveFile(const std::filesystem::path& from, const std::filesystem::path& to) {
    std::error_code error;
    std::filesystem::rename(from, to, error);
    return !error;
}

/// Delete a file and its signature, if they are there
void removeWithSignature(const std::filesystem::path& file) {
    std::error_code ignored;
    std::filesystem::remove(file, ignored);
    std::filesystem::remove(signatureOf(file), ignored);
}

/// Remove the modules of an extracted plugin from sys.modules (the GIL is held)
void forgetModules(const std::filesystem::path& folder, const std::string& moduleName) {
    py::dict modules = py::module_::import("sys").attr("modules");
    const std::string prefix = pathText(folder);
    std::vector<std::string> gone;
    for (const auto& [name, module] : modules) {
        const std::string key = py::str(name);
        if (key == moduleName || key.rfind(moduleName + ".", 0) == 0) {
            gone.push_back(key);
            continue;
        }
        const py::object file = py::getattr(module, "__file__", py::none());
        if (!file.is_none() && std::string(py::str(file)).rfind(prefix, 0) == 0) {
            gone.push_back(key);
        }
    }
    for (const std::string& key : gone) {
        modules.attr("pop")(key, py::none());
    }
}

}  // namespace

PluginManager& PluginManager::getInstance() {
    static PluginManager instance;
    return instance;
}

PluginManager::PluginManager() {
    // Function-local statics are destroyed in reverse order of construction.
    // Creating Logger and PythonInterpreter first makes them outlive this
    // singleton, so the plugins (and their archives, which log on cleanup)
    // are released while both are still alive.
    Logger::getInstance();
    PythonInterpreter::getInstance();
}

PluginManager::~PluginManager() {
    try {
        if (Py_IsInitialized() != 0) {
            py::gil_scoped_acquire gil;
            m_loaded_plugins.clear();
            return;
        }
    } catch (...) {
        // Logger may already be destroyed at process exit
        std::fputs("PluginManager: failed to release plugins at exit\n", stderr);
    }

    // The interpreter is already finalized: dropping a reference now would
    // crash, so the Python objects are intentionally leaked at process exit.
    for (auto& [id, plugin] : m_loaded_plugins) {
        if (plugin.instance) {
            plugin.instance->release();
        }
        if (plugin.module) {
            plugin.module->release();
        }
    }
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
            Logger::getInstance().debug("PluginManager: Found plugins directory: {}", pathText(path));
            return path;
        }
    }

    // Not found - return empty path
    Logger::getInstance().warn("PluginManager: plugins/ directory not found in any search path");
    return std::filesystem::path();
}

void PluginManager::setPluginFolders(std::vector<std::filesystem::path> folders) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_folders = std::move(folders);
}

std::vector<std::filesystem::path> PluginManager::pluginFolders() const {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_folders.empty()) {
            return m_folders;
        }
    }
    std::vector<std::filesystem::path> folders{userPluginsFolder()};
    if (std::filesystem::path program = getPluginsDirectory(); !program.empty()) {
        folders.push_back(std::move(program));
    }
    return folders;
}

std::filesystem::path PluginManager::userPluginsFolder() {
    return SettingsManager::getInstance().getSettingsFilePath().parent_path() / "plugins";
}

size_t PluginManager::discoverPlugins() {
    Logger::getInstance().info("PluginManager: Discovering plugins...");
    const std::vector<std::filesystem::path> folders = pluginFolders();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_plugins.clear();
    }

    size_t discovered = 0;
    for (const std::filesystem::path& folder : folders) {
        discovered += discoverIn(folder);
    }

    Logger::getInstance().info("PluginManager: Plugin discovery complete. Found: {} plugins",
                               discovered);
    return discovered;
}

size_t PluginManager::discoverIn(const std::filesystem::path& folder) {
    std::error_code error;
    if (!std::filesystem::is_directory(folder, error)) {
        return 0;
    }

    size_t discovered = 0;
    for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".kplugin") {
            continue;
        }
        std::string why;
        std::optional<PluginMetadata> metadata = examine(entry.path(), &why);
        if (!metadata) {
            continue;  // examine() logged why
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_plugins.count(metadata->id) != 0) {
            Logger::getInstance().info("PluginManager: '{}' in {} is hidden by the one found first",
                                       metadata->id, pathText(entry.path()));
            continue;
        }
        Logger::getInstance().info("PluginManager: Discovered plugin: {} v{} ({})",
                                   metadata->name, metadata->version, metadata->id);
        m_plugins[metadata->id] = std::move(*metadata);
        discovered++;
    }
    return discovered;
}

std::optional<PluginMetadata> PluginManager::examine(const std::filesystem::path& file,
                                                     std::string* error) {
    const auto refuse = [error](std::string why) {
        Logger::getInstance().warn("PluginManager: {}", why);
        if (error != nullptr) {
            *error = std::move(why);
        }
        return std::nullopt;
    };
    try {
        // Read manifest from .kplugin archive
        std::optional<PluginManifest> manifest = readManifestFromArchive(file);
        if (!manifest.has_value()) {
            return refuse("Failed to read manifest from: " + pathText(file.filename()));
        }
        if (!manifest->validate()) {
            return refuse("Invalid manifest in: " + pathText(file.filename()));
        }

        // Verify plugin signature
        std::string signedBy;
        std::string sigError;
        switch (PluginSignature::verify(file, signedBy, sigError)) {
            case PluginSignature::VerifyResult::Valid:
                Logger::getInstance().info("PluginManager: Plugin '{}' verified, signed by: {}",
                                           manifest->id, signedBy);
                break;

            case PluginSignature::VerifyResult::NotFound:
                // Only Kalahari's own add-ons are loaded, and they are signed
                return refuse("The plugin '" + manifest->id +
                              "' is not signed by Kalahari, so it is not loaded");

            case PluginSignature::VerifyResult::Invalid:
            case PluginSignature::VerifyResult::KeyNotTrusted:
            case PluginSignature::VerifyResult::FormatError:
                // Do not load plugin with bad signature
                return refuse("Signature verification FAILED for '" + manifest->id +
                              "': " + sigError);
        }

        return PluginMetadata{.id = manifest->id,
                              .name = manifest->name,
                              .version = manifest->version,
                              .path = file,
                              .manifest = *manifest};
    } catch (const std::exception& e) {
        return refuse("Error discovering plugin " + pathText(file.filename()) + ": " + e.what());
    }
}

size_t PluginManager::loadEnabledPlugins() {
    discoverPlugins();
    std::vector<std::string> enabled;
    for (const PluginMetadata& plugin : getDiscoveredPlugins()) {
        if (isPluginEnabled(plugin.id)) {
            enabled.push_back(plugin.id);
        }
    }
    if (enabled.empty()) {
        return 0;  // Python is not started for nothing
    }

    auto& python = PythonInterpreter::getInstance();
    if (!python.isInitialized()) {
        try {
            python.initialize();
        } catch (const std::exception& e) {
            Logger::getInstance().error("PluginManager: No plugins, Python did not start: {}",
                                        e.what());
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const std::string& id : enabled) {
                PluginInstance failed;
                failed.id = id;
                failed.state = PluginState::Error;
                failed.manifest = m_plugins[id].manifest;
                failed.error_message = std::string("Python did not start: ") + e.what();
                m_loaded_plugins[id] = std::move(failed);
            }
            return 0;
        }
    }

    size_t loaded = 0;
    for (const std::string& id : enabled) {
        if (loadPlugin(id)) {
            ++loaded;
        }
    }
    return loaded;
}

void PluginManager::unloadAll() {
    std::vector<std::string> ids;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& [id, instance] : m_loaded_plugins) {
            ids.push_back(id);
        }
    }
    for (const std::string& id : ids) {
        unloadPlugin(id);
    }
}

bool PluginManager::isPluginEnabled(const std::string& pluginId) {
    const json disabled = SettingsManager::getInstance().get<json>("plugins.disabled");
    if (!disabled.is_array()) {
        return true;
    }
    return std::none_of(disabled.begin(), disabled.end(), [&pluginId](const json& each) {
        return each.is_string() && each.get<std::string>() == pluginId;
    });
}

bool PluginManager::setPluginEnabled(const std::string& pluginId, bool enabled) {
    auto& settings = SettingsManager::getInstance();
    json disabled = settings.get<json>("plugins.disabled");
    if (!disabled.is_array()) {
        disabled = json::array();
    }
    json kept = json::array();
    for (const json& each : disabled) {
        if (!(each.is_string() && each.get<std::string>() == pluginId)) {
            kept.push_back(each);
        }
    }
    if (!enabled) {
        kept.push_back(pluginId);
    }
    settings.set<json>("plugins.disabled", kept);

    if (!enabled) {
        if (getPluginInstance(pluginId) != nullptr) {
            unloadPlugin(pluginId);
        }
        return true;
    }
    if (isPluginLoaded(pluginId)) {
        return true;
    }
    // A plugin that failed before gets another chance
    if (getPluginInstance(pluginId) != nullptr) {
        unloadPlugin(pluginId);
    }
    if (getPluginMetadata(pluginId) == nullptr) {
        return false;
    }
    if (auto& python = PythonInterpreter::getInstance(); !python.isInitialized()) {
        try {
            python.initialize();
        } catch (const std::exception& e) {
            Logger::getInstance().error("PluginManager: Python did not start: {}", e.what());
            return false;
        }
    }
    return loadPlugin(pluginId);
}

std::optional<std::string> PluginManager::installPlugin(const std::filesystem::path& file,
                                                        std::string* error) {
    const auto fail = [error](std::string why) -> std::optional<std::string> {
        Logger::getInstance().error("PluginManager: Not installed: {}", why);
        if (error != nullptr) {
            *error = std::move(why);
        }
        return std::nullopt;
    };
    std::string why;
    const std::optional<PluginMetadata> plugin = examine(file, &why);
    if (!plugin) {
        return fail(why);
    }

    // In place of the version there was: unloaded first, its files replaced
    const std::string& id = plugin->id;
    if (getPluginInstance(id) != nullptr) {
        unloadPlugin(id);
    }
    const std::filesystem::path folder = userPluginsFolder();
    const std::filesystem::path target = folder / (id + ".kplugin");
    std::error_code fileError;
    std::filesystem::create_directories(folder, fileError);
    if (fileError) {
        return fail("Cannot make the folder " + pathText(folder) + ": " + fileError.message());
    }
    if (std::filesystem::equivalent(file, target, fileError)) {
        // Installed already: only loaded again
    } else if (std::string failed = replaceInstalled(file, target, id); !failed.empty()) {
        return fail(failed);
    }
    Logger::getInstance().info("PluginManager: Installed '{}' v{} to {}", id, plugin->version,
                               pathText(target));

    discoverPlugins();
    if (isPluginEnabled(id)) {
        setPluginEnabled(id, true);
    }
    return id;
}

std::string PluginManager::replaceInstalled(const std::filesystem::path& file,
                                           const std::filesystem::path& target,
                                           const std::string& pluginId) {
    // The new pair is copied beside the old one and checked there; then the old pair steps
    // aside, the new one takes its place, and the old one is deleted. A step that fails puts
    // the old pair back, so a plugin that worked keeps working.
    std::filesystem::path staged = target;
    staged += ".new";
    std::filesystem::path kept = target;
    kept += ".old";
    removeWithSignature(staged);
    removeWithSignature(kept);

    std::error_code error;
    std::filesystem::copy_file(file, staged, error);
    if (!error) {
        std::filesystem::copy_file(signatureOf(file), signatureOf(staged), error);
    }
    if (error) {
        removeWithSignature(staged);
        return "Cannot copy " + pathText(file) + ": " + error.message();
    }
    std::string why;
    const std::optional<PluginMetadata> copied = examine(staged, &why);
    if (!copied || copied->id != pluginId) {
        removeWithSignature(staged);
        return "The copy of " + pathText(file) + " is not the plugin: " + why;
    }

    const bool hadOne = std::filesystem::exists(target, error);
    const bool hadSignature = std::filesystem::exists(signatureOf(target), error);
    if ((hadOne && !moveFile(target, kept)) ||
        (hadSignature && !moveFile(signatureOf(target), signatureOf(kept)))) {
        // The old pair as it was
        moveFile(kept, target);
        removeWithSignature(staged);
        return "Cannot move the installed version of '" + pluginId + "' aside";
    }
    if (!moveFile(staged, target) || !moveFile(signatureOf(staged), signatureOf(target))) {
        removeWithSignature(target);
        if (hadOne) {
            moveFile(kept, target);
        }
        if (hadSignature) {
            moveFile(signatureOf(kept), signatureOf(target));
        }
        removeWithSignature(staged);
        return "Cannot put the new version of '" + pluginId + "' in place";
    }
    removeWithSignature(kept);
    return {};
}

bool PluginManager::isUserPlugin(const std::string& pluginId) const {
    const PluginMetadata* plugin = getPluginMetadata(pluginId);
    if (plugin == nullptr) {
        return false;
    }
    std::error_code error;
    return std::filesystem::equivalent(plugin->path.parent_path(), userPluginsFolder(), error);
}

bool PluginManager::removePlugin(const std::string& pluginId, bool removeSettings,
                                 std::string* error) {
    if (!isUserPlugin(pluginId)) {
        if (error != nullptr) {
            *error = "The plugin '" + pluginId + "' is not in the user's folder";
        }
        return false;
    }
    const std::filesystem::path file = getPluginMetadata(pluginId)->path;
    if (getPluginInstance(pluginId) != nullptr) {
        unloadPlugin(pluginId);
    }
    std::error_code fileError;
    std::filesystem::remove(file, fileError);
    if (fileError) {
        if (error != nullptr) {
            *error = "Cannot delete " + pathText(file) + ": " + fileError.message();
        }
        return false;
    }
    std::filesystem::remove(signatureOf(file), fileError);
    if (removeSettings) {
        forgetPluginSettings(pluginId);
    }
    Logger::getInstance().info("PluginManager: Removed '{}'", pluginId);
    discoverPlugins();
    notifyChanged(pluginId);
    return true;
}

PluginContributions PluginManager::contributionsOf(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_loaded_plugins.find(pluginId);
    if (it == m_loaded_plugins.end() || !it->second.extensions) {
        return {};
    }
    return it->second.extensions->contributions();
}

void PluginManager::setLicenseCheck(LicenseCheck check) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_licenseCheck = std::move(check);
}

int PluginManager::addChangeListener(ChangeListener listener) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const int id = m_nextListener++;
    m_listeners[id] = std::move(listener);
    return id;
}

void PluginManager::removeChangeListener(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_listeners.erase(id);
}

void PluginManager::notifyChanged(const std::string& pluginId) {
    std::vector<ChangeListener> listeners;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& [id, listener] : m_listeners) {
            listeners.push_back(listener);
        }
    }
    for (const ChangeListener& listener : listeners) {
        listener(pluginId);
    }
}

std::optional<PluginManifest> PluginManager::readManifestFromArchive(const std::filesystem::path& kpluginPath) {
    int zip_error = 0;

    // Open ZIP archive
    zip_t* archive = openZipFile(kpluginPath, ZIP_RDONLY, &zip_error);

    if (!archive) {
        zip_error_t error;
        zip_error_init_with_code(&error, zip_error);
        Logger::getInstance().error("PluginManager: Failed to open archive {}: {}",
                                   pathText(kpluginPath.filename()),
                                   zip_error_strerror(&error));
        zip_error_fini(&error);
        return std::nullopt;
    }

    // Locate manifest.json in archive
    zip_int64_t manifest_index = zip_name_locate(archive, "manifest.json", 0);

    if (manifest_index < 0) {
        Logger::getInstance().error("PluginManager: manifest.json not found in {}",
                                   pathText(kpluginPath.filename()));
        zip_close(archive);
        return std::nullopt;
    }

    // Open manifest.json for reading
    zip_file_t* manifest_file = zip_fopen_index(archive, manifest_index, 0);

    if (!manifest_file) {
        Logger::getInstance().error("PluginManager: Failed to open manifest.json in {}",
                                   pathText(kpluginPath.filename()));
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
                                   pathText(kpluginPath.filename()));
        return std::nullopt;
    }

    // Parse JSON
    try {
        json manifest_json = json::parse(manifest_content);
        PluginManifest manifest = PluginManifest::fromJson(manifest_json);
        return manifest;

    } catch (const json::exception& e) {
        Logger::getInstance().error("PluginManager: JSON parse error in {}: {}",
                                   pathText(kpluginPath.filename()), e.what());
        return std::nullopt;
    }
}

bool PluginManager::loadPlugin(const std::string& pluginId) {
    Logger::getInstance().info("PluginManager: Loading plugin '{}'", pluginId);

    // =========================================================================
    // PHASE 1: Preparation (mutex only) - copy data, release mutex
    // =========================================================================
    std::filesystem::path plugin_path;
    PluginManifest plugin_manifest;
    std::string entry_point;
    LicenseCheck licenseCheck;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

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

        // Copy data needed for Python operations
        const PluginMetadata& metadata = plugin_it->second;
        plugin_path = metadata.path;
        plugin_manifest = metadata.manifest;
        entry_point = metadata.manifest.entry_point;
        licenseCheck = m_licenseCheck;
    }
    // Mutex released here

    // A plugin that is not loaded stays in the list, with the reason
    const auto refuse = [this, &pluginId, &plugin_manifest](const std::string& why) {
        Logger::getInstance().warn("PluginManager: '{}' not loaded: {}", pluginId, why);
        PluginInstance failed;
        failed.id = pluginId;
        failed.state = PluginState::Error;
        failed.manifest = plugin_manifest;
        failed.error_message = why;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_loaded_plugins[pluginId] = std::move(failed);
        return false;
    };

    // The file is checked again as it is now, and the copy checked is the one unpacked: a
    // plugin changed or unsigned after it was found (and turned on later) is not loaded
    const PluginSnapshot snapshot(plugin_path);
    std::string why = snapshot.error();
    std::optional<PluginMetadata> checked;
    if (!snapshot.file().empty()) {
        checked = examine(snapshot.file(), &why);
    }
    if (!checked) {
        return refuse(why);
    }
    if (checked->id != pluginId) {
        return refuse("Its file now has the plugin '" + checked->id + "'");
    }
    plugin_manifest = checked->manifest;
    entry_point = plugin_manifest.entry_point;

    // A plugin its licence does not let load stays unloaded, with the reason
    if (licenseCheck) {
        if (std::optional<std::string> refused = licenseCheck(plugin_manifest)) {
            return refuse(*refused);
        }
    }

    // =========================================================================
    // PHASE 2: Python operations (GIL only) - all Python work in one block
    // =========================================================================
    PluginInstance instance;
    instance.id = pluginId;
    instance.state = PluginState::Loading;
    instance.manifest = plugin_manifest;

    try {
        // Step 1: Extract .kplugin archive (no GIL needed)
        Logger::getInstance().debug("PluginManager: Extracting plugin archive: {}", pathText(plugin_path));
        instance.archive = std::make_unique<PluginArchive>(snapshot.file());

        if (!instance.archive->isValid()) {
            // The archive's own reason, for the writer (it is in the log already)
            instance.state = PluginState::Error;
            instance.error_message = instance.archive->errorMessage();
            Logger::getInstance().error("PluginManager: Plugin '{}' not loaded", pluginId);

            // Store error state with mutex
            std::lock_guard<std::mutex> lock(m_mutex);
            m_loaded_plugins[pluginId] = std::move(instance);
            return false;
        }

        auto extracted_path = instance.archive->getExtractedPath();
        Logger::getInstance().info("PluginManager: Plugin extracted to: {}", pathText(extracted_path));

        // Step 2: Parse entry_point (no GIL needed)
        auto [module_name, class_name] = parseEntryPoint(entry_point);
        Logger::getInstance().debug("PluginManager: Entry point: {}:{}", module_name, class_name);

        // Step 3: All Python operations in single GIL block
        {
            py::gil_scoped_acquire gil;

            // Add extracted directory to Python sys.path (the plugin's other modules)
            py::module_ sys = py::module_::import("sys");
            py::list path = sys.attr("path");
            path.append(pathText(extracted_path));
            Logger::getInstance().debug("PluginManager: Added to sys.path: {}", pathText(extracted_path));

            // Import Python module, under a name of the plugin's own: every plugin may have
            // its plugin.py
            instance.moduleName = uniqueModuleName(pluginId);
            Logger::getInstance().debug("PluginManager: Importing module: {} as {}", module_name,
                                        instance.moduleName);
            instance.module = std::make_shared<py::object>(
                importEntryModule(extracted_path, module_name, instance.moduleName));

            // Get plugin class and create instance
            Logger::getInstance().debug("PluginManager: Creating plugin instance: {}", class_name);

            if (!py::hasattr(*instance.module, class_name.c_str())) {
                throw std::runtime_error("Plugin class '" + class_name + "' not found in module '" + module_name + "'");
            }

            py::object plugin_class = instance.module->attr(class_name.c_str());
            instance.instance = std::make_shared<py::object>(plugin_class());

            instance.state = PluginState::Loaded;
            Logger::getInstance().info("PluginManager: Plugin '{}' loaded successfully", pluginId);

            // Call on_init() if exists
            if (py::hasattr(*instance.instance, "on_init")) {
                Logger::getInstance().debug("PluginManager: Calling on_init()");
                instance.instance->attr("on_init")();
                Logger::getInstance().debug("PluginManager: on_init() completed");
            }

            // Its settings have their values from now on, in on_activate() too
            instance.extensions = std::make_shared<PythonPlugin>(
                pluginId, plugin_manifest.version, extracted_path, instance.instance);

            // Call on_activate() if exists
            if (py::hasattr(*instance.instance, "on_activate")) {
                Logger::getInstance().debug("PluginManager: Calling on_activate()");
                instance.instance->attr("on_activate")();
                Logger::getInstance().debug("PluginManager: on_activate() completed");
            }
        }
        // GIL released here

        // Its extension points: what it gives the program, as it is once active
        instance.extensions->readContributions();
        ExtensionPointRegistry::getInstance().registerPlugin(instance.extensions);

        instance.state = PluginState::Activated;
        Logger::getInstance().info("PluginManager: Plugin '{}' activated successfully", pluginId);

    } catch (const py::error_already_set& e) {
        instance.extensions.reset();  // Not registered: nothing of it is used
        instance.state = PluginState::Error;
        instance.error_message = std::string("Python error: ") + e.what();
        Logger::getInstance().error("PluginManager: Failed to load plugin '{}': {}", pluginId, instance.error_message);

        // Store error state with mutex
        std::lock_guard<std::mutex> lock(m_mutex);
        m_loaded_plugins[pluginId] = std::move(instance);
        return false;

    } catch (const std::exception& e) {
        instance.extensions.reset();  // Not registered: nothing of it is used
        instance.state = PluginState::Error;
        instance.error_message = std::string("C++ error: ") + e.what();
        Logger::getInstance().error("PluginManager: Failed to load plugin '{}': {}", pluginId, instance.error_message);

        // Store error state with mutex
        std::lock_guard<std::mutex> lock(m_mutex);
        m_loaded_plugins[pluginId] = std::move(instance);
        return false;
    }

    // =========================================================================
    // PHASE 3: Store result (mutex only) - double-check and save
    // =========================================================================
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Double-check: another thread might have loaded this plugin while we were working
        if (m_loaded_plugins.find(pluginId) != m_loaded_plugins.end()) {
            Logger::getInstance().warn("PluginManager: Plugin '{}' was loaded by another thread, discarding our instance", pluginId);
            // Our instance will be destroyed, but the other thread's instance remains
            return true;
        }

        // Store loaded plugin
        m_loaded_plugins[pluginId] = std::move(instance);
    }

    notifyChanged(pluginId);
    return true;
}

void PluginManager::unloadPlugin(const std::string& pluginId) {
    Logger::getInstance().info("PluginManager: Unloading plugin '{}'", pluginId);

    // =========================================================================
    // PHASE 1: Preparation (mutex only) - extract instance, release mutex
    // =========================================================================
    PluginInstance instance;
    std::filesystem::path extracted_path;
    bool has_valid_archive = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Check if plugin is loaded
        auto plugin_it = m_loaded_plugins.find(pluginId);
        if (plugin_it == m_loaded_plugins.end()) {
            Logger::getInstance().warn("PluginManager: Plugin '{}' is not loaded", pluginId);
            return;
        }

        // Move instance out of map (we will either destroy it or put it back on error)
        instance = std::move(plugin_it->second);
        m_loaded_plugins.erase(plugin_it);

        // Copy path info before releasing mutex
        if (instance.archive && instance.archive->isValid()) {
            has_valid_archive = true;
            extracted_path = instance.archive->getExtractedPath();
        }
    }
    // Mutex released here

    // Its extension points go first: the program stops using them
    if (instance.extensions) {
        ExtensionPointRegistry::getInstance().unregisterPlugin(pluginId);
        instance.extensions.reset();
    }

    // Without an interpreter (Python did not start, or it is gone) there is nothing of Python
    // to release, and taking its lock would crash
    if (Py_IsInitialized() == 0) {
        if (instance.instance) {
            instance.instance->release();
        }
        if (instance.module) {
            instance.module->release();
        }
        Logger::getInstance().info("PluginManager: Plugin '{}' unloaded", pluginId);
        notifyChanged(pluginId);
        return;
    }

    // =========================================================================
    // PHASE 2: Python operations (GIL only) - all Python work in one block
    // =========================================================================
    try {
        instance.state = PluginState::Unloading;

        // All Python operations in single GIL block
        {
            py::gil_scoped_acquire gil;

            // Step 1: Call on_deactivate() if exists
            if (instance.instance && instance.instance->ptr() != nullptr) {
                if (py::hasattr(*instance.instance, "on_deactivate")) {
                    Logger::getInstance().debug("PluginManager: Calling on_deactivate()");
                    instance.instance->attr("on_deactivate")();
                    Logger::getInstance().debug("PluginManager: on_deactivate() completed");
                }
            }

            // Step 2: Remove from sys.path (if archive is valid)
            if (has_valid_archive) {
                py::module_ sys = py::module_::import("sys");
                py::list path = sys.attr("path");

                // Find and remove the path
                std::string path_str = pathText(extracted_path);
                for (size_t i = 0; i < py::len(path); ++i) {
                    std::string item = py::str(path[i]);
                    if (item == path_str) {
                        path.attr("pop")(i);
                        Logger::getInstance().debug("PluginManager: Removed from sys.path: {}", path_str);
                        break;
                    }
                }
            }

            // Step 3: Its modules leave sys.modules, so a new version is imported anew
            if (has_valid_archive) {
                forgetModules(extracted_path, instance.moduleName);
            }

            // Step 4: Clear Python objects (will decrement refcount)
            // Must be done inside GIL block for safe reference counting
            instance.instance.reset();
            instance.module.reset();
        }
        // GIL released here

        // Step 4: Archive destructor will clean up temp files automatically (no GIL needed)

        Logger::getInstance().info("PluginManager: Plugin '{}' unloaded successfully", pluginId);

    } catch (const py::error_already_set& e) {
        Logger::getInstance().error("PluginManager: Python error during unload of '{}': {}", pluginId, e.what());
        // Instance will be destroyed when it goes out of scope
    } catch (const std::exception& e) {
        Logger::getInstance().error("PluginManager: Error during unload of '{}': {}", pluginId, e.what());
        // Instance will be destroyed when it goes out of scope
    }

    // =========================================================================
    // PHASE 3: No need to store result - instance already removed from map
    // =========================================================================
    // The instance goes out of scope here and will be fully destroyed
    notifyChanged(pluginId);
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

py::object PluginManager::importEntryModule(const std::filesystem::path& folder,
                                           const std::string& module,
                                           const std::string& uniqueName) {
    // A module of a dotted name is in a package: imported by its name from sys.path
    if (module.find('.') != std::string::npos) {
        return py::module_::import(module.c_str());
    }
    // The file of the module, or its package
    std::filesystem::path file = folder / (module + ".py");
    py::object locations = py::none();
    if (!std::filesystem::exists(file)) {
        file = folder / module / "__init__.py";
        if (!std::filesystem::exists(file)) {
            throw std::runtime_error("The module '" + module + "' is not in the plugin");
        }
        py::list search;
        search.append(pathText(folder / module));
        locations = search;
    }
    py::module_ util = py::module_::import("importlib.util");
    py::object spec = util.attr("spec_from_file_location")(
        uniqueName, pathText(file), py::arg("submodule_search_locations") = locations);
    if (spec.is_none()) {
        throw std::runtime_error("The module '" + module + "' cannot be imported");
    }
    py::object loaded = util.attr("module_from_spec")(spec);
    py::dict modules = py::module_::import("sys").attr("modules");
    modules[py::str(uniqueName)] = loaded;
    try {
        spec.attr("loader").attr("exec_module")(loaded);
    } catch (...) {
        modules.attr("pop")(uniqueName, py::none());
        throw;
    }
    return loaded;
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
