/// @file settings_manager.cpp
/// @brief Implementation of SettingsManager

#include <kalahari/core/settings_manager.h>
#include <kalahari/core/logger.h>
#include <fstream>
#include <cstdlib>  // std::getenv
#include <vector>   // std::vector for log color keys

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>  // SHGetFolderPath
#endif

namespace kalahari {
namespace core {

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

        // Write JSON to file (pretty-print with 4-space indent)
        std::ofstream file(m_filePath);
        if (!file.is_open()) {
            Logger::getInstance().error("Failed to open settings file for writing: {}", m_filePath.string());
            return false;
        }

        file << m_settings.dump(4);  // Pretty-print with indent
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
    int width = get<int>("window.width", 1280);
    int height = get<int>("window.height", 800);
    return QSize(width, height);
}

void SettingsManager::setWindowSize(const QSize& size) {
    set("window.width", size.width());
    set("window.height", size.height());
}

QPoint SettingsManager::getWindowPosition() const {
    int x = get<int>("window.x", 100);
    int y = get<int>("window.y", 100);
    return QPoint(x, y);
}

void SettingsManager::setWindowPosition(const QPoint& pos) {
    set("window.x", pos.x());
    set("window.y", pos.y());
}

bool SettingsManager::isWindowMaximized() const {
    return get<bool>("window.maximized", false);
}

void SettingsManager::setWindowMaximized(bool maximized) {
    set("window.maximized", maximized);
}

std::string SettingsManager::getLanguage() const {
    return get<std::string>("ui.language", "en");
}

void SettingsManager::setLanguage(const std::string& lang) {
    set("ui.language", lang);
}

std::string SettingsManager::getTheme() const {
    return get<std::string>("appearance.theme", "Light");
}

void SettingsManager::setTheme(const std::string& theme) {
    set("appearance.theme", theme);
}

std::string SettingsManager::getIconColorPrimary() const {
    return get<std::string>("icons.colorPrimary", "#333333");
}

void SettingsManager::setIconColorPrimary(const std::string& color) {
    set("icons.colorPrimary", color);
}

std::string SettingsManager::getIconColorSecondary() const {
    return get<std::string>("icons.colorSecondary", "#999999");
}

void SettingsManager::setIconColorSecondary(const std::string& color) {
    set("icons.colorSecondary", color);
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
    m_settings = nlohmann::json{
        {"version", "1.0"},
        {"window", {
            {"width", 1280},
            {"height", 800},
            {"x", 100},
            {"y", 100},
            {"maximized", false}
        }},
        {"ui", {
            {"language", "en"},
            {"font_size", 12}
        }},
        {"appearance", {
            {"theme", "Light"},
            {"uiFontSize", 12},
            {"iconTheme", "twotone"},
            {"toolbarIconSize", 24},
            {"menuIconSize", 16},
            {"treeViewIconSize", 16},
            {"tabBarIconSize", 16},
            {"statusBarIconSize", 16},
            {"buttonIconSize", 20},
            {"comboBoxIconSize", 16}
        }},
        {"log", {
            {"bufferSize", 500},
            {"fontSize", 10}
        }},
        {"session", {
            {"auto_save_interval", 300},
            {"backup_enabled", true}
        }},
        {"recent_files", nlohmann::json::array()},
        {"dashboard", {
            {"maxItems", 5},
            {"iconSize", 48},
            {"showKalahariNews", true},
            {"showRecentFiles", true}
        }},
        {"startup", {
            {"autoLoadLastProject", false}
        }}
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
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        std::string pointer = keyToJsonPointer(key);
        // For removing nested keys, we need to access parent and erase child
        // Example: "ui.theme" -> find "ui" object and erase "theme" key

        size_t lastSlash = pointer.rfind('/');
        if (lastSlash == 0) {
            // Top-level key like "/version"
            std::string topKey = pointer.substr(1);
            if (m_settings.contains(topKey)) {
                m_settings.erase(topKey);
                Logger::getInstance().debug("Removed setting key: {}", key);
            }
        } else {
            // Nested key like "/ui/theme"
            std::string parentPointer = pointer.substr(0, lastSlash);
            std::string childKey = pointer.substr(lastSlash + 1);

            // Check if parent path exists before accessing
            nlohmann::json::json_pointer parentPtr(parentPointer);
            if (!m_settings.contains(parentPtr)) {
                // Parent doesn't exist, nothing to remove - silently ignore
                return;
            }

            nlohmann::json& parent = m_settings.at(parentPtr);
            if (parent.is_object() && parent.contains(childKey)) {
                parent.erase(childKey);
                Logger::getInstance().debug("Removed setting key: {}", key);
            }
        }
    } catch (const nlohmann::json::exception&) {
        // Silently ignore - key doesn't exist, which is fine for removeKey
        Logger::getInstance().debug("Key '{}' not found (nothing to remove)", key);
    }
}

void SettingsManager::migrateIfNeeded() {
    // Check current version (no mutex needed, called from load() which already holds lock)
    std::string version = get<std::string>("version", "0.0");

    Logger::getInstance().debug("Checking settings version: {}", version);

    if (version == "1.0") {
        Logger::getInstance().info("Migrating settings from 1.0 to 1.1...");
        migrateFrom_1_0_to_1_1();
        set("version", "1.1");
        save();  // Save migrated settings immediately
        Logger::getInstance().info("Settings migration complete: 1.0 -> 1.1");
    }

    // Future migrations here:
    // if (version == "1.1") { migrateFrom_1_1_to_1_2(); ... }
}

void SettingsManager::migrateFrom_1_0_to_1_1() {
    // =========================================================================
    // Migration 1.0 -> 1.1 (Task #00020 - Appearance Settings)
    // =========================================================================
    //
    // Changes:
    // 1. Move ui.theme -> appearance.theme
    // 2. Add appearance.iconSize (default: 24)
    // =========================================================================

    // 1. Migrate ui.theme -> appearance.theme (only if appearance.theme doesn't exist)
    if (hasKey("ui.theme")) {
        std::string theme = get<std::string>("ui.theme", "Light");
        if (!hasKey("appearance.theme")) {
            set("appearance.theme", theme);
        }
        removeKey("ui.theme");
        Logger::getInstance().info("Migrated ui.theme='{}' (removed legacy key)", theme);
    }
    // Note: appearance.theme default is now in createDefaultSettings(), no need to create here

    // 2. Add appearance.iconSize if not exists
    if (!hasKey("appearance.iconSize")) {
        set("appearance.iconSize", 24);
        Logger::getInstance().debug("Added appearance.iconSize=24");
    }

    // Add default Log settings if not exist (Phase 1)
    if (!hasKey("log.bufferSize")) {
        set("log.bufferSize", 500);
        set("log.backgroundColor.r", 60);
        set("log.backgroundColor.g", 60);
        set("log.backgroundColor.b", 60);
        set("log.textColor.r", 255);
        set("log.textColor.g", 255);
        set("log.textColor.b", 255);
        set("log.fontSize", 11);
        Logger::getInstance().debug("Added default log settings");
    }

    Logger::getInstance().info("Migration 1.0 -> 1.1 complete");
}

} // namespace core
} // namespace kalahari
