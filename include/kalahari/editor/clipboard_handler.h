/// @file clipboard_handler.h
/// @brief Clipboard operations for BookEditor (OpenSpec #00042 Phase 4.13-4.16)
///
/// This header provides clipboard copy, cut, paste operations with support
/// for multiple formats: KML (native), HTML, and plain text. Text from other programs is
/// pasted in one of three modes (PasteMode).

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
// Paste Modes
// =============================================================================

/// @brief How text from other programs is pasted: by Edit > Paste as the setting
///        editor.paste.mode says (Settings > Editor > General, "Paste Command"), and by the
///        paste of each mode in the Edit menu
///
/// Text copied in Kalahari keeps everything in every mode, but Paste Text Only
/// (BookEditor::pasteAs()): it comes as KML.
enum class PasteMode {
    Merge,   ///< Headings, quotes, paragraphs, emphasis and lines; the text takes the look of
             ///< the place it goes to (Word's "Merge Formatting"; the default)
    Source,  ///< Also the fonts, sizes and colors of the other program, and every alignment
    Text,    ///< Only the text, with its paragraphs and line breaks
};

/// @brief Setting key of the paste mode
constexpr const char* PASTE_MODE_KEY = "editor.paste.mode";
/// @brief Setting values of the paste modes
constexpr const char* PASTE_MODE_MERGE = "merge";
constexpr const char* PASTE_MODE_SOURCE = "source";
constexpr const char* PASTE_MODE_TEXT = "text";

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
    /// Converts HTML/text to KML if native format not available; HTML as @p mode reads it,
    /// and as plain text in PasteMode::Text.
    static QString pasteAsKml(PasteMode mode = PasteMode::Merge);

    /// @brief Get paste content as plain text
    /// @return Plain text string, or empty if nothing to paste
    static QString pasteAsText();

    // =========================================================================
    // Format Conversion
    // =========================================================================

    /// @brief The paste mode of a setting value ("merge", "source", "text"); PasteMode::Merge
    ///        for any other
    static PasteMode pasteModeFromId(const QString& id);

    /// @brief The look of the scene breaks: the setting editor.sceneBreak.style
    ///        ("asterisks", "asterism", "fleuron", "line" or "blank")
    static QString sceneBreakStyle();

    /// @brief Convert KML to HTML for other programs
    ///
    /// Paragraphs and headings (h1-h3) keep their alignment and their line breaks (br);
    /// formatting tags become b/i/u/s/sub/sup and inline styles (font, size, color, bg) CSS.
    /// Paragraphs in the style "quote" or "block_quote" go in a blockquote, the other
    /// styles are plain paragraphs. A horizontal line is an hr, a scene break a centered
    /// paragraph of its look (a short hr for "line", an empty paragraph for "blank"), and a
    /// page break the CSS page-break-before of what follows it. Metadata tags keep only their
    /// text.
    /// @param kml KML markup string
    /// @param sceneBreakStyle The look of the scene breaks, as sceneBreakStyle() gives it;
    ///        empty: the setting
    /// @return HTML string with equivalent formatting
    static QString kmlToHtml(const QString& kml, const QString& sceneBreakStyle = QString());

    /// @brief HTML for the clipboard: a whole document in UTF-8 around @p fragment, with the
    ///        fragment marked in it
    ///
    /// As the browsers and Word put it on the clipboard: the programs of Windows read the
    /// clipboard's HTML (CF_HTML) as a document, and Qt adds only the markers of the fragment.
    static QString htmlDocument(const QString& fragment);

    /// @brief The clipboard's data of text @p kml copied in Kalahari: the KML itself (MIME_KML),
    ///        its HTML (htmlDocument()) for other programs and its plain text, whose
    ///        paragraphs and line breaks end a line
    static std::unique_ptr<QMimeData> mimeDataOf(const QString& kml);

    /// @brief Convert HTML of another program (Word, LibreOffice, a browser) to KML
    ///
    /// The HTML is read by Qt (QTextDocumentFragment::fromHtml), without anything it refers
    /// to (style sheets, images), as a browser shows it: white space that does not show and
    /// a line break that ends a paragraph are left out, and so are empty elements. In
    /// PasteMode::Merge and PasteMode::Source it keeps the headings (h1-h3; h4-h6 as h3),
    /// the quotes (blockquote, and Word's quote styles) as paragraphs in the style "quote",
    /// the horizontal lines (hr), the page breaks (CSS page-break-before and -after, and
    /// Word's page break), the line breaks and the emphasis (bold, italic, underline,
    /// strikethrough, subscript, superscript); Merge also centered and right-aligned
    /// paragraphs, Source every alignment and the fonts, sizes and colors, also of the styles
    /// of Word's HTML (which Merge, as Word's Merge Formatting, leaves out). PasteMode::Text
    /// keeps the paragraphs and line breaks only. Until the editor has them, a list item
    /// starts with its number or bullet, a table row is a paragraph with its cells separated
    /// by tabs, a link is its text and an image is left out.
    /// @param html HTML markup string
    /// @param mode What the KML keeps
    /// @return KML (<kml> with the paragraphs), empty without text, lines and page breaks
    static QString htmlToKml(const QString& html, PasteMode mode = PasteMode::Merge);

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
    ///
    /// Each paragraph and heading is a line, and so is a line break in it. A horizontal
    /// line is a line of box-drawing characters, a scene break a line of its look (empty
    /// for "blank") and a page break a form feed on a line of its own.
    /// @param kml KML markup string
    /// @param sceneBreakStyle The look of the scene breaks, as sceneBreakStyle() gives it;
    ///        empty: the setting
    /// @return Plain text without any formatting
    static QString kmlToText(const QString& kml, const QString& sceneBreakStyle = QString());
};

}  // namespace kalahari::editor
