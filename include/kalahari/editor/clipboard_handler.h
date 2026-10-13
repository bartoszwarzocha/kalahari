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

    /// @brief Convert KML to HTML for other programs
    ///
    /// Paragraphs keep their alignment and their line breaks (br), formatting tags become
    /// b/i/u/s/sub/sup and inline styles (font, size, color, bg) CSS. Metadata tags keep
    /// only their text.
    /// @param kml KML markup string
    /// @return HTML string with equivalent formatting
    static QString kmlToHtml(const QString& kml);

    /// @brief Convert HTML to KML
    /// @param html HTML markup string
    /// @return KML string with equivalent formatting
    static QString htmlToKml(const QString& html);

    /// @brief Convert plain text to KML
    ///
    /// Each line is a paragraph; U+2028 and a vertical tab (as Word writes it) are a line
    /// break in the paragraph (<br/>). Other control characters than tab are dropped.
    /// @param text Plain text string
    /// @return KML string with escaped characters, paragraph and line breaks
    static QString textToKml(const QString& text);

    /// @brief The plain text another program gave with its HTML, with the line ends of the
    ///        HTML: U+2028 for its line breaks in a paragraph, no blank line it does not have
    ///
    /// Word and the browsers give a line break in a paragraph (Shift+Enter, <br>) as a line
    /// end in the plain text, like a paragraph end, and the browsers leave a blank line
    /// between two paragraphs; their HTML tells them apart. The characters of the text are
    /// matched with those of the HTML: a line end of the text becomes U+2028 where the HTML
    /// has a line break and no paragraph end, and where the HTML has a paragraph end, the
    /// text keeps no more line ends than the HTML has there. Where the two differ in
    /// anything but white space, the text comes back as it is. The HTML is read without
    /// anything it refers to (style sheets, images).
    /// @param text The plain text (text/plain)
    /// @param html The HTML of the same content (text/html), may be empty
    /// @return The text, with U+2028 for the line breaks
    static QString withLineEndsOf(const QString& text, const QString& html);

    /// @brief Extract plain text from KML
    /// @param kml KML markup string
    /// @return Plain text without any formatting
    static QString kmlToText(const QString& kml);
};

}  // namespace kalahari::editor
