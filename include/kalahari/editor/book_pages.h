// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_pages.h
/// @brief The pages of a book as the page view of the editor shows them, for a PDF file and
///        for a printer
///
/// Each item of the export (BookExport) is laid out as the editor lays out a text in the page
/// view (KalahariTextDocumentLayout with its page flow, EditorRenderPipeline): the same line
/// and page breaks, the footnotes at the bottom of the pages, the header rows of the tables
/// repeated, the mirror margins. The look comes from the book's styles, as in a Word document:
/// the font of Normal and its size, the sizes, spacing and indents of the styles; the colors
/// are those of the light paper (Settings > Editor > Colors), also when the editor shows the
/// dark one. The pages are numbered through the book, each text starts on a new page or goes
/// on after the one before it (BookExport::textsOnNewPage), with mirror margins on a right
/// page unless it starts on a left one (a page left empty before it). Above the text the
/// running head, below it the number of the page.

#pragma once

#include <kalahari/editor/annotation.h>
#include <kalahari/editor/book_export.h>
#include <kalahari/editor/render_context.h>

#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include <memory>
#include <vector>

class QPainter;

namespace kalahari::editor {

class ImageStore;

/// @brief A place in the book: a page (from 0) and a point on it, in points from its top left
struct PagePlace {
    int page = -1;  ///< -1: none
    QPointF point;
};

/// @brief A heading of the book: a bookmark of a PDF, a line of the table of contents
struct PageHeading {
    int level = 1;     ///< 1 to 3: the level of its style (a part's and a chapter's title too)
    QString text;      ///< As it reads
    PagePlace place;   ///< The top of its first line
};

/// @brief A link on a page to a place of the book: a line of the table of contents, a
///        cross-reference to a caption
struct PageLink {
    int page = 0;      ///< The page it is on
    QRectF rect;       ///< Where, in points from the page's top left
    PagePlace target;  ///< Where it goes
};

/// @brief An annotation on a page: a comment of a PDF
struct PageComment {
    int page = 0;           ///< The page of its fragment's start
    QPointF point;          ///< The top left of its fragment's start, in points
    Annotation annotation;  ///< Its kind, text, author and date
};

/// @brief The pages of a book, laid out as the page view does
class BookPages {
public:
    /// @brief How the pages show the text
    struct Options {
        /// The links of the text stay links (a PDF to read or to archive); false: their text
        /// only, as on paper (a printer, a PDF for a print shop)
        bool links = true;
    };

    /// @brief The pages of @p book (which must outlive them); layOut() fills them
    explicit BookPages(const BookExport& book, const Options& options);
    ~BookPages();

    BookPages(const BookPages&) = delete;
    BookPages& operator=(const BookPages&) = delete;

    /// @brief Lay the book out on its pages (the numbers of the table of contents too)
    /// @param progress Reports the progress, 0-100; empty: none
    void layOut(const ExportProgress& progress = {});

    /// @brief The pages of the book
    int pageCount() const;

    /// @brief The size of a page, in points
    QSizeF pageSize() const;

    /// @brief Draw a page with its running head and its number
    /// @param painter Its origin at the top left of the page, its units points
    /// @param page From 0
    void paint(QPainter* painter, int page) const;

    /// @brief Whether a page is left empty: the next text starts on a right page
    bool isBlank(int page) const;

    /// @brief The page a place of a text is on (the cursor's in its editor)
    /// @param elementId The text's element (empty: a file outside the book, the only item)
    /// @param position A position in the text as its editor has it
    /// @return From 0; -1 when the text is not in the book
    int pageAt(const QString& elementId, int position) const;

    /// @brief The headings of the book in reading order: the titles of its items (without the
    ///        table of contents) and the headings of its texts
    const std::vector<PageHeading>& headings() const { return m_headings; }

    /// @brief The links to places of the book (Options::links): the lines of the tables of
    ///        contents, the cross-references
    const std::vector<PageLink>& links() const { return m_links; }

    /// @brief The annotations of the texts, in reading order
    const std::vector<PageComment>& comments() const { return m_comments; }

    /// @brief The images on the pages and those whose files are missing (left out)
    int images() const { return m_images; }
    int missingImages() const { return m_missingImages; }

private:
    struct Item;

    /// @brief A page of the book: the pages of the items on it (two when a text goes on
    ///        after another on its last page)
    struct Page {
        std::vector<std::pair<size_t, int>> parts;  ///< Item, its page
        bool blank = false;
    };

    /// @brief Build the document of an item with its title and its text (the table of
    ///        contents with the pages of the headings found so far)
    void build(size_t index);

    /// @brief The headings of the texts (their places come from collect())
    void findHeadings();

    /// @brief Put the items on the pages of the book, each laid out where it starts
    void place();

    /// @brief The headings, the links and the annotations on the pages
    void collect();

    /// @brief The KML of the table of contents: a table without borders, a heading a row, its
    ///        page on the right
    QString contentsKml() const;

    /// @brief The place on the book's pages of a point of an item (document coordinates)
    PagePlace placeOf(const Item& item, const QPointF& point) const;

    /// @brief The left margin of a page of the book, in document units
    double leftMarginOf(int page) const;

    /// @brief Draw the running head and the number of a page
    void paintMargins(QPainter* painter, int page) const;

    const BookExport& m_book;
    Options m_options;
    std::shared_ptr<ImageStore> m_store;  ///< The files of the images, for every item
    RenderContext m_context;      ///< Of the pages, the first one a right page (computed)
    std::vector<std::unique_ptr<Item>> m_items;
    std::vector<Page> m_pages;
    std::vector<PageHeading> m_headings;
    std::vector<std::pair<size_t, int>> m_headingBlocks;  ///< Of each heading: item, block
    std::vector<int> m_contentsPages;  ///< The pages the tables of contents show
    std::vector<PageLink> m_links;
    std::vector<PageComment> m_comments;
    int m_images = 0;
    int m_missingImages = 0;
    double m_dpi = DEFAULT_DPI;  ///< The document units of an inch (the layout's pixels)
    double m_pitch = 0.0;        ///< From the top of a page's text area to the next one's
    double m_textHeight = 0.0;   ///< Of a page's text area
    double m_goOnRoom = 0.0;     ///< A text goes on after another when so much room is left
};

}  // namespace kalahari::editor
