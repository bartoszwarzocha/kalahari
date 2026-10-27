/// @file text_tables.h
/// @brief Tables in the text: a table of the document, its cells made of paragraphs
///
/// A table is a QTextTable in the editor's document. Each cell holds one or more paragraphs
/// (blocks), which are in the document's order of blocks row by row, each row from left to
/// right, so a place in a cell is a paragraph and an offset like any other. A cell may span
/// more than one row or column. The table's format keeps the widths of its columns (percent
/// of the width of the text) and the number of its header rows; Qt's borders, padding and
/// spacing are not used, the editor lays out and draws the table itself.
///
/// Qt keeps a paragraph before a table at the start of the document and one after a table
/// at its end: a table is never the first or the last block of a chapter.
///
/// KML writes a table as <table> with <col width="...%"/>, <tr>, and <th> or <td> cells
/// with their paragraphs (docs/kml_format.md).

#pragma once

#include <utility>
#include <vector>

class QTextBlock;
class QTextCursor;
class QTextDocument;
class QTextTable;
class QTextTableFormat;

namespace kalahari::editor {

/// @brief The most rows or columns one cell of KML may span
inline constexpr int MAX_TABLE_SPAN = 100;

/// @brief A cell that spans more than one row or column
struct TableSpan {
    int row = 0;      ///< The row of its top left corner
    int column = 0;   ///< The column of its top left corner
    int rows = 1;     ///< The rows it spans
    int columns = 1;  ///< The columns it spans

    bool operator==(const TableSpan&) const = default;
};

/// @brief The shape of a table: its rows, columns, their widths, header and spans
struct TableShape {
    int rows = 0;
    int columns = 0;
    std::vector<double> columnWidths;  ///< Percent of the table's width, one per column (a
                                       ///< hundred together); empty: equal columns
    int headerRows = 0;                ///< The rows of header cells at its top
    std::vector<TableSpan> spans;      ///< The cells that span more than one row or column

    bool operator==(const TableShape&) const = default;
};

/// @brief The cell a paragraph of KML is in: its table (the tables numbered from 0 in the
///        order of the KML), its row and its column
struct TableCellPlace {
    int table = 0;
    int row = 0;
    int column = 0;

    bool operator==(const TableCellPlace&) const = default;
};

/// @brief The table holding the paragraph; nullptr for a paragraph outside a table
QTextTable* tableOf(const QTextBlock& block);

/// @brief The table holding the position; nullptr for a position outside a table
QTextTable* tableAt(const QTextDocument& document, int position);

/// @brief The format of a table of the shape: its columns, their widths and its header
///        rows, without Qt's borders, padding, spacing and margins
QTextTableFormat tableFormatOf(const TableShape& shape);

/// @brief The shape of a table of the document
TableShape tableShapeOf(const QTextTable& table);

/// @brief Insert a table of the shape at the cursor, its cells empty and not spanned yet
///
/// The table splits the paragraph at the cursor: its text before the cursor stays before the
/// table, the rest goes after it, both parts with the paragraph's formatting (at its start,
/// an empty paragraph stays before the table). The cursor ends at the start of the first
/// cell.
/// @return The table; nullptr for a shape without rows or columns
QTextTable* insertTable(QTextCursor& cursor, const TableShape& shape);

/// @brief Whether the character at the position is a break at a table: the start of one of
///        its cells (the first one's is the end of the paragraph before the table) or its end
bool isTableBreakAt(const QTextDocument& document, int position);

/// @brief Whether removeBlock() takes the block whole: one of its paragraph breaks is not a
///        break at a table (the only paragraph of a cell, or between a table and the start or
///        end of the text or another table, stays)
bool canRemoveBlock(const QTextBlock& block);

/// @brief The parts of positions [from, to) of the document, in its order, that do not cross
///        a break at a table: the text of each cell the range goes into, and the text outside
///        tables, with a table the range covers whole
std::vector<std::pair<int, int>> partsAtTables(const QTextDocument& document, int from, int to);

/// @brief Remove positions [from, to) of the document as removeKeepingAnnotations() does
///        (with @p keepAnnotations false: removeSelectionKeepingObjects()), but not the
///        structure of a table the range goes into or out of
///
/// Of a cell the range goes into its text in the range goes, the cell stays; a table it
/// covers whole goes. One edit block; the cursor ends at @p from (as the removal leaves it:
/// after an object, at the text after it), without a selection.
void removeKeepingTables(QTextCursor& cursor, int from, int to, bool keepAnnotations = true);

/// @brief Make the cells of the spans span their rows and columns
///
/// Qt keeps a cell's span in the character format of its first paragraph, which
/// QTextCursor::setBlockCharFormat() replaces: the cells are spanned once their paragraphs
/// have their formats. The cells a span covers must be empty.
void spanCells(QTextTable& table, const std::vector<TableSpan>& spans);

}  // namespace kalahari::editor
