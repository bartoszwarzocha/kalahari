/// @file text_tables.cpp
/// @brief Tables in the text: a table of the document, its cells made of paragraphs

#include "kalahari/editor/text_tables.h"
#include "kalahari/editor/annotation.h"
#include "kalahari/editor/paragraph_styles.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>

#include <algorithm>

namespace kalahari::editor {

namespace {

// The characters QTextDocument keeps at the breaks of a table (not in Qt's public headers):
// the start of each cell, and the end of the table
constexpr char16_t CELL_START = 0xFDD0;
constexpr char16_t TABLE_END = 0xFDD1;

/// The properties of rows and cells in a cell's format
constexpr int CELL_PROPERTIES[] = {ROW_MIN_HEIGHT_PROPERTY, ROW_SPLIT_PROPERTY,
                                   CELL_ALIGNMENT_PROPERTY, CELL_BACKGROUND_PROPERTY};

/// The cells starting in a row
std::vector<QTextTableCell> cellsStartingIn(const QTextTable& table, int row) {
    std::vector<QTextTableCell> cells;
    for (int column = 0; column < table.columns(); ++column) {
        const QTextTableCell cell = table.cellAt(row, column);
        if (cell.row() == row && cell.column() == column) {
            cells.push_back(cell);
        }
    }
    return cells;
}

}  // anonymous namespace

QTextTable* tableOf(const QTextBlock& block)
{
    if (!block.isValid() || !block.document()) {
        return nullptr;
    }
    return tableAt(*block.document(), block.position());
}

QTextTable* tableAt(const QTextDocument& document, int position)
{
    // A cell is not a frame of its own: the frame of a place in a cell is its table
    return qobject_cast<QTextTable*>(document.frameAt(position));
}

QTextTableFormat tableFormatOf(const TableShape& shape)
{
    QTextTableFormat format;
    format.setColumns(shape.columns);
    format.setHeaderRowCount(shape.headerRows);
    format.setWidth(QTextLength(QTextLength::PercentageLength, 100));
    format.setBorder(0);
    format.setBorderStyle(QTextFrameFormat::BorderStyle_None);
    format.setCellPadding(0);
    format.setCellSpacing(0);
    format.setMargin(0);
    format.setPadding(0);
    if (!shape.columnWidths.empty() &&
        shape.columnWidths.size() == static_cast<size_t>(shape.columns)) {
        QList<QTextLength> widths;
        for (const double width : shape.columnWidths) {
            widths.append(QTextLength(QTextLength::PercentageLength, width));
        }
        format.setColumnWidthConstraints(widths);
    }
    if (shape.properties != TableProperties{}) {
        setTableProperties(format, shape.properties);
    }
    return format;
}

TableProperties tablePropertiesOf(const QTextTableFormat& format)
{
    TableProperties properties;
    const QTextLength width = format.width();
    if (width.type() == QTextLength::FixedLength && width.rawValue() > 0) {
        properties.width = width.rawValue();
    }
    const Qt::Alignment alignment = format.alignment() & Qt::AlignHorizontal_Mask;
    properties.alignment = (alignment & Qt::AlignHCenter) ? TableAlignment::Center
                           : (alignment & Qt::AlignRight) ? TableAlignment::Right
                                                          : TableAlignment::Left;
    properties.indent = std::max(0.0, format.doubleProperty(TABLE_INDENT_PROPERTY));
    if (format.hasProperty(TABLE_BORDER_LINES_PROPERTY)) {
        properties.borders.lines = format.intProperty(TABLE_BORDER_LINES_PROPERTY) & BORDER_GRID;
    }
    if (format.hasProperty(TABLE_BORDER_STYLE_PROPERTY)) {
        properties.borders.style = static_cast<BorderStyle>(std::clamp(
            format.intProperty(TABLE_BORDER_STYLE_PROPERTY), 0, static_cast<int>(BorderStyle::Double)));
    }
    if (format.hasProperty(TABLE_BORDER_WIDTH_PROPERTY)) {
        properties.borders.width = format.doubleProperty(TABLE_BORDER_WIDTH_PROPERTY);
    }
    properties.borders.color = format.colorProperty(TABLE_BORDER_COLOR_PROPERTY);
    return properties;
}

void setTableProperties(QTextTableFormat& format, const TableProperties& properties)
{
    format.setWidth(properties.width ? QTextLength(QTextLength::FixedLength, *properties.width)
                                     : QTextLength(QTextLength::PercentageLength, 100));
    format.setAlignment(properties.alignment == TableAlignment::Center  ? Qt::AlignHCenter
                        : properties.alignment == TableAlignment::Right ? Qt::AlignRight
                                                                         : Qt::AlignLeft);
    format.setProperty(TABLE_INDENT_PROPERTY, properties.indent);
    format.setProperty(TABLE_BORDER_LINES_PROPERTY, properties.borders.lines);
    format.setProperty(TABLE_BORDER_STYLE_PROPERTY, static_cast<int>(properties.borders.style));
    format.setProperty(TABLE_BORDER_WIDTH_PROPERTY, properties.borders.width);
    format.setProperty(TABLE_BORDER_COLOR_PROPERTY, properties.borders.color);
}

TableRowProperties rowPropertiesOf(const QTextTable& table, int row)
{
    TableRowProperties properties;
    const std::vector<QTextTableCell> cells = cellsStartingIn(table, row);
    if (!cells.empty()) {
        const QTextCharFormat format = cells.front().format();
        properties.minHeight = std::max(0.0, format.doubleProperty(ROW_MIN_HEIGHT_PROPERTY));
        properties.allowSplit = format.boolProperty(ROW_SPLIT_PROPERTY);
    }
    return properties;
}

TableCellProperties cellPropertiesOf(const QTextTableCell& cell)
{
    TableCellProperties properties;
    const QTextCharFormat format = cell.format();
    properties.alignment = static_cast<CellAlignment>(std::clamp(
        format.intProperty(CELL_ALIGNMENT_PROPERTY), 0, static_cast<int>(CellAlignment::Bottom)));
    properties.background = format.colorProperty(CELL_BACKGROUND_PROPERTY);
    return properties;
}

void setRowProperties(QTextCursor& cursor, const QTextTable& table, int row,
                      const TableRowProperties& properties)
{
    QTextCharFormat format;
    format.setProperty(ROW_MIN_HEIGHT_PROPERTY, std::max(0.0, properties.minHeight));
    format.setProperty(ROW_SPLIT_PROPERTY, properties.allowSplit);
    for (const QTextTableCell& cell : cellsStartingIn(table, row)) {
        cursor.setPosition(cell.firstPosition());
        cursor.mergeBlockCharFormat(format);
    }
}

void setCellProperties(QTextCursor& cursor, const QTextTableCell& cell,
                       const TableCellProperties& properties)
{
    QTextCharFormat format;
    format.setProperty(CELL_ALIGNMENT_PROPERTY, static_cast<int>(properties.alignment));
    format.setProperty(CELL_BACKGROUND_PROPERTY, properties.background);
    cursor.setPosition(cell.firstPosition());
    cursor.mergeBlockCharFormat(format);
}

TableShape tableShapeOf(const QTextTable& table)
{
    TableShape shape;
    shape.rows = table.rows();
    shape.columns = table.columns();
    const QTextTableFormat format = table.format();
    shape.headerRows = format.headerRowCount();
    const QList<QTextLength> widths = format.columnWidthConstraints();
    if (widths.size() == shape.columns) {
        for (const QTextLength& width : widths) {
            if (width.type() != QTextLength::PercentageLength) {
                shape.columnWidths.clear();
                break;
            }
            shape.columnWidths.push_back(width.rawValue());
        }
    }
    shape.properties = tablePropertiesOf(format);
    for (int row = 0; row < shape.rows; ++row) {
        if (const TableRowProperties properties = rowPropertiesOf(table, row);
            properties != TableRowProperties{}) {
            shape.rowFormats.push_back({row, properties});
        }
        for (int column = 0; column < shape.columns; ++column) {
            const QTextTableCell cell = table.cellAt(row, column);
            if (cell.row() != row || cell.column() != column) {
                continue;
            }
            if (cell.rowSpan() > 1 || cell.columnSpan() > 1) {
                shape.spans.push_back({row, column, cell.rowSpan(), cell.columnSpan()});
            }
            if (const TableCellProperties properties = cellPropertiesOf(cell);
                properties != TableCellProperties{}) {
                shape.cellFormats.push_back({row, column, properties});
            }
        }
    }
    return shape;
}

QTextTable* insertTable(QTextCursor& cursor, const TableShape& shape)
{
    if (shape.rows <= 0 || shape.columns <= 0 || !cursor.document()) {
        return nullptr;
    }

    // The rest of the paragraph after the table keeps its formatting, without the
    // annotations on its start (they stay where they are, before the table)
    const QTextBlockFormat blockFormat = cursor.blockFormat();
    QTextCharFormat charFormat = cursor.blockCharFormat();
    setAnnotations(charFormat, {});

    cursor.beginEditBlock();
    QTextTable* table = cursor.insertTable(shape.rows, shape.columns, tableFormatOf(shape));
    QTextCursor after(cursor.document());
    after.setPosition(table->lastPosition() + 1);
    after.setBlockFormat(blockFormat);
    after.setBlockCharFormat(charFormat);
    cursor.endEditBlock();

    cursor.setPosition(table->cellAt(0, 0).firstPosition());
    return table;
}

bool isTableBreakAt(const QTextDocument& document, int position)
{
    const QChar character = document.characterAt(position);
    return character == QChar(CELL_START) || character == QChar(TABLE_END);
}

bool canRemoveBlock(const QTextBlock& block)
{
    const QTextDocument& document = *block.document();
    return (block.next().isValid() &&
            !isTableBreakAt(document, block.position() + block.length() - 1)) ||
           (block.previous().isValid() && !isTableBreakAt(document, block.position() - 1));
}

std::vector<std::pair<int, int>> partsAtTables(const QTextDocument& document, int from, int to)
{
    // The cell of a block (the position of its start), or -1 outside tables and in a table
    // the range covers whole
    const auto cellOf = [&document, from, to](const QTextBlock& block) {
        const QTextTable* table = tableAt(document, block.position());
        if (!table || (from < table->firstPosition() && to > table->lastPosition())) {
            return -1;
        }
        return table->cellAt(block.position()).firstPosition();
    };

    std::vector<std::pair<int, int>> parts;
    int start = from;
    QTextBlock block = document.findBlock(from);
    int cell = cellOf(block);
    for (QTextBlock next = block.next(); next.isValid() && next.position() <= to;
         next = next.next()) {
        const int nextCell = cellOf(next);
        if (nextCell != cell) {
            parts.emplace_back(start, next.position() - 1);  // to the end of the block before
            start = next.position();
            cell = nextCell;
        }
    }
    parts.emplace_back(start, to);
    return parts;
}

void removeKeepingTables(QTextCursor& cursor, int from, int to, bool keepAnnotations)
{
    QTextDocument& document = *cursor.document();
    const std::vector<std::pair<int, int>> parts = partsAtTables(document, from, to);

    // From the last part: the positions of the parts before it stay. The cursor ends where
    // removing the first part puts it (after an object: at the text after it).
    cursor.beginEditBlock();
    for (auto part = parts.rbegin(); part != parts.rend(); ++part) {
        cursor.setPosition(part->first);
        if (part->first >= part->second) {
            continue;
        }
        cursor.setPosition(part->second, QTextCursor::KeepAnchor);
        if (keepAnnotations) {
            removeKeepingAnnotations(cursor);
        } else {
            removeSelectionKeepingObjects(cursor);
        }
    }
    cursor.endEditBlock();
}

bool isCellProperty(int property)
{
    return (property >= QTextFormat::TableCellRowSpan &&
            property <= QTextFormat::TableCellRightBorderBrush) ||
           std::find(std::begin(CELL_PROPERTIES), std::end(CELL_PROPERTIES), property) !=
               std::end(CELL_PROPERTIES);
}

QTextCharFormat paragraphCharFormat(const QTextBlock& block)
{
    QTextCharFormat format = block.charFormat();
    if (format.objectType() == QTextFormat::TableCellObject) {
        format.clearProperty(QTextFormat::ObjectType);
    }
    const QMap<int, QVariant> properties = format.properties();
    for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
        if (isCellProperty(it.key())) {
            format.clearProperty(it.key());
        }
    }
    return format;
}

void setParagraphCharFormat(QTextCursor& cursor, const QTextCharFormat& format)
{
    QTextCharFormat paragraph = format;
    if (paragraph.objectType() == QTextFormat::TableCellObject) {
        paragraph.clearProperty(QTextFormat::ObjectType);
    }
    const QMap<int, QVariant> own = paragraph.properties();
    for (auto it = own.cbegin(); it != own.cend(); ++it) {
        if (isCellProperty(it.key())) {
            paragraph.clearProperty(it.key());
        }
    }
    // The first paragraph of a cell keeps the cell's format: its span, its properties
    const QTextBlock block = cursor.block();
    const QTextTable* table = tableOf(block);
    if (table && table->cellAt(block.position()).firstPosition() == block.position()) {
        const QTextCharFormat cell = block.charFormat();
        const QMap<int, QVariant> properties = cell.properties();
        for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
            if (isCellProperty(it.key()) || it.key() == QTextFormat::ObjectType) {
                paragraph.setProperty(it.key(), it.value());
            }
        }
    }
    cursor.setBlockCharFormat(paragraph);
}

void spanCells(QTextTable& table, const std::vector<TableSpan>& spans)
{
    for (const TableSpan& span : spans) {
        table.mergeCells(span.row, span.column, span.rows, span.columns);
    }
}

void finishTableCells(QTextTable& table, const TableShape& shape)
{
    spanCells(table, shape.spans);
    QTextCursor cursor(table.document());
    for (const TableRowFormat& row : shape.rowFormats) {
        if (row.row >= 0 && row.row < table.rows()) {
            setRowProperties(cursor, table, row.row, row.properties);
        }
    }
    for (const TableCellFormat& cell : shape.cellFormats) {
        const QTextTableCell target = table.cellAt(cell.row, cell.column);
        if (target.isValid() && target.row() == cell.row && target.column() == cell.column) {
            setCellProperties(cursor, target, cell.properties);
        }
    }
}

TableCellRange wholeCells(const TableCellRange& range)
{
    TableCellRange whole = range;
    if (!whole.table) {
        return whole;
    }
    // Each merged cell crossing an edge moves the edge out, until none does
    for (bool grown = true; grown;) {
        grown = false;
        for (int row = whole.firstRow; row <= whole.lastRow; ++row) {
            for (int column = whole.firstColumn; column <= whole.lastColumn; ++column) {
                const QTextTableCell cell = whole.table->cellAt(row, column);
                if (!cell.isValid()) {
                    continue;
                }
                const int lastRow = cell.row() + cell.rowSpan() - 1;
                const int lastColumn = cell.column() + cell.columnSpan() - 1;
                if (cell.row() < whole.firstRow || cell.column() < whole.firstColumn ||
                    lastRow > whole.lastRow || lastColumn > whole.lastColumn) {
                    whole.firstRow = std::min(whole.firstRow, cell.row());
                    whole.firstColumn = std::min(whole.firstColumn, cell.column());
                    whole.lastRow = std::max(whole.lastRow, lastRow);
                    whole.lastColumn = std::max(whole.lastColumn, lastColumn);
                    grown = true;
                }
            }
        }
    }
    return whole;
}

std::optional<TableCellRange> selectedCells(const QTextDocument& document, int anchor,
                                            int position)
{
    if (anchor == position) {
        return std::nullopt;
    }
    QTextTable* table = tableAt(document, anchor);
    if (!table || tableAt(document, position) != table) {
        return std::nullopt;
    }
    const QTextTableCell from = table->cellAt(anchor);
    const QTextTableCell to = table->cellAt(position);
    if (!from.isValid() || !to.isValid() || from == to) {
        return std::nullopt;
    }
    TableCellRange range;
    range.table = table;
    range.firstRow = std::min(from.row(), to.row());
    range.firstColumn = std::min(from.column(), to.column());
    range.lastRow = std::max(from.row() + from.rowSpan(), to.row() + to.rowSpan()) - 1;
    range.lastColumn = std::max(from.column() + from.columnSpan(), to.column() + to.columnSpan()) - 1;
    return wholeCells(range);
}

}  // namespace kalahari::editor
