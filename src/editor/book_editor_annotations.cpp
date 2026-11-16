// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_editor_annotations.cpp
/// @brief BookEditor: comments, TODOs and notes anchored to the text

#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/editor_types.h>
#include "book_editor_internal.h"
#include <QAction>
#include <QDateTime>
#include <QMenu>
#include <QTextBlock>

#include <algorithm>
#include <utility>

namespace kalahari::editor {

std::vector<AnnotationPlace> BookEditor::annotations() const
{
    return m_textBuffer ? annotationsIn(*m_textBuffer) : std::vector<AnnotationPlace>();
}

Annotation BookEditor::addAnnotation(AnnotationKind kind, const QString& text,
                                     const QString& author)
{
    ensureDocument();
    const QTextCursor selection = selectionCursor();
    return addAnnotation(selection.selectionStart(), selection.selectionEnd(), kind, text, author);
}

Annotation BookEditor::addAnnotation(int from, int to, AnnotationKind kind, const QString& text,
                                     const QString& author)
{
    if (!m_annotationsAllowed || m_readOnly) {
        return {};  // a plain text file keeps no annotations; a read-only text takes none
    }
    ensureDocument();

    Annotation annotation;
    annotation.kind = kind;
    annotation.text = text;
    annotation.author = author;
    // Whole seconds: the chapter file keeps no more
    annotation.created = QDateTime::currentDateTimeUtc();
    annotation.created = annotation.created.addMSecs(-annotation.created.time().msec());

    const int last = m_textBuffer->characterCount() - 1;
    QTextCursor cursor(m_textBuffer.get());
    cursor.setPosition(std::clamp(from, 0, last));
    cursor.setPosition(std::clamp(to, 0, last), QTextCursor::KeepAnchor);
    QTextCursor step(m_textBuffer.get());
    beginAnnotationStep(step, false);
    annotation = editor::addAnnotation(cursor, annotation);
    step.endEditBlock();

    update();
    emit contentChanged();
    return annotation;
}

QTextCursor BookEditor::selectionCursor() const
{
    if (!m_textBuffer) {
        return QTextCursor();
    }
    if (hasSelection()) {
        const SelectionRange sel = m_selection.normalized();
        return createCursor(m_textBuffer.get(), sel.start, sel.end);
    }
    return createCursor(m_textBuffer.get(), m_cursorPosition);
}

QRectF BookEditor::placeRect(int position, bool afterText) const
{
    if (!m_textBuffer || !m_renderPipeline) {
        return QRectF();
    }
    const int last = m_textBuffer->characterCount() - 1;
    return m_renderPipeline->placeRect(calculateCursorPosition(std::clamp(position, 0, last)),
                                       afterText);
}

QRectF BookEditor::textColumnRect(double y) const
{
    if (!m_renderPipeline) {
        return QRectF(rect());
    }
    // Of the page there: mirror margins shift the text of every other page
    const auto& computed = m_renderPipeline->context().computed;
    const double left = m_renderPipeline->documentToWidget(
        QPointF(0.0, m_renderPipeline->widgetToDocument(QPointF(0.0, y)).y())).x();
    return QRectF(left, 0.0, computed.textWidth * computed.viewScale, height());
}

QString BookEditor::annotationMarkAt(const QPointF& point) const
{
    return m_renderPipeline ? m_renderPipeline->annotationMarkAt(point) : QString();
}

// =============================================================================
// Marks beside the paragraphs
// =============================================================================

void BookEditor::setParagraphMarks(const QList<ParagraphMark>& marks, const QIcon& icon,
                                   ParagraphMarkKind kind)
{
    m_paragraphMarkIcons.insert(kind, icon);
    std::erase_if(m_paragraphMarks,
                  [kind](const AnchoredMark& each) { return each.mark.kind == kind; });
    QList<ParagraphMark> ofKind = marks;
    for (ParagraphMark& mark : ofKind) {
        mark.kind = kind;
    }
    anchorParagraphMarks(ofKind);
    syncParagraphMarks(true);
}

QList<ParagraphMark> BookEditor::paragraphMarks() const
{
    QList<ParagraphMark> marks;
    for (const AnchoredMark& each : m_paragraphMarks) {
        if (each.anchor.isNull()) {
            continue;
        }
        const int paragraph = each.anchor.block().blockNumber();
        // A paragraph that took in the next one (the text between them deleted) has the
        // marks of both
        const auto same = std::find_if(marks.begin(), marks.end(),
                                       [paragraph, &each](const ParagraphMark& mark) {
                                           return mark.paragraph == paragraph &&
                                                  mark.kind == each.mark.kind;
                                       });
        if (same == marks.end()) {
            ParagraphMark mark = each.mark;
            mark.paragraph = paragraph;
            marks.append(mark);
            continue;
        }
        for (qsizetype i = 0; i < each.mark.ids.size(); ++i) {
            if (!same->ids.contains(each.mark.ids.at(i))) {
                same->ids.append(each.mark.ids.at(i));
                same->titles.append(each.mark.titles.value(i));
            }
        }
    }
    std::sort(marks.begin(), marks.end(), [](const ParagraphMark& a, const ParagraphMark& b) {
        return a.paragraph != b.paragraph ? a.paragraph < b.paragraph : a.kind < b.kind;
    });
    return marks;
}

std::optional<ParagraphMark> BookEditor::paragraphMarkAt(const QPointF& point) const
{
    return m_renderPipeline ? m_renderPipeline->paragraphMarkAt(point) : std::nullopt;
}

void BookEditor::anchorParagraphMarks(const QList<ParagraphMark>& marks)
{
    if (!m_textBuffer) {
        return;
    }
    for (const ParagraphMark& mark : marks) {
        const QTextBlock block = m_textBuffer->findBlockByNumber(mark.paragraph);
        if (block.isValid()) {
            m_paragraphMarks.push_back({QTextCursor(block), mark});
        }
    }
}

void BookEditor::syncParagraphMarks(bool always)
{
    if (!m_renderPipeline || (!always && m_paragraphMarks.empty() &&
                              m_renderPipeline->paragraphMarks().empty())) {
        return;
    }
    const QList<ParagraphMark> now = paragraphMarks();
    std::vector<ParagraphMark> marks(now.cbegin(), now.cend());
    if (always || marks != m_renderPipeline->paragraphMarks()) {
        m_renderPipeline->setParagraphMarks(std::move(marks), m_paragraphMarkIcons);
    }
}

void BookEditor::openParagraphMark(const ParagraphMark& mark, const QPoint& globalPos)
{
    if (mark.kind == ParagraphMarkKind::Materials && mark.ids.size() == 1) {
        emit paragraphMarkClicked(mark.ids.first());
        return;
    }
    QMenu menu(this);
    addParagraphMarkItems(menu, mark);
    menu.exec(globalPos);
}

void BookEditor::addParagraphMarkItems(QMenu& menu, const ParagraphMark& mark)
{
    const QIcon icon = m_paragraphMarkIcons.value(mark.kind);
    const bool clippings = mark.kind == ParagraphMarkKind::Clippings;
    if (clippings && mark.ids.size() == 1) {
        addClippingItems(menu, mark.ids.first());
        return;
    }
    for (qsizetype i = 0; i < mark.ids.size(); ++i) {
        const QString id = mark.ids.at(i);
        QString title = mark.titles.value(i);
        if (title.isEmpty()) {
            title = id;
        }
        // A "&" of the title is shown, not taken for an access key
        title.replace(QLatin1Char('&'), QStringLiteral("&&"));
        if (clippings) {
            addClippingItems(*menu.addMenu(icon, title), id);
            continue;
        }
        const QAction* item = menu.addAction(icon, title);
        connect(item, &QAction::triggered, this, [this, id]() { emit paragraphMarkClicked(id); });
    }
}

void BookEditor::addClippingItems(QMenu& menu, const QString& id)
{
    //: The scissors beside a paragraph a clipping was cut from: shows the clipping in the
    //: Properties panel. In Polish: &Pokaż wycinek
    const QAction* show = menu.addAction(tr("&Show Clipping"));
    connect(show, &QAction::triggered, this, [this, id]() { emit paragraphMarkClicked(id); });
    //: The scissors beside a paragraph a clipping was cut from: inserts it back where it was
    //: cut from. In Polish: Przywróć &tutaj
    QAction* restore = menu.addAction(tr("&Restore Here"));
    restore->setEnabled(!m_readOnly);
    connect(restore, &QAction::triggered, this,
            [this, id]() { emit clippingRestoreRequested(id); });
}

void BookEditor::addParagraphMarkMenus(QMenu& menu, int paragraph)
{
    bool separated = false;
    for (const ParagraphMark& mark : paragraphMarks()) {
        if (mark.paragraph != paragraph) {
            continue;
        }
        if (!separated) {
            menu.addSeparator();
            separated = true;
        }
        //: The submenu of the materials linked with the paragraph. In Polish: Powiązane
        //: mate&riały
        const QString materials = tr("Linked Mate&rials");
        //: The submenu of the clippings cut from the paragraph (the scissors beside it). In
        //: Polish: Wycinki z tego &miejsca
        const QString clippings = tr("Clippings &From Here");
        addParagraphMarkItems(
            *menu.addMenu(m_paragraphMarkIcons.value(mark.kind),
                          mark.kind == ParagraphMarkKind::Clippings ? clippings : materials),
            mark);
    }
}

QString BookEditor::paragraphMarkToolTip(const ParagraphMark& mark)
{
    const bool clippings = mark.kind == ParagraphMarkKind::Clippings;
    //: The tooltip of the paperclip beside a paragraph, over the titles of the materials. In
    //: Polish: Powiązane materiały
    const QString materials = tr("Linked materials");
    //: The tooltip of the scissors beside a paragraph, over the clippings cut from it. In
    //: Polish: Wycięto tu
    const QString cut = tr("Cut here");
    QStringList lines{
        QStringLiteral("<b>%1</b>").arg((clippings ? cut : materials).toHtmlEscaped())};
    for (const QString& title : mark.titles) {
        lines << title.toHtmlEscaped();
    }
    if (clippings) {
        //: The end of the tooltip of the scissors beside a paragraph. In Polish: Kliknięcie
        //: albo menu akapitu: Pokaż wycinek, Przywróć tutaj
        lines << QStringLiteral("<i>%1</i>")
                     .arg(tr("A click or the menu of the paragraph: Show Clipping, Restore Here")
                              .toHtmlEscaped());
    }
    return QStringLiteral("<p style='white-space:pre-wrap'>%1</p>")
        .arg(lines.join(QStringLiteral("<br>")));
}

bool BookEditor::updateAnnotation(const Annotation& annotation, bool joinPreviousStep)
{
    if (m_readOnly) {
        return false;
    }
    // A step only for a change: one with nothing but the cursor in it would be undone too
    if (!m_textBuffer || !findAnnotation(*m_textBuffer, annotation.id)) {
        return false;
    }
    QTextCursor step(m_textBuffer.get());
    beginAnnotationStep(step, joinPreviousStep);
    editor::updateAnnotation(*m_textBuffer, annotation);
    step.endEditBlock();

    update();
    emit contentChanged();
    return true;
}

bool BookEditor::removeAnnotation(const QString& id)
{
    if (m_readOnly) {
        return false;
    }
    if (!m_textBuffer || !findAnnotation(*m_textBuffer, id)) {
        return false;
    }
    QTextCursor step(m_textBuffer.get());
    beginAnnotationStep(step, false);
    editor::removeAnnotation(*m_textBuffer, id);
    step.endEditBlock();

    update();
    emit contentChanged();
    return true;
}

void BookEditor::beginAnnotationStep(QTextCursor& step, bool joinPreviousStep)
{
    // Without its cursor, undoing the step would put the cursor where its edit block began:
    // at the start of the text
    if (joinPreviousStep) {
        step.joinPreviousEditBlock();
    } else {
        step.beginEditBlock();
    }
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
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

    // Its card under the paragraph is outlined, and comes into view, while the cursor is in
    // its fragment
    if (m_annotationCards.isShown()) {
        m_annotationCards.setSelected(id);
        m_selectedCard = QTextCursor(m_textBuffer.get());
        m_selectedCard.setPosition(place->start);
        m_selectedCard.setPosition(place->end, QTextCursor::KeepAnchor);
        scrollToAnnotationCard(id);
        update();
    }
    return true;
}

bool BookEditor::annotationsAllowed() const
{
    return m_annotationsAllowed;
}

void BookEditor::setAnnotationsAllowed(bool allowed)
{
    m_annotationsAllowed = allowed;
    if (m_textBuffer) {
        setKeepsAnnotations(*m_textBuffer, allowed);  // for text pasted or dropped into it
    }
}

QString BookEditor::goToNextTodo()
{
    if (!m_textBuffer) {
        return {};
    }
    const int position = calculateAbsolutePosition(m_cursorPosition);
    for (const AnnotationPlace& place : annotationsIn(*m_textBuffer)) {
        if (place.annotation.kind == AnnotationKind::Todo && !place.annotation.done &&
            place.start > position) {
            goToAnnotation(place.annotation.id);
            return place.annotation.id;
        }
    }
    return {};
}

QString BookEditor::goToPreviousTodo()
{
    if (!m_textBuffer) {
        return {};
    }

    // Before the cursor, or before the selected fragment of the TODO the cursor is at
    const int position = hasSelection()
                             ? calculateAbsolutePosition(m_selection.normalized().start)
                             : calculateAbsolutePosition(m_cursorPosition);
    const std::vector<AnnotationPlace> places = annotationsIn(*m_textBuffer);
    for (auto it = places.rbegin(); it != places.rend(); ++it) {
        if (it->annotation.kind == AnnotationKind::Todo && !it->annotation.done &&
            it->start < position) {
            goToAnnotation(it->annotation.id);
            return it->annotation.id;
        }
    }
    return {};
}

}  // namespace kalahari::editor
