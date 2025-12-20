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

class KmlDocument;

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
    // Copy Operations
    // =========================================================================

    /// @brief Copy selected content to clipboard
    /// @param document The document to copy from
    /// @param selection The selection range to copy
    /// @return true if content was copied, false if selection is empty
    ///
    /// Sets clipboard with:
    /// - KML format (native, for paste within Kalahari)
    /// - HTML format (for paste into other rich text editors)
    /// - Plain text (universal fallback)
    static bool copy(const KmlDocument* document, const SelectionRange& selection);

    /// @brief Copy selected content as specific formats
    /// @param document The document to copy from
    /// @param selection The selection range to copy
    /// @return Mime data object with all formats (caller takes ownership)
    static std::unique_ptr<QMimeData> createMimeData(const KmlDocument* document,
                                                      const SelectionRange& selection);

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

    // =========================================================================
    // Selection Extraction
    // =========================================================================

    /// @brief Extract KML for a selection range
    /// @param document The document
    /// @param selection The selection range
    /// @return KML string representing selected content
    static QString extractKml(const KmlDocument* document, const SelectionRange& selection);

    /// @brief Extract plain text for a selection range
    /// @param document The document
    /// @param selection The selection range
    /// @return Plain text of selected content
    static QString extractText(const KmlDocument* document, const SelectionRange& selection);
};

}  // namespace kalahari::editor
