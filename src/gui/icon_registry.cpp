/// @file icon_registry.cpp
/// @brief Implementation of IconRegistry - central icon management system

#include "kalahari/gui/icon_registry.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/event_bus.h"

using namespace kalahari::gui;
using namespace kalahari::core;

// ============================================================================
// IconDescriptor Serialization (JSON)
// ============================================================================

wxString IconDescriptor::toJSON() const {
    // TODO: Implement JSON serialization (Phase 1 later)
    return "{}";
}

IconDescriptor IconDescriptor::fromJSON([[maybe_unused]] const wxString& json) {
    // TODO: Implement JSON deserialization (Phase 1 later)
    IconDescriptor desc;
    return desc;
}

// ============================================================================
// IconSizeConfig Serialization (JSON)
// ============================================================================

wxString IconSizeConfig::toJSON() const {
    return wxString::Format("{\"toolbar\":%d,\"menu\":%d,\"panel\":%d,\"dialog\":%d}",
        toolbar, menu, panel, dialog);
}

IconSizeConfig IconSizeConfig::fromJSON([[maybe_unused]] const wxString& json) {
    // TODO: Implement proper JSON parsing (Phase 1 later)
    IconSizeConfig config;
    return config;
}

// ============================================================================
// IconRegistry Implementation
// ============================================================================

IconRegistry& IconRegistry::getInstance() {
    static IconRegistry instance;
    return instance;
}

void IconRegistry::initialize() {
    Logger::getInstance().info("IconRegistry: Initializing icon system...");

    // Default icons will be registered here when icons_material.h is generated
    // For now, just log initialization

    Logger::getInstance().info("IconRegistry: Icon system initialized (0 icons registered)");
}

// ============================================================================
// Icon Registration
// ============================================================================

void IconRegistry::registerIcon(const wxString& actionId,
                                 const std::string& defaultSVG,
                                 const wxString& label)
{
    IconDescriptor desc;
    desc.defaultSVG = defaultSVG;
    desc.label = label;

    m_icons[actionId] = desc;

    Logger::getInstance().debug("IconRegistry: Registered icon '{}' ({})",
        actionId.ToStdString(), label.ToStdString());
}

bool IconRegistry::hasIcon(const wxString& actionId) const {
    return m_icons.count(actionId) > 0;
}

std::vector<wxString> IconRegistry::getAllActionIds() const {
    std::vector<wxString> ids;
    ids.reserve(m_icons.size());

    for (const auto& pair : m_icons) {
        ids.push_back(pair.first);
    }

    return ids;
}

// ============================================================================
// Icon Retrieval
// ============================================================================

const IconDescriptor* IconRegistry::getIcon(const wxString& actionId) const {
    auto it = m_icons.find(actionId);
    if (it != m_icons.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string IconRegistry::getEffectiveSVG(const wxString& actionId) const {
    const IconDescriptor* desc = getIcon(actionId);
    if (desc) {
        return desc->getEffectiveSVG();
    }
    return ""; // Not found
}

wxColour IconRegistry::getEffectiveColor(const wxString& actionId) const {
    const IconDescriptor* desc = getIcon(actionId);
    if (desc && desc->colorOverride.has_value()) {
        // Per-icon color override
        return *desc->colorOverride;
    }

    // Fallback to theme color
    return getThemeColor();
}

wxString IconRegistry::getIconLabel(const wxString& actionId) const {
    const IconDescriptor* desc = getIcon(actionId);
    if (desc) {
        return desc->label;
    }
    return wxEmptyString;
}

// ============================================================================
// User Customization
// ============================================================================

void IconRegistry::setCustomIcon(const wxString& actionId, const std::string& svgData) {
    auto it = m_icons.find(actionId);
    if (it != m_icons.end()) {
        it->second.userSVG = svgData;

        Logger::getInstance().info("IconRegistry: Custom icon set for '{}'",
            actionId.ToStdString());

        notifyIconChanged(actionId);
        saveToSettings();
    } else {
        Logger::getInstance().warn("IconRegistry: Cannot set custom icon for unregistered action '{}'",
            actionId.ToStdString());
    }
}

void IconRegistry::clearCustomIcon(const wxString& actionId) {
    auto it = m_icons.find(actionId);
    if (it != m_icons.end() && it->second.userSVG.has_value()) {
        it->second.userSVG.reset();

        Logger::getInstance().info("IconRegistry: Custom icon cleared for '{}'",
            actionId.ToStdString());

        notifyIconChanged(actionId);
        saveToSettings();
    }
}

void IconRegistry::setIconColor(const wxString& actionId, const wxColour& color) {
    auto it = m_icons.find(actionId);
    if (it != m_icons.end()) {
        it->second.colorOverride = color;

        Logger::getInstance().info("IconRegistry: Color override set for '{}' ({}, {}, {})",
            actionId.ToStdString(), color.Red(), color.Green(), color.Blue());

        notifyIconChanged(actionId);
        saveToSettings();
    } else {
        Logger::getInstance().warn("IconRegistry: Cannot set color for unregistered action '{}'",
            actionId.ToStdString());
    }
}

void IconRegistry::clearIconColor(const wxString& actionId) {
    auto it = m_icons.find(actionId);
    if (it != m_icons.end() && it->second.colorOverride.has_value()) {
        it->second.colorOverride.reset();

        Logger::getInstance().info("IconRegistry: Color override cleared for '{}'",
            actionId.ToStdString());

        notifyIconChanged(actionId);
        saveToSettings();
    }
}

void IconRegistry::resetAllCustomizations() {
    Logger::getInstance().info("IconRegistry: Resetting all customizations to factory defaults...");

    int resetCount = 0;
    for (auto& pair : m_icons) {
        if (pair.second.isCustomized()) {
            pair.second.userSVG.reset();
            pair.second.colorOverride.reset();
            resetCount++;
        }
    }

    // Reset sizes to defaults
    m_sizes = IconSizeConfig();

    // Reset theme to Light
    m_theme = ColorTheme::Light;
    m_customThemeColor = wxNullColour;

    Logger::getInstance().info("IconRegistry: Reset {} customized icons", resetCount);

    notifyIconChanged("*"); // Global change
    saveToSettings();
}

// ========================================================================
// Size Configuration
// ========================================================================

void IconRegistry::setSizes(const IconSizeConfig& sizes) {
    m_sizes = sizes;

    Logger::getInstance().info("IconRegistry: Size configuration changed (toolbar={}, menu={}, panel={}, dialog={})",
        sizes.toolbar, sizes.menu, sizes.panel, sizes.dialog);

    notifyIconChanged("*"); // Global change (affects all icons)
    saveToSettings();
}

// ========================================================================
// Theme Configuration
// ========================================================================

void IconRegistry::setTheme(ColorTheme theme, wxColour customColor) {
    m_theme = theme;
    if (theme == ColorTheme::Custom) {
        m_customThemeColor = customColor;
    }

    const char* themeName = (theme == ColorTheme::Light) ? "Light" :
                            (theme == ColorTheme::Dark) ? "Dark" : "Custom";

    Logger::getInstance().info("IconRegistry: Theme changed to '{}'", themeName);

    notifyIconChanged("*"); // Global change (affects all icons)
    saveToSettings();
}

wxColour IconRegistry::getThemeColor() const {
    switch (m_theme) {
        case ColorTheme::Light:
            return wxColour(0x21, 0x21, 0x21); // #212121 (dark grey)
        case ColorTheme::Dark:
            return wxColour(0xE0, 0xE0, 0xE0); // #E0E0E0 (light grey)
        case ColorTheme::Custom:
            return m_customThemeColor.IsOk() ? m_customThemeColor : wxColour(0x21, 0x21, 0x21);
    }
    return wxColour(0x21, 0x21, 0x21); // Fallback
}

// ========================================================================
// Persistence (SettingsManager Integration)
// ========================================================================

void IconRegistry::loadFromSettings() {
    Logger::getInstance().info("IconRegistry: Loading customizations from settings...");

    auto& settings = SettingsManager::getInstance();

    // Load theme
    std::string themeStr = settings.get<std::string>("icon_system.theme", "Light");
    if (themeStr == "Dark") {
        m_theme = ColorTheme::Dark;
    } else if (themeStr == "Custom") {
        m_theme = ColorTheme::Custom;
        // TODO: Load custom color from settings
    } else {
        m_theme = ColorTheme::Light;
    }

    // Load sizes
    m_sizes.toolbar = settings.get<int>("icon_system.sizes.toolbar", 24);
    m_sizes.menu = settings.get<int>("icon_system.sizes.menu", 16);
    m_sizes.panel = settings.get<int>("icon_system.sizes.panel", 20);
    m_sizes.dialog = settings.get<int>("icon_system.sizes.dialog", 32);

    // TODO: Load per-icon customizations from settings (Phase 1 later)

    Logger::getInstance().info("IconRegistry: Loaded settings (theme={}, toolbar={}px, menu={}px)",
        themeStr, m_sizes.toolbar, m_sizes.menu);
}

void IconRegistry::saveToSettings() {
    Logger::getInstance().debug("IconRegistry: Saving customizations to settings...");

    auto& settings = SettingsManager::getInstance();

    // Save theme
    std::string themeStr = (m_theme == ColorTheme::Light) ? "Light" :
                           (m_theme == ColorTheme::Dark) ? "Dark" : "Custom";
    settings.set("icon_system.theme", themeStr);

    // Save sizes
    settings.set("icon_system.sizes.toolbar", m_sizes.toolbar);
    settings.set("icon_system.sizes.menu", m_sizes.menu);
    settings.set("icon_system.sizes.panel", m_sizes.panel);
    settings.set("icon_system.sizes.dialog", m_sizes.dialog);

    // TODO: Save per-icon customizations (Phase 1 later)

    settings.save();
}

// ========================================================================
// Change Notification (EventBus Integration)
// ========================================================================

void IconRegistry::notifyIconChanged(const wxString& actionId) {
    // Fire event through EventBus
    auto& eventBus = EventBus::getInstance();

    Event evt(EVENT_ICON_CHANGED, actionId.ToStdString());

    eventBus.emit(evt);

    Logger::getInstance().debug("IconRegistry: Fired EVENT_ICON_CHANGED (actionId='{}')",
        actionId.ToStdString());
}
