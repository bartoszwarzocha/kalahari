/// @file settings_manager.cpp
/// @brief Implementation of SettingsManager

#include <kalahari/core/settings_manager.h>
#include <kalahari/core/logger.h>
#include <algorithm>
#include <fstream>
#include <cstdlib>  // std::getenv
#include <vector>   // std::vector for log color keys

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>  // SHGetFolderPath
#endif

namespace kalahari {
namespace core {

namespace {
/// Version written by createDefaults() and reached by migrateIfNeeded()
constexpr const char* CURRENT_SETTINGS_VERSION = "1.2";
}

// =============================================================================
// Singleton instance
// =============================================================================

SettingsManager& SettingsManager::getInstance() {
    static SettingsManager instance;
    return instance;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

SettingsManager::SettingsManager() {
    m_filePath = getSettingsDirectoryPath() / "settings.json";
    createDefaults();
    Logger::getInstance().info("SettingsManager initialized (file: {})", m_filePath.string());
}

SettingsManager::~SettingsManager() {
    // Auto-save on destruction
    save();
    Logger::getInstance().info("SettingsManager destroyed (settings auto-saved)");
}

// =============================================================================
// Load / Save
// =============================================================================

bool SettingsManager::load() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!std::filesystem::exists(m_filePath)) {
        Logger::getInstance().info("Settings file not found, using defaults: {}", m_filePath.string());
        createDefaults();  // Reset to defaults when file doesn't exist
        return true;  // Not an error, just first run
    }

    try {
        std::ifstream file(m_filePath);
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open settings file: {}", m_filePath.string());
            return false;
        }

        m_settings = nlohmann::json::parse(file);
        Logger::getInstance().info("Settings loaded successfully from: {}", m_filePath.string());

        // Migrate settings if needed (unlock for migration to call set())
        m_mutex.unlock();
        migrateIfNeeded();
        m_mutex.lock();

        return true;

    } catch (const nlohmann::json::exception& e) {
        Logger::getInstance().warn("Settings file corrupted ({}), using defaults: {}",
                     e.what(), m_filePath.string());

        // Backup corrupted file
        try {
            std::filesystem::path backupPath = m_filePath;
            backupPath += ".bak";
            std::filesystem::copy_file(m_filePath, backupPath,
                                      std::filesystem::copy_options::overwrite_existing);
            Logger::getInstance().info("Corrupted settings backed up to: {}", backupPath.string());
        } catch (...) {
            Logger::getInstance().warn("SettingsManager: Failed to backup corrupted settings file");
        }

        createDefaults();
        return false;
    }
}

bool SettingsManager::save() {
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        // Create directory if it doesn't exist
        std::filesystem::path dir = m_filePath.parent_path();
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
            Logger::getInstance().info("Created settings directory: {}", dir.string());
        }

        // Write to a temporary file and rename it over settings.json, so an
        // interrupted save never leaves a truncated settings file behind
        std::filesystem::path tempPath = m_filePath;
        tempPath += ".tmp";
        {
            std::ofstream file(tempPath, std::ios::trunc);
            if (!file.is_open()) {
                Logger::getInstance().error("Failed to open settings file for writing: {}", tempPath.string());
                return false;
            }
            file << m_settings.dump(4);  // Pretty-print with indent
            file.flush();
            if (!file) {
                Logger::getInstance().error("Failed to write settings file: {}", tempPath.string());
                return false;
            }
        }
        std::filesystem::rename(tempPath, m_filePath);
        Logger::getInstance().info("Settings saved successfully to: {}", m_filePath.string());
        return true;

    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to save settings: {}", e.what());
        return false;
    }
}

void SettingsManager::resetToDefaults() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Delete settings file if exists
    if (std::filesystem::exists(m_filePath)) {
        try {
            std::filesystem::remove(m_filePath);
            Logger::getInstance().info("Deleted settings file: {}", m_filePath.string());
        } catch (const std::exception& e) {
            Logger::getInstance().warn("Failed to delete settings file: {}", e.what());
        }
    }

    // Reset to defaults in memory
    createDefaults();
    Logger::getInstance().info("Settings reset to defaults");
}

// =============================================================================
// Convenience methods
// =============================================================================

QSize SettingsManager::getWindowSize() const {
    int width = get<int>("window.width");
    int height = get<int>("window.height");
    return QSize(width, height);
}

void SettingsManager::setWindowSize(const QSize& size) {
    set("window.width", size.width());
    set("window.height", size.height());
}

QPoint SettingsManager::getWindowPosition() const {
    int x = get<int>("window.x");
    int y = get<int>("window.y");
    return QPoint(x, y);
}

void SettingsManager::setWindowPosition(const QPoint& pos) {
    set("window.x", pos.x());
    set("window.y", pos.y());
}

bool SettingsManager::isWindowMaximized() const {
    return get<bool>("window.maximized");
}

void SettingsManager::setWindowMaximized(bool maximized) {
    set("window.maximized", maximized);
}

std::string SettingsManager::getLanguage() const {
    return get<std::string>("ui.language");
}

void SettingsManager::setLanguage(const std::string& lang) {
    set("ui.language", lang);
}

std::string SettingsManager::getTheme() const {
    return get<std::string>("appearance.theme");
}

void SettingsManager::setTheme(const std::string& theme) {
    set("appearance.theme", theme);
}

// =============================================================================
// Per-theme icon colors (Task #00025)
// =============================================================================

std::string SettingsManager::getIconColorPrimaryForTheme(const std::string& themeName,
                                                          const std::string& defaultColor) const {
    std::string key = "icons.themes." + themeName + ".colorPrimary";
    return get<std::string>(key, defaultColor);
}

void SettingsManager::setIconColorPrimaryForTheme(const std::string& themeName,
                                                   const std::string& color) {
    std::string key = "icons.themes." + themeName + ".colorPrimary";
    set(key, color);
}

std::string SettingsManager::getIconColorSecondaryForTheme(const std::string& themeName,
                                                            const std::string& defaultColor) const {
    std::string key = "icons.themes." + themeName + ".colorSecondary";
    return get<std::string>(key, defaultColor);
}

void SettingsManager::setIconColorSecondaryForTheme(const std::string& themeName,
                                                     const std::string& color) {
    std::string key = "icons.themes." + themeName + ".colorSecondary";
    set(key, color);
}

bool SettingsManager::hasCustomIconColorsForTheme(const std::string& themeName) const {
    std::string keyPrimary = "icons.themes." + themeName + ".colorPrimary";
    std::string keySecondary = "icons.themes." + themeName + ".colorSecondary";
    return hasKey(keyPrimary) || hasKey(keySecondary);
}

void SettingsManager::clearCustomIconColorsForTheme(const std::string& themeName) {
    std::string keyPrimary = "icons.themes." + themeName + ".colorPrimary";
    std::string keySecondary = "icons.themes." + themeName + ".colorSecondary";

    if (hasKey(keyPrimary)) {
        removeKey(keyPrimary);
    }
    if (hasKey(keySecondary)) {
        removeKey(keySecondary);
    }

    Logger::getInstance().info("Cleared custom icon colors for theme: {}", themeName);
}

// =============================================================================
// Per-theme log colors (Task #00027)
// =============================================================================

std::string SettingsManager::getLogColorForTheme(const std::string& themeName,
                                                  const std::string& colorKey,
                                                  const std::string& defaultColor) const {
    std::string key = "themes." + themeName + ".log." + colorKey;
    return get<std::string>(key, defaultColor);
}

void SettingsManager::setLogColorForTheme(const std::string& themeName,
                                           const std::string& colorKey,
                                           const std::string& color) {
    std::string key = "themes." + themeName + ".log." + colorKey;
    set(key, color);
}

bool SettingsManager::hasCustomLogColorsForTheme(const std::string& themeName) const {
    // Check if any of the log color keys exist for this theme
    static const std::vector<std::string> colorKeys = {
        "trace", "debug", "info", "warning", "error", "critical", "background"
    };

    for (const auto& colorKey : colorKeys) {
        std::string key = "themes." + themeName + ".log." + colorKey;
        if (hasKey(key)) {
            return true;
        }
    }
    return false;
}

void SettingsManager::clearCustomLogColorsForTheme(const std::string& themeName) {
    static const std::vector<std::string> colorKeys = {
        "trace", "debug", "info", "warning", "error", "critical", "background"
    };

    for (const auto& colorKey : colorKeys) {
        std::string key = "themes." + themeName + ".log." + colorKey;
        if (hasKey(key)) {
            removeKey(key);
        }
    }

    Logger::getInstance().info("Cleared custom log colors for theme: {}", themeName);
}

// =============================================================================
// Per-theme UI colors (Task #00028)
// =============================================================================

std::string SettingsManager::getUiColorForTheme(const std::string& themeName,
                                                 const std::string& colorKey,
                                                 const std::string& defaultColor) const {
    std::string key = "themes." + themeName + ".ui." + colorKey;
    return get<std::string>(key, defaultColor);
}

void SettingsManager::setUiColorForTheme(const std::string& themeName,
                                          const std::string& colorKey,
                                          const std::string& color) {
    std::string key = "themes." + themeName + ".ui." + colorKey;
    set(key, color);
}

bool SettingsManager::hasCustomUiColorsForTheme(const std::string& themeName) const {
    // Check if any of the UI color keys exist for this theme
    static const std::vector<std::string> colorKeys = {
        "toolTipBase", "toolTipText", "placeholderText", "brightText"
    };

    for (const auto& colorKey : colorKeys) {
        std::string key = "themes." + themeName + ".ui." + colorKey;
        if (hasKey(key)) {
            return true;
        }
    }
    return false;
}

void SettingsManager::clearCustomUiColorsForTheme(const std::string& themeName) {
    static const std::vector<std::string> colorKeys = {
        "toolTipBase", "toolTipText", "placeholderText", "brightText"
    };

    for (const auto& colorKey : colorKeys) {
        std::string key = "themes." + themeName + ".ui." + colorKey;
        if (hasKey(key)) {
            removeKey(key);
        }
    }

    Logger::getInstance().info("Cleared custom UI colors for theme: {}", themeName);
}

// =============================================================================
// Per-theme palette colors (Task #00028 - Full QPalette support)
// =============================================================================

std::string SettingsManager::getPaletteColorForTheme(const std::string& themeName,
                                                      const std::string& colorKey,
                                                      const std::string& defaultColor) const {
    std::string key = "themes." + themeName + ".palette." + colorKey;
    return get<std::string>(key, defaultColor);
}

void SettingsManager::setPaletteColorForTheme(const std::string& themeName,
                                               const std::string& colorKey,
                                               const std::string& color) {
    std::string key = "themes." + themeName + ".palette." + colorKey;
    set(key, color);
}

bool SettingsManager::hasCustomPaletteColorsForTheme(const std::string& themeName) const {
    // Check if any of the palette color keys exist for this theme
    static const std::vector<std::string> colorKeys = {
        "window", "windowText", "base", "alternateBase", "text",
        "button", "buttonText", "highlight", "highlightedText",
        "light", "midlight", "mid", "dark", "shadow",
        "link", "linkVisited"
    };

    for (const auto& colorKey : colorKeys) {
        std::string key = "themes." + themeName + ".palette." + colorKey;
        if (hasKey(key)) {
            return true;
        }
    }
    return false;
}

void SettingsManager::clearCustomPaletteColorsForTheme(const std::string& themeName) {
    static const std::vector<std::string> colorKeys = {
        "window", "windowText", "base", "alternateBase", "text",
        "button", "buttonText", "highlight", "highlightedText",
        "light", "midlight", "mid", "dark", "shadow",
        "link", "linkVisited"
    };

    for (const auto& colorKey : colorKeys) {
        std::string key = "themes." + themeName + ".palette." + colorKey;
        if (hasKey(key)) {
            removeKey(key);
        }
    }

    Logger::getInstance().info("Cleared custom palette colors for theme: {}", themeName);
}

std::filesystem::path SettingsManager::getSettingsFilePath() const {
    return m_filePath;
}

// =============================================================================
// Private helpers
// =============================================================================

std::filesystem::path SettingsManager::getSettingsDirectoryPath() const {
    // TEST MODE: Use temp directory instead of user directory
    // This prevents tests from polluting real user settings
#ifdef _WIN32
    char* testMode = nullptr;
    size_t testLen = 0;
    if (_dupenv_s(&testMode, &testLen, "KALAHARI_TEST_MODE") == 0 && testMode != nullptr) {
        free(testMode);
        return std::filesystem::temp_directory_path() / "kalahari_test";
    }
#else
    const char* testMode = std::getenv("KALAHARI_TEST_MODE");
    if (testMode) {
        return std::filesystem::temp_directory_path() / "kalahari_test";
    }
#endif

#ifdef _WIN32
    // Windows: %APPDATA%\Kalahari
    // Example: C:\Users\Username\AppData\Roaming\Kalahari

    // Use _dupenv_s (thread-safe, MSVC-recommended)
    char* appdata = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appdata, &len, "APPDATA") == 0 && appdata != nullptr) {
        std::filesystem::path result = std::filesystem::path(appdata) / "Kalahari";
        free(appdata);  // _dupenv_s allocates memory, must free
        return result;
    }

    // Fallback: use SHGetFolderPath
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::filesystem::path(path) / "Kalahari";
    }

    // Last resort fallback
    return std::filesystem::path(".") / "kalahari_settings";

#elif defined(__APPLE__)
    // macOS: ~/Library/Application Support/Kalahari

    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / "Library" / "Application Support" / "Kalahari";
    }

    // Fallback
    return std::filesystem::path(".") / "kalahari_settings";

#else
    // Linux: ~/.config/kalahari
    // Follows XDG Base Directory Specification

    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config) {
        return std::filesystem::path(xdg_config) / "kalahari";
    }

    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".config" / "kalahari";
    }

    // Fallback
    return std::filesystem::path(".") / "kalahari_settings";
#endif
}

void SettingsManager::createDefaults() {
    // Default values live in settings_schema; the file stores only what differs
    // or was set explicitly
    m_settings = nlohmann::json{
        {"version", CURRENT_SETTINGS_VERSION},
        {"recent_files", nlohmann::json::array()}
    };

    Logger::getInstance().info("Default settings created");
}

std::string SettingsManager::keyToJsonPointer(const std::string& key) const {
    // Convert "window.width" to "/window/width"
    std::string pointer = "/" + key;
    for (char& c : pointer) {
        if (c == '.') c = '/';
    }
    return pointer;
}

// =============================================================================
// Migration Support (Task #00020 - Settings Migration)
// =============================================================================

bool SettingsManager::hasKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        std::string pointer = keyToJsonPointer(key);
        m_settings.at(nlohmann::json::json_pointer(pointer));
        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

void SettingsManager::removeKey(const std::string& key) {
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the parent object and erase the child ("ui.theme" -> erase "theme" in "ui")
        std::string pointer = keyToJsonPointer(key);
        size_t lastSlash = pointer.rfind('/');
        std::string childKey = pointer.substr(lastSlash + 1);
        nlohmann::json::json_pointer parentPtr(pointer.substr(0, lastSlash));

        if (m_settings.contains(parentPtr)) {
            nlohmann::json& parent = m_settings.at(parentPtr);
            if (parent.is_object() && parent.erase(childKey) > 0) {
                removed = true;
                Logger::getInstance().debug("Removed setting key: {}", key);
            }
        }
    }

    if (removed) {
        notifyChanged(key);
    }
}

void SettingsManager::migrateIfNeeded() {
    std::string version = get<std::string>("version", "0.0");

    Logger::getInstance().debug("Checking settings version: {}", version);

    if (version != CURRENT_SETTINGS_VERSION) {
        Logger::getInstance().info("Migrating settings from {} to {}...", version, CURRENT_SETTINGS_VERSION);
        migrateToCurrentVersion();
        set("version", std::string(CURRENT_SETTINGS_VERSION));
        save();  // Save migrated settings immediately
        Logger::getInstance().info("Settings migration complete");
    }
}

void SettingsManager::migrateToCurrentVersion() {
    // 1.0: ui.theme moved to appearance.theme
    if (hasKey("ui.theme")) {
        std::string theme = get<std::string>("ui.theme", "Light");
        if (!hasKey("appearance.theme")) {
            set("appearance.theme", theme);
        }
        removeKey("ui.theme");
        Logger::getInstance().info("Migrated ui.theme='{}' (removed legacy key)", theme);
    }

    // 1.0 and 1.1 wrote keys that nothing reads
    static const char* const obsoleteKeys[] = {
        "ui.font_size",
        "appearance.iconSize",
        "appearance.toolbarIconSize",
        "appearance.menuIconSize",
        "appearance.treeViewIconSize",
        "appearance.tabBarIconSize",
        "appearance.statusBarIconSize",
        "appearance.buttonIconSize",
        "appearance.comboBoxIconSize",
        "log.fontSize",
        "log.backgroundColor",
        "log.textColor",
        "session",
        "dashboard.autoLoadLastProject",  // duplicate of startup.autoLoadLastProject
        "icons.colorPrimary",             // icon colors are stored per theme
        "icons.colorSecondary",
        "icons.theme",                    // second copy of appearance.theme
    };
    for (const char* key : obsoleteKeys) {
        removeKey(key);
    }

    // Old per-theme copies of the icon colors (icons.themes.<name> holds them)
    std::vector<std::string> themeNames;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_settings.contains("themes") && m_settings["themes"].is_object()) {
            for (const auto& entry : m_settings["themes"].items()) {
                themeNames.push_back(entry.key());
            }
        }
    }
    for (const std::string& themeName : themeNames) {
        removeKey("themes." + themeName + ".colors.primary");
        removeKey("themes." + themeName + ".colors.secondary");
    }
}

// =============================================================================
// Change notification
// =============================================================================

int SettingsManager::subscribe(ChangeListener listener) {
    std::lock_guard<std::mutex> lock(m_listenersMutex);
    int id = m_nextListenerId++;
    m_listeners.emplace(id, std::move(listener));
    return id;
}

void SettingsManager::unsubscribe(int id) {
    std::lock_guard<std::mutex> lock(m_listenersMutex);
    m_listeners.erase(id);
}

void SettingsManager::notifyChanged(const std::string& key) {
    std::string normalized = key;
    std::replace(normalized.begin(), normalized.end(), '/', '.');

    // Copy, so a listener may subscribe or unsubscribe while being called
    std::vector<ChangeListener> listeners;
    {
        std::lock_guard<std::mutex> lock(m_listenersMutex);
        listeners.reserve(m_listeners.size());
        for (const auto& entry : m_listeners) {
            listeners.push_back(entry.second);
        }
    }
    for (const auto& listener : listeners) {
        listener(normalized);
    }
}

void SettingsManager::warnMissingDefault(const std::string& key) const {
    Logger::getInstance().warn("SettingsManager: '{}' has no default in settings_schema", key);
}

} // namespace core
} // namespace kalahari
