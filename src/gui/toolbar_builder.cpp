/// @file toolbar_builder.cpp
/// @brief ToolbarBuilder implementation (Qt6)

#include "kalahari/gui/toolbar_builder.h"
#include <QAction>
#include <stdexcept>

namespace kalahari {
namespace gui {

QToolBar* ToolbarBuilder::buildToolBar(CommandRegistry& registry, QMainWindow* parent) {
    if (!parent) {
        throw std::runtime_error("ToolbarBuilder::buildToolBar: parent cannot be null");
    }

    // Create toolbar
    QToolBar* toolbar = new QToolBar(tr("Standard"), parent);
    toolbar->setObjectName("StandardToolbar");
    toolbar->setMovable(false);  // Phase 0: fixed toolbar

    // Add File actions
    addActionsFromCategory(toolbar, registry, "File");

    // Separator
    addSeparator(toolbar);

    // Add Edit actions
    addActionsFromCategory(toolbar, registry, "Edit");

    // Future: Format category (Phase 1)
    // addSeparator(toolbar);
    // addActionsFromCategory(toolbar, registry, "Format");

    return toolbar;
}

void ToolbarBuilder::addActionsFromCategory(QToolBar* toolbar,
                                            CommandRegistry& registry,
                                            const std::string& category) {
    if (!toolbar) {
        return;
    }

    // Get all commands in category
    std::vector<Command*> commands = registry.getCommandsByCategory(category);

    // Filter: only commands with showInToolbar=true
    for (Command* cmd : commands) {
        if (cmd && cmd->showInToolbar) {
            createToolAction(toolbar, *cmd, registry);
        }
    }
}

void ToolbarBuilder::addSeparator(QToolBar* toolbar) {
    if (toolbar) {
        toolbar->addSeparator();
    }
}

void ToolbarBuilder::createToolAction(QToolBar* toolbar,
                                      const Command& command,
                                      CommandRegistry& registry) {
    if (!toolbar) {
        return;
    }

    // Create QAction from Command
    QAction* action = command.toQAction(toolbar);

    // Connect signal to CommandRegistry::executeCommand()
    QObject::connect(action, &QAction::triggered, [&registry, id = command.id]() {
        registry.executeCommand(id);
    });

    // Add to toolbar
    toolbar->addAction(action);
}

} // namespace gui
} // namespace kalahari
