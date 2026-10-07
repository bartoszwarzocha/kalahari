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
/// int width = settings.get<int>("window.width");  // default from settings_schema
/// settings.set("window.width", 1600);
///
/// settings.save();  // Save to disk
/// @endcode

#pragma once

#include <string>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>
#include <kalahari/core/settings_schema.h>
#include <QSize>
#include <QPoint>

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
/// - Type-safe get/set API; defaults come from settings_schema
/// - Change notification (subscribe())
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

    /// @brief Get setting value
    /// @tparam T Type of the value (int, bool, std::string, etc.)
    /// @param key Dot-separated path (e.g., "window.width")
    /// @return Stored value, or the default from settings_schema if the key is
    ///         missing (logs a warning and returns T{} for keys not in the schema)
    template<typename T>
    T get(const std::string& key) const;

    /// @brief Get setting value with a fallback
    /// @tparam T Type of the value (int, bool, std::string, etc.)
    /// @param key Dot-separated path (e.g., "window.width")
    /// @param fallback Value used only if the key is missing and not in settings_schema
    /// @return Stored value, schema default, or fallback (in this order)
    template<typename T>
    T get(const std::string& key, const T& fallback) const;

    /// @brief Set setting value; notifies subscribers if the value changed
    /// @tparam T Type of the value
    /// @param key Dot-separated path (e.g., "window.width")
    /// @param value Value to set
    template<typename T>
    void set(const std::string& key, const T& value);

    /// @brief Callback called with the key of a setting whose value changed
    using ChangeListener = std::function<void(const std::string& key)>;

    /// @brief Get notified about changed settings
    ///
    /// The listener runs on the thread that changed the setting, after the
    /// value is stored. Keys are reported with '.' separators.
    /// @return Id for unsubscribe()
    int subscribe(ChangeListener listener);

    /// @brief Stop notifications for a listener
    /// @param id Value returned by subscribe()
    void unsubscribe(int id);

    // Convenience methods for common settings

    /// @brief Get window size
    /// @return Window size (default: 1280x800)
    QSize getWindowSize() const;

    /// @brief Set window size
    /// @param size New window size
    void setWindowSize(const QSize& size);

    /// @brief Get window position
    /// @return Window position (default: 100, 100)
    QPoint getWindowPosition() const;

    /// @brief Set window position
    /// @param pos New window position
    void setWindowPosition(const QPoint& pos);

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

    // =========================================================================
    // Per-theme icon colors (Task #00025)
    // Stores custom icon colors per theme: icons.themes.<ThemeName>.colorPrimary
    // =========================================================================

    /// @brief Get primary icon color for a specific theme
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @param defaultColor Default color if no custom color is set
    /// @return Color in hex format
    std::string getIconColorPrimaryForTheme(const std::string& themeName,
                                            const std::string& defaultColor) const;

    /// @brief Set primary icon color for a specific theme
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @param color Color in hex format (e.g., "#333333")
    void setIconColorPrimaryForTheme(const std::string& themeName,
                                     const std::string& color);

    /// @brief Get secondary icon color for a specific theme
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @param defaultColor Default color if no custom color is set
    /// @return Color in hex format
    std::string getIconColorSecondaryForTheme(const std::string& themeName,
                                              const std::string& defaultColor) const;

    /// @brief Set secondary icon color for a specific theme
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @param color Color in hex format (e.g., "#999999")
    void setIconColorSecondaryForTheme(const std::string& themeName,
                                       const std::string& color);

    /// @brief Check if a theme has custom icon colors
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @return true if custom colors exist for this theme
    bool hasCustomIconColorsForTheme(const std::string& themeName) const;

    /// @brief Clear custom icon colors for a theme (restore to theme defaults)
    /// @param themeName Theme name (e.g., "Light", "Dark")
    void clearCustomIconColorsForTheme(const std::string& themeName);

    // =========================================================================
    // Per-theme log colors (Task #00027)
    // Stores custom log colors per theme: themes.<ThemeName>.log.<colorKey>
    // Valid colorKey values: trace, debug, info, warning, error, critical, background
    // =========================================================================

    /// @brief Get log color for a specific theme and color key
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @param colorKey Log color key (trace, debug, info, warning, error, critical, background)
    /// @param defaultColor Default color if no custom color is set
    /// @return Color in hex format
    std::string getLogColorForTheme(const std::string& themeName,
                                    const std::string& colorKey,
                                    const std::string& defaultColor) const;

    /// @brief Set log color for a specific theme and color key
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @param colorKey Log color key (trace, debug, info, warning, error, critical, background)
    /// @param color Color in hex format (e.g., "#FF0000")
    void setLogColorForTheme(const std::string& themeName,
                             const std::string& colorKey,
                             const std::string& color);

    /// @brief Check if a theme has custom log colors
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @return true if custom log colors exist for this theme
    bool hasCustomLogColorsForTheme(const std::string& themeName) const;

    /// @brief Clear custom log colors for a theme (restore to theme defaults)
    /// @param themeName Theme name (e.g., "Light", "Dark")
    void clearCustomLogColorsForTheme(const std::string& themeName);

    // =========================================================================
    // Per-theme UI colors (Task #00028)
    // Stores custom UI colors per theme: themes.<ThemeName>.ui.<colorKey>
    // Valid colorKey values: toolTipBase, toolTipText, placeholderText, brightText
    // =========================================================================

    /// @brief Get UI color for a specific theme and color key
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @param colorKey UI color key (toolTipBase, toolTipText, placeholderText, brightText)
    /// @param defaultColor Default color if no custom color is set
    /// @return Color in hex format
    std::string getUiColorForTheme(const std::string& themeName,
                                   const std::string& colorKey,
                                   const std::string& defaultColor) const;

    /// @brief Set UI color for a specific theme and color key
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @param colorKey UI color key (toolTipBase, toolTipText, placeholderText, brightText)
    /// @param color Color in hex format (e.g., "#ffffdc")
    void setUiColorForTheme(const std::string& themeName,
                            const std::string& colorKey,
                            const std::string& color);

    /// @brief Check if a theme has custom UI colors
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @return true if custom UI colors exist for this theme
    bool hasCustomUiColorsForTheme(const std::string& themeName) const;

    /// @brief Clear custom UI colors for a theme (restore to theme defaults)
    /// @param themeName Theme name (e.g., "Light", "Dark")
    void clearCustomUiColorsForTheme(const std::string& themeName);

    // =========================================================================
    // Per-theme palette colors (Task #00028 - Full QPalette support)
    // Stores custom palette colors per theme: themes.<ThemeName>.palette.<colorKey>
    // Valid colorKey values: window, windowText, base, alternateBase, text,
    //   button, buttonText, highlight, highlightedText, light, midlight,
    //   mid, dark, shadow, link, linkVisited
    // =========================================================================

    /// @brief Get palette color for a specific theme and color key
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @param colorKey Palette color key (window, windowText, base, etc.)
    /// @param defaultColor Default color if no custom color is set
    /// @return Color in hex format
    std::string getPaletteColorForTheme(const std::string& themeName,
                                        const std::string& colorKey,
                                        const std::string& defaultColor) const;

    /// @brief Set palette color for a specific theme and color key
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @param colorKey Palette color key (window, windowText, base, etc.)
    /// @param color Color in hex format (e.g., "#f0f0f0")
    void setPaletteColorForTheme(const std::string& themeName,
                                 const std::string& colorKey,
                                 const std::string& color);

    /// @brief Check if a theme has custom palette colors
    /// @param themeName Theme name (e.g., "Light", "Dark")
    /// @return true if custom palette colors exist for this theme
    bool hasCustomPaletteColorsForTheme(const std::string& themeName) const;

    /// @brief Clear custom palette colors for a theme (restore to theme defaults)
    /// @param themeName Theme name (e.g., "Light", "Dark")
    void clearCustomPaletteColorsForTheme(const std::string& themeName);

    /// @brief Get settings file path
    /// @return Absolute path to settings.json
    std::filesystem::path getSettingsFilePath() const;

    /// @brief Check if a setting key exists
    /// @param key JSON pointer path (e.g., "appearance.theme")
    /// @return true if key exists, false otherwise
    bool hasKey(const std::string& key) const;

    /// @brief Remove a setting key
    /// @param key JSON pointer path (e.g., "ui.theme")
    void removeKey(const std::string& key);

    /// @brief Migrate settings from older versions if needed
    /// Called automatically by load()
    void migrateIfNeeded();

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

    /// @brief Bring settings from versions before 1.2 up to date
    /// Moves ui.theme -> appearance.theme and removes keys nothing reads
    void migrateToCurrentVersion();

    /// @brief Convert a JSON value, or nullopt if it has another type
    template<typename T>
    static std::optional<T> convert(const nlohmann::json& value);

    /// @brief Report a get() of a key that has no default in settings_schema
    void warnMissingDefault(const std::string& key) const;

    /// @brief Call change listeners (must be called without m_mutex held)
    void notifyChanged(const std::string& key);

    /// In-memory settings (nlohmann::json)
    nlohmann::json m_settings;

    /// Change listeners by id
    std::map<int, ChangeListener> m_listeners;
    int m_nextListenerId = 1;
    mutable std::mutex m_listenersMutex;

    /// Path to settings.json file
    std::filesystem::path m_filePath;

    /// Mutex for thread-safe access
    mutable std::mutex m_mutex;
};

// Template implementations must be in header

template<typename T>
std::optional<T> SettingsManager::convert(const nlohmann::json& value) {
    try {
        return value.get<T>();
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;  // Stored or default value has another type
    }
}

template<typename T>
T SettingsManager::get(const std::string& key, const T& fallback) const {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const nlohmann::json::json_pointer pointer(keyToJsonPointer(key));
        if (m_settings.contains(pointer)) {
            if (std::optional<T> stored = convert<T>(m_settings.at(pointer))) {
                return *stored;
            }
        }
    }

    if (const nlohmann::json* schemaDefault = settings_schema::defaultValue(key)) {
        if (std::optional<T> value = convert<T>(*schemaDefault)) {
            return *value;
        }
    }
    return fallback;
}

template<typename T>
T SettingsManager::get(const std::string& key) const {
    if (settings_schema::defaultValue(key) == nullptr && !hasKey(key)) {
        warnMissingDefault(key);
    }
    return get<T>(key, T{});
}

template<typename T>
void SettingsManager::set(const std::string& key, const T& value) {
    nlohmann::json newValue = value;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        nlohmann::json& slot = m_settings[nlohmann::json::json_pointer(keyToJsonPointer(key))];
        if (slot == newValue) {
            return;
        }
        slot = std::move(newValue);
    }
    notifyChanged(key);
}

} // namespace core
} // namespace kalahari
