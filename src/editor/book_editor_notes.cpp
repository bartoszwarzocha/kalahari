/// @file book_editor_notes.cpp
/// @brief BookEditor: footnotes and endnotes - inserting them, their text, their place

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/editor_render_pipeline.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/paragraph_styles.h>

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QMenu>
#include <QSet>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

/// The room kept between a note scrolled into view and the view's edges, in pixels
constexpr qreal NOTE_SCROLL_MARGIN = 30.0;

/// @brief The layout of a document (nullptr: none, or not Kalahari's)
KalahariTextDocumentLayout* layoutOf(const QTextDocument* document) {
    return document != nullptr
               ? qobject_cast<KalahariTextDocumentLayout*>(document->documentLayout())
               : nullptr;
}

}  // namespace

QString BookEditor::insertNote(NoteKind kind)
{
    if (refusesChange()) {
        return {};
    }
    ensureDocument();
    QTextDocument* document = m_textBuffer.get();

    // After the selection, which stays text (as in Word): one undo step that brings the
    // selection back
    const StepCursor before{m_cursorPosition, m_selection};
    int position = calculateAbsolutePosition(m_cursorPosition);
    if (hasSelection()) {
        position = calculateAbsolutePosition(m_selection.normalized().end);
        clearSelection();
    }
    QTextCursor step(document);
    step.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, before] { m_stepCursor = before; }));
    QTextCursor cursor(document);
    cursor.setPosition(position);
    leaveObject(cursor);  // never in the block of a line or a break

    Footnote note;
    note.id = newFootnoteId(noteIds(*document));
    note.kind = kind;
    QTextCharFormat format = insertionFormat(cursor);
    setFootnote(format, note);
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), format);
    const CursorPosition after = absoluteToCursorPosition(document, cursor.position());
    document->appendUndoItem(
        new CallbackUndoItem([this, state = StepCursor{after, {}}] { m_stepCursor = state; }));
    step.endEditBlock();
    m_learnableChange.reset();
    m_insertedNote = InsertedNote{note.id, document->availableUndoSteps()};

    m_selectionAnchor = after;
    finishEdit(cursor);
    return note.id;
}

std::optional<NotePlace> BookEditor::noteOf(const QString& id) const
{
    if (!m_textBuffer || id.isEmpty()) {
        return std::nullopt;
    }
    return findNote(*m_textBuffer, id);
}

QString BookEditor::noteTitle(NoteKind kind, const QString& number)
{
    //: The note in its frame, its tooltip and its menu; %1 is its number. In Polish: Przypis
    //: końcowy %1
    return kind == NoteKind::Endnote ? tr("Endnote %1").arg(number)
                                     //: In Polish: Przypis dolny %1
                                     : tr("Footnote %1").arg(number);
}

QString BookEditor::noteTextAt(const QPointF& widgetPos) const
{
    const KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    if (layout == nullptr || !m_renderPipeline) {
        return {};
    }
    const std::optional<KalahariTextDocumentLayout::NoteArea> area =
        layout->noteAreaAt(m_renderPipeline->widgetToDocument(widgetPos));
    return area ? area->id : QString();
}

void BookEditor::addNoteActions(QMenu& menu, const NotePlace& note)
{
    const QString id = note.note.id;
    const bool endnote = note.note.kind == NoteKind::Endnote;
    QAction* edit = menu.addAction(
        //: The context menu of a note's number. In Polish: &Edytuj przypis
        endnote ? tr("&Edit Endnote")
                //: In Polish: &Edytuj przypis
                : tr("&Edit Footnote"),
        this, [this, id]() { emit noteEditRequested(id); });
    disableWhileReadOnly(edit);
    QAction* convert = menu.addAction(
        //: In Polish: &Zamień na przypis dolny
        endnote ? tr("Con&vert to Footnote")
                //: In Polish: &Zamień na przypis końcowy
                : tr("Con&vert to Endnote"),
        this, [this, id, endnote]() {
            setNoteKind(id, endnote ? NoteKind::Footnote : NoteKind::Endnote);
        });
    disableWhileReadOnly(convert);
    QAction* remove = menu.addAction(
        //: In Polish: &Usuń przypis
        endnote ? tr("Delete Endn&ote")
                //: In Polish: &Usuń przypis
                : tr("Delete F&ootnote"),
        this, [this, id]() { removeNote(id); });
    disableWhileReadOnly(remove);
}

QString BookEditor::noteNumber(const QString& id) const
{
    const KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    return layout != nullptr ? layout->footnoteNumbers().numberOf(id) : QString();
}

std::optional<NotePlace> BookEditor::noteAtCursor() const
{
    if (!m_textBuffer) {
        return std::nullopt;
    }
    // The number selected alone, or the one just before the cursor
    int position = calculateAbsolutePosition(m_cursorPosition) - 1;
    if (hasSelection()) {
        const SelectionRange range = m_selection.normalized();
        const int from = calculateAbsolutePosition(range.start);
        if (calculateAbsolutePosition(range.end) != from + 1) {
            return std::nullopt;
        }
        position = from;
    }
    if (position < 0) {
        return std::nullopt;
    }
    QTextCursor cursor(m_textBuffer.get());
    cursor.setPosition(position + 1);
    const std::optional<Footnote> note = footnoteOf(cursor.charFormat());
    if (!note || m_textBuffer->characterAt(position) != QChar::ObjectReplacementCharacter) {
        return std::nullopt;
    }
    return NotePlace{position, *note};
}

std::optional<NotePlace> BookEditor::noteAt(const QPointF& widgetPos) const
{
    const KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    if (layout == nullptr || !m_renderPipeline) {
        return std::nullopt;
    }
    const QPointF point = m_renderPipeline->widgetToDocument(widgetPos);

    // Its text: under its paragraph, at the bottom of a page, after the last paragraph
    const std::optional<KalahariTextDocumentLayout::NoteArea> area = layout->noteAreaAt(point);
    if (area) {
        return noteOf(area->id);
    }

    // Its number: the character under the point (the layout's exact hit)
    const int hit = m_textBuffer->documentLayout()->hitTest(point, Qt::ExactHit);
    if (hit < 0) {
        return std::nullopt;
    }
    if (const NotePlace* place = layout->footnoteNumbers().noteAt(hit)) {
        return *place;
    }
    return std::nullopt;
}

bool BookEditor::setNoteContent(const QString& id, const QString& content, bool joinInsertion)
{
    if (m_readOnly || !m_textBuffer) {
        return false;
    }
    const std::optional<NotePlace> place = findNote(*m_textBuffer, id);
    if (!place) {
        return false;
    }
    if (place->note.content == content) {
        return true;
    }
    QTextDocument* document = m_textBuffer.get();

    // A part of the step of its insertion while nothing was done since; else a step of its own
    const bool join = joinInsertion && m_insertedNote && m_insertedNote->id == id &&
                      m_insertedNote->undoSteps == document->availableUndoSteps() &&
                      document->availableRedoSteps() == 0;
    QTextCursor step(document);
    if (join) {
        step.joinPreviousEditBlock();
    } else {
        step.beginEditBlock();
        document->appendUndoItem(new CallbackUndoItem(
            [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
    }
    QTextCursor cursor(document);
    cursor.setPosition(place->position);
    cursor.setPosition(place->position + 1, QTextCursor::KeepAnchor);
    QTextCharFormat format = cursor.charFormat();
    Footnote note = place->note;
    note.content = content;
    setFootnote(format, note);
    cursor.setCharFormat(format);
    step.endEditBlock();
    if (m_insertedNote && m_insertedNote->id == id) {
        m_insertedNote.reset();  // written: no longer new, a change of it is a step of its own
    }
    m_learnableChange.reset();

    update();
    emit contentChanged();
    return true;
}

void BookEditor::cancelNewNote(const QString& id)
{
    if (!m_textBuffer || !findNote(*m_textBuffer, id)) {
        m_insertedNote.reset();
        return;
    }
    const bool undoable = m_insertedNote && m_insertedNote->id == id &&
                          m_insertedNote->undoSteps == m_textBuffer->availableUndoSteps() &&
                          m_textBuffer->availableRedoSteps() == 0;
    m_insertedNote.reset();
    if (undoable) {
        undoWithoutRedo();
    } else {
        removeNote(id);
    }
}

bool BookEditor::setNoteKind(const QString& id, NoteKind kind)
{
    if (refusesChange() || !m_textBuffer) {
        return false;
    }
    const std::optional<NotePlace> place = findNote(*m_textBuffer, id);
    if (!place) {
        return false;
    }
    if (place->note.kind == kind) {
        return true;
    }
    QTextDocument* document = m_textBuffer.get();
    QTextCursor step(document);
    step.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
    QTextCursor cursor(document);
    cursor.setPosition(place->position);
    cursor.setPosition(place->position + 1, QTextCursor::KeepAnchor);
    QTextCharFormat format = cursor.charFormat();
    Footnote note = place->note;
    note.kind = kind;
    setFootnote(format, note);
    cursor.setCharFormat(format);
    step.endEditBlock();
    m_learnableChange.reset();

    update();
    emit contentChanged();
    return true;
}

bool BookEditor::removeNote(const QString& id)
{
    if (refusesChange() || !m_textBuffer) {
        return false;
    }
    const std::optional<NotePlace> place = findNote(*m_textBuffer, id);
    if (!place) {
        return false;
    }
    QTextDocument* document = m_textBuffer.get();

    // The cursor where the number was, the selection gone; undone, both come back
    const StepCursor before{m_cursorPosition, m_selection};
    clearSelection();
    QTextCursor step(document);
    step.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, before] { m_stepCursor = before; }));
    QTextCursor cursor(document);
    cursor.setPosition(place->position);
    cursor.setPosition(place->position + 1, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    const CursorPosition after = absoluteToCursorPosition(document, cursor.position());
    document->appendUndoItem(
        new CallbackUndoItem([this, state = StepCursor{after, {}}] { m_stepCursor = state; }));
    step.endEditBlock();
    m_learnableChange.reset();

    m_selectionAnchor = after;
    finishEdit(cursor);
    return true;
}

void BookEditor::renewPastedNoteIds(int from, int to)
{
    if (!m_textBuffer || from >= to) {
        return;
    }
    // The ids of the notes outside the pasted part; a pasted one of these gets one of its own
    QTextDocument* document = m_textBuffer.get();
    QSet<QString> taken;
    std::vector<NotePlace> pasted;
    for (const NotePlace& place : notesIn(*document)) {
        if (place.position >= from && place.position < to) {
            pasted.push_back(place);
        } else {
            taken.insert(place.note.id);
        }
    }
    QTextCursor cursor(document);
    bool joined = false;
    for (NotePlace& place : pasted) {
        if (!place.note.id.isEmpty() && !taken.contains(place.note.id)) {
            taken.insert(place.note.id);
            continue;
        }
        place.note.id = newFootnoteId(taken);
        taken.insert(place.note.id);
        if (!joined) {
            cursor.joinPreviousEditBlock();  // the paste's undo step
            joined = true;
        }
        cursor.setPosition(place.position);
        cursor.setPosition(place.position + 1, QTextCursor::KeepAnchor);
        QTextCharFormat format = cursor.charFormat();
        setFootnote(format, place.note);
        cursor.setCharFormat(format);
    }
    if (joined) {
        cursor.endEditBlock();
    }
}

QRectF BookEditor::noteRect(const QString& id) const
{
    const KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    if (layout == nullptr || !m_renderPipeline) {
        return {};
    }
    const std::optional<QRectF> rect = layout->noteRect(id);
    if (!rect) {
        return {};
    }
    return QRectF(m_renderPipeline->documentToWidget(rect->topLeft()),
                  m_renderPipeline->documentToWidget(rect->bottomRight()));
}

void BookEditor::setNoteFrameRoom(const QString& id, qreal height)
{
    KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    if (layout == nullptr) {
        return;
    }
    const double scale = m_renderPipeline ? m_renderPipeline->context().computed.viewScale : 1.0;
    layout->setNoteRoom(id, height > 0.0 && scale > 0.0 ? height / scale : 0.0);
    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    update();
}

void BookEditor::scrollToNote(const QString& id)
{
    const QRectF note = noteRect(id);
    if (note.isEmpty() || !m_viewportManager || height() <= 0) {
        return;
    }

    // Its bottom comes into view, then its top; the cursor's line stays in view when both fit
    const qreal topEdge = NOTE_SCROLL_MARGIN + findReplaceBarHeight();
    const qreal bottomEdge = height() - NOTE_SCROLL_MARGIN;
    qreal delta = std::max<qreal>(0.0, note.bottom() - bottomEdge);
    delta = std::min(delta, note.top() - topEdge);
    if (delta > 0.0 && m_renderPipeline) {
        const QRectF caret = m_renderPipeline->cursorRect();
        if (!caret.isEmpty() && note.bottom() - caret.top() <= bottomEdge - topEdge) {
            delta = std::max<qreal>(0.0, std::min(delta, caret.top() - topEdge));
        }
    }
    if (std::abs(delta) >= 1.0) {
        setScrollOffset(scrollOffset() + delta / m_viewportManager->viewScale());
    }
}

QFont BookEditor::noteFrameFont() const
{
    QFont font = m_textBuffer ? m_textBuffer->defaultFont() : m_appearance.typography.textFont;
    const double scale = m_renderPipeline ? m_renderPipeline->context().computed.viewScale : 1.0;
    if (font.pixelSize() > 0) {
        font.setPixelSize(std::max(1, static_cast<int>(std::lround(font.pixelSize() * scale))));
    } else if (font.pointSizeF() > 0.0) {
        font.setPointSizeF(font.pointSizeF() * scale);
    }
    return font;
}

}  // namespace kalahari::editor
