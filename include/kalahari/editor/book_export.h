// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file book_export.h
/// @brief Export of a book to a Word document (DOCX), to Markdown (GitHub Flavored), to PDF and
///        to EPUB
///
/// The caller gathers what goes to the file (BookExport): the texts of the book in reading
/// order with their KML, the titles the Navigator gives them, the numbering of their captions
/// and notes in the book around them (CaptionContext, FootnoteContext), the book's paragraph
/// styles and the page of the settings. The writers read each text as the editor does
/// (KmlDocumentModel, appendParagraphs()), so the numbers of the lists, the captions and the
/// notes are the program's.
///
/// The look of the file comes from the book's styles (the style pack with the book's changes),
/// not from the editor's look on the screen: the sizes, spacing and indents of the styles, the
/// font of Normal the caller chooses (the editor's by default). A PDF has the pages of the page
/// view (BookPages); an EPUB the texts, which the reader divides into its pages.
/// docs/kml_format.md lists what each element of the text becomes in each format.

#ifndef KALAHARI_EDITOR_BOOK_EXPORT_H
#define KALAHARI_EDITOR_BOOK_EXPORT_H

#include <kalahari/core/page_start.h>
#include <kalahari/editor/annotation.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/text_captions.h>
#include <kalahari/editor/text_fields.h>
#include <kalahari/editor/text_footnotes.h>

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

namespace kalahari::editor {

/// @brief The formats a book is exported to
enum class ExportFormat {
    Docx,      ///< A Word document
    Markdown,  ///< GitHub Flavored Markdown
    Pdf,       ///< A PDF document: the pages of the page view
    Epub,      ///< An EPUB 3 e-book
};

/// @brief What the top of the pages of a text shows (a PDF, a printout, a Word document)
enum class RunningHead {
    None,             ///< Nothing
    AuthorAndTitle,   ///< The author on the left pages, the book's title on the right ones
    TitleAndChapter,  ///< The book's title on the left pages, the text's title on the right ones
    Chapter,          ///< The text's title on both
};

/// @brief What a PDF is for
enum class PdfPurpose {
    Reading,    ///< To read and to send: links, bookmarks, comments
    PrintShop,  ///< For a print shop (PDF/X-4): without links and comments, with a color profile
    Archive,    ///< To keep (PDF/A-1b): it opens the same in years
};

/// @brief The options of a PDF
struct PdfOptions {
    PdfPurpose purpose = PdfPurpose::Reading;
    bool bookmarks = true;     ///< The titles of the parts and texts and the headings as bookmarks
    bool facingPages = false;  ///< The reader shows the pages in pairs, as an open book
    bool grayscale = false;    ///< In shades of gray, for a black and white printer
};

/// @brief The options and the data of an EPUB
struct EpubOptions {
    bool bookStyles = true;  ///< The look of the book's styles; false: a simple one, the font and
                             ///< its size chosen by the reader
    bool embedFont = false;  ///< The font of the text in the file, when its licence allows it
    QString coverImage;      ///< The file of the cover's picture; empty: none
    QString description;     ///< What the book is about (the blurb); empty: none
    QString isbn;            ///< Empty, or not an ISBN (isIsbn()): the book's identifier is
                             ///< @c identifier
    QString publisher;       ///< Empty: none
    QString identifier;      ///< The book's own lasting identifier (a UUID) when it has no ISBN
    QDateTime modified;      ///< When the book was changed last; invalid: now
};

/// @brief The page of a Word document, as Settings > Editor > Pages and Margins give it
struct ExportPage {
    double width = 210.0;   ///< Millimetres
    double height = 297.0;  ///< Millimetres
    double top = 25.4;      ///< Margins in millimetres
    double bottom = 25.4;
    double left = 25.4;     ///< Without mirror margins
    double right = 25.4;
    bool mirrored = false;  ///< Mirror margins: inner and outer instead of left and right
    double inner = 30.0;    ///< At the binding
    double outer = 20.0;    ///< At the edge
    bool pageNumbers = true;  ///< Numbers at the bottom of the pages

    /// @brief The width of the text between the margins, in points
    [[nodiscard]] double textWidthPoints() const;

    /// @brief The height of the text between the margins, in points
    [[nodiscard]] double textHeightPoints() const;
};

/// @brief What an item of the export is
enum class ExportItemKind {
    Text,      ///< A text of the book: its KML
    Group,     ///< A group of texts (a part): only its title
    Contents,  ///< The table of contents of the book: its title and the headings of the file
};

/// @brief An item of the export, in the order of the file
struct ExportItem {
    ExportItemKind kind = ExportItemKind::Text;
    QString elementId;   ///< Its element in the book; empty: a file outside a book
    QString title;       ///< The title the Navigator gives it; empty: none in the file
    QString titleStyle;  ///< The paragraph style of its title (part_title, chapter_title);
                         ///< empty or one the book does not have: Heading 1
    QString kml;         ///< Its text (ExportItemKind::Text)
    CaptionContext captions;   ///< The numbering of its captions in the book around it
    FootnoteContext footnotes;  ///< The numbering of its notes in the book around it
    /// The page it starts on with mirror margins and each item from a new page: a right one,
    /// a left one (a page left empty before it when needed) or the next one; never Auto
    core::PageStart start = core::PageStart::Right;
};

/// @brief What goes to the file of an export, and how it looks
struct BookExport {
    QString title;     ///< Of the book: the document's properties
    QString author;
    QString language;  ///< Language code of the text ("pl"); the spelling language in Word
    ParagraphStyleSet styles;  ///< The book's paragraph styles
    QString fontFamily;        ///< The font of Normal (and of the styles without one of their own)
    QString imagesFolder;      ///< The folder the sources of the images are relative to; empty:
                               ///< no images go to the file
    SceneBreakLook sceneBreak = SceneBreakLook::Asterisks;  ///< As the editor shows them
    ExportPage page;
    bool textsOnNewPage = true;   ///< Each item from a new page (DOCX)
    bool titleHeadings = true;    ///< The titles of the items as headings (a text that starts
                                  ///< with a heading of its own keeps it, without the title)
    bool annotationsAsComments = false;  ///< The annotations as comments (DOCX, PDF)
    RunningHead runningHead = RunningHead::None;  ///< At the top of the pages (DOCX, PDF)
    /// The colors of the pages (PDF, printing): those of the light paper
    EditorColors colors;
    PdfOptions pdf;
    EpubOptions epub;
    bool copyImages = true;  ///< Markdown: the images to a folder next to the file; false: the
                             ///< links name the files of the book
    bool endnotesAtBookEnd = false;  ///< The endnotes at the end of the file; false: after each
                                     ///< text
    /// The words in front of the text of a TODO and a note in Word's comments ("TODO", "Note"),
    /// in the program's language; a comment has none
    QHash<AnnotationKind, QString> annotationLabels;
    /// The own fields of the book and of its series, by their ids: what their fields show
    QMap<QString, CustomFieldValue> customFields;
    std::vector<ExportItem> items;
};

/// @brief What the fields of an item of the export show: the book's title, author and
///        language, the item's title, the own fields of the book, the time of the export
FieldContext exportFieldContext(const BookExport& book, const ExportItem& item);

/// @brief What went wrong with an export
struct ExportProblem {
    enum class Kind {
        None,         ///< Nothing went wrong
        CannotWrite,  ///< The file or its folder of images cannot be written there
        DiskFull,     ///< The disk has no room left
        Other,        ///< Something else: only the lines say what
    };

    Kind kind = Kind::None;
    QStringList lines;  ///< How it went wrong, in English, one problem a line
};

/// @brief What an export wrote
struct ExportResult {
    QString file;          ///< The file
    QString imagesFolder;  ///< Markdown: the folder of its images; empty: none written
    int images = 0;        ///< The images written to the file or to its folder
    int missingImages = 0;  ///< The images whose files are missing or unreadable (left out)
    bool coverMissing = false;     ///< EPUB: the cover's picture cannot be read (left out)
    bool fontNotEmbedded = false;  ///< EPUB: the font of the text is not in the file (its
                                   ///< licence does not allow it, or it is not on the computer)
};

/// @brief Reports the progress of the work, 0-100
using ExportProgress = std::function<void(int)>;

/// @brief The name of the folder of the images of a Markdown file named @p baseName (without
///        its extension): "<name> – images", the last word in the program's language
QString markdownImagesFolderName(const QString& baseName, const QString& imagesWord);

/// @brief Write the book to a Word document at @p path (replaced only once it is written whole)
/// @return false, with what went wrong in @p problem, when it cannot
bool exportDocx(const BookExport& book, const QString& path, const ExportProgress& progress,
                ExportResult* result, ExportProblem* problem);

/// @brief Write the book to a PDF document at @p path (replaced only once it is written whole)
/// @return false, with what went wrong in @p problem, when it cannot
bool exportPdf(const BookExport& book, const QString& path, const ExportProgress& progress,
               ExportResult* result, ExportProblem* problem);

/// @brief Write the book to an EPUB 3 e-book at @p path (replaced only once it is written whole)
/// @return false, with what went wrong in @p problem, when it cannot
bool exportEpub(const BookExport& book, const QString& path, const ExportProgress& progress,
                ExportResult* result, ExportProblem* problem);

/// @brief Whether @p text is an ISBN: 13 digits (or 10, the last one may be X) with the check
///        digit of an ISBN, between them hyphens or spaces
bool isIsbn(const QString& text);

/// @brief Whether the PDF options of this purpose can be written by the Qt the program runs
///        with: a file for a print shop (PDF/X-4) and one in shades of gray need Qt 6.8
bool pdfPurposeSupported(PdfPurpose purpose);
bool pdfGrayscaleSupported();

/// @brief Write the book to a Markdown file at @p path, its images to a folder next to it
///        (BookExport::copyImages) named @p imagesFolderName
/// @return false, with what went wrong in @p problem, when it cannot
bool exportMarkdown(const BookExport& book, const QString& path, const QString& imagesFolderName,
                    const ExportProgress& progress, ExportResult* result,
                    ExportProblem* problem);

}  // namespace kalahari::editor

#endif  // KALAHARI_EDITOR_BOOK_EXPORT_H
