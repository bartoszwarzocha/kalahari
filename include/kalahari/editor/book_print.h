// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file book_print.h
/// @brief The print of a book: which of its pages go to the printer, how they go on the
///        sheets of paper (a page, two side by side, a booklet), and the sides of the sheets
///        drawn on a printer, a PDF writer or a preview
///
/// The pages are those of BookPages, numbered from 0; they show numbers one higher. A sheet
/// has two sides; one-sided printing uses only the fronts. Two pages side by side and a
/// booklet lie on a sheet turned sideways, with the spine of the pages in its middle: a
/// booklet's stack of sheets, folded in half, reads in order.

#pragma once

#include <QRectF>
#include <QSizeF>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

class QPagedPaintDevice;
class QPainter;

namespace kalahari::editor {

class BookPages;

/// @brief How the pages of the book go on a side of a sheet
enum class SheetLayout {
    OnePage,   ///< A page on a side
    TwoPages,  ///< Two pages side by side, on a sheet turned sideways
    Booklet,   ///< Two pages side by side, in the order of a stack folded in half
};

/// @brief Which of the chosen pages go to the printer, by the numbers they show
enum class PageParity {
    All,   ///< Every page
    Odd,   ///< The pages with odd numbers: 1, 3, 5... (the right pages)
    Even,  ///< The pages with even numbers: 2, 4, 6... (the left pages)
};

/// @brief A side of a sheet: its pages from the left, from 0; -1 a page left empty (of a
///        booklet whose pages are not a multiple of four, or of two pages side by side); no
///        pages: the empty back of a sheet
using SheetSide = std::vector<int>;

/// @brief How the pages lie on a side
struct SheetOptions {
    SheetLayout layout = SheetLayout::OnePage;
    /// One page: as large as the printable area allows; false: its own size in the middle of
    /// the sheet (a page smaller than the paper, A5 on A4)
    bool fit = true;
    /// One page of its own size: the marks of its corners, where the sheet is cut
    bool cropMarks = false;
};

/// @brief Whether the sheet lies sideways (landscape): two pages side by side and a booklet,
///        and one page wider than high
bool sheetSideways(const SheetOptions& options, const QSizeF& pageSize);

/// @brief The pages of a range typed as the pages show their numbers: "3-5, 9", "3–5; 9"
/// @param text The numbers and the ranges, separated by commas, semicolons or spaces; a
///        hyphen or an en dash between the first and the last page of a range
/// @param count The pages of the book
/// @return The pages from 0 within the book, in order, each once (empty: none of them is in
///         the book); nullopt when the text is not a range (also an empty text)
std::optional<std::vector<int>> pagesOfRange(const QString& text, int count);

/// @brief The pages of @p pages (from 0) whose numbers have @p parity
std::vector<int> pagesOfParity(const std::vector<int>& pages, PageParity parity);

/// @brief The sides of the sheets that print @p pages, in the order of the print: a front,
///        then its back (with two-sided printing)
/// @param pages The pages, from 0, in their order
/// @param layout A booklet has as many sides as four pages make, its last pages empty
std::vector<SheetSide> sheetSides(const std::vector<int>& pages, SheetLayout layout);

/// @brief The sheets of @p sides: with two-sided printing two sides a sheet
int sheetCount(int sides, bool twoSided);

/// @brief The sides of @p copies copies of @p sides, in the order of the print
/// @param collate Each copy whole before the next; false: the copies of each sheet together
/// @param twoSided A sheet has a front and a back: of more copies, a copy of an odd number
///        of sides ends with an empty back, so the next copy starts on a sheet of its own
std::vector<SheetSide> copiesOf(const std::vector<SheetSide>& sides, int copies, bool collate,
                                bool twoSided);

/// @brief The sides of @p sides with their sheets in the reverse order, the last sheet first
///        (for a printer that lays the sheets face up); a sheet keeps its front and its back
/// @param twoSided A sheet has a front and a back: of an odd number of sides the last sheet
///        has an empty back, and it prints first
std::vector<SheetSide> reversedSheets(const std::vector<SheetSide>& sides, bool twoSided);

/// @brief The fronts (@p backs false) or the backs of the sheets of @p sides, two-sided, in
///        the order of the print: for two-sided printing by hand, the fronts first, then the
///        backs on the turned stack
std::vector<SheetSide> sidesOfSheets(const std::vector<SheetSide>& sides, bool backs);

/// @brief Where the pages of a side go: a rectangle for each page of the side, in points from
///        the top left of the sheet
/// @param pageSize The size of a page of the book, in points
/// @param paper The size of the sheet as it lies (sideways for two pages), in points
/// @param printable Where the printer prints on it, in points from its top left
std::vector<QRectF> pageRects(const QSizeF& pageSize, int pagesOnSide, const QSizeF& paper,
                              const QRectF& printable, const SheetOptions& options);

/// @brief Draw a side of a sheet: its pages, and the crop marks
/// @param painter Its origin at the top left of the sheet, its units points
void paintSheetSide(QPainter* painter, const BookPages& pages, const SheetSide& side,
                    const QSizeF& paper, const QRectF& printable, const SheetOptions& options);

/// @brief Print @p sides on @p device, a side a page of the device
/// @param device A printer or a PDF writer, its page layout set: the paper, which way it lies,
///        the whole page (QPageLayout::FullPageMode); its minimum margins say where it prints
/// @param progress Told the sides printed so far; false stops the print
/// @return false when the device cannot be drawn on, or when @p progress stopped the print
bool printSheetSides(QPagedPaintDevice* device, const BookPages& pages,
                     const std::vector<SheetSide>& sides, const SheetOptions& options,
                     const std::function<bool(int printed)>& progress = {});

}  // namespace kalahari::editor
