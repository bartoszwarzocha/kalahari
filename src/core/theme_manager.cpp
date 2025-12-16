/// @file theme_manager.cpp
/// @brief Implementation of ThemeManager

#include "kalahari/core/theme_manager.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/resource_paths.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/stylesheet.h"
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
    m_currentTheme.colors.infoPrimary = QColor("#1C69A8");
    m_currentTheme.colors.infoHeader = QColor("#2B4763");
    m_currentTheme.colors.dashboardSecondary = QColor("#36BBA7");
    m_currentTheme.colors.dashboardPrimary = QColor("#18786F");
    m_currentTheme.colors.infoSecondary = QColor("#34A6F4");
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

    // Task #00028: Load per-theme custom palette colors if they exist
    bool isDark = (savedThemeName == "Dark");
    if (settings.hasCustomPaletteColorsForTheme(themeName)) {
        // Load and apply all 16 palette colors
        auto loadPaletteColor = [&](const std::string& key, QColor& target, const std::string& defValue) {
            std::string saved = settings.getPaletteColorForTheme(themeName, key, defValue);
            QColor color(QString::fromStdString(saved));
            if (color.isValid()) {
                target = color;
                m_overrides["palette." + key] = color;
            }
        };

        // Basic Colors
        loadPaletteColor("window", m_currentTheme.palette.window, isDark ? "#2d2d2d" : "#f0f0f0");
        loadPaletteColor("windowText", m_currentTheme.palette.windowText, isDark ? "#e0e0e0" : "#000000");
        loadPaletteColor("base", m_currentTheme.palette.base, isDark ? "#252525" : "#ffffff");
        loadPaletteColor("alternateBase", m_currentTheme.palette.alternateBase, isDark ? "#323232" : "#f5f5f5");
        loadPaletteColor("text", m_currentTheme.palette.text, isDark ? "#e0e0e0" : "#000000");
        // Button Colors
        loadPaletteColor("button", m_currentTheme.palette.button, isDark ? "#404040" : "#e0e0e0");
        loadPaletteColor("buttonText", m_currentTheme.palette.buttonText, isDark ? "#e0e0e0" : "#000000");
        // Selection Colors
        loadPaletteColor("highlight", m_currentTheme.palette.highlight, "#0078d4");
        loadPaletteColor("highlightedText", m_currentTheme.palette.highlightedText, "#ffffff");
        // 3D Effect Colors
        loadPaletteColor("light", m_currentTheme.palette.light, isDark ? "#505050" : "#ffffff");
        loadPaletteColor("midlight", m_currentTheme.palette.midlight, isDark ? "#404040" : "#e0e0e0");
        loadPaletteColor("mid", m_currentTheme.palette.mid, isDark ? "#303030" : "#a0a0a0");
        loadPaletteColor("dark", m_currentTheme.palette.dark, isDark ? "#202020" : "#606060");
        loadPaletteColor("shadow", m_currentTheme.palette.shadow, "#000000");
        // Link Colors
        loadPaletteColor("link", m_currentTheme.palette.link, isDark ? "#5eb3f0" : "#0078d4");
        loadPaletteColor("linkVisited", m_currentTheme.palette.linkVisited, isDark ? "#b48ade" : "#551a8b");
        // UI Colors
        loadPaletteColor("toolTipBase", m_currentTheme.palette.toolTipBase, isDark ? "#3c3c3c" : "#ffffdc");
        loadPaletteColor("toolTipText", m_currentTheme.palette.toolTipText, isDark ? "#e0e0e0" : "#000000");
        loadPaletteColor("placeholderText", m_currentTheme.palette.placeholderText, isDark ? "#808080" : "#a0a0a0");
        loadPaletteColor("brightText", m_currentTheme.palette.brightText, "#ffffff");

        Logger::getInstance().info("ThemeManager: Loaded custom palette colors for theme '{}'", themeName);
    }

    // Task #00027: Load per-theme custom log colors if they exist
    if (settings.hasCustomLogColorsForTheme(themeName)) {
        auto loadLogColor = [&](const std::string& key, QColor& target, const std::string& defValue) {
            std::string saved = settings.getLogColorForTheme(themeName, key, defValue);
            QColor color(QString::fromStdString(saved));
            if (color.isValid()) {
                target = color;
                m_overrides["log." + key] = color;
            }
        };

        loadLogColor("trace", m_currentTheme.log.trace, isDark ? "#FF66FF" : "#CC00CC");
        loadLogColor("debug", m_currentTheme.log.debug, isDark ? "#FF66FF" : "#CC00CC");
        loadLogColor("info", m_currentTheme.log.info, isDark ? "#FFFFFF" : "#000000");
        loadLogColor("warning", m_currentTheme.log.warning, isDark ? "#FFA500" : "#FF8C00");
        loadLogColor("error", m_currentTheme.log.error, isDark ? "#FF4444" : "#CC0000");
        loadLogColor("critical", m_currentTheme.log.critical, isDark ? "#FF4444" : "#CC0000");
        loadLogColor("background", m_currentTheme.log.background, isDark ? "#252525" : "#F5F5F5");

        Logger::getInstance().info("ThemeManager: Loaded custom log colors for theme '{}'", themeName);
    }

    // Apply palette to QApplication at startup for full theme support
    // Requires Fusion style (set in main.cpp before ThemeManager init)
    QPalette palette = m_currentTheme.palette.toQPalette();
    QApplication::setPalette(palette);

    // Apply QSS stylesheet (OpenSpec #00028)
    QString qss = StyleSheet::generate(m_currentTheme);
    if (qApp) {
        qApp->setStyleSheet(qss);
        Logger::getInstance().debug("ThemeManager: Applied stylesheet ({} chars)", qss.length());
    }

    Logger::getInstance().info("ThemeManager: Initial palette and stylesheet applied (Fusion style)");
}

// ============================================================================
// Theme Loading
// ============================================================================

std::optional<nlohmann::json> ThemeManager::loadThemeFile(const QString& themeName) {
    // Use ResourcePaths for cross-platform resource resolution (OpenSpec #00029)
    QString themePath = ResourcePaths::getInstance().getThemePath(themeName);

    if (themePath.isEmpty()) {
        Logger::getInstance().error("ThemeManager: Resources not found, cannot load theme '{}'",
            themeName.toStdString());
        return std::nullopt;
    }

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

    // Use ResourcePaths for cross-platform resource resolution (OpenSpec #00029)
    QString themesPath = ResourcePaths::getInstance().getThemesDir();
    if (themesPath.isEmpty()) {
        Logger::getInstance().warn("ThemeManager: Resources not found, cannot list themes");
        return themes;
    }

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

    // Apply QSS stylesheet (OpenSpec #00028)
    QString qss = StyleSheet::generate(theme);
    if (qApp) {
        qApp->setStyleSheet(qss);
    }

    Logger::getInstance().debug("ThemeManager: Applied stylesheet ({} chars)", qss.length());
    Logger::getInstance().debug("ThemeManager: QPalette applied with {} color roles",
                                static_cast<int>(QPalette::NColorRoles));

    // Save to settings
    auto& settings = SettingsManager::getInstance();
    settings.setTheme(theme.name);
    settings.save();

    Logger::getInstance().info("ThemeManager: Applied theme '{}' (palette + stylesheet + settings)", theme.name);

    emit themeChanged(m_currentTheme);
    emit themeStyleChanged();
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
        } else if (key == "infoPrimary") {
            m_currentTheme.colors.infoPrimary = color;
        } else if (key == "infoHeader") {
            m_currentTheme.colors.infoHeader = color;
        } else if (key == "dashboardSecondary") {
            m_currentTheme.colors.dashboardSecondary = color;
        } else if (key == "dashboardPrimary") {
            m_currentTheme.colors.dashboardPrimary = color;
        } else if (key == "infoSecondary") {
            m_currentTheme.colors.infoSecondary = color;
        } else if (key == "log.info") {
            m_currentTheme.log.info = color;
        } else if (key == "log.debug") {
            m_currentTheme.log.debug = color;
        } else if (key == "log.background") {
            m_currentTheme.log.background = color;
        }
    }

    // Re-apply palette with overrides
    QPalette palette = m_currentTheme.palette.toQPalette();
    QApplication::setPalette(palette);

    // Re-apply stylesheet with updated colors (OpenSpec #00028)
    QString qss = StyleSheet::generate(m_currentTheme);
    if (qApp) {
        qApp->setStyleSheet(qss);
    }

    Logger::getInstance().info("ThemeManager: Applied {} color overrides", overrides.size());
    Logger::getInstance().debug("ThemeManager: Re-applied stylesheet ({} chars)", qss.length());

    emit themeChanged(m_currentTheme);
    emit themeStyleChanged();
}

void ThemeManager::setColorOverride(const QString& key, const QColor& color) {
    if (!color.isValid()) {
        Logger::getInstance().warn("ThemeManager: Invalid color for key '{}'", key.toStdString());
        return;
    }

    // Store the override
    m_overrides[key.toStdString()] = color;

    // Apply override to current theme
    // Theme colors
    if (key == "primary" || key == "colors.primary") {
        m_currentTheme.colors.primary = color;
    } else if (key == "secondary" || key == "colors.secondary") {
        m_currentTheme.colors.secondary = color;
    } else if (key == "accent" || key == "colors.accent") {
        m_currentTheme.colors.accent = color;
    } else if (key == "background" || key == "colors.background") {
        m_currentTheme.colors.background = color;
    } else if (key == "text" || key == "colors.text") {
        m_currentTheme.colors.text = color;
    }
    // Custom color: dashboardSecondary
    else if (key == "dashboardSecondary" || key == "colors.dashboardSecondary") {
        m_currentTheme.colors.dashboardSecondary = color;
    }
    // Custom color: dashboardPrimary
    else if (key == "dashboardPrimary" || key == "colors.dashboardPrimary") {
        m_currentTheme.colors.dashboardPrimary = color;
    }
    // Custom color: infoSecondary
    else if (key == "infoSecondary" || key == "colors.infoSecondary") {
        m_currentTheme.colors.infoSecondary = color;
    }
    // Custom color: infoPrimary
    else if (key == "infoPrimary" || key == "colors.infoPrimary") {
        m_currentTheme.colors.infoPrimary = color;
    }
    // Custom color: infoHeader
    else if (key == "infoHeader" || key == "colors.infoHeader") {
        m_currentTheme.colors.infoHeader = color;
    }
    // Log colors
    else if (key == "log.trace") {
        m_currentTheme.log.trace = color;
    } else if (key == "log.debug") {
        m_currentTheme.log.debug = color;
    } else if (key == "log.info") {
        m_currentTheme.log.info = color;
    } else if (key == "log.warning") {
        m_currentTheme.log.warning = color;
    } else if (key == "log.error") {
        m_currentTheme.log.error = color;
    } else if (key == "log.critical") {
        m_currentTheme.log.critical = color;
    } else if (key == "log.background") {
        m_currentTheme.log.background = color;
    }
    // Palette colors (16 standard QPalette roles)
    else if (key == "palette.window") {
        m_currentTheme.palette.window = color;
    } else if (key == "palette.windowText") {
        m_currentTheme.palette.windowText = color;
    } else if (key == "palette.base") {
        m_currentTheme.palette.base = color;
    } else if (key == "palette.alternateBase") {
        m_currentTheme.palette.alternateBase = color;
    } else if (key == "palette.text") {
        m_currentTheme.palette.text = color;
    } else if (key == "palette.button") {
        m_currentTheme.palette.button = color;
    } else if (key == "palette.buttonText") {
        m_currentTheme.palette.buttonText = color;
    } else if (key == "palette.highlight") {
        m_currentTheme.palette.highlight = color;
    } else if (key == "palette.highlightedText") {
        m_currentTheme.palette.highlightedText = color;
    } else if (key == "palette.light") {
        m_currentTheme.palette.light = color;
    } else if (key == "palette.midlight") {
        m_currentTheme.palette.midlight = color;
    } else if (key == "palette.mid") {
        m_currentTheme.palette.mid = color;
    } else if (key == "palette.dark") {
        m_currentTheme.palette.dark = color;
    } else if (key == "palette.shadow") {
        m_currentTheme.palette.shadow = color;
    } else if (key == "palette.link") {
        m_currentTheme.palette.link = color;
    } else if (key == "palette.linkVisited") {
        m_currentTheme.palette.linkVisited = color;
    }
    // Additional UI palette colors
    else if (key == "palette.toolTipBase") {
        m_currentTheme.palette.toolTipBase = color;
    } else if (key == "palette.toolTipText") {
        m_currentTheme.palette.toolTipText = color;
    } else if (key == "palette.placeholderText") {
        m_currentTheme.palette.placeholderText = color;
    } else if (key == "palette.brightText") {
        m_currentTheme.palette.brightText = color;
    } else {
        Logger::getInstance().warn("ThemeManager: Unknown color key '{}'", key.toStdString());
    }
}

void ThemeManager::refreshTheme() {
    // Re-apply palette with current overrides
    QPalette palette = m_currentTheme.palette.toQPalette();
    QApplication::setPalette(palette);

    // Re-apply stylesheet with updated colors
    QString qss = StyleSheet::generate(m_currentTheme);
    if (qApp) {
        qApp->setStyleSheet(qss);
    }

    Logger::getInstance().debug("ThemeManager: Theme refreshed (palette + stylesheet)");

    emit themeChanged(m_currentTheme);
    emit themeStyleChanged();
}

void ThemeManager::resetColorOverrides() {
    m_overrides.clear();
    m_currentTheme = m_baseTheme;

    Logger::getInstance().info("ThemeManager: Reset all color overrides");
    emit themeChanged(m_currentTheme);
}
