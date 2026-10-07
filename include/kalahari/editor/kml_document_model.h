/// @file kml_document_model.h
/// @brief KML reader: the paragraphs of a chapter with their text and formatting
///
/// KmlDocumentModel reads KML into paragraphs: plain text, format runs (formatting and
/// metadata as QTextCharFormat) and alignment. BookEditor builds its QTextDocument from
/// it, both when loading a chapter and when pasting Kalahari content.

#pragma once

#include <kalahari/editor/format_run.h>

#include <QString>
#include <QTextCharFormat>

#include <vector>

class QXmlStreamReader;

namespace kalahari {
namespace editor {

/// @brief KML reader: the paragraphs of a chapter with their text and formatting
///
/// Usage:
/// @code
/// KmlDocumentModel content;
/// content.loadKml(kmlString);
/// for (size_t i = 0; i < content.paragraphCount(); ++i) {
///     QString text = content.paragraphText(i);
///     const std::vector<FormatRun>& formats = content.paragraphFormats(i);
///     Qt::Alignment alignment = content.paragraphAlignment(i);
/// }
/// @endcode
class KmlDocumentModel {
public:
    // =========================================================================
    // Document Loading
    // =========================================================================

    /// @brief Read KML, replacing the previous content
    /// @param kml KML markup string (a fragment without a root element gets one)
    /// @return false if the KML is not well-formed; the paragraphs read before the error
    ///         are kept
    bool loadKml(const QString& kml);

    /// @brief Clear document
    void clear();

    /// @brief Check if document is empty
    /// @return true if no paragraphs
    bool isEmpty() const;

    // =========================================================================
    // Paragraph Access
    // =========================================================================

    /// @brief Get paragraph count
    /// @return Number of paragraphs
    size_t paragraphCount() const;

    /// @brief Get plain text of paragraph
    /// @param index Paragraph index (0-based)
    /// @return Plain text content
    QString paragraphText(size_t index) const;

    /// @brief Get format runs for paragraph
    /// @param index Paragraph index (0-based)
    /// @return Vector of format runs
    const std::vector<FormatRun>& paragraphFormats(size_t index) const;

    /// @brief Get the paragraph's own alignment
    /// @param index Paragraph index (0-based)
    /// @return The alignment its align attribute sets; none without one (it is shown with
    ///         DEFAULT_PARAGRAPH_ALIGNMENT) or if index is out of range
    Qt::Alignment paragraphAlignment(size_t index) const;

private:
    /// @brief Internal paragraph storage
    struct Paragraph {
        QString text;                           ///< Plain text content
        std::vector<FormatRun> formats;         ///< Format runs within paragraph
        Qt::Alignment alignment;                ///< Own alignment (none: the default)
    };

    /// @brief Parse a paragraph element
    /// @param reader XML reader positioned at the paragraph's start element; on return it
    ///               is positioned after the matching end element
    /// @param para Output paragraph structure
    void parseParagraphElement(QXmlStreamReader& reader, Paragraph& para);

    /// @brief Parse inline content recursively
    /// @param reader XML reader positioned at content
    /// @param text Output plain text (accumulated)
    /// @param formats Output format runs (accumulated)
    /// @param currentFormat Current active format
    /// @param currentPos Current position in text
    /// @param endTag Tag name to stop at
    void parseInlineContent(QXmlStreamReader& reader,
                            QString& text,
                            std::vector<FormatRun>& formats,
                            QTextCharFormat currentFormat,
                            size_t& currentPos,
                            const QString& endTag);

    std::vector<Paragraph> m_paragraphs;    ///< All paragraphs

    static const std::vector<FormatRun> s_emptyFormats;  ///< Empty format vector
};

} // namespace editor
} // namespace kalahari
