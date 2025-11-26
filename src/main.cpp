/// @file main.cpp
/// @brief Kalahari Writer's IDE - Main entry point (Qt6)
///
/// Task #00002: QMainWindow Structure with Menus and Toolbars
/// Task #00018: Diagnostic Mode with --diag parameter
/// Task #00020: Icon Downloader Tool (CLI mode)

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QEventLoop>
#include <QTimer>
#include "kalahari/gui/main_window.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/cmd_line_parser.h"
#include "kalahari/core/utils/icon_downloader.h"
#include "kalahari/core/utils/svg_converter.h"
#include "kalahari/gui/kalahari_style.h"

// ============================================================================
// DownloadHelper - Qt Signal/Slot helper for CLI icon downloads
// ============================================================================
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

    void onError(const QString& /*url*/, const QString& error) {
        auto& logger = kalahari::core::Logger::getInstance();
        logger.error("CLI: Download failed: {}", error.toStdString());
        fprintf(stderr, "Error: %s\n", error.toUtf8().constData());
        downloadSuccess = false;
    }
};

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
    settings.load();
    logger.info("Kalahari {} starting", app.applicationVersion().toStdString());

    // Set KalahariStyle (wraps Fusion) for dynamic icon sizing
    // Must be set BEFORE ThemeManager applies QPalette
    // OpenSpec #00026: KalahariStyle reads icon sizes from ArtProvider
    app.setStyle(new kalahari::gui::KalahariStyle());

    // Initialize IconRegistry (triggers ThemeManager initialization)
    auto& iconRegistry = kalahari::core::IconRegistry::getInstance();
    iconRegistry.initialize();

    // Initialize ArtProvider (central visual resource manager)
    // OpenSpec #00026: Must be after IconRegistry
    auto& artProvider = kalahari::core::ArtProvider::getInstance();
    artProvider.initialize();

    // Parse command line arguments
    kalahari::core::CmdLineParser cmdLine(argc, argv);
    cmdLine.setApplicationDescription("Kalahari", "Writer's IDE for book authors");
    cmdLine.addSwitch("", "cli", "Run in CLI mode (no GUI)");
    cmdLine.addSwitch("d", "diag", "Enable diagnostic mode (show Diagnostics menu)");
    cmdLine.addSwitch("", "dev", "Enable developer tools (Dev Tools menu + CLI features)");
    cmdLine.addOption("", "get-icon", "Download icon from URL (requires --cli)", "url");
    cmdLine.addOption("", "icon-name", "Output icon name (required with --get-icon)", "name");
    cmdLine.addOption("", "theme", "Target theme: twotone, rounded, outlined (default: twotone)", "theme");

    if (!cmdLine.parse()) {
        logger.info("Command line parsing failed or help requested");
        return 0;
    }

    // ========================================================================
    // CLI Mode: Icon Downloader
    // ========================================================================
    if (cmdLine.hasSwitch("cli") && cmdLine.hasOption("get-icon")) {
        logger.info("CLI Mode: Icon download");

        QString url = cmdLine.getOptionValue("get-icon");
        QString iconName = cmdLine.getOptionValue("icon-name");
        QString theme = cmdLine.hasOption("theme") ? cmdLine.getOptionValue("theme") : "twotone";

        // Validate inputs
        if (url.isEmpty()) {
            fprintf(stderr, "Error: --get-icon requires a URL\n");
            return 1;
        }

        if (iconName.isEmpty()) {
            fprintf(stderr, "Error: --icon-name is required\n");
            fprintf(stderr, "Usage: kalahari --cli --get-icon URL --icon-name NAME [--theme THEME]\n");
            return 1;
        }

        if (!url.startsWith("http://") && !url.startsWith("https://")) {
            fprintf(stderr, "Error: URL must start with http:// or https://\n");
            return 1;
        }

        logger.info("CLI: Downloading from {} -> {}/{}.svg", url.toStdString(), theme.toStdString(), iconName.toStdString());
        fprintf(stdout, "Downloading: %s\n", url.toUtf8().constData());

        // Create downloader
        kalahari::core::IconDownloader downloader;
        kalahari::core::SvgConverter converter;

        // Create helper for signal/slot handling
        DownloadHelper helper;
        helper.expectedTheme = theme;
        helper.downloadSuccess = false;
        helper.downloadedSvg.clear();

        // Use QEventLoop for synchronous download
        QEventLoop loop;

        QObject::connect(&downloader, &kalahari::core::IconDownloader::downloadComplete,
                        &helper, &DownloadHelper::onComplete);
        QObject::connect(&downloader, &kalahari::core::IconDownloader::downloadError,
                        &helper, &DownloadHelper::onError);
        QObject::connect(&downloader, &kalahari::core::IconDownloader::downloadComplete,
                        &loop, &QEventLoop::quit);
        QObject::connect(&downloader, &kalahari::core::IconDownloader::downloadError,
                        &loop, &QEventLoop::quit);

        // Start download
        downloader.downloadFromUrl(url, theme);

        // Wait for completion (with 15 second timeout)
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        loop.exec();

        if (!helper.downloadSuccess) {
            fprintf(stderr, "Download failed\n");
            return 1;
        }

        // Convert SVG
        auto conversionResult = converter.convertToTemplate(helper.downloadedSvg);
        if (!conversionResult.success) {
            logger.error("CLI: Conversion failed: {}", conversionResult.errorMessage.toStdString());
            fprintf(stderr, "Error: Conversion failed: %s\n", conversionResult.errorMessage.toUtf8().constData());
            return 1;
        }

        // Ensure directory exists
        QString dirPath = QString("resources/icons/%1").arg(theme);
        QDir dir;
        if (!dir.exists(dirPath)) {
            if (!dir.mkpath(dirPath)) {
                logger.error("CLI: Failed to create directory: {}", dirPath.toStdString());
                fprintf(stderr, "Error: Cannot create directory: %s\n", dirPath.toUtf8().constData());
                return 1;
            }
        }

        // Save SVG to file
        QString filePath = QString("resources/icons/%1/%2.svg").arg(theme, iconName);
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            logger.error("CLI: Failed to open file for writing: {}", filePath.toStdString());
            fprintf(stderr, "Error: Cannot write to file: %s\n", filePath.toUtf8().constData());
            return 1;
        }

        file.write(conversionResult.svg.toUtf8());
        file.close();

        logger.info("CLI: ✓ Saved {} ({} bytes)", filePath.toStdString(), conversionResult.svg.length());
        fprintf(stdout, "✓ Saved: %s\n", filePath.toUtf8().constData());

        return 0;
    }

    // CLI mode without valid command
    if (cmdLine.hasSwitch("cli")) {
        fprintf(stderr, "CLI mode requires --get-icon URL\n");
        fprintf(stderr, "Usage: kalahari --cli --get-icon URL --icon-name NAME [--theme THEME]\n");
        fprintf(stderr, "\nExample:\n");
        fprintf(stderr, "  kalahari --cli --get-icon https://raw.githubusercontent.com/google/material-design-icons/master/src/content/save/materialiconstwotone/24px.svg --icon-name save --theme twotone\n");
        return 1;
    }

    // ========================================================================
    // GUI Mode
    // ========================================================================
    kalahari::gui::MainWindow window;

    // Enable diagnostic mode if --diag flag present
    if (cmdLine.isDiagnosticMode()) {
        logger.info("Diagnostic mode enabled via --diag flag");
        window.enableDiagnosticMode();
    }

    // Enable dev mode if --dev flag present
    if (cmdLine.hasSwitch("dev")) {
        logger.info("Dev mode enabled via --dev flag");
        window.enableDevMode();
    }

    window.show();

    logger.info("Main window shown - entering event loop");

    int result = app.exec();

    logger.info("Application exited with code: {}", result);
    return result;
}

// Include MOC-generated code for DownloadHelper class
#include "main.moc"
