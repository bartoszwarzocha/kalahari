/// @file menu_builder.cpp
/// @brief MenuBuilder implementation with hierarchical menu support (Task #00016)
///
/// OpenSpec #00040: Refactored to use CommandRegistry::getAction() for shared
/// QAction instances. Menu and toolbar now share the same action objects.

#include "kalahari/gui/menu_builder.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/gui/utils/access_keys.h"
#include <QAction>
#include <QApplication>
#include <QMenuBar>
#include <algorithm>
#include <iterator>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

namespace kalahari {
namespace gui {

// Translate technical menu names to display names (i18n), with their access keys: unique
// among the menus of the menu bar and in each menu (test_menu_access_keys)
QString MenuBuilder::menuText(const std::string& technicalName) {
    // Top-level menus
    if (technicalName == "FILE") return QObject::tr("&File");
    if (technicalName == "EDIT") return QObject::tr("&Edit");
    if (technicalName == "BOOK") return QObject::tr("&Book");
    if (technicalName == "INSERT") return QObject::tr("&Insert");
    if (technicalName == "FORMAT") return QObject::tr("F&ormat");
    if (technicalName == "TOOLS") return QObject::tr("&Tools");
    if (technicalName == "ASSISTANT") return QObject::tr("&Assistant");
    if (technicalName == "VIEW") return QObject::tr("&View");
    if (technicalName == "HELP") return QObject::tr("&Help");

    // Submenus
    if (technicalName == "Open") return QObject::tr("O&pen");
    if (technicalName == "Import") return QObject::tr("&Import");
    if (technicalName == "Export") return QObject::tr("&Export");
    if (technicalName == "Text Style") return QObject::tr("Text &Style");
    if (technicalName == "Text Effects") return QObject::tr("&Text Effects");
    if (technicalName == "Change Case") return QObject::tr("&Change Case");
    if (technicalName == "Align Text") return QObject::tr("A&lign Text");
    if (technicalName == "Formatting Character") return QObject::tr("Fo&rmatting Character");
    if (technicalName == "Statistics") return QObject::tr("&Statistics");
    if (technicalName == "Plugins") return QObject::tr("P&lugins");
    if (technicalName == "Assistant Actions") return QObject::tr("Assistant A&ctions");
    if (technicalName == "View Mode") return QObject::tr("View &Mode");
    if (technicalName == "Zoom") return QObject::tr("&Zoom");
    if (technicalName == "Panels") return QObject::tr("&Panels");
    if (technicalName == "Perspectives") return QObject::tr("Pe&rspectives");
    if (technicalName == "Toolbars") return QObject::tr("&Toolbars");
    if (technicalName == "Recent Books") return QObject::tr("&Recent Books");

    // Fallback: as it is, with no access key
    return literalMenuText(QString::fromStdString(technicalName));
}

QString MenuBuilder::menuTitle(const std::string& technicalName) {
    return withoutAccessKey(menuText(technicalName));
}

const std::vector<std::string>& MenuBuilder::topLevelMenus() {
    static const std::vector<std::string> menus = {
        "FILE", "EDIT", "BOOK", "INSERT", "FORMAT", "TOOLS", "ASSISTANT", "VIEW", "HELP"
    };
    return menus;
}

QString MenuBuilder::submenuIconId(const std::string& technicalName) {
    // Registered by registerAllIcons()
    static const std::map<std::string, QString> icons = {
        {"Import", QStringLiteral("menu.import")},
        {"Export", QStringLiteral("menu.export")},
        {"Recent Books", QStringLiteral("menu.recentBooks")},
        {"Formatting Character", QStringLiteral("menu.formattingCharacter")},
        {"Text Style", QStringLiteral("menu.textStyle")},
        {"Text Effects", QStringLiteral("menu.textEffects")},
        {"Change Case", QStringLiteral("menu.changeCase")},
        {"Align Text", QStringLiteral("menu.alignText")},
        {"Statistics", QStringLiteral("menu.statistics")},
        {"Plugins", QStringLiteral("menu.plugins")},
        {"Assistant Actions", QStringLiteral("menu.assistantActions")},
        {"View Mode", QStringLiteral("menu.viewMode")},
        {"Zoom", QStringLiteral("menu.zoom")},
        {"Panels", QStringLiteral("menu.panels")},
        {"Perspectives", QStringLiteral("menu.perspectives")},
        {"Toolbars", QStringLiteral("menu.toolbars")},
    };
    const auto it = icons.find(technicalName);
    return it != icons.end() ? it->second : QString();
}

void MenuBuilder::showSubmenuIcon(QMenu* menu, const QString& iconId) {
    if (menu == nullptr || iconId.isEmpty()) {
        return;
    }
    // As the commands' actions get theirs (CommandRegistry), drawn smaller in a menu
    const auto show = [menu, iconId]() {
        menu->setIcon(core::ArtProvider::getInstance().getIcon(iconId, core::IconContext::Toolbar));
    };
    show();
    QObject::connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, menu,
                     show);
}

void MenuBuilder::buildMenuBar(CommandRegistry& registry, QMainWindow* parent) {
    if (!parent) {
        throw std::runtime_error("MenuBuilder::buildMenuBar: parent cannot be null");
    }

    auto& logger = core::Logger::getInstance();
    logger.debug("MenuBuilder: Building menu bar from CommandRegistry");

    QMenuBar* menuBar = parent->menuBar();
    menuBar->clear();
    m_menuCache.clear();

    // Get all commands
    std::vector<Command> allCommands = registry.getAllCommands();

    logger.debug("MenuBuilder: Processing {} commands", allCommands.size());

    // Build hierarchical menu structure from menuPath
    buildMenuHierarchy(menuBar, allCommands, registry);

    logger.info("MenuBuilder: Menu bar built successfully");
}

void MenuBuilder::buildMenuHierarchy(QMenuBar* menuBar,
                                      const std::vector<Command>& commands,
                                      CommandRegistry& registry) {
    auto& logger = core::Logger::getInstance();

    // Group commands by top-level menu (first element of menuPath)
    std::map<std::string, std::vector<Command>> commandsByMenu;

    for (const Command& cmd : commands) {
        if (!cmd.showInMenu || cmd.menuPath.empty()) {
            continue;  // Skip commands not shown in menu
        }

        // Parse menuPath: "FILE/Import/DOCX" → top="FILE"
        std::istringstream pathStream(cmd.menuPath);
        std::string topLevel;
        std::getline(pathStream, topLevel, '/');

        commandsByMenu[topLevel].push_back(cmd);
    }

    // Create top-level menus in order
    for (const std::string& topLevel : topLevelMenus()) {
        auto it = commandsByMenu.find(topLevel);
        if (it == commandsByMenu.end()) {
            continue;  // No commands for this menu
        }

        // Create top-level menu (translated, with its access key)
        QString title = menuText(topLevel);

        QMenu* topMenu = menuBar->addMenu(title);
        // Menus hide action tooltips by default; they explain why a command is disabled
        topMenu->setToolTipsVisible(true);
        topMenu->setObjectName(QString::fromStdString(topLevel + "Menu"));  // e.g., "VIEWMenu"
        m_menuCache[topLevel] = topMenu;

        logger.debug("MenuBuilder: Created top-level menu: {} (objectName={})",
                     topLevel, topMenu->objectName().toStdString());

        // Sort ALL commands by menuOrder (CRITICAL for correct order!)
        std::vector<Command> sortedCommands = it->second;
        std::sort(sortedCommands.begin(), sortedCommands.end(), [](const Command& a, const Command& b) {
            return a.menuOrder < b.menuOrder;
        });

        // The submenus of each command: "FILE/Import/DOCX" → ["Import"]
        std::vector<std::vector<std::string>> submenus;
        submenus.reserve(sortedCommands.size());
        for (const Command& cmd : sortedCommands) {
            std::vector<std::string> levels;
            std::istringstream pathStream(cmd.menuPath);
            std::string level;
            while (std::getline(pathStream, level, '/')) {
                levels.push_back(level);
            }
            // Skip first (top-level) and last (action)
            std::vector<std::string> path;
            if (levels.size() > 2) {
                path.assign(std::next(levels.begin()), std::prev(levels.end()));
            }
            submenus.push_back(std::move(path));
        }

        // The menu at the path of submenus under the top-level menu (translated, with access
        // keys), made where it is not there yet
        const auto menuAt = [this, topMenu](const std::vector<std::string>& path, size_t depth) {
            QMenu* menu = topMenu;
            for (size_t j = 0; j < depth && j < path.size(); ++j) {
                menu = getOrCreateSubmenu(menu, path[j]);
            }
            return menu;
        };

        // Add commands IN ORDER, creating submenus as needed
        for (size_t i = 0; i < sortedCommands.size(); ++i) {
            const Command& cmd = sortedCommands[i];
            const std::vector<std::string>& path = submenus[i];
            QMenu* currentMenu = menuAt(path, path.size());

            // Create action and add to menu
            createMenuAction(currentMenu, cmd, registry);

            // Add separator if requested AND not last item. After the last item of a
            // submenu the line goes after the submenu, in the menu the next item shares
            if (cmd.addSeparatorAfter && i < sortedCommands.size() - 1) {
                const std::vector<std::string>& next = submenus[i + 1];
                size_t shared = 0;
                while (shared < path.size() && shared < next.size() &&
                       path[shared] == next[shared]) {
                    ++shared;
                }
                menuAt(path, shared)->addSeparator();
            }
        }
    }
}

QMenu* MenuBuilder::getOrCreateSubmenu(QMenu* parent, const std::string& technicalName) {
    if (!parent) {
        return nullptr;
    }

    // Check if submenu already exists
    const QString title = menuText(technicalName);
    for (QAction* action : parent->actions()) {
        if (action->menu() && action->text() == title) {
            return action->menu();
        }
    }

    // Create new submenu
    QMenu* submenu = parent->addMenu(title);
    submenu->setToolTipsVisible(true);
    showSubmenuIcon(submenu, submenuIconId(technicalName));
    return submenu;
}

void MenuBuilder::createMenuAction(QMenu* menu,
                                    const Command& command,
                                    CommandRegistry& registry) {
    if (!menu) {
        return;
    }

    // OpenSpec #00040: Use shared QAction from CommandRegistry
    // CommandRegistry owns the action and handles all configuration:
    // - Icon (via ArtProvider with auto-refresh)
    // - Tooltip, shortcut, checkable state
    // - triggered signal connection to executeCommand()
    QAction* action = registry.getAction(command.id);
    if (action) {
        menu->addAction(action);
    }
}

void MenuBuilder::registerDynamicMenu(const std::string& menuPath, DynamicMenuProvider provider) {
    auto& logger = core::Logger::getInstance();
    logger.debug("MenuBuilder: Registering dynamic menu: {}", menuPath);

    m_dynamicProviders[menuPath] = provider;
}

void MenuBuilder::updateDynamicMenus() {
    auto& logger = core::Logger::getInstance();
    logger.debug("MenuBuilder: Updating {} dynamic menus", m_dynamicProviders.size());

    for (auto& [path, provider] : m_dynamicProviders) {
        // Find menu in cache
        auto it = m_menuCache.find(path);
        if (it == m_menuCache.end()) {
            logger.warn("MenuBuilder: Dynamic menu not found in cache: {}", path);
            continue;
        }

        QMenu* menu = it->second;
        if (!menu) {
            continue;
        }

        // Clear existing dynamic actions (keep static ones)
        menu->clear();

        // Call provider and add actions
        std::vector<QAction*> actions = provider();
        for (QAction* action : actions) {
            menu->addAction(action);
        }

        logger.debug("MenuBuilder: Updated dynamic menu '{}' with {} actions", path, actions.size());
    }
}

QMenu* MenuBuilder::getMenu(const std::string& technicalName) const {
    auto it = m_menuCache.find(technicalName);
    if (it != m_menuCache.end()) {
        return it->second;
    }
    return nullptr;
}

// OpenSpec #00040: refreshIcons() method REMOVED
// Icon refresh is now automatic via CommandRegistry's shared actions.
// CommandRegistry uses ArtProvider::createAction() which connects each
// action to ArtProvider::resourcesChanged() signal for auto-refresh.

} // namespace gui
} // namespace kalahari
