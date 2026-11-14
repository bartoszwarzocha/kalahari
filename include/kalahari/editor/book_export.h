// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_export.h
/// @brief Export of a book to a Word document (DOCX) and to Markdown (GitHub Flavored)
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
/// font of Normal the caller chooses (the editor's by default). docs/kml_format.md lists what
/// each element of the text becomes in each format.

#pragma once

#include <kalahari/editor/annotation.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/text_captions.h>
#include <kalahari/editor/text_footnotes.h>

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
    double textWidthPoints() const;

    /// @brief The height of the text between the margins, in points
    double textHeightPoints() const;
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
    bool firstPageLeft = false;  ///< It starts on a left page (mirror margins)
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
    bool annotationsAsComments = false;  ///< The annotations as Word's comments (DOCX)
    bool copyImages = true;  ///< Markdown: the images to a folder next to the file; false: the
                             ///< links name the files of the book
    bool endnotesAtBookEnd = false;  ///< The endnotes at the end of the file; false: after each
                                     ///< text
    /// The words in front of the text of a TODO and a note in Word's comments ("TODO", "Note"),
    /// in the program's language; a comment has none
    QHash<AnnotationKind, QString> annotationLabels;
    std::vector<ExportItem> items;
};

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

/// @brief Write the book to a Markdown file at @p path, its images to a folder next to it
///        (BookExport::copyImages) named @p imagesFolderName
/// @return false, with what went wrong in @p problem, when it cannot
bool exportMarkdown(const BookExport& book, const QString& path, const QString& imagesFolderName,
                    const ExportProgress& progress, ExportResult* result,
                    ExportProblem* problem);

}  // namespace kalahari::editor
