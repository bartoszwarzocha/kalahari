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
    // Advanced / General
    // ========================================================================

    bool diagnosticMode = false;///< Enable diagnostic menu

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
               iconSizes != other.iconSizes ||
               uiFontSize != other.uiFontSize ||
               logTraceColor != other.logTraceColor ||
               logDebugColor != other.logDebugColor ||
               logInfoColor != other.logInfoColor ||
               logWarningColor != other.logWarningColor ||
               logErrorColor != other.logErrorColor ||
               logCriticalColor != other.logCriticalColor ||
               logBackgroundColor != other.logBackgroundColor;
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
               iconTheme != other.iconTheme ||
               iconSizes != other.iconSizes ||
               editorFontFamily != other.editorFontFamily ||
               editorFontSize != other.editorFontSize ||
               tabSize != other.tabSize ||
               showLineNumbers != other.showLineNumbers ||
               wordWrap != other.wordWrap ||
               diagnosticMode != other.diagnosticMode ||
               logBufferSize != other.logBufferSize ||
               logTraceColor != other.logTraceColor ||
               logDebugColor != other.logDebugColor ||
               logInfoColor != other.logInfoColor ||
               logWarningColor != other.logWarningColor ||
               logErrorColor != other.logErrorColor ||
               logCriticalColor != other.logCriticalColor ||
               logBackgroundColor != other.logBackgroundColor;
    }

    bool operator==(const SettingsData& other) const {
        return !(*this != other);
    }
};

} // namespace gui
} // namespace kalahari
