/// @file kalahari_app.cpp
/// @brief Implementation of KalahariApp

#include "kalahari_app.h"
#include "main_window.h"
#include "../core/logger.h"
#include <wx/stdpaths.h>
#include <wx/filename.h>

namespace kalahari {
namespace gui {

bool KalahariApp::OnInit() {
    // 1. Initialize logging FIRST (before any other operations)
    initializeLogging();
    core::Logger::getInstance().info("=================================================");
    core::Logger::getInstance().info("Kalahari Writer's IDE starting...");
    core::Logger::getInstance().info("=================================================");

    // 2. Set application metadata (used by wxConfig, wxStandardPaths, etc.)
    SetAppName("Kalahari");
    SetVendorName("Kalahari Project");
    SetAppDisplayName("Kalahari Writer's IDE");

    core::Logger::getInstance().info("Application metadata set (vendor: {}, app: {})",
                                     GetVendorName().utf8_str().data(),
                                     GetAppName().utf8_str().data());

    // 3. Initialize wxWidgets image handlers (needed for toolbar icons, etc.)
    wxInitAllImageHandlers();
    core::Logger::getInstance().debug("Image handlers initialized");

    // 4. Show splash screen (placeholder for future - Phase 1)
    // showSplashScreen();

    // 5. Create and show main window
    core::Logger::getInstance().info("Creating main window...");
    m_mainWindow = new MainWindow();

    if (!m_mainWindow) {
        core::Logger::getInstance().critical("Failed to create main window!");
        return false;
    }

    m_mainWindow->Show(true);
    core::Logger::getInstance().info("Main window created and shown successfully");

    // Success!
    return true;
}

int KalahariApp::OnExit() {
    core::Logger::getInstance().info("=================================================");
    core::Logger::getInstance().info("Kalahari Writer's IDE shutting down...");
    core::Logger::getInstance().info("=================================================");

    // Flush logs before exit
    core::Logger::getInstance().flush();

    // Return 0 for successful exit
    return 0;
}

void KalahariApp::initializeLogging() {
    // Get platform-specific user data directory
    // Windows: C:\Users\<user>\AppData\Roaming\Kalahari Project\Kalahari
    // macOS: ~/Library/Application Support/Kalahari
    // Linux: ~/.config/kalahari
    wxStandardPaths& stdPaths = wxStandardPaths::Get();
    wxString userDataDir = stdPaths.GetUserDataDir();

    // Create logs subdirectory
    wxString logDir = userDataDir + wxFileName::GetPathSeparator() + "logs";
    wxFileName::Mkdir(logDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

    // Create log file path: logs/kalahari.log
    wxString logFilePath = logDir + wxFileName::GetPathSeparator() + "kalahari.log";

    // Initialize logger
    try {
        core::Logger::getInstance().init(logFilePath.utf8_str().data());
    } catch (const std::exception& e) {
        // If logging fails, show error dialog and continue
        // (app can still run without logging)
        wxMessageBox(wxString::Format("Failed to initialize logging: %s\n\nApplication will continue without file logging.",
                                      e.what()),
                    "Logging Error",
                    wxOK | wxICON_WARNING);
    }
}

void KalahariApp::showSplashScreen() {
    // Placeholder for Phase 1 implementation
    // Will show:
    // - Kalahari logo
    // - One of 8 assistant animals (random)
    // - Loading progress bar
    // - Version information
}

} // namespace gui
} // namespace kalahari
