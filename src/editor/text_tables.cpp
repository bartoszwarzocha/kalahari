/// @file text_tables.cpp
/// @brief Tables in the text: a table of the document, its cells made of paragraphs

#include "kalahari/editor/text_tables.h"
#include "kalahari/editor/annotation.h"
#include "kalahari/editor/paragraph_styles.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>

namespace kalahari::editor {

namespace {

// The characters QTextDocument keeps at the breaks of a table (not in Qt's public headers):
// the start of each cell, and the end of the table
constexpr char16_t CELL_START = 0xFDD0;
constexpr char16_t TABLE_END = 0xFDD1;

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
    return format;
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
    for (int row = 0; row < shape.rows; ++row) {
        for (int column = 0; column < shape.columns; ++column) {
            const QTextTableCell cell = table.cellAt(row, column);
            if (cell.row() == row && cell.column() == column &&
                (cell.rowSpan() > 1 || cell.columnSpan() > 1)) {
                shape.spans.push_back({row, column, cell.rowSpan(), cell.columnSpan()});
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

void spanCells(QTextTable& table, const std::vector<TableSpan>& spans)
{
    for (const TableSpan& span : spans) {
        table.mergeCells(span.row, span.column, span.rows, span.columns);
    }
}

}  // namespace kalahari::editor
