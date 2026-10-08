/// @file kml_document_model.h
/// @brief KML reader: the paragraphs of a chapter with their text and formatting
///
/// KmlDocumentModel reads KML into paragraphs: plain text, format runs (formatting,
/// metadata and annotations as QTextCharFormat) and alignment. BookEditor builds its
/// QTextDocument from it, both when loading a chapter and when pasting Kalahari content.

#pragma once

#include <kalahari/editor/annotation.h>
#include <kalahari/editor/format_run.h>

#include <QHash>
#include <QSet>
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

    /// @brief The annotations on the place where the paragraph starts
    /// @param index Paragraph index (0-based)
    /// @return Annotations on a place (the annotations inside the paragraph are in its
    ///         format runs); none if index is out of range
    AnnotationList paragraphStartAnnotations(size_t index) const;

private:
    /// @brief Internal paragraph storage
    struct Paragraph {
        QString text;                           ///< Plain text content
        std::vector<FormatRun> formats;         ///< Format runs within paragraph, in text order
        Qt::Alignment alignment;                ///< Own alignment (none: the default)
        AnnotationList startAnnotations;        ///< Annotations on the paragraph's start
    };

    /// @brief Read the <annotations> section: the annotations the anchors refer to
    /// @param reader XML reader positioned at the section's start element; on return it is
    ///               positioned at its end element
    void parseAnnotations(QXmlStreamReader& reader);

    /// @brief Parse a paragraph element
    /// @param reader XML reader positioned at the paragraph's start element; on return it
    ///               is positioned after the matching end element
    /// @param para Output paragraph structure
    void parseParagraphElement(QXmlStreamReader& reader, Paragraph& para);

    /// @brief Parse inline content recursively
    /// @param reader XML reader positioned at content
    /// @param para Output paragraph: its text and format runs (accumulated)
    /// @param currentFormat Current active format
    /// @param endTag Tag name to stop at
    void parseInlineContent(QXmlStreamReader& reader,
                            Paragraph& para,
                            QTextCharFormat currentFormat,
                            const QString& endTag);

    /// @brief Anchor an annotation to the place after the paragraph's text read so far
    ///
    /// It goes on the last character read, or on the paragraph's start.
    static void anchorAfterText(Paragraph& para, const Annotation& annotation);

    std::vector<Paragraph> m_paragraphs;    ///< All paragraphs
    QHash<QString, Annotation> m_annotations;  ///< The annotations of the section, by id
    QSet<QString> m_anchoredIds;            ///< Ids of the annotations anchored in the text

    static const std::vector<FormatRun> s_emptyFormats;  ///< Empty format vector
};

} // namespace editor
} // namespace kalahari
