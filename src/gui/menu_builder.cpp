/// @file menu_builder.cpp
/// @brief MenuBuilder implementation (Qt6)

#include "kalahari/gui/menu_builder.h"
#include <QAction>
#include <stdexcept>

namespace kalahari {
namespace gui {

void MenuBuilder::buildMenuBar(CommandRegistry& registry, QMainWindow* parent) {
    if (!parent) {
        throw std::runtime_error("MenuBuilder::buildMenuBar: parent cannot be null");
    }

    QMenuBar* menuBar = parent->menuBar();

    // File menu
    QMenu* fileMenu = buildMenu(tr("&File"), "File", registry, menuBar);
    menuBar->addMenu(fileMenu);

    // Edit menu
    QMenu* editMenu = buildMenu(tr("&Edit"), "Edit", registry, menuBar);
    menuBar->addMenu(editMenu);

    // View menu (will be populated by MainWindow with panel toggles)
    QMenu* viewMenu = new QMenu(tr("&View"), menuBar);
    viewMenu->setObjectName("ViewMenu");  // MainWindow can find and populate it
    menuBar->addMenu(viewMenu);

    // Help menu
    QMenu* helpMenu = buildMenu(tr("&Help"), "Help", registry, menuBar);
    menuBar->addMenu(helpMenu);
}

QMenu* MenuBuilder::buildMenu(const QString& title,
                               const std::string& category,
                               CommandRegistry& registry,
                               QObject* parent) {
    QMenu* menu = new QMenu(title, qobject_cast<QWidget*>(parent));

    // Get commands from category
    std::vector<Command*> commands = registry.getCommandsByCategory(category);

    // Add commands to menu
    addCommandsToMenu(menu, commands, registry);

    return menu;
}

void MenuBuilder::addCommandsToMenu(QMenu* menu,
                                    const std::vector<Command*>& commands,
                                    CommandRegistry& registry) {
    if (!menu) {
        return;
    }

    // Filter commands: only those with showInMenu=true
    for (Command* cmd : commands) {
        if (cmd && cmd->showInMenu) {
            createMenuAction(menu, *cmd, registry);
        }
    }
}

void MenuBuilder::createMenuAction(QMenu* menu,
                                   const Command& command,
                                   CommandRegistry& registry) {
    if (!menu) {
        return;
    }

    // Create QAction from Command
    QAction* action = command.toQAction(menu);

    // Connect signal to CommandRegistry::executeCommand()
    QObject::connect(action, &QAction::triggered, [&registry, id = command.id]() {
        registry.executeCommand(id);
    });

    // Add to menu
    menu->addAction(action);

    // Separator logic (Phase 1: add separator groups)
    // For now: no separators, add in Phase 1
}

} // namespace gui
} // namespace kalahari
