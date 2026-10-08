/// @file book_editor_annotations.cpp
/// @brief BookEditor: comments, TODOs and notes anchored to the text

#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/buffer_commands.h>
#include <QDateTime>

namespace kalahari::editor {

std::vector<AnnotationPlace> BookEditor::annotations() const
{
    return m_textBuffer ? annotationsIn(*m_textBuffer) : std::vector<AnnotationPlace>();
}

Annotation BookEditor::addAnnotation(AnnotationKind kind, const QString& text,
                                     const QString& author)
{
    ensureDocument();

    Annotation annotation;
    annotation.kind = kind;
    annotation.text = text;
    annotation.author = author;
    // Whole seconds: the chapter file keeps no more
    annotation.created = QDateTime::currentDateTimeUtc();
    annotation.created = annotation.created.addMSecs(-annotation.created.time().msec());

    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    if (hasSelection()) {
        const SelectionRange sel = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
    }
    annotation = editor::addAnnotation(cursor, annotation);

    update();
    emit contentChanged();
    return annotation;
}

bool BookEditor::updateAnnotation(const Annotation& annotation)
{
    if (!m_textBuffer || !editor::updateAnnotation(*m_textBuffer, annotation)) {
        return false;
    }
    update();
    emit contentChanged();
    return true;
}

bool BookEditor::removeAnnotation(const QString& id)
{
    if (!m_textBuffer || !editor::removeAnnotation(*m_textBuffer, id)) {
        return false;
    }
    update();
    emit contentChanged();
    return true;
}

bool BookEditor::goToAnnotation(const QString& id)
{
    if (!m_textBuffer) {
        return false;
    }
    const std::optional<AnnotationPlace> place = findAnnotation(*m_textBuffer, id);
    if (!place) {
        return false;
    }

    // A fragment is selected, with the cursor at its end; on a place the cursor goes there
    const CursorPosition start = calculateCursorPosition(place->start);
    const CursorPosition end = calculateCursorPosition(place->end);
    clearSelection();
    setCursorPosition(end);
    if (start != end) {
        m_selectionAnchor = start;
        setSelection({start, end});
    }
    ensureCursorVisible();
    return true;
}

bool BookEditor::goToNextTodo()
{
    if (!m_textBuffer) {
        return false;
    }
    const int position = calculateAbsolutePosition(m_cursorPosition);
    for (const AnnotationPlace& place : annotationsIn(*m_textBuffer)) {
        if (place.annotation.kind == AnnotationKind::Todo && !place.annotation.done &&
            place.start > position) {
            return goToAnnotation(place.annotation.id);
        }
    }
    return false;
}

bool BookEditor::goToPreviousTodo()
{
    if (!m_textBuffer) {
        return false;
    }

    // Before the cursor, or before the selected fragment of the TODO the cursor is at
    const int position = hasSelection()
                             ? calculateAbsolutePosition(m_selection.normalized().start)
                             : calculateAbsolutePosition(m_cursorPosition);
    const std::vector<AnnotationPlace> places = annotationsIn(*m_textBuffer);
    for (auto it = places.rbegin(); it != places.rend(); ++it) {
        if (it->annotation.kind == AnnotationKind::Todo && !it->annotation.done &&
            it->start < position) {
            return goToAnnotation(it->annotation.id);
        }
    }
    return false;
}

}  // namespace kalahari::editor
