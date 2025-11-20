/// @file main.cpp
/// @brief Kalahari Writer's IDE - Main entry point (Qt6)
///
/// Task #00001: Qt6 Hello World - Minimal QMainWindow

#include <QApplication>
#include <QMainWindow>
#include "core/logger.h"
#include "core/settings_manager.h"

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
    logger.info("Kalahari {} starting (Qt6 Hello World)", app.applicationVersion().toStdString());

    // Create minimal main window
    QMainWindow window;
    window.setWindowTitle("Kalahari Writer's IDE");
    window.resize(1280, 720);
    window.show();

    logger.info("Main window shown - entering event loop");

    // Enter Qt event loop
    int result = app.exec();

    logger.info("Application exited with code: {}", result);
    return result;
}
