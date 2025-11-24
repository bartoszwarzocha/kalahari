/// @file icon_registry.cpp
/// @brief Implementation of IconRegistry - central icon management system

#include "kalahari/core/icon_registry.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include <QFile>
#include <QSvgRenderer>
#include <QPainter>
#include <QTextStream>

using namespace kalahari::core;

// ============================================================================
// Static Constants
// ============================================================================

const ThemeConfig ThemeConfig::DEFAULT_LIGHT = {
    QColor("#424242"),  // Primary: Dark gray
    QColor("#757575"),  // Secondary: Medium gray
    "Light"
};

const ThemeConfig ThemeConfig::DEFAULT_DARK = {
    QColor("#E0E0E0"),  // Primary: Light gray
    QColor("#B0B0B0"),  // Secondary: Medium-light gray
    "Dark"
};

const IconSizeConfig IconSizeConfig::DEFAULT_SIZES = {
    24,  // toolbar
    16,  // menu
    20,  // panel
    32   // dialog
};

// ============================================================================
// Singleton Instance
// ============================================================================

IconRegistry& IconRegistry::getInstance() {
    static IconRegistry instance;
    return instance;
}

void IconRegistry::initialize() {
    Logger::getInstance().info("IconRegistry: Initializing icon system...");

    // Load customizations from QSettings
    loadFromSettings();

    Logger::getInstance().info("IconRegistry: Icon system initialized ({} icons registered, theme: {})",
        m_icons.size(), m_theme.name.toStdString());
}

// ============================================================================
// Icon Registration
// ============================================================================

void IconRegistry::registerIcon(const QString& actionId,
                                 const QString& defaultSVGPath,
                                 const QString& label)
{
    IconDescriptor desc;
    desc.defaultSVGPath = defaultSVGPath;
    desc.label = label;

    m_icons[actionId] = desc;

    Logger::getInstance().debug("IconRegistry: Registered icon '{}' ({})",
        actionId.toStdString(), label.toStdString());
}

bool IconRegistry::hasIcon(const QString& actionId) const {
    return m_icons.count(actionId) > 0;
}

QStringList IconRegistry::getAllIconIds() const {
    QStringList ids;
    ids.reserve(static_cast<int>(m_icons.size()));

    for (const auto& pair : m_icons) {
        ids.push_back(pair.first);
    }

    ids.sort();
    return ids;
}

// ============================================================================
// Icon Retrieval with Caching
// ============================================================================

QIcon IconRegistry::getIcon(const QString& actionId, const QString& theme, int size) {
    // Check if icon is registered
    if (!hasIcon(actionId)) {
        Logger::getInstance().warn("IconRegistry: Icon '{}' not registered", actionId.toStdString());
        return QIcon();
    }

    // Get IconDescriptor
    const IconDescriptor& desc = m_icons.at(actionId);

    // Determine effective colors
    QColor primary = getEffectivePrimaryColor(actionId);
    QColor secondary = getEffectiveSecondaryColor(actionId);

    // Construct cache key
    QString cacheKey = constructCacheKey(actionId, theme, size, primary, secondary);

    // Check cache
    auto cacheIt = m_pixmapCache.find(cacheKey);
    if (cacheIt != m_pixmapCache.end()) {
        // Cache hit!
        return QIcon(cacheIt->second);
    }

    // Cache miss - load and render
    QString svgPath = desc.getEffectiveSVGPath();
    QString svgContent = loadSVGFromFile(svgPath);

    if (svgContent.isEmpty()) {
        Logger::getInstance().warn("IconRegistry: Failed to load SVG from '{}'", svgPath.toStdString());
        return QIcon();
    }

    // Replace color placeholders
    QString processedSVG = replaceColorPlaceholders(svgContent, primary, secondary);

    // Render to QPixmap
    QPixmap pixmap = renderSVGToPixmap(processedSVG, size);

    if (pixmap.isNull()) {
        Logger::getInstance().error("IconRegistry: Failed to render SVG for '{}'", actionId.toStdString());
        return QIcon();
    }

    // Cache the pixmap
    m_pixmapCache[cacheKey] = pixmap;

    return QIcon(pixmap);
}

const IconDescriptor* IconRegistry::getIconDescriptor(const QString& actionId) const {
    auto it = m_icons.find(actionId);
    if (it != m_icons.end()) {
        return &it->second;
    }
    return nullptr;
}

QString IconRegistry::getIconLabel(const QString& actionId) const {
    const IconDescriptor* desc = getIconDescriptor(actionId);
    if (desc) {
        return desc->label;
    }
    return QString();
}

// ============================================================================
// User Customization
// ============================================================================

void IconRegistry::setCustomIconPath(const QString& actionId, const QString& svgPath) {
    if (!hasIcon(actionId)) {
        Logger::getInstance().warn("IconRegistry: Cannot set custom path for unregistered icon '{}'",
            actionId.toStdString());
        return;
    }

    m_icons[actionId].userSVGPath = svgPath;

    Logger::getInstance().info("IconRegistry: Custom SVG path set for '{}': {}",
        actionId.toStdString(), svgPath.toStdString());

    // Clear cache for this icon
    clearCachePattern(actionId + "_");

    // Save to settings
    saveToSettings();
}

void IconRegistry::clearCustomIconPath(const QString& actionId) {
    if (!hasIcon(actionId)) {
        return;
    }

    m_icons[actionId].userSVGPath = std::nullopt;

    Logger::getInstance().info("IconRegistry: Custom SVG path cleared for '{}'", actionId.toStdString());

    // Clear cache for this icon
    clearCachePattern(actionId + "_");

    // Save to settings
    saveToSettings();
}

void IconRegistry::setIconPrimaryColor(const QString& actionId, const QColor& color) {
    if (!hasIcon(actionId)) {
        Logger::getInstance().warn("IconRegistry: Cannot set primary color for unregistered icon '{}'",
            actionId.toStdString());
        return;
    }

    m_icons[actionId].primaryOverride = color;

    Logger::getInstance().info("IconRegistry: Primary color set for '{}': {}",
        actionId.toStdString(), color.name().toStdString());

    // Clear cache for this icon
    clearCachePattern(actionId + "_");

    // Save to settings
    saveToSettings();
}

void IconRegistry::setIconSecondaryColor(const QString& actionId, const QColor& color) {
    if (!hasIcon(actionId)) {
        Logger::getInstance().warn("IconRegistry: Cannot set secondary color for unregistered icon '{}'",
            actionId.toStdString());
        return;
    }

    m_icons[actionId].secondaryOverride = color;

    Logger::getInstance().info("IconRegistry: Secondary color set for '{}': {}",
        actionId.toStdString(), color.name().toStdString());

    // Clear cache for this icon
    clearCachePattern(actionId + "_");

    // Save to settings
    saveToSettings();
}

void IconRegistry::clearIconColors(const QString& actionId) {
    if (!hasIcon(actionId)) {
        return;
    }

    m_icons[actionId].primaryOverride = std::nullopt;
    m_icons[actionId].secondaryOverride = std::nullopt;

    Logger::getInstance().info("IconRegistry: Color overrides cleared for '{}'", actionId.toStdString());

    // Clear cache for this icon
    clearCachePattern(actionId + "_");

    // Save to settings
    saveToSettings();
}

void IconRegistry::resetAllCustomizations() {
    Logger::getInstance().info("IconRegistry: Resetting ALL customizations to defaults...");

    // Clear all per-icon customizations
    for (auto& pair : m_icons) {
        pair.second.userSVGPath = std::nullopt;
        pair.second.primaryOverride = std::nullopt;
        pair.second.secondaryOverride = std::nullopt;
    }

    // Reset theme
    resetTheme();

    // Reset sizes
    resetSizes();

    // Clear entire cache
    clearCache();

    // Save to settings (clears all custom keys)
    saveToSettings();

    Logger::getInstance().info("IconRegistry: All customizations reset to defaults");
}

// ============================================================================
// Size Configuration
// ============================================================================

void IconRegistry::setSizes(const IconSizeConfig& sizes) {
    m_sizes = sizes;

    Logger::getInstance().info("IconRegistry: Icon sizes updated (toolbar={}, menu={}, panel={}, dialog={})",
        sizes.toolbar, sizes.menu, sizes.panel, sizes.dialog);

    // Clear cache (all icons need re-render with new sizes)
    clearCache();

    // Save to settings
    saveToSettings();
}

void IconRegistry::resetSizes() {
    m_sizes = IconSizeConfig::DEFAULT_SIZES;

    Logger::getInstance().info("IconRegistry: Icon sizes reset to defaults");

    // Clear cache
    clearCache();

    // Save to settings
    saveToSettings();
}

// ============================================================================
// Theme Configuration
// ============================================================================

void IconRegistry::setThemeColors(const QColor& primary, const QColor& secondary, const QString& name) {
    m_theme.primaryColor = primary;
    m_theme.secondaryColor = secondary;
    m_theme.name = name;

    Logger::getInstance().info("IconRegistry: Theme changed to '{}' (primary={}, secondary={})",
        name.toStdString(), primary.name().toStdString(), secondary.name().toStdString());

    // Clear cache (all icons need re-render with new colors)
    clearCache();

    // Save to settings
    saveToSettings();
}

void IconRegistry::resetTheme() {
    m_theme = ThemeConfig::DEFAULT_LIGHT;

    Logger::getInstance().info("IconRegistry: Theme reset to default Light");

    // Clear cache
    clearCache();

    // Save to settings
    saveToSettings();
}

// ============================================================================
// Internal Helpers - SVG Loading and Rendering
// ============================================================================

QString IconRegistry::loadSVGFromFile(const QString& filePath) const {
    QFile file(filePath);

    if (!file.exists()) {
        Logger::getInstance().warn("IconRegistry: Icon file not found: {}", filePath.toStdString());
        return QString();
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::getInstance().error("IconRegistry: Failed to open icon file: {}", filePath.toStdString());
        return QString();
    }

    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();

    return content;
}

QString IconRegistry::replaceColorPlaceholders(const QString& svgContent,
                                                const QColor& primary,
                                                const QColor& secondary) const {
    QString result = svgContent;
    result.replace("{COLOR_PRIMARY}", primary.name());
    result.replace("{COLOR_SECONDARY}", secondary.name());
    return result;
}

QPixmap IconRegistry::renderSVGToPixmap(const QString& svgContent, int size) const {
    QSvgRenderer renderer(svgContent.toUtf8());

    if (!renderer.isValid()) {
        Logger::getInstance().error("IconRegistry: Failed to parse SVG (invalid XML)");
        return QPixmap();
    }

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter);

    return pixmap;
}

// ============================================================================
// Internal Helpers - Color Resolution
// ============================================================================

QColor IconRegistry::getEffectivePrimaryColor(const QString& actionId) const {
    const IconDescriptor* desc = getIconDescriptor(actionId);
    if (desc && desc->primaryOverride.has_value()) {
        // Per-icon override
        return *desc->primaryOverride;
    }

    // Fallback to theme color
    return m_theme.primaryColor;
}

QColor IconRegistry::getEffectiveSecondaryColor(const QString& actionId) const {
    const IconDescriptor* desc = getIconDescriptor(actionId);
    if (desc && desc->secondaryOverride.has_value()) {
        // Per-icon override
        return *desc->secondaryOverride;
    }

    // Fallback to theme color
    return m_theme.secondaryColor;
}

// ============================================================================
// Internal Helpers - Caching
// ============================================================================

QString IconRegistry::constructCacheKey(const QString& actionId,
                                        const QString& theme,
                                        int size,
                                        const QColor& primary,
                                        const QColor& secondary) const {
    return QString("%1_%2_%3_%4_%5")
        .arg(actionId)
        .arg(theme)
        .arg(size)
        .arg(primary.name())
        .arg(secondary.name());
}

void IconRegistry::clearCachePattern(const QString& pattern) {
    // Remove all cache entries matching pattern (e.g., "file.save_*")
    auto it = m_pixmapCache.begin();
    while (it != m_pixmapCache.end()) {
        if (it->first.startsWith(pattern)) {
            it = m_pixmapCache.erase(it);
        } else {
            ++it;
        }
    }

    Logger::getInstance().debug("IconRegistry: Cleared cache entries matching '{}'", pattern.toStdString());
}

void IconRegistry::clearCache() {
    size_t count = m_pixmapCache.size();
    m_pixmapCache.clear();
    Logger::getInstance().debug("IconRegistry: Cleared entire cache ({} entries)", count);
}

// ============================================================================
// Settings Persistence
// ============================================================================

void IconRegistry::saveToSettings() {
    auto& settings = SettingsManager::getInstance();

    // Save theme
    settings.set("icons/theme/primary_color", m_theme.primaryColor.name().toStdString());
    settings.set("icons/theme/secondary_color", m_theme.secondaryColor.name().toStdString());
    settings.set("icons/theme/name", m_theme.name.toStdString());

    // Save sizes
    settings.set("icons/sizes/toolbar", m_sizes.toolbar);
    settings.set("icons/sizes/menu", m_sizes.menu);
    settings.set("icons/sizes/panel", m_sizes.panel);
    settings.set("icons/sizes/dialog", m_sizes.dialog);

    // Save per-icon customizations
    // First, clear all existing custom settings
    // (SettingsManager doesn't have removeGroup, so we manually track and remove)

    for (const auto& pair : m_icons) {
        const QString& actionId = pair.first;
        const IconDescriptor& desc = pair.second;

        QString customPrefix = QString("icons/custom/%1/").arg(actionId);

        if (desc.userSVGPath.has_value()) {
            settings.set((customPrefix + "svg_path").toStdString(), desc.userSVGPath->toStdString());
        } else {
            settings.removeKey((customPrefix + "svg_path").toStdString());
        }

        if (desc.primaryOverride.has_value()) {
            settings.set((customPrefix + "primary_color").toStdString(), desc.primaryOverride->name().toStdString());
        } else {
            settings.removeKey((customPrefix + "primary_color").toStdString());
        }

        if (desc.secondaryOverride.has_value()) {
            settings.set((customPrefix + "secondary_color").toStdString(), desc.secondaryOverride->name().toStdString());
        } else {
            settings.removeKey((customPrefix + "secondary_color").toStdString());
        }
    }

    Logger::getInstance().debug("IconRegistry: Settings saved");
}

void IconRegistry::loadFromSettings() {
    auto& settings = SettingsManager::getInstance();

    // Load theme (with defaults if missing)
    std::string primaryHexStr = settings.get<std::string>("icons/theme/primary_color", "#424242");
    std::string secondaryHexStr = settings.get<std::string>("icons/theme/secondary_color", "#757575");
    std::string themeNameStr = settings.get<std::string>("icons/theme/name", "Light");

    QString primaryHex = QString::fromStdString(primaryHexStr);
    QString secondaryHex = QString::fromStdString(secondaryHexStr);
    QString themeName = QString::fromStdString(themeNameStr);

    QColor primary(primaryHex);
    QColor secondary(secondaryHex);

    if (!primary.isValid()) {
        Logger::getInstance().warn("IconRegistry: Invalid primary color in settings ({}), using default",
            primaryHex.toStdString());
        primary = QColor("#424242");
    }

    if (!secondary.isValid()) {
        Logger::getInstance().warn("IconRegistry: Invalid secondary color in settings ({}), using default",
            secondaryHex.toStdString());
        secondary = QColor("#757575");
    }

    m_theme.primaryColor = primary;
    m_theme.secondaryColor = secondary;
    m_theme.name = themeName;

    // Load sizes (with defaults if missing)
    m_sizes.toolbar = settings.get<int>("icons/sizes/toolbar", 24);
    m_sizes.menu = settings.get<int>("icons/sizes/menu", 16);
    m_sizes.panel = settings.get<int>("icons/sizes/panel", 20);
    m_sizes.dialog = settings.get<int>("icons/sizes/dialog", 32);

    // Load per-icon customizations
    // Note: This requires iterating over all registered icons and checking for custom keys
    // For now, we defer this to after icons are registered (called in initialize())

    Logger::getInstance().debug("IconRegistry: Settings loaded (theme={}, sizes={}x{}x{}x{})",
        themeName.toStdString(), m_sizes.toolbar, m_sizes.menu, m_sizes.panel, m_sizes.dialog);
}
