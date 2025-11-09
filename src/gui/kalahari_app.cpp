/// @file kalahari_app.cpp
/// @brief Implementation of KalahariApp

#include "kalahari_app.h"
#include "main_window.h"
#include <kalahari/core/document.h>  // Required for std::unique_ptr<Document> in MainWindow
#include <kalahari/core/logger.h>
#include <kalahari/core/python_interpreter.h>
#include <kalahari/core/cmd_line_parser.h>
#include <kalahari/core/diagnostic_manager.h>
#include <kalahari/core/settings_manager.h>  // For theme settings
#include <bwx_sdk/bwx_core/bwx_exception.h>
#include <bwx_sdk/bwx_core/bwx_exception_handler.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>

#ifndef _WIN32
// VirtualBox specific includes would go here if needed
#endif

namespace kalahari {
namespace gui {

bool KalahariApp::OnInit() {
    // 0. Parse command line arguments (BEFORE logging initialization)
    // wxApp provides argc/argv through wxCmdLineParser-compatible interface
    wxCmdLineParser cmdLine(wxApp::argc, wxApp::argv);
    cmdLine.AddSwitch("d", "diag", "Enable diagnostic mode");
    cmdLine.AddSwitch("", "test-python", "Auto-run Python integration tests on startup (for testing)");
    cmdLine.SetSwitchChars("-");

    // Parse (don't use CmdLineParser yet - too early for Logger)
    if (cmdLine.Parse() != 0) {
        // Help was shown or error occurred
        return false;
    }

    // Check if diagnostic mode was requested
    bool diagMode = (cmdLine.FoundSwitch("diag") == wxCMD_SWITCH_ON);
    core::DiagnosticManager::getInstance().setEnabled(diagMode);

    // Check if Python test mode was requested
    bool testPython = (cmdLine.FoundSwitch("test-python") == wxCMD_SWITCH_ON);

    // 1. Initialize logging FIRST (before any other operations)
    initializeLogging();
    core::Logger::getInstance().info("=================================================");
    core::Logger::getInstance().info("Kalahari Writer's IDE starting...");
    core::Logger::getInstance().info("=================================================");

    if (diagMode) {
        core::Logger::getInstance().info("========================================");
        core::Logger::getInstance().info("DIAGNOSTIC MODE ENABLED");
        core::Logger::getInstance().info("========================================");
    }

    // 2. Initialize Python interpreter (BEFORE wxWidgets initialization)
    // PythonInterpreter auto-detects Python location - no manual configuration needed
    try {
        core::PythonInterpreter::getInstance().initialize();
        core::Logger::getInstance().info("Python {} initialized successfully",
                                         core::PythonInterpreter::getInstance().getPythonVersion());
    } catch (const std::exception& e) {
        core::Logger::getInstance().error("Failed to initialize Python: {}", e.what());
        wxMessageBox(wxString::Format("Failed to initialize Python interpreter:\n%s\n\nPlugin system will be unavailable.",
                                      e.what()),
                    "Python Initialization Error",
                    wxOK | wxICON_ERROR);
        // Continue without Python - core features still work
    }

    // 3. Set application metadata (used by wxConfig, wxStandardPaths, etc.)
    SetAppName("Kalahari");
    SetVendorName("Kalahari Project");
    SetAppDisplayName("Kalahari Writer's IDE");

    core::Logger::getInstance().info("Application metadata set (vendor: {}, app: {})",
                                     GetVendorName().utf8_str().data(),
                                     GetAppName().utf8_str().data());

    // 3.5. Apply appearance theme (wxWidgets 3.3+ dark mode support)
    // IMPORTANT: Must be called BEFORE window creation for proper dark mode rendering
    core::SettingsManager& settingsMgr = core::SettingsManager::getInstance();
    settingsMgr.load();  // CRITICAL: Load settings.json BEFORE reading theme
    wxString themeName = wxString::FromUTF8(
        settingsMgr.get<std::string>("appearance.theme", "System")
    );

    // Apply theme using wxWidgets 3.3 SetAppearance() API
    if (themeName == "Dark") {
        SetAppearance(wxApp::Appearance::Dark);
        core::Logger::getInstance().info("Appearance theme set to: Dark (forced)");
    } else if (themeName == "Light") {
        SetAppearance(wxApp::Appearance::Light);
        core::Logger::getInstance().info("Appearance theme set to: Light (forced)");
    } else {
        // "System" or unknown value - follow OS preference
        SetAppearance(wxApp::Appearance::System);
        core::Logger::getInstance().info("Appearance theme set to: System (follow OS)");
    }

    // 4. Initialize wxWidgets image handlers (needed for toolbar icons, etc.)
    wxInitAllImageHandlers();
    core::Logger::getInstance().debug("Image handlers initialized");

    // 5. Show splash screen (placeholder for future - Phase 1)
    // showSplashScreen();

    // 6. Create and show main window
    core::Logger::getInstance().info("Creating main window...");
    m_mainWindow = new MainWindow();

    if (!m_mainWindow) {
        core::Logger::getInstance().critical("Failed to create main window!");
        return false;
    }

    m_mainWindow->Show(true);
    core::Logger::getInstance().info("Main window created and shown successfully");

    // Auto-run Python tests if requested (for testing hang scenario)
    if (testPython && core::PythonInterpreter::getInstance().isInitialized()) {
        core::Logger::getInstance().info("========================================");
        core::Logger::getInstance().info("AUTO-RUNNING PYTHON INTEGRATION TESTS");
        core::Logger::getInstance().info("========================================");
        std::string result = core::PythonInterpreter::getInstance().executeTest();
        core::Logger::getInstance().info("Python test results:\n{}", result);
        core::Logger::getInstance().info("========================================");
        core::Logger::getInstance().info("Python tests complete - application will continue running");
        core::Logger::getInstance().info("Close window to test shutdown behavior");
        core::Logger::getInstance().info("========================================");
    }

    // Success!
    return true;
}

int KalahariApp::OnExit() {
    core::Logger::getInstance().info("=================================================");
    core::Logger::getInstance().info("Kalahari Writer's IDE shutting down...");
    core::Logger::getInstance().info("=================================================");

    // Finalize Python interpreter (AFTER wxWidgets cleanup)
    if (core::PythonInterpreter::getInstance().isInitialized()) {
        core::Logger::getInstance().info("Finalizing Python interpreter...");
        core::PythonInterpreter::getInstance().finalize();
        core::Logger::getInstance().info("Python finalized successfully");
    }

    // Flush logs before exit
    core::Logger::getInstance().flush();

#ifndef _WIN32
    // VirtualBox shared folder workaround: force immediate process termination
    // bypassing wxWidgets atexit handlers that hang on vboxsf filesystem
    // All cleanup is already done (Python finalized, logs flushed, settings saved)
    core::Logger::getInstance().debug("Using std::_Exit(0) to bypass hanging atexit handlers");
    core::Logger::getInstance().flush();

    std::_Exit(0);  // Immediate exit without calling atexit handlers
#endif

    // Return 0 for successful exit (Windows only reaches here)
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

bool KalahariApp::OnExceptionInMainLoop() {
    // This method is called by wxWidgets when an exception is thrown in the main event loop.
    // Standard C++ try-catch doesn't work across event boundaries in wxWidgets,
    // so this is the proper way to handle exceptions.

    auto& logger = core::Logger::getInstance();

    try {
        // Rethrow the caught exception to identify its type
        throw;
    }
    catch (const bwx::core::LayoutException& e) {
        // Layout exceptions (FitInside, sizers, etc.) - usually non-critical
        logger.error("Layout exception caught in main loop: {}", e.what());

        bwx::core::ExceptionHandler::logException(e,
            [&logger]([[maybe_unused]] bwx::core::ExceptionSeverity severity,
                     [[maybe_unused]] const std::string& message,
                     const std::string& details) {
                logger.error("Exception details: {}", details);
            });

        // Show user-friendly error dialog
        wxMessageBox(
            wxString::Format("A layout error occurred:\n\n%s\n\nThe application will continue, but some windows may not display correctly.",
                           e.what()),
            bwx::core::ExceptionHandler::getErrorTitle(e),
            wxOK | wxICON_WARNING
        );

        return true;  // Continue execution
    }
    catch (const bwx::core::WidgetException& e) {
        logger.error("Widget exception caught in main loop: {}", e.what());

        bwx::core::ExceptionHandler::logException(e,
            [&logger]([[maybe_unused]] bwx::core::ExceptionSeverity severity,
                     [[maybe_unused]] const std::string& message,
                     const std::string& details) {
                logger.error("Exception details: {}", details);
            });

        wxMessageBox(
            wxString::Format("A widget error occurred:\n\n%s\n\nThe application will continue.",
                           e.what()),
            bwx::core::ExceptionHandler::getErrorTitle(e),
            wxOK | wxICON_WARNING
        );

        return true;  // Continue execution
    }
    catch (const bwx::core::WindowException& e) {
        logger.error("Window exception caught in main loop: {}", e.what());

        bwx::core::ExceptionHandler::logException(e,
            [&logger]([[maybe_unused]] bwx::core::ExceptionSeverity severity,
                     [[maybe_unused]] const std::string& message,
                     const std::string& details) {
                logger.error("Exception details: {}", details);
            });

        wxMessageBox(
            wxString::Format("A window error occurred:\n\n%s\n\nThe application will continue.",
                           e.what()),
            bwx::core::ExceptionHandler::getErrorTitle(e),
            wxOK | wxICON_WARNING
        );

        return true;  // Continue execution
    }
    catch (const bwx::core::ResourceException& e) {
        logger.error("Resource exception caught in main loop: {}", e.what());

        bwx::core::ExceptionHandler::logException(e,
            [&logger]([[maybe_unused]] bwx::core::ExceptionSeverity severity,
                     [[maybe_unused]] const std::string& message,
                     const std::string& details) {
                logger.error("Exception details: {}", details);
            });

        wxMessageBox(
            wxString::Format("A resource error occurred:\n\n%s\n\nThe application will continue.",
                           e.what()),
            bwx::core::ExceptionHandler::getErrorTitle(e),
            wxOK | wxICON_WARNING
        );

        return true;  // Continue execution
    }
    catch (const bwx::core::BwxException& e) {
        // Generic bwx exception
        logger.error("BWX exception caught in main loop: {}", e.what());

        bwx::core::ExceptionHandler::logException(e,
            [&logger]([[maybe_unused]] bwx::core::ExceptionSeverity severity,
                     [[maybe_unused]] const std::string& message,
                     const std::string& details) {
                logger.error("Exception details: {}", details);
            });

        wxMessageBox(
            wxString::Format("An application error occurred:\n\n%s\n\nThe application will continue.",
                           e.what()),
            bwx::core::ExceptionHandler::getErrorTitle(e),
            wxOK | wxICON_WARNING
        );

        return true;  // Continue execution
    }
    catch (const std::exception& e) {
        // Standard C++ exception - potentially critical
        logger.critical("Unhandled std::exception in main loop: {}", e.what());

        int result = wxMessageBox(
            wxString::Format("An unexpected error occurred:\n\n%s\n\nContinue running? (Not recommended)",
                           e.what()),
            "Critical Error",
            wxYES_NO | wxICON_ERROR | wxNO_DEFAULT
        );

        return (result == wxYES);  // Let user decide
    }
    catch (...) {
        // Unknown exception type - very critical
        logger.critical("Unknown exception caught in main loop!");

        int result = wxMessageBox(
            "An unknown critical error occurred.\n\nContinue running? (Not recommended)",
            "Critical Error",
            wxYES_NO | wxICON_ERROR | wxNO_DEFAULT
        );

        return (result == wxYES);  // Let user decide
    }
}

} // namespace gui
} // namespace kalahari
