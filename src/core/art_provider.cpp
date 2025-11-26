/// @file art_provider.cpp
/// @brief Implementation of ArtProvider - Central Visual Resource Manager

#include "kalahari/core/art_provider.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"

namespace kalahari {
namespace core {

// ============================================================================
// Singleton Instance
// ============================================================================

ArtProvider& ArtProvider::getInstance() {
    static ArtProvider instance;
    return instance;
}

// ============================================================================
// Constructor
// ============================================================================

ArtProvider::ArtProvider()
    : QObject(nullptr)
    , m_iconTheme("twotone")
{
}

// ============================================================================
// Initialization
// ============================================================================

void ArtProvider::initialize() {
    Logger::getInstance().info("ArtProvider: Initializing central visual resource manager...");

    // Load icon theme from settings
    auto& settings = SettingsManager::getInstance();
    m_iconTheme = QString::fromStdString(
        settings.get<std::string>("appearance.iconTheme", "twotone")
    );

    // CRITICAL: Synchronize IconRegistry colors from ThemeManager BEFORE any icons are loaded
    // This ensures toolbars/menus get correct colors at startup
    const Theme& currentTheme = ThemeManager::getInstance().getCurrentTheme();
    IconRegistry::getInstance().setThemeColors(
        currentTheme.colors.primary,
        currentTheme.colors.secondary,
        QString::fromStdString(currentTheme.name)
    );
    Logger::getInstance().info("ArtProvider: Synchronized IconRegistry colors from ThemeManager (primary={}, secondary={})",
        currentTheme.colors.primary.name().toStdString(),
        currentTheme.colors.secondary.name().toStdString());

    // Connect to ThemeManager for automatic updates
    connect(&ThemeManager::getInstance(), &ThemeManager::themeChanged,
            this, &ArtProvider::onThemeChanged);

    Logger::getInstance().info("ArtProvider: Initialized (iconTheme={})",
        m_iconTheme.toStdString());
}

// ============================================================================
// Action Factory
// ============================================================================

QAction* ArtProvider::createAction(const QString& cmdId,
                                    const QString& text,
                                    QObject* parent,
                                    IconContext context)
{
    QAction* action = new QAction(text, parent);

    // Store cmdId and context in action's data for refresh
    QVariantMap data;
    data["cmdId"] = cmdId;
    data["context"] = static_cast<int>(context);
    action->setData(data);

    // Set initial icon
    action->setIcon(getIcon(cmdId, context));

    // Connect to resourcesChanged for auto-update
    connect(this, &ArtProvider::resourcesChanged, action, [this, action]() {
        refreshAction(action);
    });

    // Track managed action
    m_managedActions.insert(action);

    // Remove from tracking when destroyed
    connect(action, &QObject::destroyed, this, [this, action]() {
        m_managedActions.remove(action);
    });

    Logger::getInstance().debug("ArtProvider: Created action '{}' ({})",
        cmdId.toStdString(), text.toStdString());

    return action;
}

// ============================================================================
// Direct Icon Access
// ============================================================================

QIcon ArtProvider::getIcon(const QString& cmdId, IconContext context) {
    int size = getIconSize(context);
    return IconRegistry::getInstance().getIcon(cmdId, m_iconTheme, size);
}

QPixmap ArtProvider::getPixmap(const QString& cmdId, int size) {
    QIcon icon = IconRegistry::getInstance().getIcon(cmdId, m_iconTheme, size);
    return icon.pixmap(size, size);
}

QPixmap ArtProvider::getPreviewPixmap(const QString& cmdId,
                                       int logicalSize,
                                       qreal devicePixelRatio,
                                       const QString& iconThemeOverride)
{
    QString theme = iconThemeOverride.isEmpty() ? m_iconTheme : iconThemeOverride;
    int physicalSize = static_cast<int>(logicalSize * devicePixelRatio);

    // Get icon at physical size
    QIcon icon = IconRegistry::getInstance().getIcon(cmdId, theme, physicalSize);
    QPixmap pixmap = icon.pixmap(physicalSize, physicalSize);

    // Set device pixel ratio for HiDPI display
    pixmap.setDevicePixelRatio(devicePixelRatio);

    return pixmap;
}

// ============================================================================
// Theme Information
// ============================================================================

QString ArtProvider::getIconTheme() const {
    return m_iconTheme;
}

QColor ArtProvider::getPrimaryColor() const {
    return IconRegistry::getInstance().getThemeConfig().primaryColor;
}

QColor ArtProvider::getSecondaryColor() const {
    return IconRegistry::getInstance().getThemeConfig().secondaryColor;
}

QString ArtProvider::getThemeName() const {
    return IconRegistry::getInstance().getThemeConfig().name;
}

// ============================================================================
// Size Configuration
// ============================================================================

int ArtProvider::getIconSize(IconContext context) const {
    const auto& sizes = IconRegistry::getInstance().getSizes();

    switch (context) {
        case IconContext::Toolbar:   return sizes.toolbar;
        case IconContext::Menu:      return sizes.menu;
        case IconContext::TreeView:  return sizes.treeView;
        case IconContext::TabBar:    return sizes.tabBar;
        case IconContext::StatusBar: return sizes.statusBar;
        case IconContext::Button:    return sizes.button;
        case IconContext::Panel:     return sizes.panel;
        case IconContext::Dialog:    return sizes.dialog;
        case IconContext::ComboBox:  return sizes.comboBox;
        default:                     return sizes.toolbar;
    }
}

void ArtProvider::setIconSize(IconContext context, int size) {
    auto sizes = IconRegistry::getInstance().getSizes();

    switch (context) {
        case IconContext::Toolbar:   sizes.toolbar = size; break;
        case IconContext::Menu:      sizes.menu = size; break;
        case IconContext::TreeView:  sizes.treeView = size; break;
        case IconContext::TabBar:    sizes.tabBar = size; break;
        case IconContext::StatusBar: sizes.statusBar = size; break;
        case IconContext::Button:    sizes.button = size; break;
        case IconContext::Panel:     sizes.panel = size; break;
        case IconContext::Dialog:    sizes.dialog = size; break;
        case IconContext::ComboBox:  sizes.comboBox = size; break;
    }

    IconRegistry::getInstance().setSizes(sizes);

    Logger::getInstance().info("ArtProvider: Icon size changed for context {} to {}px",
        static_cast<int>(context), size);

    emit resourcesChanged();
}

// ============================================================================
// Theme Configuration
// ============================================================================

void ArtProvider::setIconTheme(const QString& theme) {
    if (m_iconTheme == theme) {
        return;
    }

    m_iconTheme = theme;

    // Save to settings
    auto& settings = SettingsManager::getInstance();
    settings.set("appearance.iconTheme", theme.toStdString());

    Logger::getInstance().info("ArtProvider: Icon theme changed to '{}'",
        theme.toStdString());

    emit resourcesChanged();
}

void ArtProvider::setPrimaryColor(const QColor& color) {
    auto& registry = IconRegistry::getInstance();
    auto config = registry.getThemeConfig();

    if (config.primaryColor == color) {
        return;
    }

    registry.setThemeColors(color, config.secondaryColor, config.name);

    Logger::getInstance().info("ArtProvider: Primary color changed to {}",
        color.name().toStdString());

    emit resourcesChanged();
}

void ArtProvider::setSecondaryColor(const QColor& color) {
    auto& registry = IconRegistry::getInstance();
    auto config = registry.getThemeConfig();

    if (config.secondaryColor == color) {
        return;
    }

    registry.setThemeColors(config.primaryColor, color, config.name);

    Logger::getInstance().info("ArtProvider: Secondary color changed to {}",
        color.name().toStdString());

    emit resourcesChanged();
}

// ============================================================================
// Theme Changed Slot
// ============================================================================

void ArtProvider::onThemeChanged() {
    Logger::getInstance().info("ArtProvider: Theme changed, refreshing all icons...");
    emit resourcesChanged();
}

// ============================================================================
// Internal Helpers
// ============================================================================

void ArtProvider::refreshAction(QAction* action) {
    if (!action) {
        return;
    }

    QVariantMap data = action->data().toMap();
    QString cmdId = data.value("cmdId").toString();
    IconContext context = static_cast<IconContext>(data.value("context").toInt());

    if (cmdId.isEmpty()) {
        return;
    }

    QIcon newIcon = getIcon(cmdId, context);
    action->setIcon(newIcon);

    Logger::getInstance().debug("ArtProvider: Refreshed action '{}'", cmdId.toStdString());
}

} // namespace core
} // namespace kalahari
