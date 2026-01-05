/// @file settings_coordinator.cpp
/// @brief Settings dialog coordination implementation
///
/// OpenSpec #00038 - Phase 5: Extract Settings Management from MainWindow

#include "kalahari/gui/settings_coordinator.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/settings_data.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/editor_appearance.h"  // For CursorStyle enum
#include <QMainWindow>
#include <QStatusBar>

namespace kalahari {
namespace gui {

SettingsCoordinator::SettingsCoordinator(QMainWindow* mainWindow,
                                          DockCoordinator* dockCoordinator,
                                          QStatusBar* statusBar,
                                          QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_dockCoordinator(dockCoordinator)
    , m_statusBar(statusBar)
    , m_diagnosticModeGetter([]() { return false; })
{
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsCoordinator: Created");
}

void SettingsCoordinator::setDiagnosticModeGetter(std::function<bool()> callback) {
    m_diagnosticModeGetter = std::move(callback);
}

void SettingsCoordinator::openSettingsDialog() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Settings");

    // Collect current settings to pass to dialog
    SettingsData currentSettings = collectCurrentSettings();

    // Create dialog with current settings
    SettingsDialog dialog(m_mainWindow, currentSettings);

    // Connect settings applied signal - coordinator reacts
    connect(&dialog, &SettingsDialog::settingsApplied,
            this, [this](const SettingsData& settings) {
                onApplySettings(settings, false);
            });

    int result = dialog.exec();

    if (result == QDialog::Accepted) {
        logger.info("Settings dialog: OK clicked");
        if (m_statusBar) {
            m_statusBar->showMessage(QObject::tr("Settings applied"), 2000);
        }
    } else {
        logger.info("Settings dialog: Cancel clicked (changes discarded)");
        if (m_statusBar) {
            m_statusBar->showMessage(QObject::tr("Settings changes discarded"), 2000);
        }
    }
}

SettingsData SettingsCoordinator::collectCurrentSettings() const {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsCoordinator: Collecting current settings");

    auto& settings = core::SettingsManager::getInstance();
    auto& iconRegistry = core::IconRegistry::getInstance();

    SettingsData settingsData;

    // Appearance/General
    settingsData.language = QString::fromStdString(settings.getLanguage());
    settingsData.uiFontSize = settings.get<int>("appearance.uiFontSize", 12);

    // Appearance/Theme
    settingsData.theme = QString::fromStdString(settings.getTheme());
    // Get colors from ArtProvider (which uses IconRegistry's current colors)
    // This ensures we get the user's custom colors, not theme defaults
    auto& artProvider = core::ArtProvider::getInstance();
    settingsData.primaryColor = artProvider.getPrimaryColor();
    settingsData.secondaryColor = artProvider.getSecondaryColor();

    // Info header color from theme
    const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();
    settingsData.infoHeaderColor = theme.colors.infoHeader;
    settingsData.dashboardSecondaryColor = theme.colors.dashboardSecondary;
    settingsData.dashboardPrimaryColor = theme.colors.dashboardPrimary;
    settingsData.infoSecondaryColor = theme.colors.infoSecondary;
    settingsData.infoPrimaryColor = theme.colors.infoPrimary;

    // Appearance/Icons
    settingsData.iconTheme = QString::fromStdString(settings.get<std::string>("appearance.iconTheme", "twotone"));
    const auto& sizes = iconRegistry.getSizes();
    settingsData.iconSizes[core::IconContext::Toolbar] = sizes.toolbar;
    settingsData.iconSizes[core::IconContext::Menu] = sizes.menu;
    settingsData.iconSizes[core::IconContext::TreeView] = sizes.treeView;
    settingsData.iconSizes[core::IconContext::TabBar] = sizes.tabBar;
    settingsData.iconSizes[core::IconContext::Button] = sizes.button;
    settingsData.iconSizes[core::IconContext::StatusBar] = sizes.statusBar;
    settingsData.iconSizes[core::IconContext::ComboBox] = sizes.comboBox;

    // Editor/General
    settingsData.editorFontFamily = QString::fromStdString(settings.get<std::string>("editor.fontFamily", "Consolas"));
    settingsData.editorFontSize = settings.get<int>("editor.fontSize", 12);
    settingsData.tabSize = settings.get<int>("editor.tabSize", 4);
    settingsData.showLineNumbers = settings.get<bool>("editor.lineNumbers", true);
    settingsData.wordWrap = settings.get<bool>("editor.wordWrap", false);

    // Editor/Colors
    settingsData.editorDarkMode = settings.get<bool>("editor.darkMode", true);
    settingsData.editorBackgroundLight = QColor(QString::fromStdString(
        settings.get<std::string>("editor.colors.backgroundLight", "#ffffff")));
    settingsData.editorTextLight = QColor(QString::fromStdString(
        settings.get<std::string>("editor.colors.textLight", "#1e1e1e")));
    settingsData.editorInactiveLight = QColor(QString::fromStdString(
        settings.get<std::string>("editor.colors.inactiveLight", "#aaaaaa")));
    settingsData.editorBackgroundDark = QColor(QString::fromStdString(
        settings.get<std::string>("editor.colors.backgroundDark", "#232328")));
    settingsData.editorTextDark = QColor(QString::fromStdString(
        settings.get<std::string>("editor.colors.textDark", "#e0e0e0")));
    settingsData.editorInactiveDark = QColor(QString::fromStdString(
        settings.get<std::string>("editor.colors.inactiveDark", "#78787d")));

    // Editor/Cursor
    int cursorStyleInt = settings.get<int>("editor.cursor.style", 0);  // 0 = Line
    settingsData.cursorStyle = static_cast<editor::CursorStyle>(cursorStyleInt);
    settingsData.cursorUseCustomColor = settings.get<bool>("editor.cursor.useCustomColor", false);
    settingsData.cursorCustomColor = QColor(QString::fromStdString(
        settings.get<std::string>("editor.cursor.customColor", "#ffffff")));
    settingsData.cursorBlinking = settings.get<bool>("editor.cursor.blinking", true);
    settingsData.cursorBlinkInterval = settings.get<int>("editor.cursor.blinkInterval", 500);
    settingsData.cursorLineWidth = settings.get<int>("editor.cursor.lineWidth", 2);

    // Editor/Margins
    settingsData.viewMarginHorizontal = static_cast<int>(settings.get<double>("editor.margins.viewHorizontal", 50.0));
    settingsData.viewMarginVertical = static_cast<int>(settings.get<double>("editor.margins.viewVertical", 30.0));
    settingsData.pageMarginTop = settings.get<double>("editor.margins.pageTop", 25.4);
    settingsData.pageMarginBottom = settings.get<double>("editor.margins.pageBottom", 25.4);
    settingsData.pageMarginLeft = settings.get<double>("editor.margins.pageLeft", 25.4);
    settingsData.pageMarginRight = settings.get<double>("editor.margins.pageRight", 25.4);
    settingsData.pageMirrorMarginsEnabled = settings.get<bool>("editor.margins.mirrorEnabled", false);
    settingsData.pageMarginInner = settings.get<double>("editor.margins.pageInner", 30.0);
    settingsData.pageMarginOuter = settings.get<double>("editor.margins.pageOuter", 20.0);

    // Editor/Text Frame Border
    settingsData.textFrameBorderShow = settings.get<bool>("editor.textFrameBorder.show", false);
    settingsData.textFrameBorderColor = QColor(QString::fromStdString(
        settings.get<std::string>("editor.textFrameBorder.color", "#b4b4b4")));
    settingsData.textFrameBorderWidth = settings.get<int>("editor.textFrameBorder.width", 1);

    // Advanced/General - use callback to get diagnostic mode
    settingsData.diagnosticMode = m_diagnosticModeGetter();

    // Advanced/Log
    settingsData.logBufferSize = settings.get<int>("log.bufferSize", 500);

    // UI Colors (Task #00028)
    // Load per-theme colors with theme-appropriate defaults
    std::string themeName = settingsData.theme.toStdString();
    bool isDark = (themeName == "Dark");

    // Define UI color defaults (from theme.cpp)
    std::string defToolTipBase = isDark ? "#3c3c3c" : "#ffffdc";
    std::string defToolTipText = isDark ? "#e0e0e0" : "#000000";
    std::string defPlaceholderText = isDark ? "#808080" : "#a0a0a0";
    std::string defBrightText = "#ffffff";

    if (settings.hasCustomUiColorsForTheme(themeName)) {
        settingsData.tooltipBackgroundColor = QColor(QString::fromStdString(
            settings.getUiColorForTheme(themeName, "toolTipBase", defToolTipBase)));
        settingsData.tooltipTextColor = QColor(QString::fromStdString(
            settings.getUiColorForTheme(themeName, "toolTipText", defToolTipText)));
        settingsData.placeholderTextColor = QColor(QString::fromStdString(
            settings.getUiColorForTheme(themeName, "placeholderText", defPlaceholderText)));
        settingsData.brightTextColor = QColor(QString::fromStdString(
            settings.getUiColorForTheme(themeName, "brightText", defBrightText)));
    } else {
        settingsData.tooltipBackgroundColor = QColor(QString::fromStdString(defToolTipBase));
        settingsData.tooltipTextColor = QColor(QString::fromStdString(defToolTipText));
        settingsData.placeholderTextColor = QColor(QString::fromStdString(defPlaceholderText));
        settingsData.brightTextColor = QColor(QString::fromStdString(defBrightText));
    }

    // Log Panel Colors (Task #00027)
    // Define theme defaults
    std::string defTrace = isDark ? "#FF66FF" : "#CC00CC";
    std::string defDebug = isDark ? "#FF66FF" : "#CC00CC";
    std::string defInfo = isDark ? "#FFFFFF" : "#000000";
    std::string defWarning = isDark ? "#FFA500" : "#FF8C00";
    std::string defError = isDark ? "#FF4444" : "#CC0000";
    std::string defCritical = isDark ? "#FF4444" : "#CC0000";
    std::string defBackground = isDark ? "#252525" : "#F5F5F5";

    // Check for corrupted log color settings (all #000000 due to previous bug)
    bool useStoredLogColors = false;
    if (settings.hasCustomLogColorsForTheme(themeName)) {
        std::string storedTrace = settings.getLogColorForTheme(themeName, "trace", defTrace);
        std::string storedWarning = settings.getLogColorForTheme(themeName, "warning", defWarning);
        std::string storedError = settings.getLogColorForTheme(themeName, "error", defError);

        // If trace, warning, AND error are all #000000, data is corrupted
        bool corrupted = (storedTrace == "#000000" && storedWarning == "#000000" && storedError == "#000000");

        if (corrupted) {
            logger.warn("SettingsCoordinator: Detected corrupted log colors for theme '{}', clearing", themeName);
            settings.clearCustomLogColorsForTheme(themeName);
            useStoredLogColors = false;
        } else {
            useStoredLogColors = true;
        }
    }

    if (useStoredLogColors) {
        settingsData.logTraceColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "trace", defTrace)));
        settingsData.logDebugColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "debug", defDebug)));
        settingsData.logInfoColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "info", defInfo)));
        settingsData.logWarningColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "warning", defWarning)));
        settingsData.logErrorColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "error", defError)));
        settingsData.logCriticalColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "critical", defCritical)));
        settingsData.logBackgroundColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "background", defBackground)));
    } else {
        settingsData.logTraceColor = QColor(QString::fromStdString(defTrace));
        settingsData.logDebugColor = QColor(QString::fromStdString(defDebug));
        settingsData.logInfoColor = QColor(QString::fromStdString(defInfo));
        settingsData.logWarningColor = QColor(QString::fromStdString(defWarning));
        settingsData.logErrorColor = QColor(QString::fromStdString(defError));
        settingsData.logCriticalColor = QColor(QString::fromStdString(defCritical));
        settingsData.logBackgroundColor = QColor(QString::fromStdString(defBackground));
    }

    // Palette Colors (Task #00028)
    // Define palette color defaults (from theme.cpp)
    std::string defWindow = isDark ? "#2d2d2d" : "#f0f0f0";
    std::string defWindowText = isDark ? "#e0e0e0" : "#000000";
    std::string defBase = isDark ? "#252525" : "#ffffff";
    std::string defAlternateBase = isDark ? "#323232" : "#f5f5f5";
    std::string defTextPalette = isDark ? "#e0e0e0" : "#000000";
    std::string defButton = isDark ? "#404040" : "#e0e0e0";
    std::string defButtonText = isDark ? "#e0e0e0" : "#000000";
    std::string defHighlight = "#0078d4";
    std::string defHighlightedText = "#ffffff";
    std::string defLight = isDark ? "#505050" : "#ffffff";
    std::string defMidlight = isDark ? "#404040" : "#e0e0e0";
    std::string defMid = isDark ? "#303030" : "#a0a0a0";
    std::string defDark = isDark ? "#202020" : "#606060";
    std::string defShadow = "#000000";
    std::string defLink = isDark ? "#5eb3f0" : "#0078d4";
    std::string defLinkVisited = isDark ? "#b48ade" : "#551a8b";

    if (settings.hasCustomPaletteColorsForTheme(themeName)) {
        settingsData.paletteWindowColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "window", defWindow)));
        settingsData.paletteWindowTextColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "windowText", defWindowText)));
        settingsData.paletteBaseColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "base", defBase)));
        settingsData.paletteAlternateBaseColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "alternateBase", defAlternateBase)));
        settingsData.paletteTextColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "text", defTextPalette)));
        settingsData.paletteButtonColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "button", defButton)));
        settingsData.paletteButtonTextColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "buttonText", defButtonText)));
        settingsData.paletteHighlightColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "highlight", defHighlight)));
        settingsData.paletteHighlightedTextColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "highlightedText", defHighlightedText)));
        settingsData.paletteLightColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "light", defLight)));
        settingsData.paletteMidlightColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "midlight", defMidlight)));
        settingsData.paletteMidColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "mid", defMid)));
        settingsData.paletteDarkColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "dark", defDark)));
        settingsData.paletteShadowColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "shadow", defShadow)));
        settingsData.paletteLinkColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "link", defLink)));
        settingsData.paletteLinkVisitedColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "linkVisited", defLinkVisited)));
    } else {
        settingsData.paletteWindowColor = QColor(QString::fromStdString(defWindow));
        settingsData.paletteWindowTextColor = QColor(QString::fromStdString(defWindowText));
        settingsData.paletteBaseColor = QColor(QString::fromStdString(defBase));
        settingsData.paletteAlternateBaseColor = QColor(QString::fromStdString(defAlternateBase));
        settingsData.paletteTextColor = QColor(QString::fromStdString(defTextPalette));
        settingsData.paletteButtonColor = QColor(QString::fromStdString(defButton));
        settingsData.paletteButtonTextColor = QColor(QString::fromStdString(defButtonText));
        settingsData.paletteHighlightColor = QColor(QString::fromStdString(defHighlight));
        settingsData.paletteHighlightedTextColor = QColor(QString::fromStdString(defHighlightedText));
        settingsData.paletteLightColor = QColor(QString::fromStdString(defLight));
        settingsData.paletteMidlightColor = QColor(QString::fromStdString(defMidlight));
        settingsData.paletteMidColor = QColor(QString::fromStdString(defMid));
        settingsData.paletteDarkColor = QColor(QString::fromStdString(defDark));
        settingsData.paletteShadowColor = QColor(QString::fromStdString(defShadow));
        settingsData.paletteLinkColor = QColor(QString::fromStdString(defLink));
        settingsData.paletteLinkVisitedColor = QColor(QString::fromStdString(defLinkVisited));
    }

    // Dashboard settings (OpenSpec #00036)
    settingsData.showKalahariNews = settings.get<bool>("dashboard.showKalahariNews", true);
    settingsData.showRecentFiles = settings.get<bool>("dashboard.showRecentFiles", true);
    settingsData.autoLoadLastProject = settings.get<bool>("startup.autoLoadLastProject", false);
    settingsData.dashboardMaxItems = settings.get<int>("dashboard.maxItems", 5);
    settingsData.dashboardIconSize = settings.get<int>("dashboard.iconSize", 48);

    logger.debug("SettingsCoordinator: Settings collected");
    return settingsData;
}

void SettingsCoordinator::onApplySettings(const SettingsData& settings, bool /*fromOkButton*/) {
    auto& logger = core::Logger::getInstance();
    logger.info("SettingsCoordinator: Reacting to settings applied");

    // Handle diagnostic mode change
    bool currentDiagMode = m_diagnosticModeGetter();
    if (settings.diagnosticMode != currentDiagMode) {
        if (settings.diagnosticMode) {
            emit enableDiagnosticModeRequested();
        } else {
            emit disableDiagnosticModeRequested();
        }
    }

    // Handle log buffer size change
    LogPanel* logPanel = m_dockCoordinator->logPanel();
    if (logPanel && static_cast<int>(logPanel->getMaxBufferSize()) != settings.logBufferSize) {
        logPanel->setMaxBufferSize(static_cast<size_t>(settings.logBufferSize));
        logger.info("SettingsCoordinator: Log buffer size updated to {}", settings.logBufferSize);
    }

    // Apply log panel color changes (Task #00027)
    if (logPanel) {
        logPanel->applyThemeColors();
        logger.info("SettingsCoordinator: Log panel colors updated");
    }

    // Refresh dashboard to reflect new settings (e.g., show/hide recent files)
    DashboardPanel* dashboardPanel = m_dockCoordinator->dashboardPanel();
    if (dashboardPanel) {
        dashboardPanel->onSettingsChanged();
        logger.info("SettingsCoordinator: Dashboard refreshed after settings change");
    }

    // Emit signal for editor settings changes (font, colors, etc.)
    // MainWindow connects to this and applies settings to all EditorPanels
    emit editorSettingsChanged();
    logger.info("SettingsCoordinator: Editor settings change signal emitted");

    logger.info("SettingsCoordinator: Settings reaction complete");
}

} // namespace gui
} // namespace kalahari
