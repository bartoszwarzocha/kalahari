/// @file book_editor_links.cpp
/// @brief BookEditor: links, made with Insert > Link and by autocorrect

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/editor/buffer_commands.h>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextDocument>
#include <algorithm>

namespace kalahari::editor {

namespace {

/// @brief The text of positions [from, to) of a document as a line of the dialog: its
///        paragraph breaks and line breaks spaces, without the marks of objects
QString lineOf(const QTextDocument& document, int from, int to)
{
    QTextCursor cursor(const_cast<QTextDocument*>(&document));
    cursor.setPosition(from);
    cursor.setPosition(to, QTextCursor::KeepAnchor);
    QString text = cursor.selectedText();
    text.replace(QChar::ParagraphSeparator, QLatin1Char(' '));
    text.replace(QChar::LineSeparator, QLatin1Char(' '));
    text.remove(QChar::ObjectReplacementCharacter);
    return text;
}

/// @brief Whether the dialog can change the text of positions [from, to): text of one
///        paragraph without a line break or an object
bool isLineOfText(const QTextDocument& document, int from, int to)
{
    if (document.findBlock(from) != document.findBlock(to)) {
        return false;
    }
    for (int position = from; position < to; ++position) {
        const QChar ch = document.characterAt(position);
        if (ch == QChar::LineSeparator || ch == QChar::ObjectReplacementCharacter) {
            return false;
        }
    }
    return true;
}

/// @brief A text written in the dialog's field as the text of a link: one line
QString linkText(const QString& written)
{
    QString text = written;
    for (const QChar breaking : {QChar(u'\n'), QChar(u'\r'), QChar(QChar::ParagraphSeparator),
                                 QChar(QChar::LineSeparator)}) {
        text.replace(breaking, QLatin1Char(' '));
    }
    text.remove(QChar::ObjectReplacementCharacter);
    return text;
}

}  // namespace

LinkTarget BookEditor::linkTarget() const
{
    LinkTarget target;
    if (!m_textBuffer) {
        return target;
    }
    const QTextDocument& document = *m_textBuffer;
    const auto ofLink = [&document](const TextLink& link) {
        return LinkTarget{link.from,
                          link.to,
                          lineOf(document, link.from, link.to),
                          link.address,
                          true,
                          isLineOfText(document, link.from, link.to)};
    };

    if (hasSelection()) {
        const SelectionRange selection = m_selection.normalized();
        const int from = calculateAbsolutePosition(selection.start);
        const int to = calculateAbsolutePosition(selection.end);
        if (const std::optional<TextLink> link = linkAtCharacter(document, from);
            link && to <= link->to) {
            return ofLink(*link);
        }
        target.from = from;
        target.to = to;
        target.text = lineOf(document, from, to);
        target.textEditable = isLineOfText(document, from, to);
        return target;
    }

    const int position = calculateAbsolutePosition(m_cursorPosition);
    if (const std::optional<TextLink> link = linkAtPosition(document, position)) {
        return ofLink(*link);
    }
    target.from = position;
    target.to = position;
    return target;
}

bool BookEditor::setLink(const LinkTarget& target, const QString& text, const QString& address)
{
    if (refusesChange() || !m_textBuffer) {
        return false;
    }
    const QString linkAddress = normalizedLinkAddress(address);
    if (linkAddress.isEmpty()) {
        return false;
    }
    QTextDocument* document = m_textBuffer.get();
    const int last = document->characterCount() - 1;
    const int from = std::clamp(target.from, 0, last);
    const int to = std::clamp(target.to, from, last);
    QString shown = linkText(text);
    const bool keepsText = from < to && (!target.textEditable || shown == target.text);
    if (shown.isEmpty()) {
        shown = linkText(address.trimmed());
    }

    // One undo step: undone, it brings back the cursor and the selection it was made with;
    // redone, the cursor after the link
    const StepCursor before{m_cursorPosition, m_selection};
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(
        new CallbackUndoItem([this, before] { m_stepCursor = before; }));
    int end = to;
    if (keepsText) {
        // The text keeps its formatting; only the link changes, also in the pieces of other
        // formats
        const SelectionRange range{absoluteToCursorPosition(document, from),
                                   absoluteToCursorPosition(document, to)};
        for (const FormatPiece& piece :
             changedFormatPieces(range, [&linkAddress](QTextCharFormat& format) {
                 setLinkAddress(format, linkAddress);
             })) {
            if (piece.emptyParagraph) {
                continue;  // the link is on text, not on what is typed in an empty paragraph
            }
            cursor.setPosition(piece.from);
            cursor.setPosition(piece.to, QTextCursor::KeepAnchor);
            cursor.setCharFormat(piece.format);
        }
    } else if (from < to) {
        // The new text in the format of the first character it replaces, with the fragments
        // of annotations it is in; an annotation on a replaced character stays on its place
        QTextCursor first(document);
        first.setPosition(from + 1);
        QTextCharFormat format = first.charFormat();
        AnnotationList fragments = annotationsOf(format);
        fragments.removeIf([](const Annotation& annotation) { return annotation.point; });
        setAnnotations(format, fragments);
        setLinkAddress(format, linkAddress);
        cursor.setPosition(from);
        cursor.insertText(shown, format);
        end = from + static_cast<int>(shown.size());
        cursor.setPosition(end + (to - from), QTextCursor::KeepAnchor);
        removeKeepingAnnotations(cursor);
    } else {
        // At the cursor, as typed text: in the format of its place and the one given for the
        // text typed next; next to an object in a paragraph of its own
        QTextCharFormat typed = pendingFormat();
        setLinkAddress(typed, linkAddress);
        cursor.setPosition(from);
        insertKeepingAnnotations(cursor, shown, typed);
        end = cursor.position();
    }
    const CursorPosition after = absoluteToCursorPosition(document, end);
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{after, {}}] { m_stepCursor = state; }));
    cursor.endEditBlock();
    m_learnableChange.reset();

    clearSelection();
    m_selectionAnchor = after;
    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    finishEdit(createCursor(document, after));
    return true;
}

bool BookEditor::removeLink(const LinkTarget& target)
{
    if (refusesChange() || !m_textBuffer || target.from >= target.to) {
        return false;
    }
    QTextDocument* document = m_textBuffer.get();
    const int last = document->characterCount() - 1;
    const SelectionRange range{
        absoluteToCursorPosition(document, std::clamp(target.from, 0, last)),
        absoluteToCursorPosition(document, std::clamp(target.to, 0, last))};
    const std::vector<FormatPiece> pieces =
        changedFormatPieces(range, [](QTextCharFormat& format) { setLinkAddress(format, {}); });
    if (pieces.empty()) {
        return false;
    }
    setFormats(pieces);
    return true;
}

std::optional<TextLink> BookEditor::linkAt(const QPointF& widgetPos) const
{
    if (!m_textBuffer || !m_renderPipeline) {
        return std::nullopt;
    }
    // The character under the point: the layout's exact hit, which misses the space around
    // the text
    const QPointF docPoint = m_renderPipeline->widgetToDocument(widgetPos);
    const int hit = m_textBuffer->documentLayout()->hitTest(docPoint, Qt::ExactHit);
    if (hit < 0) {
        return std::nullopt;
    }
    return linkAtCharacter(*m_textBuffer, hit);
}

bool BookEditor::autoLinkTyped(const QTextBlock& block, int end)
{
    const std::optional<AutoLink> link = autoLink(block.text().left(end), m_autoCorrect);
    if (!link) {
        return false;
    }
    const int from = block.position() + link->start;
    const int to = block.position() + link->end;
    for (const TextLink& existing : linksIn(block)) {
        if (existing.from < to && from < existing.to) {
            return false;  // the writer's own link
        }
    }

    // An undo step of its own, apart from the text typed before and after it: undone right
    // after, the address stays as typed
    const StepCursor typed{m_cursorPosition, {}};
    QTextCursor cursor(m_textBuffer.get());
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem([this, typed] { m_stepCursor = typed; }));
    cursor.setPosition(from);
    cursor.setPosition(to, QTextCursor::KeepAnchor);
    QTextCharFormat format;
    setLinkAddress(format, link->address);
    cursor.mergeCharFormat(format);
    m_textBuffer->appendUndoItem(new CallbackUndoItem([this, typed] { m_stepCursor = typed; }));
    cursor.endEditBlock();
    m_learnableChange.reset();

    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    finishEdit(createCursor(m_textBuffer.get(), m_cursorPosition));
    return true;
}

}  // namespace kalahari::editor
