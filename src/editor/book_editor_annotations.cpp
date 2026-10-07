/// @file book_editor_annotations.cpp
/// @brief BookEditor: comments and TODO/note markers

#include <kalahari/editor/book_editor.h>
#include <kalahari/core/logger.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/kml_comment.h>
#include <QDateTime>
#include <QInputDialog>

namespace kalahari::editor {

// =============================================================================
// Comments (Phase 7.9)
// =============================================================================

void BookEditor::insertComment()
{
    if (!m_textBuffer) {
        return;
    }

    // Must have a selection to add a comment
    if (!hasSelection()) {
        core::Logger::getInstance().debug("BookEditor::insertComment() - no selection, cannot add comment");
        return;
    }

    // Get the normalized selection range
    SelectionRange sel = m_selection.normalized();

    // Comments within a single paragraph are simpler
    if (sel.start.paragraph != sel.end.paragraph) {
        core::Logger::getInstance().debug("BookEditor::insertComment() - multi-paragraph selection not supported");
        return;
    }

    // Show input dialog for comment text
    bool ok = false;
    QString commentText = QInputDialog::getMultiLineText(
        this,
        tr("Insert Comment"),
        tr("Enter comment:"),
        QString(),
        &ok
    );

    if (!ok || commentText.isEmpty()) {
        return;
    }

    // Phase 11: TODO - Comments need QTextCharFormat::UserProperty-based storage
    // For now, stub out comment functionality
    core::Logger::getInstance().debug("BookEditor::insertComment() - not implemented in Phase 11");
    Q_UNUSED(commentText);
    update();
}

void BookEditor::deleteComment(const QString& commentId)
{
    // Phase 11: TODO - Comments need QTextCharFormat::UserProperty-based storage
    core::Logger::getInstance().debug("BookEditor::deleteComment() - not implemented in Phase 11");
    Q_UNUSED(commentId);
}

void BookEditor::editComment(const QString& commentId)
{
    // Phase 11: TODO - Comments need QTextCharFormat::UserProperty-based storage
    core::Logger::getInstance().debug("BookEditor::editComment() - not implemented in Phase 11");
    Q_UNUSED(commentId);
}

QList<KmlComment> BookEditor::commentsInCurrentParagraph() const
{
    // Phase 11: TODO - Comments need QTextCharFormat::UserProperty-based storage
    // For now, return empty list - comment feature requires Phase 12 implementation
    Q_UNUSED(m_cursorPosition);
    return {};
}

void BookEditor::navigateToComment(int paragraphIndex, const QString& commentId)
{
    // Phase 11: TODO - Comments need QTextCharFormat::UserProperty-based storage
    // For now, just move cursor to paragraph start - full comment navigation requires Phase 12
    if (!m_textBuffer) {
        return;
    }

    // Validate paragraph index
    if (paragraphIndex < 0 || paragraphIndex >= m_textBuffer->blockCount()) {
        return;
    }

    // Move cursor to paragraph start (simplified - no comment offset without new storage)
    CursorPosition newPos{paragraphIndex, 0};
    setCursorPosition(newPos);

    ensureCursorVisible();
    emit commentSelected(paragraphIndex, commentId);
    Q_UNUSED(commentId);
}

// =============================================================================
// TODO/Note Markers (Phase 9.12)
// =============================================================================

void BookEditor::addTodoAtCursor(const QString& text)
{
    // Phase 11.6: Markers stored in QTextCharFormat::UserProperty
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    // Calculate absolute position
    int absPos = calculateAbsolutePosition(m_cursorPosition);

    TextMarker marker;
    marker.id = TextMarker::generateId();
    marker.position = absPos;
    marker.length = 1;
    marker.text = text.isEmpty() ? tr("TODO") : text;
    marker.type = MarkerType::Todo;
    marker.completed = false;
    marker.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);

    setMarkerInDocument(m_textBuffer.get(), marker);  // native undo records the char-format change

    update();
}

void BookEditor::addNoteAtCursor(const QString& text)
{
    // Phase 11.6: Markers stored in QTextCharFormat::UserProperty
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    // Calculate absolute position
    int absPos = calculateAbsolutePosition(m_cursorPosition);

    TextMarker marker;
    marker.id = TextMarker::generateId();
    marker.position = absPos;
    marker.length = 1;
    marker.text = text.isEmpty() ? tr("Note") : text;
    marker.type = MarkerType::Note;
    marker.completed = false;
    marker.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);

    setMarkerInDocument(m_textBuffer.get(), marker);  // native undo records the char-format change

    update();
}

void BookEditor::removeMarkerAtCursor()
{
    // Phase 11.6: Use findAllMarkers from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    // Find all markers and filter by position
    auto allMarkers = findAllMarkers(m_textBuffer.get(), std::nullopt);
    for (const auto& marker : allMarkers) {
        if (marker.position == absPos) {
            // Remove the first marker at cursor position (native undo records it).
            removeMarkerFromDocument(m_textBuffer.get(), marker.position, marker.length);
            update();
            return;
        }
    }
}

void BookEditor::toggleTodoAtCursor()
{
    // Phase 11.6: Use findAllMarkers from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    // Find all markers and filter by position and type
    auto allMarkers = findAllMarkers(m_textBuffer.get(), MarkerType::Todo);
    for (const auto& marker : allMarkers) {
        if (marker.position == absPos) {
            // Toggle the TODO completion state directly (native undo records it).
            TextMarker toggled = marker;
            toggled.completed = !toggled.completed;
            setMarkerInDocument(m_textBuffer.get(), toggled);
            update();
            return;
        }
    }
}

void BookEditor::goToNextTodo()
{
    // Phase 11.6: Use findNextMarker from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    auto next = findNextMarker(m_textBuffer.get(), absPos, MarkerType::Todo);
    if (next) {
        CursorPosition newPos = calculateCursorPosition(next->position);
        m_cursorPosition = newPos;
        ensureCursorVisible();
        update();
    }
}

void BookEditor::goToPreviousTodo()
{
    // Phase 11.6: Use findPreviousMarker from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    auto prev = findPreviousMarker(m_textBuffer.get(), absPos, MarkerType::Todo);
    if (prev) {
        CursorPosition newPos = calculateCursorPosition(prev->position);
        m_cursorPosition = newPos;
        ensureCursorVisible();
        update();
    }
}

void BookEditor::goToNextNote()
{
    // Phase 11.6: Use findNextMarker from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    auto next = findNextMarker(m_textBuffer.get(), absPos, MarkerType::Note);
    if (next) {
        CursorPosition newPos = calculateCursorPosition(next->position);
        m_cursorPosition = newPos;
        ensureCursorVisible();
        update();
    }
}

void BookEditor::goToPreviousNote()
{
    // Phase 11.6: Use findPreviousMarker from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    auto prev = findPreviousMarker(m_textBuffer.get(), absPos, MarkerType::Note);
    if (prev) {
        CursorPosition newPos = calculateCursorPosition(prev->position);
        m_cursorPosition = newPos;
        ensureCursorVisible();
        update();
    }
}

void BookEditor::goToNextMarker()
{
    // Phase 11.6: Use findNextMarker from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    auto next = findNextMarker(m_textBuffer.get(), absPos, std::nullopt);  // Any type
    if (next) {
        CursorPosition newPos = calculateCursorPosition(next->position);
        m_cursorPosition = newPos;
        ensureCursorVisible();
        update();
    }
}

void BookEditor::goToPreviousMarker()
{
    // Phase 11.6: Use findPreviousMarker from buffer_commands.h
    if (!m_textBuffer || !m_textBuffer.get()) {
        return;
    }

    int absPos = calculateAbsolutePosition(m_cursorPosition);

    auto prev = findPreviousMarker(m_textBuffer.get(), absPos, std::nullopt);  // Any type
    if (prev) {
        CursorPosition newPos = calculateCursorPosition(prev->position);
        m_cursorPosition = newPos;
        ensureCursorVisible();
        update();
    }
}

}  // namespace kalahari::editor
