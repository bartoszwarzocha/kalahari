/// @file main.cpp
/// @brief Kalahari Writer's IDE - Main entry point (Qt6)
///
/// Task #00002: QMainWindow Structure with Menus and Toolbars
/// Task #00018: Diagnostic Mode with --diag parameter
/// Task #00020: Icon Downloader Tool (CLI mode) - TEMPORARILY DISABLED

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QEventLoop>
#include <QTimer>
#include "kalahari/gui/main_window.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/core/cmd_line_parser.h"
// TEMPORARY: IconDownloader disabled (moc linkage issue from Task #00020)
// #include "kalahari/core/utils/icon_downloader.h"
#include "kalahari/core/utils/svg_converter.h"

#if 0  // TEMPORARY: IconDownloader CLI mode disabled (moc linkage issue from Task #00020)
// ============================================================================
// DownloadHelper - Qt Signal/Slot helper for CLI icon downloads
// ============================================================================
// This helper class avoids DLL export issues with Qt signals on Windows.
// Using old-style SIGNAL/SLOT macros doesn't require staticMetaObject export.
class DownloadHelper : public QObject {
    Q_OBJECT

public:
    DownloadHelper(QObject* parent = nullptr)
        : QObject(parent), downloadSuccess(false) {}

    bool downloadSuccess;
    QString downloadedSvg;
    QString expectedTheme;

public slots:
    void onComplete(const QString& theme, const QString& svgData) {
        if (theme == expectedTheme) {
            downloadedSvg = svgData;
            downloadSuccess = true;
        }
    }

    void onError(const QString& /*iconName*/, const QString& theme, const QString& error) {
        if (theme == expectedTheme) {
            auto& logger = kalahari::core::Logger::getInstance();
            logger.error("CLI: Download failed: {}", error.toStdString());
            fprintf(stderr, "Error: %s\n", error.toUtf8().constData());
            downloadSuccess = false;
        }
    }
};
#endif  // DownloadHelper class disabled

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

    // Initialize IconRegistry (Task #00021)
    auto& iconRegistry = kalahari::core::IconRegistry::getInstance();
    iconRegistry.initialize();  // Load theme/sizes/customizations from settings

    // Parse command line arguments (Task #00018, #00020)
    kalahari::core::CmdLineParser cmdLine(argc, argv);
    cmdLine.setApplicationDescription("Kalahari", "Writer's IDE for book authors");
    cmdLine.addSwitch("", "cli", "Run in CLI mode (no GUI)");
    cmdLine.addSwitch("d", "diag", "Enable diagnostic mode (show Diagnostics menu)");
    cmdLine.addSwitch("", "dev", "Enable developer tools (Dev Tools menu + CLI features)");
    cmdLine.addOption("", "get-icon", "Download Material Design icon (requires --cli --dev)", "name");
    cmdLine.addOption("", "get-icons", "Download multiple icons (comma-separated, requires --cli --dev)", "list");
    cmdLine.addOption("", "themes", "Icon themes to download (comma-separated, default: twotone,rounded,outlined)", "list");
    cmdLine.addOption("", "source", "Custom Material Design source URL (default: GitHub Raw)", "url");

    if (!cmdLine.parse()) {
        // Parsing failed (error or --help requested)
        logger.info("Command line parsing failed or help requested");
        return 0;
    }

#if 0  // TEMPORARY: IconDownloader CLI mode disabled (moc linkage issue from Task #00020)
    // ========================================================================
    // CLI Mode: Icon Downloader (Task #00020)
    // ========================================================================
    if (cmdLine.hasSwitch("cli")) {
        logger.info("CLI Mode activated");

        // Get icon name(s)
        QStringList iconNames;
        if (cmdLine.hasOption("get-icon")) {
            iconNames << cmdLine.getOptionValue("get-icon");
        } else if (cmdLine.hasOption("get-icons")) {
            QString iconList = cmdLine.getOptionValue("get-icons");
            iconNames = iconList.split(',', Qt::SkipEmptyParts);
        }

        // Get themes (default: all 3)
        QStringList themes;
        if (cmdLine.hasOption("themes")) {
            QString themeList = cmdLine.getOptionValue("themes");
            themes = themeList.split(',', Qt::SkipEmptyParts);
        } else {
            themes << "twotone" << "rounded" << "outlined";
        }

        // Get source URL (default: GitHub Raw)
        QString sourceUrl = kalahari::core::IconDownloader::getDefaultSourceUrl();
        if (cmdLine.hasOption("source")) {
            sourceUrl = cmdLine.getOptionValue("source");
        }

        logger.info("CLI: Downloading {} icon(s) in {} theme(s)", iconNames.size(), themes.size());

        // Create icon downloader
        kalahari::core::IconDownloader downloader(sourceUrl);
        kalahari::core::SvgConverter converter;

        // Track success/failure
        int downloadedCount = 0;
        int failedCount = 0;

        // Download each icon
        for (const QString& iconName : iconNames) {
            for (const QString& theme : themes) {
                logger.info("CLI: Downloading '{}'  (theme: {})", iconName.toStdString(), theme.toStdString());

                // Create helper for signal/slot handling (avoids DLL export issues)
                DownloadHelper helper;
                helper.expectedTheme = theme;
                helper.downloadSuccess = false;
                helper.downloadedSvg.clear();

                // Use QEventLoop for synchronous download
                QEventLoop loop;

                // Connect signals using old-style SIGNAL/SLOT (no staticMetaObject export needed)
                QObject::connect(&downloader, SIGNAL(downloadComplete(QString,QString)),
                               &helper, SLOT(onComplete(QString,QString)));
                QObject::connect(&downloader, SIGNAL(downloadError(QString,QString,QString)),
                               &helper, SLOT(onError(QString,QString,QString)));
                QObject::connect(&downloader, SIGNAL(downloadComplete(QString,QString)),
                               &loop, SLOT(quit()));
                QObject::connect(&downloader, SIGNAL(downloadError(QString,QString,QString)),
                               &loop, SLOT(quit()));

                // Start download
                downloader.downloadIcon(iconName, {theme});

                // Wait for completion (with 15 second timeout)
                QTimer::singleShot(15000, &loop, &QEventLoop::quit);
                loop.exec();

                if (!helper.downloadSuccess) {
                    failedCount++;
                    continue;
                }

                // Convert SVG
                auto conversionResult = converter.convertToTemplate(helper.downloadedSvg);
                if (!conversionResult.success) {
                    logger.error("CLI: Conversion failed: {}", conversionResult.errorMessage.toStdString());
                    fprintf(stderr, "Error: Conversion failed: %s\n",
                           conversionResult.errorMessage.toUtf8().constData());
                    failedCount++;
                    continue;
                }

                // Ensure directory exists
                QString dirPath = QString("resources/icons/%1").arg(theme);
                QDir dir;
                if (!dir.exists(dirPath)) {
                    if (!dir.mkpath(dirPath)) {
                        logger.error("CLI: Failed to create directory: {}", dirPath.toStdString());
                        fprintf(stderr, "Error: Cannot create directory: %s\n", dirPath.toUtf8().constData());
                        failedCount++;
                        continue;
                    }
                    logger.info("CLI: Created directory: {}", dirPath.toStdString());
                }

                // Save SVG to file
                QString filePath = QString("resources/icons/%1/%2.svg").arg(theme, iconName);
                QFile file(filePath);
                if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    logger.error("CLI: Failed to open file for writing: {}", filePath.toStdString());
                    fprintf(stderr, "Error: Cannot write to file: %s\n", filePath.toUtf8().constData());
                    failedCount++;
                    continue;
                }

                file.write(conversionResult.svg.toUtf8());
                file.close();

                logger.info("CLI: ✓ Saved {} ({} bytes)", filePath.toStdString(), conversionResult.svg.length());
                fprintf(stdout, "✓ Downloaded: %s\n", filePath.toUtf8().constData());
                downloadedCount++;
            }
        }

        // Print summary
        logger.info("CLI: Download complete - {} succeeded, {} failed", downloadedCount, failedCount);
        fprintf(stdout, "\nSummary: %d downloaded, %d failed\n", downloadedCount, failedCount);

        // Exit without starting GUI
        return (failedCount == 0) ? 0 : 1;
    }
#endif  // IconDownloader CLI mode disabled

    // Create main window with menus/toolbars
    kalahari::gui::MainWindow window;

    // Enable diagnostic mode if --diag flag present (Task #00018)
    if (cmdLine.isDiagnosticMode()) {
        logger.info("Diagnostic mode enabled via --diag flag");
        window.enableDiagnosticMode();
    }

    // Enable dev mode if --dev flag present (Task #00020)
    if (cmdLine.hasSwitch("dev")) {
        logger.info("Dev mode enabled via --dev flag");
        window.enableDevMode();
    }

    window.show();

    logger.info("Main window shown - entering event loop");

    // Enter Qt event loop
    int result = app.exec();

    logger.info("Application exited with code: {}", result);
    return result;
}

// TEMPORARY: MOC disabled (DownloadHelper class disabled)
// #include "main.moc"
