// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_editor_skeleton.cpp
/// @brief BookEditor: the skeleton of a text for publishers, the headings of the points of its
///        template with their hints

#include <kalahari/editor/book_editor.h>

#include "kalahari/editor/detail/book_editor_internal.h"

#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/text_skeleton.h>

#include <QTextCursor>
#include <QTextDocument>

namespace kalahari::editor {

bool BookEditor::insertTemplateSkeleton(const QList<SkeletonPoint>& points)
{
    if (points.isEmpty() || refusesChange()) {
        return false;
    }
    ensureDocument();
    QTextDocument* document = m_textBuffer.get();
    QTextCursor cursor = createCursor(document, m_cursorPosition);
    if (cursor.currentTable() != nullptr) {
        return false;
    }

    // One undo step, which brings back the cursor and the selection it was made with
    const StepCursor before{.cursor = m_cursorPosition, .selection = m_selection};
    clearSelection();
    QTextCursor step(document);
    step.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, before] { m_stepCursor = before; }));
    cursor.setPosition(insertSkeleton(cursor, points));
    const CursorPosition after = absoluteToCursorPosition(document, cursor.position());
    document->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{.cursor = after, .selection = {}}] { m_stepCursor = state; }));
    step.endEditBlock();
    m_learnableChange.reset();

    m_selectionAnchor = after;
    finishEdit(cursor);
    return true;
}

int BookEditor::removeTemplateSkeleton()
{
    if (!m_textBuffer || !hasSkeleton(*m_textBuffer) || refusesChange()) {
        return 0;
    }
    QTextDocument* document = m_textBuffer.get();

    // One undo step; the cursor follows its character as the paragraphs go
    const StepCursor before{.cursor = m_cursorPosition, .selection = m_selection};
    clearSelection();
    const QTextCursor kept = createCursor(document, m_cursorPosition);
    QTextCursor step(document);
    step.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, before] { m_stepCursor = before; }));
    QTextCursor cursor(document);
    const int removed = removeSkeleton(cursor);
    const CursorPosition after = absoluteToCursorPosition(document, kept.position());
    document->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{.cursor = after, .selection = {}}] { m_stepCursor = state; }));
    step.endEditBlock();
    m_learnableChange.reset();

    m_selectionAnchor = after;
    finishEdit(kept);
    return removed;
}

}  // namespace kalahari::editor
