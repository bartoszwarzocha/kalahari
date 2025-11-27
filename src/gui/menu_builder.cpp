/// @file menu_builder.cpp
/// @brief MenuBuilder implementation with hierarchical menu support (Task #00016)
///
/// OpenSpec #00026: Refactored to use ArtProvider::createAction() for
/// self-updating icons. Removed refreshIcons() method entirely.

#include "kalahari/gui/menu_builder.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/art_provider.h"
#include <QAction>
#include <QMessageBox>
#include <QApplication>
#include <QMenuBar>
#include <algorithm>
#include <sstream>
#include <vector>

namespace kalahari {
namespace gui {

// Helper: Translate technical menu names to display names (i18n)
static QString translateMenuName(const std::string& technicalName) {
    // Top-level menus
    if (technicalName == "FILE") return QObject::tr("File");
    if (technicalName == "EDIT") return QObject::tr("Edit");
    if (technicalName == "BOOK") return QObject::tr("Book");
    if (technicalName == "INSERT") return QObject::tr("Insert");
    if (technicalName == "FORMAT") return QObject::tr("Format");
    if (technicalName == "TOOLS") return QObject::tr("Tools");
    if (technicalName == "ASSISTANT") return QObject::tr("Assistant");
    if (technicalName == "VIEW") return QObject::tr("View");
    if (technicalName == "HELP") return QObject::tr("Help");

    // Submenus
    if (technicalName == "Import") return QObject::tr("Import");
    if (technicalName == "Export") return QObject::tr("Export");
    if (technicalName == "Text Style") return QObject::tr("Text Style");
    if (technicalName == "Statistics") return QObject::tr("Statistics");
    if (technicalName == "Focus Mode") return QObject::tr("Focus Mode");
    if (technicalName == "Plugins") return QObject::tr("Plugins");
    if (technicalName == "Assistant Actions") return QObject::tr("Assistant Actions");
    if (technicalName == "Panels") return QObject::tr("Panels");
    if (technicalName == "Perspectives") return QObject::tr("Perspectives");
    if (technicalName == "Toolbars") return QObject::tr("Toolbars");
    if (technicalName == "Recent Books") return QObject::tr("Recent Books");

    // Fallback: return as-is
    return QString::fromStdString(technicalName);
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
    std::map<std::string, std::vector<Command>> topLevelMenus;

    for (const Command& cmd : commands) {
        if (!cmd.showInMenu || cmd.menuPath.empty()) {
            continue;  // Skip commands not shown in menu
        }

        // Parse menuPath: "FILE/Import/DOCX" → top="FILE"
        std::istringstream pathStream(cmd.menuPath);
        std::string topLevel;
        std::getline(pathStream, topLevel, '/');

        topLevelMenus[topLevel].push_back(cmd);
    }

    // Define menu order (FILE, EDIT, BOOK, INSERT, FORMAT, TOOLS, ASSISTANT, VIEW, HELP)
    std::vector<std::string> menuOrder = {
        "FILE", "EDIT", "BOOK", "INSERT", "FORMAT", "TOOLS", "ASSISTANT", "VIEW", "HELP"
    };

    // Create top-level menus in order
    for (const std::string& topLevel : menuOrder) {
        auto it = topLevelMenus.find(topLevel);
        if (it == topLevelMenus.end()) {
            continue;  // No commands for this menu
        }

        // Create top-level menu (translated)
        QString menuTitle = translateMenuName(topLevel);

        QMenu* topMenu = menuBar->addMenu(menuTitle);
        topMenu->setObjectName(QString::fromStdString(topLevel + "Menu"));  // e.g., "VIEWMenu"
        m_menuCache[topLevel] = topMenu;

        logger.debug("MenuBuilder: Created top-level menu: {} (objectName={})",
                     topLevel, topMenu->objectName().toStdString());

        // Sort ALL commands by menuOrder (CRITICAL for correct order!)
        std::vector<Command> sortedCommands = it->second;
        std::sort(sortedCommands.begin(), sortedCommands.end(), [](const Command& a, const Command& b) {
            return a.menuOrder < b.menuOrder;
        });

        // Add commands IN ORDER, creating submenus as needed
        for (size_t i = 0; i < sortedCommands.size(); ++i) {
            const Command& cmd = sortedCommands[i];

            // Parse path: "FILE/Import/DOCX" → levels=["FILE", "Import", "DOCX"]
            std::vector<std::string> levels;
            std::istringstream pathStream(cmd.menuPath);
            std::string level;
            while (std::getline(pathStream, level, '/')) {
                levels.push_back(level);
            }

            if (levels.empty()) {
                continue;
            }

            // Navigate/create submenu hierarchy (translated)
            QMenu* currentMenu = topMenu;
            for (size_t j = 1; j < levels.size() - 1; ++j) {  // Skip first (top-level) and last (action)
                currentMenu = getOrCreateSubmenu(currentMenu, translateMenuName(levels[j]));
            }

            // Create action and add to menu
            createMenuAction(currentMenu, cmd, registry);

            // Add separator if requested AND not last item
            if (cmd.addSeparatorAfter && i < sortedCommands.size() - 1) {
                currentMenu->addSeparator();
            }
        }
    }
}

QMenu* MenuBuilder::getOrCreateSubmenu(QMenu* parent, const QString& title) {
    if (!parent) {
        return nullptr;
    }

    // Check if submenu already exists
    for (QAction* action : parent->actions()) {
        if (action->menu() && action->text() == title) {
            return action->menu();
        }
    }

    // Create new submenu
    QMenu* submenu = parent->addMenu(title);
    return submenu;
}

void MenuBuilder::createMenuAction(QMenu* menu,
                                    const Command& command,
                                    CommandRegistry& registry) {
    if (!menu) {
        return;
    }

    auto& artProvider = core::ArtProvider::getInstance();

    // OpenSpec #00026: Use ArtProvider::createAction() for self-updating icons
    // This creates an action that automatically refreshes on theme/color changes
    QAction* action = artProvider.createAction(
        QString::fromStdString(command.id),
        QString::fromStdString(command.label),
        menu,
        core::IconContext::Menu
    );

    // Set tooltip and shortcut from command
    if (!command.tooltip.empty()) {
        action->setToolTip(QString::fromStdString(command.tooltip));
        action->setStatusTip(QString::fromStdString(command.tooltip));
    }
    if (!command.shortcut.isEmpty()) {
        action->setShortcut(command.shortcut.toQKeySequence());
    }

    // Set checkable state if command has isChecked callback
    if (command.isChecked) {
        action->setCheckable(true);
        action->setChecked(command.isChecked());
    }

    // Handle phase marker: if phase > 0, override execute with QMessageBox
    if (command.phase > 0) {
        QObject::connect(action, &QAction::triggered, [phase = command.phase, label = command.label]() {
            QMessageBox::information(
                qApp->activeWindow(),
                QObject::tr("Feature Not Implemented"),
                QObject::tr("'%1' will be available in Phase %2.\n\n"
                            "This feature is planned but not yet implemented.")
                    .arg(QString::fromStdString(label))
                    .arg(phase)
            );
        });
    } else {
        // Normal command execution via CommandRegistry
        QObject::connect(action, &QAction::triggered, [&registry, id = command.id]() {
            registry.executeCommand(id);
        });
    }

    // Add to menu
    menu->addAction(action);
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

// OpenSpec #00026: refreshIcons() method REMOVED
// Icon refresh is now automatic via ArtProvider::createAction() which
// connects each action to ArtProvider::resourcesChanged() signal.
// When theme/colors/sizes change, ArtProvider emits resourcesChanged()
// and all managed actions update their icons automatically.

} // namespace gui
} // namespace kalahari
