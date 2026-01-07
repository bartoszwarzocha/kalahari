/// @file diagnostic_controller.cpp
/// @brief Controller for diagnostic and dev mode functionality
///
/// Extracted from MainWindow as part of OpenSpec #00038 refactoring.

#include "kalahari/gui/diagnostic_controller.h"
#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/dialogs/icon_downloader_dialog.h"
#include "kalahari/gui/main_window.h"
#include "kalahari/editor/editor_benchmark.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"

#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QAction>
#include <QMessageBox>
#include <QSysInfo>
#include <QCoreApplication>
#include <QDir>
#include <QProgressDialog>

#include <spdlog/spdlog.h>

namespace kalahari {
namespace gui {

DiagnosticController::DiagnosticController(QMainWindow* mainWindow,
                                           LogPanel* logPanel,
                                           QDockWidget* logDock,
                                           QStatusBar* statusBar,
                                           QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_logPanel(logPanel)
    , m_logDock(logDock)
    , m_statusBar(statusBar)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("DiagnosticController created");
}

// =============================================================================
// Diagnostic Mode
// =============================================================================

void DiagnosticController::enableDiagnosticMode() {
    auto& logger = core::Logger::getInstance();
    logger.info("Enabling diagnostic mode");

    m_diagnosticMode = true;
    createDiagnosticMenu();

    // Set Logger level to TRACE to enable ALL log messages (OpenSpec #00024)
    // This must be done BEFORE setDiagnosticMode on LogPanel, otherwise
    // debug/trace messages won't reach the sink
    logger.setLevel(spdlog::level::trace);

    // Show log panel and enable all log levels (OpenSpec #00024)
    if (m_logPanel) {
        m_logPanel->setDiagnosticMode(true);
    }
    if (m_logDock) {
        m_logDock->show();
    }

    if (m_statusBar) {
        m_statusBar->showMessage(tr("Diagnostic mode enabled"), 3000);
    }

    emit diagnosticModeChanged(true);
}

void DiagnosticController::disableDiagnosticMode() {
    auto& logger = core::Logger::getInstance();
    logger.info("Disabling diagnostic mode");

    removeDiagnosticMenu();
    m_diagnosticMode = false;

    // Only restore log level and hide panel if dev mode is also disabled
    if (!m_devMode) {
        // Restore Logger level based on build type (OpenSpec #00024)
#ifdef NDEBUG
        logger.setLevel(spdlog::level::info);   // Release: info and above
#else
        logger.setLevel(spdlog::level::debug);  // Debug: debug and above
#endif

        // Hide log panel and filter to INFO+ only (OpenSpec #00024)
        if (m_logPanel) {
            m_logPanel->setDiagnosticMode(false);
        }
        if (m_logDock) {
            m_logDock->hide();
        }
    }

    if (m_statusBar) {
        m_statusBar->showMessage(tr("Diagnostic mode disabled"), 3000);
    }

    emit diagnosticModeChanged(false);
}

void DiagnosticController::onDiagModeChanged(bool enabled) {
    if (enabled) {
        enableDiagnosticMode();
    } else {
        disableDiagnosticMode();
    }
}

void DiagnosticController::createDiagnosticMenu() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating Diagnostics menu");

    // Don't create if already exists
    if (m_diagnosticMenu) {
        logger.warn("Diagnostics menu already exists");
        return;
    }

    if (!m_mainWindow) {
        logger.error("Cannot create Diagnostics menu: mainWindow is null");
        return;
    }

    // Create menu (inserted between Help and existing menus)
    m_diagnosticMenu = m_mainWindow->menuBar()->addMenu(tr("&Diagnostics"));

    // === CATEGORY 1: System Information ===
    QAction* sysInfoAction = m_diagnosticMenu->addAction(tr("System Information"));
    connect(sysInfoAction, &QAction::triggered, this, &DiagnosticController::onDiagSystemInfo);

    QAction* qtEnvAction = m_diagnosticMenu->addAction(tr("Qt Environment"));
    connect(qtEnvAction, &QAction::triggered, this, &DiagnosticController::onDiagQtEnvironment);

    QAction* fsCheckAction = m_diagnosticMenu->addAction(tr("File System Check"));
    connect(fsCheckAction, &QAction::triggered, this, &DiagnosticController::onDiagFileSystemCheck);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 2: Application State ===
    QAction* settingsDumpAction = m_diagnosticMenu->addAction(tr("Settings Dump"));
    connect(settingsDumpAction, &QAction::triggered, this, &DiagnosticController::onDiagSettingsDump);

    QAction* memStatsAction = m_diagnosticMenu->addAction(tr("Memory Statistics"));
    connect(memStatsAction, &QAction::triggered, this, &DiagnosticController::onDiagMemoryStats);

    QAction* openDocsAction = m_diagnosticMenu->addAction(tr("Open Documents Statistics"));
    connect(openDocsAction, &QAction::triggered, this, &DiagnosticController::onDiagOpenDocsStats);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 3: Core Systems ===
    QAction* loggerTestAction = m_diagnosticMenu->addAction(tr("Logger Test"));
    connect(loggerTestAction, &QAction::triggered, this, &DiagnosticController::onDiagLoggerTest);

    QAction* eventBusTestAction = m_diagnosticMenu->addAction(tr("Event Bus Test"));
    connect(eventBusTestAction, &QAction::triggered, this, &DiagnosticController::onDiagEventBusTest);

    QAction* pluginCheckAction = m_diagnosticMenu->addAction(tr("Plugin Manager Check"));
    connect(pluginCheckAction, &QAction::triggered, this, &DiagnosticController::onDiagPluginCheck);

    QAction* cmdRegistryDumpAction = m_diagnosticMenu->addAction(tr("Command Registry Dump"));
    connect(cmdRegistryDumpAction, &QAction::triggered, this, &DiagnosticController::onDiagCommandRegistryDump);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 4: Python Environment ===
    QAction* pyEnvAction = m_diagnosticMenu->addAction(tr("Python Environment"));
    connect(pyEnvAction, &QAction::triggered, this, &DiagnosticController::onDiagPythonEnvironment);

    QAction* pyImportAction = m_diagnosticMenu->addAction(tr("Python Import Test"));
    connect(pyImportAction, &QAction::triggered, this, &DiagnosticController::onDiagPythonImportTest);

    QAction* pyMemoryAction = m_diagnosticMenu->addAction(tr("Python Memory Test"));
    connect(pyMemoryAction, &QAction::triggered, this, &DiagnosticController::onDiagPythonMemoryTest);

    QAction* pyInterpAction = m_diagnosticMenu->addAction(tr("Embedded Interpreter Status"));
    connect(pyInterpAction, &QAction::triggered, this, &DiagnosticController::onDiagEmbeddedInterpreterStatus);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 5: Performance ===
    QAction* perfBenchAction = m_diagnosticMenu->addAction(tr("Performance Benchmark"));
    connect(perfBenchAction, &QAction::triggered, this, &DiagnosticController::onDiagPerformanceBenchmark);

    QAction* renderStatsAction = m_diagnosticMenu->addAction(tr("Render Statistics"));
    connect(renderStatsAction, &QAction::triggered, this, &DiagnosticController::onDiagRenderStats);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 6: Quick Actions ===
    QAction* clearLogAction = m_diagnosticMenu->addAction(tr("Clear Log"));
    connect(clearLogAction, &QAction::triggered, this, &DiagnosticController::onDiagClearLog);

#ifdef _DEBUG
    m_diagnosticMenu->addSeparator();
    QAction* forceCrashAction = m_diagnosticMenu->addAction(tr("Force Crash (Debug Only)"));
    connect(forceCrashAction, &QAction::triggered, this, &DiagnosticController::onDiagForceCrash);

    QAction* memLeakAction = m_diagnosticMenu->addAction(tr("Memory Leak Test (Debug Only)"));
    connect(memLeakAction, &QAction::triggered, this, &DiagnosticController::onDiagMemoryLeakTest);
#endif

    logger.debug("Diagnostics menu created successfully with 18 tools");
}

void DiagnosticController::removeDiagnosticMenu() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Removing Diagnostics menu");

    if (!m_diagnosticMenu) {
        logger.debug("Diagnostics menu doesn't exist, nothing to remove");
        return;
    }

    if (!m_mainWindow) {
        logger.error("Cannot remove Diagnostics menu: mainWindow is null");
        return;
    }

    // Remove from menu bar
    m_mainWindow->menuBar()->removeAction(m_diagnosticMenu->menuAction());

    // Delete menu
    delete m_diagnosticMenu;
    m_diagnosticMenu = nullptr;

    logger.debug("Diagnostics menu removed successfully");
}

// =============================================================================
// Diagnostic Tool Implementations
// =============================================================================

void DiagnosticController::onDiagSystemInfo() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: System Information ===");
    logger.info("OS: {}", QSysInfo::prettyProductName().toStdString());
    logger.info("Kernel: {}", QSysInfo::kernelType().toStdString() + " " + QSysInfo::kernelVersion().toStdString());
    logger.info("CPU Architecture: {}", QSysInfo::currentCpuArchitecture().toStdString());
    logger.info("Build ABI: {}", QSysInfo::buildAbi().toStdString());
    if (m_statusBar) {
        m_statusBar->showMessage(tr("System information logged"), 2000);
    }
}

void DiagnosticController::onDiagQtEnvironment() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Qt Environment ===");
    logger.info("Qt Version: {}", qVersion());
    logger.info("Qt Build Mode: {}",
#ifdef QT_DEBUG
        "Debug"
#else
        "Release"
#endif
    );
    logger.info("Application: {} {}",
        QCoreApplication::applicationName().toStdString(),
        QCoreApplication::applicationVersion().toStdString());
    logger.info("Organization: {}", QCoreApplication::organizationName().toStdString());
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Qt environment logged"), 2000);
    }
}

void DiagnosticController::onDiagFileSystemCheck() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: File System Check ===");
    logger.info("Working Directory: {}", QDir::currentPath().toStdString());
    logger.info("Application Path: {}", QCoreApplication::applicationDirPath().toStdString());
    logger.info("Temp Path: {}", QDir::tempPath().toStdString());
    logger.info("Home Path: {}", QDir::homePath().toStdString());
    if (m_statusBar) {
        m_statusBar->showMessage(tr("File system check logged"), 2000);
    }
}

void DiagnosticController::onDiagSettingsDump() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Settings Dump ===");
    auto& settings = core::SettingsManager::getInstance();
    logger.info("Theme: {}", settings.getTheme());
    logger.info("Language: {}", settings.getLanguage());
    logger.info("Editor Font: {}", settings.get<std::string>("editor.fontFamily", "N/A"));
    logger.info("Editor Font Size: {}", settings.get<int>("editor.fontSize", 0));
    logger.info("Tab Size: {}", settings.get<int>("editor.tabSize", 0));
    logger.info("Line Numbers: {}", settings.get<bool>("editor.lineNumbers", false));
    logger.info("Word Wrap: {}", settings.get<bool>("editor.wordWrap", false));
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Settings dumped to log"), 2000);
    }
}

void DiagnosticController::onDiagMemoryStats() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Memory Statistics ===");
    logger.info("NOTE: Detailed memory stats require platform-specific code");
    logger.info("Memory statistics logged (basic info only)");
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Memory statistics logged"), 2000);
    }
}

void DiagnosticController::onDiagOpenDocsStats() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Open Documents Statistics ===");
    logger.info("NOTE: Document statistics require MainWindow access");
    logger.info("Open documents statistics logged (basic info only)");
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Document statistics logged"), 2000);
    }
}

void DiagnosticController::onDiagLoggerTest() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Logger Test ===");
    logger.debug("DEBUG level message");
    logger.info("INFO level message");
    logger.warn("WARN level message");
    logger.error("ERROR level message");
    logger.critical("CRITICAL level message");
    logger.info("Logger test complete - check Log Panel for all levels");
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Logger test complete"), 2000);
    }
}

void DiagnosticController::onDiagEventBusTest() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Event Bus Test ===");
    logger.info("NOTE: Event Bus implementation pending (Phase 1)");
    logger.info("Event Bus test will be implemented when Event Bus is ready");
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Event Bus test logged"), 2000);
    }
}

void DiagnosticController::onDiagPluginCheck() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Plugin Manager Check ===");
    logger.info("NOTE: Plugin Manager implementation pending (Phase 2)");
    logger.info("Plugin check will be implemented when Plugin Manager is ready");
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Plugin check logged"), 2000);
    }
}

void DiagnosticController::onDiagCommandRegistryDump() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Command Registry Dump ===");
    logger.info("NOTE: CommandRegistry::dump() not yet implemented");
    logger.info("Command Registry diagnostic will be enhanced in future");
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Command Registry dump logged"), 2000);
    }
}

void DiagnosticController::onDiagPythonEnvironment() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Python Environment ===");
    logger.info("NOTE: Python environment check requires pybind11 integration");
    logger.info("This diagnostic will be implemented in Phase 2 (Plugin System)");
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Python environment check logged"), 2000);
    }
}

void DiagnosticController::onDiagPythonImportTest() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Python Import Test ===");
    logger.info("NOTE: Python import test requires embedded interpreter");
    logger.info("This diagnostic will be implemented in Phase 2 (Plugin System)");
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Python import test logged"), 2000);
    }
}

void DiagnosticController::onDiagPythonMemoryTest() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Python Memory Test ===");
    logger.info("NOTE: Python memory test requires embedded interpreter");
    logger.info("This diagnostic will be implemented in Phase 2 (Plugin System)");
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Python memory test logged"), 2000);
    }
}

void DiagnosticController::onDiagEmbeddedInterpreterStatus() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Embedded Interpreter Status ===");
    logger.info("NOTE: Embedded interpreter check requires pybind11 integration");
    logger.info("This diagnostic will be implemented in Phase 2 (Plugin System)");
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Interpreter status logged"), 2000);
    }
}

void DiagnosticController::onDiagPerformanceBenchmark() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Performance Benchmark ===");

    // Get current editor through MainWindow
    auto* mainWindow = qobject_cast<MainWindow*>(m_mainWindow);
    if (!mainWindow) {
        logger.error("Cannot run benchmark: MainWindow not available");
        if (m_statusBar) {
            m_statusBar->showMessage(tr("Benchmark failed: No main window"), 3000);
        }
        return;
    }

    EditorPanel* editorPanel = mainWindow->getCurrentEditor();
    if (!editorPanel) {
        logger.error("Cannot run benchmark: No editor open");
        QMessageBox::warning(m_mainWindow, tr("Benchmark"),
            tr("Please open a document before running the benchmark."));
        return;
    }

    editor::BookEditor* bookEditor = editorPanel->getBookEditor();
    if (!bookEditor) {
        logger.error("Cannot run benchmark: BookEditor not available");
        QMessageBox::warning(m_mainWindow, tr("Benchmark"),
            tr("BookEditor not available."));
        return;
    }

    // Confirm benchmark will modify document temporarily
    auto reply = QMessageBox::question(m_mainWindow, tr("Editor Benchmark"),
        tr("This benchmark will temporarily modify the editor content.\n"
           "The original content will NOT be preserved.\n\n"
           "Do you want to continue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        logger.info("Benchmark cancelled by user");
        return;
    }

    // Create and run benchmark
    editor::EditorBenchmark benchmark(bookEditor, this);
    benchmark.setIterations(500);      // Reduced for reasonable runtime
    benchmark.setWarmupIterations(50);

    // Show progress dialog
    QProgressDialog progress(tr("Running Editor Benchmark..."), tr("Cancel"), 0, 100, m_mainWindow);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();

    // Connect progress signal
    connect(&benchmark, &editor::EditorBenchmark::progressUpdated,
            [&progress](int current, int total) {
                progress.setValue(current * 100 / total);
                QCoreApplication::processEvents();
            });

    // Run all benchmarks
    logger.info("Starting editor performance benchmarks...");
    progress.setValue(0);

    auto results = benchmark.runAll();

    progress.setValue(100);
    progress.close();

    // Show summary in message box
    QString summary = tr("Editor Benchmark Results\n\n");
    for (const auto& result : results) {
        double refOps = editor::EditorBenchmark::referenceOpsPerSecond(result.name);
        QString status;
        if (refOps > 0) {
            double ratio = result.opsPerSecond / refOps;
            if (ratio >= 1.0) {
                status = QString(" [OK: %1x]").arg(ratio, 0, 'f', 1);
            } else {
                status = QString(" [SLOW: %1x]").arg(ratio, 0, 'f', 2);
            }
        }
        summary += QString("%1: %2 ops/sec%3\n")
            .arg(result.name)
            .arg(result.opsPerSecond, 0, 'f', 0)
            .arg(status);
    }
    summary += tr("\nDetails logged to Log Panel.");

    QMessageBox::information(m_mainWindow, tr("Benchmark Complete"), summary);

    if (m_statusBar) {
        m_statusBar->showMessage(tr("Performance benchmark complete - see log"), 5000);
    }
}

void DiagnosticController::onDiagRenderStats() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Render Statistics ===");
    logger.info("NOTE: Render statistics require Qt rendering metrics");
    logger.info("This diagnostic will show FPS, paint events, update regions, etc.");
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Render statistics logged"), 2000);
    }
}

void DiagnosticController::onDiagClearLog() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Clear Log ===");

    // Clear log panel
    if (m_logPanel) {
        m_logPanel->clear();
        logger.info("Log Panel cleared");
    }

    if (m_statusBar) {
        m_statusBar->showMessage(tr("Log cleared"), 2000);
    }
}

#ifdef _DEBUG
void DiagnosticController::onDiagForceCrash() {
    auto& logger = core::Logger::getInstance();
    logger.critical("=== DIAGNOSTIC: Force Crash (Debug Only) ===");
    logger.critical("User requested application crash - SIMULATING CRITICAL ERROR");

    // Show confirmation
    QMessageBox::StandardButton reply = QMessageBox::critical(m_mainWindow,
        tr("Force Crash"),
        tr("This will IMMEDIATELY crash the application!\n\n"
           "All unsaved work will be LOST.\n\n"
           "Are you sure you want to continue?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        logger.critical("Crashing application NOW!");
        std::abort();  // Immediate crash
    } else {
        logger.info("Crash cancelled by user");
        if (m_statusBar) {
            m_statusBar->showMessage(tr("Crash cancelled"), 2000);
        }
    }
}

void DiagnosticController::onDiagMemoryLeakTest() {
    auto& logger = core::Logger::getInstance();
    logger.warn("=== DIAGNOSTIC: Memory Leak Test (Debug Only) ===");

    // Intentionally leak memory for testing
    const size_t leakSize = 1024 * 1024;  // 1 MB
    char* leak = new char[leakSize];
    (void)leak;  // Suppress unused variable warning

    logger.warn("Leaked {} bytes of memory (intentional)", leakSize);
    logger.info("Use Valgrind/AddressSanitizer to detect this leak");
    if (m_statusBar) {
        m_statusBar->showMessage(tr("Memory leak created (1 MB)"), 3000);
    }
}
#endif

// =============================================================================
// Dev Tools Mode
// =============================================================================

void DiagnosticController::enableDevMode() {
    auto& logger = core::Logger::getInstance();
    logger.info("Enabling dev mode");

    m_devMode = true;
    createDevToolsMenu();

    // Dev mode also enables full logging like diagnostic mode (OpenSpec #00024)
    logger.setLevel(spdlog::level::trace);

    // Show log panel and enable all log levels
    if (m_logPanel) {
        m_logPanel->setDiagnosticMode(true);
    }
    if (m_logDock) {
        m_logDock->show();
    }

    if (m_statusBar) {
        m_statusBar->showMessage(tr("Dev mode enabled"), 3000);
    }

    emit devModeChanged(true);
}

void DiagnosticController::disableDevMode() {
    auto& logger = core::Logger::getInstance();
    logger.info("Disabling dev mode");

    m_devMode = false;
    removeDevToolsMenu();

    // Only restore log level and hide panel if diagnostic mode is also disabled
    if (!m_diagnosticMode) {
        // Restore Logger level based on build type (OpenSpec #00024)
#ifdef NDEBUG
        logger.setLevel(spdlog::level::info);   // Release: info and above
#else
        logger.setLevel(spdlog::level::debug);  // Debug: debug and above
#endif

        // Hide log panel and filter to INFO+ only
        if (m_logPanel) {
            m_logPanel->setDiagnosticMode(false);
        }
        if (m_logDock) {
            m_logDock->hide();
        }
    }

    if (m_statusBar) {
        m_statusBar->showMessage(tr("Dev mode disabled"), 3000);
    }

    emit devModeChanged(false);
}

void DiagnosticController::createDevToolsMenu() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating Dev Tools menu");

    // Don't create if already exists
    if (m_devToolsMenu) {
        logger.warn("Dev Tools menu already exists");
        return;
    }

    if (!m_mainWindow) {
        logger.error("Cannot create Dev Tools menu: mainWindow is null");
        return;
    }

    // Create menu (inserted before Help menu)
    m_devToolsMenu = m_mainWindow->menuBar()->addMenu(tr("&Dev Tools"));

    // === Icon Downloader ===
    QAction* iconDownloaderAction = m_devToolsMenu->addAction(tr("Icon Downloader"));
    iconDownloaderAction->setToolTip(tr("Download Material Design icons for the project"));
    connect(iconDownloaderAction, &QAction::triggered, this, &DiagnosticController::onDevToolsIconDownloader);

    logger.info("Dev Tools menu created");
}

void DiagnosticController::removeDevToolsMenu() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Removing Dev Tools menu");

    if (!m_devToolsMenu) {
        logger.debug("Dev Tools menu doesn't exist, nothing to remove");
        return;
    }

    if (!m_mainWindow) {
        logger.error("Cannot remove Dev Tools menu: mainWindow is null");
        return;
    }

    // Remove from menu bar
    m_mainWindow->menuBar()->removeAction(m_devToolsMenu->menuAction());

    // Delete menu
    delete m_devToolsMenu;
    m_devToolsMenu = nullptr;

    logger.info("Dev Tools menu removed");
}

// =============================================================================
// Dev Tools Implementations
// =============================================================================

void DiagnosticController::onDevToolsIconDownloader() {
    auto& logger = core::Logger::getInstance();
    logger.info("Opening Icon Downloader dialog");

    IconDownloaderDialog dialog(m_mainWindow);
    dialog.exec();

    logger.info("Icon Downloader dialog closed");
}

} // namespace gui
} // namespace kalahari
