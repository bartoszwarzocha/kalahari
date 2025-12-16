/// @file toolbar_manager.cpp
/// @brief Implementation of ToolbarManager
///
/// OpenSpec #00026: Refactored to use ArtProvider::createAction() for
/// self-updating icons. Removed refreshIcons() method entirely.

#include "kalahari/gui/toolbar_manager.h"
#include "kalahari/gui/dialogs/toolbar_manager_dialog.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/command.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include <QSettings>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStatusBar>
#include <QRegularExpression>
#include <QMessageBox>
#include <QFontComboBox>
#include <QSpinBox>
#include <QLabel>

namespace kalahari {
namespace gui {

// OpenSpec #00037: Toolbar config version for reset detection
// Increment this when changing default toolbar visibility or layout
// Version history:
//   1: Initial version (all toolbars visible by default)
//   2: OpenSpec #00037 - File, Edit, Quick Actions, Help visible; others hidden
//   3: Fix toolbar visibility reset - force re-apply of default visibility
//   4: Final toolbar fix - quickActions/edit/format/help visible, no addToolBarBreak
//   5: Final layout - Row 1: Quick Actions, Edit, Format, Insert; Row 2: Book, Styles, Tools, Help; Hidden: File, View
static constexpr int TOOLBAR_CONFIG_VERSION = 5;

ToolbarManager::ToolbarManager(QMainWindow* mainWindow)
    : m_mainWindow(mainWindow)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("ToolbarManager: Constructor called");

    initializeConfigs();

    // OpenSpec #00031: Initialize default configs for reset functionality
    for (const auto& [id, config] : m_configs) {
        QStringList commands;
        for (const auto& cmdId : config.commandIds) {
            commands << QString::fromStdString(cmdId);
        }
        m_defaultConfigs[QString::fromStdString(id)] = commands;
        m_toolbarCommands[QString::fromStdString(id)] = commands;
    }

    // Load user configurations (may override defaults)
    loadConfigurations();

    logger.debug("ToolbarManager: Initialized with {} toolbar configs", m_configs.size());
}

void ToolbarManager::initializeConfigs() {
    // File Toolbar
    m_configs["file"] = {
        "file",
        "File Toolbar",
        Qt::TopToolBarArea,
        false,  // hidden by default (accessible via Quick Actions)
        {"file.new", "file.new.project", "file.open", "file.save", "file.saveAs", "file.close"}
    };

    // Edit Toolbar
    m_configs["edit"] = {
        "edit",
        "Edit Toolbar",
        Qt::TopToolBarArea,
        true,  // visible by default (Row 1)
        {"edit.undo", "edit.redo", SEPARATOR_ID,
         "edit.cut", "edit.copy", "edit.paste", "edit.selectAll"}
    };

    // Book Toolbar
    m_configs["book"] = {
        "book",
        "Book Toolbar",
        Qt::TopToolBarArea,
        true,  // visible by default (Row 2)
        {"book.newChapter", "book.newCharacter", "book.newLocation", "book.properties"}
    };

    // View Toolbar (panel toggles)
    m_configs["view"] = {
        "view",
        "View Toolbar",
        Qt::TopToolBarArea,
        false,  // hidden by default (accessible via menu)
        {"view.dashboard", SEPARATOR_ID, "view.navigator", "view.properties", "view.search", "view.assistant", "view.log"}
    };

    // Tools Toolbar
    m_configs["tools"] = {
        "tools",
        "Tools Toolbar",
        Qt::TopToolBarArea,
        true,  // visible by default (Row 2)
        {"tools.spellcheck", "tools.stats.wordCount", "tools.focus.normal"}
    };

    // Format Toolbar (text formatting - essential for writer's IDE)
    // Special widget IDs: WIDGET_FONT_COMBO_ID, WIDGET_FONT_SIZE_ID
    m_configs["format"] = {
        "format",
        "Format Toolbar",
        Qt::TopToolBarArea,
        true,  // visible by default (Row 1)
        {WIDGET_FONT_COMBO_ID, WIDGET_FONT_SIZE_ID, SEPARATOR_ID,
         "format.bold", "format.italic", "format.underline", "format.strikethrough", SEPARATOR_ID,
         "format.alignLeft", "format.alignCenter", "format.alignRight", "format.justify", SEPARATOR_ID,
         "format.bullets", "format.numbering", SEPARATOR_ID,
         "format.clearFormatting"}
    };

    // Insert Toolbar (quick access to insert elements)
    m_configs["insert"] = {
        "insert",
        "Insert Toolbar",
        Qt::TopToolBarArea,
        true,  // visible by default (Row 1)
        {"insert.image", "insert.table", "insert.link", SEPARATOR_ID,
         "insert.footnote", "insert.comment"}
    };

    // Styles Toolbar (quick paragraph styles)
    m_configs["styles"] = {
        "styles",
        "Styles Toolbar",
        Qt::TopToolBarArea,
        true,  // visible by default (Row 2)
        {"format.style.heading1", "format.style.heading2", "format.style.heading3",
         "format.style.body", "format.style.quote", "format.style.code"}
    };

    // Quick Actions Toolbar (OpenSpec #00037 - Phase C)
    // Consolidated toolbar with most frequently used actions for rapid access
    // Note: Undo/Redo are in Edit toolbar, not duplicated here
    m_configs["quickActions"] = {
        "quickActions",
        "Quick Actions",
        Qt::TopToolBarArea,
        true,  // visible by default (Row 1)
        {"file.new", "file.new.project", "file.open", "file.save", "file.saveAll", SEPARATOR_ID,
         "edit.find", SEPARATOR_ID,
         "tools.backupNow", SEPARATOR_ID,
         "edit.settings", "tools.toolbarManager"}
    };

    // Help Toolbar (OpenSpec #00037 - Phase D)
    // Quick access to help resources, keyboard shortcuts, updates, and about info
    m_configs["help"] = {
        "help",
        "Help Toolbar",
        Qt::TopToolBarArea,
        true,  // visible by default (Row 2)
        {"help.manual", "help.shortcuts", "help.checkUpdates", "help.about"}
    };
}

void ToolbarManager::createToolbars(CommandRegistry& registry) {
    auto& logger = core::Logger::getInstance();
    logger.debug("ToolbarManager: Creating toolbars from configurations");

    // OpenSpec #00037 Phase E: Multi-row toolbar layout
    // Row 1: Quick Actions, Edit, Format, Insert (visible by default)
    // Row 2: Book, Styles, Tools, Help (visible by default)
    // Hidden: File, View

    // Row 1 toolbars: Quick Actions, Edit, Format, Insert
    std::vector<std::string> row1 = {"quickActions", "edit", "format", "insert"};
    for (const std::string& id : row1) {
        auto it = m_configs.find(id);
        if (it != m_configs.end()) {
            QToolBar* toolbar = createToolbar(it->second, registry);
            m_toolbars[id] = toolbar;
            logger.debug("ToolbarManager: Created toolbar '{}' (Row 1)", id);
        }
    }

    // Add toolbar break between rows
    m_mainWindow->addToolBarBreak(Qt::TopToolBarArea);

    // Row 2 toolbars: Book, Styles, Tools, Help
    std::vector<std::string> row2 = {"book", "styles", "tools", "help"};
    for (const std::string& id : row2) {
        auto it = m_configs.find(id);
        if (it != m_configs.end()) {
            QToolBar* toolbar = createToolbar(it->second, registry);
            m_toolbars[id] = toolbar;
            logger.debug("ToolbarManager: Created toolbar '{}' (Row 2)", id);
        }
    }

    // Hidden toolbars: File, View (create but don't show)
    std::vector<std::string> hidden = {"file", "view"};
    for (const std::string& id : hidden) {
        auto it = m_configs.find(id);
        if (it != m_configs.end()) {
            QToolBar* toolbar = createToolbar(it->second, registry);
            m_toolbars[id] = toolbar;
            logger.debug("ToolbarManager: Created toolbar '{}' (hidden)", id);
        }
    }

    logger.info("ToolbarManager: Created {} toolbars", m_toolbars.size());
}

QToolBar* ToolbarManager::createToolbar(const ToolbarConfig& config, CommandRegistry& registry) {
    auto& artProvider = core::ArtProvider::getInstance();
    auto& logger = core::Logger::getInstance();

    // Create toolbar
    QToolBar* toolbar = new QToolBar(QObject::tr(config.label.c_str()), m_mainWindow);
    toolbar->setObjectName(QString::fromStdString(config.id));  // For QSettings

    // OpenSpec #00031 FIX: Use m_toolbarCommands (loaded from settings) instead of
    // config.commandIds (defaults). This ensures user customizations persist across restarts.
    QString toolbarId = QString::fromStdString(config.id);
    const QStringList& commands = m_toolbarCommands.value(toolbarId);

    logger.debug("ToolbarManager: Creating toolbar '{}' with {} commands from m_toolbarCommands",
                 config.id, commands.size());

    // Add actions and widgets using shared helper method
    for (const QString& cmdId : commands) {
        addToolbarItem(toolbar, cmdId, registry);
    }

    // Configure toolbar appearance and behavior
    // OpenSpec #00031 - Phase E: Apply lock state when creating toolbar
    toolbar->setMovable(!m_toolbarsLocked);
    toolbar->setFloatable(true);
    // OpenSpec #00026: Icon size now managed by ArtProvider/IconSizeConfig
    int toolbarIconSize = artProvider.getIconSize(core::IconContext::Toolbar);
    toolbar->setIconSize(QSize(toolbarIconSize, toolbarIconSize));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);  // Icons only, no text

    // OpenSpec #00031 - Phase E: Set up context menu for right-click
    toolbar->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(toolbar, &QWidget::customContextMenuRequested,
        [this, toolbar](const QPoint& pos) {
            showContextMenu(toolbar->mapToGlobal(pos));
        });

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

    // OpenSpec #00037: Check if we have any saved toolbar visibility settings
    // If not (first run or after clearSavedWindowState), use defaults and apply layout
    bool hasSavedSettings = false;
    for (const auto& [id, config] : m_configs) {
        if (settings.contains(QString::fromStdString(id + "/visible"))) {
            hasSavedSettings = true;
            break;
        }
    }

    // Restore visibility for each toolbar
    for (const auto& [id, toolbar] : m_toolbars) {
        QString key = QString::fromStdString(id + "/visible");

        // Get default visibility from config
        bool defaultVisible = true;
        auto configIt = m_configs.find(id);
        if (configIt != m_configs.end()) {
            defaultVisible = configIt->second.defaultVisible;
        }

        // If no saved settings, use defaults directly
        bool visible = hasSavedSettings ? settings.value(key, defaultVisible).toBool() : defaultVisible;
        toolbar->setVisible(visible);

        // Update View menu action if exists
        auto actionIt = m_viewActions.find(id);
        if (actionIt != m_viewActions.end()) {
            actionIt->second->setChecked(visible);
        }

        logger.debug("ToolbarManager: Toolbar '{}' visibility set to {} (default: {}, hasSaved: {})",
                     id, visible, defaultVisible, hasSavedSettings);
    }

    settings.endGroup();

    // OpenSpec #00037: Apply default layout if no saved settings
    // This ensures proper 2-row layout on first run or after config reset
    if (!hasSavedSettings) {
        logger.info("ToolbarManager: No saved toolbar settings, applying default 2-row layout");
        applyDefaultLayout();
    }

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

    // OpenSpec #00030: Create Toolbars submenu instead of adding directly to VIEW menu
    // Structure: Standard Toolbars → separator → User-defined (future) → separator → Toolbar Manager...
    QMenu* toolbarsMenu = viewMenu->addMenu(QObject::tr("Toolbars"));

    // Create checkable action for each toolbar
    // OpenSpec #00037: Order matches layout - Row 1 visible, Row 2 visible, then hidden
    std::vector<std::string> order = {"quickActions", "edit", "format", "insert", "book", "styles", "tools", "help", "file", "view"};

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
        QAction* action = new QAction(actionText, toolbarsMenu);
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

        toolbarsMenu->addAction(action);
        m_viewActions[id] = action;

        logger.debug("ToolbarManager: Created View menu action for toolbar '{}'", id);
    }

    // Add separator and Toolbar Manager... action
    toolbarsMenu->addSeparator();
    QAction* managerAction = new QAction(QObject::tr("Toolbar Manager..."), toolbarsMenu);
    toolbarsMenu->addAction(managerAction);

    // OpenSpec #00031: Connect Toolbar Manager dialog
    QObject::connect(managerAction, &QAction::triggered, m_mainWindow, [this]() {
        dialogs::ToolbarManagerDialog dialog(this, m_mainWindow);
        if (dialog.exec() == QDialog::Accepted) {
            // Configurations already applied via dialog
            if (m_mainWindow->statusBar()) {
                m_mainWindow->statusBar()->showMessage(QObject::tr("Toolbar configuration saved"), 3000);
            }
        }
    });

    logger.info("ToolbarManager: Created {} View menu actions in Toolbars submenu", m_viewActions.size());
}

// OpenSpec #00026: refreshIcons() method REMOVED
// Icon refresh is now automatic via ArtProvider::createAction() which
// connects each action to ArtProvider::resourcesChanged() signal.
// When theme/colors/sizes change, ArtProvider emits resourcesChanged()
// and all managed actions update their icons automatically.

void ToolbarManager::updateIconSizes() {
    auto& logger = core::Logger::getInstance();
    auto& artProvider = core::ArtProvider::getInstance();

    int newSize = artProvider.getIconSize(core::IconContext::Toolbar);
    QSize iconSize(newSize, newSize);

    logger.debug("ToolbarManager: Updating icon sizes to {}x{}", newSize, newSize);

    for (auto& [id, toolbar] : m_toolbars) {
        if (toolbar) {
            toolbar->setIconSize(iconSize);
        }
    }

    logger.info("ToolbarManager: Updated {} toolbars to icon size {}x{}",
        m_toolbars.size(), newSize, newSize);
}

// ============================================================================
// Customization API (OpenSpec #00031)
// ============================================================================

QStringList ToolbarManager::getToolbarCommands(const QString& toolbarId) const {
    return m_toolbarCommands.value(toolbarId, QStringList());
}

void ToolbarManager::setToolbarCommands(const QString& toolbarId, const QStringList& commands) {
    auto& logger = core::Logger::getInstance();

    if (!m_toolbarCommands.contains(toolbarId) && !toolbarId.startsWith("user_")) {
        logger.warn("ToolbarManager: Cannot set commands for unknown toolbar '{}'",
                    toolbarId.toStdString());
        return;
    }

    m_toolbarCommands[toolbarId] = commands;
    rebuildToolbar(toolbarId);
    saveConfigurations();

    logger.debug("ToolbarManager: Updated commands for toolbar '{}'", toolbarId.toStdString());
}

QStringList ToolbarManager::getToolbarIds() const {
    return m_toolbarCommands.keys();
}

QString ToolbarManager::getToolbarName(const QString& toolbarId) const {
    // Check user toolbar names first
    if (m_userToolbarNames.contains(toolbarId)) {
        return m_userToolbarNames[toolbarId];
    }

    // Check built-in toolbars
    std::string id = toolbarId.toStdString();
    auto it = m_configs.find(id);
    if (it != m_configs.end()) {
        return QString::fromStdString(it->second.label);
    }

    return toolbarId;
}

bool ToolbarManager::isUserToolbar(const QString& toolbarId) const {
    return toolbarId.startsWith("user_");
}

QString ToolbarManager::createUserToolbar(const QString& name, const QStringList& commands) {
    auto& logger = core::Logger::getInstance();

    // Generate unique ID
    QString baseId = "user_" + name.toLower().replace(QRegularExpression("[^a-z0-9]"), "_");
    QString toolbarId = baseId;
    int counter = 1;
    while (m_toolbarCommands.contains(toolbarId)) {
        toolbarId = baseId + "_" + QString::number(counter++);
    }

    // Store configuration
    m_userToolbarNames[toolbarId] = name;
    m_toolbarCommands[toolbarId] = commands;

    // Create the actual QToolBar
    QToolBar* toolbar = new QToolBar(name, m_mainWindow);
    toolbar->setObjectName(toolbarId);
    toolbar->setMovable(true);
    toolbar->setFloatable(true);

    auto& artProvider = core::ArtProvider::getInstance();
    int toolbarIconSize = artProvider.getIconSize(core::IconContext::Toolbar);
    toolbar->setIconSize(QSize(toolbarIconSize, toolbarIconSize));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_mainWindow->addToolBar(Qt::TopToolBarArea, toolbar);
    m_toolbars[toolbarId.toStdString()] = toolbar;

    // Build toolbar content
    rebuildToolbar(toolbarId);
    saveConfigurations();

    logger.info("ToolbarManager: Created user toolbar '{}' with ID '{}'",
                name.toStdString(), toolbarId.toStdString());

    return toolbarId;
}

bool ToolbarManager::deleteUserToolbar(const QString& toolbarId) {
    auto& logger = core::Logger::getInstance();

    if (!isUserToolbar(toolbarId)) {
        logger.warn("ToolbarManager: Cannot delete non-user toolbar '{}'", toolbarId.toStdString());
        return false;
    }

    if (!m_toolbarCommands.contains(toolbarId)) {
        logger.warn("ToolbarManager: User toolbar '{}' not found", toolbarId.toStdString());
        return false;
    }

    // Remove from configurations
    m_toolbarCommands.remove(toolbarId);
    m_userToolbarNames.remove(toolbarId);

    // Remove QToolBar from MainWindow
    std::string id = toolbarId.toStdString();
    auto it = m_toolbars.find(id);
    if (it != m_toolbars.end()) {
        QToolBar* toolbar = it->second;
        m_mainWindow->removeToolBar(toolbar);
        toolbar->deleteLater();
        m_toolbars.erase(it);
    }

    saveConfigurations();

    logger.info("ToolbarManager: Deleted user toolbar '{}'", toolbarId.toStdString());
    return true;
}

bool ToolbarManager::renameUserToolbar(const QString& toolbarId, const QString& newName) {
    auto& logger = core::Logger::getInstance();

    if (!isUserToolbar(toolbarId)) {
        logger.warn("ToolbarManager: Cannot rename non-user toolbar '{}'", toolbarId.toStdString());
        return false;
    }

    if (!m_userToolbarNames.contains(toolbarId)) {
        logger.warn("ToolbarManager: User toolbar '{}' not found", toolbarId.toStdString());
        return false;
    }

    m_userToolbarNames[toolbarId] = newName;

    // Update QToolBar title
    std::string id = toolbarId.toStdString();
    auto it = m_toolbars.find(id);
    if (it != m_toolbars.end()) {
        it->second->setWindowTitle(newName);
    }

    saveConfigurations();

    logger.debug("ToolbarManager: Renamed toolbar '{}' to '{}'",
                 toolbarId.toStdString(), newName.toStdString());
    return true;
}

void ToolbarManager::rebuildToolbar(const QString& toolbarId) {
    auto& logger = core::Logger::getInstance();
    std::string id = toolbarId.toStdString();

    auto toolbarIt = m_toolbars.find(id);
    if (toolbarIt == m_toolbars.end()) {
        logger.warn("ToolbarManager: Cannot rebuild unknown toolbar '{}'", id);
        return;
    }

    QToolBar* toolbar = toolbarIt->second;
    toolbar->clear();

    const QStringList& commands = m_toolbarCommands[toolbarId];
    auto& registry = CommandRegistry::getInstance();

    // Add actions and widgets using shared helper method
    for (const QString& cmdId : commands) {
        addToolbarItem(toolbar, cmdId, registry);
    }

    logger.debug("ToolbarManager: Rebuilt toolbar '{}' with {} commands", id, commands.size());
}

void ToolbarManager::addToolbarItem(QToolBar* toolbar, const QString& cmdId, CommandRegistry& registry) {
    auto& logger = core::Logger::getInstance();
    auto& artProvider = core::ArtProvider::getInstance();

    if (cmdId == SEPARATOR_ID) {
        toolbar->addSeparator();
    } else if (cmdId == WIDGET_FONT_COMBO_ID) {
        // Font family dropdown
        QFontComboBox* fontCombo = new QFontComboBox(toolbar);
        fontCombo->setObjectName("formatFontCombo");
        fontCombo->setToolTip(QObject::tr("Font Family"));
        fontCombo->setMaximumWidth(150);
        fontCombo->setMinimumWidth(100);
        // TODO: Connect to editor when available
        toolbar->addWidget(fontCombo);
        m_fontComboBox = fontCombo;
    } else if (cmdId == WIDGET_FONT_SIZE_ID) {
        // Font size spinner
        QSpinBox* sizeSpinner = new QSpinBox(toolbar);
        sizeSpinner->setObjectName("formatFontSize");
        sizeSpinner->setToolTip(QObject::tr("Font Size"));
        sizeSpinner->setRange(6, 72);
        sizeSpinner->setValue(12);
        sizeSpinner->setSuffix(" pt");
        sizeSpinner->setMinimumWidth(60);
        sizeSpinner->setMaximumWidth(80);
        // TODO: Connect to editor when available
        toolbar->addWidget(sizeSpinner);
        m_fontSizeSpinner = sizeSpinner;
    } else {
        std::string cmdIdStd = cmdId.toStdString();
        Command* cmd = registry.getCommand(cmdIdStd);
        if (cmd && cmd->canExecute()) {
            // OpenSpec #00026: Use ArtProvider::createAction() for self-updating icons
            // This creates an action that automatically refreshes on theme/color changes
            QAction* action = artProvider.createAction(
                cmdId,
                QString::fromStdString(cmd->label),
                toolbar,
                core::IconContext::Toolbar
            );

            // Set tooltip and shortcut from command
            if (!cmd->tooltip.empty()) {
                action->setToolTip(QString::fromStdString(cmd->tooltip));
                action->setStatusTip(QString::fromStdString(cmd->tooltip));
            }
            if (!cmd->shortcut.isEmpty()) {
                action->setShortcut(cmd->shortcut.toQKeySequence());
            }

            // Set checkable state if command has isChecked callback
            if (cmd->isChecked) {
                action->setCheckable(true);
                action->setChecked(cmd->isChecked());
            }

            toolbar->addAction(action);

            // Connect action to command execution
            QObject::connect(action, &QAction::triggered, [cmdIdStd, &registry]() {
                registry.executeCommand(cmdIdStd);
            });
        } else {
            logger.warn("ToolbarManager: Command '{}' not found or not executable", cmdIdStd);
        }
    }
}

void ToolbarManager::resetToDefaults() {
    auto& logger = core::Logger::getInstance();
    logger.info("ToolbarManager: Resetting all toolbars to defaults");

    // Delete all user toolbars
    QStringList userToolbars;
    for (const QString& id : m_toolbarCommands.keys()) {
        if (isUserToolbar(id)) {
            userToolbars << id;
        }
    }
    for (const QString& id : userToolbars) {
        deleteUserToolbar(id);
    }

    // Reset built-in toolbars to defaults
    for (auto it = m_defaultConfigs.constBegin(); it != m_defaultConfigs.constEnd(); ++it) {
        m_toolbarCommands[it.key()] = it.value();
        rebuildToolbar(it.key());
    }

    saveConfigurations();

    // OpenSpec #00037: Apply default 2-row layout after reset
    applyDefaultLayout();

    logger.info("ToolbarManager: Reset complete");
}

void ToolbarManager::applyDefaultLayout() {
    auto& logger = core::Logger::getInstance();
    logger.debug("ToolbarManager: Applying default 2-row layout");

    // OpenSpec #00037: Rearrange toolbars into 2 rows
    // Row 1: Quick Actions, Edit, Format, Insert (visible by default)
    // Row 2: Book, Styles, Tools, Help (visible by default)
    // Hidden by default: File, View

    // First, remove all toolbars from main window (but don't delete them)
    // NOTE: removeToolBar() also HIDES the toolbar, so we must restore visibility after adding
    for (auto& [id, toolbar] : m_toolbars) {
        if (toolbar) {
            m_mainWindow->removeToolBar(toolbar);
        }
    }

    // Remove any existing toolbar breaks
    // Note: Qt doesn't have a direct way to remove breaks, but removing and re-adding
    // toolbars effectively clears the layout

    // Row 1 toolbars: Quick Actions, Edit, Format, Insert
    std::vector<std::string> row1 = {"quickActions", "edit", "format", "insert"};
    for (const std::string& id : row1) {
        auto it = m_toolbars.find(id);
        if (it != m_toolbars.end() && it->second) {
            m_mainWindow->addToolBar(Qt::TopToolBarArea, it->second);
        }
    }

    // Add toolbar break between Row 1 and Row 2
    m_mainWindow->addToolBarBreak(Qt::TopToolBarArea);

    // Row 2 toolbars: Book, Styles, Tools, Help
    std::vector<std::string> row2 = {"book", "styles", "tools", "help"};
    for (const std::string& id : row2) {
        auto it = m_toolbars.find(id);
        if (it != m_toolbars.end() && it->second) {
            m_mainWindow->addToolBar(Qt::TopToolBarArea, it->second);
        }
    }

    // Hidden toolbars: File, View
    std::vector<std::string> hidden = {"file", "view"};
    for (const std::string& id : hidden) {
        auto it = m_toolbars.find(id);
        if (it != m_toolbars.end() && it->second) {
            m_mainWindow->addToolBar(Qt::TopToolBarArea, it->second);
        }
    }

    // CRITICAL: Restore visibility after addToolBar() because removeToolBar() hides toolbars
    // Use defaultVisible from m_configs to determine which toolbars should be visible
    for (auto& [id, toolbar] : m_toolbars) {
        if (toolbar) {
            bool defaultVisible = false;
            auto configIt = m_configs.find(id);
            if (configIt != m_configs.end()) {
                defaultVisible = configIt->second.defaultVisible;
            }
            toolbar->setVisible(defaultVisible);
            logger.debug("ToolbarManager: Toolbar '{}' visibility set to {} (default)", id, defaultVisible);
        }
    }

    logger.info("ToolbarManager: Applied default 2-row toolbar layout");
}

void ToolbarManager::loadConfigurations() {
    auto& logger = core::Logger::getInstance();
    auto& settings = core::SettingsManager::getInstance();

    logger.debug("ToolbarManager: Loading configurations from settings");

    // Load built-in toolbar customizations
    std::string toolbarConfigJson = settings.get<std::string>("toolbars.configurations", "{}");
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(toolbarConfigJson));

    if (doc.isObject()) {
        QJsonObject root = doc.object();

        // Load built-in toolbar customizations
        if (root.contains("builtIn") && root["builtIn"].isObject()) {
            QJsonObject builtIn = root["builtIn"].toObject();
            for (auto it = builtIn.begin(); it != builtIn.end(); ++it) {
                if (it.value().isArray()) {
                    QStringList commands;
                    for (const QJsonValue& v : it.value().toArray()) {
                        commands << v.toString();
                    }
                    m_toolbarCommands[it.key()] = commands;
                }
            }
        }

        // Load user toolbars
        if (root.contains("userToolbars") && root["userToolbars"].isObject()) {
            QJsonObject userToolbars = root["userToolbars"].toObject();
            for (auto it = userToolbars.begin(); it != userToolbars.end(); ++it) {
                if (it.value().isObject()) {
                    QJsonObject toolbarObj = it.value().toObject();
                    QString name = toolbarObj["name"].toString();
                    QStringList commands;
                    for (const QJsonValue& v : toolbarObj["commands"].toArray()) {
                        commands << v.toString();
                    }
                    m_userToolbarNames[it.key()] = name;
                    m_toolbarCommands[it.key()] = commands;
                }
            }
        }
    }

    // OpenSpec #00031 - Phase E: Load toolbar lock state
    m_toolbarsLocked = settings.get<bool>("toolbars.locked", false);
    logger.debug("ToolbarManager: Toolbars locked state: {}", m_toolbarsLocked);

    logger.debug("ToolbarManager: Loaded {} toolbar configurations", m_toolbarCommands.size());
}

void ToolbarManager::saveConfigurations() {
    auto& logger = core::Logger::getInstance();
    auto& settings = core::SettingsManager::getInstance();

    logger.debug("ToolbarManager: Saving configurations to settings");

    QJsonObject root;

    // Save built-in toolbar customizations (only if different from defaults)
    QJsonObject builtIn;
    for (auto it = m_defaultConfigs.constBegin(); it != m_defaultConfigs.constEnd(); ++it) {
        const QString& id = it.key();
        if (m_toolbarCommands.contains(id) && m_toolbarCommands[id] != it.value()) {
            QJsonArray commands;
            for (const QString& cmd : m_toolbarCommands[id]) {
                commands.append(cmd);
            }
            builtIn[id] = commands;
        }
    }
    if (!builtIn.isEmpty()) {
        root["builtIn"] = builtIn;
    }

    // Save user toolbars
    QJsonObject userToolbars;
    for (auto it = m_userToolbarNames.constBegin(); it != m_userToolbarNames.constEnd(); ++it) {
        QJsonObject toolbarObj;
        toolbarObj["name"] = it.value();
        QJsonArray commands;
        for (const QString& cmd : m_toolbarCommands[it.key()]) {
            commands.append(cmd);
        }
        toolbarObj["commands"] = commands;
        userToolbars[it.key()] = toolbarObj;
    }
    if (!userToolbars.isEmpty()) {
        root["userToolbars"] = userToolbars;
    }

    QJsonDocument doc(root);
    std::string jsonStr = doc.toJson(QJsonDocument::Compact).toStdString();
    settings.set<std::string>("toolbars.configurations", jsonStr);
    settings.save();

    logger.debug("ToolbarManager: Saved toolbar configurations");
}

// ============================================================================
// Context Menu & Locking API (OpenSpec #00031 - Phase E)
// ============================================================================

void ToolbarManager::showContextMenu(const QPoint& globalPos) {
    QMenu menu;

    // Add toolbar visibility toggles
    for (const QString& id : getToolbarIds()) {
        std::string idStd = id.toStdString();
        auto it = m_toolbars.find(idStd);
        if (it == m_toolbars.end()) continue;
        QToolBar* toolbar = it->second;
        if (!toolbar) continue;

        QAction* action = menu.addAction(getToolbarName(id));
        action->setCheckable(true);
        action->setChecked(toolbar->isVisible());
        QObject::connect(action, &QAction::toggled, [toolbar](bool checked) {
            toolbar->setVisible(checked);
        });
    }

    menu.addSeparator();

    // Lock toggle
    QAction* lockAction = menu.addAction(QObject::tr("Lock Toolbar Positions"));
    lockAction->setCheckable(true);
    lockAction->setChecked(m_toolbarsLocked);
    QObject::connect(lockAction, &QAction::toggled, [this](bool checked) {
        setToolbarsLocked(checked);
    });

    menu.addSeparator();

    // Customize
    QAction* customizeAction = menu.addAction(QObject::tr("Customize..."));
    QObject::connect(customizeAction, &QAction::triggered, [this]() {
        dialogs::ToolbarManagerDialog dialog(this, m_mainWindow);
        dialog.exec();
    });

    // Reset
    QAction* resetAction = menu.addAction(QObject::tr("Reset to Default"));
    QObject::connect(resetAction, &QAction::triggered, [this]() {
        // Confirm before reset
        QMessageBox::StandardButton result = QMessageBox::question(
            m_mainWindow,
            QObject::tr("Reset Toolbars"),
            QObject::tr("Reset all toolbars to default configuration?"),
            QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::Yes) {
            resetToDefaults();
        }
    });

    menu.exec(globalPos);
}

void ToolbarManager::setToolbarsLocked(bool locked) {
    auto& logger = core::Logger::getInstance();
    auto& settings = core::SettingsManager::getInstance();

    m_toolbarsLocked = locked;

    for (auto& [id, toolbar] : m_toolbars) {
        if (toolbar) {
            toolbar->setMovable(!locked);
        }
    }

    // Persist setting
    settings.set<bool>("toolbars.locked", locked);
    settings.save();

    logger.info("ToolbarManager: Toolbars locked state set to {}", locked);
}

bool ToolbarManager::isToolbarsLocked() const {
    return m_toolbarsLocked;
}

void ToolbarManager::openToolbarManagerDialog() {
    dialogs::ToolbarManagerDialog dialog(this, m_mainWindow);
    if (dialog.exec() == QDialog::Accepted) {
        // Configurations already applied via dialog
        if (m_mainWindow->statusBar()) {
            m_mainWindow->statusBar()->showMessage(QObject::tr("Toolbar configuration saved"), 3000);
        }
    }
}

bool ToolbarManager::needsConfigReset() {
    QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
    settings.beginGroup("Toolbars");
    int savedVersion = settings.value("configVersion", 0).toInt();
    settings.endGroup();

    return savedVersion < TOOLBAR_CONFIG_VERSION;
}

void ToolbarManager::clearSavedWindowState() {
    auto& logger = core::Logger::getInstance();
    logger.info("ToolbarManager: Clearing saved window state for toolbar reset");

    QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");

    // Clear windowState which contains toolbar positions/visibility
    // This forces QMainWindow to use default toolbar layout
    settings.remove("windowState");

    // Also clear individual toolbar visibility settings
    settings.beginGroup("Toolbars");
    settings.remove("");  // Remove all keys in group
    settings.setValue("configVersion", TOOLBAR_CONFIG_VERSION);
    settings.endGroup();

    settings.sync();

    logger.info("ToolbarManager: Window state cleared, will use default toolbar layout");
}

} // namespace gui
} // namespace kalahari
