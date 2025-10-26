/// @file kalahari_app.h
/// @brief Main application class for Kalahari Writer's IDE
///
/// This file contains the wxApp-derived application class that manages
/// the application lifecycle, initialization, and cleanup.

#pragma once

#include <wx/wx.h>

namespace kalahari {
namespace gui {

// Forward declarations
class MainWindow;

/// @brief Main application class
///
/// Manages application initialization, event loop, and cleanup.
/// Follows wxWidgets application architecture with OnInit/OnExit overrides.
class KalahariApp : public wxApp {
public:
    /// @brief Initialize the application
    /// @return true if initialization succeeded, false to abort startup
    ///
    /// Called by wxWidgets on application startup. Initializes logging,
    /// sets application metadata, and creates the main window.
    virtual bool OnInit() override;

    /// @brief Cleanup on application exit
    /// @return Exit code for the application
    ///
    /// Called by wxWidgets when application is terminating.
    /// Performs cleanup tasks and flushes logs.
    virtual int OnExit() override;

private:
    /// @brief Initialize logging subsystem
    ///
    /// Creates log file in platform-appropriate location
    /// (e.g., %APPDATA%/Kalahari/logs on Windows, ~/.config/kalahari/logs on Linux)
    void initializeLogging();

    /// @brief Show splash screen (placeholder for future implementation)
    ///
    /// Will be implemented in Phase 1 to show loading progress
    /// and one of the 8 assistant animals.
    void showSplashScreen();

    /// @brief Main application window (owned by wxWidgets, deleted automatically)
    MainWindow* m_mainWindow = nullptr;
};

} // namespace gui
} // namespace kalahari
