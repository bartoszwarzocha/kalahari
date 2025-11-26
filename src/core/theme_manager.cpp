/// @file theme_manager.cpp
/// @brief Implementation of ThemeManager

#include "kalahari/core/theme_manager.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QApplication>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace kalahari::core;

// ============================================================================
// Singleton Instance
// ============================================================================

ThemeManager& ThemeManager::getInstance() {
    static ThemeManager instance;
    return instance;
}

// ============================================================================
// Constructor
// ============================================================================

ThemeManager::ThemeManager() {
    Logger::getInstance().info("ThemeManager: Initializing...");

    // Create fallback theme first (used if loading fails)
    m_currentTheme.name = "Fallback";
    m_currentTheme.version = "1.0";
    m_currentTheme.colors.primary = QColor("#333333");
    m_currentTheme.colors.secondary = QColor("#999999");
    m_currentTheme.colors.accent = QColor("#0078D4");
    m_currentTheme.colors.background = QColor("#FFFFFF");
    m_currentTheme.colors.text = QColor("#000000");
    m_currentTheme.log.info = QColor("#000000");
    m_currentTheme.log.debug = QColor("#666666");
    m_currentTheme.log.background = QColor("#F5F5F5");
    m_baseTheme = m_currentTheme;

    // Load saved theme from settings (default: "Light")
    auto& settings = SettingsManager::getInstance();
    QString savedThemeName = QString::fromStdString(settings.getTheme());

    try {
        m_currentTheme = loadTheme(savedThemeName);
        m_baseTheme = m_currentTheme;
        Logger::getInstance().info("ThemeManager: Theme '{}' loaded from settings",
                                   savedThemeName.toStdString());
    } catch (const std::exception& e) {
        Logger::getInstance().error("ThemeManager: Failed to load theme '{}': {}",
                                    savedThemeName.toStdString(), e.what());
        Logger::getInstance().warn("ThemeManager: Using fallback theme");
    }

    // Task #00025: Load user's custom color overrides from per-theme storage
    // This ensures icons have correct colors at startup (not just after Apply in Settings)
    std::string themeName = savedThemeName.toStdString();

    // Get theme defaults for fallback
    std::string defaultPrimary = (savedThemeName == "Dark") ? "#999999" : "#333333";
    std::string defaultSecondary = (savedThemeName == "Dark") ? "#333333" : "#999999";

    // Load per-theme custom colors if they exist
    if (settings.hasCustomIconColorsForTheme(themeName)) {
        QString savedPrimary = QString::fromStdString(
            settings.getIconColorPrimaryForTheme(themeName, defaultPrimary));
        QString savedSecondary = QString::fromStdString(
            settings.getIconColorSecondaryForTheme(themeName, defaultSecondary));

        QColor primaryColor(savedPrimary);
        QColor secondaryColor(savedSecondary);

        if (primaryColor.isValid()) {
            m_currentTheme.colors.primary = primaryColor;
            m_overrides["primary"] = primaryColor;
        }
        if (secondaryColor.isValid()) {
            m_currentTheme.colors.secondary = secondaryColor;
            m_overrides["secondary"] = secondaryColor;
        }

        Logger::getInstance().info("ThemeManager: Loaded custom icon colors for theme '{}' (primary={}, secondary={})",
            themeName, savedPrimary.toStdString(), savedSecondary.toStdString());
    } else {
        Logger::getInstance().info("ThemeManager: Using default icon colors for theme '{}'", themeName);
    }

    // Apply palette to QApplication at startup for full theme support
    // Requires Fusion style (set in main.cpp before ThemeManager init)
    QPalette palette = m_currentTheme.palette.toQPalette();
    QApplication::setPalette(palette);
    Logger::getInstance().info("ThemeManager: Initial palette applied (Fusion style)");
}

// ============================================================================
// Theme Loading
// ============================================================================

std::optional<nlohmann::json> ThemeManager::loadThemeFile(const QString& themeName) {
    QString relativePath = QString("resources/themes/%1.json").arg(themeName);

    // Resolve path relative to application directory
    QString themePath = QCoreApplication::applicationDirPath() + "/" + relativePath;

    QFile file(themePath);
    if (!file.exists()) {
        Logger::getInstance().error("ThemeManager: Theme file not found: {}", themePath.toStdString());
        return std::nullopt;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::getInstance().error("ThemeManager: Cannot open theme file: {}", themePath.toStdString());
        return std::nullopt;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    try {
        nlohmann::json json = nlohmann::json::parse(jsonData.toStdString());
        Logger::getInstance().debug("ThemeManager: Theme file '{}' parsed successfully", themeName.toStdString());
        return json;
    } catch (const nlohmann::json::parse_error& e) {
        Logger::getInstance().error("ThemeManager: JSON parse error in '{}': {}",
                                    themeName.toStdString(), e.what());
        return std::nullopt;
    }
}

Theme ThemeManager::loadTheme(const QString& themeName) {
    Logger::getInstance().info("ThemeManager: Loading theme '{}'", themeName.toStdString());

    auto jsonOpt = loadThemeFile(themeName);
    if (!jsonOpt.has_value()) {
        throw std::runtime_error("Failed to load theme file: " + themeName.toStdString());
    }

    Theme theme = Theme::fromJson(jsonOpt.value());
    Logger::getInstance().info("ThemeManager: Theme '{}' loaded successfully", theme.name);

    return theme;
}

// ============================================================================
// Theme Management
// ============================================================================

const Theme& ThemeManager::getCurrentTheme() const {
    return m_currentTheme;
}

QStringList ThemeManager::getAvailableThemes() const {
    QStringList themes;

    QString themesPath = QCoreApplication::applicationDirPath() + "/resources/themes";
    QDir themesDir(themesPath);
    if (!themesDir.exists()) {
        Logger::getInstance().warn("ThemeManager: Themes directory not found: {}", themesPath.toStdString());
        return themes;
    }

    QStringList filters;
    filters << "*.json";
    themesDir.setNameFilters(filters);

    QFileInfoList files = themesDir.entryInfoList(QDir::Files);
    for (const QFileInfo& fileInfo : files) {
        QString themeName = fileInfo.baseName();  // Name without extension
        themes.append(themeName);
    }

    Logger::getInstance().debug("ThemeManager: Found {} available themes", themes.size());
    return themes;
}

void ThemeManager::applyTheme(const Theme& theme) {
    m_currentTheme = theme;
    m_baseTheme = theme;
    m_overrides.clear();  // Clear overrides when switching themes

    // Apply QPalette to QApplication for native widget styling
    // This is the key step that makes Qt Fusion style use our theme colors!
    QPalette palette = theme.palette.toQPalette();
    QApplication::setPalette(palette);

    Logger::getInstance().debug("ThemeManager: QPalette applied with {} color roles",
                                static_cast<int>(QPalette::NColorRoles));

    // Save to settings
    auto& settings = SettingsManager::getInstance();
    settings.setTheme(theme.name);
    settings.save();

    Logger::getInstance().info("ThemeManager: Applied theme '{}' (palette + settings)", theme.name);

    emit themeChanged(m_currentTheme);
}

bool ThemeManager::switchTheme(const QString& themeName) {
    try {
        Theme theme = loadTheme(themeName);
        applyTheme(theme);

        // Task #00025: Load per-theme custom colors if they exist
        auto& settings = SettingsManager::getInstance();
        std::string themeNameStr = themeName.toStdString();

        if (settings.hasCustomIconColorsForTheme(themeNameStr)) {
            std::string defaultPrimary = (themeName == "Dark") ? "#999999" : "#333333";
            std::string defaultSecondary = (themeName == "Dark") ? "#333333" : "#999999";

            std::string savedPrimary = settings.getIconColorPrimaryForTheme(themeNameStr, defaultPrimary);
            std::string savedSecondary = settings.getIconColorSecondaryForTheme(themeNameStr, defaultSecondary);

            std::map<std::string, QColor> overrides;
            QColor primaryColor(QString::fromStdString(savedPrimary));
            QColor secondaryColor(QString::fromStdString(savedSecondary));

            if (primaryColor.isValid()) {
                overrides["primary"] = primaryColor;
            }
            if (secondaryColor.isValid()) {
                overrides["secondary"] = secondaryColor;
            }

            if (!overrides.empty()) {
                applyColorOverrides(overrides);
                Logger::getInstance().info("ThemeManager: switchTheme loaded custom colors for '{}' (primary={}, secondary={})",
                    themeNameStr, savedPrimary, savedSecondary);
            }
        }

        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("ThemeManager: Failed to switch to theme '{}': {}",
                                    themeName.toStdString(), e.what());
        return false;
    }
}

// ============================================================================
// Color Overrides
// ============================================================================

void ThemeManager::applyColorOverrides(const std::map<std::string, QColor>& overrides) {
    m_overrides = overrides;
    m_currentTheme = m_baseTheme;  // Start from base theme

    // Apply overrides
    for (const auto& [key, color] : overrides) {
        if (key == "primary") {
            m_currentTheme.colors.primary = color;
        } else if (key == "secondary") {
            m_currentTheme.colors.secondary = color;
        } else if (key == "accent") {
            m_currentTheme.colors.accent = color;
        } else if (key == "background") {
            m_currentTheme.colors.background = color;
        } else if (key == "text") {
            m_currentTheme.colors.text = color;
        } else if (key == "log.info") {
            m_currentTheme.log.info = color;
        } else if (key == "log.debug") {
            m_currentTheme.log.debug = color;
        } else if (key == "log.background") {
            m_currentTheme.log.background = color;
        }
    }

    Logger::getInstance().info("ThemeManager: Applied {} color overrides", overrides.size());
    emit themeChanged(m_currentTheme);
}

void ThemeManager::resetColorOverrides() {
    m_overrides.clear();
    m_currentTheme = m_baseTheme;

    Logger::getInstance().info("ThemeManager: Reset all color overrides");
    emit themeChanged(m_currentTheme);
}
