/// @file book_editor_editing.cpp
/// @brief BookEditor: typing and deleting, undo and redo, the clipboard

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/core/logger.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/kml_serializer.h>
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

namespace kalahari::editor {

/// @brief Get paragraph text
/// @param doc QTextDocument pointer
/// @param index Block/paragraph index
/// @return Text content of the paragraph
inline QString paragraphText(QTextDocument* doc, int index) {
    if (!doc) return QString();
    QTextBlock block = doc->findBlockByNumber(index);
    return block.isValid() ? block.text() : QString();
}

namespace {

/// @brief Clipboard text as the editor can store and save it
///
/// Line and paragraph breaks of every platform become paragraph breaks. Characters XML
/// cannot hold (control characters other than tab, unpaired surrogates, U+FFFE, U+FFFF)
/// are dropped, so pasted text cannot make the chapter file unreadable.
QString pastedPlainText(const QString& text) {
    QString result;
    result.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        const char16_t code = ch.unicode();
        if (code == u'\r' && i + 1 < text.size() && text.at(i + 1) == u'\n') {
            continue;  // the \n that follows makes the break
        }
        if (code == u'\n' || code == u'\r' || code == u'\v' || code == u'\f' ||
            code == QChar::LineSeparator || code == QChar::ParagraphSeparator) {
            result += u'\n';
        } else if (ch.isHighSurrogate() && i + 1 < text.size() && text.at(i + 1).isLowSurrogate()) {
            result += ch;
            result += text.at(++i);
        } else if ((code >= 0x20 || code == u'\t') && !ch.isSurrogate() && code != 0xFFFE &&
                   code != 0xFFFF) {
            result += ch;
        }
    }
    return result;
}

/// @brief Insert a document at the cursor, replacing its selection, as one edit block
///
/// Each fragment keeps its character format. Paragraphs inserted whole keep their block
/// format; the paragraph the document goes into keeps its own, also on the text after the
/// insertion point. The cursor ends after the inserted text.
void insertDocument(QTextCursor& cursor, const QTextDocument& source) {
    cursor.beginEditBlock();
    cursor.removeSelectedText();

    const QTextBlockFormat targetFormat = cursor.blockFormat();
    const bool atParagraphStart = cursor.atBlockStart();
    const bool atParagraphEnd = cursor.atBlockEnd();
    const int insertionStart = cursor.position();
    const QTextBlock firstBlock = source.firstBlock();
    const QTextBlock lastBlock = source.lastBlock();

    for (QTextBlock block = firstBlock; block.isValid(); block = block.next()) {
        if (block != firstBlock) {
            // The last inserted paragraph also holds the text after the insertion point,
            // unless there is none
            const bool whole = block != lastBlock || (atParagraphEnd && block.length() > 1);
            cursor.insertBlock(whole ? block.blockFormat() : targetFormat, block.charFormat());
        }
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            cursor.insertText(fragment.text(), fragment.charFormat());
        }
    }

    // The first inserted paragraph is whole when it starts a paragraph and more follow
    if (firstBlock != lastBlock && atParagraphStart && firstBlock.length() > 1) {
        QTextCursor first(cursor.document());
        first.setPosition(insertionStart);
        first.setBlockFormat(firstBlock.blockFormat());
    }
    cursor.endEditBlock();
}

/// @brief Insert MIME data at the cursor, replacing its selection, as one edit block
///
/// Kalahari content (KML) is parsed like a chapter file, so formatting and alignment
/// survive; text from other programs takes the formatting of the insertion point. Only the
/// document changes: the editor's cursor and view follow once the outermost edit block has
/// ended. The cursor ends after the inserted text.
void insertMimeData(QTextCursor& cursor, const QMimeData& source) {
    if (source.hasFormat(QString::fromLatin1(MIME_KML))) {
        KmlDocumentModel model;
        if (model.loadKml(QString::fromUtf8(source.data(QString::fromLatin1(MIME_KML)))) &&
            model.paragraphCount() > 0) {
            QTextDocument content;
            content.setUndoRedoEnabled(false);
            QTextCursor contentCursor(&content);
            appendParagraphs(contentCursor, model);
            insertDocument(cursor, content);
            return;
        }
        core::Logger::getInstance().warn("BookEditor: unreadable KML, inserting the text");
    }

    cursor.beginEditBlock();  // the replaced selection and the text: one undo step
    cursor.insertText(pastedPlainText(source.text()));
    cursor.endEditBlock();
}

}  // anonymous namespace

// =============================================================================
// Text Input (Phase 4.1 - 4.4)
// =============================================================================

void BookEditor::insertText(const QString& text)
{
    if (text.isEmpty()) {
        return;
    }

    ensureDocument();

    // Direct QTextCursor edit — recorded by QTextDocument's native undo.
    QTextCursor cursor(m_textBuffer.get());
    if (hasSelection()) {
        SelectionRange sel = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
        cursor.beginEditBlock();  // one undo step for the replace (delete + insert)
        cursor.insertText(text);  // replaces the selection
        cursor.endEditBlock();
        clearSelection();
    } else {
        cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
        cursor.insertText(text);
    }

    // Mirror the resulting QTextCursor into the editor's cursor model.
    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();

    ensureCursorVisible();
    syncPipelineCursor();
    update();
    emit contentChanged();
    emit paragraphModified(m_cursorPosition.paragraph);
}

bool BookEditor::deleteSelectedText()
{
    if (!hasSelection()) {
        return false;
    }

    ensureDocument();

    SelectionRange sel = m_selection.normalized();

    // Direct QTextCursor delete — recorded by QTextDocument's native undo.
    QTextCursor cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
    cursor.removeSelectedText();

    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();
    clearSelection();

    update();
    emit contentChanged();
    return true;
}

void BookEditor::insertNewline()
{
    ensureDocument();

    // Split the paragraph via a direct QTextCursor insertBlock() — recorded by
    // QTextDocument's native undo, together with the replaced selection as one step.
    // insertBlock() inherits the current block format (zero margins + alignment), so the
    // new paragraph keeps the same layout.
    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    if (hasSelection()) {
        const SelectionRange sel = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
        clearSelection();
    }
    cursor.beginEditBlock();
    cursor.removeSelectedText();
    cursor.insertBlock();
    cursor.endEditBlock();

    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();

    ensureCursorVisible();
    syncPipelineCursor();
    update();
    emit contentChanged();
    emit paragraphInserted(m_cursorPosition.paragraph);
}

void BookEditor::deleteBackward()
{
    ensureDocument();

    if (hasSelection()) {
        deleteSelectedText();
        return;
    }

    // If at start of document, nothing to delete
    if (m_cursorPosition.paragraph == 0 && m_cursorPosition.offset == 0) {
        return;
    }

    const int oldPara = m_cursorPosition.paragraph;
    const bool wasAtBlockStart = (m_cursorPosition.offset == 0);

    // deletePreviousChar() removes the previous character, OR merges with the previous
    // paragraph when at the start of a block. Recorded by QTextDocument's native undo.
    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    cursor.deletePreviousChar();

    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();

    ensureCursorVisible();
    syncPipelineCursor();
    update();
    emit contentChanged();
    if (wasAtBlockStart) {
        emit paragraphRemoved(oldPara);
    } else {
        emit paragraphModified(m_cursorPosition.paragraph);
    }
}

void BookEditor::deleteForward()
{
    ensureDocument();

    if (hasSelection()) {
        deleteSelectedText();
        return;
    }

    const int paraLen = paragraphText(m_textBuffer.get(), m_cursorPosition.paragraph).length();
    const bool atBlockEnd = (m_cursorPosition.offset >= paraLen);
    const bool hasNextBlock = (m_cursorPosition.paragraph + 1 < m_textBuffer->blockCount());

    if (!atBlockEnd || hasNextBlock) {
        // deleteChar() removes the character at the cursor, OR merges with the next
        // paragraph at the end of a block. Recorded by QTextDocument's native undo.
        QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
        cursor.deleteChar();

        m_cursorPosition.paragraph = cursor.blockNumber();
        m_cursorPosition.offset = cursor.positionInBlock();

        syncPipelineCursor();
        update();
        emit contentChanged();
        if (atBlockEnd) {
            emit paragraphRemoved(m_cursorPosition.paragraph + 1);
        } else {
            emit paragraphModified(m_cursorPosition.paragraph);
        }
    }
}

// =============================================================================
// Undo/Redo (Phase 4.8)
// =============================================================================

bool BookEditor::canUndo() const
{
    return m_textBuffer && m_textBuffer->isUndoAvailable();
}

bool BookEditor::canRedo() const
{
    return m_textBuffer && m_textBuffer->isRedoAvailable();
}

void BookEditor::undo()
{
    if (!m_textBuffer || !m_textBuffer->isUndoAvailable()) {
        return;
    }

    // QTextDocument's native undo is the single source of truth for BOTH text and
    // formatting. undo(&cursor) also positions the cursor at the change — mirror it
    // into the editor's own cursor model.
    m_stepCursor.reset();
    QTextCursor cursor(m_textBuffer.get());
    m_textBuffer->undo(&cursor);
    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();
    clearSelection();
    restoreStepCursor();

    syncPipelineCursor();
    ensureCursorVisible();
    update();
    emit contentChanged();
    emit cursorPositionChanged(m_cursorPosition);
}

void BookEditor::redo()
{
    if (!m_textBuffer || !m_textBuffer->isRedoAvailable()) {
        return;
    }

    m_stepCursor.reset();
    QTextCursor cursor(m_textBuffer.get());
    m_textBuffer->redo(&cursor);
    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();
    clearSelection();
    restoreStepCursor();

    syncPipelineCursor();
    ensureCursorVisible();
    update();
    emit contentChanged();
    emit cursorPositionChanged(m_cursorPosition);
}

void BookEditor::clearUndoStack()
{
    if (m_textBuffer) {
        m_textBuffer->clearUndoRedoStacks();
    }
}

// =============================================================================
// Clipboard (Phase 4.13-4.16)
// =============================================================================

void BookEditor::copy()
{
    if (std::unique_ptr<QMimeData> mimeData = createMimeDataFromSelection()) {
        QGuiApplication::clipboard()->setMimeData(mimeData.release());  // clipboard takes ownership
    }
}

void BookEditor::cut()
{
    if (!hasSelection() || !m_textBuffer) {
        return;
    }

    // Copy first
    copy();

    // Then delete selection (one undo step)
    deleteSelectedText();
}

void BookEditor::paste()
{
    insertFromMimeData(QGuiApplication::clipboard()->mimeData());
}

std::unique_ptr<QMimeData> BookEditor::createMimeDataFromSelection() const
{
    if (!hasSelection() || !m_textBuffer) {
        return nullptr;
    }

    const SelectionRange sel = m_selection.normalized();
    const QTextCursor range = createCursor(m_textBuffer.get(), sel.start, sel.end);
    const QString kml =
        KmlSerializer().toKml(m_textBuffer.get(), range.selectionStart(), range.selectionEnd());

    // Paragraphs separated by line breaks
    QString text = range.selectedText();
    text.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));

    auto mimeData = std::make_unique<QMimeData>();
    mimeData->setText(text);
    mimeData->setHtml(ClipboardHandler::kmlToHtml(kml));
    mimeData->setData(QString::fromLatin1(MIME_KML), kml.toUtf8());
    return mimeData;
}

bool BookEditor::canInsertFromMimeData(const QMimeData* source)
{
    return source != nullptr &&
           (source->hasFormat(QString::fromLatin1(MIME_KML)) || source->hasText());
}

void BookEditor::insertFromMimeData(const QMimeData* source)
{
    if (!canInsertFromMimeData(source)) {
        return;
    }
    // Text from other programs is cleaned first: nothing to insert, nothing replaced
    if (!source->hasFormat(QString::fromLatin1(MIME_KML)) &&
        pastedPlainText(source->text()).isEmpty()) {
        return;
    }
    ensureDocument();

    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    if (hasSelection()) {
        const SelectionRange sel = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
        clearSelection();
    }
    insertMimeData(cursor, *source);
    finishEdit(cursor);
}

bool BookEditor::dropMimeData(const QMimeData* source, const CursorPosition& position,
                              bool moveSelection)
{
    if (!canInsertFromMimeData(source)) {
        return false;
    }
    ensureDocument();
    if (moveSelection && (!hasSelection() || isInSelection(position))) {
        return false;
    }

    const SelectionRange moved = m_selection.normalized();
    clearSelection();

    // One undo step: moved text leaves its place and lands at the drop point. Both
    // cursors stay at the same text while the other one edits the document.
    QTextCursor cursor = createCursor(m_textBuffer.get(), validateCursorPosition(position));
    QTextCursor oldPlace;
    cursor.beginEditBlock();
    if (moveSelection) {
        oldPlace = createCursor(m_textBuffer.get(), moved.start, moved.end);
        oldPlace.removeSelectedText();
    }
    const int insertionStart = cursor.position();
    insertMimeData(cursor, *source);
    cursor.endEditBlock();

    // The dropped text is selected, with the cursor at its end
    finishEdit(cursor);
    if (!oldPlace.isNull() && oldPlace.blockNumber() != m_cursorPosition.paragraph) {
        emit paragraphModified(oldPlace.blockNumber());
    }
    const QTextBlock startBlock = m_textBuffer->findBlock(insertionStart);
    m_selectionAnchor = {startBlock.blockNumber(), insertionStart - startBlock.position()};
    setSelection({m_selectionAnchor, m_cursorPosition});
    return true;
}

void BookEditor::finishEdit(const QTextCursor& cursor)
{
    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();

    ensureCursorVisible();
    syncPipelineCursor();
    update();
    emit contentChanged();
    emit paragraphModified(m_cursorPosition.paragraph);
}

bool BookEditor::canPaste() const
{
    return ClipboardHandler::canPaste();
}

}  // namespace kalahari::editor
