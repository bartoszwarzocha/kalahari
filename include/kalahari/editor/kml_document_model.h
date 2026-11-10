/// @file kml_document_model.h
/// @brief KML reader: the paragraphs of a chapter with their text and formatting
///
/// KmlDocumentModel reads KML into paragraphs: plain text, format runs (formatting,
/// metadata and annotations as QTextCharFormat), alignment, style, the paragraph's own
/// formatting and its place in a list, and the objects between them (a horizontal line, the
/// scene and page breaks). BookEditor builds its
/// QTextDocument from it, both when loading a chapter and when pasting Kalahari content.

#pragma once

#include <kalahari/editor/annotation.h>
#include <kalahari/editor/format_run.h>
#include <kalahari/editor/paragraph_format.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/text_lists.h>

#include <QHash>
#include <QSet>
#include <QString>
#include <QTextCharFormat>

#include <optional>
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
///     QString style = content.paragraphStyle(i);       // empty: Normal
///     TextObject object = content.paragraphObject(i);  // None: a paragraph of text
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

    /// @brief The paragraph's style: <h1>-<h3> are the headings, <p style="..."> names one
    /// @param index Paragraph index (0-based)
    /// @return The id of its style; empty for Normal (a <p> without one), an object, or if
    ///         index is out of range
    QString paragraphStyle(size_t index) const;

    /// @brief The object the paragraph is: <hr/>, <scene-break/> or <page-break/>
    /// @param index Paragraph index (0-based)
    /// @return The object; None for a paragraph of text or if index is out of range
    TextObject paragraphObject(size_t index) const;

    /// @brief The paragraph's own formatting: indents, spacing, line spacing, breaks
    /// @param index Paragraph index (0-based)
    /// @return The parts its attributes set; none if index is out of range
    ParagraphFormat paragraphOwnFormat(size_t index) const;

    /// @brief The paragraph's place in a list: an item of <ol> or <ul>
    /// @param index Paragraph index (0-based)
    /// @return The item, its list numbered from 1 in the order of the lists of the KML
    ///         (a list with continue="true" has the number of the list before it); nothing
    ///         for a paragraph outside a list or if index is out of range
    std::optional<ListItem> paragraphList(size_t index) const;

    /// @brief The annotations on the place where the paragraph starts
    /// @param index Paragraph index (0-based)
    /// @return Annotations on a place (the annotations inside the paragraph are in its
    ///         format runs); none if index is out of range
    AnnotationList paragraphStartAnnotations(size_t index) const;

    // =========================================================================
    // Annotations of a chapter
    // =========================================================================

    /// @brief The annotations of a chapter's KML, without reading its paragraphs
    ///
    /// They come from the <annotations> section before the paragraphs, in its order: the
    /// order of their anchors in the text. An annotation without an id or with an unknown
    /// kind is left out, and so is a repeated id.
    /// @param kml KML markup string
    static AnnotationList readAnnotations(const QString& kml);

private:
    /// @brief Internal paragraph storage
    struct Paragraph {
        QString text;                           ///< Plain text content
        std::vector<FormatRun> formats;         ///< Format runs within paragraph, in text order
        Qt::Alignment alignment;                ///< Own alignment (none: the default)
        AnnotationList startAnnotations;        ///< Annotations on the paragraph's start
        QString style;                          ///< Id of its style (empty: Normal)
        TextObject object = TextObject::None;   ///< The object it is (None: text)
        ParagraphFormat format;                 ///< Its own formatting
        std::optional<ListItem> list;           ///< Its place in a list
    };

    /// @brief The list being read: its number, and the formats of its levels read so far
    struct ListReading {
        int id = 0;
        ListDefinition definition;
        /// The paragraphs of the list's element, each with the levels known when it was read
        std::vector<std::pair<size_t, unsigned>> items;
        unsigned knownLevels = 0;  ///< Bit of each level whose element was read
    };

    /// @brief Whether an element at the top level of KML is a paragraph or an object
    static bool isBlockElement(QStringView tag);

    /// @brief Read the <annotations> section: the annotations the anchors refer to
    /// @param reader XML reader positioned at the section's start element; on return it is
    ///               positioned at its end element
    void parseAnnotations(QXmlStreamReader& reader);

    /// @brief Parse a paragraph element: <p>, <paragraph> or a heading <h1>-<h3>
    /// @param reader XML reader positioned at the paragraph's start element; on return it
    ///               is positioned after the matching end element
    /// @param para Output paragraph structure
    void parseParagraphElement(QXmlStreamReader& reader, Paragraph& para);

    /// @brief Read the attributes of a paragraph element: its style, alignment and own
    ///        formatting
    static void readParagraphAttributes(QXmlStreamReader& reader, Paragraph& para);

    /// @brief Parse a list at the top level: <ol> or <ul>
    /// @param reader XML reader positioned at the list's start element; on return it is
    ///               positioned after the matching end element
    void parseTopLevelList(QXmlStreamReader& reader);

    /// @brief Parse a list element of a level, with its items and the lists inside them
    void parseList(QXmlStreamReader& reader, ListReading& list, int level);

    /// @brief Parse an item <li> of a list's level: its paragraph, the lists inside it, and
    ///        paragraphs without a number for the text after them
    void parseListItem(QXmlStreamReader& reader, ListReading& list, int level);

    /// @brief Add a paragraph of a list's element
    void appendListParagraph(Paragraph&& para, ListReading& list);

    /// @brief Parse inline content recursively
    /// @param reader XML reader positioned at content
    /// @param para Output paragraph: its text and format runs (accumulated)
    /// @param currentFormat Current active format
    /// @param endTag Tag name to stop at
    /// @param stopAtBlocks Stop at a paragraph or list element (in a list item), without
    ///        reading it
    /// @return false when it stopped at a paragraph or list element, else true
    bool parseInlineContent(QXmlStreamReader& reader,
                            Paragraph& para,
                            QTextCharFormat currentFormat,
                            const QString& endTag,
                            bool stopAtBlocks = false);

    /// @brief Anchor an annotation to the place after the paragraph's text read so far
    ///
    /// It goes on the last character read, or on the paragraph's start.
    static void anchorAfterText(Paragraph& para, const Annotation& annotation);

    /// @brief Append text read in a format to a paragraph: a format run when the format
    ///        has any property
    static void appendText(Paragraph& para, const QString& text, const QTextCharFormat& format);

    std::vector<Paragraph> m_paragraphs;    ///< All paragraphs
    int m_lastListId = 0;                   ///< The number of the last list read
    ListDefinition m_lastListDefinition;    ///< The formats of its levels at its end
    QHash<QString, Annotation> m_annotations;  ///< The annotations of the section, by id
    QSet<QString> m_anchoredIds;            ///< Ids of the annotations anchored in the text

    static const std::vector<FormatRun> s_emptyFormats;  ///< Empty format vector
};

} // namespace editor
} // namespace kalahari
