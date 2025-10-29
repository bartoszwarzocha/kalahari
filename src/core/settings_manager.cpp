/// @file settings_manager.cpp
/// @brief Implementation of SettingsManager

#include <kalahari/core/settings_manager.h>
#include <kalahari/core/logger.h>
#include <fstream>
#include <cstdlib>  // std::getenv

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
            // Ignore backup errors
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

wxSize SettingsManager::getWindowSize() const {
    int width = get<int>("window.width", 1280);
    int height = get<int>("window.height", 800);
    return wxSize(width, height);
}

void SettingsManager::setWindowSize(const wxSize& size) {
    set("window.width", size.GetWidth());
    set("window.height", size.GetHeight());
}

wxPoint SettingsManager::getWindowPosition() const {
    int x = get<int>("window.x", 100);
    int y = get<int>("window.y", 100);
    return wxPoint(x, y);
}

void SettingsManager::setWindowPosition(const wxPoint& pos) {
    set("window.x", pos.x);
    set("window.y", pos.y);
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
    return get<std::string>("ui.theme", "Light");
}

void SettingsManager::setTheme(const std::string& theme) {
    set("ui.theme", theme);
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
            {"theme", "Light"},
            {"font_size", 12}
        }},
        {"session", {
            {"auto_save_interval", 300},
            {"backup_enabled", true}
        }},
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

} // namespace core
} // namespace kalahari
