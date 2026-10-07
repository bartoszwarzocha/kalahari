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
    m_currentTheme.colors.infoBarBorder = QColor("#FFD54F");
    m_currentTheme.colors.infoBarBackground = QColor("#FFF8E1");
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

    // Load user's custom per-theme colors, so icons and widgets have them at startup
    m_baseTheme = m_currentTheme;
    applyStoredColors();

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

QColor ThemeManager::editorColor(const std::string& key, const QColor& fallback) const {
    auto it = m_currentTheme.editor.find(key);
    return it != m_currentTheme.editor.end() ? it->second : fallback;
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

bool ThemeManager::switchTheme(const QString& themeName) {
    if (!reloadTheme(themeName)) {
        return false;
    }
    SettingsManager::getInstance().save();
    return true;
}

void ThemeManager::applyStoredColors() {
    // A stored color replaces the theme's own value; colors that are not stored keep
    // the theme file values (the user may have changed only some of them)
    auto& settings = SettingsManager::getInstance();
    const std::string themeName = m_currentTheme.name;

    auto apply = [this](const std::string& overrideKey, QColor& target, const std::string& saved) {
        QColor color(QString::fromStdString(saved));
        if (color.isValid() && color != target) {
            target = color;
            m_overrides[overrideKey] = color;
        }
    };
    auto current = [](const QColor& color) { return color.name().toStdString(); };

    Theme& theme = m_currentTheme;
    apply("primary", theme.colors.primary,
          settings.getIconColorPrimaryForTheme(themeName, current(theme.colors.primary)));
    apply("secondary", theme.colors.secondary,
          settings.getIconColorSecondaryForTheme(themeName, current(theme.colors.secondary)));

    // Info panel and Dashboard colors, stored as themes.<name>.colors.<key>
    const std::pair<const char*, QColor Theme::Colors::*> panelColors[] = {
        {"infoHeader", &Theme::Colors::infoHeader}, {"infoPrimary", &Theme::Colors::infoPrimary},
        {"infoSecondary", &Theme::Colors::infoSecondary},
        {"dashboardPrimary", &Theme::Colors::dashboardPrimary},
        {"dashboardSecondary", &Theme::Colors::dashboardSecondary},
    };
    for (const auto& [key, field] : panelColors) {
        QColor& target = theme.colors.*field;
        apply(std::string("colors.") + key, target,
              settings.get<std::string>("themes." + themeName + ".colors." + key, current(target)));
    }

    const std::pair<const char*, QColor Theme::Palette::*> paletteColors[] = {
        {"window", &Theme::Palette::window}, {"windowText", &Theme::Palette::windowText},
        {"base", &Theme::Palette::base}, {"alternateBase", &Theme::Palette::alternateBase},
        {"text", &Theme::Palette::text}, {"button", &Theme::Palette::button},
        {"buttonText", &Theme::Palette::buttonText}, {"highlight", &Theme::Palette::highlight},
        {"highlightedText", &Theme::Palette::highlightedText}, {"light", &Theme::Palette::light},
        {"midlight", &Theme::Palette::midlight}, {"mid", &Theme::Palette::mid},
        {"dark", &Theme::Palette::dark}, {"shadow", &Theme::Palette::shadow},
        {"link", &Theme::Palette::link}, {"linkVisited", &Theme::Palette::linkVisited},
    };
    for (const auto& [key, field] : paletteColors) {
        QColor& target = theme.palette.*field;
        apply(std::string("palette.") + key, target,
              settings.getPaletteColorForTheme(themeName, key, current(target)));
    }

    // Tooltip, placeholder and bright text are stored by the Settings dialog under "ui"
    const std::pair<const char*, QColor Theme::Palette::*> uiColors[] = {
        {"toolTipBase", &Theme::Palette::toolTipBase}, {"toolTipText", &Theme::Palette::toolTipText},
        {"placeholderText", &Theme::Palette::placeholderText}, {"brightText", &Theme::Palette::brightText},
    };
    for (const auto& [key, field] : uiColors) {
        QColor& target = theme.palette.*field;
        apply(std::string("palette.") + key, target,
              settings.getUiColorForTheme(themeName, key, current(target)));
    }

    const std::pair<const char*, QColor Theme::LogColors::*> logColors[] = {
        {"trace", &Theme::LogColors::trace}, {"debug", &Theme::LogColors::debug},
        {"info", &Theme::LogColors::info}, {"warning", &Theme::LogColors::warning},
        {"error", &Theme::LogColors::error}, {"critical", &Theme::LogColors::critical},
        {"background", &Theme::LogColors::background},
    };
    for (const auto& [key, field] : logColors) {
        QColor& target = theme.log.*field;
        apply(std::string("log.") + key, target,
              settings.getLogColorForTheme(themeName, key, current(target)));
    }

    // Editor colors: whatever the theme file lists
    const std::string editorPrefix = "themes." + themeName + ".editor.";
    for (auto& [key, target] : theme.editor) {
        apply("editor." + key, target, settings.get<std::string>(editorPrefix + key, current(target)));
    }

    if (!m_overrides.empty()) {
        Logger::getInstance().info("ThemeManager: Applied {} stored colors for theme '{}'",
                                   m_overrides.size(), themeName);
    }
}

bool ThemeManager::reloadTheme(const QString& themeName,
                               const std::map<std::string, QColor>& extraOverrides) {
    Theme theme;
    try {
        theme = loadTheme(themeName);
    } catch (const std::exception& e) {
        Logger::getInstance().error("ThemeManager: Failed to load theme '{}': {}",
                                    themeName.toStdString(), e.what());
        return false;
    }

    m_baseTheme = theme;
    m_currentTheme = theme;
    m_overrides.clear();
    applyStoredColors();
    for (const auto& [key, color] : extraOverrides) {
        setColorOverride(QString::fromStdString(key), color);
    }
    SettingsManager::getInstance().setTheme(theme.name);

    refreshTheme();
    Logger::getInstance().info("ThemeManager: Reloaded theme '{}'", theme.name);
    return true;
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
        } else if (key == "infoBarBorder") {
            m_currentTheme.colors.infoBarBorder = color;
        } else if (key == "infoBarBackground") {
            m_currentTheme.colors.infoBarBackground = color;
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
    // Custom color: infoBarBorder
    else if (key == "infoBarBorder" || key == "colors.infoBarBorder") {
        m_currentTheme.colors.infoBarBorder = color;
    }
    // Custom color: infoBarBackground
    else if (key == "infoBarBackground" || key == "colors.infoBarBackground") {
        m_currentTheme.colors.infoBarBackground = color;
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
    } else if (key.startsWith("editor.")) {
        m_currentTheme.editor[key.mid(7).toStdString()] = color;
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
