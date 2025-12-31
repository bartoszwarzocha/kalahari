/// @file clipboard_handler.h
/// @brief Clipboard operations for BookEditor (OpenSpec #00042 Phase 4.13-4.16)
///
/// This header provides clipboard copy, cut, paste operations with support
/// for multiple formats: KML (native), HTML, and plain text.

#pragma once

#include <kalahari/editor/editor_types.h>
#include <QString>
#include <QMimeData>
#include <memory>

namespace kalahari::editor {

// =============================================================================
// MIME Type Constants
// =============================================================================

/// Custom MIME type for Kalahari KML content
constexpr const char* MIME_KML = "application/x-kalahari-kml";

// =============================================================================
// Clipboard Handler
// =============================================================================

/// @brief Handles clipboard operations for the BookEditor
///
/// Provides copy, cut, and paste operations with format conversion between
/// KML (native format), HTML, and plain text.
class ClipboardHandler {
public:
    ClipboardHandler() = default;
    ~ClipboardHandler() = default;

    // Non-copyable
    ClipboardHandler(const ClipboardHandler&) = delete;
    ClipboardHandler& operator=(const ClipboardHandler&) = delete;

    // =========================================================================
    // Paste Operations
    // =========================================================================

    /// @brief Check if clipboard has pasteable content
    /// @return true if clipboard contains KML, HTML, or text
    static bool canPaste();

    /// @brief Get paste content as KML
    /// @return KML string ready for insertion, or empty if nothing to paste
    ///
    /// Priority: KML > HTML > Plain Text
    /// Converts HTML/text to KML if native format not available.
    static QString pasteAsKml();

    /// @brief Get paste content as plain text
    /// @return Plain text string, or empty if nothing to paste
    static QString pasteAsText();

    // =========================================================================
    // Format Conversion
    // =========================================================================

    /// @brief Convert KML to HTML
    /// @param kml KML markup string
    /// @return HTML string with equivalent formatting
    static QString kmlToHtml(const QString& kml);

    /// @brief Convert HTML to KML
    /// @param html HTML markup string
    /// @return KML string with equivalent formatting
    static QString htmlToKml(const QString& html);

    /// @brief Convert plain text to KML
    /// @param text Plain text string
    /// @return KML string with escaped characters and paragraph breaks
    static QString textToKml(const QString& text);

    /// @brief Extract plain text from KML
    /// @param kml KML markup string
    /// @return Plain text without any formatting
    static QString kmlToText(const QString& kml);
};

}  // namespace kalahari::editor
