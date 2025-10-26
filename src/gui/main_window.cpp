/// @file main_window.cpp
/// @brief Implementation of MainWindow

#include "main_window.h"
#include "../core/logger.h"
#include <wx/artprov.h>

namespace kalahari {
namespace gui {

// ============================================================================
// Event Table (maps events to handler methods)
// ============================================================================

wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
    // File menu events
    EVT_MENU(wxID_NEW,     MainWindow::onFileNew)
    EVT_MENU(wxID_OPEN,    MainWindow::onFileOpen)
    EVT_MENU(wxID_SAVE,    MainWindow::onFileSave)
    EVT_MENU(wxID_SAVEAS,  MainWindow::onFileSaveAs)
    EVT_MENU(wxID_EXIT,    MainWindow::onFileExit)

    // Edit menu events
    EVT_MENU(wxID_UNDO,    MainWindow::onEditUndo)
    EVT_MENU(wxID_REDO,    MainWindow::onEditRedo)

    // Help menu events
    EVT_MENU(wxID_ABOUT,   MainWindow::onHelpAbout)

    // Window events
    EVT_CLOSE(MainWindow::onClose)
wxEND_EVENT_TABLE()

// ============================================================================
// Constructor / Destructor
// ============================================================================

MainWindow::MainWindow()
    : wxFrame(nullptr, wxID_ANY, _("Kalahari Writer's IDE"))
{
    core::Logger::getInstance().debug("Constructing MainWindow...");

    // Set window size and position
    SetSize(1024, 768);
    Centre();

    core::Logger::getInstance().debug("Window size set to 1024x768, centered on screen");

    // Create UI components (order matters!)
    createMenuBar();
    createToolBar();
    createStatusBar();
    setupMainPanel();

    core::Logger::getInstance().info("MainWindow construction complete");
}

MainWindow::~MainWindow() {
    core::Logger::getInstance().debug("MainWindow destructor called");
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
    // Help Menu
    // ------------------------------------------------------------------------
    wxMenu* helpMenu = new wxMenu();
    helpMenu->Append(wxID_HELP,   _("&Contents\tF1"),      _("Show help contents"));
    helpMenu->AppendSeparator();
    helpMenu->Append(wxID_ABOUT,  _("&About Kalahari"),    _("About this application"));

    m_menuBar->Append(helpMenu, _("&Help"));

    // Set the menu bar
    SetMenuBar(m_menuBar);

    core::Logger::getInstance().debug("Menu bar created with 4 menus (File, Edit, View, Help)");
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

    m_statusBar->SetStatusText(_("Open document (stub)"), 0);

    wxMessageBox(
        _("Open document functionality will be implemented in Phase 1.\n\n"
          "Phase 0 Week 2: GUI Infrastructure"),
        _("Open Document"),
        wxOK | wxICON_INFORMATION,
        this
    );
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
          "© 2025 Kalahari Project\n"
          "Licensed under MIT License"),
        _("About Kalahari"),
        wxOK | wxICON_INFORMATION,
        this
    );
}

void MainWindow::onClose(wxCloseEvent& event) {
    core::Logger::getInstance().info("Window closing (user initiated)");

    // TODO Phase 1: Check for unsaved changes
    // TODO Phase 1: Prompt user if needed
    // TODO Phase 1: Save window position/size

    m_statusBar->SetStatusText(_("Closing..."), 0);

    // Allow window to close
    event.Skip();
}

} // namespace gui
} // namespace kalahari
