/// @file book_editor_table_commands.cpp
/// @brief BookEditor: the Table menu – rows, columns, cells, the header, sorting, moving,
///        turning text into a table and back, and the properties of a table

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/table_editing.h>
#include <kalahari/editor/text_sorting.h>
#include <kalahari/editor/text_tables.h>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace kalahari::editor {

namespace {

/// The text of a cell: its paragraphs separated by @p separator
QString cellText(const QTextTableCell& cell, QChar separator)
{
    QStringList paragraphs;
    for (const CopiedParagraph& paragraph : cellParagraphs(cell)) {
        paragraphs << paragraph.text();
    }
    return paragraphs.join(separator);
}

/// The places of the corners of a rectangle of cells: the start of its first cell and the
/// end of its last one, as a selection of them
std::pair<int, int> cornersOf(const QTextTable& table, int firstRow, int firstColumn,
                              int lastRow, int lastColumn)
{
    return {table.cellAt(firstRow, firstColumn).firstPosition(),
            table.cellAt(lastRow, lastColumn).lastPosition()};
}

}  // anonymous namespace

// =============================================================================
// The cells
// =============================================================================

std::optional<TableCellRange> BookEditor::selectedCellRange() const
{
    if (!m_textBuffer || !hasSelection()) {
        return std::nullopt;
    }
    const SelectionRange sel = m_selection.normalized();
    return selectedCells(*m_textBuffer, calculateAbsolutePosition(sel.start),
                         calculateAbsolutePosition(sel.end));
}

std::optional<TableCellRange> BookEditor::tableCells() const
{
    if (std::optional<TableCellRange> selected = selectedCellRange()) {
        return selected;
    }
    if (!m_textBuffer) {
        return std::nullopt;
    }
    const CursorPosition place = hasSelection() ? m_selection.normalized().start : m_cursorPosition;
    const int position = calculateAbsolutePosition(place);
    QTextTable* table = tableAt(*m_textBuffer, position);
    if (!table) {
        return std::nullopt;
    }
    const QTextTableCell cell = table->cellAt(position);
    if (!cell.isValid()) {
        return std::nullopt;
    }
    return TableCellRange{table, cell.row(), cell.row() + cell.rowSpan() - 1, cell.column(),
                          cell.column() + cell.columnSpan() - 1};
}

void BookEditor::beginTableStep(QTextCursor& cursor)
{
    m_stepCursor.reset();
    m_pendingFormat = QTextCharFormat();
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
}

void BookEditor::endTableStep(QTextCursor& cursor, int position, int anchor)
{
    const int end = std::max(0, m_textBuffer->characterCount() - 1);
    position = std::clamp(position, 0, end);
    anchor = anchor < 0 ? position : std::clamp(anchor, 0, end);
    const CursorPosition at = calculateCursorPosition(position);
    const CursorPosition from = calculateCursorPosition(anchor);
    const SelectionRange selection =
        anchor != position ? SelectionRange{from, at}.normalized() : SelectionRange{};
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{at, selection}] { m_stepCursor = state; }));
    cursor.endEditBlock();

    clearSelection();
    m_preferredCursorXValid = false;
    m_cursorPosition = at;
    if (anchor != position) {
        m_selectionAnchor = from;
        setSelection({from, at});
    }
    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    ensureCursorVisible();
    syncPipelineCursor();
    update();
    emit contentChanged();
    emit paragraphModified(m_cursorPosition.paragraph);
    emit cursorPositionChanged(m_cursorPosition);
}

bool BookEditor::clearSelectedCells(bool keepAnnotations)
{
    const std::optional<TableCellRange> cells = selectedCellRange();
    if (!cells) {
        return false;
    }
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    // From the last cell back: the places of the cells before stay where they are
    const std::vector<QTextTableCell> cleared = startingCellsOf(*cells);
    for (auto cell = cleared.rbegin(); cell != cleared.rend(); ++cell) {
        clearCell(cursor, *cell, keepAnnotations);
    }
    endTableStep(cursor, cells->table->cellAt(cells->firstRow, cells->firstColumn).firstPosition());
    return true;
}

// =============================================================================
// Rows and columns
// =============================================================================

void BookEditor::insertTableRows(bool below)
{
    if (refusesChange()) {
        return;
    }
    const std::optional<TableCellRange> cells = tableCells();
    if (!cells) {
        return;
    }
    // The cursor and the selection stay at their text
    QTextCursor anchor = createCursor(m_textBuffer.get(), hasSelection() ? m_selectionAnchor : m_cursorPosition);
    QTextCursor place = createCursor(m_textBuffer.get(), m_cursorPosition);
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    editor::insertTableRows(cursor, *cells->table, below ? cells->lastRow : cells->firstRow,
                            cells->rows(), below, paragraphBlockFormat());
    endTableStep(cursor, place.position(), anchor.position());
}

void BookEditor::insertTableColumns(bool right)
{
    if (refusesChange()) {
        return;
    }
    const std::optional<TableCellRange> cells = tableCells();
    if (!cells) {
        return;
    }
    QTextCursor anchor = createCursor(m_textBuffer.get(), hasSelection() ? m_selectionAnchor : m_cursorPosition);
    QTextCursor place = createCursor(m_textBuffer.get(), m_cursorPosition);
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    editor::insertTableColumns(cursor, *cells->table, right ? cells->lastColumn : cells->firstColumn,
                               cells->columns(), right, paragraphBlockFormat());
    endTableStep(cursor, place.position(), anchor.position());
}

void BookEditor::deleteTableRows()
{
    if (refusesChange()) {
        return;
    }
    const std::optional<TableCellRange> cells = tableCells();
    if (!cells) {
        return;
    }
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    const int place = removeTableRows(cursor, *cells->table, cells->firstRow, cells->rows());
    endTableStep(cursor, place);
}

void BookEditor::deleteTableColumns()
{
    if (refusesChange()) {
        return;
    }
    const std::optional<TableCellRange> cells = tableCells();
    if (!cells) {
        return;
    }
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    const int place =
        removeTableColumns(cursor, *cells->table, cells->firstColumn, cells->columns());
    endTableStep(cursor, place);
}

void BookEditor::deleteTable()
{
    if (refusesChange()) {
        return;
    }
    const std::optional<TableCellRange> cells = tableCells();
    if (!cells) {
        return;
    }
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    const int place = removeWholeTable(cursor, *cells->table);
    endTableStep(cursor, place);
}

void BookEditor::selectTablePart(TablePart part)
{
    const std::optional<TableCellRange> cells = tableCells();
    if (!cells) {
        return;
    }
    const QTextTable& table = *cells->table;
    std::pair<int, int> corners;
    switch (part) {
        case TablePart::Cell: {
            const QTextTableCell cell = table.cellAt(cells->firstRow, cells->firstColumn);
            corners = {cell.firstPosition(), cell.lastPosition()};
            break;
        }
        case TablePart::Row:
            corners = cornersOf(table, cells->firstRow, 0, cells->lastRow, table.columns() - 1);
            break;
        case TablePart::Column:
            corners = cornersOf(table, 0, cells->firstColumn, table.rows() - 1, cells->lastColumn);
            break;
        case TablePart::Table:
            corners = cornersOf(table, 0, 0, table.rows() - 1, table.columns() - 1);
            break;
    }
    const CursorPosition anchor = calculateCursorPosition(corners.first);
    const CursorPosition position = calculateCursorPosition(corners.second);
    clearSelection();
    m_preferredCursorXValid = false;
    m_selectionAnchor = anchor;
    m_cursorPosition = position;
    if (anchor != position) {
        setSelection({anchor, position});
    }
    syncPipelineCursor();
    ensureCursorVisible();
    update();
    emit cursorPositionChanged(m_cursorPosition);
}

// =============================================================================
// Merging and splitting
// =============================================================================

bool BookEditor::canMergeTableCells() const
{
    return selectedCellRange().has_value();
}

void BookEditor::mergeTableCells()
{
    if (refusesChange()) {
        return;
    }
    const std::optional<TableCellRange> cells = selectedCellRange();
    if (!cells) {
        return;
    }
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    editor::mergeTableCells(cursor, *cells);
    endTableStep(cursor, cells->table->cellAt(cells->firstRow, cells->firstColumn).firstPosition());
}

bool BookEditor::canSplitTableCells() const
{
    return tableCells().has_value();
}

int BookEditor::splitCellParagraphs(bool mergeFirst) const
{
    const std::optional<TableCellRange> selected = mergeFirst ? selectedCellRange() : std::nullopt;
    if (selected) {
        // The paragraphs of the cells merged: an empty cell adds none
        int count = 0;
        for (const QTextTableCell& cell : startingCellsOf(*selected)) {
            if (!isCellEmpty(cell)) {
                count += static_cast<int>(cellParagraphs(cell).size());
            }
        }
        return std::max(1, count);
    }
    if (!m_textBuffer) {
        return 0;
    }
    const int position = calculateAbsolutePosition(m_cursorPosition);
    const QTextTable* table = tableAt(*m_textBuffer, position);
    return table ? static_cast<int>(cellParagraphs(table->cellAt(position)).size()) : 0;
}

void BookEditor::splitTableCells(int columns, int rows, bool mergeFirst, bool distribute)
{
    if (refusesChange()) {
        return;
    }
    columns = std::clamp(columns, 1, MAX_SPLIT_CELLS);
    rows = std::clamp(rows, 1, MAX_SPLIT_CELLS);
    const std::optional<TableCellRange> selected = mergeFirst ? selectedCellRange() : std::nullopt;
    const int position = calculateAbsolutePosition(m_cursorPosition);
    QTextTable* table = selected ? selected->table : tableAt(*m_textBuffer, position);
    if (!table || (columns == 1 && rows == 1 && !selected)) {
        return;
    }
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    int row = 0;
    int column = 0;
    if (selected) {
        editor::mergeTableCells(cursor, *selected);
        row = selected->firstRow;
        column = selected->firstColumn;
    } else {
        const QTextTableCell cell = table->cellAt(position);
        row = cell.row();
        column = cell.column();
    }
    splitTableCell(cursor, *table, row, column, rows, columns, distribute, paragraphBlockFormat());
    endTableStep(cursor, table->cellAt(row, column).firstPosition());
}

// =============================================================================
// Rows and columns evenly
// =============================================================================

void BookEditor::distributeTableRows()
{
    if (refusesChange()) {
        return;
    }
    const std::optional<TableCellRange> cells = tableCells();
    const auto* layout =
        m_textBuffer ? qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())
                     : nullptr;
    if (!cells || !layout) {
        return;
    }
    QTextTable& table = *cells->table;
    const bool selected = selectedCellRange().has_value();
    const int first = selected ? cells->firstRow : 0;
    const int last = selected ? cells->lastRow : table.rows() - 1;

    // The least height of the highest row, in points (as the layout shows points)
    const std::vector<qreal> heights = layout->tableRowHeights(table);
    qreal highest = 0;
    for (int row = first; row <= last; ++row) {
        highest = std::max(highest, heights[static_cast<size_t>(row)]);
    }
    const qreal pixels = layout->pixelsPerPoint();
    if (highest <= 0 || pixels <= 0) {
        return;
    }
    const double points = std::min(std::round(highest / pixels * 100.0) / 100.0, MAX_ROW_HEIGHT);

    QTextCursor anchor = createCursor(m_textBuffer.get(), hasSelection() ? m_selectionAnchor : m_cursorPosition);
    QTextCursor place = createCursor(m_textBuffer.get(), m_cursorPosition);
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    for (int row = first; row <= last; ++row) {
        TableRowProperties properties = rowPropertiesOf(table, row);
        properties.minHeight = points;
        setRowProperties(cursor, table, row, properties);
    }
    endTableStep(cursor, place.position(), anchor.position());
}

void BookEditor::distributeTableColumns()
{
    if (refusesChange()) {
        return;
    }
    const std::optional<TableCellRange> cells = tableCells();
    if (!cells) {
        return;
    }
    QTextTable& table = *cells->table;
    const bool selected = selectedCellRange().has_value();
    QTextCursor anchor = createCursor(m_textBuffer.get(), hasSelection() ? m_selectionAnchor : m_cursorPosition);
    QTextCursor place = createCursor(m_textBuffer.get(), m_cursorPosition);
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    if (selected) {
        distributeColumns(table, cells->firstColumn, cells->lastColumn);
    } else {
        distributeColumns(table, 0, table.columns() - 1);
    }
    endTableStep(cursor, place.position(), anchor.position());
}

// =============================================================================
// The header
// =============================================================================

bool BookEditor::isTableHeader() const
{
    const std::optional<TableCellRange> cells = tableCells();
    return cells && cells->firstRow < cells->table->format().headerRowCount();
}

bool BookEditor::canChangeTableHeader() const
{
    const std::optional<TableCellRange> cells = tableCells();
    return cells && cells->firstRow <= std::max(0, cells->table->format().headerRowCount());
}

void BookEditor::setTableHeader(bool header)
{
    if (refusesChange()) {
        return;
    }
    const std::optional<TableCellRange> cells = tableCells();
    if (!cells || !canChangeTableHeader()) {
        return;
    }
    const int rows = header ? cells->lastRow + 1 : cells->firstRow;
    if (rows == cells->table->format().headerRowCount()) {
        return;
    }
    QTextCursor anchor = createCursor(m_textBuffer.get(), hasSelection() ? m_selectionAnchor : m_cursorPosition);
    QTextCursor place = createCursor(m_textBuffer.get(), m_cursorPosition);
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    setHeaderRows(*cells->table, rows);
    endTableStep(cursor, place.position(), anchor.position());
}

// =============================================================================
// Sorting
// =============================================================================

std::pair<int, int> BookEditor::selectedBlockRange() const
{
    if (!hasSelection()) {
        return {m_cursorPosition.paragraph, m_cursorPosition.paragraph};
    }
    const SelectionRange sel = m_selection.normalized();
    int last = sel.end.paragraph;
    if (sel.end.offset == 0 && last > sel.start.paragraph) {
        --last;  // the selection ends at the start of the paragraph: not in it
    }
    return {sel.start.paragraph, last};
}

std::optional<std::pair<int, int>> BookEditor::sortedParagraphs() const
{
    if (!m_textBuffer) {
        return std::nullopt;
    }
    auto [first, last] = selectedBlockRange();
    if (first == last) {
        // The items of the cursor's list, around it
        const QTextBlock block = m_textBuffer->findBlockByNumber(first);
        const std::optional<ListItem> item = listItemOf(block.blockFormat());
        if (!item || tableOf(block) || blockObject(block) != TextObject::None) {
            return std::nullopt;
        }
        const auto sameList = [&item](const QTextBlock& other) {
            const std::optional<ListItem> otherItem = listItemOf(other.blockFormat());
            return other.isValid() && !tableOf(other) && blockObject(other) == TextObject::None &&
                   otherItem && otherItem->id == item->id;
        };
        for (QTextBlock before = block.previous(); sameList(before); before = before.previous()) {
            first = before.blockNumber();
        }
        for (QTextBlock after = block.next(); sameList(after); after = after.next()) {
            last = after.blockNumber();
        }
        if (first == last) {
            return std::nullopt;
        }
        return std::pair{first, last};
    }
    for (int number = first; number <= last; ++number) {
        const QTextBlock block = m_textBuffer->findBlockByNumber(number);
        if (tableOf(block) || blockObject(block) != TextObject::None) {
            return std::nullopt;
        }
    }
    return std::pair{first, last};
}

std::optional<TableCellRange> BookEditor::sortedRows() const
{
    const std::optional<TableCellRange> cells = tableCells();
    if (!cells) {
        return std::nullopt;
    }
    QTextTable& table = *cells->table;
    const int header = std::clamp(table.format().headerRowCount(), 0, table.rows());
    TableCellRange rows{&table, header, table.rows() - 1, 0, table.columns() - 1};
    if (selectedCellRange() && cells->rows() > 1) {
        rows.firstRow = std::max(cells->firstRow, header);  // the header stays on top
        rows.lastRow = cells->lastRow;
    }
    if (rows.lastRow <= rows.firstRow || hasRowSpans(table, rows.firstRow, rows.lastRow)) {
        return std::nullopt;
    }
    return rows;
}

std::optional<BookEditor::SortSubject> BookEditor::sortSubject() const
{
    if (!m_textBuffer) {
        return std::nullopt;
    }
    SortSubject subject;
    if (isInTable()) {
        const std::optional<TableCellRange> rows = sortedRows();
        if (!rows) {
            return std::nullopt;
        }
        subject.tableRows = true;
        subject.count = rows->rows();
        // The names of the columns: the texts of the last header row
        const int header = rows->table->format().headerRowCount();
        for (int column = 0; column < rows->table->columns(); ++column) {
            subject.columns << (header > 0 ? cellText(rows->table->cellAt(header - 1, column),
                                                      QLatin1Char(' '))
                                                 .simplified()
                                           : QString());
        }
        return subject;
    }
    const std::optional<std::pair<int, int>> paragraphs = sortedParagraphs();
    if (!paragraphs) {
        return std::nullopt;
    }
    subject.count = paragraphs->second - paragraphs->first + 1;
    return subject;
}

QString BookEditor::sortRefusal() const
{
    if (!m_textBuffer || sortSubject()) {
        return QString();
    }
    if (isInTable()) {
        const std::optional<TableCellRange> cells = tableCells();
        if (cells) {
            const QTextTable& table = *cells->table;
            const int header = std::clamp(table.format().headerRowCount(), 0, table.rows());
            if (table.rows() - header < 2) {
                return tr("The table has no rows to sort under its header");
            }
        }
        return tr("Rows with cells merged across rows cannot be sorted");
    }
    const auto [first, last] = selectedBlockRange();
    if (first == last) {
        return tr("Select the paragraphs to sort, or put the cursor in a list");
    }
    return tr("Only paragraphs of text can be sorted: the selection holds a table or an object");
}

void BookEditor::sortSelection(SortOptions options)
{
    if (refusesChange()) {
        return;
    }
    if (options.language.isEmpty() && m_textLanguage) {
        options.language = m_textLanguage();
    }
    if (options.keys.empty()) {
        options.keys.push_back(SortKey{});
    }
    const bool cellsSelected = selectedCellRange().has_value();
    const bool textSelected = hasSelection();

    if (isInTable()) {
        const std::optional<TableCellRange> rows = sortedRows();
        if (!rows) {
            return;
        }
        QTextTable& table = *rows->table;
        std::vector<QStringList> texts;
        for (int row = rows->firstRow; row <= rows->lastRow; ++row) {
            QStringList columns;
            for (int column = 0; column < table.columns(); ++column) {
                const QTextTableCell cell = table.cellAt(row, column);
                columns << (cell.row() == row && cell.column() == column
                                ? cellText(cell, QLatin1Char(' '))
                                : QString());
            }
            texts.push_back(columns);
        }
        const std::vector<int> order = sortedOrder(texts, options);
        std::vector<int> unchanged(order.size());
        std::iota(unchanged.begin(), unchanged.end(), 0);
        if (order == unchanged) {
            return;
        }
        // The cursor goes to the start of the sorted rows; the selected cells stay selected
        const std::optional<TableCellRange> selected = selectedCellRange();
        QTextCursor cursor(m_textBuffer.get());
        beginTableStep(cursor);
        reorderTableRows(cursor, table, rows->firstRow, order);
        if (cellsSelected && selected) {
            const auto [from, to] = cornersOf(table, selected->firstRow, selected->firstColumn,
                                              selected->lastRow, selected->lastColumn);
            endTableStep(cursor, to, from);
        } else {
            endTableStep(cursor, table.cellAt(rows->firstRow, 0).firstPosition());
        }
        return;
    }

    const std::optional<std::pair<int, int>> paragraphs = sortedParagraphs();
    if (!paragraphs) {
        return;
    }
    const auto [first, last] = *paragraphs;
    std::vector<QStringList> texts;
    for (int number = first; number <= last; ++number) {
        texts.push_back({m_textBuffer->findBlockByNumber(number).text()});
    }
    for (SortKey& key : options.keys) {
        key.column = 0;
    }
    const std::vector<int> order = sortedOrder(texts, options);
    std::vector<int> unchanged(order.size());
    std::iota(unchanged.begin(), unchanged.end(), 0);
    if (order == unchanged) {
        return;
    }
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    reorderParagraphs(cursor, first, order);
    // The sorted paragraphs selected, as they were (the cursor's list: the cursor at its start)
    const QTextBlock firstBlock = m_textBuffer->findBlockByNumber(first);
    const QTextBlock lastBlock = m_textBuffer->findBlockByNumber(last);
    if (textSelected) {
        endTableStep(cursor, lastBlock.position() + lastBlock.length() - 1, firstBlock.position());
    } else {
        endTableStep(cursor, firstBlock.position());
    }
}

// =============================================================================
// Moving rows and paragraphs
// =============================================================================

bool BookEditor::moveParagraphsOrRows(bool down)
{
    if (refusesChange() || !m_textBuffer) {
        return false;
    }
    if (const std::optional<TableCellRange> cells = tableCells()) {
        QTextTable& table = *cells->table;
        const int neighbour = down ? cells->lastRow + 1 : cells->firstRow - 1;
        if (neighbour < 0 || neighbour >= table.rows()) {
            return false;
        }
        // A cell spanning the neighbour row, or one of the rows, holds them together
        const int lowest = std::min(cells->firstRow, neighbour);
        const int highest = std::max(cells->lastRow, neighbour);
        const int header = table.format().headerRowCount();
        if ((cells->firstRow < header) != (neighbour < header) ||
            hasRowSpans(table, lowest, highest)) {
            return false;
        }
        // The rows of the cells and the neighbour change places
        const int count = cells->rows();
        std::vector<int> order;
        if (down) {
            order.push_back(count);
            for (int i = 0; i < count; ++i) {
                order.push_back(i);
            }
        } else {
            for (int i = 1; i <= count; ++i) {
                order.push_back(i);
            }
            order.push_back(0);
        }

        // The cursor and the selection go with the rows, at the same place of their cells
        const int shift = down ? 1 : -1;
        const auto placeInCell = [&table](int position) {
            const QTextTableCell cell = table.cellAt(position);
            return std::tuple{cell.row(), cell.column(), position - cell.firstPosition()};
        };
        const int position = calculateAbsolutePosition(m_cursorPosition);
        const int anchorPosition =
            calculateAbsolutePosition(hasSelection() ? m_selectionAnchor : m_cursorPosition);
        const auto [cursorRow, cursorColumn, cursorOffset] = placeInCell(position);
        const auto [anchorRow, anchorColumn, anchorOffset] = placeInCell(anchorPosition);

        QTextCursor cursor(m_textBuffer.get());
        beginTableStep(cursor);
        reorderTableRows(cursor, table, lowest, order);
        const auto placeAfter = [&table, shift](int row, int column, int offset) {
            const QTextTableCell cell = table.cellAt(row + shift, column);
            return std::min(cell.firstPosition() + offset, cell.lastPosition());
        };
        endTableStep(cursor, placeAfter(cursorRow, cursorColumn, cursorOffset),
                     placeAfter(anchorRow, anchorColumn, anchorOffset));
        return true;
    }

    // Paragraphs outside a table, past the paragraph, the object or the table next to them
    const auto [first, last] = selectedBlockRange();
    for (int number = first; number <= last; ++number) {
        if (tableOf(m_textBuffer->findBlockByNumber(number))) {
            return false;
        }
    }
    const QTextBlock neighbour = down ? m_textBuffer->findBlockByNumber(last).next()
                                      : m_textBuffer->findBlockByNumber(first).previous();
    if (!neighbour.isValid()) {
        return false;  // the start or the end of the text
    }
    const CursorPosition cursorAt = m_cursorPosition;
    const CursorPosition anchorAt = hasSelection() ? m_selectionAnchor : m_cursorPosition;
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    const int moved = std::max(0, moveParagraphs(cursor, first, last, down));
    const int shift = moved - first;
    const auto placeAfter = [this, shift](const CursorPosition& place) {
        const QTextBlock block = m_textBuffer->findBlockByNumber(place.paragraph + shift);
        return block.position() + std::min(place.offset, block.length() - 1);
    };
    endTableStep(cursor, placeAfter(cursorAt), placeAfter(anchorAt));
    return true;
}

// =============================================================================
// Text to table and back
// =============================================================================

QStringList BookEditor::paragraphsForTable() const
{
    if (!m_textBuffer || isInTable()) {
        return {};
    }
    const auto [first, last] = selectedBlockRange();
    QStringList texts;
    for (int number = first; number <= last; ++number) {
        const QTextBlock block = m_textBuffer->findBlockByNumber(number);
        if (!block.isValid() || tableOf(block) || blockObject(block) != TextObject::None) {
            return {};
        }
        texts << block.text();
    }
    return texts;
}

void BookEditor::convertTextToTable(QChar separator, int columns, bool headerRow)
{
    if (refusesChange() || paragraphsForTable().isEmpty()) {
        return;
    }
    const auto [first, last] = selectedBlockRange();
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    const QTextTable* table =
        textToTable(cursor, first, last, separator, std::clamp(columns, 1, MAX_INSERTED_TABLE_COLUMNS),
                    headerRow, paragraphBlockFormat());
    endTableStep(cursor, table ? table->cellAt(0, 0).firstPosition() : cursor.position());
}

std::vector<QStringList> BookEditor::tableTexts() const
{
    std::vector<QStringList> rows;
    const std::optional<TableCellRange> cells = tableCells();
    if (!cells) {
        return rows;
    }
    const QTextTable& table = *cells->table;
    for (int row = 0; row < table.rows(); ++row) {
        QStringList texts;
        for (int column = 0; column < table.columns(); ++column) {
            const QTextTableCell cell = table.cellAt(row, column);
            if (cell.row() == row && cell.column() == column) {
                texts << cellText(cell, QChar(QChar::LineSeparator));
            }
        }
        rows.push_back(texts);
    }
    return rows;
}

void BookEditor::convertTableToText(QChar separator)
{
    if (refusesChange()) {
        return;
    }
    const std::optional<TableCellRange> cells = tableCells();
    if (!cells) {
        return;
    }
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    const int first = tableToText(cursor, *cells->table, separator);
    endTableStep(cursor, m_textBuffer->findBlockByNumber(first).position());
}

// =============================================================================
// Properties
// =============================================================================

std::optional<TablePropertiesData> BookEditor::tablePropertiesData() const
{
    const std::optional<TableCellRange> cells = tableCells();
    const auto* layout =
        m_textBuffer ? qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())
                     : nullptr;
    if (!cells || !layout || layout->pixelsPerPoint() <= 0) {
        return std::nullopt;
    }
    const QTextTable& table = *cells->table;
    const qreal pixels = layout->pixelsPerPoint();
    TablePropertiesData data;
    data.table = tablePropertiesOf(table.format());
    data.textWidth = std::round(m_textBuffer->textWidth() / pixels * 100.0) / 100.0;
    const std::vector<qreal> edges = layout->tableColumnEdges(table);
    for (size_t column = 0; column + 1 < edges.size(); ++column) {
        data.columnWidths.push_back((edges[column + 1] - edges[column]) / pixels);
    }
    for (int row = 0; row < table.rows(); ++row) {
        data.rows.push_back(rowPropertiesOf(table, row));
    }
    data.headerRows = std::clamp(table.format().headerRowCount(), 0, table.rows());
    data.row = cells->firstRow;
    data.column = cells->firstColumn;
    data.cell = cellPropertiesOf(table.cellAt(cells->firstRow, cells->firstColumn));
    return data;
}

void BookEditor::setTablePropertiesData(const TablePropertiesData& data)
{
    if (refusesChange()) {
        return;
    }
    const std::optional<TableCellRange> cells = tableCells();
    if (!cells) {
        return;
    }
    QTextTable& table = *cells->table;
    const std::optional<TablePropertiesData> before = tablePropertiesData();
    if (!before) {
        return;
    }
    // What changed: nothing, no undo step
    const bool tableChanged = data.table != before->table || data.headerRows != before->headerRows;
    const bool rowsChanged = data.rows.size() == before->rows.size() && data.rows != before->rows;
    // The widths of the columns, in proportion: only when they changed (a table of equal
    // columns keeps none)
    bool columnsChanged = false;
    if (data.columnWidths.size() == static_cast<size_t>(table.columns())) {
        const double total =
            std::accumulate(data.columnWidths.begin(), data.columnWidths.end(), 0.0);
        const std::vector<double> current = columnWidthsOf(table);
        for (size_t column = 0; total > 0 && column < current.size(); ++column) {
            columnsChanged = columnsChanged || std::abs(data.columnWidths[column] * 100.0 / total -
                                                        current[column]) > 0.05;
        }
    }
    if (!tableChanged && !rowsChanged && !columnsChanged && !data.cellChanged) {
        return;
    }

    QTextCursor anchor = createCursor(m_textBuffer.get(), hasSelection() ? m_selectionAnchor : m_cursorPosition);
    QTextCursor place = createCursor(m_textBuffer.get(), m_cursorPosition);
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    if (tableChanged) {
        QTextTableFormat format = table.format();
        setTableProperties(format, data.table);
        format.setHeaderRowCount(std::clamp(data.headerRows, 0, table.rows()));
        table.setFormat(format);
    }
    for (int row = 0; rowsChanged && row < table.rows(); ++row) {
        if (data.rows[static_cast<size_t>(row)] != before->rows[static_cast<size_t>(row)]) {
            setRowProperties(cursor, table, row, data.rows[static_cast<size_t>(row)]);
        }
    }
    if (columnsChanged) {
        setColumnWidths(table, data.columnWidths);
    }
    if (data.cellChanged) {
        for (const QTextTableCell& cell : startingCellsOf(*cells)) {
            setCellProperties(cursor, cell, data.cell);
        }
    }
    endTableStep(cursor, place.position(), anchor.position());
}

void BookEditor::setColumnEdge(QTextTable& table, int column, qreal x)
{
    if (refusesChange()) {
        return;
    }
    const auto* layout =
        m_textBuffer ? qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())
                     : nullptr;
    if (!layout || column < 1 || column >= table.columns()) {
        return;
    }
    std::vector<qreal> edges = layout->tableColumnEdges(table);
    const qreal narrowest = std::max<qreal>(1.0, MIN_COLUMN_WIDTH * layout->pixelsPerPoint());
    const qreal left = edges[static_cast<size_t>(column - 1)] + narrowest;
    const qreal right = edges[static_cast<size_t>(column + 1)] - narrowest;
    if (left > right) {
        return;
    }
    const qreal edge = std::round(std::clamp(x, left, right));
    if (std::abs(edge - edges[static_cast<size_t>(column)]) < 1.0) {
        return;
    }
    edges[static_cast<size_t>(column)] = edge;
    std::vector<double> widths;
    for (size_t i = 0; i + 1 < edges.size(); ++i) {
        widths.push_back(edges[i + 1] - edges[i]);
    }
    QTextCursor anchor = createCursor(m_textBuffer.get(), hasSelection() ? m_selectionAnchor : m_cursorPosition);
    QTextCursor place = createCursor(m_textBuffer.get(), m_cursorPosition);
    QTextCursor cursor(m_textBuffer.get());
    beginTableStep(cursor);
    setColumnWidths(table, widths);
    endTableStep(cursor, place.position(), anchor.position());
}

}  // namespace kalahari::editor
