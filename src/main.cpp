/// @file main.cpp
/// @brief Kalahari Writer's IDE - Main entry point (Qt6)
///
/// Task #00002: QMainWindow Structure with Menus and Toolbars
/// Task #00018: Diagnostic Mode with --diag parameter

#include <QApplication>
#include "kalahari/gui/main_window.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/cmd_line_parser.h"

int main(int argc, char *argv[]) {
    // Initialize Qt application
    QApplication app(argc, argv);
    app.setApplicationName("Kalahari");
    app.setOrganizationName("Bartosz W. Warzocha & Kalahari Team");
    app.setApplicationVersion("0.3.0-alpha");

    // Initialize core systems
    auto& logger = kalahari::core::Logger::getInstance();
    logger.init("kalahari.log");

    auto& settings = kalahari::core::SettingsManager::getInstance();
    settings.load();  // Load settings from disk
    logger.info("Kalahari {} starting", app.applicationVersion().toStdString());

    // Parse command line arguments (Task #00018)
    kalahari::core::CmdLineParser cmdLine(argc, argv);
    cmdLine.setApplicationDescription("Kalahari", "Writer's IDE for book authors");
    cmdLine.addSwitch("d", "diag", "Enable diagnostic mode (show Diagnostics menu)");

    if (!cmdLine.parse()) {
        // Parsing failed (error or --help requested)
        logger.info("Command line parsing failed or help requested");
        return 0;
    }

    // Create main window with menus/toolbars
    kalahari::gui::MainWindow window;

    // Enable diagnostic mode if --diag flag present (Task #00018)
    if (cmdLine.isDiagnosticMode()) {
        logger.info("Diagnostic mode enabled via --diag flag");
        window.enableDiagnosticMode();
    }

    window.show();

    logger.info("Main window shown - entering event loop");

    // Enter Qt event loop
    int result = app.exec();

    logger.info("Application exited with code: {}", result);
    return result;
}
