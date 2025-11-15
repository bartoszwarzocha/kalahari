/// @file log_panel.cpp
/// @brief Implementation of LogPanel

#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/gui/icon_registry.h"
#include "../settings_dialog.h"  // In src/gui/ not include/
#include <kalahari/core/logger.h>
#include <kalahari/core/settings_manager.h>
#include <wx/clipbrd.h>
#include <wx/artprov.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <algorithm>

namespace kalahari {
namespace gui {

// ============================================================================
// Event IDs
// ============================================================================

enum {
    ID_LOG_OPTIONS = wxID_HIGHEST + 300,
    ID_LOG_OPEN_FOLDER,
    ID_LOG_COPY,
    ID_LOG_CLEAR
};

// ============================================================================
// Event Table
// ============================================================================

wxBEGIN_EVENT_TABLE(LogPanel, wxPanel)
    EVT_TOOL(ID_LOG_OPTIONS, LogPanel::onOptions)
    EVT_TOOL(ID_LOG_OPEN_FOLDER, LogPanel::onOpenLogFolder)
    EVT_TOOL(ID_LOG_COPY, LogPanel::onCopyToClipboard)
    EVT_TOOL(ID_LOG_CLEAR, LogPanel::onClearLog)
wxEND_EVENT_TABLE()

// ============================================================================
// Constructor / Destructor
// ============================================================================

LogPanel::LogPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    core::Logger::getInstance().info("Creating Log panel (diagnostic mode only)");
    setupLayout();
    applySettings(); // Apply initial settings (colors, font, buffer size)
}

// ============================================================================
// Public Methods
// ============================================================================

void LogPanel::appendLog(const std::string& message) {
    wxString wxMsg(message);

    // Add to ring buffer
    m_logBuffer.push_back(wxMsg);

    // Remove oldest line if buffer exceeds max size
    if (m_logBuffer.size() > m_maxBufferSize) {
        m_logBuffer.pop_front();
        // Rebuild entire display when buffer overflows
        rebuildDisplay();
    } else {
        // Just append the new line
        m_logDisplay->AppendText(wxMsg);
        if (!wxMsg.EndsWith("\n")) {
            m_logDisplay->AppendText("\n");
        }
    }
}

void LogPanel::setMaxBufferSize(size_t size) {
    // Clamp to 1-1000 range
    m_maxBufferSize = std::clamp(size, size_t(1), size_t(1000));

    // Trim buffer if current size exceeds new max
    while (m_logBuffer.size() > m_maxBufferSize) {
        m_logBuffer.pop_front();
    }

    // Rebuild display if buffer was trimmed
    if (m_logBuffer.size() < m_maxBufferSize) {
        rebuildDisplay();
    }
}

void LogPanel::clearLog() {
    m_logBuffer.clear();
    m_logDisplay->Clear();
    core::Logger::getInstance().info("Log panel cleared");
}

void LogPanel::applySettings() {
    // Load settings from SettingsManager
    auto& settingsMgr = core::SettingsManager::getInstance();

    // Load buffer size
    int bufferSize = settingsMgr.get<int>("log.bufferSize", 500);
    setMaxBufferSize(bufferSize);

    // Load background color
    wxColour bgColor(
        settingsMgr.get<int>("log.backgroundColor.r", 60),
        settingsMgr.get<int>("log.backgroundColor.g", 60),
        settingsMgr.get<int>("log.backgroundColor.b", 60)
    );

    // Load text color
    wxColour textColor(
        settingsMgr.get<int>("log.textColor.r", 255),
        settingsMgr.get<int>("log.textColor.g", 255),
        settingsMgr.get<int>("log.textColor.b", 255)
    );

    // Load font size
    int fontSize = settingsMgr.get<int>("log.fontSize", 11);
    wxFont monoFont(fontSize, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

    // Apply colors and font to wxTextCtrl
    m_logDisplay->SetBackgroundColour(bgColor);
    m_logDisplay->SetForegroundColour(textColor);
    m_logDisplay->SetFont(monoFont);

    // For wxTE_RICH2, also set default style (fixes Windows black text issue)
    wxTextAttr defaultStyle;
    defaultStyle.SetTextColour(textColor);
    defaultStyle.SetBackgroundColour(bgColor);
    defaultStyle.SetFont(monoFont);
    m_logDisplay->SetDefaultStyle(defaultStyle);

    // Rebuild display to apply new styling
    rebuildDisplay();

    core::Logger::getInstance().info("Log panel settings applied (buffer={}, fontSize={}, bg=({},{},{}), text=({},{},{}))",
        bufferSize, fontSize,
        bgColor.Red(), bgColor.Green(), bgColor.Blue(),
        textColor.Red(), textColor.Green(), textColor.Blue());
}

// ============================================================================
// Event Handlers
// ============================================================================

void LogPanel::onOptions([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("Log Options clicked - opening Settings Dialog");

    // Get MainWindow parent
    wxWindow* parent = GetParent();
    while (parent && !dynamic_cast<wxFrame*>(parent)) {
        parent = parent->GetParent();
    }

    if (!parent) {
        core::Logger::getInstance().error("LogPanel::onOptions - Could not find MainWindow parent");
        return;
    }

    // Create and show Settings Dialog (same pattern as MainWindow::onToolsSettings)
    // Note: SettingsState currently only has diagnostic mode settings
    // When more settings are added, this will need to be expanded
    SettingsState currentState;
    currentState.diagnosticModeEnabled = true;  // Diagnostic mode is active (LogPanel only exists when enabled)
    currentState.launchedWithDiagFlag = false;  // Unknown from here, but dialog handles this

    SettingsDialog dlg(parent, currentState);

    // TODO: Select "Diagnostics" tree item when Settings Dialog supports it
    // Currently no API to pre-select tree item, so dialog will open with default selection

    dlg.ShowModal();
}

void LogPanel::onOpenLogFolder([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("Open Log Folder clicked");

    // Get log directory path (same pattern as KalahariApp::initializeLogging)
    wxStandardPaths& stdPaths = wxStandardPaths::Get();
    wxString userDataDir = stdPaths.GetUserDataDir();
    wxString logDir = userDataDir + wxFileName::GetPathSeparator() + "logs";

    // Check if directory exists
    if (!wxFileName::DirExists(logDir)) {
        core::Logger::getInstance().warn("Log directory does not exist: {}", logDir.utf8_str().data());
        wxMessageBox(wxString::Format("Log directory not found:\n%s", logDir),
                     "Open Log Folder", wxOK | wxICON_WARNING, this);
        return;
    }

    // Open directory in system file explorer
    // wxLaunchDefaultBrowser works for file:// URLs, but better to use platform-specific commands
#ifdef __WXMSW__
    // Windows: explorer.exe
    wxString command = wxString::Format("explorer \"%s\"", logDir);
    wxExecute(command);
#elif defined(__WXMAC__)
    // macOS: open command
    wxString command = wxString::Format("open \"%s\"", logDir);
    wxExecute(command);
#else
    // Linux: xdg-open (works on most desktop environments)
    wxString command = wxString::Format("xdg-open \"%s\"", logDir);
    if (wxExecute(command) == 0) {
        // Fallback if xdg-open not available
        core::Logger::getInstance().warn("xdg-open failed, trying file manager alternatives");
        // Try common file managers
        if (wxExecute(wxString::Format("nautilus \"%s\"", logDir)) == 0 &&
            wxExecute(wxString::Format("dolphin \"%s\"", logDir)) == 0 &&
            wxExecute(wxString::Format("thunar \"%s\"", logDir)) == 0) {
            wxMessageBox(wxString::Format("Could not open file manager.\nLog directory:\n%s", logDir),
                         "Open Log Folder", wxOK | wxICON_WARNING, this);
        }
    }
#endif

    core::Logger::getInstance().info("Opened log folder: {}", logDir.utf8_str().data());
}

void LogPanel::onCopyToClipboard([[maybe_unused]] wxCommandEvent& event) {
    if (wxTheClipboard->Open()) {
        wxString logText;
        for (const wxString& line : m_logBuffer) {
            logText += line;
            if (!line.EndsWith("\n")) {
                logText += "\n";
            }
        }

        wxTheClipboard->SetData(new wxTextDataObject(logText));
        wxTheClipboard->Close();

        core::Logger::getInstance().info("Log copied to clipboard ({} lines)", m_logBuffer.size());

        // Show brief feedback (status bar would be better, but panel doesn't have direct access)
        wxMessageBox(wxString::Format("Copied %zu log lines to clipboard.", m_logBuffer.size()),
                     "Copy to Clipboard", wxOK | wxICON_INFORMATION, this);
    } else {
        core::Logger::getInstance().warn("Failed to open clipboard");
        wxMessageBox("Failed to access clipboard.", "Copy to Clipboard",
                     wxOK | wxICON_ERROR, this);
    }
}

void LogPanel::onClearLog([[maybe_unused]] wxCommandEvent& event) {
    clearLog();
    core::Logger::getInstance().info("Log panel cleared by user");
}

// ============================================================================
// Helper Methods
// ============================================================================

void LogPanel::setupLayout() {
    // Main horizontal sizer: [TextCtrl (flexible) | ToolBar (fixed)]
    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);

    // ========================================================================
    // Left: wxTextCtrl (log display area)
    // ========================================================================

    m_logDisplay = new wxTextCtrl(this, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxTE_WORDWRAP);

    // Apply default styling (will be overridden by applySettings())
    wxColour bgColor(60, 60, 60);
    wxColour textColor(255, 255, 255);
    wxFont monoFont(11, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

    m_logDisplay->SetBackgroundColour(bgColor);
    m_logDisplay->SetForegroundColour(textColor);
    m_logDisplay->SetFont(monoFont);

    // For wxTE_RICH2, set default style (important for Windows)
    wxTextAttr defaultStyle;
    defaultStyle.SetTextColour(textColor);
    defaultStyle.SetBackgroundColour(bgColor);
    defaultStyle.SetFont(monoFont);
    m_logDisplay->SetDefaultStyle(defaultStyle);

    mainSizer->Add(m_logDisplay, 1, wxALL | wxEXPAND, 0);

    // ========================================================================
    // Right: Vertical wxToolBar (16px buttons)
    // ========================================================================

    // Create vertical toolbar (TB_VERTICAL + TB_FLAT + TB_NODIVIDER for clean look)
    m_toolBar = new wxToolBar(this, wxID_ANY,
        wxDefaultPosition, wxDefaultSize,
        wxTB_VERTICAL | wxTB_FLAT | wxTB_NODIVIDER);

    // Set toolbar button size from IconRegistry (24px default)
    auto& iconReg = IconRegistry::getInstance();
    int toolbarSize = iconReg.getSizeForClient(wxART_TOOLBAR);
    m_toolBar->SetToolBitmapSize(wxSize(toolbarSize, toolbarSize));

    // Add toolbar buttons using KalahariArtProvider (Material Design SVG icons)
    wxBitmap optionsIcon = wxArtProvider::GetBitmap(wxART_HELP_SETTINGS, wxART_TOOLBAR, wxDefaultSize);
    wxBitmap folderIcon = wxArtProvider::GetBitmap(wxART_FOLDER, wxART_TOOLBAR, wxDefaultSize);
    wxBitmap copyIcon = wxArtProvider::GetBitmap(wxART_COPY, wxART_TOOLBAR, wxDefaultSize);
    wxBitmap clearIcon = wxArtProvider::GetBitmap(wxART_DELETE, wxART_TOOLBAR, wxDefaultSize);

    m_toolBar->AddTool(ID_LOG_OPTIONS, "Options", optionsIcon, "Open Settings Dialog (Log tab)");
    m_toolBar->AddTool(ID_LOG_OPEN_FOLDER, "Open Folder", folderIcon, "Open log directory in file explorer");
    m_toolBar->AddTool(ID_LOG_COPY, "Copy", copyIcon, "Copy entire log to clipboard");
    m_toolBar->AddTool(ID_LOG_CLEAR, "Clear", clearIcon, "Clear all log messages");

    m_toolBar->Realize();

    mainSizer->Add(m_toolBar, 0, wxALL | wxEXPAND, 0);

    // ========================================================================
    // Set sizer
    // ========================================================================

    SetSizer(mainSizer);

    // Add welcome message
    appendLog("Kalahari Diagnostic Log Panel initialized");
    appendLog("Log messages will appear here in real-time");
    appendLog("----------------------------------------");
}

void LogPanel::rebuildDisplay() {
    m_logDisplay->Clear();

    for (const wxString& line : m_logBuffer) {
        m_logDisplay->AppendText(line);
        if (!line.EndsWith("\n")) {
            m_logDisplay->AppendText("\n");
        }
    }
}

} // namespace gui
} // namespace kalahari
