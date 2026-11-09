/// @file table_editing.cpp
/// @brief Changes of tables (the Table menu): rows, columns, cells, widths, moving, sorting
///        and turning text into a table and back

#include "kalahari/editor/table_editing.h"
#include "kalahari/editor/annotation.h"
#include "kalahari/editor/paragraph_styles.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>

namespace kalahari::editor {

namespace {

/// Widths in percent closer than this are the same edge
constexpr double WIDTH_EPSILON = 0.01;

/// The cells starting in a row, from left to right
std::vector<QTextTableCell> startingCells(const QTextTable& table, int row) {
    std::vector<QTextTableCell> cells;
    for (int column = 0; column < table.columns(); ++column) {
        const QTextTableCell cell = table.cellAt(row, column);
        if (cell.row() == row && cell.column() == column) {
            cells.push_back(cell);
        }
    }
    return cells;
}

/// Whether the cell is one of its own at the row and column (not a part of a merged one)
bool startsAt(const QTextTableCell& cell, int row, int column) {
    return cell.isValid() && cell.row() == row && cell.column() == column;
}

/// Give a new cell an empty paragraph of the format, without the formatting of the cell it
/// was copied from, and the properties
void prepareNewCell(QTextCursor& cursor, const QTextTableCell& cell,
                    const QTextBlockFormat& paragraphFormat,
                    const TableCellProperties& properties) {
    cursor.setPosition(cell.firstPosition());
    cursor.setBlockFormat(paragraphFormat);
    setParagraphCharFormat(cursor, QTextCharFormat());
    setCellProperties(cursor, cell, properties);
}

/// Append the paragraphs after the cursor's paragraph, each a new one
void appendParagraphs(QTextCursor& cursor, const std::vector<CopiedParagraph>& paragraphs) {
    for (const CopiedParagraph& paragraph : paragraphs) {
        cursor.insertBlock(paragraph.blockFormat, paragraph.charFormat);
        for (const CopiedRun& run : paragraph.runs) {
            cursor.insertText(run.text, run.format);
        }
    }
}

/// Insert the paragraphs before the block, each a new one; the block keeps its text and
/// formats after them
void insertParagraphsBefore(QTextCursor& cursor, const QTextBlock& block,
                            const std::vector<CopiedParagraph>& paragraphs) {
    const QTextBlockFormat format = block.blockFormat();
    const QTextCharFormat charFormat = paragraphCharFormat(block);
    cursor.setPosition(block.position());
    for (const CopiedParagraph& paragraph : paragraphs) {
        cursor.setBlockFormat(paragraph.blockFormat);
        setParagraphCharFormat(cursor, paragraph.charFormat);
        for (const CopiedRun& run : paragraph.runs) {
            cursor.insertText(run.text, run.format);
        }
        cursor.insertBlock(format, charFormat);
    }
}

/// Remove the spaces at the start (or the end) of the runs, and the runs left empty
void trimRuns(std::vector<CopiedRun>& runs, bool atStart) {
    while (!runs.empty()) {
        QString& text = atStart ? runs.front().text : runs.back().text;
        qsizetype count = 0;
        while (count < text.size() &&
               text.at(atStart ? count : text.size() - 1 - count) == QLatin1Char(' ')) {
            ++count;
        }
        text.remove(atStart ? 0 : text.size() - count, count);
        if (!text.isEmpty()) {
            return;
        }
        runs.erase(atStart ? runs.begin() : runs.end() - 1);
    }
}

/// The paragraphs of a paragraph divided at the separator: each part one, in its formats;
/// at a separator other than a tab the spaces next to it go ("Anna; Bartek")
std::vector<CopiedParagraph> divided(const CopiedParagraph& paragraph, QChar separator) {
    std::vector<CopiedParagraph> parts(1);
    parts.back().blockFormat = paragraph.blockFormat;
    parts.back().charFormat = paragraph.charFormat;
    for (const CopiedRun& run : paragraph.runs) {
        const QStringList pieces = run.text.split(separator);
        for (qsizetype i = 0; i < pieces.size(); ++i) {
            if (i > 0) {
                parts.emplace_back();
                parts.back().blockFormat = paragraph.blockFormat;
                parts.back().charFormat = paragraph.charFormat;
            }
            if (!pieces[i].isEmpty()) {
                parts.back().runs.push_back({pieces[i], run.format});
            }
        }
    }
    if (separator != QLatin1Char('\t')) {
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                trimRuns(parts[i].runs, true);
            }
            if (i + 1 < parts.size()) {
                trimRuns(parts[i].runs, false);
            }
        }
    }
    return parts;
}

/// The left edge of each column and the right one of the last, in percent
std::vector<double> edgesOf(const std::vector<double>& widths) {
    std::vector<double> edges(widths.size() + 1, 0.0);
    for (size_t i = 0; i < widths.size(); ++i) {
        edges[i + 1] = edges[i] + widths[i];
    }
    return edges;
}

/// Remove the blocks with one paragraph break: the one after them, or before them when
/// that is a break at a table; between two such breaks only their text goes
void removeBlocks(QTextCursor& cursor, const QTextBlock& first, const QTextBlock& last) {
    const QTextDocument& document = *cursor.document();
    const int end = last.position() + last.length() - 1;
    if (last.next().isValid() && !isTableBreakAt(document, end)) {
        cursor.setPosition(first.position());
        cursor.setPosition(last.next().position(), QTextCursor::KeepAnchor);
    } else if (first.previous().isValid() && !isTableBreakAt(document, first.position() - 1)) {
        cursor.setPosition(first.position() - 1);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
    } else {
        cursor.setPosition(first.position());
        cursor.setPosition(end, QTextCursor::KeepAnchor);
    }
    cursor.removeSelectedText();
}

/// Remove the table with all its rows; returns the number of the paragraph before it
///
/// The marks of the table's start and end end the paragraphs next to it, so without them
/// Qt joins the paragraph before the table and the one after it; the one after is split
/// off again, in its own formats.
int removeTableFrame(QTextCursor& cursor, QTextTable& table) {
    QTextDocument& document = *cursor.document();
    const QTextBlock before = document.findBlock(table.firstPosition() - 1);
    const QTextBlock after = document.findBlock(table.lastPosition() + 1);
    const int beforeNumber = before.blockNumber();
    const int beforeLength = before.length();
    const int expectedBlocks = document.blockCount() - (after.blockNumber() - beforeNumber - 1);
    const QTextBlockFormat afterFormat = after.blockFormat();
    const QTextCharFormat afterCharFormat = paragraphCharFormat(after);

    table.removeRows(0, table.rows());

    if (document.blockCount() < expectedBlocks) {
        const QTextBlock joined = document.findBlockByNumber(beforeNumber);
        cursor.setPosition(joined.position() + beforeLength - 1);
        cursor.insertBlock(afterFormat, afterCharFormat);
    }
    return beforeNumber;
}

}  // anonymous namespace

// =============================================================================
// Paragraphs copied with their formats
// =============================================================================

QString CopiedParagraph::text() const
{
    QString text;
    for (const CopiedRun& run : runs) {
        text += run.text;
    }
    return text;
}

std::vector<CopiedParagraph> copyParagraphs(const QTextDocument& document, int from, int to)
{
    std::vector<CopiedParagraph> paragraphs;
    for (QTextBlock block = document.findBlock(from); block.isValid() && block.position() <= to;
         block = block.next()) {
        CopiedParagraph paragraph;
        paragraph.blockFormat = block.blockFormat();
        paragraph.charFormat = paragraphCharFormat(block);
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const int start = std::max(from, fragment.position());
            const int end = std::min(to, fragment.position() + fragment.length());
            if (start < end) {
                paragraph.runs.push_back(
                    {fragment.text().mid(start - fragment.position(), end - start),
                     fragment.charFormat()});
            }
        }
        paragraphs.push_back(std::move(paragraph));
    }
    return paragraphs;
}

void insertParagraphs(QTextCursor& cursor, const std::vector<CopiedParagraph>& paragraphs)
{
    bool first = true;
    for (const CopiedParagraph& paragraph : paragraphs) {
        if (first) {
            if (cursor.block().length() == 1) {
                cursor.setBlockFormat(paragraph.blockFormat);
                setParagraphCharFormat(cursor, paragraph.charFormat);
            }
            first = false;
        } else {
            cursor.insertBlock(paragraph.blockFormat, paragraph.charFormat);
        }
        for (const CopiedRun& run : paragraph.runs) {
            cursor.insertText(run.text, run.format);
        }
    }
}

std::vector<CopiedParagraph> cellParagraphs(const QTextTableCell& cell)
{
    if (!cell.isValid()) {
        return {};
    }
    const QTextCursor start = cell.firstCursorPosition();
    return copyParagraphs(*start.document(), cell.firstPosition(), cell.lastPosition());
}

std::vector<QTextTableCell> startingCellsOf(const TableCellRange& range)
{
    std::vector<QTextTableCell> cells;
    if (!range.table) {
        return cells;
    }
    for (int row = range.firstRow; row <= range.lastRow; ++row) {
        for (int column = range.firstColumn; column <= range.lastColumn; ++column) {
            const QTextTableCell cell = range.table->cellAt(row, column);
            if (startsAt(cell, row, column)) {
                cells.push_back(cell);
            }
        }
    }
    return cells;
}

bool isCellEmpty(const QTextTableCell& cell)
{
    return cell.firstPosition() == cell.lastPosition();
}

void clearCell(QTextCursor& cursor, const QTextTableCell& cell, bool keepAnnotations)
{
    if (isCellEmpty(cell)) {
        return;
    }
    const QTextBlock first = cursor.document()->findBlock(cell.firstPosition());
    const QTextBlockFormat format = first.blockFormat();
    cursor.setPosition(cell.firstPosition());
    cursor.setPosition(cell.lastPosition(), QTextCursor::KeepAnchor);
    if (keepAnnotations) {
        removeKeepingAnnotations(cursor);
    } else {
        removeSelectionKeepingObjects(cursor);
    }
    cursor.setBlockFormat(format);
}

void setCellParagraphs(QTextCursor& cursor, const QTextTableCell& cell,
                       const std::vector<CopiedParagraph>& paragraphs)
{
    clearCell(cursor, cell);
    cursor.setPosition(cell.firstPosition());
    insertParagraphs(cursor, paragraphs);
}

// =============================================================================
// Columns and header
// =============================================================================

std::vector<double> columnWidthsOf(const QTextTable& table)
{
    const int columns = std::max(1, table.columns());
    const QList<QTextLength> constraints = table.format().columnWidthConstraints();
    std::vector<double> widths;
    if (constraints.size() == columns) {
        for (const QTextLength& length : constraints) {
            if (length.type() != QTextLength::PercentageLength || length.rawValue() <= 0) {
                widths.clear();
                break;
            }
            widths.push_back(length.rawValue());
        }
    }
    if (widths.empty()) {
        widths.assign(static_cast<size_t>(columns), 100.0 / columns);
    }
    const double total = std::accumulate(widths.begin(), widths.end(), 0.0);
    for (double& width : widths) {
        width = width * 100.0 / total;
    }
    return widths;
}

void setColumnWidths(QTextTable& table, const std::vector<double>& widths)
{
    if (widths.size() != static_cast<size_t>(table.columns()) || widths.empty()) {
        return;
    }
    const double total = std::accumulate(widths.begin(), widths.end(), 0.0);
    if (total <= 0) {
        return;
    }
    QList<QTextLength> constraints;
    double sum = 0;
    for (size_t i = 0; i < widths.size(); ++i) {
        double width = std::round(widths[i] * 10000.0 / total) / 100.0;
        if (i + 1 == widths.size()) {
            width = std::round((100.0 - sum) * 100.0) / 100.0;
        }
        sum += width;
        constraints.append(QTextLength(QTextLength::PercentageLength, std::max(0.01, width)));
    }
    QTextTableFormat format = table.format();
    format.setColumnWidthConstraints(constraints);
    table.setFormat(format);
}

void distributeColumns(QTextTable& table, int first, int last)
{
    first = std::clamp(first, 0, table.columns() - 1);
    last = std::clamp(last, first, table.columns() - 1);
    if (first == 0 && last == table.columns() - 1) {
        QTextTableFormat format = table.format();
        format.clearColumnWidthConstraints();
        table.setFormat(format);
        return;
    }
    std::vector<double> widths = columnWidthsOf(table);
    double together = 0;
    for (int column = first; column <= last; ++column) {
        together += widths[static_cast<size_t>(column)];
    }
    for (int column = first; column <= last; ++column) {
        widths[static_cast<size_t>(column)] = together / (last - first + 1);
    }
    setColumnWidths(table, widths);
}

void setHeaderRows(QTextTable& table, int rows)
{
    QTextTableFormat format = table.format();
    format.setHeaderRowCount(std::clamp(rows, 0, table.rows()));
    table.setFormat(format);
}

// =============================================================================
// Rows and columns
// =============================================================================

void insertTableRows(QTextCursor& cursor, QTextTable& table, int row, int count, bool below,
                     const QTextBlockFormat& paragraphFormat)
{
    if (count <= 0 || row < 0 || row >= table.rows()) {
        return;
    }
    // The new rows are of the row: its properties, its cells' ones and their spans in it
    const TableRowProperties rowProperties = rowPropertiesOf(table, row);
    std::vector<TableCellProperties> cellProperties(static_cast<size_t>(table.columns()));
    std::vector<int> spans(static_cast<size_t>(table.columns()), 1);
    for (int column = 0; column < table.columns(); ++column) {
        const QTextTableCell cell = table.cellAt(row, column);
        cellProperties[static_cast<size_t>(column)] = cellPropertiesOf(cell);
        if (cell.column() == column && cell.rowSpan() == 1) {
            spans[static_cast<size_t>(column)] = cell.columnSpan();
        }
    }
    const int headerRows = table.format().headerRowCount();
    const int at = below ? row + 1 : row;
    table.insertRows(at, count);

    for (int added = at; added < at + count; ++added) {
        for (int column = 0; column < table.columns(); ++column) {
            const QTextTableCell cell = table.cellAt(added, column);
            if (startsAt(cell, added, column)) {
                prepareNewCell(cursor, cell, paragraphFormat,
                               cellProperties[static_cast<size_t>(column)]);
            }
        }
        for (int column = 0; column < table.columns(); ++column) {
            const int span = spans[static_cast<size_t>(column)];
            if (span <= 1 || column + span > table.columns()) {
                continue;
            }
            bool fresh = true;  // only new cells merge as the row's did
            for (int part = column; part < column + span; ++part) {
                const QTextTableCell cell = table.cellAt(added, part);
                fresh = fresh && cell.row() == added && cell.rowSpan() == 1 && cell.columnSpan() == 1;
            }
            if (fresh) {
                table.mergeCells(added, column, 1, span);
            }
        }
        setRowProperties(cursor, table, added, rowProperties);
    }
    if (row < headerRows) {
        setHeaderRows(table, headerRows + count);
    }
}

void insertTableColumns(QTextCursor& cursor, QTextTable& table, int column, int count,
                        bool right, const QTextBlockFormat& paragraphFormat)
{
    if (count <= 0 || column < 0 || column >= table.columns()) {
        return;
    }
    const bool hadWidths = !table.format().columnWidthConstraints().isEmpty();
    std::vector<double> widths = columnWidthsOf(table);
    std::vector<TableCellProperties> cellProperties(static_cast<size_t>(table.rows()));
    for (int row = 0; row < table.rows(); ++row) {
        cellProperties[static_cast<size_t>(row)] = cellPropertiesOf(table.cellAt(row, column));
    }
    const int at = right ? column + 1 : column;
    const double width = widths[static_cast<size_t>(column)];
    table.insertColumns(at, count);

    for (int row = 0; row < table.rows(); ++row) {
        bool added = false;
        for (int part = at; part < at + count; ++part) {
            const QTextTableCell cell = table.cellAt(row, part);
            if (startsAt(cell, row, part)) {
                prepareNewCell(cursor, cell, paragraphFormat, cellProperties[static_cast<size_t>(row)]);
                added = true;
            }
        }
        if (added) {
            setRowProperties(cursor, table, row, rowPropertiesOf(table, row));
        }
    }
    if (hadWidths) {
        widths.insert(widths.begin() + at, static_cast<size_t>(count), width);
        setColumnWidths(table, widths);
    } else {
        QTextTableFormat format = table.format();
        format.clearColumnWidthConstraints();
        table.setFormat(format);
    }
}

int removeTableRows(QTextCursor& cursor, QTextTable& table, int first, int count)
{
    first = std::clamp(first, 0, table.rows() - 1);
    count = std::clamp(count, 1, table.rows() - first);
    if (first == 0 && count == table.rows()) {
        return removeWholeTable(cursor, table);
    }
    const int headerRows = table.format().headerRowCount();
    const int removedHeader = std::max(0, std::min(headerRows, first + count) - first);
    table.removeRows(first, count);
    if (removedHeader > 0) {
        setHeaderRows(table, headerRows - removedHeader);
    }
    const int row = std::min(first, table.rows() - 1);
    return table.cellAt(row, 0).firstPosition();
}

int removeTableColumns(QTextCursor& cursor, QTextTable& table, int first, int count)
{
    first = std::clamp(first, 0, table.columns() - 1);
    count = std::clamp(count, 1, table.columns() - first);
    if (first == 0 && count == table.columns()) {
        return removeWholeTable(cursor, table);
    }
    const bool hadWidths = !table.format().columnWidthConstraints().isEmpty();
    std::vector<double> widths = columnWidthsOf(table);
    table.removeColumns(first, count);
    if (hadWidths) {
        widths.erase(widths.begin() + first, widths.begin() + first + count);
        setColumnWidths(table, widths);
    } else {
        QTextTableFormat format = table.format();
        format.clearColumnWidthConstraints();
        table.setFormat(format);
    }
    const int column = std::min(first, table.columns() - 1);
    return table.cellAt(0, column).firstPosition();
}

int removeWholeTable(QTextCursor& cursor, QTextTable& table)
{
    QTextDocument& document = *cursor.document();
    const QTextBlock before = document.findBlock(table.firstPosition() - 1);
    const QTextBlock after = document.findBlock(table.lastPosition() + 1);
    const bool emptyStart = before.blockNumber() == 0 && before.length() == 1;
    const bool emptyEnd = !after.next().isValid() && after.length() == 1;
    const int beforeNumber = removeTableFrame(cursor, table);

    // An empty paragraph Qt kept only for the table goes, unless the text would be empty
    if (emptyEnd) {
        const QTextBlock last = document.lastBlock();
        if (last.blockNumber() > beforeNumber && last.length() == 1 && canRemoveBlock(last)) {
            removeBlock(cursor, last);
        }
    }
    const QTextBlock start = document.findBlockByNumber(beforeNumber);
    if (emptyStart && start.length() == 1 && start.next().isValid() && canRemoveBlock(start)) {
        removeBlock(cursor, start);
        return document.findBlockByNumber(beforeNumber).position();
    }
    const QTextBlock next = document.findBlockByNumber(beforeNumber + 1);
    return next.isValid() ? next.position() : start.position();
}

// =============================================================================
// Merging and splitting
// =============================================================================

void mergeTableCells(QTextCursor& cursor, const TableCellRange& range)
{
    QTextTable* table = range.table;
    if (!table || (range.rows() <= 1 && range.columns() <= 1)) {
        return;
    }
    const QTextTableCell target = table->cellAt(range.firstRow, range.firstColumn);
    std::vector<CopiedParagraph> moved;
    for (int row = range.firstRow; row <= range.lastRow; ++row) {
        for (int column = range.firstColumn; column <= range.lastColumn; ++column) {
            const QTextTableCell cell = table->cellAt(row, column);
            if (!startsAt(cell, row, column) || cell == target || isCellEmpty(cell)) {
                continue;
            }
            const std::vector<CopiedParagraph> paragraphs = cellParagraphs(cell);
            moved.insert(moved.end(), paragraphs.begin(), paragraphs.end());
            clearCell(cursor, cell);
        }
    }
    if (!moved.empty()) {
        if (isCellEmpty(target)) {
            setCellParagraphs(cursor, target, moved);
        } else {
            cursor.setPosition(target.lastPosition());
            appendParagraphs(cursor, moved);
        }
    }
    table->mergeCells(range.firstRow, range.firstColumn, range.rows(), range.columns());
}

void splitTableCell(QTextCursor& cursor, QTextTable& table, int row, int column, int rows,
                    int columns, bool distribute, const QTextBlockFormat& paragraphFormat)
{
    rows = std::clamp(rows, 1, MAX_SPLIT_CELLS);
    columns = std::clamp(columns, 1, MAX_SPLIT_CELLS);
    const QTextTableCell cell = table.cellAt(row, column);
    if (!cell.isValid()) {
        return;
    }
    const int top = cell.row();
    const int left = cell.column();
    int spannedRows = cell.rowSpan();
    int spannedColumns = cell.columnSpan();
    if (rows == spannedRows && columns == spannedColumns && rows * columns == 1) {
        return;
    }
    const std::vector<CopiedParagraph> paragraphs =
        distribute ? cellParagraphs(cell) : std::vector<CopiedParagraph>{};
    const TableCellProperties properties = cellPropertiesOf(cell);
    const int headerRows = table.format().headerRowCount();
    const bool hadWidths = !table.format().columnWidthConstraints().isEmpty();
    std::vector<double> widths = columnWidthsOf(table);

    // 1. A merged cell into the cells of its rows and columns
    if (spannedRows > 1 || spannedColumns > 1) {
        table.splitCell(top, left, 1, 1);
    }

    // 2. The new columns' edges: a column a new edge goes through splits in two, and the
    //    cells of the other rows span both parts
    std::vector<double> edges = edgesOf(widths);
    const double cellLeft = edges[static_cast<size_t>(left)];
    const double cellRight = edges[static_cast<size_t>(left + spannedColumns)];
    std::vector<double> targetEdges;  // a merged cell split back: its columns' edges
    for (int part = 0; part <= columns; ++part) {
        targetEdges.push_back(columns == spannedColumns
                                  ? edges[static_cast<size_t>(left + part)]
                                  : cellLeft + (cellRight - cellLeft) * part / columns);
    }
    for (int part = 1; part < columns; ++part) {
        const double x = targetEdges[static_cast<size_t>(part)];
        edges = edgesOf(widths);
        int split = -1;
        for (int grid = left; grid < left + spannedColumns; ++grid) {
            if (edges[static_cast<size_t>(grid)] + WIDTH_EPSILON < x &&
                x < edges[static_cast<size_t>(grid) + 1] - WIDTH_EPSILON) {
                split = grid;
                break;
            }
        }
        if (split < 0) {
            continue;  // an edge the cell has already
        }
        table.insertColumns(split + 1, 1);
        const double splitLeft = edges[static_cast<size_t>(split)];
        const double splitRight = edges[static_cast<size_t>(split) + 1];
        widths[static_cast<size_t>(split)] = x - splitLeft;
        widths.insert(widths.begin() + split + 1, splitRight - x);
        ++spannedColumns;
        std::set<int> merged;  // the cells by their first position
        for (int other = 0; other < table.rows(); ++other) {
            if (other >= top && other < top + spannedRows) {
                continue;
            }
            const QTextTableCell before = table.cellAt(other, split);
            const QTextTableCell added = table.cellAt(other, split + 1);
            if (before == added || !startsAt(added, other, split + 1) ||
                merged.count(before.firstPosition()) > 0) {
                continue;  // spanning the place, Qt made it wider
            }
            merged.insert(before.firstPosition());
            table.mergeCells(before.row(), before.column(), before.rowSpan(),
                             before.columnSpan() + 1);
        }
    }

    // 3. The new rows: below the cell's ones, the cells of the other columns spanning them
    const int addedRows = std::max(0, rows - spannedRows);
    if (addedRows > 0) {
        const TableRowProperties rowProperties = rowPropertiesOf(table, top + spannedRows - 1);
        const int at = top + spannedRows;
        table.insertRows(at, addedRows);
        std::set<int> merged;
        for (int other = 0; other < table.columns(); ++other) {
            if (other >= left && other < left + spannedColumns) {
                continue;
            }
            const QTextTableCell above = table.cellAt(at - 1, other);
            const QTextTableCell added = table.cellAt(at, other);
            if (above == added || !startsAt(added, at, other) ||
                merged.count(above.firstPosition()) > 0) {
                continue;
            }
            merged.insert(above.firstPosition());
            table.mergeCells(above.row(), above.column(), above.rowSpan() + addedRows,
                             above.columnSpan());
        }
        for (int added = at; added < at + addedRows; ++added) {
            setRowProperties(cursor, table, added, rowProperties);
        }
        spannedRows += addedRows;
        if (top + spannedRows - addedRows - 1 < headerRows) {
            setHeaderRows(table, headerRows + addedRows);
        }
    }

    // 4. The new cells: groups of the cells of the grid between the new edges
    std::vector<int> rowStarts;  // the first grid row of each new row, then the end
    for (int part = 0, start = top; part <= rows; ++part) {
        rowStarts.push_back(start);
        if (part < rows) {
            start += spannedRows / rows + (part < spannedRows % rows ? 1 : 0);
        }
    }
    edges = edgesOf(widths);
    std::vector<int> columnStarts;
    for (const double x : targetEdges) {
        int grid = left;
        while (grid < left + spannedColumns &&
               edges[static_cast<size_t>(grid)] < x - WIDTH_EPSILON) {
            ++grid;
        }
        columnStarts.push_back(grid);
    }
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < columns; ++j) {
            const int firstRow = rowStarts[static_cast<size_t>(i)];
            const int firstColumn = columnStarts[static_cast<size_t>(j)];
            const int height = rowStarts[static_cast<size_t>(i) + 1] - firstRow;
            const int width = columnStarts[static_cast<size_t>(j) + 1] - firstColumn;
            if (height > 1 || width > 1) {
                table.mergeCells(firstRow, firstColumn, height, width);
            }
        }
    }
    std::vector<QTextTableCell> cells;  // the new cells row by row
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < columns; ++j) {
            cells.push_back(table.cellAt(rowStarts[static_cast<size_t>(i)],
                                         columnStarts[static_cast<size_t>(j)]));
        }
    }
    for (size_t i = 1; i < cells.size(); ++i) {
        if (isCellEmpty(cells[i])) {
            prepareNewCell(cursor, cells[i], paragraphFormat, properties);
        }
    }

    // 5. The widths, and the paragraphs into the new cells in turn
    if (hadWidths || widths.size() != static_cast<size_t>(table.columns()) ||
        std::adjacent_find(widths.begin(), widths.end(), [](double a, double b) {
            return std::abs(a - b) > WIDTH_EPSILON;
        }) != widths.end()) {
        setColumnWidths(table, widths);
    }
    if (distribute && paragraphs.size() > 1) {
        clearCell(cursor, cells.front());
        for (size_t i = 0; i < cells.size(); ++i) {
            if (i >= paragraphs.size()) {
                break;
            }
            const bool last = i + 1 == cells.size();
            const std::vector<CopiedParagraph> part =
                last ? std::vector<CopiedParagraph>(paragraphs.begin() + static_cast<long>(i), paragraphs.end())
                     : std::vector<CopiedParagraph>{paragraphs[i]};
            setCellParagraphs(cursor, cells[i], part);
        }
    }
}

// =============================================================================
// Moving and sorting rows
// =============================================================================

bool hasRowSpans(const QTextTable& table, int first, int last)
{
    for (int row = std::max(0, first); row <= std::min(last, table.rows() - 1); ++row) {
        for (int column = 0; column < table.columns(); ++column) {
            if (table.cellAt(row, column).rowSpan() > 1) {
                return true;
            }
        }
    }
    return false;
}

void reorderTableRows(QTextCursor& cursor, QTextTable& table, int first,
                      const std::vector<int>& order)
{
    struct Row {
        TableRowProperties properties;
        std::vector<std::vector<CopiedParagraph>> cells;
        std::vector<TableCellProperties> cellProperties;
        std::vector<int> spans;
    };
    const int columns = table.columns();
    std::vector<Row> rows(order.size());
    for (size_t i = 0; i < order.size(); ++i) {
        const int row = first + static_cast<int>(i);
        Row& copy = rows[i];
        copy.properties = rowPropertiesOf(table, row);
        copy.cells.resize(static_cast<size_t>(columns));
        copy.cellProperties.resize(static_cast<size_t>(columns));
        copy.spans.assign(static_cast<size_t>(columns), 0);
        for (int column = 0; column < columns; ++column) {
            const QTextTableCell cell = table.cellAt(row, column);
            if (cell.column() == column) {
                copy.cells[static_cast<size_t>(column)] = cellParagraphs(cell);
                copy.cellProperties[static_cast<size_t>(column)] = cellPropertiesOf(cell);
                copy.spans[static_cast<size_t>(column)] = cell.columnSpan();
            }
        }
    }
    for (size_t i = 0; i < order.size(); ++i) {
        const int source = order[i];
        if (source == static_cast<int>(i) || source < 0 || static_cast<size_t>(source) >= rows.size()) {
            continue;
        }
        const int row = first + static_cast<int>(i);
        for (int column = 0; column < columns; ++column) {
            const QTextTableCell cell = table.cellAt(row, column);
            if (startsAt(cell, row, column) && cell.columnSpan() > 1) {
                table.splitCell(row, column, 1, 1);
            }
        }
        const Row& copy = rows[static_cast<size_t>(source)];
        for (int column = 0; column < columns; ++column) {
            const QTextTableCell cell = table.cellAt(row, column);
            setCellParagraphs(cursor, cell, copy.cells[static_cast<size_t>(column)]);
            setCellProperties(cursor, cell, copy.cellProperties[static_cast<size_t>(column)]);
        }
        setRowProperties(cursor, table, row, copy.properties);
        for (int column = 0; column < columns; ++column) {
            const int span = copy.spans[static_cast<size_t>(column)];
            if (span > 1) {
                table.mergeCells(row, column, 1, std::min(span, columns - column));
            }
        }
    }
}

void reorderParagraphs(QTextCursor& cursor, int first, const std::vector<int>& order)
{
    QTextDocument& document = *cursor.document();
    std::vector<CopiedParagraph> paragraphs;
    for (size_t i = 0; i < order.size(); ++i) {
        const QTextBlock block = document.findBlockByNumber(first + static_cast<int>(i));
        std::vector<CopiedParagraph> copy =
            copyParagraphs(document, block.position(), block.position() + block.length() - 1);
        paragraphs.push_back(copy.empty() ? CopiedParagraph{} : std::move(copy.front()));
    }
    std::vector<CopiedParagraph> ordered;
    for (const int source : order) {
        ordered.push_back(paragraphs[static_cast<size_t>(source)]);
    }
    const QTextBlock firstBlock = document.findBlockByNumber(first);
    const QTextBlock lastBlock =
        document.findBlockByNumber(first + static_cast<int>(order.size()) - 1);
    cursor.setPosition(firstBlock.position());
    cursor.setPosition(lastBlock.position() + lastBlock.length() - 1, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    insertParagraphs(cursor, ordered);
}

int moveParagraphs(QTextCursor& cursor, int first, int last, bool down)
{
    QTextDocument& document = *cursor.document();
    const QTextBlock firstBlock = document.findBlockByNumber(first);
    const QTextBlock lastBlock = document.findBlockByNumber(last);
    const QTextBlock neighbour = down ? lastBlock.next() : firstBlock.previous();
    if (!firstBlock.isValid() || !lastBlock.isValid() || !neighbour.isValid() ||
        tableOf(firstBlock) || tableOf(lastBlock)) {
        return -1;
    }
    const int count = last - first + 1;
    QTextTable* table = tableOf(neighbour);
    if (!table) {
        // Past the paragraph or object next to them
        std::vector<int> order(static_cast<size_t>(count) + 1);
        if (down) {
            order[0] = count;
            std::iota(order.begin() + 1, order.end(), 0);
            reorderParagraphs(cursor, first, order);
            return first + 1;
        }
        std::iota(order.begin(), order.end() - 1, 1);
        order.back() = 0;
        reorderParagraphs(cursor, first - 1, order);
        return first - 1;
    }

    // Past the whole table: they go, and come back on its other side
    const std::vector<CopiedParagraph> paragraphs = copyParagraphs(
        document, firstBlock.position(), lastBlock.position() + lastBlock.length() - 1);
    removeBlocks(cursor, firstBlock, lastBlock);
    if (down) {
        const QTextBlock after = document.findBlock(table->lastPosition() + 1);
        const int number = after.blockNumber();
        if (after.length() == 1 && !after.next().isValid()) {
            cursor.setPosition(after.position());
            insertParagraphs(cursor, paragraphs);  // the empty paragraph Qt keeps at the end
        } else {
            insertParagraphsBefore(cursor, after, paragraphs);
        }
        return number;
    }
    const QTextBlock before = document.findBlock(table->firstPosition() - 1);
    if (before.length() == 1 && before.blockNumber() == 0) {
        cursor.setPosition(before.position());
        insertParagraphs(cursor, paragraphs);  // the empty paragraph Qt keeps at the start
        return 0;
    }
    cursor.setPosition(before.position() + before.length() - 1);
    appendParagraphs(cursor, paragraphs);
    return before.blockNumber() + 1;
}

// =============================================================================
// Text to table and back
// =============================================================================

QStringList cellTexts(const QString& paragraph, QChar separator)
{
    return separator.isNull() ? QStringList{paragraph} : paragraph.split(separator);
}

QChar likeliestSeparator(const QStringList& paragraphs)
{
    QChar best;
    qsizetype bestCount = 0;
    for (const QChar candidate : {QChar(u'\t'), QChar(u';'), QChar(u',')}) {
        const qsizetype count =
            std::count_if(paragraphs.begin(), paragraphs.end(),
                          [candidate](const QString& text) { return text.contains(candidate); });
        if (count > bestCount) {
            best = candidate;
            bestCount = count;
        }
    }
    return best;
}

QTextTable* textToTable(QTextCursor& cursor, int first, int last, QChar separator, int columns,
                        bool headerRow, const QTextBlockFormat& paragraphFormat)
{
    QTextDocument& document = *cursor.document();
    const QTextBlock firstBlock = document.findBlockByNumber(first);
    const QTextBlock lastBlock = document.findBlockByNumber(last);
    if (!firstBlock.isValid() || !lastBlock.isValid() || last < first) {
        return nullptr;
    }
    const std::vector<CopiedParagraph> paragraphs = copyParagraphs(
        document, firstBlock.position(), lastBlock.position() + lastBlock.length() - 1);

    // The cells row by row
    std::vector<std::vector<CopiedParagraph>> rows;
    if (separator.isNull()) {
        columns = std::max(1, columns);
        for (size_t i = 0; i < paragraphs.size(); ++i) {
            if (i % static_cast<size_t>(columns) == 0) {
                rows.emplace_back();
            }
            rows.back().push_back(paragraphs[i]);
        }
    } else {
        columns = 1;
        for (const CopiedParagraph& paragraph : paragraphs) {
            rows.push_back(divided(paragraph, separator));
            columns = std::max(columns, static_cast<int>(rows.back().size()));
        }
    }
    if (rows.empty()) {
        return nullptr;
    }

    // In the place of the paragraphs, an empty one the table splits
    cursor.setPosition(firstBlock.position());
    cursor.setPosition(lastBlock.position() + lastBlock.length() - 1, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.setBlockFormat(paragraphFormat);
    setParagraphCharFormat(cursor, QTextCharFormat());

    TableShape shape;
    shape.rows = static_cast<int>(rows.size());
    shape.columns = std::min(columns, MAX_TABLE_SPAN);
    shape.headerRows = headerRow ? 1 : 0;
    QTextTable* table = insertTable(cursor, shape);
    for (int row = 0; row < shape.rows; ++row) {
        for (int column = 0; column < shape.columns; ++column) {
            const QTextTableCell cell = table->cellAt(row, column);
            cursor.setPosition(cell.firstPosition());
            cursor.setBlockFormat(paragraphFormat);
            cursor.setBlockCharFormat(QTextCharFormat());
            const std::vector<CopiedParagraph>& cells = rows[static_cast<size_t>(row)];
            if (static_cast<size_t>(column) < cells.size()) {
                setCellParagraphs(cursor, cell, {cells[static_cast<size_t>(column)]});
            }
        }
    }
    const QTextBlock after = document.findBlock(table->lastPosition() + 1);
    if (after.length() == 1 && canRemoveBlock(after)) {
        removeBlock(cursor, after);
    }
    const QTextBlock before = document.findBlock(table->firstPosition() - 1);
    if (before.length() == 1 && canRemoveBlock(before)) {
        removeBlock(cursor, before);
    }
    return table;
}

int tableToText(QTextCursor& cursor, QTextTable& table, QChar separator)
{
    QTextDocument& document = *cursor.document();
    std::vector<CopiedParagraph> paragraphs;
    for (int row = 0; row < table.rows(); ++row) {
        const std::vector<QTextTableCell> cells = startingCells(table, row);
        if (separator.isNull()) {
            for (const QTextTableCell& cell : cells) {
                const std::vector<CopiedParagraph> own = cellParagraphs(cell);
                paragraphs.insert(paragraphs.end(), own.begin(), own.end());
            }
            continue;
        }
        if (cells.empty()) {
            continue;  // covered whole by cells spanning from above
        }
        CopiedParagraph line;
        for (size_t i = 0; i < cells.size(); ++i) {
            const std::vector<CopiedParagraph> own = cellParagraphs(cells[i]);
            if (i == 0 && !own.empty()) {
                line.blockFormat = own.front().blockFormat;
                line.charFormat = own.front().charFormat;
            }
            if (i > 0) {
                // A space after a semicolon and a comma, as in a sentence
                const bool spaced = separator == QLatin1Char(';') || separator == QLatin1Char(',');
                line.runs.push_back({spaced ? QString(separator) + QLatin1Char(' ') : QString(separator),
                                     QTextCharFormat()});
            }
            for (size_t j = 0; j < own.size(); ++j) {
                if (j > 0) {
                    line.runs.push_back({QStringLiteral(" "), QTextCharFormat()});
                }
                line.runs.insert(line.runs.end(), own[j].runs.begin(), own[j].runs.end());
            }
        }
        paragraphs.push_back(std::move(line));
    }

    const QTextBlock before = document.findBlock(table.firstPosition() - 1);
    const QTextBlock after = document.findBlock(table.lastPosition() + 1);
    const bool emptyStart = before.blockNumber() == 0 && before.length() == 1;
    const bool emptyEnd = !after.next().isValid() && after.length() == 1;
    const int beforeNumber = removeTableFrame(cursor, table);

    const QTextBlock start = document.findBlockByNumber(beforeNumber);
    int firstNumber = beforeNumber + 1;
    if (emptyStart) {
        cursor.setPosition(start.position());
        insertParagraphs(cursor, paragraphs);
        firstNumber = beforeNumber;
    } else {
        cursor.setPosition(start.position() + start.length() - 1);
        appendParagraphs(cursor, paragraphs);
    }
    if (emptyEnd) {
        const QTextBlock lastBlock = document.lastBlock();
        if (lastBlock.blockNumber() > firstNumber + static_cast<int>(paragraphs.size()) - 1 &&
            lastBlock.length() == 1 && canRemoveBlock(lastBlock)) {
            removeBlock(cursor, lastBlock);
        }
    }
    return firstNumber;
}

}  // namespace kalahari::editor
