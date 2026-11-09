/// @file table_editing.h
/// @brief Changes of tables (the Table menu): rows and columns, merging and splitting cells,
///        the widths of columns, the header rows, moving and sorting rows, and turning
///        paragraphs into a table and a table into paragraphs
///
/// Each function changes the document through the cursor; the caller makes one edit block
/// (one undo step) of it. The text moves with its formats: the paragraphs' styles and their
/// own formatting, the formatting of the characters, links and the anchors of annotations.

#pragma once

#include <kalahari/editor/text_tables.h>

#include <QString>
#include <QTextBlockFormat>
#include <QTextCharFormat>

#include <vector>

class QTextCursor;
class QTextDocument;
class QTextTable;
class QTextTableCell;

namespace kalahari::editor {

/// @brief The most cells Table > Split Cells makes of one, in each direction
inline constexpr int MAX_SPLIT_CELLS = 20;

/// @brief The narrowest column the editor makes, in points (half a centimetre)
inline constexpr double MIN_COLUMN_WIDTH = 72.0 / 2.54 / 2.0;

/// @brief What Table > Table Properties... shows and changes: the properties of a table, of
///        its rows and columns, and of the cells it was opened for
struct TablePropertiesData {
    TableProperties table;
    double textWidth = 0.0;                  ///< Points: the widest a table is
    std::vector<double> columnWidths;        ///< Points, a column each, the table's width
                                             ///< together
    std::vector<TableRowProperties> rows;    ///< A row each
    int headerRows = 0;
    int row = 0;                             ///< The row shown first: the cursor's, or the
                                             ///< first selected one
    int column = 0;                          ///< The column shown first, as the row
    TableCellProperties cell;                ///< Of the cursor's cell, or the first selected
    bool cellChanged = false;                ///< The cells get @c cell (set by the window)

    bool operator==(const TablePropertiesData&) const = default;
};

// =============================================================================
// Paragraphs copied with their formats
// =============================================================================

/// @brief A run of text of one character format
struct CopiedRun {
    QString text;
    QTextCharFormat format;
};

/// @brief A paragraph copied with its formats: its block format, its character format
///        (without that of a cell it starts) and its runs of text
struct CopiedParagraph {
    QTextBlockFormat blockFormat;
    QTextCharFormat charFormat;
    std::vector<CopiedRun> runs;

    /// @brief Its text
    QString text() const;
};

/// @brief The paragraphs of positions [from, to) of the document: their parts in the range
std::vector<CopiedParagraph> copyParagraphs(const QTextDocument& document, int from, int to);

/// @brief Insert the paragraphs at the cursor: the text of the first one at the cursor,
///        each next one in a paragraph of its own after it
///
/// The paragraph at the cursor takes the formats of the first one when it is empty, and an
/// inserted paragraph keeps its own; the text after the cursor stays in the last one. Of a
/// cell's first paragraph the format of the cell stays. The cursor ends after the text.
void insertParagraphs(QTextCursor& cursor, const std::vector<CopiedParagraph>& paragraphs);

/// @brief The paragraphs of a cell
std::vector<CopiedParagraph> cellParagraphs(const QTextTableCell& cell);

/// @brief The cells starting in a rectangle of cells, row by row (in the order of the text)
std::vector<QTextTableCell> startingCellsOf(const TableCellRange& range);

/// @brief Whether a cell has no text: one empty paragraph
bool isCellEmpty(const QTextTableCell& cell);

/// @brief Remove the text of a cell: one empty paragraph in the format of its first stays
/// @param keepAnnotations The annotations of the text stay on its place (Delete); false:
///        they go with it (to the clipboard, or into another cell)
void clearCell(QTextCursor& cursor, const QTextTableCell& cell, bool keepAnnotations = false);

/// @brief Give a cell the paragraphs in place of its own (none: the cell is emptied)
void setCellParagraphs(QTextCursor& cursor, const QTextTableCell& cell,
                       const std::vector<CopiedParagraph>& paragraphs);

// =============================================================================
// Columns and header
// =============================================================================

/// @brief The widths of a table's columns in percent of its width, a hundred together
///        (equal columns for a table without widths)
std::vector<double> columnWidthsOf(const QTextTable& table);

/// @brief Give a table's columns the widths, in proportion to one another (a hundred
///        percent together, rounded to two decimals)
void setColumnWidths(QTextTable& table, const std::vector<double>& widths);

/// @brief Make the columns from @p first to @p last equally wide, together as wide as before
///        (all of them: a table without widths)
void distributeColumns(QTextTable& table, int first, int last);

/// @brief Make the first @p rows rows the header rows (none: 0)
void setHeaderRows(QTextTable& table, int rows);

// =============================================================================
// Rows and columns
// =============================================================================

/// @brief Insert @p count rows above or below the row, of the cells of that row: their
///        properties and those of the row, a cell spanning the place grows instead; the new
///        cells hold an empty paragraph of @p paragraphFormat. Rows inserted next to a header
///        row are header rows too.
void insertTableRows(QTextCursor& cursor, QTextTable& table, int row, int count, bool below,
                     const QTextBlockFormat& paragraphFormat);

/// @brief Insert @p count columns to the left or the right of the column, as wide as it is
///        (the others narrower: the table keeps its width), their cells of the properties of
///        the column's cells, a cell spanning the place grows instead
void insertTableColumns(QTextCursor& cursor, QTextTable& table, int column, int count,
                        bool right, const QTextBlockFormat& paragraphFormat);

/// @brief Remove the rows, a cell spanning into them gets shorter; all rows: the table
/// @return The place for the cursor after: the cell below in the column of the first
///         removed row's first cell, or above, or the paragraph after the table
int removeTableRows(QTextCursor& cursor, QTextTable& table, int first, int count);

/// @brief Remove the columns, the others wider (the table keeps its width); all: the table
/// @return The place for the cursor after, as removeTableRows() gives it
int removeTableColumns(QTextCursor& cursor, QTextTable& table, int first, int count);

/// @brief Remove the table; the paragraphs before and after it stay, but an empty one Qt
///        kept only for the table at the start or the end of the text
/// @return The place for the cursor after: the start of the paragraph after the table
int removeWholeTable(QTextCursor& cursor, QTextTable& table);

// =============================================================================
// Merging and splitting
// =============================================================================

/// @brief Merge the cells of the range into its first: the paragraphs of each of them go
///        into it in turn (empty cells add none), Qt's merging joining them by spaces
void mergeTableCells(QTextCursor& cursor, const TableCellRange& range);

/// @brief Split the cell at the row and column into @p rows by @p columns cells (1 to
///        MAX_SPLIT_CELLS); the new columns divide the cell's width equally, the other cells
///        of its columns and rows span the new ones, so the rest of the table looks as before
///
/// A merged cell splits along the edges of its rows and columns where they are the new
/// cells' edges. With @p distribute the paragraphs of the cell go into the new cells in
/// turn, row by row, the rest into the last one; otherwise all of them stay in the first.
/// The new cells hold an empty paragraph of @p paragraphFormat.
void splitTableCell(QTextCursor& cursor, QTextTable& table, int row, int column, int rows,
                    int columns, bool distribute, const QTextBlockFormat& paragraphFormat);

// =============================================================================
// Moving and sorting rows
// =============================================================================

/// @brief Whether a cell spanning rows has a part in the rows from @p first to @p last:
///        those rows cannot be moved nor sorted
bool hasRowSpans(const QTextTable& table, int first, int last);

/// @brief Put the rows from @p first on in the order: the i-th of them gets the row
///        first + order[i], with its cells' paragraphs, their properties and spans, and the
///        properties of the row (the rows have no cells spanning rows, hasRowSpans())
void reorderTableRows(QTextCursor& cursor, QTextTable& table, int first,
                      const std::vector<int>& order);

/// @brief Put the paragraphs from block @p first on in the order: the i-th of them gets the
///        paragraph first + order[i] (paragraphs outside tables, without objects)
void reorderParagraphs(QTextCursor& cursor, int first, const std::vector<int>& order);

/// @brief Move the paragraphs from block @p first to @p last one place up or down: past the
///        paragraph or object next to them, or a whole table
/// @return The number of the block the first of them is in after; -1: they stay (the
///         start or the end of the text)
int moveParagraphs(QTextCursor& cursor, int first, int last, bool down);

// =============================================================================
// Text to table and back
// =============================================================================

/// @brief The cells a paragraph's text is divided into at the separator
QStringList cellTexts(const QString& paragraph, QChar separator);

/// @brief The separator most of the paragraphs have: a tab, a semicolon or a comma (the
///        first of them that is in the most paragraphs); a null QChar: none of them
QChar likeliestSeparator(const QStringList& paragraphs);

/// @brief Turn the paragraphs of blocks @p first to @p last into a table: each a row, its
///        text divided into cells at the separator (the spaces next to a separator other
///        than a tab going); a null @p separator: each paragraph a cell, row by row in
///        @p columns columns
///
/// The table is as wide as the text with equal columns; the cells keep the paragraphs'
/// formats. An empty paragraph left next to it that it does not need goes, as for Insert >
/// Table.
/// @return The table; nullptr when there are no paragraphs
QTextTable* textToTable(QTextCursor& cursor, int first, int last, QChar separator, int columns,
                        bool headerRow, const QTextBlockFormat& paragraphFormat);

/// @brief Turn a table into paragraphs: each row one, its cells separated by the separator
///        (a semicolon and a comma with a space after them) and a cell's paragraphs joined
///        by spaces; a null @p separator: each cell's paragraphs as they are, row by row
/// @return The number of the block of the first paragraph
int tableToText(QTextCursor& cursor, QTextTable& table, QChar separator);

}  // namespace kalahari::editor
