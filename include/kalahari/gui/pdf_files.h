// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file pdf_files.h
/// @brief PDF files as the program shows them: whether it can, their first page, their data,
/// and the settings of their preview

#ifndef KALAHARI_GUI_PDF_FILES_H
#define KALAHARI_GUI_PDF_FILES_H

#include <QCoreApplication>
#include <QImage>
#include <QSize>
#include <QString>

#include <initializer_list>
#include <optional>

class QWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

class PreviewPanel;

/// @brief What a PDF file says of itself (PdfDocument::metaData()), for "Fill In from File"
struct PdfMetaData {
    QString title;    ///< Its title; empty: none
    QString author;   ///< Its author or authors, as the file has them; empty: none
    QString subject;  ///< Its subject; empty: none
    QString year;     ///< The year it was made; empty: none
    int pages = 0;    ///< Its number of pages
};

/// @brief PDF files: whether the program shows them, their first page and their data, and the
/// settings of their preview (Settings > Materials)
///
/// The program shows a PDF only when it was built with its preview (PDFium,
/// previewAvailable()); without it a PDF opens in the system program and the commands of the
/// preview are greyed out.
class PdfFiles {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::PdfFiles)

public:
    /// @brief How a PDF opens (Settings > Materials > Opening): PREVIEW (at first) or SYSTEM
    static constexpr const char* OPEN_SETTING = "materials.open.pdf";
    static constexpr const char* PREVIEW = "preview";  ///< On a preview tab of the program
    static constexpr const char* SYSTEM = "system";    ///< In the system program

    /// @brief How the preview lays out the pages: CONTINUOUS (at first), SINGLE or TWO
    static constexpr const char* LAYOUT_SETTING = "materials.pdf.layout";
    static constexpr const char* CONTINUOUS = "continuous";  ///< One under another
    static constexpr const char* SINGLE = "single";          ///< One page at a time
    static constexpr const char* TWO = "two";                ///< Two side by side

    /// @brief How the pages look in a dark theme: AS_FILE (at first), DIMMED or INVERTED
    static constexpr const char* DARK_SETTING = "materials.pdf.dark";
    static constexpr const char* AS_FILE = "file";     ///< As in the file
    static constexpr const char* DIMMED = "dim";       ///< Darker
    static constexpr const char* INVERTED = "invert";  ///< Light text on a dark page

    /// @brief The zoom a preview opens with: PAGE_WIDTH (at first), WHOLE_PAGE, ACTUAL or LAST
    static constexpr const char* ZOOM_SETTING = "materials.pdf.zoom";
    static constexpr const char* PAGE_WIDTH = "pageWidth";  ///< The width of the page
    static constexpr const char* WHOLE_PAGE = "wholePage";  ///< The whole page
    static constexpr const char* ACTUAL = "actual";         ///< 100%
    static constexpr const char* LAST = "last";             ///< As shown last (LAST_ZOOM_SETTING)
    /// @brief The zoom shown last, for LAST: PAGE_WIDTH, WHOLE_PAGE or a number ("1.5" is
    /// 150%)
    static constexpr const char* LAST_ZOOM_SETTING = "materials.pdf.lastZoom";

    /// @brief What the side panel of a preview shows first: BOOKMARKS (at first: the
    /// bookmarks, the contents of the file without them), CONTENTS, THUMBNAILS or HIDDEN
    static constexpr const char* SIDE_SETTING = "materials.pdf.side";
    static constexpr const char* BOOKMARKS = "bookmarks";    ///< The bookmarks
    static constexpr const char* CONTENTS = "contents";      ///< The contents of the file
    static constexpr const char* THUMBNAILS = "thumbnails";  ///< The pages small
    static constexpr const char* HIDDEN = "hidden";          ///< No side panel

    /// @brief Whether a preview opens at the page shown last (a bool, true at first)
    static constexpr const char* LAST_PAGE_SETTING = "materials.pdf.openAtLastPage";

    /// @brief Whether text on the pages can be highlighted, the highlights kept in the book (a
    /// bool, false at first): the PDF file stays as it is
    static constexpr const char* HIGHLIGHTS_SETTING = "materials.pdf.highlights";

    /// @brief Whether the program shows PDF files: it was built with their preview (PDFium)
    [[nodiscard]] static bool previewAvailable();

    /// @brief Whether @p path is a PDF file, by its suffix or its content type
    [[nodiscard]] static bool isPdf(const QString& path);

    /// @brief Whether a PDF opens on a preview tab: the program shows PDFs and the settings
    /// say so
    [[nodiscard]] static bool opensInPreview();

    /// @brief Whether the preview highlights text: the settings say so (HIGHLIGHTS_SETTING)
    [[nodiscard]] static bool highlightsOn();

    /// @brief The value of setting @p key of the preview; @p fallback when it is not one of
    /// @p values
    [[nodiscard]] static QString choice(const char* key, const char* fallback,
                                        std::initializer_list<const char*> values);

    /// @brief The first page of PDF @p path, at most @p bounds large; a null image without
    /// the preview or when the file cannot be read, and then why in @p error
    [[nodiscard]] static QImage firstPage(const QString& path, const QSize& bounds,
                                          QString* error = nullptr);

    /// @brief How many pages PDF @p path has; 0 without the preview or when the file cannot
    /// be read, and then why in @p error
    [[nodiscard]] static int pageCount(const QString& path, QString* error = nullptr);

    /// @brief What PDF @p path says of itself; none without the preview or when the file
    /// cannot be read
    [[nodiscard]] static std::optional<PdfMetaData> metaData(const QString& path);

    /// @brief Why a PDF file did not load, in a sentence
    /// @param error The error of the load (a PdfDocument::Error)
    [[nodiscard]] static QString loadProblem(int error);

    /// @brief A new preview of the PDF file of material @p elementId of @p book (a
    /// PdfPreviewPanel); nullptr without the preview
    [[nodiscard]] static PreviewPanel* newPreview(core::ProjectManager& book,
                                                  const QString& elementId, QWidget* parent);
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PDF_FILES_H
