/// @file toolbar_manager.cpp
/// @brief Implementation of ToolbarManager

#include "kalahari/gui/toolbar_manager.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/core/logger.h"
#include <QSettings>
#include <QApplication>

namespace kalahari {
namespace gui {

ToolbarManager::ToolbarManager(QMainWindow* mainWindow)
    : m_mainWindow(mainWindow)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("ToolbarManager: Constructor called");

    initializeConfigs();

    logger.debug("ToolbarManager: Initialized with {} toolbar configs", m_configs.size());
}

void ToolbarManager::initializeConfigs() {
    // File Toolbar
    m_configs["file"] = {
        "file",
        "File Toolbar",
        Qt::TopToolBarArea,
        true,  // visible by default
        {"file.new", "file.open", "file.save", "file.saveAs", "file.close"}
    };

    // Edit Toolbar
    m_configs["edit"] = {
        "edit",
        "Edit Toolbar",
        Qt::TopToolBarArea,
        true,  // visible by default
        {"edit.undo", "edit.redo", "_SEPARATOR_",
         "edit.cut", "edit.copy", "edit.paste", "edit.selectAll"}
    };

    // Book Toolbar
    m_configs["book"] = {
        "book",
        "Book Toolbar",
        Qt::TopToolBarArea,
        true,  // visible by default
        {"book.newChapter", "book.newCharacter", "book.newLocation", "book.properties"}
    };

    // View Toolbar (panel toggles)
    m_configs["view"] = {
        "view",
        "View Toolbar",
        Qt::TopToolBarArea,
        true,  // visible by default
        {"view.navigator", "view.properties", "view.search", "view.assistant", "view.log"}
    };

    // Tools Toolbar
    m_configs["tools"] = {
        "tools",
        "Tools Toolbar",
        Qt::TopToolBarArea,
        true,  // visible by default
        {"tools.spellcheck", "tools.stats.wordCount", "tools.focus.normal"}
    };
}

void ToolbarManager::createToolbars(CommandRegistry& registry) {
    auto& logger = core::Logger::getInstance();
    logger.debug("ToolbarManager: Creating toolbars from configurations");

    // Create toolbars in order: File, Edit, Book, View, Tools
    std::vector<std::string> order = {"file", "edit", "book", "view", "tools"};

    for (const std::string& id : order) {
        auto it = m_configs.find(id);
        if (it != m_configs.end()) {
            QToolBar* toolbar = createToolbar(it->second, registry);
            m_toolbars[id] = toolbar;
            logger.debug("ToolbarManager: Created toolbar '{}'", id);
        }
    }

    logger.info("ToolbarManager: Created {} toolbars", m_toolbars.size());
}

QToolBar* ToolbarManager::createToolbar(const ToolbarConfig& config, CommandRegistry& registry) {
    auto& logger = core::Logger::getInstance();

    // Create toolbar
    QToolBar* toolbar = new QToolBar(QObject::tr(config.label.c_str()), m_mainWindow);
    toolbar->setObjectName(QString::fromStdString(config.id));  // For QSettings

    // Add actions
    for (const std::string& cmdId : config.commandIds) {
        if (cmdId == "_SEPARATOR_") {
            toolbar->addSeparator();
        } else {
            Command* cmd = registry.getCommand(cmdId);
            if (cmd && cmd->canExecute()) {
                QAction* action = cmd->toQAction(toolbar);
                toolbar->addAction(action);

                // Connect action to command execution
                QObject::connect(action, &QAction::triggered, [cmdId, &registry]() {
                    registry.executeCommand(cmdId);
                });
            } else {
                logger.warn("ToolbarManager: Command '{}' not found or not executable", cmdId);
            }
        }
    }

    // Configure toolbar appearance and behavior
    toolbar->setMovable(true);
    toolbar->setFloatable(true);
    toolbar->setIconSize(QSize(24, 24));  // 24x24 icons
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);  // Icons only, no text

    // Add to main window
    m_mainWindow->addToolBar(config.defaultArea, toolbar);
    toolbar->setVisible(config.defaultVisible);

    return toolbar;
}

QToolBar* ToolbarManager::getToolbar(const std::string& id) {
    auto it = m_toolbars.find(id);
    return (it != m_toolbars.end()) ? it->second : nullptr;
}

void ToolbarManager::showToolbar(const std::string& id, bool visible) {
    auto& logger = core::Logger::getInstance();

    QToolBar* toolbar = getToolbar(id);
    if (!toolbar) {
        logger.warn("ToolbarManager: Toolbar '{}' not found", id);
        return;
    }

    toolbar->setVisible(visible);
    logger.debug("ToolbarManager: Toolbar '{}' visibility set to {}", id, visible);

    // Update corresponding View menu action if it exists
    auto actionIt = m_viewActions.find(id);
    if (actionIt != m_viewActions.end()) {
        QAction* action = actionIt->second;
        action->blockSignals(true);  // Prevent recursive triggering
        action->setChecked(visible);
        action->blockSignals(false);
    }
}

bool ToolbarManager::isToolbarVisible(const std::string& id) {
    QToolBar* toolbar = getToolbar(id);
    return toolbar ? toolbar->isVisible() : false;
}

void ToolbarManager::saveState() {
    auto& logger = core::Logger::getInstance();
    logger.debug("ToolbarManager: Saving toolbar state");

    QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
    settings.beginGroup("Toolbars");

    // Save visibility for each toolbar
    for (const auto& [id, toolbar] : m_toolbars) {
        settings.setValue(QString::fromStdString(id + "/visible"), toolbar->isVisible());
    }

    settings.endGroup();

    logger.debug("ToolbarManager: Toolbar state saved");

    // Note: Toolbar positions are automatically saved by Qt via
    // QMainWindow::saveState() in MainWindow::closeEvent()
}

void ToolbarManager::restoreState() {
    auto& logger = core::Logger::getInstance();
    logger.debug("ToolbarManager: Restoring toolbar state");

    QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
    settings.beginGroup("Toolbars");

    // Restore visibility for each toolbar
    for (const auto& [id, toolbar] : m_toolbars) {
        QString key = QString::fromStdString(id + "/visible");
        bool visible = settings.value(key, true).toBool();  // Default to true
        toolbar->setVisible(visible);

        // Update View menu action if exists
        auto actionIt = m_viewActions.find(id);
        if (actionIt != m_viewActions.end()) {
            actionIt->second->setChecked(visible);
        }

        logger.debug("ToolbarManager: Toolbar '{}' visibility restored to {}", id, visible);
    }

    settings.endGroup();

    // Note: Toolbar positions are automatically restored by Qt via
    // QMainWindow::restoreState() in MainWindow::showEvent()
}

void ToolbarManager::createViewMenuActions(QMenu* viewMenu) {
    auto& logger = core::Logger::getInstance();
    logger.debug("ToolbarManager: Creating View menu actions");

    if (!viewMenu) {
        logger.error("ToolbarManager: View menu is null");
        return;
    }

    // Add separator before "Toolbars" section
    viewMenu->addSeparator();

    // Create checkable action for each toolbar
    std::vector<std::string> order = {"file", "edit", "book", "view", "tools"};

    for (const std::string& id : order) {
        auto it = m_configs.find(id);
        if (it == m_configs.end()) {
            continue;
        }

        const ToolbarConfig& config = it->second;
        QToolBar* toolbar = getToolbar(id);
        if (!toolbar) {
            continue;
        }

        // Create checkable action
        QString actionText = QObject::tr(config.label.c_str());
        QAction* action = new QAction(actionText, viewMenu);
        action->setCheckable(true);
        action->setChecked(toolbar->isVisible());

        // Store toolbar ID in action data
        action->setData(QString::fromStdString(id));

        // Connect to toggle slot
        QObject::connect(action, &QAction::toggled, [this, id](bool checked) {
            showToolbar(id, checked);
        });

        // Also connect toolbar visibility changes back to action
        QObject::connect(toolbar, &QToolBar::visibilityChanged, [action](bool visible) {
            action->blockSignals(true);
            action->setChecked(visible);
            action->blockSignals(false);
        });

        viewMenu->addAction(action);
        m_viewActions[id] = action;

        logger.debug("ToolbarManager: Created View menu action for toolbar '{}'", id);
    }

    logger.info("ToolbarManager: Created {} View menu actions", m_viewActions.size());
}

} // namespace gui
} // namespace kalahari
