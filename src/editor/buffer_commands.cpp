/// @file buffer_commands.cpp
/// @brief Cursor positions in a QTextDocument

#include <kalahari/editor/buffer_commands.h>
#include <QTextBlock>

namespace kalahari::editor {

// =============================================================================
// Helper Functions
// =============================================================================

int calculateAbsolutePosition(const QTextDocument* document, int blockNumber, int offset)
{
    if (!document) {
        return 0;
    }

    QTextBlock block = document->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        // Return end of document if block doesn't exist
        return document->characterCount() - 1;
    }

    return block.position() + offset;
}

int calculateAbsolutePosition(const QTextDocument* document, const CursorPosition& pos)
{
    return calculateAbsolutePosition(document, pos.paragraph, pos.offset);
}

CursorPosition absoluteToCursorPosition(const QTextDocument* document, int absolutePos)
{
    if (!document) {
        return CursorPosition{0, 0};
    }

    QTextBlock block = document->findBlock(absolutePos);
    if (!block.isValid()) {
        // Clamp to end of document
        QTextBlock lastBlock = document->lastBlock();
        return CursorPosition{
            lastBlock.blockNumber(),
            lastBlock.length() - 1  // -1 for block separator
        };
    }

    return CursorPosition{
        block.blockNumber(),
        absolutePos - block.position()
    };
}

QTextCursor createCursor(QTextDocument* document, const CursorPosition& pos)
{
    if (!document) {
        return QTextCursor();
    }

    int absPos = calculateAbsolutePosition(document, pos);
    QTextCursor cursor(document);
    cursor.setPosition(absPos);
    return cursor;
}

QTextCursor createCursor(QTextDocument* document, const CursorPosition& start, const CursorPosition& end)
{
    if (!document) {
        return QTextCursor();
    }

    int startPos = calculateAbsolutePosition(document, start);
    int endPos = calculateAbsolutePosition(document, end);

    QTextCursor cursor(document);
    cursor.setPosition(startPos);
    cursor.setPosition(endPos, QTextCursor::KeepAnchor);
    return cursor;
}

}  // namespace kalahari::editor
