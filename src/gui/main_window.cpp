/// @file main_window.cpp
/// @brief Main application window implementation

#include "kalahari/gui/main_window.h"
#include "kalahari/gui/diagnostic_controller.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/settings_coordinator.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/document_coordinator.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/dialogs/about_dialog.h"
#include "kalahari/gui/dialogs/add_to_project_dialog.h"
#include "kalahari/gui/dialogs/icon_downloader_dialog.h"
#include "kalahari/gui/dialogs/new_item_dialog.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/toolbar_builder.h"
#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/gui/widgets/standalone_info_bar.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/log_panel_sink.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/theme.h"
#include "kalahari/core/document.h"
#include "kalahari/core/document_archive.h"
#include "kalahari/core/book.h"
#include "kalahari/core/book_element.h"
#include "kalahari/core/part.h"
#include "kalahari/core/recent_books_manager.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QApplication>
#include <QSettings>
#include <QDockWidget>
#include <QCloseEvent>
#include <QShowEvent>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QDir>
#include <QTextEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QStyle>
#include <QProgressDialog>
#include <QInputDialog>
#include <QTimer>
#include <map>

namespace kalahari {
namespace gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_fileMenu(nullptr)
    , m_editMenu(nullptr)
    , m_viewMenu(nullptr)
    , m_toolbarManager(nullptr)
    , m_menuBuilder(nullptr)
    , m_dockCoordinator(nullptr)
    , m_firstShow(true)
    , m_diagnosticController(nullptr)
    , m_settingsCoordinator(nullptr)
    , m_navigatorCoordinator(nullptr)
    , m_documentCoordinator(nullptr)
    , m_isDirty(false)
{
    auto& logger = core::Logger::getInstance();
    logger.info("MainWindow constructor called");

    // Set window properties
    setWindowTitle("Kalahari Writer's IDE");
    resize(1280, 720);

    // Create UI components (Command Registry pattern)
    registerCommands();    // Register all commands with CommandRegistry
    createMenus();         // Build menus from CommandRegistry
    createToolbars();      // Build toolbars from CommandRegistry
    createStatusBar();
    createDocks();

    // Create DiagnosticController after docks (needs LogPanel and LogDock from DockCoordinator)
    // OpenSpec #00038 - Diagnostic/dev mode extracted from MainWindow
    m_diagnosticController = new DiagnosticController(
        this,
        m_dockCoordinator->logPanel(),
        m_dockCoordinator->logDock(),
        statusBar(),
        this
    );
    logger.debug("MainWindow: DiagnosticController created");

    // Create SettingsCoordinator after docks (needs DockCoordinator for panel access)
    // OpenSpec #00038 Phase 5 - Settings management extracted from MainWindow
    m_settingsCoordinator = new SettingsCoordinator(
        this,
        m_dockCoordinator,
        statusBar(),
        this
    );
    // Set diagnostic mode getter so SettingsCoordinator can check current state
    m_settingsCoordinator->setDiagnosticModeGetter([this]() { return isDiagnosticMode(); });
    // Connect diagnostic mode signals
    connect(m_settingsCoordinator, &SettingsCoordinator::enableDiagnosticModeRequested,
            this, &MainWindow::enableDiagnosticMode);
    connect(m_settingsCoordinator, &SettingsCoordinator::disableDiagnosticModeRequested,
            this, &MainWindow::disableDiagnosticMode);
    logger.debug("MainWindow: SettingsCoordinator created");

    // Create NavigatorCoordinator after docks (needs DockCoordinator for panel/dock access)
    // OpenSpec #00038 Phase 6 - Navigator handlers extracted from MainWindow
    m_navigatorCoordinator = new NavigatorCoordinator(
        m_dockCoordinator->navigatorPanel(),
        m_dockCoordinator->propertiesPanel(),
        m_dockCoordinator->centralTabs(),
        m_dockCoordinator->propertiesDock(),
        statusBar(),
        this
    );
    logger.debug("MainWindow: NavigatorCoordinator created");

    // Connect DockCoordinator navigator signals to NavigatorCoordinator
    // These connections must be made after NavigatorCoordinator is created
    connect(m_dockCoordinator, &DockCoordinator::navigatorRequestRename,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestRename);
    connect(m_dockCoordinator, &DockCoordinator::navigatorRequestDelete,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestDelete);
    connect(m_dockCoordinator, &DockCoordinator::navigatorRequestMove,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestMove);
    connect(m_dockCoordinator, &DockCoordinator::navigatorRequestProperties,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestProperties);
    connect(m_dockCoordinator, &DockCoordinator::navigatorRequestSectionProperties,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestSectionProperties);
    connect(m_dockCoordinator, &DockCoordinator::navigatorRequestPartProperties,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestPartProperties);
    connect(m_dockCoordinator, &DockCoordinator::chapterReordered,
            m_navigatorCoordinator, &NavigatorCoordinator::onChapterReordered);
    connect(m_dockCoordinator, &DockCoordinator::partReordered,
            m_navigatorCoordinator, &NavigatorCoordinator::onPartReordered);
    logger.debug("MainWindow: Connected DockCoordinator navigator signals to NavigatorCoordinator");

    // Create DocumentCoordinator after NavigatorCoordinator (needs it for dirty chapter tracking)
    // OpenSpec #00038 Phase 7 - Document operations extracted from MainWindow
    m_documentCoordinator = new DocumentCoordinator(
        this,
        m_dockCoordinator->centralTabs(),
        m_dockCoordinator->navigatorPanel(),
        m_dockCoordinator->propertiesPanel(),
        m_dockCoordinator->dashboardPanel(),
        m_navigatorCoordinator,
        m_dockCoordinator->standaloneInfoBar(),
        statusBar(),
        [this]() { return m_isDirty; },
        [this](bool dirty) { setDirty(dirty); },
        [this]() { updateWindowTitle(); },
        this
    );
    // Connect DocumentCoordinator signals
    connect(m_documentCoordinator, &DocumentCoordinator::windowTitleChanged,
            this, &MainWindow::setWindowTitle);
    logger.debug("MainWindow: DocumentCoordinator created");

    // NOTE (Task #00015): EditorPanel textChanged signal connected when tab created
    // No m_editorPanel at startup - Dashboard is default first tab

    // Connect ThemeManager to MainWindow (Task #00023)
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged,
            this, &MainWindow::onThemeChanged);
    logger.debug("MainWindow: Connected ThemeManager::themeChanged signal to MainWindow");

    // Note: ThemeManager->IconRegistry connection is handled internally by ArtProvider
    // ArtProvider::initialize() connects ThemeManager::themeChanged to IconRegistry::onThemeChanged
    // and synchronizes colors from the current theme

    // Connect ProjectManager signals (OpenSpec #00033 Phase D) to DocumentCoordinator
    auto& pm = core::ProjectManager::getInstance();
    connect(&pm, &core::ProjectManager::projectOpened,
            m_documentCoordinator, &DocumentCoordinator::onProjectOpened);
    connect(&pm, &core::ProjectManager::projectClosed,
            m_documentCoordinator, &DocumentCoordinator::onProjectClosed);
    logger.debug("MainWindow: Connected ProjectManager signals to DocumentCoordinator");

    logger.info("MainWindow initialized successfully");
}

void MainWindow::registerCommands() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Registering commands with CommandRegistry (via CommandRegistrar)");

    // =========================================================================
    // ICON REGISTRY - Register default SVG icons (OpenSpec #00038: extracted to IconRegistrar)
    // =========================================================================
    registerAllIcons();

    // =========================================================================
    // COMMAND REGISTRY - Register all commands (OpenSpec #00038: extracted to CommandRegistrar)
    // =========================================================================

    // Create callbacks struct with lambdas that call MainWindow methods or DocumentCoordinator
    // NOTE: Document operations delegated to DocumentCoordinator (OpenSpec #00038 Phase 7)
    // DocumentCoordinator is created later, so we capture 'this' and call via m_documentCoordinator
    CommandCallbacks callbacks;

    // File commands - delegate to DocumentCoordinator
    callbacks.onNewDocument = [this]() { if (m_documentCoordinator) m_documentCoordinator->onNewDocument(); };
    callbacks.onNewProject = [this]() { if (m_documentCoordinator) m_documentCoordinator->onNewProject(); };
    callbacks.onOpenDocument = [this]() { if (m_documentCoordinator) m_documentCoordinator->onOpenDocument(); };
    callbacks.onOpenStandaloneFile = [this]() { if (m_documentCoordinator) m_documentCoordinator->onOpenStandaloneFile(); };
    callbacks.onSaveDocument = [this]() { if (m_documentCoordinator) m_documentCoordinator->onSaveDocument(); };
    callbacks.onSaveAsDocument = [this]() { if (m_documentCoordinator) m_documentCoordinator->onSaveAsDocument(); };
    callbacks.onImportArchive = [this]() { if (m_documentCoordinator) m_documentCoordinator->onImportArchive(); };
    callbacks.onExportArchive = [this]() { if (m_documentCoordinator) m_documentCoordinator->onExportArchive(); };
    callbacks.onExit = [this]() { onExit(); };

    // Edit commands
    callbacks.onUndo = [this]() { onUndo(); };
    callbacks.onRedo = [this]() { onRedo(); };
    callbacks.onCut = [this]() { onCut(); };
    callbacks.onCopy = [this]() { onCopy(); };
    callbacks.onPaste = [this]() { onPaste(); };
    callbacks.onSelectAll = [this]() { onSelectAll(); };
    callbacks.onSettings = [this]() { onSettings(); };

    // View commands
    callbacks.onDashboard = [this]() {
        // Check if Dashboard tab already exists
        QTabWidget* centralTabs = m_dockCoordinator->centralTabs();
        for (int i = 0; i < centralTabs->count(); ++i) {
            if (qobject_cast<DashboardPanel*>(centralTabs->widget(i))) {
                // Dashboard exists - activate it
                centralTabs->setCurrentIndex(i);
                core::Logger::getInstance().debug("Dashboard tab activated at index {}", i);
                return;
            }
        }

        // Dashboard doesn't exist - create new one
        DashboardPanel* dashboardPanel = new DashboardPanel(this);
        m_dockCoordinator->setDashboardPanel(dashboardPanel);
        auto& artProvider = core::ArtProvider::getInstance();
        QIcon dashboardIcon = artProvider.getIcon("view.dashboard");
        int dashboardIndex = centralTabs->addTab(dashboardPanel, dashboardIcon, tr("Dashboard"));
        centralTabs->setCurrentIndex(dashboardIndex);

        // Reconnect Dashboard signals to DocumentCoordinator
        connect(dashboardPanel, &DashboardPanel::openRecentBookRequested,
                m_documentCoordinator, &DocumentCoordinator::onOpenRecentFile);

        core::Logger::getInstance().info("Dashboard tab created at index {}", dashboardIndex);
    };
    callbacks.onResetLayout = [this]() { resetLayout(); };

    // Tools commands
    callbacks.onToolbarManager = [this]() { m_toolbarManager->openToolbarManagerDialog(); };

    // Help commands
    callbacks.onAbout = [this]() { onAbout(); };

    // Register all commands with the callbacks
    int count = registerAllCommands(callbacks);

    logger.debug("Commands registered successfully ({} commands)", count);
}


void MainWindow::createMenus() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating menus from CommandRegistry");

    // Build menu bar from CommandRegistry using MenuBuilder
    // Task #00025: Store MenuBuilder for icon refresh on theme change
    m_menuBuilder = new MenuBuilder();
    CommandRegistry& registry = CommandRegistry::getInstance();
    m_menuBuilder->buildMenuBar(registry, this);

    // Store FILE menu pointer for Recent Books integration
    m_fileMenu = m_menuBuilder->getMenu("FILE");

    // OpenSpec #00030: Add Recent Books submenu to FILE menu
    if (m_fileMenu) {
        auto& recentBooks = core::RecentBooksManager::getInstance();
        recentBooks.createRecentBooksMenu(m_fileMenu);

        // Connect signal to open recent file via DocumentCoordinator
        // Note: DocumentCoordinator is created later, but signal connections are deferred
        connect(&recentBooks, &core::RecentBooksManager::recentFileClicked,
                this, [this](const QString& path) {
                    if (m_documentCoordinator) m_documentCoordinator->onOpenRecentFile(path);
                });

        logger.debug("Recent Books submenu added to FILE menu");
    }

    // Note: View menu will be populated by createDocks() with panel toggles
    // This is intentional - View menu is created empty by MenuBuilder
    // and filled with QDockWidget toggleViewAction() later

    logger.debug("Menus created successfully from CommandRegistry");
}

void MainWindow::createToolbars() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating toolbars from CommandRegistry using ToolbarManager");

    // Task #00019: Create ToolbarManager and build 5 toolbars
    m_toolbarManager = new ToolbarManager(this);
    CommandRegistry& registry = CommandRegistry::getInstance();
    m_toolbarManager->createToolbars(registry);

    // OpenSpec #00026: Connect ArtProvider::resourcesChanged() to update toolbar icon sizes
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            this, [this]() {
                if (m_toolbarManager) {
                    m_toolbarManager->updateIconSizes();
                }
            });

    // OpenSpec #00032: Connect ArtProvider::resourcesChanged() to refresh dock title bar icons
    // Note: Connection moved to createDocks() after DockCoordinator is created

    logger.debug("5 toolbars created successfully (File, Edit, Book, View, Tools)");
}

void MainWindow::createStatusBar() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating status bar");

    statusBar()->showMessage(tr("Ready"), 3000);  // Show for 3 seconds

    logger.debug("Status bar created successfully");
}

// =============================================================================
// Document Operations - MOVED to DocumentCoordinator (OpenSpec #00038 Phase 7)
// onNewDocument, onNewProject, onOpenDocument, onOpenRecentFile,
// onSaveDocument, onSaveAsDocument, onSaveAll are now in document_coordinator.cpp
// =============================================================================

void MainWindow::onExit() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Exit");
    QApplication::quit();
}

void MainWindow::onUndo() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Undo");

    EditorPanel* editor = getCurrentEditor();
    if (editor) {
        editor->getTextEdit()->undo();
        statusBar()->showMessage(tr("Undo performed"), 2000);
    }
}

void MainWindow::onRedo() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Redo");

    EditorPanel* editor = getCurrentEditor();
    if (editor) {
        editor->getTextEdit()->redo();
        statusBar()->showMessage(tr("Redo performed"), 2000);
    }
}

void MainWindow::onCut() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Cut");

    EditorPanel* editor = getCurrentEditor();
    if (editor) {
        editor->getTextEdit()->cut();
        statusBar()->showMessage(tr("Cut to clipboard"), 2000);
    }
}

void MainWindow::onCopy() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Copy");

    EditorPanel* editor = getCurrentEditor();
    if (editor) {
        editor->getTextEdit()->copy();
        statusBar()->showMessage(tr("Copied to clipboard"), 2000);
    }
}

void MainWindow::onPaste() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Paste");

    EditorPanel* editor = getCurrentEditor();
    if (editor) {
        editor->getTextEdit()->paste();
        statusBar()->showMessage(tr("Pasted from clipboard"), 2000);
    }
}

void MainWindow::onSelectAll() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Select All");

    EditorPanel* editor = getCurrentEditor();
    if (editor) {
        editor->getTextEdit()->selectAll();
        statusBar()->showMessage(tr("All text selected"), 2000);
    }
}

void MainWindow::onSettings() {
    // OpenSpec #00038 Phase 5: Delegate to SettingsCoordinator
    m_settingsCoordinator->openSettingsDialog();
}

void MainWindow::onAbout() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: About Kalahari");

    dialogs::AboutDialog dialog(this);
    dialog.exec();

    logger.info("About dialog closed");
}

void MainWindow::onAboutQt() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: About Qt");

    QMessageBox::aboutQt(this, tr("About Qt"));

    logger.info("About Qt dialog displayed");
}

// Dock management - OpenSpec #00038 Phase 4: Delegated to DockCoordinator

void MainWindow::createDocks() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating dock widgets via DockCoordinator");

    // Create DockCoordinator to manage all dock widgets and panels
    m_dockCoordinator = new DockCoordinator(this, m_menuBuilder, this);
    m_dockCoordinator->createDocks();

    // Connect DockCoordinator signals to MainWindow slots or DocumentCoordinator
    // Note: navigatorElementSelected is kept for MainWindow delegation to NavigatorCoordinator
    connect(m_dockCoordinator, &DockCoordinator::navigatorElementSelected,
            this, &MainWindow::onNavigatorElementSelected);
    // OpenSpec #00038 Phase 7: Document operations delegated to DocumentCoordinator
    // These connections are made BEFORE DocumentCoordinator is created, so we use lambdas
    connect(m_dockCoordinator, &DockCoordinator::openRecentBookRequested,
            this, [this](const QString& path) {
                if (m_documentCoordinator) m_documentCoordinator->onOpenRecentFile(path);
            });
    connect(m_dockCoordinator, &DockCoordinator::addToProjectRequested,
            this, [this]() {
                if (m_documentCoordinator) m_documentCoordinator->onAddToProject();
            });
    connect(m_dockCoordinator, &DockCoordinator::openSettingsRequested,
            this, &MainWindow::onSettings);

    // NOTE: Navigator context menu signals and drag/drop reorder signals are connected
    // in the constructor after NavigatorCoordinator is created (OpenSpec #00038 Phase 6)

    // Connect PropertiesPanel status change to Navigator refresh
    connect(m_dockCoordinator, &DockCoordinator::chapterStatusChanged,
            m_dockCoordinator->navigatorPanel(), &NavigatorPanel::refreshItem);

    // Connect tab close signal (Task #00015)
    QTabWidget* centralTabs = m_dockCoordinator->centralTabs();
    connect(m_dockCoordinator, &DockCoordinator::tabCloseRequested, this, [this, centralTabs](int index) {
        auto& logger = core::Logger::getInstance();
        QWidget* widget = centralTabs->widget(index);
        EditorPanel* editor = qobject_cast<EditorPanel*>(widget);

        // Check for unsaved changes if EditorPanel
        if (editor && m_isDirty) {
            // TODO (Phase 1): Prompt user to save changes
            // For now: just close
        }

        // Remove tab and delete widget
        centralTabs->removeTab(index);
        widget->deleteLater();

        logger.debug("Tab closed at index {}", index);
    });

    // Connect tab change to navigator highlight (OpenSpec #00034 Phase C)
    connect(m_dockCoordinator, &DockCoordinator::currentTabChanged, this, [this, centralTabs](int index) {
        NavigatorPanel* navigatorPanel = m_dockCoordinator->navigatorPanel();
        if (index < 0) {
            navigatorPanel->clearHighlight();
            return;
        }

        QWidget* currentWidget = centralTabs->widget(index);
        EditorPanel* editor = qobject_cast<EditorPanel*>(currentWidget);
        if (editor) {
            QString elementId = editor->property("elementId").toString();
            if (!elementId.isEmpty()) {
                navigatorPanel->highlightElement(elementId);
            } else {
                navigatorPanel->clearHighlight();
            }
        } else {
            navigatorPanel->clearHighlight();
        }
    });

    // OpenSpec #00032: Connect ArtProvider::resourcesChanged() to refresh dock title bar icons
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            m_dockCoordinator, &DockCoordinator::refreshDockIcons);

    // Get VIEW menu reference for toolbar actions
    if (m_menuBuilder) {
        m_viewMenu = m_menuBuilder->getMenu("VIEW");
    }
    if (!m_viewMenu) {
        m_viewMenu = menuBar()->addMenu(tr("&View"));
    }

    // Task #00019: Add toolbar toggle actions to View menu
    if (m_toolbarManager) {
        m_toolbarManager->createViewMenuActions(m_viewMenu);
        logger.debug("Toolbar toggle actions added to VIEW menu");
    }

    logger.debug("Dock widgets created successfully via DockCoordinator");
}

void MainWindow::resetLayout() {
    m_dockCoordinator->resetLayout(isDiagnosticMode(), isDevMode());
    statusBar()->showMessage(tr("Layout reset to default"), 2000);
}

// Perspective save/restore
void MainWindow::closeEvent(QCloseEvent* event) {
    auto& logger = core::Logger::getInstance();
    logger.debug("MainWindow::closeEvent triggered");

    // Check for unsaved changes (Task #00008)
    // OpenSpec #00038 Phase 7: Use DocumentCoordinator for file path info
    if (m_isDirty) {
        QString filename = "Untitled";
        if (m_documentCoordinator && !m_documentCoordinator->currentFilePath().empty()) {
            filename = QString::fromStdString(m_documentCoordinator->currentFilePath().filename().string());
        }

        auto reply = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("Do you want to save changes to %1?").arg(filename),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Save) {
            if (m_documentCoordinator) m_documentCoordinator->onSaveDocument();
            if (m_isDirty) {
                // Save was cancelled or failed
                event->ignore();
                return;
            }
        } else if (reply == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        // Discard -> continue with close
    }

    // Phase F: Save Navigator expansion state before closing
    auto& pm = core::ProjectManager::getInstance();
    if (pm.isProjectOpen()) {
        QString projectPath = pm.getProjectPath();
        if (!projectPath.isEmpty()) {
            QFileInfo pathInfo(projectPath);
            QString projectId = pathInfo.absoluteFilePath()
                .replace("/", "_")
                .replace("\\", "_")
                .replace(":", "_")
                .replace(" ", "_");
            m_dockCoordinator->navigatorPanel()->saveExpansionState(projectId);
            logger.debug("Saved expansion state for project: {}", projectId.toStdString());
        }
    }

    // Save perspective (existing code)
    logger.debug("Saving window perspective");

    QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    // Task #00019: Save toolbar state (visibility)
    if (m_toolbarManager) {
        m_toolbarManager->saveState();
    }

    // Save settings to disk (including Navigator expansion state)
    core::SettingsManager::getInstance().save();

    logger.debug("Window perspective saved");

    event->accept();
}

// Helper methods (Task #00008)
void MainWindow::setDirty(bool dirty) {
    m_isDirty = dirty;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle() {
    // OpenSpec #00038 Phase 7: Get file path from DocumentCoordinator
    QString title = "Kalahari - ";

    if (m_documentCoordinator && !m_documentCoordinator->currentFilePath().empty()) {
        QString filename = QString::fromStdString(m_documentCoordinator->currentFilePath().filename().string());
        title += filename;
    } else {
        title += "Untitled";
    }

    if (m_isDirty) {
        title = "Kalahari - *" + title.mid(11);  // Insert "*" after "Kalahari - "
    }

    setWindowTitle(title);
}

// NOTE: getPhase0Content and setPhase0Content moved to DocumentCoordinator (OpenSpec #00038 Phase 7)

EditorPanel* MainWindow::getCurrentEditor() {
    // Task #00015: Get active EditorPanel tab (or nullptr)
    QTabWidget* centralTabs = m_dockCoordinator->centralTabs();
    if (!centralTabs) {
        return nullptr;
    }

    QWidget* currentWidget = centralTabs->currentWidget();
    return qobject_cast<EditorPanel*>(currentWidget);  // nullptr if not EditorPanel
}

// =============================================================================
// Project Manager Slots - MOVED to DocumentCoordinator (OpenSpec #00038 Phase 7)
// onProjectOpened, onProjectClosed are now in document_coordinator.cpp
// =============================================================================

void MainWindow::onNavigatorElementSelected(const QString& elementId, const QString& elementTitle) {
    // OpenSpec #00038 Phase 6: Delegate to NavigatorCoordinator
    if (m_navigatorCoordinator) {
        m_navigatorCoordinator->onElementSelected(elementId, elementTitle);
    }
}

// =============================================================================
// Diagnostic/Dev Mode (OpenSpec #00038 - Delegated to DiagnosticController)
// =============================================================================

void MainWindow::enableDiagnosticMode() {
    if (m_diagnosticController) {
        m_diagnosticController->enableDiagnosticMode();
    }
}

void MainWindow::disableDiagnosticMode() {
    if (m_diagnosticController) {
        m_diagnosticController->disableDiagnosticMode();
    }
}

bool MainWindow::isDiagnosticMode() const {
    return m_diagnosticController ? m_diagnosticController->isDiagnosticMode() : false;
}

void MainWindow::enableDevMode() {
    if (m_diagnosticController) {
        m_diagnosticController->enableDevMode();
    }
}

void MainWindow::disableDevMode() {
    if (m_diagnosticController) {
        m_diagnosticController->disableDevMode();
    }
}

bool MainWindow::isDevMode() const {
    return m_diagnosticController ? m_diagnosticController->isDevMode() : false;
}

void MainWindow::onThemeChanged(const core::Theme& theme) {
    auto& logger = core::Logger::getInstance();
    logger.info("MainWindow: Theme changed to '{}'", theme.name);

    // Note: IconRegistry::setThemeColors() is called automatically by ArtProvider
    // when ThemeManager::themeChanged is emitted - no need to call it here

    // OpenSpec #00026: Manual icon refresh REMOVED for menu/toolbar actions
    // Icons now auto-refresh via ArtProvider::resourcesChanged() signal
    // which is connected to all actions created by ArtProvider::createAction()

    // Task #00028: Refresh dock panel title bar icons
    // These are QLabel pixmaps, not QActions, so they need manual refresh
    m_dockCoordinator->refreshDockIcons();
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);

    if (m_firstShow) {
        auto& logger = core::Logger::getInstance();
        logger.debug("Restoring window perspective");

        // OpenSpec #00037: Check if toolbar config needs reset BEFORE restoring state
        // If reset needed, clear saved window state to prevent old toolbar layout from being restored
        bool toolbarResetNeeded = ToolbarManager::needsConfigReset();
        if (toolbarResetNeeded) {
            logger.info("Toolbar config reset needed, clearing saved window state");
            ToolbarManager::clearSavedWindowState();
        }

        QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
        restoreGeometry(settings.value("geometry").toByteArray());

        // Only restore window state if we haven't cleared it for toolbar reset
        // IMPORTANT: Read windowState AFTER clearSavedWindowState() to get fresh value
        QByteArray windowState = settings.value("windowState").toByteArray();
        if (!windowState.isEmpty() && !toolbarResetNeeded) {
            // Normal case: restore saved window state (includes toolbar positions)
            restoreState(windowState);
        }

        // Task #00019: Restore toolbar state (visibility)
        // This runs AFTER QMainWindow::restoreState() to ensure our visibility settings
        // take precedence over Qt's saved state
        if (m_toolbarManager) {
            m_toolbarManager->restoreState();
        }

        // OpenSpec #00024: LogDock visibility is INDEPENDENT of saveState/restoreState
        // It depends ONLY on diagnostic mode, not on saved window state
        // Use setVisible() instead of show()/hide() for more reliable behavior
        QDockWidget* logDock = m_dockCoordinator->logDock();
        if (logDock) {
            bool shouldShow = isDiagnosticMode() || isDevMode();
            logDock->setVisible(shouldShow);
            logger.info("LogDock: {} (diagnosticMode={}, devMode={})",
                       shouldShow ? "shown" : "hidden", isDiagnosticMode(), isDevMode());
        }

        logger.debug("Window perspective restored");

        // OpenSpec #00036: Auto-load last project if enabled
        // OpenSpec #00038 Phase 7: Use DocumentCoordinator for file operations
        auto& settingsManager = core::SettingsManager::getInstance();
        bool autoLoad = settingsManager.get<bool>("startup.autoLoadLastProject", false);
        if (autoLoad) {
            auto& recentManager = core::RecentBooksManager::getInstance();
            QStringList recentFiles = recentManager.getRecentFiles();
            if (!recentFiles.isEmpty()) {
                QString lastFile = recentFiles.first();
                logger.info("Auto-loading last project: {}", lastFile.toStdString());
                // Use QTimer::singleShot to defer opening after window is fully shown
                QTimer::singleShot(100, this, [this, lastFile]() {
                    if (m_documentCoordinator) m_documentCoordinator->onOpenRecentFile(lastFile);
                });
            }
        }

        m_firstShow = false;
    }
}

// =============================================================================
// Settings Management - NOTE: Moved to SettingsCoordinator (OpenSpec #00038 Phase 5)
// collectCurrentSettings() and onApplySettings() are now in settings_coordinator.cpp
// =============================================================================

// =============================================================================
// Standalone File Support - MOVED to DocumentCoordinator (OpenSpec #00038 Phase 7)
// onOpenStandaloneFile, openStandaloneFile, onAddToProject,
// onExportArchive, onImportArchive are now in document_coordinator.cpp
// =============================================================================

// =============================================================================
// Navigator Context Menu Handlers - Moved to NavigatorCoordinator (OpenSpec #00038 Phase 6)
// =============================================================================

} // namespace gui
} // namespace kalahari
