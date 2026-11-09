/// @file text_tables.h
/// @brief Tables in the text: a table of the document, its cells made of paragraphs
///
/// A table is a QTextTable in the editor's document. Each cell holds one or more paragraphs
/// (blocks), which are in the document's order of blocks row by row, each row from left to
/// right, so a place in a cell is a paragraph and an offset like any other. A cell may span
/// more than one row or column. The table's format keeps the widths of its columns (percent
/// of the width of the table), the number of its header rows (repeated at the top of each
/// page the table goes on to), its width, alignment and indent, and its borders; Qt's
/// borders, padding and spacing are not used, the editor lays out and draws the table
/// itself.
///
/// Qt keeps no format of a row: the properties of a row (its least height, whether it may
/// be split between pages) are kept by each cell starting in it, with the cell's own (its
/// vertical alignment and background), in the format of the cell. Qt keeps that in the
/// character format of the cell's first paragraph (QTextBlock::charFormat()), which
/// QTextCursor::setBlockCharFormat() replaces: the properties are merged into it.
///
/// Qt keeps a paragraph before a table at the start of the document and one after a table
/// at its end: a table is never the first or the last block of a chapter.
///
/// KML writes a table as <table> with <col width="...%"/>, <tr>, and <th> or <td> cells
/// with their paragraphs (docs/kml_format.md).

#pragma once

#include <QColor>
#include <QString>
#include <QTextFormat>

#include <optional>
#include <utility>
#include <vector>

class QTextBlock;
class QTextCharFormat;
class QTextCursor;
class QTextDocument;
class QTextTable;
class QTextTableCell;
class QTextTableFormat;
class QXmlStreamAttributes;

namespace kalahari::editor {

/// @brief The most rows or columns one cell of KML may span
inline constexpr int MAX_TABLE_SPAN = 100;

// =============================================================================
// Properties of the table's format
// =============================================================================

/// @brief The indent of a table aligned to the left, from the left edge of the text, in
///        points (double); the width of a table of its own is QTextFrameFormat::width(), a
///        fixed length in points (a percentage: as wide as the text)
inline constexpr int TABLE_INDENT_PROPERTY = QTextFormat::UserProperty + 160;

/// @brief The lines of the borders (int, TableBorderLine flags); without it: all of them
inline constexpr int TABLE_BORDER_LINES_PROPERTY = QTextFormat::UserProperty + 161;

/// @brief The style of the borders' lines (int, BorderStyle)
inline constexpr int TABLE_BORDER_STYLE_PROPERTY = QTextFormat::UserProperty + 162;

/// @brief The width of the borders' lines, in points (double)
inline constexpr int TABLE_BORDER_WIDTH_PROPERTY = QTextFormat::UserProperty + 163;

/// @brief The color of the borders (QColor); without it, or not valid: the color of the
///        tables' lines in the settings (Settings > Editor > Colors)
inline constexpr int TABLE_BORDER_COLOR_PROPERTY = QTextFormat::UserProperty + 164;

// =============================================================================
// Properties of a cell's format
// =============================================================================

/// @brief The least height of the cell's row, in points (double); 0 or without it: as high
///        as its text
inline constexpr int ROW_MIN_HEIGHT_PROPERTY = QTextFormat::UserProperty + 165;

/// @brief Whether the cell's row may be split between pages (bool); without it: not, the
///        row goes on to the next page whole (a row higher than a page is split anyway)
inline constexpr int ROW_SPLIT_PROPERTY = QTextFormat::UserProperty + 166;

/// @brief The vertical alignment of the cell's text (int, CellAlignment)
inline constexpr int CELL_ALIGNMENT_PROPERTY = QTextFormat::UserProperty + 167;

/// @brief The background of the cell (QColor); not valid: none
inline constexpr int CELL_BACKGROUND_PROPERTY = QTextFormat::UserProperty + 168;

/// @brief The width of the borders' lines of a new table, in points
inline constexpr double DEFAULT_BORDER_WIDTH = 0.5;

/// @brief The widest lines of the borders, in points
inline constexpr double MAX_BORDER_WIDTH = 6.0;

/// @brief The greatest least height of a row, in points (22 inches)
inline constexpr double MAX_ROW_HEIGHT = 1584.0;

/// @brief Where a table is between the edges of the text
enum class TableAlignment { Left, Center, Right };

/// @brief Where the text of a cell is between its top and bottom
enum class CellAlignment { Top, Middle, Bottom };

/// @brief How the lines of the borders are drawn
enum class BorderStyle { Solid, Dashed, Dotted, Double };

/// @brief The lines of a table's borders: the four edges of the table, and the lines
///        between its rows and between its columns
enum TableBorderLine : int {
    BORDER_TOP = 1,
    BORDER_BOTTOM = 2,
    BORDER_LEFT = 4,
    BORDER_RIGHT = 8,
    BORDER_ROWS = 16,     ///< Between the rows
    BORDER_COLUMNS = 32,  ///< Between the columns
};

/// @brief No lines, the edges of the table (Box), and all lines (Grid)
inline constexpr int BORDER_NONE = 0;
inline constexpr int BORDER_BOX = BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT;
inline constexpr int BORDER_GRID = BORDER_BOX | BORDER_ROWS | BORDER_COLUMNS;

/// @brief The borders of a table
struct TableBorders {
    int lines = BORDER_GRID;                     ///< TableBorderLine flags
    BorderStyle style = BorderStyle::Solid;
    double width = DEFAULT_BORDER_WIDTH;         ///< Points
    QColor color;                                ///< Not valid: automatic (the settings')

    bool operator==(const TableBorders&) const = default;
};

/// @brief The properties of a table (Table > Table Properties: Table, Borders)
struct TableProperties {
    std::optional<double> width;                 ///< Points; nothing: as wide as the text
    TableAlignment alignment = TableAlignment::Left;
    double indent = 0.0;                         ///< Points, of a table aligned to the left
    TableBorders borders;

    bool operator==(const TableProperties&) const = default;
};

/// @brief The properties of a row of a table (Table Properties: Row)
struct TableRowProperties {
    double minHeight = 0.0;                      ///< Points; 0: as high as its text
    bool allowSplit = false;                     ///< May be split between pages

    bool operator==(const TableRowProperties&) const = default;
};

/// @brief The properties of a cell of a table (Table Properties: Cell)
struct TableCellProperties {
    CellAlignment alignment = CellAlignment::Top;
    QColor background;                           ///< Not valid: none

    bool operator==(const TableCellProperties&) const = default;
};

/// @brief A row whose properties are not the default ones
struct TableRowFormat {
    int row = 0;
    TableRowProperties properties;

    bool operator==(const TableRowFormat&) const = default;
};

/// @brief A cell whose properties are not the default ones, by its top left corner
struct TableCellFormat {
    int row = 0;
    int column = 0;
    TableCellProperties properties;

    bool operator==(const TableCellFormat&) const = default;
};

/// @brief A cell that spans more than one row or column
struct TableSpan {
    int row = 0;      ///< The row of its top left corner
    int column = 0;   ///< The column of its top left corner
    int rows = 1;     ///< The rows it spans
    int columns = 1;  ///< The columns it spans

    bool operator==(const TableSpan&) const = default;
};

/// @brief The shape of a table: its rows, columns, their widths, header and spans, and its
///        properties and those of its rows and cells
struct TableShape {
    int rows = 0;
    int columns = 0;
    std::vector<double> columnWidths;  ///< Percent of the table's width, one per column (a
                                       ///< hundred together); empty: equal columns
    int headerRows = 0;                ///< The rows of header cells at its top
    std::vector<TableSpan> spans;      ///< The cells that span more than one row or column
    TableProperties properties;
    std::vector<TableRowFormat> rowFormats;    ///< The rows of other than default properties
    std::vector<TableCellFormat> cellFormats;  ///< The cells of other than default properties

    bool operator==(const TableShape&) const = default;
};

// =============================================================================
// The properties in KML
// =============================================================================

/// @brief An attribute of KML: its name and its value
using KmlAttribute = std::pair<QString, QString>;

/// @brief A length of KML in points: "12.5pt" (the number rounded to two decimals)
QString kmlLength(double points);

/// @brief The points of a length of KML: a number with a unit, pt, cm, mm, in, px (a 96th of
///        an inch) or pc, or without one (points); std::nullopt for anything else, or below 0
std::optional<double> pointsOfKmlLength(const QString& length);

/// @brief The attributes of <table> for its properties, the default ones left out: width,
///        align, indent, borders ("none", "box", or the lines: "top bottom left right rows
///        columns"), border-style, border-width and border-color
std::vector<KmlAttribute> tableKmlAttributes(const TableProperties& properties);

/// @brief The properties of a table from the attributes of its <table> (the default ones
///        for those it has not, or that are not known)
TableProperties tablePropertiesOfKml(const QXmlStreamAttributes& attributes);

/// @brief The attributes of <tr> for the properties of a row: height and split="true"
std::vector<KmlAttribute> rowKmlAttributes(const TableRowProperties& properties);

/// @brief The properties of a row from the attributes of its <tr>
TableRowProperties rowPropertiesOfKml(const QXmlStreamAttributes& attributes);

/// @brief The attributes of <td> or <th> for the properties of a cell: valign ("middle",
///        "bottom") and background
std::vector<KmlAttribute> cellKmlAttributes(const TableCellProperties& properties);

/// @brief The properties of a cell from the attributes of its <td> or <th>
TableCellProperties cellPropertiesOfKml(const QXmlStreamAttributes& attributes);

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

/// @brief The format of a table of the shape: its columns, their widths, its header rows
///        and its properties, without Qt's borders, padding, spacing and margins
QTextTableFormat tableFormatOf(const TableShape& shape);

/// @brief The properties of a table's format
TableProperties tablePropertiesOf(const QTextTableFormat& format);

/// @brief Give a table's format the properties (the default ones are written too, so the
///        format can be merged)
void setTableProperties(QTextTableFormat& format, const TableProperties& properties);

/// @brief The properties of a row: those of its first cell starting in it (the default ones
///        for a row covered whole by cells spanning from above)
TableRowProperties rowPropertiesOf(const QTextTable& table, int row);

/// @brief The properties of a cell
TableCellProperties cellPropertiesOf(const QTextTableCell& cell);

/// @brief Give the cells starting in a row the row's properties, merged into their formats
void setRowProperties(QTextCursor& cursor, const QTextTable& table, int row,
                      const TableRowProperties& properties);

/// @brief Give a cell its properties, merged into its format
void setCellProperties(QTextCursor& cursor, const QTextTableCell& cell,
                       const TableCellProperties& properties);

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

/// @brief The character format of a paragraph, without that of the cell of a table it starts
///
/// Qt keeps the format of a cell, with its span, in the character format of the cell's first
/// paragraph (QTextBlock::charFormat()): given to a paragraph of another cell, it would make
/// that cell span without its table knowing.
QTextCharFormat paragraphCharFormat(const QTextBlock& block);

/// @brief Whether the property is one of the format of a cell: its span, or a property of
///        its row or its own
bool isCellProperty(int property);

/// @brief Give the paragraph at the cursor the character format (without that of a cell),
///        keeping the format of the cell it starts
void setParagraphCharFormat(QTextCursor& cursor, const QTextCharFormat& format);

/// @brief Make the cells of the spans span their rows and columns
///
/// Qt keeps a cell's span in the character format of its first paragraph, which
/// QTextCursor::setBlockCharFormat() replaces: the cells are spanned once their paragraphs
/// have their formats. The cells a span covers must be empty.
void spanCells(QTextTable& table, const std::vector<TableSpan>& spans);

/// @brief Give a table filled with its paragraphs the spans, and the properties of the rows
///        and cells, of its shape (as spanCells(), once the paragraphs have their formats)
void finishTableCells(QTextTable& table, const TableShape& shape);

/// @brief A rectangle of cells of a table: its first and last row and column
struct TableCellRange {
    QTextTable* table = nullptr;
    int firstRow = 0;
    int lastRow = 0;
    int firstColumn = 0;
    int lastColumn = 0;

    int rows() const { return lastRow - firstRow + 1; }
    int columns() const { return lastColumn - firstColumn + 1; }
    bool operator==(const TableCellRange&) const = default;
};

/// @brief The cells a selection from @p anchor to @p position selects: when both ends are
///        in cells of the same table, but not in one cell, the rectangle of the cells
///        between them, with every cell spanning into it whole
std::optional<TableCellRange> selectedCells(const QTextDocument& document, int anchor,
                                            int position);

/// @brief The rectangle of the cells, grown until no merged cell crosses its edges
TableCellRange wholeCells(const TableCellRange& range);

}  // namespace kalahari::editor
