/// @file kml_parser.h
/// @brief KML Parser - parses KML directly into QTextDocument (OpenSpec #00043 Phase 11.1)
///
/// KmlParser uses QXmlStreamReader to parse KML (Kalahari Markup Language) documents
/// directly into QTextDocument with QTextCharFormat for formatting and metadata.
///
/// Key design principles (Phase 11 Architecture Correction):
/// - Direct KML -> QTextDocument parsing (no intermediate TextBuffer/FormatLayer)
/// - Standard formatting via QTextCharFormat (bold, italic, underline, strikethrough)
/// - Metadata (comments, todos, footnotes) via QTextFormat::UserProperty
///
/// Supported KML structure:
/// @code
/// <kml>
///   <p>Paragraph with <b>bold</b> and <i>italic</i> text.</p>
///   <p>Formula: H<sub>2</sub>O and x<sup>2</sup></p>
///   <p>Text with <comment id="c1">annotated</comment> word.</p>
/// </kml>
/// @endcode

#pragma once

#include <kalahari/editor/kml_format_registry.h>  // For KmlPropertyId enum
#include <QString>
#include <QTextDocument>
#include <QTextCharFormat>
#include <memory>

class QXmlStreamReader;
class QTextCursor;

namespace kalahari {
namespace editor {

// KmlPropertyId enum is now defined in kml_format_registry.h

/// @brief KML document parser producing QTextDocument output
///
/// Parses KML markup strings directly into QTextDocument objects.
/// Uses QTextCharFormat for both formatting (bold, italic, etc.)
/// and metadata (comments, todos, footnotes).
///
/// Example usage:
/// @code
/// KmlParser parser;
///
/// // Parse to new document
/// QTextDocument* doc = parser.parseKml("<kml><p>Hello <b>world</b></p></kml>");
/// if (doc) {
///     // Use document...
///     delete doc;  // Caller owns the document
/// } else {
///     qWarning() << "Parse error:" << parser.lastError();
/// }
///
/// // Or parse into existing document
/// QTextDocument existingDoc;
/// if (parser.parseInto("<p>Text</p>", &existingDoc)) {
///     // Document updated
/// }
/// @endcode
class KmlParser {
public:
    /// @brief Default constructor
    KmlParser();

    /// @brief Destructor
    ~KmlParser();

    // =========================================================================
    // Main Parsing Methods
    // =========================================================================

    /// @brief Parse KML markup into a new QTextDocument
    /// @param kml The KML markup string
    /// @return New QTextDocument (caller owns), or nullptr on error
    /// @note Accepts <kml>, <doc>, <document> wrappers or bare paragraphs
    QTextDocument* parseKml(const QString& kml);

    /// @brief Parse KML into an existing document (clears document first)
    /// @param kml The KML markup string
    /// @param document Target document (will be cleared before parsing)
    /// @return true if parsing succeeded
    bool parseInto(const QString& kml, QTextDocument* document);

    // =========================================================================
    // Error Information
    // =========================================================================

    /// @brief Get the last error message
    /// @return Error message from the last failed parse operation
    QString lastError() const;

    /// @brief Get the line number of the last error
    /// @return Line number (1-based), or -1 if unknown
    int lastErrorLine() const;

    /// @brief Get the column number of the last error
    /// @return Column number (1-based), or -1 if unknown
    int lastErrorColumn() const;

private:
    // =========================================================================
    // Internal Parsing Methods
    // =========================================================================

    /// @brief Parse document content (paragraphs within root element)
    /// @param reader XML reader positioned at document root
    /// @param cursor Text cursor for inserting content
    /// @param rootTag Name of the root element to match end tag
    /// @return true if parsing succeeded
    bool parseDocumentContent(QXmlStreamReader& reader,
                              QTextCursor& cursor,
                              const QString& rootTag);

    /// @brief Parse a single paragraph element
    /// @param reader XML reader positioned at <p> start
    /// @param cursor Text cursor for inserting content
    void parseParagraph(QXmlStreamReader& reader, QTextCursor& cursor);

    /// @brief Parse inline content (text and formatting elements)
    /// @param reader XML reader
    /// @param cursor Text cursor for inserting content
    /// @param activeFormat Current active format (accumulated from parent elements)
    /// @param endTag Tag name to stop at
    /// @note Uses KmlFormatRegistry for tag classification and format application
    void parseInlineContent(QXmlStreamReader& reader,
                            QTextCursor& cursor,
                            QTextCharFormat activeFormat,
                            const QString& endTag);

    /// @brief Parse metadata element and apply to format
    /// @param reader XML reader positioned at metadata element start
    /// @param tag The metadata tag name
    /// @param format Format to modify with metadata property
    void parseMetadataElement(QXmlStreamReader& reader,
                              const QString& tag,
                              QTextCharFormat& format);

    // =========================================================================
    // Error Handling
    // =========================================================================

    /// @brief Set error from XML reader
    void setError(QXmlStreamReader& reader);

    /// @brief Set custom error message
    void setError(const QString& message, int line = -1, int col = -1);

    /// @brief Clear error state
    void clearError();

    // =========================================================================
    // Members
    // =========================================================================

    QString m_lastError;      ///< Last error message
    int m_lastErrorLine;      ///< Last error line number
    int m_lastErrorColumn;    ///< Last error column number
};

} // namespace editor
} // namespace kalahari
