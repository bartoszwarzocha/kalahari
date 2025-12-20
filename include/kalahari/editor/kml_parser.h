/// @file kml_parser.h
/// @brief KML Parser - parses KML markup into document model (OpenSpec #00042 Phase 1.10/1.12)
///
/// KmlParser uses QXmlStreamReader to parse KML (Kalahari Markup Language) documents
/// into KmlDocument/KmlParagraph/KmlElement objects. It handles:
/// - Paragraph elements (<p>)
/// - Inline formatting elements (<b>, <i>, <u>, <s>, <sub>, <sup>)
/// - Text runs (<t>)
/// - Table elements (<table>, <tr>, <td>, <th>) - Phase 1.12
/// - Style attributes
/// - Nested element structures
///
/// Error handling:
/// - Returns nullptr or empty document for malformed input
/// - Provides error message via lastError()

#pragma once

#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/kml_element.h>
#include <kalahari/editor/kml_table.h>
#include <QString>
#include <memory>

class QXmlStreamReader;

namespace kalahari::editor {

/// @brief Result of a parse operation
///
/// Contains either a successfully parsed result or error information.
template<typename T>
struct ParseResult {
    std::unique_ptr<T> result;  ///< Parsed object (nullptr on error)
    bool success;               ///< True if parsing succeeded
    QString errorMessage;       ///< Error message if parsing failed
    int errorLine;              ///< Line number where error occurred (-1 if unknown)
    int errorColumn;            ///< Column number where error occurred (-1 if unknown)

    /// @brief Check if parsing was successful
    explicit operator bool() const { return success; }

    /// @brief Create a successful result
    static ParseResult<T> ok(std::unique_ptr<T> value) {
        return ParseResult<T>{std::move(value), true, QString(), -1, -1};
    }

    /// @brief Create an error result
    static ParseResult<T> error(const QString& message, int line = -1, int col = -1) {
        return ParseResult<T>{nullptr, false, message, line, col};
    }
};

/// @brief KML document parser
///
/// Parses KML markup strings into KmlDocument objects. The parser is stateless
/// and can be reused for multiple parse operations.
///
/// Supported KML structure:
/// @code
/// <doc>
///   <p style="heading1">Heading</p>
///   <p>Normal paragraph with <b>bold</b> and <i>italic</i> text.</p>
///   <p>Formula: H<sub>2</sub>O and x<sup>2</sup></p>
/// </doc>
/// @endcode
///
/// Simplified format (paragraphs without doc wrapper):
/// @code
/// <p>First paragraph</p>
/// <p>Second paragraph</p>
/// @endcode
///
/// Example usage:
/// @code
/// KmlParser parser;
/// auto result = parser.parseDocument("<doc><p>Hello world</p></doc>");
/// if (result) {
///     // Use result.result
/// } else {
///     qWarning() << "Parse error:" << result.errorMessage;
/// }
/// @endcode
class KmlParser {
public:
    /// @brief Default constructor
    KmlParser();

    /// @brief Destructor
    ~KmlParser();

    // =========================================================================
    // Document Parsing
    // =========================================================================

    /// @brief Parse a complete KML document
    /// @param kml The KML markup string
    /// @return ParseResult containing KmlDocument or error information
    /// @note Accepts either <doc>...</doc> wrapper or bare paragraphs
    ParseResult<KmlDocument> parseDocument(const QString& kml);

    /// @brief Parse a single paragraph from KML
    /// @param kml The KML markup string (e.g., "<p>Hello</p>")
    /// @return ParseResult containing KmlParagraph or error information
    ParseResult<KmlParagraph> parseParagraph(const QString& kml);

    /// @brief Parse inline elements from KML
    /// @param kml The KML markup string (e.g., "<b>Bold</b>")
    /// @return ParseResult containing KmlElement or error information
    /// @note Can return any inline element type (text run, bold, italic, etc.)
    ParseResult<KmlElement> parseElement(const QString& kml);

    /// @brief Parse a table from KML
    /// @param kml The KML markup string (e.g., "<table><tr><td>Cell</td></tr></table>")
    /// @return ParseResult containing KmlTable or error information
    ParseResult<KmlTable> parseTable(const QString& kml);

    // =========================================================================
    // Error Information
    // =========================================================================

    /// @brief Get the last error message
    /// @return Error message from the last failed parse operation
    const QString& lastError() const;

    /// @brief Get the line number of the last error
    /// @return Line number (1-based), or -1 if unknown
    int lastErrorLine() const;

    /// @brief Get the column number of the last error
    /// @return Column number (1-based), or -1 if unknown
    int lastErrorColumn() const;

private:
    // Internal parsing methods
    std::unique_ptr<KmlDocument> parseDocumentContent(QXmlStreamReader& reader);
    std::unique_ptr<KmlParagraph> parseParagraphElement(QXmlStreamReader& reader);
    std::unique_ptr<KmlElement> parseInlineElement(QXmlStreamReader& reader);
    std::unique_ptr<KmlElement> parseTextRun(QXmlStreamReader& reader);

    // Table parsing methods (Phase 1.12)
    std::unique_ptr<KmlTable> parseTableElement(QXmlStreamReader& reader);
    std::unique_ptr<KmlTableRow> parseTableRowElement(QXmlStreamReader& reader);
    std::unique_ptr<KmlTableCell> parseTableCellElement(QXmlStreamReader& reader, bool isHeader);

    /// @brief Parse children of an inline container element
    /// @param reader The XML reader positioned after the start element
    /// @param container The container to add children to
    /// @param endTagName The expected end tag name
    /// @return true if parsing succeeded
    bool parseInlineChildren(QXmlStreamReader& reader,
                            class KmlInlineContainer* container,
                            const QString& endTagName);

    /// @brief Parse cell content (inline elements) into a paragraph
    /// @param reader The XML reader positioned after the cell start element
    /// @param paragraph The paragraph to add elements to
    /// @param endTagName The expected end tag name (td or th)
    /// @return true if parsing succeeded
    bool parseCellContent(QXmlStreamReader& reader,
                          KmlParagraph* paragraph,
                          const QString& endTagName);

    /// @brief Create inline element by tag name
    /// @param tagName The XML tag name (b, i, u, s, sub, sup)
    /// @return New element or nullptr if tag is unknown
    std::unique_ptr<KmlElement> createInlineElement(const QString& tagName);

    /// @brief Set error from XML reader
    void setError(QXmlStreamReader& reader);

    /// @brief Set custom error message
    void setError(const QString& message, int line = -1, int col = -1);

    /// @brief Clear error state
    void clearError();

    QString m_lastError;     ///< Last error message
    int m_lastErrorLine;     ///< Last error line number
    int m_lastErrorColumn;   ///< Last error column number
};

}  // namespace kalahari::editor
