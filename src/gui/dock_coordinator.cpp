/// @file dock_coordinator.cpp
/// @brief Dock widget and panel coordination implementation
///
/// OpenSpec #00038 - Phase 4: Extract Dock/Panel Management from MainWindow

#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/gui/panels/search_panel.h"
#include "kalahari/gui/panels/assistant_panel.h"
#include "kalahari/gui/widgets/standalone_info_bar.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/log_panel_sink.h"
#include "kalahari/core/art_provider.h"
#include <QMainWindow>
#include <QDockWidget>
#include <QTabWidget>
#include <QLabel>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QAction>

namespace kalahari {
namespace gui {

DockCoordinator::DockCoordinator(QMainWindow* mainWindow, MenuBuilder* menuBuilder, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_menuBuilder(menuBuilder)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("DockCoordinator created");
}

void DockCoordinator::createDocks() {
    auto& logger = core::Logger::getInstance();
    logger.debug("DockCoordinator: Creating dock widgets");

    // Create central tabbed workspace first
    createCentralWidget();

    // Create dock widgets
    createNavigatorDock();
    createPropertiesDock();
    createLogDock();
    createSearchDock();
    createAssistantDock();

    // Tab right-side docks
    m_mainWindow->tabifyDockWidget(m_propertiesDock, m_searchDock);
    m_mainWindow->tabifyDockWidget(m_searchDock, m_assistantDock);

    // Raise Properties tab (default visible)
    m_propertiesDock->raise();

    // Setup VIEW menu panel actions
    setupViewMenuActions();

    logger.debug("DockCoordinator: Dock widgets created successfully");
}

void DockCoordinator::createCentralWidget() {
    auto& logger = core::Logger::getInstance();
    logger.debug("DockCoordinator: Creating central widget");

    // Create wrapper widget for info bar + tabs
    m_centralWrapper = new QWidget(m_mainWindow);
    QVBoxLayout* centralLayout = new QVBoxLayout(m_centralWrapper);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    // Create central tabbed workspace
    m_centralTabs = new QTabWidget(m_centralWrapper);
    m_centralTabs->setTabsClosable(true);
    m_centralTabs->setMovable(true);
    m_centralTabs->setDocumentMode(true);
    centralLayout->addWidget(m_centralTabs, 1);  // Stretch factor 1 to fill space

    // Create standalone info bar at BOTTOM (hidden by default)
    m_standaloneInfoBar = new StandaloneInfoBar(m_centralWrapper);
    m_standaloneInfoBar->hide();
    centralLayout->addWidget(m_standaloneInfoBar);

    // Connect info bar signals
    connect(m_standaloneInfoBar, &StandaloneInfoBar::addToProjectClicked,
            this, &DockCoordinator::addToProjectRequested);
    connect(m_standaloneInfoBar, &StandaloneInfoBar::dismissed,
            this, &DockCoordinator::infoBarDismissed);
    connect(m_standaloneInfoBar, &StandaloneInfoBar::dismissed,
            m_standaloneInfoBar, &QWidget::hide);

    m_mainWindow->setCentralWidget(m_centralWrapper);

    // Add Dashboard as first tab (default at startup, closable)
    m_dashboardPanel = new DashboardPanel(m_mainWindow);
    auto& artProvider = core::ArtProvider::getInstance();
    QIcon dashboardIcon = artProvider.getIcon("view.dashboard");
    int dashboardIndex = m_centralTabs->addTab(m_dashboardPanel, dashboardIcon, QObject::tr("Dashboard"));
    m_centralTabs->setCurrentIndex(dashboardIndex);

    // Connect Dashboard recent book signal
    connect(m_dashboardPanel, &DashboardPanel::openRecentBookRequested,
            this, &DockCoordinator::openRecentBookRequested);

    // Connect tab close signal
    connect(m_centralTabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        auto& logger = core::Logger::getInstance();
        QWidget* widget = m_centralTabs->widget(index);

        // Check if this is the Dashboard panel being closed
        // Note: m_dashboardPanel is QPointer - auto-nulls when widget is deleted
        if (widget == m_dashboardPanel) {
            emit dashboardClosed();
            logger.debug("DockCoordinator: Dashboard panel closed (QPointer will auto-null)");
        }

        // Emit signal for MainWindow to handle (unsaved changes check, etc.)
        emit tabCloseRequested(index);

        logger.debug("DockCoordinator: Tab close requested at index {}", index);
    });

    // Connect current tab changed signal
    connect(m_centralTabs, &QTabWidget::currentChanged,
            this, &DockCoordinator::currentTabChanged);

    logger.debug("DockCoordinator: Central widget created");
}

void DockCoordinator::createNavigatorDock() {
    auto& logger = core::Logger::getInstance();

    m_navigatorPanel = new NavigatorPanel(m_mainWindow);
    m_navigatorDock = new QDockWidget(QObject::tr("Navigator"), m_mainWindow);
    m_navigatorDock->setWidget(m_navigatorPanel);
    m_navigatorDock->setObjectName("NavigatorDock");  // Required for saveState!
    setupDockTitleBar(m_navigatorDock, "view.navigator", QObject::tr("Navigator"));
    m_mainWindow->addDockWidget(Qt::LeftDockWidgetArea, m_navigatorDock);

    // Connect Navigator element selection signal
    connect(m_navigatorPanel, &NavigatorPanel::elementSelected,
            this, &DockCoordinator::navigatorElementSelected);

    // Connect Navigator drag & drop reorder signals
    connect(m_navigatorPanel, &NavigatorPanel::chapterReordered,
            this, &DockCoordinator::chapterReordered);
    connect(m_navigatorPanel, &NavigatorPanel::partReordered,
            this, &DockCoordinator::partReordered);

    // Connect Navigator context menu signals
    connect(m_navigatorPanel, &NavigatorPanel::requestRename,
            this, &DockCoordinator::navigatorRequestRename);
    connect(m_navigatorPanel, &NavigatorPanel::requestDelete,
            this, &DockCoordinator::navigatorRequestDelete);
    connect(m_navigatorPanel, &NavigatorPanel::requestMoveElement,
            this, &DockCoordinator::navigatorRequestMove);
    connect(m_navigatorPanel, &NavigatorPanel::requestProperties,
            this, &DockCoordinator::navigatorRequestProperties);
    connect(m_navigatorPanel, &NavigatorPanel::requestSectionProperties,
            this, &DockCoordinator::navigatorRequestSectionProperties);
    connect(m_navigatorPanel, &NavigatorPanel::requestPartProperties,
            this, &DockCoordinator::navigatorRequestPartProperties);

    // Connect Navigator add item signals (OpenSpec #00042 Task 7.19 Issue #1)
    connect(m_navigatorPanel, &NavigatorPanel::requestAddChapter,
            this, &DockCoordinator::requestAddChapter);
    connect(m_navigatorPanel, &NavigatorPanel::requestAddPart,
            this, &DockCoordinator::requestAddPart);
    connect(m_navigatorPanel, &NavigatorPanel::requestAddItem,
            this, &DockCoordinator::requestAddItem);

    logger.debug("DockCoordinator: Navigator dock created");
}

void DockCoordinator::createPropertiesDock() {
    auto& logger = core::Logger::getInstance();

    m_propertiesPanel = new PropertiesPanel(m_mainWindow);
    m_propertiesDock = new QDockWidget(QObject::tr("Properties"), m_mainWindow);
    m_propertiesDock->setWidget(m_propertiesPanel);
    m_propertiesDock->setObjectName("PropertiesDock");
    setupDockTitleBar(m_propertiesDock, "view.properties", QObject::tr("Properties"));
    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // Connect PropertiesPanel status change to coordinator
    connect(m_propertiesPanel, &PropertiesPanel::chapterStatusChanged,
            this, &DockCoordinator::chapterStatusChanged);

    logger.debug("DockCoordinator: Properties dock created");
}

void DockCoordinator::createLogDock() {
    auto& logger = core::Logger::getInstance();

    // At startup, diagnostic mode is always false
    m_logPanel = new LogPanel(m_mainWindow, false);
    m_logDock = new QDockWidget(QObject::tr("Log"), m_mainWindow);
    m_logDock->setWidget(m_logPanel);
    m_logDock->setObjectName("LogDock");
    setupDockTitleBar(m_logDock, "view.log", QObject::tr("Log"));
    m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

    // Register LogPanel's sink with spdlog logger
    core::Logger::getInstance().addSink(m_logPanel->getSink());

    // Connect LogPanel's Options button to open Settings Dialog
    connect(m_logPanel, &LogPanel::openSettingsRequested,
            this, &DockCoordinator::openSettingsRequested);

    // Hide log panel by default in normal mode
    m_logDock->hide();

    logger.debug("DockCoordinator: Log dock created");
}

void DockCoordinator::createSearchDock() {
    auto& logger = core::Logger::getInstance();

    m_searchPanel = new SearchPanel(m_mainWindow);
    m_searchDock = new QDockWidget(QObject::tr("Search"), m_mainWindow);
    m_searchDock->setWidget(m_searchPanel);
    m_searchDock->setObjectName("SearchDock");
    setupDockTitleBar(m_searchDock, "view.search", QObject::tr("Search"));
    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_searchDock);

    logger.debug("DockCoordinator: Search dock created");
}

void DockCoordinator::createAssistantDock() {
    auto& logger = core::Logger::getInstance();

    m_assistantPanel = new AssistantPanel(m_mainWindow);
    m_assistantDock = new QDockWidget(QObject::tr("Assistant"), m_mainWindow);
    m_assistantDock->setWidget(m_assistantPanel);
    m_assistantDock->setObjectName("AssistantDock");
    setupDockTitleBar(m_assistantDock, "view.assistant", QObject::tr("Assistant"));
    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_assistantDock);

    logger.debug("DockCoordinator: Assistant dock created");
}

void DockCoordinator::setupDockTitleBar(QDockWidget* dock, const QString& iconId, const QString& title) {
    auto& artProvider = core::ArtProvider::getInstance();

    // Create custom title bar widget
    QWidget* titleBar = new QWidget(dock);
    QHBoxLayout* layout = new QHBoxLayout(titleBar);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(4);

    // Icon label (16x16 for title bar)
    QLabel* iconLabel = new QLabel(titleBar);
    QIcon icon = artProvider.getIcon(iconId);
    iconLabel->setPixmap(icon.pixmap(16, 16));
    iconLabel->setFixedSize(16, 16);
    iconLabel->setProperty("iconId", iconId);  // Store icon ID for theme refresh
    layout->addWidget(iconLabel);

    // Store reference for theme refresh
    m_dockIconLabels.append(iconLabel);

    // Title label
    QLabel* titleLabel = new QLabel(title, titleBar);
    titleLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(titleLabel);

    // Stretch to push buttons to the right
    layout->addStretch();

    // Float button (if dock is floatable)
    QDockWidget::DockWidgetFeatures features = dock->features();
    if (features & QDockWidget::DockWidgetFloatable) {
        QToolButton* floatButton = new QToolButton(titleBar);
        floatButton->setAutoRaise(true);
        floatButton->setIcon(artProvider.getIcon("dock.float"));
        floatButton->setToolTip(QObject::tr("Float"));
        floatButton->setFixedSize(16, 16);
        floatButton->setProperty("iconId", "dock.float");  // Store for theme refresh
        QObject::connect(floatButton, &QToolButton::clicked, dock, [dock]() {
            dock->setFloating(!dock->isFloating());
        });
        layout->addWidget(floatButton);
        m_dockToolButtons.append(floatButton);
    }

    // Close button (if dock is closable)
    if (features & QDockWidget::DockWidgetClosable) {
        QToolButton* closeButton = new QToolButton(titleBar);
        closeButton->setAutoRaise(true);
        closeButton->setIcon(artProvider.getIcon("dock.close"));
        closeButton->setToolTip(QObject::tr("Close"));
        closeButton->setFixedSize(16, 16);
        closeButton->setProperty("iconId", "dock.close");  // Store for theme refresh
        QObject::connect(closeButton, &QToolButton::clicked, dock, &QDockWidget::close);
        layout->addWidget(closeButton);
        m_dockToolButtons.append(closeButton);
    }

    dock->setTitleBarWidget(titleBar);
}

void DockCoordinator::refreshDockIcons() {
    auto& artProvider = core::ArtProvider::getInstance();
    auto& logger = core::Logger::getInstance();

    int refreshedCount = 0;

    // Refresh icon labels (panel icons)
    for (QLabel* label : m_dockIconLabels) {
        if (label) {
            QString iconId = label->property("iconId").toString();
            if (!iconId.isEmpty()) {
                label->setPixmap(artProvider.getIcon(iconId).pixmap(16, 16));
                ++refreshedCount;
            }
        }
    }

    // Refresh tool buttons (float/close buttons)
    for (QToolButton* button : m_dockToolButtons) {
        if (button) {
            QString iconId = button->property("iconId").toString();
            if (!iconId.isEmpty()) {
                button->setIcon(artProvider.getIcon(iconId));
                ++refreshedCount;
            }
        }
    }

    logger.debug("DockCoordinator: Refreshed {} dock title bar icons", refreshedCount);
}

void DockCoordinator::setupViewMenuActions() {
    auto& logger = core::Logger::getInstance();

    // Get VIEW menu from MenuBuilder
    if (m_menuBuilder) {
        m_viewMenu = m_menuBuilder->getMenu("VIEW");
        if (m_viewMenu) {
            logger.debug("DockCoordinator: Found VIEW menu via MenuBuilder::getMenu()");
        }
    }

    if (!m_viewMenu) {
        logger.warn("DockCoordinator: VIEW menu not found in MenuBuilder! Creating fallback menu.");
        m_viewMenu = m_mainWindow->menuBar()->addMenu(QObject::tr("&View"));
    }

    // Connect panel toggle commands to dock widgets
    connectPanelCommand("view.navigator", m_navigatorDock);
    connectPanelCommand("view.properties", m_propertiesDock);
    connectPanelCommand("view.log", m_logDock);
    connectPanelCommand("view.search", m_searchDock);
    connectPanelCommand("view.assistant", m_assistantDock);

    // Create Panels submenu with actions from CommandRegistry
    QMenu* panelsSubmenu = m_viewMenu->addMenu(QObject::tr("Panels"));
    logger.debug("DockCoordinator: Created VIEW/Panels submenu for dock toggles");

    // Create panel toggle actions
    m_viewNavigatorAction = createPanelAction("view.navigator", m_navigatorDock, panelsSubmenu);
    m_viewPropertiesAction = createPanelAction("view.properties", m_propertiesDock, panelsSubmenu);
    m_viewLogAction = createPanelAction("view.log", m_logDock, panelsSubmenu);
    m_viewSearchAction = createPanelAction("view.search", m_searchDock, panelsSubmenu);
    m_viewAssistantAction = createPanelAction("view.assistant", m_assistantDock, panelsSubmenu);
}

void DockCoordinator::connectPanelCommand(const std::string& cmdId, QDockWidget* dock) {
    auto& registry = CommandRegistry::getInstance();
    Command* cmd = registry.getCommand(cmdId);
    if (cmd) {
        // Set execute callback to toggle dock visibility
        cmd->execute = [dock]() {
            dock->setVisible(!dock->isVisible());
        };
        // Set isChecked callback for checkable state
        cmd->isChecked = [dock]() {
            return dock->isVisible();
        };
    }
}

QAction* DockCoordinator::createPanelAction(const std::string& cmdId, QDockWidget* dock, QMenu* menu) {
    auto& logger = core::Logger::getInstance();
    auto& registry = CommandRegistry::getInstance();

    // Get action from CommandRegistry (single source of truth)
    // Action is already configured with icon, shortcut, checkable state from command
    QAction* action = registry.getAction(cmdId);
    if (!action) {
        logger.warn("DockCoordinator: Action not found for command: {}", cmdId);
        return nullptr;
    }

    // Set checkable state (Command's isChecked callback is already set in connectPanelCommand)
    action->setCheckable(true);
    action->setChecked(dock->isVisible());

    // Two-way binding: dock -> action (sync visual state)
    // Note: action->triggered is already connected to executeCommand in CommandRegistry,
    // which calls cmd->execute (set in connectPanelCommand) to toggle dock visibility
    QObject::connect(dock, &QDockWidget::visibilityChanged, [action](bool visible) {
        action->blockSignals(true);
        action->setChecked(visible);
        action->blockSignals(false);
    });

    menu->addAction(action);
    return action;
}

void DockCoordinator::resetLayout(bool diagnosticMode, bool devMode) {
    auto& logger = core::Logger::getInstance();
    logger.info("DockCoordinator: Resetting dock layout to default");

    // Remove all docks
    m_mainWindow->removeDockWidget(m_navigatorDock);
    m_mainWindow->removeDockWidget(m_propertiesDock);
    m_mainWindow->removeDockWidget(m_logDock);
    m_mainWindow->removeDockWidget(m_searchDock);
    m_mainWindow->removeDockWidget(m_assistantDock);

    // Re-add in default layout
    m_mainWindow->addDockWidget(Qt::LeftDockWidgetArea, m_navigatorDock);
    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_searchDock);
    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_assistantDock);

    // Tab right-side docks
    m_mainWindow->tabifyDockWidget(m_propertiesDock, m_searchDock);
    m_mainWindow->tabifyDockWidget(m_searchDock, m_assistantDock);

    // Raise Properties tab
    m_propertiesDock->raise();

    // Show docks (LogDock only in diagnostic/dev mode)
    m_navigatorDock->show();
    m_propertiesDock->show();
    m_logDock->setVisible(diagnosticMode || devMode);
    m_searchDock->show();
    m_assistantDock->show();

    // Set default column proportions: 20% | 60% | 20%
    int totalWidth = m_mainWindow->width();
    int sideWidth = totalWidth * 20 / 100;  // 20% for each side

    m_mainWindow->resizeDocks({m_navigatorDock}, {sideWidth}, Qt::Horizontal);
    m_mainWindow->resizeDocks({m_propertiesDock}, {sideWidth}, Qt::Horizontal);

    logger.info("DockCoordinator: Layout reset: side columns {}px each (diag={}, dev={})",
                sideWidth, diagnosticMode, devMode);
}

} // namespace gui
} // namespace kalahari
