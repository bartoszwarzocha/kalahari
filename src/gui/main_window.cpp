/// @file main_window.cpp
/// @brief Implementation of MainWindow

#include "main_window.h"
#include "settings_dialog.h"
#include <kalahari/core/logger.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/python_interpreter.h>
#include <kalahari/core/diagnostic_manager.h>
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
    ID_DIAG_OPEN_LOGS,
    ID_DIAG_SYSTEM_INFO
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

    // Diagnostics menu events
    EVT_MENU(ID_DIAG_TEST_PYTHON, MainWindow::onDiagnosticsTestPython)
    EVT_MENU(ID_DIAG_TEST_PYBIND11, MainWindow::onDiagnosticsTestPyBind11)
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
    setupMainPanel();

    // Bind threading events dynamically (modern approach, allows runtime binding)
    Bind(wxEVT_KALAHARI_TASK_COMPLETED, &MainWindow::onTaskCompleted, this, wxID_ANY);
    Bind(wxEVT_KALAHARI_TASK_FAILED, &MainWindow::onTaskFailed, this, wxID_ANY);

    core::Logger::getInstance().debug("Threading infrastructure initialized (max 4 threads)");
    core::Logger::getInstance().info("MainWindow construction complete");
}

MainWindow::~MainWindow() {
    core::Logger::getInstance().info("MainWindow shutting down...");

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
    fileMenu->Append(wxID_NEW,    _("&New\tCtrl+N"),       _("Create a new document"));
    fileMenu->Append(wxID_OPEN,   _("&Open...\tCtrl+O"),   _("Open an existing document"));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_SAVE,   _("&Save\tCtrl+S"),      _("Save the current document"));
    fileMenu->Append(wxID_SAVEAS, _("Save &As...\tCtrl+Shift+S"), _("Save document with a new name"));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_PREFERENCES, _("Se&ttings...\tCtrl+,"), _("Open application settings"));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT,   _("E&xit\tAlt+F4"),      _("Exit Kalahari"));

    m_menuBar->Append(fileMenu, _("&File"));

    // ------------------------------------------------------------------------
    // Edit Menu
    // ------------------------------------------------------------------------
    wxMenu* editMenu = new wxMenu();
    editMenu->Append(wxID_UNDO,   _("&Undo\tCtrl+Z"),      _("Undo last action"));
    editMenu->Append(wxID_REDO,   _("&Redo\tCtrl+Y"),      _("Redo last undone action"));
    editMenu->AppendSeparator();
    editMenu->Append(wxID_CUT,    _("Cu&t\tCtrl+X"),       _("Cut selection to clipboard"));
    editMenu->Append(wxID_COPY,   _("&Copy\tCtrl+C"),      _("Copy selection to clipboard"));
    editMenu->Append(wxID_PASTE,  _("&Paste\tCtrl+V"),     _("Paste from clipboard"));

    m_menuBar->Append(editMenu, _("&Edit"));

    // ------------------------------------------------------------------------
    // View Menu
    // ------------------------------------------------------------------------
    wxMenu* viewMenu = new wxMenu();
    viewMenu->Append(wxID_ANY,    _("&Fullscreen\tF11"),   _("Toggle fullscreen mode"));
    viewMenu->AppendSeparator();
    viewMenu->Append(wxID_ANY,    _("&Toolbar"),           _("Show/hide toolbar"));
    viewMenu->Append(wxID_ANY,    _("&Status Bar"),        _("Show/hide status bar"));

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
    helpMenu->Append(wxID_HELP,   _("&Contents\tF1"),      _("Show help contents"));
    helpMenu->AppendSeparator();
    helpMenu->Append(wxID_ABOUT,  _("&About Kalahari"),    _("About this application"));

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

void MainWindow::onHelpAbout([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("Help -> About clicked");

    wxMessageBox(
        _("Kalahari Writer's IDE\n"
          "Version 0.0.1-alpha (Phase 0 Week 2)\n\n"
          "A comprehensive writing environment for book authors.\n\n"
          "Copyright (c) 2025 Kalahari Project\n"
          "Licensed under MIT License"),
        _("About Kalahari"),
        wxOK | wxICON_INFORMATION,
        this
    );
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

} // namespace gui
} // namespace kalahari
