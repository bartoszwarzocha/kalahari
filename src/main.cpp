/// @file main.cpp
/// @brief Kalahari Writer's IDE - Main entry point (Qt6)
///
/// Task #00002: QMainWindow Structure with Menus and Toolbars

#include <QApplication>
#include "kalahari/gui/main_window.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"

int main(int argc, char *argv[]) {
    // Initialize Qt application
    QApplication app(argc, argv);
    app.setApplicationName("Kalahari");
    app.setOrganizationName("Bartosz W. Warzocha & Kalahari Team");
    app.setApplicationVersion("0.3.0-alpha");

    // Initialize core systems
    kalahari::core::Logger::initialize("kalahari.log");
    kalahari::core::SettingsManager::initialize("settings.json");

    auto& logger = kalahari::core::Logger::getInstance();
    logger.info("Kalahari {} starting", app.applicationVersion().toStdString());

    // Create main window with menus/toolbars
    kalahari::gui::MainWindow window;
    window.show();

    logger.info("Main window shown - entering event loop");

    // Enter Qt event loop
    int result = app.exec();

    logger.info("Application exited with code: {}", result);
    return result;
}
