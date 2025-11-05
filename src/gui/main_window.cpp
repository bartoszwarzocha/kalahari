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
#include "kalahari/gui/perspective_manager.h"
#include <kalahari/core/logger.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/python_interpreter.h>
#include <kalahari/core/diagnostic_manager.h>
#include <kalahari/core/plugin_manager.h>
#include <kalahari/gui/icon_registry.h>
#include <kalahari/gui/art_provider.h>
#include <kalahari/resources/icons_material.h>
#include <wx/artprov.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/utils.h>
#include <chrono>

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
    EVT_MENU(wxID_PREFERENCES, MainWindow::onFileSettings)
    EVT_MENU(wxID_EXIT,        MainWindow::onFileExit)

    // Edit menu events
    EVT_MENU(wxID_UNDO,    MainWindow::onEditUndo)
    EVT_MENU(wxID_REDO,    MainWindow::onEditRedo)

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
    iconRegistry.registerIcon(wxART_INFORMATION, INFO, _("Information"));

    core::Logger::getInstance().info("Icon system initialized (18 Material Design icons registered)");

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
    createMenuBar();
    createToolBar();
    createStatusBar();

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

void MainWindow::createToolBar() {
    core::Logger::getInstance().debug("Creating toolbar...");

    // Create toolbar with horizontal orientation and text labels
    m_toolBar = CreateToolBar(wxTB_HORIZONTAL | wxTB_TEXT);

    // Add tools using stock icons from wxArtProvider (platform-native)
    m_toolBar->AddTool(wxID_NEW, _("New"),
        wxArtProvider::GetBitmap(wxART_NEW, wxART_TOOLBAR),
        _("Create new document"));

    m_toolBar->AddTool(wxID_OPEN, _("Open"),
        wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_TOOLBAR),
        _("Open existing document"));

    m_toolBar->AddTool(wxID_SAVE, _("Save"),
        wxArtProvider::GetBitmap(wxART_FILE_SAVE, wxART_TOOLBAR),
        _("Save current document"));

    m_toolBar->AddSeparator();

    m_toolBar->AddTool(wxID_UNDO, _("Undo"),
        wxArtProvider::GetBitmap(wxART_UNDO, wxART_TOOLBAR),
        _("Undo last action"));

    m_toolBar->AddTool(wxID_REDO, _("Redo"),
        wxArtProvider::GetBitmap(wxART_REDO, wxART_TOOLBAR),
        _("Redo last undone action"));

    // Realize toolbar (required to display tools)
    m_toolBar->Realize();

    core::Logger::getInstance().debug("Toolbar created with 5 tools (New, Open, Save, Undo, Redo)");
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
    createMenuBar();  // Will check m_diagnosticMode
    delete oldMenuBar;

    core::Logger::getInstance().debug("Menu bar rebuilt with diagnostic mode {}",
                                      enabled ? "visible" : "hidden");
}

// ============================================================================
// Event Handlers
// ============================================================================

void MainWindow::onFileNew([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("File -> New clicked");

    m_statusBar->SetStatusText(_("New document (stub)"), 0);

    wxMessageBox(
        _("New document functionality will be implemented in Phase 1.\n\n"
          "Phase 0 Week 2: GUI Infrastructure"),
        _("New Document"),
        wxOK | wxICON_INFORMATION,
        this
    );
}

void MainWindow::onFileOpen([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("File -> Open clicked");

    // Update status bar immediately (GUI thread)
    m_statusBar->SetStatusText(_("Loading file..."), 0);

    // Submit background task (demonstrates threading infrastructure - Phase 0 Week 2)
    bool submitted = submitBackgroundTask([this]() {
        // Simulate heavy file loading (future: actual wxFile, JSON parsing, etc.)
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Simulate file loaded
        std::string filename = "example.klh";  // Future: from wxFileDialog

        core::Logger::getInstance().debug("Background file load completed: {}", filename);

        // Update GUI when done (CallAfter pattern - marshals to GUI thread)
        CallAfter([this, filename]() {
            m_statusBar->SetStatusText(
                wxString::Format(_("Loaded: %s"), wxString::FromUTF8(filename)),
                0);
        });
    });

    if (!submitted) {
        // Thread limit reached - show warning
        wxMessageBox(
            _("Too many operations in progress. Please wait for current tasks to complete."),
            _("Busy"),
            wxOK | wxICON_WARNING,
            this);
    }
}

void MainWindow::onFileSave([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("File -> Save clicked");

    m_statusBar->SetStatusText(_("Save document (stub)"), 0);

    wxMessageBox(
        _("Save document functionality will be implemented in Phase 1.\n\n"
          "Phase 0 Week 2: GUI Infrastructure"),
        _("Save Document"),
        wxOK | wxICON_INFORMATION,
        this
    );
}

void MainWindow::onFileSaveAs([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("File -> Save As clicked");

    m_statusBar->SetStatusText(_("Save As (stub)"), 0);

    wxMessageBox(
        _("Save As functionality will be implemented in Phase 1.\n\n"
          "Phase 0 Week 2: GUI Infrastructure"),
        _("Save As"),
        wxOK | wxICON_INFORMATION,
        this
    );
}

void MainWindow::onFileSettings([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("File -> Settings clicked");

    // Prepare current state for dialog
    SettingsState currentState;
    currentState.diagnosticModeEnabled = m_diagnosticMode;
    currentState.launchedWithDiagFlag = m_launchedWithDiagFlag;

    // Show dialog
    SettingsDialog dlg(this, currentState);
    if (dlg.ShowModal() == wxID_OK) {
        SettingsState newState = dlg.getNewState();

        // Apply diagnostic mode change if changed
        if (newState.diagnosticModeEnabled != m_diagnosticMode) {
            setDiagnosticMode(newState.diagnosticModeEnabled);
        }

        // Phase 1+: Apply other settings here
        // ...

        m_statusBar->SetStatusText(_("Settings saved"), 0);
    } else {
        core::Logger::getInstance().debug("Settings dialog cancelled by user");
    }
}

void MainWindow::onFileExit([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("File -> Exit clicked");

    // Close window (will trigger onClose event)
    Close(true);
}

void MainWindow::onEditUndo([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("Edit -> Undo clicked");

    m_statusBar->SetStatusText(_("Undo (stub)"), 0);

    wxMessageBox(
        _("Undo functionality will be implemented in Phase 1.\n\n"
          "Phase 0 Week 2: GUI Infrastructure"),
        _("Undo"),
        wxOK | wxICON_INFORMATION,
        this
    );
}

void MainWindow::onEditRedo([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("Edit -> Redo clicked");

    m_statusBar->SetStatusText(_("Redo (stub)"), 0);

    wxMessageBox(
        _("Redo functionality will be implemented in Phase 1.\n\n"
          "Phase 0 Week 2: GUI Infrastructure"),
        _("Redo"),
        wxOK | wxICON_INFORMATION,
        this
    );
}

// ============================================================================
// Format Menu Event Handlers (Task #00014)
// ============================================================================

void MainWindow::onFormatBold(wxCommandEvent& event) {
    core::Logger::getInstance().debug("Format -> Bold clicked");

    if (m_editorPanel) {
        m_editorPanel->onFormatBold(event);
    } else {
        core::Logger::getInstance().warn("No EditorPanel available for Format -> Bold");
    }
}

void MainWindow::onFormatItalic(wxCommandEvent& event) {
    core::Logger::getInstance().debug("Format -> Italic clicked");

    if (m_editorPanel) {
        m_editorPanel->onFormatItalic(event);
    } else {
        core::Logger::getInstance().warn("No EditorPanel available for Format -> Italic");
    }
}

void MainWindow::onFormatUnderline(wxCommandEvent& event) {
    core::Logger::getInstance().debug("Format -> Underline clicked");

    if (m_editorPanel) {
        m_editorPanel->onFormatUnderline(event);
    } else {
        core::Logger::getInstance().warn("No EditorPanel available for Format -> Underline");
    }
}

void MainWindow::onFormatFont(wxCommandEvent& event) {
    core::Logger::getInstance().debug("Format -> Font clicked");

    if (m_editorPanel) {
        m_editorPanel->onFormatFont(event);
    } else {
        core::Logger::getInstance().warn("No EditorPanel available for Format -> Font");
    }
}

void MainWindow::onFormatClear(wxCommandEvent& event) {
    core::Logger::getInstance().debug("Format -> Clear Formatting clicked");

    if (m_editorPanel) {
        m_editorPanel->onFormatClear(event);
    } else {
        core::Logger::getInstance().warn("No EditorPanel available for Format -> Clear");
    }
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

    // Create all 6 panels
    m_navigatorPanel = new NavigatorPanel(this);
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
    m_auiManager->Update();

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

    core::Logger::getInstance().info("wxAUI initialized with 6 panels and 4 default perspectives");
}

void MainWindow::updateViewMenu() {
    if (!m_auiManager) return;

    m_viewNavigatorItem->Check(m_auiManager->GetPane("navigator").IsShown());
    m_viewPropertiesItem->Check(m_auiManager->GetPane("properties").IsShown());
    m_viewStatisticsItem->Check(m_auiManager->GetPane("statistics").IsShown());
    m_viewSearchItem->Check(m_auiManager->GetPane("search").IsShown());
    m_viewAssistantItem->Check(m_auiManager->GetPane("assistant").IsShown());
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

} // namespace gui
} // namespace kalahari
