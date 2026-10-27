/// @file book_editor_internal.h
/// @brief Helpers shared by the BookEditor source files (not part of the editor's API)

#pragma once

#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/text_tables.h>
#include <functional>
#include <utility>
#include <QAbstractUndoItem>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>

namespace kalahari::editor {

/// @brief Get paragraph length (excluding block separator)
/// @param doc QTextDocument pointer
/// @param index Block/paragraph index
/// @return Character count in paragraph (length - 1 to exclude block separator)
inline int paragraphLength(QTextDocument* doc, int index) {
    if (!doc) return 0;
    QTextBlock block = doc->findBlockByNumber(index);
    return block.isValid() ? (block.length() - 1) : 0;
}

/// @brief Undo item that calls back when its step is undone or redone
class CallbackUndoItem final : public QAbstractUndoItem {
public:
    explicit CallbackUndoItem(std::function<void()> callback)
        : m_callback(std::move(callback)) {}

    void undo() override { m_callback(); }
    void redo() override { m_callback(); }

private:
    std::function<void()> m_callback;
};

/// @brief Fill a document from a parsed KML model, starting at the cursor's (empty) block
///
/// Each paragraph gets zero margins, its own alignment, if it has one (without one it is
/// shown with the alignment of its style), its style, its own formatting, its place in a
/// list, the annotations on its start as its own character format, and its text on a clean
/// base format with the run formats on top. The lists get ids the document's lists do not
/// have. An object (a horizontal line, a break) is a block with its character. A table is
/// built with its first paragraph, its paragraphs going into their cells; the paragraph
/// after it (an empty one when the table ends the KML) is the block Qt keeps after a table.
/// Shared by loading a chapter and pasting Kalahari content, so both read KML the same way.
inline void appendParagraphs(QTextCursor& cursor, const KmlDocumentModel& model) {
    QTextBlockFormat zeroMarginFormat;
    zeroMarginFormat.setTopMargin(0);
    zeroMarginFormat.setBottomMargin(0);
    const int listIdOffset = newListId(*cursor.document()) - 1;

    QTextTable* table = nullptr;       // the table being filled
    int tableIndex = -1;               // its index in the model
    std::optional<TableCellPlace> lastCell;  // the cell of the previous paragraph
    bool emptyBlock = true;            // the cursor's block is a new one, without text
    const auto leaveTable = [&]() {
        // Its cells span their rows and columns once their paragraphs have their formats
        spanCells(*table, model.table(static_cast<size_t>(tableIndex)).spans);
        cursor.setPosition(table->lastPosition() + 1);  // the block after it
        cursor.setBlockFormat(zeroMarginFormat);
        cursor.setBlockCharFormat(QTextCharFormat());
        table = nullptr;
        tableIndex = -1;
        lastCell.reset();
        emptyBlock = true;
    };

    for (size_t i = 0; i < model.paragraphCount(); ++i) {
        QTextBlockFormat blockFormat = zeroMarginFormat;
        if (const Qt::Alignment alignment = model.paragraphAlignment(i); alignment) {
            blockFormat.setAlignment(alignment);
        }
        setParagraphStyle(blockFormat, model.paragraphStyle(i));
        mergeParagraphFormat(blockFormat, model.paragraphOwnFormat(i));
        if (std::optional<ListItem> item = model.paragraphList(i)) {
            item->id += listIdOffset;
            setListItem(blockFormat, *item);
        }
        QTextCharFormat paragraphFormat;  // nothing of the previous paragraph's last run
        setAnnotations(paragraphFormat, model.paragraphStartAnnotations(i));

        // A paragraph of a table goes into its cell: the first one of a cell into its
        // (empty) block, the next ones after it
        const std::optional<TableCellPlace> cell = model.paragraphCell(i);
        if (table && (!cell || cell->table != tableIndex)) {
            leaveTable();
        }
        if (cell && !table) {
            if (emptyBlock) {
                cursor.setBlockFormat(zeroMarginFormat);
                cursor.setBlockCharFormat(QTextCharFormat());
            }
            table = insertTable(cursor, model.table(static_cast<size_t>(cell->table)));
            tableIndex = cell->table;
        }
        if (cell && table && lastCell != cell) {
            cursor.setPosition(table->cellAt(cell->row, cell->column).firstPosition());
            emptyBlock = true;
        }
        lastCell = cell;

        if (emptyBlock) {
            cursor.setBlockFormat(blockFormat);
            cursor.setBlockCharFormat(paragraphFormat);
            emptyBlock = false;
        } else {
            cursor.insertBlock(blockFormat, paragraphFormat);
        }

        if (const TextObject object = model.paragraphObject(i); object != TextObject::None) {
            cursor.insertText(QString(QChar::ObjectReplacementCharacter), objectCharFormat(object));
            continue;
        }

        // An EXPLICIT default char format, so the text does not take the format the cursor
        // still carries from the previous paragraph's last run (formatting bled into every
        // following paragraph on reload); the runs then format only their own ranges.
        const int blockStart = cursor.position();
        cursor.insertText(model.paragraphText(i), QTextCharFormat());
        for (const auto& run : model.paragraphFormats(i)) {
            cursor.setPosition(blockStart + static_cast<int>(run.start));
            cursor.setPosition(blockStart + static_cast<int>(run.end), QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(run.format);
        }
        cursor.movePosition(QTextCursor::EndOfBlock);
    }
    if (table) {
        leaveTable();  // the KML ends with the table: the empty paragraph after it
    }
}

}  // namespace kalahari::editor
