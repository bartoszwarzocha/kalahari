// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file book_editor_fields.cpp
/// @brief BookEditor: the fields of the book's title, its author, the text's title, the page,
///        the words and the date

#include <kalahari/editor/book_editor.h>

#include "kalahari/editor/detail/book_editor_internal.h"

#include <kalahari/editor/annotation.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/editor_render_pipeline.h>
#include <kalahari/editor/editor_types.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/text_captions.h>
#include <kalahari/editor/text_fields.h>
#include <kalahari/editor/view_modes.h>

#include <QMenu>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTimer>

#include <algorithm>
#include <optional>
#include <vector>

namespace kalahari::editor {

namespace {

/// The layout of a document; nullptr: another one
KalahariTextDocumentLayout* layoutOf(const QTextDocument* document)
{
    return document != nullptr
               ? qobject_cast<KalahariTextDocumentLayout*>(document->documentLayout())
               : nullptr;
}

/// The format of the character at a position of a document
QTextCharFormat characterFormat(QTextDocument* document, int position)
{
    QTextCursor cursor(document);
    cursor.setPosition(position);
    cursor.setPosition(position + 1, QTextCursor::KeepAnchor);
    return cursor.charFormat();
}

/// Whether a position of a document is the character of a field (of any kind)
bool isFieldAt(QTextDocument* document, int position)
{
    if (document == nullptr || position < 0 || position + 1 >= document->characterCount() ||
        document->characterAt(position) != QChar::ObjectReplacementCharacter) {
        return false;
    }
    return isFieldFormat(characterFormat(document, position));
}

/// What the tooltip of a field of the book's title or its author says it changes with
QString bookFieldNote(const TextField& field, const FieldContext& context)
{
    const bool empty = isTextFieldEmpty(field, context);
    if (field.type == FieldType::BookTitle) {
        return empty
                   //: The tooltip of a field of a book without a title
                   ? BookEditor::tr("The book has no title yet: write it in the Properties panel.")
                   : BookEditor::tr("It changes with the book's title in the Properties panel.");
    }
    return empty ? BookEditor::tr("The book has no author yet: write it in the Properties panel.")
                 : BookEditor::tr("It changes with the book's author in the Properties panel.");
}

/// What the tooltip of a field of the page or the pages says it shows, in the page view
/// (@p pages) or in the others
QString pageFieldNote(FieldType type, bool pages)
{
    if (type == FieldType::Page) {
        return pages ? BookEditor::tr("The page the field is on in the Page Layout view.")
                     //: The tooltip of a page number field in the continuous views, where
                     //: it shows "#"
                     : BookEditor::tr("The page number shows in the Page Layout view (View > "
                                      "View Mode).");
    }
    return pages ? BookEditor::tr("The pages of the text in the Page Layout view.")
                 : BookEditor::tr("The page count shows in the Page Layout view (View > View "
                                  "Mode).");
}

/// What the tooltip of a field of the date says it shows: the date or the time
QString dateFieldNote(const QString& format)
{
    return format == QLatin1String("time") || format == QLatin1String("date-time")
               ? BookEditor::tr("The time the text was opened or brought to the front.")
               : BookEditor::tr(
                     "Today's date: it changes when the text is opened on another day.");
}

/// What the tooltip of a field says it shows and how it changes, in the page view (@p pages) or
/// in the others
QString fieldNote(const TextField& field, const FieldContext& context, bool pages)
{
    switch (field.type) {
        case FieldType::BookTitle:
        case FieldType::Author:
            return bookFieldNote(field, context);
        case FieldType::TextTitle:
            return BookEditor::tr("It changes with the title of the text.");
        case FieldType::Page:
        case FieldType::Pages:
            return pageFieldNote(field.type, pages);
        case FieldType::Words:
            return BookEditor::tr("The words of the text, as the status bar counts them.");
        case FieldType::Date:
            return dateFieldNote(field.format);
        case FieldType::Custom: {
            if (!context.customFields.contains(field.key)) {
                //: The tooltip of an own field the book does not have any more
                return BookEditor::tr("The book has no such field any more: change the field or "
                                      "convert it to text.");
            }
            const QString description =
                context.customFields.value(field.key).description.simplified();
            //: The tooltip of an own field of the book
            const QString note = BookEditor::tr(
                "It changes with the value of the field (Insert > Field..., the Properties "
                "panel).");
            return description.isEmpty() ? note : description + QLatin1Char('\n') + note;
        }
    }
    return {};
}

}  // namespace

// =============================================================================
// The book around the text
// =============================================================================

void BookEditor::setFieldContext(const FieldContext& context)
{
    m_fieldContext = context;
    if (!m_fieldContext.now.isValid()) {
        m_fieldContext.now = QDateTime::currentDateTime();
    }
    if (KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get())) {
        layout->setFieldContext(m_fieldContext);
    }
}

void BookEditor::refreshFieldWords()
{
    KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    if (layout == nullptr || !layout->hasTextField(FieldType::Words)) {
        return;
    }
    layout->setFieldWords(textCounts().words);
}

// =============================================================================
// The fields of the text
// =============================================================================

bool BookEditor::insertField(const TextField& field)
{
    if (refusesChange()) {
        return false;
    }
    ensureDocument();
    QTextDocument* document = m_textBuffer.get();

    // One undo step with the selection it replaces
    const StepCursor before{.cursor = m_cursorPosition, .selection = m_selection};
    QTextCursor step(document);
    step.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, before] { m_stepCursor = before; }));
    QTextCursor cursor = createCursor(document, m_cursorPosition);
    AnnotationList fragments;
    bool replacing = false;
    if (hasSelection()) {
        cursor = selectionToReplace();
        clearSelection();
        replacing = cursor.hasSelection();
        if (replacing) {
            fragments = removeForReplacement(cursor);
        }
    } else {
        leaveObject(cursor);  // never in the block of a line or a break
    }
    QTextCharFormat format = insertionFormat(cursor);
    if (replacing) {
        setAnnotations(format, fragments);
    }
    setTextField(format, field);
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), format);
    const CursorPosition after = absoluteToCursorPosition(document, cursor.position());
    document->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{.cursor = after, .selection = {}}] { m_stepCursor = state; }));
    step.endEditBlock();
    m_learnableChange.reset();

    m_selectionAnchor = after;
    finishEdit(cursor);
    if (field.type == FieldType::Words) {
        refreshFieldWords();  // at once, not when typing pauses
    }
    return true;
}

std::optional<int> BookEditor::fieldAtCursor() const
{
    if (!m_textBuffer) {
        return std::nullopt;
    }
    QTextDocument* document = m_textBuffer.get();
    const auto textFieldThere = [this, document](int position) -> std::optional<int> {
        if (isFieldAt(document, position) && textFieldAt(position)) {
            return position;
        }
        return std::nullopt;
    };
    // The field selected alone
    if (hasSelection()) {
        const SelectionRange range = m_selection.normalized();
        const int from = calculateAbsolutePosition(range.start);
        if (calculateAbsolutePosition(range.end) != from + 1) {
            return std::nullopt;
        }
        return textFieldThere(from);
    }
    // The one just before the cursor, else the one just after it
    const int position = calculateAbsolutePosition(m_cursorPosition);
    if (const std::optional<int> field = textFieldThere(position - 1)) {
        return field;
    }
    return textFieldThere(position);
}

std::optional<TextField> BookEditor::textFieldAt(int position) const
{
    if (!m_textBuffer || position < 0 || position + 1 >= m_textBuffer->characterCount() ||
        m_textBuffer->characterAt(position) != QChar::ObjectReplacementCharacter) {
        return std::nullopt;
    }
    return textFieldOf(characterFormat(m_textBuffer.get(), position));
}

QString BookEditor::fieldTextAt(int position) const
{
    QTextDocument* document = m_textBuffer.get();
    if (!isFieldAt(document, position)) {
        return {};
    }
    const QTextCharFormat format = characterFormat(document, position);
    if (const KalahariTextDocumentLayout* layout = layoutOf(document)) {
        return layout->fieldText(position, format).text;
    }
    return captionNumbers().fieldText(position, format).text;
}

bool BookEditor::setTextFieldAt(int position, const TextField& field)
{
    const std::optional<TextField> current = textFieldAt(position);
    if (!current || refusesChange()) {
        return false;
    }
    TextField wanted = field;
    wanted.format = normalizedFieldFormat(wanted.type, wanted.format);
    if (*current == wanted) {
        return false;
    }
    QTextDocument* document = m_textBuffer.get();
    QTextCursor step(document);
    step.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{.cursor = m_cursorPosition, .selection = m_selection}] {
            m_stepCursor = state;
        }));
    QTextCursor cursor(document);
    cursor.setPosition(position);
    cursor.setPosition(position + 1, QTextCursor::KeepAnchor);
    QTextCharFormat format = cursor.charFormat();
    setTextField(format, wanted);
    cursor.setCharFormat(format);
    step.endEditBlock();
    m_learnableChange.reset();

    if (wanted.type == FieldType::Words) {
        refreshFieldWords();
    }
    update();
    emit contentChanged();
    return true;
}

bool BookEditor::convertFieldToText(int position)
{
    if (!textFieldAt(position) || refusesChange()) {
        return false;
    }
    const QString text = fieldTextAt(position);
    QTextDocument* document = m_textBuffer.get();

    // The text in the formatting of the field (its font, its annotations), as one undo step;
    // the cursor after it
    const StepCursor before{.cursor = m_cursorPosition, .selection = m_selection};
    QTextCursor step(document);
    step.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, before] { m_stepCursor = before; }));
    QTextCursor cursor(document);
    cursor.setPosition(position);
    cursor.setPosition(position + 1, QTextCursor::KeepAnchor);
    QTextCharFormat format = cursor.charFormat();
    format.clearProperty(QTextFormat::ObjectType);
    clearField(format);
    clearSelection();
    cursor.insertText(text, format);
    const CursorPosition after = absoluteToCursorPosition(document, cursor.position());
    document->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{.cursor = after, .selection = {}}] { m_stepCursor = state; }));
    step.endEditBlock();
    m_learnableChange.reset();

    m_selectionAnchor = after;
    finishEdit(cursor);
    return true;
}

int BookEditor::changeCustomFields(const QString& key, const CustomFieldValue& value,
                                   bool asText)
{
    if (key.isEmpty() || refusesChange() || !m_textBuffer ||
        customFieldCount(*m_textBuffer, key) == 0) {
        return 0;
    }
    QTextDocument* document = m_textBuffer.get();

    // The places of the fields, from the end: the positions before them stay as they were
    std::vector<int> positions;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const std::optional<TextField> field =
                fragment.isValid() ? textFieldOf(fragment.charFormat()) : std::nullopt;
            if (!field || field->type != FieldType::Custom || field->key != key) {
                continue;
            }
            const QString characters = fragment.text();
            for (qsizetype i = 0; i < characters.size(); ++i) {
                if (characters.at(i) == QChar::ObjectReplacementCharacter) {
                    positions.push_back(fragment.position() + static_cast<int>(i));
                }
            }
        }
    }

    // One undo step, the cursor where it was
    const StepCursor before{.cursor = m_cursorPosition, .selection = m_selection};
    QTextCursor step(document);
    step.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, before] { m_stepCursor = before; }));
    FieldContext context;
    context.language = m_fieldContext.language;
    context.customFields.insert(key, value);
    const int cursorAt = calculateAbsolutePosition(m_cursorPosition);
    int shift = 0;
    for (auto it = positions.rbegin(); it != positions.rend(); ++it) {
        QTextCursor cursor(document);
        cursor.setPosition(*it);
        cursor.setPosition(*it + 1, QTextCursor::KeepAnchor);
        QTextCharFormat format = cursor.charFormat();
        std::optional<TextField> field = textFieldOf(format);
        if (!field) {
            continue;
        }
        if (asText) {
            const QString text = textFieldText(*field, context, FieldCounts{});
            format.clearProperty(QTextFormat::ObjectType);
            clearField(format);
            cursor.insertText(text, format);
            if (*it < cursorAt) {
                shift += static_cast<int>(text.size()) - 1;
            }
        } else {
            field->name = value.name;
            setTextField(format, *field);
            cursor.setCharFormat(format);
        }
    }
    clearSelection();
    const CursorPosition after = absoluteToCursorPosition(
        document, std::clamp(cursorAt + shift, 0, document->characterCount() - 1));
    document->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{.cursor = after, .selection = {}}] { m_stepCursor = state; }));
    step.endEditBlock();
    m_learnableChange.reset();

    m_cursorPosition = after;
    m_selectionAnchor = after;
    update();
    emit contentChanged();
    return static_cast<int>(positions.size());
}

std::optional<int> BookEditor::fieldAt(const QPointF& widgetPos) const
{
    const KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    if (layout == nullptr || !m_renderPipeline) {
        return std::nullopt;
    }
    const int hit = layout->hitTest(m_renderPipeline->widgetToDocument(widgetPos), Qt::ExactHit);
    if (!isFieldAt(m_textBuffer.get(), hit)) {
        return std::nullopt;
    }
    return hit;
}

// =============================================================================
// The menu and the tooltip
// =============================================================================

void BookEditor::addFieldActions(QMenu& menu, int position)
{
    //: The context menu of a field. In Polish: &Edytuj pole...
    QAction* edit = menu.addAction(tr("&Edit Field..."), this,
                                   [this, position]() { emit fieldEditRequested(position); });
    disableWhileReadOnly(edit);
    //: The field becomes the text it shows. In Polish: Zamień na &stały tekst
    QAction* convert = menu.addAction(tr("Con&vert to Text"), this,
                                      [this, position]() { convertFieldToText(position); });
    disableWhileReadOnly(convert);
}

QString BookEditor::fieldToolTip(int position) const
{
    const std::optional<TextField> field = textFieldAt(position);
    if (!field) {
        return {};
    }
    const QString note = fieldNote(*field, m_fieldContext, m_viewMode == ViewMode::Page);
    //: The tooltip of a field: its name in bold, what it shows, how to change it. %1: the
    //: field's name ("Page Number")
    const QString title = tr("Field: %1").arg(textFieldTitle(*field, m_fieldContext));
    const QString change =
        m_readOnly ? QString() : tr("Double-click to change the field.");
    QString tip = QStringLiteral("<p style='white-space:pre-wrap'><b>%1</b><br>%2")
                      .arg(title.toHtmlEscaped(), note.toHtmlEscaped());
    if (!change.isEmpty()) {
        tip += QStringLiteral("<br>") + change.toHtmlEscaped();
    }
    return tip + QStringLiteral("</p>");
}

}  // namespace kalahari::editor
