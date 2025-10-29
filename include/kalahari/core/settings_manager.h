/// @file settings_manager.h
/// @brief Settings management system with JSON persistence
///
/// SettingsManager is a singleton that manages application-wide settings,
/// persisting them to a JSON file in the user's config directory.
///
/// Thread-safe: All public methods are protected with std::mutex.
///
/// @example
/// @code
/// auto& settings = SettingsManager::getInstance();
/// settings.load();  // Load from disk
///
/// int width = settings.get<int>("window.width", 1280);
/// settings.set("window.width", 1600);
///
/// settings.save();  // Save to disk
/// @endcode

#pragma once

#include <string>
#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>
#include <wx/gdicmn.h>  // wxSize, wxPoint

namespace kalahari {
namespace core {

/// @brief Singleton settings manager with JSON persistence
///
/// Manages application settings with automatic persistence to user's config directory:
/// - Windows: %APPDATA%/Kalahari/settings.json
/// - Linux:   ~/.config/kalahari/settings.json
/// - macOS:   ~/Library/Application Support/Kalahari/settings.json
///
/// Features:
/// - Type-safe get/set API with default values
/// - Thread-safe access (std::mutex)
/// - Automatic directory creation
/// - Graceful error handling (corrupted JSON → defaults)
/// - JSON format for human-readability
class SettingsManager {
public:
    /// @brief Get singleton instance
    /// @return Reference to the singleton SettingsManager
    static SettingsManager& getInstance();

    // Prevent copy and move
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;
    SettingsManager(SettingsManager&&) = delete;
    SettingsManager& operator=(SettingsManager&&) = delete;

    /// @brief Load settings from disk
    /// @return true on success, false if file doesn't exist or is corrupted (uses defaults)
    bool load();

    /// @brief Save settings to disk
    /// @return true on success, false on I/O error
    bool save();

    /// @brief Reset settings to defaults and delete settings file
    /// Useful for tests or user-requested reset
    void resetToDefaults();

    /// @brief Get setting value with default
    /// @tparam T Type of the value (int, bool, std::string, etc.)
    /// @param key JSON pointer path (e.g., "window.width" or "ui.theme")
    /// @param defaultValue Value to return if key doesn't exist
    /// @return Setting value or defaultValue if not found
    template<typename T>
    T get(const std::string& key, const T& defaultValue) const;

    /// @brief Set setting value
    /// @tparam T Type of the value
    /// @param key JSON pointer path (e.g., "window.width")
    /// @param value Value to set
    template<typename T>
    void set(const std::string& key, const T& value);

    // Convenience methods for common settings

    /// @brief Get window size
    /// @return Window size (default: 1280x800)
    wxSize getWindowSize() const;

    /// @brief Set window size
    /// @param size New window size
    void setWindowSize(const wxSize& size);

    /// @brief Get window position
    /// @return Window position (default: 100, 100)
    wxPoint getWindowPosition() const;

    /// @brief Set window position
    /// @param pos New window position
    void setWindowPosition(const wxPoint& pos);

    /// @brief Check if window is maximized
    /// @return true if maximized (default: false)
    bool isWindowMaximized() const;

    /// @brief Set window maximized state
    /// @param maximized true to maximize
    void setWindowMaximized(bool maximized);

    /// @brief Get UI language
    /// @return Language code (default: "en")
    std::string getLanguage() const;

    /// @brief Set UI language
    /// @param lang Language code ("en", "pl")
    void setLanguage(const std::string& lang);

    /// @brief Get UI theme
    /// @return Theme name (default: "Light")
    std::string getTheme() const;

    /// @brief Set UI theme
    /// @param theme Theme name ("Light", "Dark", "Savanna", "Midnight")
    void setTheme(const std::string& theme);

    /// @brief Get settings file path
    /// @return Absolute path to settings.json
    std::filesystem::path getSettingsFilePath() const;

private:
    /// @brief Private constructor (singleton)
    SettingsManager();

    /// @brief Destructor (saves settings automatically)
    ~SettingsManager();

    /// @brief Get platform-specific settings directory path
    /// @return Path to settings directory (e.g., ~/.config/kalahari/)
    std::filesystem::path getSettingsDirectoryPath() const;

    /// @brief Create default settings (first run)
    void createDefaults();

    /// @brief Convert dot-separated key to JSON pointer
    /// @param key Key like "window.width"
    /// @return JSON pointer like "/window/width"
    std::string keyToJsonPointer(const std::string& key) const;

    /// In-memory settings (nlohmann::json)
    nlohmann::json m_settings;

    /// Path to settings.json file
    std::filesystem::path m_filePath;

    /// Mutex for thread-safe access
    mutable std::mutex m_mutex;
};

// Template implementations must be in header

template<typename T>
T SettingsManager::get(const std::string& key, const T& defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        std::string pointer = keyToJsonPointer(key);
        return m_settings.at(nlohmann::json::json_pointer(pointer)).get<T>();
    } catch (const nlohmann::json::exception&) {
        return defaultValue;
    }
}

template<typename T>
void SettingsManager::set(const std::string& key, const T& value) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string pointer = keyToJsonPointer(key);
    m_settings[nlohmann::json::json_pointer(pointer)] = value;
}

} // namespace core
} // namespace kalahari
