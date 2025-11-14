/// @file main_window.cpp
/// @brief Implementation of MainWindow

#include "main_window.h"
#include "settings_dialog.h"
#include "kalahari/gui/dialogs/about_dialog.h"
#include "kalahari/gui/dialogs/manage_perspectives_dialog.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/statistics_panel.h"
#include "kalahari/gui/panels/search_panel.h"
#include "kalahari/gui/panels/assistant_panel.h"
#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/gui/perspective_manager.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/shortcut_manager.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/toolbar_builder.h"
#include <kalahari/core/logger.h>
#include <kalahari/core/gui_log_sink.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/python_interpreter.h>
#include <kalahari/core/diagnostic_manager.h>
#include <kalahari/core/plugin_manager.h>
#include <kalahari/core/document.h>
#include <kalahari/core/book.h>
#include <kalahari/core/part.h>
#include <kalahari/core/book_element.h>
#include <kalahari/gui/icon_registry.h>
#include <kalahari/gui/art_provider.h>
#include <kalahari/resources/icons_material.h>
#include <wx/artprov.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/utils.h>
#include <chrono>
#include <algorithm>

namespace kalahari {
namespace gui {

// ============================================================================
// Custom Event Definitions (KALAHARI convention)
// ============================================================================

wxDEFINE_EVENT(wxEVT_KALAHARI_TASK_COMPLETED, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_KALAHARI_TASK_FAILED, wxThreadEvent);

// ============================================================================
// Event Table (maps events to handler methods)
// ============================================================================

// ============================================================================
// Menu IDs (custom IDs for application-specific menu items)
// ============================================================================

enum {
    ID_DIAG_TEST_PYTHON = wxID_HIGHEST + 1,
    ID_DIAG_TEST_PYBIND11,
    ID_DIAG_TEST_PLUGINS,
    ID_DIAG_OPEN_LOGS,
    ID_DIAG_SYSTEM_INFO,

    // Format menu (Task #00014)
    ID_FORMAT_BOLD,
    ID_FORMAT_ITALIC,
    ID_FORMAT_UNDERLINE,
    ID_FORMAT_FONT,
    ID_FORMAT_CLEAR,

    // View menu (panel visibility toggles)
    ID_VIEW_NAVIGATOR,
    ID_VIEW_PROPERTIES,
    ID_VIEW_STATISTICS,
    ID_VIEW_SEARCH,
    ID_VIEW_ASSISTANT,
    ID_VIEW_LOG,  // Diagnostic Log Panel (only visible in diagnostic mode)

    // Editor View Modes (Task #00019)
    ID_VIEW_MODE_FULL,
    ID_VIEW_MODE_PAGE,
    ID_VIEW_MODE_TYPEWRITER,
    ID_VIEW_MODE_PUBLISHER,

    // Perspectives menu
    ID_PERSPECTIVE_DEFAULT,
    ID_PERSPECTIVE_WRITING,
    ID_PERSPECTIVE_EDITING,
    ID_PERSPECTIVE_RESEARCH,
    ID_PERSPECTIVE_SAVE,
    ID_PERSPECTIVE_MANAGE,

    // Custom perspectives (dynamic menu items, max 5)
    ID_PERSPECTIVE_CUSTOM_START = wxID_HIGHEST + 250
    // IDs: ID_PERSPECTIVE_CUSTOM_START + 0 through +4
};

wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
    // File menu events
    EVT_MENU(wxID_NEW,         MainWindow::onFileNew)
    EVT_MENU(wxID_OPEN,        MainWindow::onFileOpen)
    EVT_MENU(wxID_SAVE,        MainWindow::onFileSave)
    EVT_MENU(wxID_SAVEAS,      MainWindow::onFileSaveAs)
    // EVT_MENU(wxID_PREFERENCES, MainWindow::onFileSettings)  // Task #00033: Now handled by CommandRegistry
    EVT_MENU(wxID_EXIT,        MainWindow::onFileExit)

    // Edit menu events
    EVT_MENU(wxID_UNDO,       MainWindow::onEditUndo)
    EVT_MENU(wxID_REDO,       MainWindow::onEditRedo)
    EVT_MENU(wxID_CUT,        MainWindow::onEditCut)
    EVT_MENU(wxID_COPY,       MainWindow::onEditCopy)
    EVT_MENU(wxID_PASTE,      MainWindow::onEditPaste)
    EVT_MENU(wxID_SELECTALL,  MainWindow::onEditSelectAll)

    // Format menu events (Task #00014)
    EVT_MENU(ID_FORMAT_BOLD,       MainWindow::onFormatBold)
    EVT_MENU(ID_FORMAT_ITALIC,     MainWindow::onFormatItalic)
    EVT_MENU(ID_FORMAT_UNDERLINE,  MainWindow::onFormatUnderline)
    EVT_MENU(ID_FORMAT_FONT,       MainWindow::onFormatFont)
    EVT_MENU(ID_FORMAT_CLEAR,      MainWindow::onFormatClear)

    // View menu events (panel visibility)
    EVT_MENU(ID_VIEW_NAVIGATOR,   MainWindow::onViewNavigator)
    EVT_MENU(ID_VIEW_PROPERTIES,  MainWindow::onViewProperties)
    EVT_MENU(ID_VIEW_STATISTICS,  MainWindow::onViewStatistics)
    EVT_MENU(ID_VIEW_SEARCH,      MainWindow::onViewSearch)
    EVT_MENU(ID_VIEW_ASSISTANT,   MainWindow::onViewAssistant)
    EVT_MENU(ID_VIEW_LOG,         MainWindow::onViewLog)

    // Editor View Mode events (Task #00019)
    EVT_MENU_RANGE(ID_VIEW_MODE_FULL, ID_VIEW_MODE_PUBLISHER, MainWindow::onViewMode)

    // Perspectives menu events
    EVT_MENU(ID_PERSPECTIVE_DEFAULT,  MainWindow::onLoadPerspective)
    EVT_MENU(ID_PERSPECTIVE_WRITING,  MainWindow::onLoadPerspective)
    EVT_MENU(ID_PERSPECTIVE_EDITING,  MainWindow::onLoadPerspective)
    EVT_MENU(ID_PERSPECTIVE_RESEARCH, MainWindow::onLoadPerspective)
    EVT_MENU_RANGE(ID_PERSPECTIVE_CUSTOM_START, ID_PERSPECTIVE_CUSTOM_START + 4, MainWindow::onLoadPerspective)
    EVT_MENU(ID_PERSPECTIVE_SAVE,     MainWindow::onSavePerspective)
    EVT_MENU(ID_PERSPECTIVE_MANAGE,   MainWindow::onManagePerspectives)

    // Diagnostics menu events
    EVT_MENU(ID_DIAG_TEST_PYTHON, MainWindow::onDiagnosticsTestPython)
    EVT_MENU(ID_DIAG_TEST_PYBIND11, MainWindow::onDiagnosticsTestPyBind11)
    EVT_MENU(ID_DIAG_TEST_PLUGINS, MainWindow::onDiagnosticsTestPlugins)
    EVT_MENU(ID_DIAG_OPEN_LOGS, MainWindow::onDiagnosticsOpenLogs)
    EVT_MENU(ID_DIAG_SYSTEM_INFO, MainWindow::onDiagnosticsSystemInfo)

    // Help menu events
    EVT_MENU(wxID_ABOUT,   MainWindow::onHelpAbout)

    // Window events
    EVT_CLOSE(MainWindow::onClose)
wxEND_EVENT_TABLE()

// ============================================================================
// Constructor / Destructor
// ============================================================================

MainWindow::MainWindow()
    : wxFrame(nullptr, wxID_ANY, _("Kalahari Writer's IDE")),
      m_threadSemaphore(4, 4)  // Max 4 background threads (initial=4, max=4)
{
    core::Logger::getInstance().info("Initializing main window with threading support");

    // Bind custom event from SettingsDialog (Apply button)
    Bind(EVT_SETTINGS_APPLIED, &MainWindow::onSettingsApplied, this);

    // Reserve thread pool capacity (avoid reallocation)
    m_activeThreads.reserve(4);

    // Load settings from disk (Task #00003)
    auto& settings = core::SettingsManager::getInstance();
    settings.load();

    // Initialize icon system (Task #00013)
    core::Logger::getInstance().info("Initializing icon system...");
    auto& iconRegistry = IconRegistry::getInstance();
    iconRegistry.initialize();
    KalahariArtProvider::Initialize();

    // Set Kalahari brand color for all icons (orange: RGB 233,84,32)
    iconRegistry.setTheme(IconRegistry::ColorTheme::Custom, wxColour(233, 84, 32));

    // Set icon sizes (24px for toolbar/trees/notebooks, standard 16px for menus)
    IconSizeConfig sizes;
    sizes.toolbar = 24;  // Toolbar icons
    sizes.menu = 16;     // Menu icons (standard size)
    sizes.panel = 24;    // Tree/Notebook icons
    sizes.dialog = 32;   // Dialog icons
    iconRegistry.setSizes(sizes);

    // Register Material Design icons for ALL standard wxID operations
    // This prevents GTK/Windows from using stock icons inconsistently
    using namespace kalahari::icons::material;

    // File operations
    iconRegistry.registerIcon(wxART_NEW, FILE_NEW, _("New"));
    iconRegistry.registerIcon(wxART_FILE_OPEN, FILE_OPEN, _("Open"));
    iconRegistry.registerIcon(wxART_FILE_SAVE, SAVE, _("Save"));
    iconRegistry.registerIcon(wxART_FILE_SAVE_AS, SAVE_AS, _("Save As"));
    iconRegistry.registerIcon(wxART_FOLDER, FOLDER, _("Folder"));
    iconRegistry.registerIcon(wxART_PRINT, PRINT, _("Print"));
    iconRegistry.registerIcon(wxART_QUIT, EXIT, _("Exit"));
    iconRegistry.registerIcon(wxART_CLOSE, CLOSE, _("Close"));

    // Edit operations
    iconRegistry.registerIcon(wxART_UNDO, UNDO, _("Undo"));
    iconRegistry.registerIcon(wxART_REDO, REDO, _("Redo"));
    iconRegistry.registerIcon(wxART_CUT, CUT, _("Cut"));
    iconRegistry.registerIcon(wxART_COPY, COPY, _("Copy"));
    iconRegistry.registerIcon(wxART_PASTE, PASTE, _("Paste"));
    iconRegistry.registerIcon(wxART_DELETE, DELETE_ICON, _("Delete"));

    // Search operations
    iconRegistry.registerIcon(wxART_FIND, FIND, _("Find"));
    iconRegistry.registerIcon(wxART_FIND_AND_REPLACE, FIND, _("Find and Replace")); // Reuse FIND icon

    // Settings and info
    iconRegistry.registerIcon(wxART_HELP_SETTINGS, SETTINGS, _("Settings"));
    iconRegistry.registerIcon(wxART_HELP, HELP, _("Help"));
    iconRegistry.registerIcon(wxART_INFORMATION, INFORMATION, _("Information"));

    // Message box icons (system dialogs)
    iconRegistry.registerIcon(wxART_WARNING, WARNING, _("Warning"));
    iconRegistry.registerIcon(wxART_ERROR, ERROR_ICON, _("Error"));
    iconRegistry.registerIcon(wxART_QUESTION, QUESTION, _("Question"));

    core::Logger::getInstance().info("Icon system initialized (22 Material Design icons registered)");

    // Restore window size and position from settings
    wxSize windowSize = settings.getWindowSize();
    wxPoint windowPos = settings.getWindowPosition();
    bool isMaximized = settings.isWindowMaximized();

    SetSize(windowSize);
    SetPosition(windowPos);

    if (isMaximized) {
        Maximize();
    }

    core::Logger::getInstance().debug("Window size restored to {}x{} at ({}, {}), maximized: {}",
                                      windowSize.GetWidth(), windowSize.GetHeight(),
                                      windowPos.x, windowPos.y, isMaximized);

    // Initialize diagnostic mode state from DiagnosticManager
    m_diagnosticMode = core::DiagnosticManager::getInstance().isEnabled();
    m_launchedWithDiagFlag = m_diagnosticMode; // If enabled now, it was via CLI flag
    core::Logger::getInstance().debug("Diagnostic mode: {}, launched with flag: {}",
                                      m_diagnosticMode, m_launchedWithDiagFlag);

    // Create UI components (order matters!)
    createMenuBarDynamic();  // Task #00031: Use MenuBuilder instead of hardcoded menus
    createToolBarDynamic();  // Task #00032: Use ToolbarBuilder instead of hardcoded toolbar
    createStatusBar();

    // Register File menu commands in CommandRegistry (Task #00028)
    registerFileCommands();

    // Register Edit menu commands in CommandRegistry (Task #00029)
    registerEditCommands();

    // Register Format menu commands in CommandRegistry (Task #00030)
    registerFormatCommands();

    // Initialize wxAUI docking system with panels (Task #00013)
    initializeAUI();

    // Bind threading events dynamically (modern approach, allows runtime binding)
    Bind(wxEVT_KALAHARI_TASK_COMPLETED, &MainWindow::onTaskCompleted, this, wxID_ANY);
    Bind(wxEVT_KALAHARI_TASK_FAILED, &MainWindow::onTaskFailed, this, wxID_ANY);

    core::Logger::getInstance().debug("Threading infrastructure initialized (max 4 threads)");
    core::Logger::getInstance().info("MainWindow construction complete");
}

MainWindow::~MainWindow() {
    core::Logger::getInstance().info("MainWindow shutting down...");

    // Uninitialize wxAUI manager (Task #00013)
    if (m_auiManager) {
        m_auiManager->UnInit();
        m_auiManager = nullptr;
        core::Logger::getInstance().debug("wxAuiManager uninitialized");
    }

    // Wait for all background tasks to complete (graceful shutdown)
    if (!m_activeThreads.empty()) {
        core::Logger::getInstance().warn("Waiting for {} background tasks to finish...",
                                         m_activeThreads.size());

        // Wait up to 5 seconds for threads to finish
        auto startTime = std::chrono::steady_clock::now();
        while (!m_activeThreads.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > std::chrono::seconds(5)) {
                core::Logger::getInstance().error(
                    "Timeout waiting for {} background tasks (forced shutdown)",
                    m_activeThreads.size());
                break;
            }
        }
    }

    core::Logger::getInstance().info("MainWindow destroyed (all background tasks completed)");
    // Cleanup handled automatically by wxWidgets for child windows
}

// ============================================================================
// Command Registry Integration (Task #00028)
// ============================================================================

void MainWindow::registerFileCommands() {
    core::Logger::getInstance().debug("Registering File menu commands in CommandRegistry...");

    CommandRegistry& registry = CommandRegistry::getInstance();
    ShortcutManager& shortcuts = ShortcutManager::getInstance();

    // ------------------------------------------------------------------------
    // file.new - Create New Document
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "file.new";
        cmd.label = _("New").ToStdString();
        cmd.tooltip = _("Create a new document").ToStdString();
        cmd.category = "File";
        cmd.showInMenu = true;
        cmd.showInToolbar = true;
        cmd.shortcut = KeyboardShortcut('N', true);  // Ctrl+N
        cmd.execute = [this]() {
            core::Logger::getInstance().info("File -> New executed via CommandRegistry");
            m_statusBar->SetStatusText(_("New document (stub)"), 0);
            wxMessageBox(
                _("New document functionality will be implemented in Phase 1.\n\n"
                  "Phase 1 Week 13: Command Registry Integration"),
                _("New Document"),
                wxOK | wxICON_INFORMATION,
                this
            );
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ------------------------------------------------------------------------
    // file.open - Open Existing Document
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "file.open";
        cmd.label = _("Open...").ToStdString();
        cmd.tooltip = _("Open an existing document").ToStdString();
        cmd.category = "File";
        cmd.showInMenu = true;
        cmd.showInToolbar = true;
        cmd.shortcut = KeyboardShortcut('O', true);  // Ctrl+O
        cmd.execute = [this]() {
            core::Logger::getInstance().info("File -> Open executed via CommandRegistry");
            m_statusBar->SetStatusText(_("Open document (stub)"), 0);
            wxMessageBox(
                _("Open document functionality will be implemented in Phase 1.\n\n"
                  "Phase 1 Week 13: Command Registry Integration"),
                _("Open Document"),
                wxOK | wxICON_INFORMATION,
                this
            );
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ------------------------------------------------------------------------
    // file.save - Save Current Document
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "file.save";
        cmd.label = _("Save").ToStdString();
        cmd.tooltip = _("Save the current document").ToStdString();
        cmd.category = "File";
        cmd.showInMenu = true;
        cmd.showInToolbar = true;
        cmd.shortcut = KeyboardShortcut('S', true);  // Ctrl+S
        cmd.execute = [this]() {
            core::Logger::getInstance().info("File -> Save executed via CommandRegistry");
            m_statusBar->SetStatusText(_("Save document (stub)"), 0);
            wxMessageBox(
                _("Save document functionality will be implemented in Phase 1.\n\n"
                  "Phase 1 Week 13: Command Registry Integration"),
                _("Save Document"),
                wxOK | wxICON_INFORMATION,
                this
            );
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ------------------------------------------------------------------------
    // file.save_as - Save Document As
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "file.save_as";
        cmd.label = _("Save As...").ToStdString();
        cmd.tooltip = _("Save document with a new name").ToStdString();
        cmd.category = "File";
        cmd.showInMenu = true;
        cmd.showInToolbar = false;
        cmd.shortcut = KeyboardShortcut('S', true, false, true);  // Ctrl+Shift+S
        cmd.execute = [this]() {
            core::Logger::getInstance().info("File -> Save As executed via CommandRegistry");
            m_statusBar->SetStatusText(_("Save As (stub)"), 0);
            wxMessageBox(
                _("Save As functionality will be implemented in Phase 1.\n\n"
                  "Phase 1 Week 13: Command Registry Integration"),
                _("Save As"),
                wxOK | wxICON_INFORMATION,
                this
            );
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ------------------------------------------------------------------------
    // file.settings - Open Settings Dialog
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "file.settings";
        cmd.label = _("Settings...").ToStdString();
        cmd.tooltip = _("Open application settings").ToStdString();
        cmd.category = "File";
        cmd.showInMenu = true;
        cmd.showInToolbar = false;
        cmd.shortcut = KeyboardShortcut(',', true);  // Ctrl+,
        cmd.execute = [this]() {
            core::Logger::getInstance().info("File -> Settings executed via CommandRegistry");

            // Call existing settings dialog implementation
            // (Task #00033: Integrated with CommandRegistry)
            wxCommandEvent dummyEvent(wxEVT_MENU, wxID_PREFERENCES);
            onFileSettings(dummyEvent);
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ------------------------------------------------------------------------
    // file.exit - Exit Application
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "file.exit";
        cmd.label = _("Exit").ToStdString();
        cmd.tooltip = _("Exit Kalahari").ToStdString();
        cmd.category = "File";
        cmd.showInMenu = true;
        cmd.showInToolbar = false;
        cmd.shortcut = KeyboardShortcut(WXK_F4, false, true);  // Alt+F4
        cmd.execute = [this]() {
            core::Logger::getInstance().info("File -> Exit executed via CommandRegistry");
            Close(true);  // Triggers wxCloseEvent (calls onClose handler)
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    core::Logger::getInstance().info("File menu commands registered (6 commands)");
}

void MainWindow::registerEditCommands() {
    core::Logger::getInstance().debug("Registering Edit menu commands in CommandRegistry...");

    CommandRegistry& registry = CommandRegistry::getInstance();
    ShortcutManager& shortcuts = ShortcutManager::getInstance();

    // ------------------------------------------------------------------------
    // edit.undo - Undo Last Action
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "edit.undo";
        cmd.label = _("Undo").ToStdString();
        cmd.tooltip = _("Undo last action").ToStdString();
        cmd.category = "Edit";
        cmd.showInMenu = true;
        cmd.showInToolbar = true;
        cmd.shortcut = KeyboardShortcut('Z', true);  // Ctrl+Z
        cmd.execute = [this]() {
            core::Logger::getInstance().info("Edit -> Undo executed via CommandRegistry");
            m_statusBar->SetStatusText(_("Undo (stub)"), 0);
            wxMessageBox(
                _("Undo functionality will be implemented in Phase 1.\n\n"
                  "Phase 1 Week 13: Command Registry Integration"),
                _("Undo"),
                wxOK | wxICON_INFORMATION,
                this
            );
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ------------------------------------------------------------------------
    // edit.redo - Redo Last Undone Action
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "edit.redo";
        cmd.label = _("Redo").ToStdString();
        cmd.tooltip = _("Redo last undone action").ToStdString();
        cmd.category = "Edit";
        cmd.showInMenu = true;
        cmd.showInToolbar = true;
        cmd.shortcut = KeyboardShortcut('Y', true);  // Ctrl+Y
        cmd.execute = [this]() {
            core::Logger::getInstance().info("Edit -> Redo executed via CommandRegistry");
            m_statusBar->SetStatusText(_("Redo (stub)"), 0);
            wxMessageBox(
                _("Redo functionality will be implemented in Phase 1.\n\n"
                  "Phase 1 Week 13: Command Registry Integration"),
                _("Redo"),
                wxOK | wxICON_INFORMATION,
                this
            );
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ------------------------------------------------------------------------
    // edit.cut - Cut Selection to Clipboard
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "edit.cut";
        cmd.label = _("Cut").ToStdString();
        cmd.tooltip = _("Cut selection to clipboard").ToStdString();
        cmd.category = "Edit";
        cmd.showInMenu = true;
        cmd.showInToolbar = true;
        cmd.shortcut = KeyboardShortcut('X', true);  // Ctrl+X
        cmd.execute = [this]() {
            core::Logger::getInstance().debug("Edit -> Cut executed via CommandRegistry");

            if (m_editorPanel) {
                // Create dummy event for EditorPanel handler
                // TODO (Phase 2): Refactor EditorPanel to have direct cut() method
                wxCommandEvent event(wxEVT_MENU, wxID_CUT);
                m_editorPanel->onEditCut(event);
            } else {
                core::Logger::getInstance().warn("No EditorPanel available for Edit -> Cut");
            }
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ------------------------------------------------------------------------
    // edit.copy - Copy Selection to Clipboard
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "edit.copy";
        cmd.label = _("Copy").ToStdString();
        cmd.tooltip = _("Copy selection to clipboard").ToStdString();
        cmd.category = "Edit";
        cmd.showInMenu = true;
        cmd.showInToolbar = true;
        cmd.shortcut = KeyboardShortcut('C', true);  // Ctrl+C
        cmd.execute = [this]() {
            core::Logger::getInstance().debug("Edit -> Copy executed via CommandRegistry");

            if (m_editorPanel) {
                // Create dummy event for EditorPanel handler
                // TODO (Phase 2): Refactor EditorPanel to have direct copy() method
                wxCommandEvent event(wxEVT_MENU, wxID_COPY);
                m_editorPanel->onEditCopy(event);
            } else {
                core::Logger::getInstance().warn("No EditorPanel available for Edit -> Copy");
            }
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ------------------------------------------------------------------------
    // edit.paste - Paste from Clipboard
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "edit.paste";
        cmd.label = _("Paste").ToStdString();
        cmd.tooltip = _("Paste from clipboard").ToStdString();
        cmd.category = "Edit";
        cmd.showInMenu = true;
        cmd.showInToolbar = true;
        cmd.shortcut = KeyboardShortcut('V', true);  // Ctrl+V
        cmd.execute = [this]() {
            core::Logger::getInstance().debug("Edit -> Paste executed via CommandRegistry");

            if (m_editorPanel) {
                // Create dummy event for EditorPanel handler
                // TODO (Phase 2): Refactor EditorPanel to have direct paste() method
                wxCommandEvent event(wxEVT_MENU, wxID_PASTE);
                m_editorPanel->onEditPaste(event);
            } else {
                core::Logger::getInstance().warn("No EditorPanel available for Edit -> Paste");
            }
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ------------------------------------------------------------------------
    // edit.select_all - Select All Text
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "edit.select_all";
        cmd.label = _("Select All").ToStdString();
        cmd.tooltip = _("Select all text").ToStdString();
        cmd.category = "Edit";
        cmd.showInMenu = true;
        cmd.showInToolbar = false;
        cmd.shortcut = KeyboardShortcut('A', true);  // Ctrl+A
        cmd.execute = [this]() {
            core::Logger::getInstance().debug("Edit -> Select All executed via CommandRegistry");

            if (m_editorPanel) {
                // Create dummy event for EditorPanel handler
                // TODO (Phase 2): Refactor EditorPanel to have direct selectAll() method
                wxCommandEvent event(wxEVT_MENU, wxID_SELECTALL);
                m_editorPanel->onEditSelectAll(event);
            } else {
                core::Logger::getInstance().warn("No EditorPanel available for Edit -> Select All");
            }
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    core::Logger::getInstance().info("Edit menu commands registered (6 commands)");
}

void MainWindow::registerFormatCommands() {
    core::Logger::getInstance().debug("Registering Format menu commands in CommandRegistry...");

    CommandRegistry& registry = CommandRegistry::getInstance();
    ShortcutManager& shortcuts = ShortcutManager::getInstance();

    // ------------------------------------------------------------------------
    // format.bold - Toggle Bold Formatting
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "format.bold";
        cmd.label = _("Bold").ToStdString();
        cmd.tooltip = _("Toggle bold formatting").ToStdString();
        cmd.category = "Format";
        cmd.showInMenu = true;
        cmd.showInToolbar = true;
        cmd.shortcut = KeyboardShortcut('B', true);  // Ctrl+B
        cmd.execute = [this]() {
            core::Logger::getInstance().debug("Format -> Bold executed via CommandRegistry");

            if (m_editorPanel) {
                // Create dummy event for EditorPanel handler
                // TODO (Phase 2): Refactor EditorPanel to have direct formatBold() method
                wxCommandEvent event(wxEVT_MENU, ID_FORMAT_BOLD);
                m_editorPanel->onFormatBold(event);
            } else {
                core::Logger::getInstance().warn("No EditorPanel available for Format -> Bold");
            }
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ------------------------------------------------------------------------
    // format.italic - Toggle Italic Formatting
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "format.italic";
        cmd.label = _("Italic").ToStdString();
        cmd.tooltip = _("Toggle italic formatting").ToStdString();
        cmd.category = "Format";
        cmd.showInMenu = true;
        cmd.showInToolbar = true;
        cmd.shortcut = KeyboardShortcut('I', true);  // Ctrl+I
        cmd.execute = [this]() {
            core::Logger::getInstance().debug("Format -> Italic executed via CommandRegistry");

            if (m_editorPanel) {
                // Create dummy event for EditorPanel handler
                // TODO (Phase 2): Refactor EditorPanel to have direct formatItalic() method
                wxCommandEvent event(wxEVT_MENU, ID_FORMAT_ITALIC);
                m_editorPanel->onFormatItalic(event);
            } else {
                core::Logger::getInstance().warn("No EditorPanel available for Format -> Italic");
            }
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ------------------------------------------------------------------------
    // format.underline - Toggle Underline
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "format.underline";
        cmd.label = _("Underline").ToStdString();
        cmd.tooltip = _("Toggle underline").ToStdString();
        cmd.category = "Format";
        cmd.showInMenu = true;
        cmd.showInToolbar = true;
        cmd.shortcut = KeyboardShortcut('U', true);  // Ctrl+U
        cmd.execute = [this]() {
            core::Logger::getInstance().debug("Format -> Underline executed via CommandRegistry");

            if (m_editorPanel) {
                // Create dummy event for EditorPanel handler
                // TODO (Phase 2): Refactor EditorPanel to have direct formatUnderline() method
                wxCommandEvent event(wxEVT_MENU, ID_FORMAT_UNDERLINE);
                m_editorPanel->onFormatUnderline(event);
            } else {
                core::Logger::getInstance().warn("No EditorPanel available for Format -> Underline");
            }
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ------------------------------------------------------------------------
    // format.font - Choose Font and Size
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "format.font";
        cmd.label = _("Font...").ToStdString();
        cmd.tooltip = _("Choose font and size").ToStdString();
        cmd.category = "Format";
        cmd.showInMenu = true;
        cmd.showInToolbar = false;
        // No keyboard shortcut for Font dialog
        cmd.execute = [this]() {
            core::Logger::getInstance().debug("Format -> Font executed via CommandRegistry");

            if (m_editorPanel) {
                // Create dummy event for EditorPanel handler
                // TODO (Phase 2): Refactor EditorPanel to have direct formatFont() method
                wxCommandEvent event(wxEVT_MENU, ID_FORMAT_FONT);
                m_editorPanel->onFormatFont(event);
            } else {
                core::Logger::getInstance().warn("No EditorPanel available for Format -> Font");
            }
        };

        registry.registerCommand(cmd);
    }

    // ------------------------------------------------------------------------
    // format.clear_formatting - Remove All Formatting
    // ------------------------------------------------------------------------
    {
        Command cmd;
        cmd.id = "format.clear_formatting";
        cmd.label = _("Clear Formatting").ToStdString();
        cmd.tooltip = _("Remove all formatting").ToStdString();
        cmd.category = "Format";
        cmd.showInMenu = true;
        cmd.showInToolbar = false;
        // No keyboard shortcut for Clear Formatting
        cmd.execute = [this]() {
            core::Logger::getInstance().debug("Format -> Clear Formatting executed via CommandRegistry");

            if (m_editorPanel) {
                // Create dummy event for EditorPanel handler
                // TODO (Phase 2): Refactor EditorPanel to have direct clearFormatting() method
                wxCommandEvent event(wxEVT_MENU, ID_FORMAT_CLEAR);
                m_editorPanel->onFormatClear(event);
            } else {
                core::Logger::getInstance().warn("No EditorPanel available for Format -> Clear Formatting");
            }
        };

        registry.registerCommand(cmd);
    }

    core::Logger::getInstance().info("Format menu commands registered (5 commands)");
}

// ============================================================================
// UI Component Creation
// ============================================================================

void MainWindow::createMenuBar() {
    core::Logger::getInstance().debug("Creating menu bar...");

    m_menuBar = new wxMenuBar();

    // ------------------------------------------------------------------------
    // File Menu
    // ------------------------------------------------------------------------
    wxMenu* fileMenu = new wxMenu();

    // New - with icon
    wxMenuItem* newItem = fileMenu->Append(wxID_NEW, _("&New\tCtrl+N"), _("Create a new document"));
    newItem->SetBitmap(wxArtProvider::GetBitmapBundle(wxART_NEW, wxART_MENU));

    // Open - with icon
    wxMenuItem* openItem = fileMenu->Append(wxID_OPEN, _("&Open...\tCtrl+O"), _("Open an existing document"));
    openItem->SetBitmap(wxArtProvider::GetBitmapBundle(wxART_FILE_OPEN, wxART_MENU));

    fileMenu->AppendSeparator();

    // Save - with icon
    wxMenuItem* saveItem = fileMenu->Append(wxID_SAVE, _("&Save\tCtrl+S"), _("Save the current document"));
    saveItem->SetBitmap(wxArtProvider::GetBitmapBundle(wxART_FILE_SAVE, wxART_MENU));

    // Save As - with Material Design icon (replaces GTK stock icon)
    wxMenuItem* saveAsItem = fileMenu->Append(wxID_SAVEAS, _("Save &As...\tCtrl+Shift+S"), _("Save document with a new name"));
    saveAsItem->SetBitmap(wxArtProvider::GetBitmapBundle(wxART_FILE_SAVE_AS, wxART_MENU));

    fileMenu->AppendSeparator();

    // Settings - with icon
    wxMenuItem* settingsItem = fileMenu->Append(wxID_PREFERENCES, _("Se&ttings...\tCtrl+,"), _("Open application settings"));
    settingsItem->SetBitmap(wxArtProvider::GetBitmapBundle(wxART_HELP_SETTINGS, wxART_MENU));

    fileMenu->AppendSeparator();

    // Exit - with Material Design icon (replaces GTK stock icon)
    wxMenuItem* exitItem = fileMenu->Append(wxID_EXIT, _("E&xit\tAlt+F4"), _("Exit Kalahari"));
    exitItem->SetBitmap(wxArtProvider::GetBitmapBundle(wxART_QUIT, wxART_MENU));

    m_menuBar->Append(fileMenu, _("&File"));

    // ------------------------------------------------------------------------
    // Edit Menu
    // ------------------------------------------------------------------------
    wxMenu* editMenu = new wxMenu();

    // Undo - with icon
    wxMenuItem* undoItem = editMenu->Append(wxID_UNDO, _("&Undo\tCtrl+Z"), _("Undo last action"));
    undoItem->SetBitmap(wxArtProvider::GetBitmapBundle(wxART_UNDO, wxART_MENU));

    // Redo - with icon
    wxMenuItem* redoItem = editMenu->Append(wxID_REDO, _("&Redo\tCtrl+Y"), _("Redo last undone action"));
    redoItem->SetBitmap(wxArtProvider::GetBitmapBundle(wxART_REDO, wxART_MENU));

    editMenu->AppendSeparator();

    // Cut, Copy, Paste - use system icons (common across all apps)
    wxMenuItem* cutItem = editMenu->Append(wxID_CUT, _("Cu&t\tCtrl+X"), _("Cut selection to clipboard"));
    cutItem->SetBitmap(wxArtProvider::GetBitmapBundle(wxART_CUT, wxART_MENU));

    wxMenuItem* copyItem = editMenu->Append(wxID_COPY, _("&Copy\tCtrl+C"), _("Copy selection to clipboard"));
    copyItem->SetBitmap(wxArtProvider::GetBitmapBundle(wxART_COPY, wxART_MENU));

    wxMenuItem* pasteItem = editMenu->Append(wxID_PASTE, _("&Paste\tCtrl+V"), _("Paste from clipboard"));
    pasteItem->SetBitmap(wxArtProvider::GetBitmapBundle(wxART_PASTE, wxART_MENU));

    editMenu->AppendSeparator();

    // Select All
    editMenu->Append(wxID_SELECTALL, _("Select &All\tCtrl+A"), _("Select all text"));

    m_menuBar->Append(editMenu, _("&Edit"));

    // ------------------------------------------------------------------------
    // Format Menu (Task #00014 - Rich text formatting)
    // ------------------------------------------------------------------------
    wxMenu* formatMenu = new wxMenu();

    formatMenu->Append(ID_FORMAT_BOLD, _("&Bold\tCtrl+B"), _("Toggle bold formatting"));
    formatMenu->Append(ID_FORMAT_ITALIC, _("&Italic\tCtrl+I"), _("Toggle italic formatting"));
    formatMenu->Append(ID_FORMAT_UNDERLINE, _("&Underline\tCtrl+U"), _("Toggle underline"));

    formatMenu->AppendSeparator();

    formatMenu->Append(ID_FORMAT_FONT, _("&Font..."), _("Choose font and size"));

    formatMenu->AppendSeparator();

    formatMenu->Append(ID_FORMAT_CLEAR, _("&Clear Formatting"), _("Remove all formatting"));

    m_menuBar->Append(formatMenu, _("F&ormat"));

    // ------------------------------------------------------------------------
    // View Menu (Task #00013 - Panel visibility and perspectives)
    // ------------------------------------------------------------------------
    wxMenu* viewMenu = new wxMenu();

    // Panel visibility toggles (checkable items)
    m_viewNavigatorItem = viewMenu->AppendCheckItem(ID_VIEW_NAVIGATOR, _("&Navigator\tF9"), _("Show/hide Navigator panel"));
    m_viewPropertiesItem = viewMenu->AppendCheckItem(ID_VIEW_PROPERTIES, _("&Properties\tF10"), _("Show/hide Properties panel"));
    m_viewStatisticsItem = viewMenu->AppendCheckItem(ID_VIEW_STATISTICS, _("&Statistics\tF11"), _("Show/hide Statistics panel"));
    m_viewSearchItem = viewMenu->AppendCheckItem(ID_VIEW_SEARCH, _("S&earch\tF12"), _("Show/hide Search panel"));
    m_viewAssistantItem = viewMenu->AppendCheckItem(ID_VIEW_ASSISTANT, _("&Assistant"), _("Show/hide AI Assistant panel"));

    // Separator before diagnostic-mode-only panels
    viewMenu->AppendSeparator();

    // Diagnostic Log Panel (only visible when diagnostic mode enabled)
    m_viewLogItem = viewMenu->AppendCheckItem(ID_VIEW_LOG, _("Diagnostic &Log"), _("Show/hide Diagnostic Log panel"));
    m_viewLogItem->Enable(m_diagnosticMode);  // Only enable when diagnostic mode active

    viewMenu->AppendSeparator();

    // Editor Mode submenu (Task #00019)
    wxMenu* editorModeMenu = new wxMenu();
    m_viewModeFullItem = editorModeMenu->AppendRadioItem(ID_VIEW_MODE_FULL,
        _("&Full View\tCtrl+1"), _("Continuous text view (MVP)"));
    m_viewModePageItem = editorModeMenu->AppendRadioItem(ID_VIEW_MODE_PAGE,
        _("&Page View\tCtrl+2"), _("MS Word-like page view (Task #00020)"));
    m_viewModeTypewriterItem = editorModeMenu->AppendRadioItem(ID_VIEW_MODE_TYPEWRITER,
        _("&Typewriter Mode\tCtrl+3"), _("Immersive writing mode (Task #00021)"));
    m_viewModePublisherItem = editorModeMenu->AppendRadioItem(ID_VIEW_MODE_PUBLISHER,
        _("P&ublisher View\tCtrl+4"), _("Manuscript format (Task #00022)"));

    // Enable only Full View (MVP limitation)
    m_viewModeFullItem->Check(true);
    m_viewModePageItem->Enable(false);
    m_viewModeTypewriterItem->Enable(false);
    m_viewModePublisherItem->Enable(false);

    viewMenu->AppendSubMenu(editorModeMenu, _("Editor &Mode"),
        _("Switch editor rendering mode"));

    viewMenu->AppendSeparator();

    // Perspectives submenu
    m_perspectivesMenu = new wxMenu();
    m_perspectivesMenu->Append(ID_PERSPECTIVE_DEFAULT, _("&Default"), _("Load default perspective"));
    m_perspectivesMenu->Append(ID_PERSPECTIVE_WRITING, _("&Writing"), _("Load writing perspective (Navigator + Editor)"));
    m_perspectivesMenu->Append(ID_PERSPECTIVE_EDITING, _("&Editing"), _("Load editing perspective (Editor + Statistics + Search)"));
    m_perspectivesMenu->Append(ID_PERSPECTIVE_RESEARCH, _("&Research"), _("Load research perspective (All panels + Assistant)"));

    // Dynamic custom perspectives will be inserted here by refreshPerspectivesMenu()

    m_perspectivesMenu->AppendSeparator();
    m_perspectivesMenu->Append(ID_PERSPECTIVE_SAVE, _("&Save Perspective..."), _("Save current layout as a new perspective"));
    m_perspectivesMenu->Append(ID_PERSPECTIVE_MANAGE, _("&Manage Perspectives..."), _("Manage saved perspectives"));

    viewMenu->AppendSubMenu(m_perspectivesMenu, _("&Perspectives"), _("Load or manage panel layout perspectives"));

    // Initialize with custom perspectives (if any)
    refreshPerspectivesMenu();

    m_menuBar->Append(viewMenu, _("&View"));

    // ------------------------------------------------------------------------
    // Diagnostics Menu (ONLY in diagnostic mode)
    // ------------------------------------------------------------------------
    if (m_diagnosticMode) {
        wxMenu* diagMenu = new wxMenu();
        diagMenu->Append(ID_DIAG_TEST_PYTHON,
                        _("Test &Python Integration"),
                        _("Run Python integration tests"));
        diagMenu->Append(ID_DIAG_TEST_PYBIND11,
                        _("Test Python &Bindings (pybind11)"),
                        _("Test kalahari_api module and pybind11 integration"));
        diagMenu->Append(ID_DIAG_TEST_PLUGINS,
                        _("Test Plugin &System"),
                        _("Test plugin discovery, loading, and lifecycle"));
        diagMenu->AppendSeparator();
        diagMenu->Append(ID_DIAG_OPEN_LOGS,
                        _("Open &Log Folder"),
                        _("Open folder containing application log files"));
        diagMenu->Append(ID_DIAG_SYSTEM_INFO,
                        _("&System Information"),
                        _("Display system and application information"));

        m_menuBar->Append(diagMenu, _("&Diagnostics"));

        core::Logger::getInstance().debug("Diagnostics menu created (diagnostic mode)");
    }

    // ------------------------------------------------------------------------
    // Help Menu
    // ------------------------------------------------------------------------
    wxMenu* helpMenu = new wxMenu();

    // Help Contents - with icon
    wxMenuItem* helpItem = helpMenu->Append(wxID_HELP, _("&Contents\tF1"), _("Show help contents"));
    helpItem->SetBitmap(wxArtProvider::GetBitmapBundle(wxART_HELP, wxART_MENU));

    helpMenu->AppendSeparator();

    // About - with icon
    wxMenuItem* aboutItem = helpMenu->Append(wxID_ABOUT, _("&About Kalahari"), _("About this application"));
    aboutItem->SetBitmap(wxArtProvider::GetBitmapBundle(wxART_INFORMATION, wxART_MENU));

    m_menuBar->Append(helpMenu, _("&Help"));

    // Set the menu bar
    SetMenuBar(m_menuBar);

    int menuCount = m_menuBar->GetMenuCount();
    core::Logger::getInstance().debug("Menu bar created with {} menus", menuCount);
}

void MainWindow::createMenuBarDynamic() {
    core::Logger::getInstance().debug("Creating menu bar dynamically from CommandRegistry...");

    // Create MenuBuilder and get CommandRegistry instance
    MenuBuilder builder;
    CommandRegistry& registry = CommandRegistry::getInstance();

    // Build basic menus (File, Edit, Format, View, Help) with event binding to this window
    m_menuBar = builder.buildMenuBar(registry, this);

    // ========================================================================
    // Special Menu Handling (not yet in CommandRegistry)
    // ========================================================================

    // View menu - need to add Editor Mode submenu and Perspectives submenu manually
    // (These are dynamic/complex menus not suitable for CommandRegistry in MVP)
    wxMenu* viewMenu = nullptr;
    for (size_t i = 0; i < m_menuBar->GetMenuCount(); ++i) {
        if (m_menuBar->GetMenuLabelText(i) == "View") {
            viewMenu = m_menuBar->GetMenu(i);
            break;
        }
    }

    if (viewMenu) {
        // Add separator before submenus
        viewMenu->AppendSeparator();

        // Editor Mode submenu (Task #00019)
        wxMenu* modeMenu = new wxMenu();
        m_viewModeFullItem = modeMenu->AppendRadioItem(wxID_ANY, _("&Full Mode\tF11"),
            _("Full screen distraction-free mode"));
        m_viewModePageItem = modeMenu->AppendRadioItem(wxID_ANY, _("&Page Mode\tF12"),
            _("Traditional page-based editor"));
        m_viewModeTypewriterItem = modeMenu->AppendRadioItem(wxID_ANY, _("&Typewriter Mode"),
            _("Keep cursor centered like a typewriter"));
        m_viewModePublisherItem = modeMenu->AppendRadioItem(wxID_ANY, _("P&ublisher Mode"),
            _("Two-column layout for final review"));
        m_viewModeFullItem->Check();  // Default to Full Mode
        viewMenu->AppendSubMenu(modeMenu, _("Editor &Mode"), _("Switch editor layout mode"));

        // Perspectives submenu
        m_perspectivesMenu = new wxMenu();
        m_perspectivesMenu->Append(wxID_ANY, _("&Default"), _("Reset to default panel layout"));
        m_perspectivesMenu->Append(wxID_ANY, _("&Writing"), _("Minimal layout for focused writing"));
        m_perspectivesMenu->Append(wxID_ANY, _("&Editing"), _("Layout optimized for editing and revision"));
        m_perspectivesMenu->Append(wxID_ANY, _("&Research"), _("Layout with research and reference panels"));
        m_perspectivesMenu->AppendSeparator();
        m_perspectivesMenu->Append(wxID_ANY, _("&Save Perspective..."), _("Save current layout as custom perspective"));
        m_perspectivesMenu->Append(wxID_ANY, _("&Manage Perspectives..."), _("Manage saved perspectives"));
        viewMenu->AppendSubMenu(m_perspectivesMenu, _("&Perspectives"), _("Load or save panel layouts"));
    }

    // Diagnostics menu (conditional on diagnostic mode)
    if (m_diagnosticMode) {
        wxMenu* diagMenu = new wxMenu();
        diagMenu->Append(wxID_ANY, _("Test &Python Integration"), _("Run Python interpreter test"));
        diagMenu->Append(wxID_ANY, _("Test Python &Bindings (pybind11)"), _("Test C++/Python interop"));
        diagMenu->AppendSeparator();
        diagMenu->Append(wxID_ANY, _("Test &Plugin System"), _("Load and test plugin discovery"));
        diagMenu->AppendSeparator();
        diagMenu->Append(wxID_ANY, _("&System Information"), _("Display system and build info"));
        diagMenu->Append(wxID_ANY, _("Open &Log Folder"), _("Open logs directory in file manager"));
        m_menuBar->Append(diagMenu, _("&Diagnostics"));
    }

    // Set menubar on frame
    SetMenuBar(m_menuBar);

    core::Logger::getInstance().info("Menu bar created dynamically (MenuBuilder)");
}

void MainWindow::createToolBar() {
    core::Logger::getInstance().debug("Creating toolbar (legacy method)...");

    // Create toolbar with horizontal orientation and text labels
    m_toolBar = CreateToolBar(wxTB_HORIZONTAL | wxTB_TEXT);
    createToolBarContent();

    core::Logger::getInstance().debug("Toolbar created with 5 tools (New, Open, Save, Undo, Redo) - legacy");
}

void MainWindow::createToolBarDynamic() {
    core::Logger::getInstance().debug("Creating toolbar dynamically from CommandRegistry...");

    // Create ToolbarBuilder and get CommandRegistry instance
    ToolbarBuilder builder;
    CommandRegistry& registry = CommandRegistry::getInstance();

    // Build toolbar from CommandRegistry (tools with showInToolbar=true)
    // ToolbarBuilder will:
    // - Query commands by category (File, Edit, Format)
    // - Add separators between categories
    // - Bind event handlers to executeCommand()
    m_toolBar = builder.buildToolBar(registry, this, this);

    core::Logger::getInstance().info("Toolbar created dynamically (ToolbarBuilder)");
}

void MainWindow::createToolBarContent() {
    if (!m_toolBar) {
        return;
    }

    // Get current icon size from IconRegistry
    IconRegistry& iconReg = IconRegistry::getInstance();
    int iconSize = iconReg.getSizes().toolbar;
    wxSize toolbarIconSize(iconSize, iconSize);

    // Add tools using stock icons from wxArtProvider (platform-native)
    m_toolBar->AddTool(wxID_NEW, _("New"),
        wxArtProvider::GetBitmap(wxART_NEW, wxART_TOOLBAR, toolbarIconSize),
        _("Create new document"));

    m_toolBar->AddTool(wxID_OPEN, _("Open"),
        wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_TOOLBAR, toolbarIconSize),
        _("Open existing document"));

    m_toolBar->AddTool(wxID_SAVE, _("Save"),
        wxArtProvider::GetBitmap(wxART_FILE_SAVE, wxART_TOOLBAR, toolbarIconSize),
        _("Save current document"));

    m_toolBar->AddSeparator();

    m_toolBar->AddTool(wxID_UNDO, _("Undo"),
        wxArtProvider::GetBitmap(wxART_UNDO, wxART_TOOLBAR, toolbarIconSize),
        _("Undo last action"));

    m_toolBar->AddTool(wxID_REDO, _("Redo"),
        wxArtProvider::GetBitmap(wxART_REDO, wxART_TOOLBAR, toolbarIconSize),
        _("Redo last undone action"));

    // Realize toolbar (required to display tools)
    m_toolBar->Realize();
}

void MainWindow::createStatusBar() {
    core::Logger::getInstance().debug("Creating status bar...");

    // Create status bar with 3 panes
    m_statusBar = CreateStatusBar(3);

    // Set pane widths:
    // - Pane 0: -1 (flexible, takes remaining space)
    // - Pane 1: 150 pixels (cursor position)
    // - Pane 2: 150 pixels (session time or other info)
    int widths[3] = { -1, 150, 150 };
    m_statusBar->SetStatusWidths(3, widths);

    // Set initial text
    m_statusBar->SetStatusText(_("Ready"), 0);
    m_statusBar->SetStatusText(_("Line 1, Col 1"), 1);
    m_statusBar->SetStatusText(_("Session: 00:00"), 2);

    core::Logger::getInstance().debug("Status bar created with 3 panes");
}

void MainWindow::setupMainPanel() {
    core::Logger::getInstance().debug("Setting up main panel...");

    // Create main panel (fills entire client area)
    m_mainPanel = new wxPanel(this, wxID_ANY);
    m_mainPanel->SetBackgroundColour(*wxWHITE);

    // Create placeholder text
    wxStaticText* placeholderText = new wxStaticText(
        m_mainPanel,
        wxID_ANY,
        _("Rich Text Editor\nwill be implemented in Phase 1\n\n"
          "Phase 0 Week 2: GUI Infrastructure\n"
          "Status: In Progress"),
        wxDefaultPosition,
        wxDefaultSize,
        wxALIGN_CENTER_HORIZONTAL
    );

    // Set larger font for placeholder
    wxFont font = placeholderText->GetFont();
    font.SetPointSize(16);
    font.SetWeight(wxFONTWEIGHT_NORMAL);
    placeholderText->SetFont(font);
    placeholderText->SetForegroundColour(wxColour(100, 100, 100));

    // Layout: center the text vertically and horizontally
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddStretchSpacer();
    sizer->Add(placeholderText, 0, wxALIGN_CENTER | wxALL, 20);
    sizer->AddStretchSpacer();

    m_mainPanel->SetSizer(sizer);

    core::Logger::getInstance().debug("Main panel setup complete (placeholder text)");
}

void MainWindow::setDiagnosticMode(bool enabled) {
    if (m_diagnosticMode == enabled) {
        core::Logger::getInstance().debug("Diagnostic mode already {}, no change needed",
                                          enabled ? "enabled" : "disabled");
        return;
    }

    m_diagnosticMode = enabled;
    core::Logger::getInstance().info("Diagnostic mode {}", enabled ? "enabled" : "disabled");

    // Clear cached menu items before rebuilding menu bar
    // (items belong to old menu and will be destroyed with it)
    m_customPerspectiveItems.clear();
    m_customPerspectiveNames.clear();

    // Rebuild menu bar to show/hide Diagnostics menu
    wxMenuBar* oldMenuBar = GetMenuBar();
    SetMenuBar(nullptr);
    createMenuBarDynamic();  // Will check m_diagnosticMode (Task #00031)
    delete oldMenuBar;

    core::Logger::getInstance().debug("Menu bar rebuilt with diagnostic mode {}",
                                      enabled ? "visible" : "hidden");

    // Dynamically create or destroy LogPanel
    if (enabled) {
        createLogPanel();
    } else {
        destroyLogPanel();
    }

    // Update View menu checkboxes to reflect new state
    updateViewMenu();
}

void MainWindow::createLogPanel() {
    // Check if already exists
    if (m_logPanel) {
        core::Logger::getInstance().debug("LogPanel already exists, skipping creation");
        return;
    }

    if (!m_auiManager) {
        core::Logger::getInstance().error("Cannot create LogPanel: wxAuiManager not initialized");
        return;
    }

    core::Logger::getInstance().info("Creating LogPanel dynamically...");

    // Create LogPanel
    m_logPanel = new LogPanel(this);

    // Add to wxAUI manager (bottom pane)
    m_auiManager->AddPane(m_logPanel,
        wxAuiPaneInfo()
            .Name("log")
            .Caption(_("Diagnostic Log"))
            .Bottom()
            .Layer(0)
            .MinSize(400, 150)
            .BestSize(800, 200)
            .CloseButton(true)
            .MaximizeButton(false)
            .PinButton(true)
            .Dockable(true)
            .Floatable(true)
            .Movable(true)
            .Show());

    m_auiManager->Update();

    // Register GUI log sink with Logger
    auto logger = core::Logger::getInstance().getLogger();
    if (logger) {
        auto gui_sink = std::make_shared<core::GuiLogSink>(m_logPanel);
        gui_sink->set_level(logger->level());
        logger->sinks().push_back(gui_sink);
        core::Logger::getInstance().info("LogPanel created and registered with logger");
    } else {
        core::Logger::getInstance().warn("Logger not available, LogPanel created without sink");
    }
}

void MainWindow::destroyLogPanel() {
    if (!m_logPanel) {
        core::Logger::getInstance().debug("LogPanel does not exist, skipping destruction");
        return;
    }

    core::Logger::getInstance().info("Destroying LogPanel...");

    // Unregister GUI sink from Logger
    auto logger = core::Logger::getInstance().getLogger();
    if (logger) {
        auto& sinks = logger->sinks();
        sinks.erase(
            std::remove_if(sinks.begin(), sinks.end(),
                [this](const spdlog::sink_ptr& sink) {
                    return std::dynamic_pointer_cast<core::GuiLogSink>(sink) != nullptr;
                }),
            sinks.end());
        core::Logger::getInstance().debug("GuiLogSink unregistered from logger");
    }

    // Remove from wxAUI manager
    if (m_auiManager) {
        m_auiManager->DetachPane(m_logPanel);
        m_auiManager->Update();
    }

    // Destroy panel
    m_logPanel->Destroy();
    m_logPanel = nullptr;

    core::Logger::getInstance().info("LogPanel destroyed");
}

// ============================================================================
// Event Handlers
// ============================================================================

void MainWindow::onFileNew([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("file.new");
}

void MainWindow::onFileOpen([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("file.open");
}

void MainWindow::onFileSave([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("file.save");
}

void MainWindow::onFileSaveAs([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("file.save_as");
}

void MainWindow::onFileSettings([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("File -> Settings clicked");

    // Prepare current state for dialog
    SettingsState currentState;
    currentState.diagnosticModeEnabled = m_diagnosticMode;
    currentState.launchedWithDiagFlag = m_launchedWithDiagFlag;

    // Load all settings from SettingsManager into currentState
    core::SettingsManager& settingsMgr = core::SettingsManager::getInstance();

    // Load Editor settings (Task #00019)
    currentState.caretBlinkEnabled = settingsMgr.get<bool>("editor.caretBlinkEnabled", true);
    currentState.caretBlinkRate = settingsMgr.get<int>("editor.caretBlinkRate", 500);
    currentState.caretWidth = settingsMgr.get<int>("editor.caretWidth", 1);
    currentState.marginLeft = settingsMgr.get<int>("editor.marginLeft", 20);
    currentState.marginRight = settingsMgr.get<int>("editor.marginRight", 20);
    currentState.marginTop = settingsMgr.get<int>("editor.marginTop", 10);
    currentState.marginBottom = settingsMgr.get<int>("editor.marginBottom", 10);
    currentState.lineSpacing = settingsMgr.get<double>("editor.lineSpacing", 1.2);
    currentState.selectionOpacity = settingsMgr.get<int>("editor.selectionOpacity", 128);
    currentState.selectionColor = wxColour(
        settingsMgr.get<int>("editor.selectionColor.r", 0),
        settingsMgr.get<int>("editor.selectionColor.g", 120),
        settingsMgr.get<int>("editor.selectionColor.b", 215)
    );
    currentState.antialiasing = settingsMgr.get<bool>("editor.antialiasing", true);
    currentState.autoFocus = settingsMgr.get<bool>("editor.autoFocus", true);
    currentState.wordWrap = settingsMgr.get<bool>("editor.wordWrap", true);
    currentState.undoLimit = settingsMgr.get<int>("editor.undoLimit", 100);

    // Load Appearance settings (Task #00020 - Option C)
    currentState.themeName = wxString::FromUTF8(
        settingsMgr.get<std::string>("appearance.theme", "System")
    );
    currentState.iconSize = settingsMgr.get<int>("appearance.iconSize", 24);
    currentState.fontScaling = settingsMgr.get<double>("appearance.fontScaling", 1.0);

    // Load Log settings (Task #00020)
    currentState.logBufferSize = settingsMgr.get<int>("log.bufferSize", 500);
    currentState.logBackgroundColor = wxColour(
        settingsMgr.get<int>("log.backgroundColor.r", 60),
        settingsMgr.get<int>("log.backgroundColor.g", 60),
        settingsMgr.get<int>("log.backgroundColor.b", 60)
    );
    currentState.logTextColor = wxColour(
        settingsMgr.get<int>("log.textColor.r", 255),
        settingsMgr.get<int>("log.textColor.g", 255),
        settingsMgr.get<int>("log.textColor.b", 255)
    );
    currentState.logFontSize = settingsMgr.get<int>("log.fontSize", 11);

    core::Logger::getInstance().debug("Loaded settings for dialog (theme='{}')",
        currentState.themeName.ToStdString());

    // Show dialog
    SettingsDialog dlg(this, currentState);
    if (dlg.ShowModal() == wxID_OK) {
        SettingsState newState = dlg.getNewState();

        // Apply diagnostic mode change if changed
        if (newState.diagnosticModeEnabled != m_diagnosticMode) {
            setDiagnosticMode(newState.diagnosticModeEnabled);
        }

        // ====================================================================
        // Phase 1: Save Editor Settings to SettingsManager (Task #00019)
        // ====================================================================

        // Margins & Padding
        settingsMgr.set("editor.marginLeft", newState.marginLeft);
        settingsMgr.set("editor.marginRight", newState.marginRight);
        settingsMgr.set("editor.marginTop", newState.marginTop);
        settingsMgr.set("editor.marginBottom", newState.marginBottom);

        // Rendering
        settingsMgr.set("editor.lineSpacing", newState.lineSpacing);
        settingsMgr.set("editor.selectionOpacity", newState.selectionOpacity);
        settingsMgr.set("editor.selectionColor.r", (int)newState.selectionColor.Red());
        settingsMgr.set("editor.selectionColor.g", (int)newState.selectionColor.Green());
        settingsMgr.set("editor.selectionColor.b", (int)newState.selectionColor.Blue());
        settingsMgr.set("editor.antialiasing", newState.antialiasing);

        // Behavior
        settingsMgr.set("editor.autoFocus", newState.autoFocus);
        settingsMgr.set("editor.wordWrap", newState.wordWrap);
        settingsMgr.set("editor.undoLimit", newState.undoLimit);

        // Caret settings
        settingsMgr.set("editor.caretBlinkEnabled", newState.caretBlinkEnabled);
        settingsMgr.set("editor.caretBlinkRate", newState.caretBlinkRate);
        settingsMgr.set("editor.caretWidth", newState.caretWidth);

        // ====================================================================
        // Phase 1: Save Appearance Settings to SettingsManager (Task #00020 - Option C)
        // ====================================================================

        settingsMgr.set("appearance.theme", newState.themeName.ToStdString());
        settingsMgr.set("appearance.iconSize", newState.iconSize);
        settingsMgr.set("appearance.fontScaling", newState.fontScaling);

        core::Logger::getInstance().info("Appearance settings saved (theme={}, iconSize={}, fontScaling={})",
            newState.themeName.ToStdString(), newState.iconSize, newState.fontScaling);

        // ====================================================================
        // Phase 1: Save Log Settings to SettingsManager (Task #00020)
        // ====================================================================

        settingsMgr.set("log.bufferSize", newState.logBufferSize);
        settingsMgr.set("log.backgroundColor.r", (int)newState.logBackgroundColor.Red());
        settingsMgr.set("log.backgroundColor.g", (int)newState.logBackgroundColor.Green());
        settingsMgr.set("log.backgroundColor.b", (int)newState.logBackgroundColor.Blue());
        settingsMgr.set("log.textColor.r", (int)newState.logTextColor.Red());
        settingsMgr.set("log.textColor.g", (int)newState.logTextColor.Green());
        settingsMgr.set("log.textColor.b", (int)newState.logTextColor.Blue());
        settingsMgr.set("log.fontSize", newState.logFontSize);

        core::Logger::getInstance().info("Log settings saved (bufferSize={}, fontSize={})",
            newState.logBufferSize, newState.logFontSize);

        // Save to JSON file
        if (!settingsMgr.save()) {
            core::Logger::getInstance().error("Failed to save settings to file!");
            wxMessageBox(
                "Failed to save settings to file. Changes may not persist.",
                "Settings Save Error",
                wxOK | wxICON_ERROR,
                this
            );
        }

        // Apply settings to EditorPanel (live update without restart)
        if (m_editorPanel) {
            m_editorPanel->applySettings();
            core::Logger::getInstance().info("Editor settings applied to EditorPanel");
        }

        // Apply settings to LogPanel (live update without restart)
        if (m_logPanel) {
            m_logPanel->applySettings();
            core::Logger::getInstance().info("Log settings applied to LogPanel");
        }

        m_statusBar->SetStatusText(_("Settings saved"), 0);
    } else {
        core::Logger::getInstance().debug("Settings dialog cancelled by user");
    }
}

void MainWindow::onSettingsApplied(SettingsAppliedEvent& event) {
    core::Logger::getInstance().info("Settings -> Apply clicked - applying settings immediately");

    SettingsState newState = event.getNewState();
    core::SettingsManager& settingsMgr = core::SettingsManager::getInstance();

    // Apply diagnostic mode change if changed
    if (newState.diagnosticModeEnabled != m_diagnosticMode) {
        setDiagnosticMode(newState.diagnosticModeEnabled);
    }

    // ====================================================================
    // Save Editor Settings to SettingsManager
    // ====================================================================

    // Margins & Padding
    settingsMgr.set("editor.marginLeft", newState.marginLeft);
    settingsMgr.set("editor.marginRight", newState.marginRight);
    settingsMgr.set("editor.marginTop", newState.marginTop);
    settingsMgr.set("editor.marginBottom", newState.marginBottom);

    // Rendering
    settingsMgr.set("editor.lineSpacing", newState.lineSpacing);
    settingsMgr.set("editor.selectionOpacity", newState.selectionOpacity);
    settingsMgr.set("editor.selectionColor.r", (int)newState.selectionColor.Red());
    settingsMgr.set("editor.selectionColor.g", (int)newState.selectionColor.Green());
    settingsMgr.set("editor.selectionColor.b", (int)newState.selectionColor.Blue());
    settingsMgr.set("editor.antialiasing", newState.antialiasing);

    // Behavior
    settingsMgr.set("editor.autoFocus", newState.autoFocus);
    settingsMgr.set("editor.wordWrap", newState.wordWrap);
    settingsMgr.set("editor.undoLimit", newState.undoLimit);

    // Caret settings
    settingsMgr.set("editor.caretBlinkEnabled", newState.caretBlinkEnabled);
    settingsMgr.set("editor.caretBlinkRate", newState.caretBlinkRate);
    settingsMgr.set("editor.caretWidth", newState.caretWidth);

    // ====================================================================
    // Save Appearance Settings to SettingsManager
    // ====================================================================

    settingsMgr.set("appearance.theme", newState.themeName.ToStdString());
    settingsMgr.set("appearance.iconSize", newState.iconSize);
    settingsMgr.set("appearance.fontScaling", newState.fontScaling);

    core::Logger::getInstance().info("Appearance settings saved (theme={}, iconSize={}, fontScaling={})",
        newState.themeName.ToStdString(), newState.iconSize, newState.fontScaling);

    // Apply icon size changes immediately
    IconRegistry& iconReg = IconRegistry::getInstance();
    IconSizeConfig newSizes = iconReg.getSizes();

    // Map appearance.iconSize to all icon contexts
    // User setting controls all icon sizes proportionally
    newSizes.toolbar = newState.iconSize;
    newSizes.menu = static_cast<int>(newState.iconSize * 0.67);  // Slightly smaller for menus
    newSizes.panel = static_cast<int>(newState.iconSize * 0.83); // Medium size for panels
    newSizes.dialog = static_cast<int>(newState.iconSize * 1.33); // Larger for dialogs

    iconReg.setSizes(newSizes);
    iconReg.saveToSettings();

    core::Logger::getInstance().info("Icon sizes updated: toolbar={}, menu={}, panel={}, dialog={}",
        newSizes.toolbar, newSizes.menu, newSizes.panel, newSizes.dialog);

    // Reload toolbar with new icon sizes
    if (m_toolBar) {
        core::Logger::getInstance().debug("Reloading toolbar with new icon size: {}", newState.iconSize);

        // Destroy existing toolbar
        wxToolBar* oldToolBar = m_toolBar;
        SetToolBar(nullptr);
        m_toolBar = nullptr;
        oldToolBar->Destroy();

        // Recreate toolbar with new icon sizes
        m_toolBar = CreateToolBar(wxTB_HORIZONTAL | wxTB_TEXT);
        createToolBarContent();

        // Refresh layout
        Layout();

        core::Logger::getInstance().info("Toolbar reloaded successfully with icon size: {}", newState.iconSize);
    }

    // ====================================================================
    // Save Log Settings to SettingsManager
    // ====================================================================

    settingsMgr.set("log.bufferSize", newState.logBufferSize);
    settingsMgr.set("log.backgroundColor.r", (int)newState.logBackgroundColor.Red());
    settingsMgr.set("log.backgroundColor.g", (int)newState.logBackgroundColor.Green());
    settingsMgr.set("log.backgroundColor.b", (int)newState.logBackgroundColor.Blue());
    settingsMgr.set("log.textColor.r", (int)newState.logTextColor.Red());
    settingsMgr.set("log.textColor.g", (int)newState.logTextColor.Green());
    settingsMgr.set("log.textColor.b", (int)newState.logTextColor.Blue());
    settingsMgr.set("log.fontSize", newState.logFontSize);

    core::Logger::getInstance().info("Log settings saved (bufferSize={}, fontSize={})",
        newState.logBufferSize, newState.logFontSize);

    // Save to JSON file
    if (!settingsMgr.save()) {
        core::Logger::getInstance().error("Failed to save settings to file!");
        wxMessageBox(
            "Failed to save settings to file. Changes may not persist.",
            "Settings Save Error",
            wxOK | wxICON_ERROR,
            this
        );
        return;
    }

    // Apply settings to EditorPanel (live update without restart)
    if (m_editorPanel) {
        m_editorPanel->applySettings();
        core::Logger::getInstance().info("Editor settings applied to EditorPanel");
    }

    // Apply settings to LogPanel (live update without restart)
    if (m_logPanel) {
        m_logPanel->applySettings();
        core::Logger::getInstance().info("Log settings applied to LogPanel");
    }

    // ====================================================================
    // Check if theme was changed - show restart dialog
    // ====================================================================

    // Get current theme from SettingsManager (saved value before this Apply)
    std::string currentTheme = settingsMgr.get<std::string>("appearance.theme", "System");

    if (newState.themeName.ToStdString() != currentTheme) {
        core::Logger::getInstance().info("Theme changed from '{}' to '{}' - showing restart dialog",
            currentTheme, newState.themeName.ToStdString());

        wxMessageDialog restartDialog(
            this,
            _("Theme changes require application restart to take full effect.\n\n"
              "Would you like to restart Kalahari now?\n\n"
              "Note: Any unsaved changes will be lost."),
            _("Restart Required"),
            wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION
        );

        restartDialog.SetYesNoLabels(_("Restart Now"), _("Restart Later"));

        int result = restartDialog.ShowModal();

        if (result == wxID_YES) {
            core::Logger::getInstance().info("User chose to restart application now");

            // TODO Phase 1: Implement proper application restart mechanism
            // For now, inform user to restart manually and close application
            wxMessageBox(
                _("Please restart Kalahari manually to apply the new theme.\n\n"
                  "The application will now close."),
                _("Manual Restart Required"),
                wxOK | wxICON_INFORMATION,
                this
            );

            Close(true);  // Force close
            return;
        } else {
            core::Logger::getInstance().info("User chose to restart later");
        }
    }

    m_statusBar->SetStatusText(_("Settings applied"), 0);
    core::Logger::getInstance().info("All settings applied successfully via Apply button");
}

void MainWindow::onFileExit([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("file.exit");
}

void MainWindow::onEditUndo([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("edit.undo");
}

void MainWindow::onEditRedo([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("edit.redo");
}

void MainWindow::onEditCut([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("edit.cut");
}

void MainWindow::onEditCopy([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("edit.copy");
}

void MainWindow::onEditPaste([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("edit.paste");
}

void MainWindow::onEditSelectAll([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("edit.select_all");
}

// ============================================================================
// Format Menu Event Handlers (Task #00014)
// ============================================================================

void MainWindow::onFormatBold([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("format.bold");
}

void MainWindow::onFormatItalic([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("format.italic");
}

void MainWindow::onFormatUnderline([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("format.underline");
}

void MainWindow::onFormatFont([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("format.font");
}

void MainWindow::onFormatClear([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("format.clear_formatting");
}

void MainWindow::onHelpAbout([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("Help -> About clicked");

    // Show custom About dialog with tabs (Task #00013)
    AboutDialog dlg(this);
    dlg.ShowModal();
}

void MainWindow::onDiagnosticsTestPython([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("Diagnostics -> Test Python Integration clicked");

    // Get PythonInterpreter instance
    auto& python = core::PythonInterpreter::getInstance();

    // Check if Python is initialized
    if (!python.isInitialized()) {
        wxMessageBox(
            _("Python interpreter is not initialized.\n\n"
              "Plugin system is unavailable."),
            _("Python Not Available"),
            wxOK | wxICON_ERROR,
            this
        );
        return;
    }

    // Run Python integration tests
    m_statusBar->SetStatusText(_("Running Python tests..."), 0);

    std::string testResults = python.executeTest();

    // Display results in message box
    wxMessageBox(
        wxString::FromUTF8(testResults),
        _("Python Integration Test Results"),
        wxOK | wxICON_INFORMATION,
        this
    );

    m_statusBar->SetStatusText(_("Python tests completed"), 0);

    core::Logger::getInstance().info("Python integration tests completed successfully");
}

void MainWindow::onDiagnosticsTestPyBind11([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("Diagnostics -> Test Python Bindings (pybind11) clicked");

    m_statusBar->SetStatusText(_("Testing pybind11 bindings..."), 0);

    // Prepare test report
    std::string report = "Testing kalahari_api (pybind11 Module)\n";
    report += "=====================================\n\n";

    // Test 1: Check if Python is initialized
    auto& python = core::PythonInterpreter::getInstance();
    if (!python.isInitialized()) {
        report += "FAILED: Python interpreter is not initialized.\n";
        report += "\nPlugin system is unavailable.\n";

        wxMessageBox(
            wxString::FromUTF8(report),
            _("PyBind11 Test Results - FAILED"),
            wxOK | wxICON_ERROR,
            this
        );

        m_statusBar->SetStatusText(_("PyBind11 tests failed"), 0);
        return;
    }

    report += "✓ Python interpreter initialized\n\n";
    report += "✓ kalahari_api module should be available\n";
    report += "✓ Logger bindings configured\n";
    report += "\nNote: Full test output in application logs.\n";
    report += "Run: python3 ../tests/test_python_bindings.py\n";

    wxMessageBox(
        wxString::FromUTF8(report),
        _("PyBind11 Test Results - READY"),
        wxOK | wxICON_INFORMATION,
        this
    );

    m_statusBar->SetStatusText(_("PyBind11 tests completed"), 0);
    core::Logger::getInstance().info("PyBind11 tests completed successfully");
}

void MainWindow::onDiagnosticsOpenLogs([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("Diagnostics -> Open Log Folder clicked");

    // Get log directory path
    wxStandardPaths& stdPaths = wxStandardPaths::Get();
    wxString userDataDir = stdPaths.GetUserDataDir();
    wxString logDir = userDataDir + wxFileName::GetPathSeparator() + "logs";

    // Check if log directory exists
    if (!wxFileName::DirExists(logDir)) {
        wxMessageBox(
            _("Log folder does not exist yet.\n\n"
              "Logs will be created when the application runs."),
            _("Log Folder Not Found"),
            wxOK | wxICON_WARNING,
            this
        );
        return;
    }

    // Open folder in system file explorer
    // wxEXEC_MAKE_GROUP_LEADER: Detach from parent process group to prevent
    // shell from waiting for file manager to close
#ifdef _WIN32
    wxExecute("explorer \"" + logDir + "\"", wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER);
#elif defined(__APPLE__)
    wxExecute("open \"" + logDir + "\"", wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER);
#else  // Linux
    wxExecute("xdg-open \"" + logDir + "\"", wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER);
#endif

    core::Logger::getInstance().info("Opened log folder: {}", logDir.utf8_str().data());
}

void MainWindow::onDiagnosticsSystemInfo([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("Diagnostics -> System Information clicked");

    // Gather system information
    wxString info;
    info += "Kalahari Writer's IDE\n";
    info += "Version: 0.0.1-alpha (Phase 0 Week 2)\n\n";

    info += "System Information:\n";
    info += "- OS: " + wxGetOsDescription() + "\n";
    info += "- Architecture: " + wxString(wxPlatformInfo::Get().GetBitnessName()) + "\n";

    info += "\nwxWidgets Information:\n";
    info += "- Version: " + wxString(wxVERSION_STRING) + "\n";
    info += "- Toolkit: " + wxString(wxPlatformInfo::Get().GetPortIdShortName()) + "\n";

    info += "\nPython Information:\n";
    auto& python = core::PythonInterpreter::getInstance();
    if (python.isInitialized()) {
        std::string pyVersion = python.getPythonVersion();
        info += "- Version: " + wxString::FromUTF8(pyVersion.substr(0, 20)) + "...\n";
        info += "- Home: " + wxString(python.getPythonHome().string()) + "\n";
    } else {
        info += "- Status: Not initialized\n";
    }

    info += "\nBuild Information:\n";
#ifdef _DEBUG
    info += "- Build Type: Debug\n";
#else
    info += "- Build Type: Release\n";
#endif

#ifdef _WIN32
    info += "- Platform: Windows\n";
#elif defined(__APPLE__)
    info += "- Platform: macOS\n";
#else
    info += "- Platform: Linux\n";
#endif

    // Display system information
    wxMessageBox(
        info,
        _("System Information"),
        wxOK | wxICON_INFORMATION,
        this
    );

    core::Logger::getInstance().info("Displayed system information");
}

void MainWindow::onDiagnosticsTestPlugins([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("Diagnostics -> Test Plugin System clicked");

    m_statusBar->SetStatusText(_("Testing plugin system..."), 0);

    // Prepare test report
    std::string report = "Plugin System Test Report\n";
    report += "=========================\n\n";

    // Test 1: Check if Python is initialized
    auto& python = core::PythonInterpreter::getInstance();
    if (!python.isInitialized()) {
        report += "FAILED: Python interpreter is not initialized.\n";
        report += "\nPlugin system requires Python to be running.\n";

        wxMessageBox(
            wxString::FromUTF8(report),
            _("Plugin System Test - FAILED"),
            wxOK | wxICON_ERROR,
            this
        );

        m_statusBar->SetStatusText(_("Plugin tests failed"), 0);
        return;
    }

    report += "✓ Python interpreter initialized\n\n";

    // Test 2: Discover plugins
    auto& pluginMgr = core::PluginManager::getInstance();

    try {
        pluginMgr.discoverPlugins();
        auto plugins = pluginMgr.getDiscoveredPlugins();

        report += "Discovery:\n";
        report += "✓ Found " + std::to_string(plugins.size()) + " plugin(s) in plugins/\n\n";

        if (plugins.empty()) {
            report += "No plugins found to test.\n";
            report += "Place .kplugin files in the plugins/ directory.\n";

            wxMessageBox(
                wxString::FromUTF8(report),
                _("Plugin System Test - No Plugins"),
                wxOK | wxICON_INFORMATION,
                this
            );

            m_statusBar->SetStatusText(_("No plugins to test"), 0);
            return;
        }

        // Test 3: Load each plugin
        bool allTestsPassed = true;
        for (const auto& metadata : plugins) {
            report += "Plugin: " + metadata.manifest.name + " (" + metadata.manifest.id + ")\n";
            report += "- Version: " + metadata.manifest.version + "\n";
            report += "- Entry Point: " + metadata.manifest.entry_point + "\n";
            report += "- Status: Attempting to load...\n";

            // Try to load the plugin
            bool loadSuccess = pluginMgr.loadPlugin(metadata.manifest.id);

            if (loadSuccess) {
                report += "  ✓ Plugin loaded successfully\n";
                report += "  ✓ on_init() called\n";
                report += "  ✓ on_activate() called\n";

                // Try to unload the plugin
                pluginMgr.unloadPlugin(metadata.manifest.id);
                report += "  ✓ on_deactivate() called\n";
                report += "  ✓ Cleanup completed\n";
            } else {
                report += "  ✗ Failed to load plugin\n";
                allTestsPassed = false;
            }

            report += "\n";
        }

        // Final result
        report += "Result: ";
        if (allTestsPassed) {
            report += "✅ ALL TESTS PASSED\n";
            report += "\nPlugin system is working correctly!\n";
        } else {
            report += "⚠️ SOME TESTS FAILED\n";
            report += "\nCheck application logs for details.\n";
        }

        wxMessageBox(
            wxString::FromUTF8(report),
            allTestsPassed ? _("Plugin System Test - SUCCESS") : _("Plugin System Test - PARTIAL"),
            wxOK | (allTestsPassed ? wxICON_INFORMATION : wxICON_WARNING),
            this
        );

        m_statusBar->SetStatusText(
            allTestsPassed ? _("Plugin tests passed") : _("Plugin tests completed with warnings"),
            0
        );

    } catch (const std::exception& e) {
        report += "\n✗ EXCEPTION: " + std::string(e.what()) + "\n";

        wxMessageBox(
            wxString::FromUTF8(report),
            _("Plugin System Test - ERROR"),
            wxOK | wxICON_ERROR,
            this
        );

        m_statusBar->SetStatusText(_("Plugin tests failed"), 0);
        core::Logger::getInstance().error("Plugin system test failed with exception: {}", e.what());
        return;
    }

    core::Logger::getInstance().info("Plugin system tests completed");
}

void MainWindow::onClose(wxCloseEvent& event) {
    core::Logger::getInstance().info("Window closing (user initiated)");

    // TODO Phase 1: Check for unsaved changes
    // TODO Phase 1: Prompt user if needed

    // Save window position/size (Task #00003)
    auto& settings = core::SettingsManager::getInstance();

    wxPoint pos = GetPosition();
    wxSize size = GetSize();
    bool maximized = IsMaximized();

    settings.setWindowPosition(pos);
    settings.setWindowSize(size);
    settings.setWindowMaximized(maximized);

    settings.save();

    core::Logger::getInstance().info("Window state saved: {}x{} at ({}, {}), maximized: {}",
                                     size.GetWidth(), size.GetHeight(),
                                     pos.x, pos.y, maximized);

    m_statusBar->SetStatusText(_("Closing..."), 0);

    // Allow window to close
    event.Skip();
}

// ============================================================================
// Threading Event Handlers (Phase 0 Week 2)
// ============================================================================

void MainWindow::onTaskCompleted(wxThreadEvent& event) {
    int threadId = event.GetId();
    core::Logger::getInstance().info("Background task completed (thread ID: {})", threadId);

    // Update status bar
    m_statusBar->SetStatusText(_("Ready"), 0);

    // Future Phase 1+: Handle task results from event.GetPayload()
    // Update document state, refresh UI, etc.
}

void MainWindow::onTaskFailed(wxThreadEvent& event) {
    int threadId = event.GetId();
    wxString error = event.GetString();

    core::Logger::getInstance().error("Background task failed (thread ID: {}): {}",
                                      threadId, error.utf8_str().data());

    // Show error to user
    m_statusBar->SetStatusText(_("Error - check logs"), 0);
    wxLogError("Background task failed: %s", error);

    // Future Phase 1+: Show error dialog with details
}

// ============================================================================
// Threading Methods (Phase 0 Week 2)
// ============================================================================

bool MainWindow::submitBackgroundTask(std::function<void()> task) {
    // Check thread limit (Bartosz's semaphore pattern)
    {
        wxMutexLocker lock(m_threadMutex);
        wxSemaError serr = m_threadSemaphore.TryWait();
        if (serr != wxSEMA_NO_ERROR) {
            core::Logger::getInstance().warn(
                "Background task rejected: thread limit reached (4/4 active)");

            // Notify user via status bar (CallAfter pattern for GUI update from any thread)
            CallAfter([this]() {
                m_statusBar->SetStatusText(_("Busy - please wait..."), 0);
            });

            return false;
        }
    }

    core::Logger::getInstance().debug("Submitting background task (thread pool: {}/4)",
                                      m_activeThreads.size() + 1);

    // Create worker thread (std::thread, NOT wxThread)
    std::thread worker([this, task]() {
        // Track this thread
        std::thread::id threadId = std::this_thread::get_id();
        {
            wxMutexLocker lock(m_threadMutex);
            m_activeThreads.push_back(threadId);
        }

        core::Logger::getInstance().debug("Background task started (thread ID: {})",
                                          static_cast<unsigned long>(std::hash<std::thread::id>{}(threadId)));

        // Execute task with exception handling
        try {
            task();  // User-provided work

            // Task succeeded - notify GUI (Bartosz's wxQueueEvent pattern)
            wxThreadEvent* evt = new wxThreadEvent(wxEVT_KALAHARI_TASK_COMPLETED);
            evt->SetId(static_cast<int>(std::hash<std::thread::id>{}(threadId)));
            wxQueueEvent(this, evt);  // Thread-safe, no mutex needed

        } catch (const std::exception& e) {
            // Task failed - notify GUI with error
            core::Logger::getInstance().error("Background task failed: {}", e.what());

            wxThreadEvent* evt = new wxThreadEvent(wxEVT_KALAHARI_TASK_FAILED);
            evt->SetString(wxString::FromUTF8(e.what()));
            evt->SetId(static_cast<int>(std::hash<std::thread::id>{}(threadId)));
            wxQueueEvent(this, evt);  // Thread-safe
        }

        // Cleanup (Bartosz's OnExit() pattern)
        {
            wxMutexLocker lock(m_threadMutex);
            auto it = std::find(m_activeThreads.begin(), m_activeThreads.end(), threadId);
            if (it != m_activeThreads.end()) {
                m_activeThreads.erase(it);
            }
        }
        m_threadSemaphore.Post();  // Release semaphore slot

        core::Logger::getInstance().debug("Background task finished (thread pool: {}/4)",
                                          m_activeThreads.size());
    });

    worker.detach();  // Fire-and-forget (like wxTHREAD_DETACHED)
    return true;
}

// ============================================================================
// wxAUI Initialization and Helpers (Task #00013)
// ============================================================================

void MainWindow::initializeAUI() {
    core::Logger::getInstance().info("Initializing wxAUI docking system (GUI Concept layout)...");

    // Create wxAuiManager
    m_auiManager = new wxAuiManager(this, wxAUI_MGR_DEFAULT);

    // Create sample document for testing (Task #00020)
    createSampleDocument();

    // Create all 6 panels
    m_navigatorPanel = new NavigatorPanel(this, m_document.get());
    m_editorPanel = new EditorPanel(this);
    m_propertiesPanel = new PropertiesPanel(this);
    m_statisticsPanel = new StatisticsPanel(this);
    m_searchPanel = new SearchPanel(this);
    m_assistantPanel = new AssistantPanel(this);

    // ========================================================================
    // Layout based on GUI Concept 1 from wxFormBuilder
    // ========================================================================

    // LEFT COLUMN - Split into top (Navigator) and bottom (Assistant)
    // Add Navigator panel (left-top, files/project structure)
    m_auiManager->AddPane(m_navigatorPanel,
        wxAuiPaneInfo()
            .Name("navigator")
            .Caption(_("Navigator"))
            .Left()
            .Layer(0)
            .Position(0)
            .MinSize(200, 300)
            .BestSize(250, 400)
            .CloseButton(true)
            .MaximizeButton(false)
            .PinButton(true));

    // Add Assistant panel (left-bottom, ALWAYS VISIBLE in default layout)
    m_auiManager->AddPane(m_assistantPanel,
        wxAuiPaneInfo()
            .Name("assistant")
            .Caption(_("AI Writing Assistant"))
            .Left()
            .Layer(0)
            .Position(1)
            .MinSize(200, 250)
            .BestSize(250, 350)
            .CloseButton(true)
            .MaximizeButton(false)
            .PinButton(true)
            .Show());  // VISIBLE by default!

    // CENTER - Editor (takes maximum space)
    m_auiManager->AddPane(m_editorPanel,
        wxAuiPaneInfo()
            .Name("editor")
            .Caption(_("Editor"))
            .CenterPane()
            .CloseButton(false)
            .PaneBorder(false));

    // RIGHT COLUMN - Statistics (top), Properties (middle)
    // Add Statistics panel (right-top, daily statistics/challenges)
    m_auiManager->AddPane(m_statisticsPanel,
        wxAuiPaneInfo()
            .Name("statistics")
            .Caption(_("Statistics"))
            .Right()
            .Layer(0)
            .Position(0)
            .MinSize(220, 200)
            .BestSize(280, 250)
            .CloseButton(true)
            .MaximizeButton(false)
            .PinButton(true));

    // Add Properties panel (right-middle, item/character preview)
    m_auiManager->AddPane(m_propertiesPanel,
        wxAuiPaneInfo()
            .Name("properties")
            .Caption(_("Properties"))
            .Right()
            .Layer(0)
            .Position(1)
            .MinSize(220, 250)
            .BestSize(280, 350)
            .CloseButton(true)
            .MaximizeButton(false)
            .PinButton(true));

    // FLOATING - Search panel (hidden by default, appears when needed)
    m_auiManager->AddPane(m_searchPanel,
        wxAuiPaneInfo()
            .Name("search")
            .Caption(_("Find and Replace"))
            .Float()
            .FloatingPosition(300, 200)
            .FloatingSize(500, 150)
            .MinSize(400, 120)
            .BestSize(500, 150)
            .Hide()
            .Floatable(true)
            .Movable(true)
            .CloseButton(true));

    // Commit changes and update
    core::Logger::getInstance().info("Calling wxAuiManager->Update()...");
    m_auiManager->Update();
    core::Logger::getInstance().info("wxAuiManager->Update() completed successfully");

    // Update View menu checkboxes to match initial visibility
    updateViewMenu();

    // Initialize default perspectives
    auto& perspMgr = PerspectiveManager::getInstance();
    if (!perspMgr.perspectiveExists("Default")) {
        // Save current layout as Default perspective
        std::string layout = m_auiManager->SavePerspective().ToStdString();
        perspMgr.savePerspective("Default", layout);
        core::Logger::getInstance().info("Created Default perspective");
    }

    // Create Writing perspective (Navigator + Editor + Assistant)
    if (!perspMgr.perspectiveExists("Writing")) {
        m_auiManager->GetPane("navigator").Show();
        m_auiManager->GetPane("editor").Show();
        m_auiManager->GetPane("assistant").Show();
        m_auiManager->GetPane("properties").Hide();
        m_auiManager->GetPane("statistics").Hide();
        m_auiManager->GetPane("search").Hide();
        m_auiManager->Update();

        std::string layout = m_auiManager->SavePerspective().ToStdString();
        perspMgr.savePerspective("Writing", layout);
        core::Logger::getInstance().info("Created Writing perspective (Navigator + Editor + Assistant)");

        // Restore Default layout
        loadPerspective("Default");
    }

    // Create Editing perspective (Editor + Statistics + Properties)
    if (!perspMgr.perspectiveExists("Editing")) {
        m_auiManager->GetPane("navigator").Hide();
        m_auiManager->GetPane("editor").Show();
        m_auiManager->GetPane("properties").Show();
        m_auiManager->GetPane("statistics").Show();
        m_auiManager->GetPane("search").Hide();
        m_auiManager->GetPane("assistant").Hide();
        m_auiManager->Update();

        std::string layout = m_auiManager->SavePerspective().ToStdString();
        perspMgr.savePerspective("Editing", layout);
        core::Logger::getInstance().info("Created Editing perspective (Editor + Stats + Properties)");

        // Restore Default layout
        loadPerspective("Default");
    }

    // Create Research perspective (All panels visible)
    if (!perspMgr.perspectiveExists("Research")) {
        m_auiManager->GetPane("navigator").Show();
        m_auiManager->GetPane("editor").Show();
        m_auiManager->GetPane("properties").Show();
        m_auiManager->GetPane("statistics").Show();
        m_auiManager->GetPane("search").Show();
        m_auiManager->GetPane("assistant").Show();
        m_auiManager->Update();

        std::string layout = m_auiManager->SavePerspective().ToStdString();
        perspMgr.savePerspective("Research", layout);
        core::Logger::getInstance().info("Created Research perspective (All panels visible)");

        // Restore Default layout (only during first-run initialization)
        loadPerspective("Default");
    }

    // ========================================================================
    // BOTTOM - Log panel (diagnostic mode only - Task #00020)
    // ========================================================================
    // IMPORTANT: Added AFTER perspective initialization, so it's NOT saved in perspectives
    // This ensures LogPanel is always visible when diagnostic mode is enabled,
    // regardless of which perspective is loaded
    if (m_diagnosticMode) {
        createLogPanel();
    }

    // Load last used perspective (or Default if none saved)
    auto& settings = core::SettingsManager::getInstance();
    std::string lastPerspective = settings.get<std::string>("ui.lastPerspective", "Default");

    // Verify perspective exists before loading
    if (perspMgr.perspectiveExists(lastPerspective)) {
        loadPerspective(lastPerspective);
        core::Logger::getInstance().info("Restored last perspective: {}", lastPerspective);
    } else {
        // Fallback to Default if last perspective no longer exists
        loadPerspective("Default");
        core::Logger::getInstance().warn("Last perspective '{}' not found, loading Default", lastPerspective);
    }

    // If diagnostic mode, ensure LogPanel is visible after perspective load
    if (m_diagnosticMode && m_logPanel) {
        m_auiManager->GetPane("log").Show();
        m_auiManager->Update();
    }

    core::Logger::getInstance().info("wxAUI initialized with 6 panels and 4 default perspectives");
}

void MainWindow::updateViewMenu() {
    if (!m_auiManager) return;

    m_viewNavigatorItem->Check(m_auiManager->GetPane("navigator").IsShown());
    m_viewPropertiesItem->Check(m_auiManager->GetPane("properties").IsShown());
    m_viewStatisticsItem->Check(m_auiManager->GetPane("statistics").IsShown());
    m_viewSearchItem->Check(m_auiManager->GetPane("search").IsShown());
    m_viewAssistantItem->Check(m_auiManager->GetPane("assistant").IsShown());

    // Update Log panel checkbox (only if diagnostic mode active)
    if (m_viewLogItem && m_diagnosticMode && m_logPanel) {
        m_viewLogItem->Check(m_auiManager->GetPane("log").IsShown());
    }
}

void MainWindow::loadPerspective(const std::string& name) {
    auto& perspMgr = PerspectiveManager::getInstance();
    auto layout = perspMgr.loadPerspective(name);

    if (layout) {
        m_auiManager->LoadPerspective(wxString::FromUTF8(*layout), true);
        m_auiManager->Update();
        updateViewMenu();

        // Save as last used perspective (for persistence across restarts)
        auto& settings = core::SettingsManager::getInstance();
        settings.set("ui.lastPerspective", name);
        settings.save();

        core::Logger::getInstance().info("Loaded perspective: {}", name);
    } else {
        core::Logger::getInstance().error("Failed to load perspective: {}", name);
        wxMessageBox(wxString::Format("Failed to load perspective '%s'.", name),
            "Perspective Error", wxOK | wxICON_ERROR, this);
    }
}

void MainWindow::savePerspective(const std::string& name) {
    std::string layout = m_auiManager->SavePerspective().ToStdString();
    auto& perspMgr = PerspectiveManager::getInstance();

    if (perspMgr.savePerspective(name, layout)) {
        core::Logger::getInstance().info("Saved perspective: {}", name);
    } else {
        core::Logger::getInstance().error("Failed to save perspective: {}", name);
        wxMessageBox(wxString::Format("Failed to save perspective '%s'.", name),
            "Perspective Error", wxOK | wxICON_ERROR, this);
    }
}

// ============================================================================
// View Menu Event Handlers (Task #00013)
// ============================================================================

void MainWindow::onViewNavigator([[maybe_unused]] wxCommandEvent& event) {
    wxAuiPaneInfo& pane = m_auiManager->GetPane("navigator");
    pane.Show(event.IsChecked());
    m_auiManager->Update();
    core::Logger::getInstance().debug("Navigator panel: {}", event.IsChecked() ? "shown" : "hidden");
}

void MainWindow::onViewProperties([[maybe_unused]] wxCommandEvent& event) {
    wxAuiPaneInfo& pane = m_auiManager->GetPane("properties");
    pane.Show(event.IsChecked());
    m_auiManager->Update();
    core::Logger::getInstance().debug("Properties panel: {}", event.IsChecked() ? "shown" : "hidden");
}

void MainWindow::onViewStatistics([[maybe_unused]] wxCommandEvent& event) {
    wxAuiPaneInfo& pane = m_auiManager->GetPane("statistics");
    pane.Show(event.IsChecked());
    m_auiManager->Update();
    core::Logger::getInstance().debug("Statistics panel: {}", event.IsChecked() ? "shown" : "hidden");
}

void MainWindow::onViewSearch([[maybe_unused]] wxCommandEvent& event) {
    wxAuiPaneInfo& pane = m_auiManager->GetPane("search");
    pane.Show(event.IsChecked());
    m_auiManager->Update();
    core::Logger::getInstance().debug("Search panel: {}", event.IsChecked() ? "shown" : "hidden");
}

void MainWindow::onViewAssistant([[maybe_unused]] wxCommandEvent& event) {
    wxAuiPaneInfo& pane = m_auiManager->GetPane("assistant");
    pane.Show(event.IsChecked());
    m_auiManager->Update();
    core::Logger::getInstance().debug("Assistant panel: {}", event.IsChecked() ? "shown" : "hidden");
}

void MainWindow::onViewLog([[maybe_unused]] wxCommandEvent& event) {
    // Only allow toggling if diagnostic mode is active (panel exists)
    if (!m_diagnosticMode || !m_logPanel) {
        core::Logger::getInstance().warn("Attempted to toggle Log panel, but diagnostic mode is disabled");
        return;
    }

    wxAuiPaneInfo& pane = m_auiManager->GetPane("log");
    pane.Show(event.IsChecked());
    m_auiManager->Update();
    core::Logger::getInstance().debug("Diagnostic Log panel: {}", event.IsChecked() ? "shown" : "hidden");
}

void MainWindow::onViewMode(wxCommandEvent& event) {
    int eventId = event.GetId();
    core::Logger::getInstance().debug("View -> Editor Mode clicked, ID: {}", eventId);

    // Determine which view mode was selected
    bwx_sdk::gui::bwxTextEditor::ViewMode mode;
    wxString modeName;

    switch (eventId) {
        case ID_VIEW_MODE_FULL:
            mode = bwx_sdk::gui::bwxTextEditor::VIEW_FULL;
            modeName = "Full View";
            break;
        case ID_VIEW_MODE_PAGE:
            modeName = "Page View";
            wxMessageBox(
                _("Page View will be implemented in Task #00020.\n\n"
                  "This mode provides MS Word-like page layout."),
                _("Page View (Not Yet Available)"),
                wxOK | wxICON_INFORMATION,
                this
            );
            m_viewModeFullItem->Check(true);  // Revert to Full View
            return;
        case ID_VIEW_MODE_TYPEWRITER:
            modeName = "Typewriter Mode";
            wxMessageBox(
                _("Typewriter Mode will be implemented in Task #00021.\n\n"
                  "This mode provides immersive writing experience."),
                _("Typewriter Mode (Not Yet Available)"),
                wxOK | wxICON_INFORMATION,
                this
            );
            m_viewModeFullItem->Check(true);  // Revert to Full View
            return;
        case ID_VIEW_MODE_PUBLISHER:
            modeName = "Publisher View";
            wxMessageBox(
                _("Publisher View will be implemented in Task #00022.\n\n"
                  "This mode provides manuscript-style formatting."),
                _("Publisher View (Not Yet Available)"),
                wxOK | wxICON_INFORMATION,
                this
            );
            m_viewModeFullItem->Check(true);  // Revert to Full View
            return;
        default:
            core::Logger::getInstance().warn("Unknown view mode ID: {}", eventId);
            return;
    }

    // Delegate to EditorPanel
    if (m_editorPanel) {
        m_editorPanel->setViewMode(mode);
        core::Logger::getInstance().info("Editor view mode changed to: {}", modeName.ToStdString());
    } else {
        core::Logger::getInstance().warn("No EditorPanel available for View Mode change");
    }
}

void MainWindow::onLoadPerspective(wxCommandEvent& event) {
    std::string perspectiveName;
    int eventId = event.GetId();

    switch (eventId) {
        case ID_PERSPECTIVE_DEFAULT:
            perspectiveName = "Default";
            break;
        case ID_PERSPECTIVE_WRITING:
            perspectiveName = "Writing";
            break;
        case ID_PERSPECTIVE_EDITING:
            perspectiveName = "Editing";
            break;
        case ID_PERSPECTIVE_RESEARCH:
            perspectiveName = "Research";
            break;
        default:
            // Check if it's a custom perspective
            if (eventId >= ID_PERSPECTIVE_CUSTOM_START &&
                eventId < ID_PERSPECTIVE_CUSTOM_START + 5) {
                auto it = m_customPerspectiveNames.find(eventId);
                if (it != m_customPerspectiveNames.end()) {
                    perspectiveName = it->second;
                } else {
                    core::Logger::getInstance().error("Unknown custom perspective ID: {}", eventId);
                    return;
                }
            } else {
                core::Logger::getInstance().error("Unknown perspective menu ID: {}", eventId);
                return;
            }
    }

    loadPerspective(perspectiveName);
}

void MainWindow::onSavePerspective([[maybe_unused]] wxCommandEvent& event) {
    wxTextEntryDialog dialog(this,
        _("Enter a name for the new perspective:"),
        _("Save Perspective"),
        _("My Perspective"),
        wxOK | wxCANCEL);

    if (dialog.ShowModal() == wxID_OK) {
        std::string name = dialog.GetValue().ToStdString();
        savePerspective(name);

        // Refresh perspectives menu to show the new perspective
        refreshPerspectivesMenu();

        wxMessageBox(wxString::Format("Perspective '%s' saved successfully.", name),
            "Perspective Saved", wxOK | wxICON_INFORMATION, this);
    }
}

void MainWindow::onManagePerspectives([[maybe_unused]] wxCommandEvent& event) {
    ManagePerspectivesDialog dialog(this);

    if (dialog.ShowModal() == wxID_OK && dialog.shouldLoadPerspective()) {
        loadPerspective(dialog.getSelectedPerspective());
    }

    // Refresh perspectives menu in case user deleted/renamed perspectives
    refreshPerspectivesMenu();

    core::Logger::getInstance().info("Perspective management dialog closed");
}

void MainWindow::refreshPerspectivesMenu() {
    // Remove all previously added custom perspective items
    for (wxMenuItem* item : m_customPerspectiveItems) {
        m_perspectivesMenu->Delete(item);
    }
    m_customPerspectiveItems.clear();
    m_customPerspectiveNames.clear();

    // Get all perspectives with timestamps (sorted by modification time, newest first)
    auto& perspMgr = PerspectiveManager::getInstance();
    auto perspectives = perspMgr.listPerspectivesWithTimestamp();

    // Filter out default perspectives
    const std::vector<std::string> defaults = {"Default", "Writing", "Editing", "Research"};
    std::vector<std::pair<std::string, std::filesystem::file_time_type>> customPerspectives;

    for (const auto& [name, timestamp] : perspectives) {
        if (std::find(defaults.begin(), defaults.end(), name) == defaults.end()) {
            customPerspectives.push_back({name, timestamp});
        }
    }

    // Take only the 5 most recent
    if (customPerspectives.size() > 5) {
        customPerspectives.resize(5);
    }

    // Add to menu if we have custom perspectives
    if (!customPerspectives.empty()) {
        // Find position to insert (AFTER "Research" item, BEFORE first separator)
        int insertPos = m_perspectivesMenu->GetMenuItemCount() - 3;  // Before separator, Save, Manage

        // Add separator before custom perspectives
        wxMenuItem* separator = new wxMenuItem(m_perspectivesMenu, wxID_SEPARATOR);
        m_perspectivesMenu->Insert(insertPos++, separator);
        m_customPerspectiveItems.push_back(separator);

        // Add custom perspectives
        int id = ID_PERSPECTIVE_CUSTOM_START;
        for (const auto& [name, timestamp] : customPerspectives) {
            wxMenuItem* item = new wxMenuItem(m_perspectivesMenu, id,
                wxString::FromUTF8(name),
                wxString::Format(_("Load custom perspective '%s'"), name));
            m_perspectivesMenu->Insert(insertPos++, item);
            m_customPerspectiveItems.push_back(item);
            m_customPerspectiveNames[id] = name;
            id++;
        }

        core::Logger::getInstance().debug("Added {} custom perspectives to menu", customPerspectives.size());
    }
}

// ============================================================================
// Sample Document Creation (Task #00020 - Testing)
// ============================================================================

void MainWindow::createSampleDocument()
{
    auto& logger = core::Logger::getInstance();
    logger.info("Creating sample document for OutlineTab testing...");

    // Create document
    m_document = std::make_unique<core::Document>(
        "My Epic Novel",
        "Jane Doe",
        "en"
    );

    core::Book& book = m_document->getBook();

    // Front Matter
    auto prologue = std::make_shared<core::BookElement>(
        "chapter", core::Document::generateId(), "Prologue", ""
    );
    prologue->setWordCount(450);
    book.addFrontMatter(prologue);

    // Body - Part I
    auto part1 = std::make_shared<core::Part>(
        core::Document::generateId(),
        "Part I: The Beginning"
    );

    auto ch1 = std::make_shared<core::BookElement>(
        "chapter", core::Document::generateId(), "Chapter 1: The Awakening", ""
    );
    ch1->setWordCount(2500);
    part1->addChapter(ch1);

    auto ch2 = std::make_shared<core::BookElement>(
        "chapter", core::Document::generateId(), "Chapter 2: First Steps", ""
    );
    ch2->setWordCount(2800);
    part1->addChapter(ch2);

    auto ch3 = std::make_shared<core::BookElement>(
        "chapter", core::Document::generateId(), "Chapter 3: The Discovery", ""
    );
    ch3->setWordCount(3100);
    part1->addChapter(ch3);

    book.addPart(part1);

    // Body - Part II
    auto part2 = std::make_shared<core::Part>(
        core::Document::generateId(),
        "Part II: The Journey"
    );

    auto ch4 = std::make_shared<core::BookElement>(
        "chapter", core::Document::generateId(), "Chapter 4: On the Road", ""
    );
    ch4->setWordCount(2700);
    part2->addChapter(ch4);

    auto ch5 = std::make_shared<core::BookElement>(
        "chapter", core::Document::generateId(), "Chapter 5: Trials and Tribulations", ""
    );
    ch5->setWordCount(3300);
    part2->addChapter(ch5);

    book.addPart(part2);

    // Back Matter
    auto epilogue = std::make_shared<core::BookElement>(
        "chapter", core::Document::generateId(), "Epilogue", ""
    );
    epilogue->setWordCount(800);
    book.addBackMatter(epilogue);

    logger.info("Sample document created: '{}' with {} chapters, {} words total",
        m_document->getTitle(),
        book.getChapterCount(),
        book.getWordCount());
}

} // namespace gui
} // namespace kalahari
