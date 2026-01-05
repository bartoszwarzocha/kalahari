/// @file settings_data.h
/// @brief Data structure for settings transfer between dialog and application
///
/// SettingsData is a plain data structure that encapsulates all settings
/// collected by SettingsDialog. It's passed to MainWindow for application.
/// This separates data collection (dialog) from data application (main window).

#pragma once

#include <QString>
#include <QColor>
#include <QMap>
#include "kalahari/core/art_provider.h"  // For IconContext enum
#include "kalahari/editor/editor_appearance.h"  // For CursorStyle enum

namespace kalahari {
namespace gui {

/// @brief Plain data structure containing all application settings
///
/// Used to transfer settings from SettingsDialog to MainWindow.
/// SettingsDialog collects and validates data, MainWindow applies it.
struct SettingsData {
    // ========================================================================
    // Appearance / General
    // ========================================================================

    QString language;           ///< UI language code (e.g., "en", "pl")
    int uiFontSize = 12;        ///< UI font size in points

    // ========================================================================
    // Appearance / Theme
    // ========================================================================

    QString theme;              ///< Theme name (e.g., "Light", "Dark")
    QColor primaryColor;        ///< Icon primary color
    QColor secondaryColor;      ///< Icon secondary color
    QColor infoHeaderColor;        ///< Color for information panel headers
    QColor dashboardSecondaryColor;        ///< Secondary dashboard accent color
    QColor dashboardPrimaryColor;        ///< Primary dashboard accent color
    QColor infoSecondaryColor;        ///< Secondary info color for panels
    QColor infoPrimaryColor;        ///< Primary info color for panels

    // ========================================================================
    // Appearance / Icons
    // ========================================================================

    QString iconTheme;          ///< Icon theme (filled, outlined, rounded, twotone)
    QMap<core::IconContext, int> iconSizes;  ///< Icon sizes per context

    // ========================================================================
    // Editor / General
    // ========================================================================

    QString editorFontFamily;   ///< Editor font family
    int editorFontSize = 12;    ///< Editor font size in points
    int tabSize = 4;            ///< Tab size in spaces
    bool showLineNumbers = true;///< Show line numbers in editor
    bool wordWrap = true;       ///< Enable word wrap

    // ========================================================================
    // Editor / Colors (Light/Dark mode, independent from app theme)
    // ========================================================================

    bool editorDarkMode = true;        ///< Editor dark mode (true = dark, false = light)

    // Light mode colors
    QColor editorBackgroundLight{255, 255, 255}; ///< Background - light mode
    QColor editorTextLight{30, 30, 30};          ///< Text - light mode
    QColor editorInactiveLight{170, 170, 170};   ///< Inactive text (Focus mode) - light

    // Dark mode colors
    QColor editorBackgroundDark{35, 35, 40};     ///< Background - dark mode
    QColor editorTextDark{224, 224, 224};        ///< Text - dark mode
    QColor editorInactiveDark{120, 120, 125};    ///< Inactive text (Focus mode) - dark

    // ========================================================================
    // Editor / Cursor
    // ========================================================================

    editor::CursorStyle cursorStyle{editor::CursorStyle::Line}; ///< Cursor shape
    bool cursorUseCustomColor{false};            ///< Use custom color instead of text color
    QColor cursorCustomColor{255, 255, 255};     ///< Custom cursor color
    bool cursorBlinking{true};                   ///< Enable cursor blinking
    int cursorBlinkInterval{500};                ///< Blink interval in milliseconds
    int cursorLineWidth{2};                      ///< Width for Line cursor in pixels

    // ========================================================================
    // Editor / Margins
    // ========================================================================

    // View margins (for Continuous/Focus views) - in pixels
    int viewMarginHorizontal = 50;               ///< Horizontal margin in pixels
    int viewMarginVertical = 30;                 ///< Vertical margin in pixels

    // Page margins (for Page/Typewriter views) - in mm
    double pageMarginTop = 25.4;                 ///< Top margin in mm
    double pageMarginBottom = 25.4;              ///< Bottom margin in mm
    double pageMarginLeft = 25.4;                ///< Left margin in mm
    double pageMarginRight = 25.4;               ///< Right margin in mm

    // Mirror margins for book binding
    bool pageMirrorMarginsEnabled = false;       ///< Enable mirror margins
    double pageMarginInner = 30.0;               ///< Inner margin (binding side) in mm
    double pageMarginOuter = 20.0;               ///< Outer margin in mm

    // ========================================================================
    // Editor / Text Frame Border
    // ========================================================================

    bool textFrameBorderShow = false;            ///< Show border around text area
    QColor textFrameBorderColor{180, 180, 180};  ///< Border color
    int textFrameBorderWidth = 1;                ///< Border width in pixels (1-5)

    // ========================================================================
    // Advanced / General
    // ========================================================================

    bool diagnosticMode = false;///< Enable diagnostic menu

    // ========================================================================
    // Appearance / Theme - UI Colors (QPalette roles)
    // ========================================================================

    QColor tooltipBackgroundColor; ///< Tooltip background color (toolTipBase)
    QColor tooltipTextColor;       ///< Tooltip text color (toolTipText)
    QColor placeholderTextColor;   ///< Placeholder text color in inputs
    QColor brightTextColor;        ///< High contrast text on dark backgrounds

    // ========================================================================
    // Appearance / Theme - Palette Colors (all 16 QPalette roles)
    // ========================================================================

    // Basic Colors
    QColor paletteWindowColor;        ///< Window background color
    QColor paletteWindowTextColor;    ///< General text color
    QColor paletteBaseColor;          ///< Input field background color
    QColor paletteAlternateBaseColor; ///< Alternating row background color
    QColor paletteTextColor;          ///< Input field text color

    // Button Colors
    QColor paletteButtonColor;        ///< Button background color
    QColor paletteButtonTextColor;    ///< Button text color

    // Selection Colors
    QColor paletteHighlightColor;       ///< Selection background color
    QColor paletteHighlightedTextColor; ///< Selected text color

    // 3D Effect Colors
    QColor paletteLightColor;    ///< Lightest color for 3D effects
    QColor paletteMidlightColor; ///< Between light and button
    QColor paletteMidColor;      ///< Medium color for borders
    QColor paletteDarkColor;     ///< Darker color for 3D effects
    QColor paletteShadowColor;   ///< Darkest color for shadows

    // Link Colors
    QColor paletteLinkColor;        ///< Hyperlink color
    QColor paletteLinkVisitedColor; ///< Visited hyperlink color

    // ========================================================================
    // Appearance / Theme - Log Panel Colors
    // ========================================================================

    QColor logTraceColor;       ///< Log TRACE message color
    QColor logDebugColor;       ///< Log DEBUG message color
    QColor logInfoColor;        ///< Log INFO message color
    QColor logWarningColor;     ///< Log WARNING message color
    QColor logErrorColor;       ///< Log ERROR message color
    QColor logCriticalColor;    ///< Log CRITICAL message color
    QColor logBackgroundColor;  ///< Log panel background color

    // ========================================================================
    // Advanced / Log
    // ========================================================================

    int logBufferSize = 500;    ///< Log panel buffer size (1-1000 lines)

    // ========================================================================
    // Appearance / Dashboard
    // ========================================================================

    bool showKalahariNews = true;       ///< Show news section on Dashboard
    bool showRecentFiles = true;        ///< Show recent files on Dashboard
    bool autoLoadLastProject = false;   ///< Auto-load last project on startup
    int dashboardMaxItems = 5;          ///< Max items in Dashboard sections (3-9)
    int dashboardIconSize = 48;         ///< Icon size in Dashboard panels (24-64)

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /// @brief Check if appearance settings changed (requires visual refresh)
    /// @param other Previous settings to compare
    /// @return true if visual refresh is needed
    bool requiresVisualRefresh(const SettingsData& other) const {
        return theme != other.theme ||
               iconTheme != other.iconTheme ||
               primaryColor != other.primaryColor ||
               secondaryColor != other.secondaryColor ||
               infoHeaderColor != other.infoHeaderColor ||
               dashboardSecondaryColor != other.dashboardSecondaryColor ||
               dashboardPrimaryColor != other.dashboardPrimaryColor ||
               infoSecondaryColor != other.infoSecondaryColor ||
               infoPrimaryColor != other.infoPrimaryColor ||
               iconSizes != other.iconSizes ||
               uiFontSize != other.uiFontSize ||
               tooltipBackgroundColor != other.tooltipBackgroundColor ||
               tooltipTextColor != other.tooltipTextColor ||
               placeholderTextColor != other.placeholderTextColor ||
               brightTextColor != other.brightTextColor ||
               // Palette colors
               paletteWindowColor != other.paletteWindowColor ||
               paletteWindowTextColor != other.paletteWindowTextColor ||
               paletteBaseColor != other.paletteBaseColor ||
               paletteAlternateBaseColor != other.paletteAlternateBaseColor ||
               paletteTextColor != other.paletteTextColor ||
               paletteButtonColor != other.paletteButtonColor ||
               paletteButtonTextColor != other.paletteButtonTextColor ||
               paletteHighlightColor != other.paletteHighlightColor ||
               paletteHighlightedTextColor != other.paletteHighlightedTextColor ||
               paletteLightColor != other.paletteLightColor ||
               paletteMidlightColor != other.paletteMidlightColor ||
               paletteMidColor != other.paletteMidColor ||
               paletteDarkColor != other.paletteDarkColor ||
               paletteShadowColor != other.paletteShadowColor ||
               paletteLinkColor != other.paletteLinkColor ||
               paletteLinkVisitedColor != other.paletteLinkVisitedColor ||
               // Log colors
               logTraceColor != other.logTraceColor ||
               logDebugColor != other.logDebugColor ||
               logInfoColor != other.logInfoColor ||
               logWarningColor != other.logWarningColor ||
               logErrorColor != other.logErrorColor ||
               logCriticalColor != other.logCriticalColor ||
               logBackgroundColor != other.logBackgroundColor ||
               // Dashboard settings
               showKalahariNews != other.showKalahariNews ||
               showRecentFiles != other.showRecentFiles ||
               autoLoadLastProject != other.autoLoadLastProject ||
               dashboardMaxItems != other.dashboardMaxItems ||
               dashboardIconSize != other.dashboardIconSize ||
               // Editor colors
               editorDarkMode != other.editorDarkMode ||
               editorBackgroundLight != other.editorBackgroundLight ||
               editorTextLight != other.editorTextLight ||
               editorInactiveLight != other.editorInactiveLight ||
               editorBackgroundDark != other.editorBackgroundDark ||
               editorTextDark != other.editorTextDark ||
               editorInactiveDark != other.editorInactiveDark ||
               // Cursor settings
               cursorStyle != other.cursorStyle ||
               cursorUseCustomColor != other.cursorUseCustomColor ||
               cursorCustomColor != other.cursorCustomColor ||
               cursorBlinking != other.cursorBlinking ||
               cursorBlinkInterval != other.cursorBlinkInterval ||
               cursorLineWidth != other.cursorLineWidth ||
               // Margin settings
               viewMarginHorizontal != other.viewMarginHorizontal ||
               viewMarginVertical != other.viewMarginVertical ||
               pageMarginTop != other.pageMarginTop ||
               pageMarginBottom != other.pageMarginBottom ||
               pageMarginLeft != other.pageMarginLeft ||
               pageMarginRight != other.pageMarginRight ||
               pageMirrorMarginsEnabled != other.pageMirrorMarginsEnabled ||
               pageMarginInner != other.pageMarginInner ||
               pageMarginOuter != other.pageMarginOuter ||
               // Text frame border settings
               textFrameBorderShow != other.textFrameBorderShow ||
               textFrameBorderColor != other.textFrameBorderColor ||
               textFrameBorderWidth != other.textFrameBorderWidth;
    }

    /// @brief Check if any setting changed
    /// @param other Previous settings to compare
    /// @return true if any setting differs
    bool operator!=(const SettingsData& other) const {
        return language != other.language ||
               uiFontSize != other.uiFontSize ||
               theme != other.theme ||
               primaryColor != other.primaryColor ||
               secondaryColor != other.secondaryColor ||
               infoHeaderColor != other.infoHeaderColor ||
               dashboardSecondaryColor != other.dashboardSecondaryColor ||
               dashboardPrimaryColor != other.dashboardPrimaryColor ||
               infoSecondaryColor != other.infoSecondaryColor ||
               infoPrimaryColor != other.infoPrimaryColor ||
               iconTheme != other.iconTheme ||
               iconSizes != other.iconSizes ||
               editorFontFamily != other.editorFontFamily ||
               editorFontSize != other.editorFontSize ||
               tabSize != other.tabSize ||
               showLineNumbers != other.showLineNumbers ||
               wordWrap != other.wordWrap ||
               // Editor colors
               editorDarkMode != other.editorDarkMode ||
               editorBackgroundLight != other.editorBackgroundLight ||
               editorTextLight != other.editorTextLight ||
               editorInactiveLight != other.editorInactiveLight ||
               editorBackgroundDark != other.editorBackgroundDark ||
               editorTextDark != other.editorTextDark ||
               editorInactiveDark != other.editorInactiveDark ||
               diagnosticMode != other.diagnosticMode ||
               logBufferSize != other.logBufferSize ||
               tooltipBackgroundColor != other.tooltipBackgroundColor ||
               tooltipTextColor != other.tooltipTextColor ||
               placeholderTextColor != other.placeholderTextColor ||
               brightTextColor != other.brightTextColor ||
               // Palette colors
               paletteWindowColor != other.paletteWindowColor ||
               paletteWindowTextColor != other.paletteWindowTextColor ||
               paletteBaseColor != other.paletteBaseColor ||
               paletteAlternateBaseColor != other.paletteAlternateBaseColor ||
               paletteTextColor != other.paletteTextColor ||
               paletteButtonColor != other.paletteButtonColor ||
               paletteButtonTextColor != other.paletteButtonTextColor ||
               paletteHighlightColor != other.paletteHighlightColor ||
               paletteHighlightedTextColor != other.paletteHighlightedTextColor ||
               paletteLightColor != other.paletteLightColor ||
               paletteMidlightColor != other.paletteMidlightColor ||
               paletteMidColor != other.paletteMidColor ||
               paletteDarkColor != other.paletteDarkColor ||
               paletteShadowColor != other.paletteShadowColor ||
               paletteLinkColor != other.paletteLinkColor ||
               paletteLinkVisitedColor != other.paletteLinkVisitedColor ||
               // Log colors
               logTraceColor != other.logTraceColor ||
               logDebugColor != other.logDebugColor ||
               logInfoColor != other.logInfoColor ||
               logWarningColor != other.logWarningColor ||
               logErrorColor != other.logErrorColor ||
               logCriticalColor != other.logCriticalColor ||
               logBackgroundColor != other.logBackgroundColor ||
               // Dashboard settings
               showKalahariNews != other.showKalahariNews ||
               showRecentFiles != other.showRecentFiles ||
               autoLoadLastProject != other.autoLoadLastProject ||
               dashboardMaxItems != other.dashboardMaxItems ||
               dashboardIconSize != other.dashboardIconSize ||
               // Cursor settings
               cursorStyle != other.cursorStyle ||
               cursorUseCustomColor != other.cursorUseCustomColor ||
               cursorCustomColor != other.cursorCustomColor ||
               cursorBlinking != other.cursorBlinking ||
               cursorBlinkInterval != other.cursorBlinkInterval ||
               cursorLineWidth != other.cursorLineWidth ||
               // Margin settings
               viewMarginHorizontal != other.viewMarginHorizontal ||
               viewMarginVertical != other.viewMarginVertical ||
               pageMarginTop != other.pageMarginTop ||
               pageMarginBottom != other.pageMarginBottom ||
               pageMarginLeft != other.pageMarginLeft ||
               pageMarginRight != other.pageMarginRight ||
               pageMirrorMarginsEnabled != other.pageMirrorMarginsEnabled ||
               pageMarginInner != other.pageMarginInner ||
               pageMarginOuter != other.pageMarginOuter ||
               // Text frame border settings
               textFrameBorderShow != other.textFrameBorderShow ||
               textFrameBorderColor != other.textFrameBorderColor ||
               textFrameBorderWidth != other.textFrameBorderWidth;
    }

    bool operator==(const SettingsData& other) const {
        return !(*this != other);
    }
};

} // namespace gui
} // namespace kalahari
