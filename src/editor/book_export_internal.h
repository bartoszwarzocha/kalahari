/// @file book_export_internal.h
/// @brief What the writers of DOCX and Markdown share (not part of the editor's API): a text
///        read as the editor reads it with its numbers, the pieces of its paragraphs, the
///        styles' kinds and the pictures of the images

#pragma once

#include <kalahari/editor/book_export.h>
#include <kalahari/editor/image_store.h>
#include <kalahari/editor/text_images.h>
#include <kalahari/editor/text_tables.h>

#include <QByteArray>
#include <QSizeF>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>

#include <memory>
#include <optional>
#include <vector>

class QTextTable;

namespace kalahari::editor::exporting {

/// @brief A text of the export, read as the editor reads it, with the numbers of its lists,
///        captions and notes in the book around it
class ExportText {
public:
    /// @brief Read the text of @p item (an item without a text: an empty document)
    explicit ExportText(const ExportItem& item);

    ExportText(const ExportText&) = delete;
    ExportText& operator=(const ExportText&) = delete;

    const QTextDocument& document() const { return *m_document; }
    const CaptionNumbers& captions() const { return m_captions; }
    const FootnoteNumbers& notes() const { return m_notes; }

    /// @brief The number of a list item as the editor counts it; -1: none (outside a list, an
    ///        item without a number)
    int listNumber(const QTextBlock& block) const;

    /// @brief The annotations of the text with their places
    const std::vector<AnnotationPlace>& annotations() const { return m_annotations; }

private:
    std::unique_ptr<QTextDocument> m_document;
    CaptionNumbers m_captions;
    FootnoteNumbers m_notes;
    std::vector<int> m_listNumbers;
    std::vector<AnnotationPlace> m_annotations;
};

/// @brief A piece of a paragraph: a run of text in one format, or one object of the line
struct Piece {
    enum class Kind {
        Text,       ///< Characters (a line break: QChar::LineSeparator, a tab: '\t')
        Image,      ///< An image
        Field,      ///< A caption's number or a cross-reference: the text it shows
        Note,       ///< The number of a footnote or an endnote
    };

    Kind kind = Kind::Text;
    QString text;            ///< Text: its characters; Field: what it shows; Note: its number
    QTextCharFormat format;  ///< Its format
    int position = 0;        ///< Its first character in the document
    int length = 0;          ///< Its characters in the document
    ImageProperties image;   ///< Kind::Image
    Footnote note;           ///< Kind::Note
    std::optional<CaptionNumberField> captionNumber;  ///< Kind::Field: a caption's number
    std::optional<CrossReference> reference;          ///< Kind::Field: a cross-reference
    QString link;  ///< The address of its link, when the program opens it; empty: none
};

/// @brief The pieces of a paragraph, in text order; neighbouring characters of one format
///        make one piece
std::vector<Piece> piecesOf(const QTextBlock& block, const ExportText& text);

/// @brief The text of a paragraph as it reads: its fields as they show, its notes and images
///        left out, a line break a space (a heading in a table of contents)
QString plainTextOf(const QTextBlock& block, const ExportText& text);

/// @brief The level of the heading a style is, through its base styles: 1 to 3 for a style
///        that is, or is based on, Heading 1 to 3; 0 for any other
int headingLevelOfStyle(const ParagraphStyleSet& styles, const QString& id);

/// @brief Whether a style is @p ancestor or is based on it, at any depth
bool isStyleBasedOn(const ParagraphStyleSet& styles, const QString& id, const QString& ancestor);

/// @brief The style a paragraph is shown in: its own, or Normal for one the book does not have
QString shownStyleOf(const ParagraphStyleSet& styles, const QTextBlock& block);

/// @brief Whether the first paragraph with text of a document is a heading: the text keeps its
///        own, without the title of the Navigator
bool startsWithHeading(const QTextDocument& document, const ParagraphStyleSet& styles);

/// @brief The title style of an item: its own when the book has it, else Heading 1
QString titleStyleOf(const ExportItem& item, const ParagraphStyleSet& styles);

/// @brief The width of a table in points: its own, or the width of the text it is in
double tableWidthPoints(const TableShape& shape, double lineWidth);

/// @brief The widths of the columns of a table in points
std::vector<double> columnWidthsPoints(const TableShape& shape, double tableWidth);

/// @brief A picture of an image to put in a file
struct ExportPicture {
    QByteArray data;      ///< The file's bytes
    QString extension;    ///< "png", "jpeg", "gif", "bmp"
    QString contentType;  ///< "image/png"...
    QSizeF points;        ///< The size it is shown in, with its frame
    bool missing = true;  ///< The file is missing or cannot be read
};

/// @brief The picture of an image as the editor shows it: its file as it is when it is shown
///        so (a photo or a drawing of a format every program reads, not cropped, turned,
///        recolored or framed), else drawn as it is shown, with its frame, as PNG
/// @param lineWidth The width of its line in points: an image is never wider, and a percent
///        is of it
/// @param pageHeight The height of the text of a page in points: an image is never higher;
///        0: no limit
ExportPicture exportPicture(const ImageStore& store, const ImageProperties& image,
                            double lineWidth, double pageHeight);

/// @brief A text with the characters XML does not allow taken out, escaped for an attribute or
///        an element
QString xmlEscaped(const QString& text);

/// @brief Write @p data as the whole of file @p path, replaced only once it is written whole
/// @return false, with what went wrong in @p problem, when it cannot
bool writeExportFile(const QString& path, const QByteArray& data, ExportProblem* problem);

}  // namespace kalahari::editor::exporting
