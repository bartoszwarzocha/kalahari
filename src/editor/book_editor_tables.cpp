/// @file book_editor_tables.cpp
/// @brief BookEditor: tables, the keys in them and at their edges

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/text_tables.h>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>
#include <algorithm>

namespace kalahari::editor {

bool BookEditor::isInTable() const
{
    if (!m_textBuffer) {
        return false;
    }
    const CursorPosition place = hasSelection() ? m_selection.normalized().start : m_cursorPosition;
    return tableOf(m_textBuffer->findBlockByNumber(place.paragraph)) != nullptr;
}

bool BookEditor::moveToCell(bool back)
{
    if (!m_textBuffer) {
        return false;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    QTextTable* table = tableOf(block);
    if (!table) {
        return false;
    }

    // The cells are in the order of the blocks: the next one starts after the cell's last
    // block, the one before ends before its first block
    const QTextTableCell cell = table->cellAt(block.position());
    QTextBlock target = back ? m_textBuffer->findBlock(cell.firstPosition()).previous()
                             : m_textBuffer->findBlock(cell.lastPosition()).next();
    if (tableOf(target) != table) {
        if (back || m_readOnly) {
            return true;  // the first cell has none before it; a read-only table stays
        }
        appendTableRow(*table);
        target = m_textBuffer->findBlock(cell.lastPosition()).next();
    } else if (back) {
        target = m_textBuffer->findBlock(table->cellAt(target.position()).firstPosition());
    }
    clearSelection();
    m_preferredCursorXValid = false;
    setCursorPosition({target.blockNumber(), 0});
    return true;
}

void BookEditor::appendTableRow(QTextTable& table)
{
    // One undo step: the row, and its cells' empty paragraphs in the format of the text
    QTextCursor cursor(m_textBuffer.get());
    cursor.beginEditBlock();
    table.appendRows(1);
    const int row = table.rows() - 1;
    for (int column = 0; column < table.columns(); ++column) {
        const QTextTableCell cell = table.cellAt(row, column);
        if (cell.row() == row && cell.column() == column) {
            cursor.setPosition(cell.firstPosition());
            cursor.setBlockFormat(paragraphBlockFormat());
        }
    }
    cursor.endEditBlock();

    ensureCursorVisible();
    update();
    emit contentChanged();
}

std::optional<int> BookEditor::verticalNeighbourInTable(const QTextBlock& block, bool up,
                                                        qreal x) const
{
    const QTextDocument& document = *m_textBuffer;
    const QTextBlock neighbour = up ? block.previous() : block.next();
    const int breakPosition = up ? block.position() - 1 : block.position() + block.length() - 1;
    if (!neighbour.isValid() || !isTableBreakAt(document, breakPosition)) {
        return std::nullopt;  // the paragraphs follow each other, in a cell too
    }

    // The column of a table under x
    const auto* layout = qobject_cast<KalahariTextDocumentLayout*>(document.documentLayout());
    const auto columnAt = [layout, x](const QTextTable& table) {
        if (!layout) {
            return 0;
        }
        const std::vector<qreal> edges = layout->tableColumnEdges(table);
        const auto after = std::upper_bound(edges.begin() + 1, edges.end() - 1, x);
        return static_cast<int>(std::distance(edges.begin() + 1, after));
    };
    const auto cellBlock = [&document, up](const QTextTableCell& cell) {
        return document.findBlock(up ? cell.lastPosition() : cell.firstPosition()).blockNumber();
    };

    if (const QTextTable* table = tableOf(block)) {
        // The cell above or below, in the cell's columns; out of the table, the paragraph
        // before or after it
        const QTextTableCell cell = table->cellAt(block.position());
        const int column = std::clamp(columnAt(*table), cell.column(),
                                      cell.column() + cell.columnSpan() - 1);
        const int row = up ? cell.row() - 1 : cell.row() + cell.rowSpan();
        if (row >= 0 && row < table->rows()) {
            return cellBlock(table->cellAt(row, column));
        }
        return document.findBlock(up ? table->firstPosition() - 1 : table->lastPosition() + 1)
            .blockNumber();
    }
    // Into a table from the paragraph before or after it: its first or last row
    const QTextTable* table = tableOf(neighbour);
    if (!table) {
        return std::nullopt;
    }
    return cellBlock(table->cellAt(up ? table->rows() - 1 : 0, columnAt(*table)));
}

bool BookEditor::deleteAtTableEdge(bool backward)
{
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    const bool atBreak = backward ? m_cursorPosition.offset == 0
                                  : m_cursorPosition.offset >= block.length() - 1;
    const QTextBlock neighbour = backward ? block.previous() : block.next();
    const int breakPosition =
        backward ? block.position() - 1 : block.position() + block.length() - 1;
    if (!atBreak || !neighbour.isValid() || !isTableBreakAt(*m_textBuffer, breakPosition)) {
        return false;
    }
    if (tableOf(block)) {
        return true;  // a cell keeps its text: cells do not join, nor a cell and a paragraph
    }

    // Next to a table: an empty paragraph goes (unless the table needs it, at the start or
    // end of the text or before another table), and the cursor goes into the table
    const QTextBlock other = backward ? block.next() : block.previous();
    const bool removable =
        block.length() == 1 && other.isValid() &&
        !isTableBreakAt(*m_textBuffer, backward ? block.position() + block.length() - 1
                                                : block.position() - 1);
    const int cellPosition =
        backward ? neighbour.position() + neighbour.length() - 1 : neighbour.position();
    QTextCursor cursor(m_textBuffer.get());
    if (removable) {
        const int removedNumber = block.blockNumber();
        removeBlock(cursor, block);
        cursor.setPosition(backward ? cellPosition : cellPosition - 1);
        finishEdit(cursor);
        emit paragraphRemoved(removedNumber);
    } else {
        setCursorPosition(
            {neighbour.blockNumber(), backward ? neighbour.length() - 1 : 0});
    }
    return true;
}

QTextCursor BookEditor::selectionToReplace()
{
    const SelectionRange sel = m_selection.normalized();
    const int from = calculateAbsolutePosition(sel.start);
    const int to = calculateAbsolutePosition(sel.end);
    if (partsAtTables(*m_textBuffer, from, to).size() <= 1) {
        return createCursor(m_textBuffer.get(), sel.start, sel.end);
    }
    // The text of the cells goes first, the cells staying
    QTextCursor cursor(m_textBuffer.get());
    removeKeepingTables(cursor, from, to);
    return cursor;
}

}  // namespace kalahari::editor
