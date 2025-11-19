/// @file main.cpp
/// @brief Kalahari Writer's IDE - Main entry point (Qt6)
///
/// NOTE: GUI layer removed during Qt migration (2025-11-19, Step 0.2)
/// This is a minimal placeholder. Qt GUI will be rebuilt in Phase 0 Week 1.

#include <QApplication>
#include <QMessageBox>
#include "core/logger.h"
#include "core/settings_manager.h"

int main(int argc, char *argv[]) {
    // Initialize Qt application
    QApplication app(argc, argv);
    app.setApplicationName("Kalahari");
    app.setOrganizationName("Kalahari Project");
    app.setApplicationVersion("0.3.0-alpha");

    // Initialize core systems
    kalahari::core::Logger::initialize("kalahari.log");
    kalahari::core::SettingsManager::initialize("settings.json");

    auto& logger = kalahari::core::Logger::getInstance();
    logger.info("Kalahari {} starting (Qt migration placeholder)", app.applicationVersion().toStdString());

    // Show placeholder message
    QMessageBox::information(
        nullptr,
        "Kalahari - Qt Migration",
        "Kalahari Writer's IDE\n\n"
        "Qt6 migration in progress.\n"
        "GUI will be rebuilt in Phase 0 Week 1.\n\n"
        "Core systems initialized successfully."
    );

    logger.info("Application closed (placeholder)");
    return 0;
}
