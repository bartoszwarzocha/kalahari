/// @file book_editor_editing.cpp
/// @brief BookEditor: typing and deleting, undo and redo, the clipboard

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/core/logger.h>
#include <kalahari/editor/annotation.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/find_replace_bar.h>
#include <kalahari/editor/kml_document_model.h>
#include <kalahari/editor/kml_serializer.h>
#include <kalahari/editor/paragraph_styles.h>
#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QTextBoundaryFinder>
#include <QTextDocumentFragment>
#include <algorithm>
#include <utility>

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
/// The line ends of every platform become paragraph breaks; U+2028 and a vertical tab (a
/// line break of Word) are a line break in the paragraph. Characters XML cannot hold
/// (control characters other than tab, unpaired surrogates, U+FFFE, U+FFFF) are dropped,
/// so pasted text cannot make the chapter file unreadable.
QString pastedPlainText(const QString& text) {
    QString result;
    result.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        const char16_t code = ch.unicode();
        if (code == u'\r' && i + 1 < text.size() && text.at(i + 1) == u'\n') {
            continue;  // the \n that follows makes the break
        }
        if (code == u'\v' || code == QChar::LineSeparator) {
            result += QChar(QChar::LineSeparator);
        } else if (code == u'\n' || code == u'\r' || code == u'\f' ||
                   code == QChar::ParagraphSeparator) {
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

    // An object that went into a paragraph gets a block of its own
    separateObjects(cursor, insertionStart, cursor.position());
    cursor.endEditBlock();
}

/// @brief Put a format under the text of a document: each character takes it, with its own
///        format on top, and so does each paragraph's own format
void putFormatUnder(QTextDocument& content, const QTextCharFormat& under) {
    struct Piece {
        int from = 0;
        int to = -1;  ///< -1: the paragraph's own format at @c from
        QTextCharFormat format;
    };
    std::vector<Piece> pieces;
    for (QTextBlock block = content.begin(); block.isValid(); block = block.next()) {
        QTextCharFormat paragraph = under;
        paragraph.merge(block.charFormat());
        pieces.push_back({block.position(), -1, paragraph});
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            QTextCharFormat format = under;
            format.merge(fragment.charFormat());
            pieces.push_back(
                {fragment.position(), fragment.position() + fragment.length(), format});
        }
    }
    QTextCursor cursor(&content);
    for (const Piece& piece : pieces) {
        cursor.setPosition(piece.from);
        if (piece.to < 0) {
            cursor.setBlockCharFormat(piece.format);
        } else {
            cursor.setPosition(piece.to, QTextCursor::KeepAnchor);
            cursor.setCharFormat(piece.format);
        }
    }
}

/// @brief Insert text with annotations at the cursor, replacing its selection, as one edit
/// block
///
/// The annotations of the replaced selection stay on its place, and the text joins the
/// fragments of the place; its annotations the document already has come in as copies.
/// @param placeFormat The text takes the format of the place (insertionFormat()), with
///        @p typed on top and then its own format, as text of another program pasted in
///        PasteMode::Merge
/// @param typed A format given for the text typed next
void insertAnnotatedDocument(QTextCursor& cursor, QTextDocument& content,
                             bool placeFormat = false,
                             const QTextCharFormat& typed = QTextCharFormat()) {
    QTextDocument& document = *cursor.document();
    cursor.beginEditBlock();
    const AnnotationList fragments = removeForReplacement(cursor);
    if (placeFormat) {
        QTextCharFormat under = insertionFormat(cursor);
        setAnnotations(under, {});  // the text joins the place's fragments below
        under.merge(typed);
        putFormatUnder(content, under);
    }
    prepareForInsertion(content, document, fragments);
    giveListsNewIds(content, document);

    // The annotations on the start of the first paragraph go on the place of the text
    const AnnotationList startPlaces = annotationsOf(content.firstBlock().charFormat());
    const int place = cursor.position();
    insertDocument(cursor, content);
    anchorToPlace(document, place, startPlaces);
    cursor.endEditBlock();
}

/// @brief Insert KML at the cursor as insertAnnotatedDocument() does
/// @return false when the KML has no paragraph the editor can read: nothing changed
bool insertKml(QTextCursor& cursor, const QString& kml, bool placeFormat = false,
               const QTextCharFormat& typed = QTextCharFormat()) {
    KmlDocumentModel model;
    if (kml.isEmpty() || !model.loadKml(kml) || model.paragraphCount() == 0) {
        return false;
    }
    QTextDocument content;
    content.setUndoRedoEnabled(false);
    QTextCursor contentCursor(&content);
    appendParagraphs(contentCursor, model);
    insertAnnotatedDocument(cursor, content, placeFormat, typed);
    return true;
}

/// @brief Insert MIME data at the cursor, replacing its selection, as one edit block
///
/// Kalahari content (KML) is parsed like a chapter file, so formatting, alignment and
/// annotations survive. Text from other programs comes as @p mode reads its HTML; in
/// PasteMode::Merge it takes the formatting of the insertion point with @p typed on top (a
/// format given for the text typed next), and then its own emphasis. Without HTML, or in
/// PasteMode::Text, the plain text takes the formatting of the insertion point and @p
/// typed, and its HTML tells its line breaks from its paragraph ends. Only the document
/// changes: the editor's cursor and view follow once the outermost edit block has ended.
/// The cursor ends after the inserted text.
void insertMimeData(QTextCursor& cursor, const QMimeData& source, PasteMode mode,
                    const QTextCharFormat& typed = QTextCharFormat()) {
    if (source.hasFormat(QString::fromLatin1(MIME_KML))) {
        if (insertKml(cursor, QString::fromUtf8(source.data(QString::fromLatin1(MIME_KML))))) {
            return;
        }
        core::Logger::getInstance().warn("BookEditor: unreadable KML, inserting the text");
    }

    if (mode != PasteMode::Text && source.hasHtml() &&
        insertKml(cursor, ClipboardHandler::htmlToKml(source.html(), mode),
                  mode == PasteMode::Merge, typed)) {
        return;
    }

    const QString text = ClipboardHandler::withLineEndsOf(source.text(), source.html());
    cursor.beginEditBlock();  // the replaced selection and the text: one undo step
    insertKeepingAnnotations(cursor, pastedPlainText(text), typed);
    cursor.endEditBlock();
}

/// @brief The text the last Control+K presses in a row deleted (macOS), for Control+Y: one
///        for all the editors, as on macOS
QString& killedText() {
    static QString text;
    return text;
}

}  // anonymous namespace

// =============================================================================
// Text Input (Phase 4.1 - 4.4)
// =============================================================================

void BookEditor::insertText(const QString& text)
{
    if (refusesChange()) {
        return;
    }
    if (text.isEmpty()) {
        return;
    }

    ensureDocument();

    // Direct QTextCursor edit - recorded by QTextDocument's native undo: the replaced
    // selection with the text as one step, typed text with the text typed before it
    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    const QTextCharFormat pending = pendingFormat();
    if (hasSelection()) {
        const SelectionRange sel = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
        clearSelection();
    }
    insertKeepingAnnotations(cursor, text, pending);

    // Mirror the resulting QTextCursor into the editor's cursor model.
    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();

    ensureCursorVisible();
    syncPipelineCursor();
    update();
    emit contentChanged();
    emit paragraphModified(m_cursorPosition.paragraph);
}

void BookEditor::typeText(const QString& text)
{
    if (refusesChange()) {
        return;
    }
    qsizetype typed = 0;
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (startsAutoCorrection(text.at(i))) {
            insertText(text.mid(typed, i + 1 - typed));
            autoCorrectTyped();
            typed = i + 1;
        }
    }
    if (typed < text.size()) {
        insertText(text.mid(typed));
    }
}

void BookEditor::setAutoCorrect(const AutoCorrectOptions& options)
{
    m_autoCorrect = options;
}

void BookEditor::setTextLanguage(std::function<QString()> language)
{
    m_textLanguage = std::move(language);
}

bool BookEditor::refusesChange()
{
    if (m_readOnly) {
        emit changeRefused();
    }
    return m_readOnly;
}

void BookEditor::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly) {
        return;
    }
    m_readOnly = readOnly;
    // Nothing is composed in a read-only text, and Find and Replace only finds in it
    setAttribute(Qt::WA_InputMethodEnabled, !readOnly);
    if (readOnly && m_findReplaceBar != nullptr && !m_findReplaceBar->isHidden()) {
        m_findReplaceBar->showFind();
    }
}

void BookEditor::setReadOnlyReason(const QString& reason)
{
    m_readOnlyReason = reason;
}

void BookEditor::disableWhileReadOnly(QAction* action) const
{
    action->setEnabled(!m_readOnly);
    if (m_readOnly && !m_readOnlyReason.isEmpty()) {
        action->setToolTip(m_readOnlyReason);
        action->setStatusTip(m_readOnlyReason);
    }
}

AutoCorrectOptions BookEditor::autoCorrectForText() const
{
    AutoCorrectOptions options = m_autoCorrect;
    if (options.sentenceCapitals && !options.languageAbbreviations.empty()) {
        const QString list = abbreviationsLanguage(
            m_textLanguage ? m_textLanguage() : QString(), options.languageAbbreviations);
        if (!list.isEmpty()) {
            options.abbreviations = options.languageAbbreviations.at(list);
        }
    }
    return options;
}

void BookEditor::autoCorrectTyped()
{
    if (!m_textBuffer || hasSelection()) {
        return;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    if (!block.isValid() || !isCheckedParagraph(block)) {
        return;  // code, and other text whose spelling is not checked, stays as typed
    }
    if (autoCorrectList(block)) {
        return;  // "* " or "1. " starting the paragraph made it a list item
    }
    const std::optional<AutoCorrection> change = autoCorrection(
        block.text().left(m_cursorPosition.offset), autoCorrectForText(),
        [this]() {
            return QuoteMarks::forLanguage(m_textLanguage ? m_textLanguage() : QString());
        },
        m_capsLockOn);
    if (change) {
        applyAutoCorrection(block, *change, m_cursorPosition);
    }
}

void BookEditor::autoCorrectEndedParagraph(int paragraph, bool atEnd)
{
    if (!m_textBuffer || hasSelection()) {
        return;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(paragraph);
    if (!block.isValid() || !isCheckedParagraph(block)) {
        return;
    }
    const QString text = block.text();

    // "***" or "---" alone in the paragraph: the object, in the paragraph's place, with the
    // cursor in the new paragraph after it
    const TextObject object = atEnd ? paragraphObjectShortcut(text, m_autoCorrect)
                                    : TextObject::None;
    if (object == TextObject::None) {
        if (const std::optional<AutoCorrection> change =
                autoCorrectionAtParagraphEnd(text, autoCorrectForText(), m_capsLockOn)) {
            applyAutoCorrection(block, *change, m_cursorPosition);
        }
        return;
    }
    const CursorPosition cursorAt = m_cursorPosition;
    QTextCursor cursor(block);
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{cursorAt, {}}] { m_stepCursor = state; }));
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    removeKeepingAnnotations(cursor);
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), objectCharFormat(object));
    cursor.setBlockFormat(objectBlockFormat());
    cursor.setBlockCharFormat(QTextCharFormat());
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{cursorAt, {}}] { m_stepCursor = state; }));
    cursor.endEditBlock();
    m_learnableChange.reset();

    finishEdit(createCursor(m_textBuffer.get(), cursorAt));
}

void BookEditor::applyAutoCorrection(const QTextBlock& block, const AutoCorrection& change,
                                     const CursorPosition& typed)
{
    const int from = block.position() + change.start;
    const int to = block.position() + change.end;
    const int length = static_cast<int>(change.text.size());
    CursorPosition changed = typed;
    if (block.blockNumber() == typed.paragraph) {
        changed.offset += length - (change.end - change.start);
    }

    // The new characters take the format of the first one they replace, with the fragments
    // of annotations it is in; an annotation on a replaced character stays on its place
    QTextCursor first(m_textBuffer.get());
    first.setPosition(from + 1);
    QTextCharFormat format = first.charFormat();
    AnnotationList fragments = annotationsOf(format);
    fragments.removeIf([](const Annotation& annotation) { return annotation.point; });
    setAnnotations(format, fragments);

    // An undo step of its own, apart from the text typed before and after it: undone, it
    // brings back the typed characters with the cursor after them (its first undo item,
    // undone last); redone, the change with the cursor after it (its last item)
    QTextCursor cursor(m_textBuffer.get());
    cursor.setPosition(from);
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{typed, {}}] { m_stepCursor = state; }));
    cursor.insertText(change.text, format);
    cursor.setPosition(to + length, QTextCursor::KeepAnchor);
    removeKeepingAnnotations(cursor);
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{changed, {}}] { m_stepCursor = state; }));
    cursor.endEditBlock();

    // Undone right after, the change may give an exception list a word (undo())
    m_learnableChange.reset();
    if (change.learns != LearnedException::None) {
        m_learnableChange = LearnableChange{change.learns, change.exception,
                                            m_textBuffer->availableUndoSteps()};
    }

    finishEdit(createCursor(m_textBuffer.get(), changed));
    if (change.capsLock) {
        emit capsLockCorrected();
    }
}

bool BookEditor::deleteSelectedText()
{
    if (refusesChange()) {
        return false;
    }
    return removeSelection(true);
}

bool BookEditor::removeSelection(bool keepAnnotations)
{
    if (!hasSelection()) {
        return false;
    }

    ensureDocument();

    SelectionRange sel = m_selection.normalized();

    // Direct QTextCursor delete — recorded by QTextDocument's native undo.
    QTextCursor cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
    if (keepAnnotations) {
        removeKeepingAnnotations(cursor);
    } else {
        removeSelectionKeepingObjects(cursor);
    }

    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();
    clearSelection();

    // The view follows the cursor, as after typing: it stays where the selection started,
    // which can be far above the view (all the text selected and deleted)
    ensureCursorVisible();
    update();
    emit contentChanged();
    return true;
}

void BookEditor::insertNewline()
{
    if (refusesChange()) {
        return;
    }
    ensureDocument();
    if (endListAtEmptyItem()) {
        return;  // an empty list item goes a level up, or out of its list
    }

    // Split the paragraph via a direct QTextCursor insertBlock() — recorded by
    // QTextDocument's native undo, together with the replaced selection as one step.
    // The new paragraph inherits the current block format (zero margins + alignment), so
    // it keeps the same layout.
    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    const QTextCharFormat pending = pendingFormat();
    // Autocorrect looks at the paragraph Enter ends, when it ends one typed in: not at an
    // object, nor in the place of a selection
    const bool corrects = !hasSelection() && !isAtObject(cursor);
    const int ended = m_cursorPosition.paragraph;
    const bool endedAtEnd = cursor.atBlockEnd();
    if (hasSelection()) {
        const SelectionRange sel = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
        clearSelection();
    }
    cursor.beginEditBlock();
    removeKeepingAnnotations(cursor);
    if (isAtObject(cursor)) {
        // Before an object an empty paragraph goes before it, and the cursor stays at the
        // object; after one a new paragraph follows it
        const bool beforeObject = cursor.atBlockStart();
        leaveObject(cursor);
        if (beforeObject) {
            cursor.movePosition(QTextCursor::NextBlock);
        }
    } else {
        // At the end of a paragraph the new one is in the style that follows its style (a
        // heading is followed by the text); a paragraph split in two keeps its style. A list
        // item is followed by the next item, which does not keep its number of its own; the
        // new paragraph does not start a new page.
        const bool atParagraphEnd = cursor.atBlockEnd();
        const QString style = paragraphStyleOf(cursor.blockFormat());
        std::optional<ListItem> item = listItemOf(cursor.blockFormat());
        insertParagraphKeepingAnnotations(cursor);
        QTextBlockFormat format = cursor.blockFormat();
        if (const QString next = m_paragraphStyles.nextStyleOf(style);
            atParagraphEnd && next != style) {
            format = paragraphBlockFormat();
            setParagraphStyle(format, next);
            if (item) {
                setListItem(format, *item);
            }
        }
        format.clearProperty(LIST_VALUE_PROPERTY);
        format.clearProperty(PARAGRAPH_PAGE_BREAK_BEFORE_PROPERTY);
        if (format != cursor.blockFormat()) {
            cursor.setBlockFormat(format);
        }
    }
    cursor.endEditBlock();

    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();

    ensureCursorVisible();
    syncPipelineCursor();
    update();
    emit contentChanged();
    emit paragraphInserted(m_cursorPosition.paragraph);

    if (corrects) {
        autoCorrectEndedParagraph(ended, endedAtEnd);
    }
    if (!pending.isEmpty()) {
        setPendingFormat(pending);  // the text typed in the new paragraph gets it
    }
}

void BookEditor::insertLineBreak()
{
    if (refusesChange()) {
        return;
    }
    ensureDocument();

    // Next to an object there is no paragraph to break: a new one starts, as with Enter
    if (!hasSelection() && isAtObject(createCursor(m_textBuffer.get(), m_cursorPosition))) {
        insertNewline();
        return;
    }

    // A character of the paragraph, typed as text: it takes the format of the place and
    // the one given for the text typed next, which the text after it keeps. Its own edit
    // block parts it from the text typed before it in the undo history.
    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    const QTextCharFormat pending = pendingFormat();
    if (hasSelection()) {
        const SelectionRange sel = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
        clearSelection();
    }
    cursor.beginEditBlock();
    insertKeepingAnnotations(cursor, QString(QChar(QChar::LineSeparator)), pending);
    cursor.endEditBlock();
    finishEdit(cursor);
    autoCorrectTyped();  // the break ends a word, as a space does
}

void BookEditor::insertCharacters(const QString& characters)
{
    if (refusesChange() || characters.isEmpty()) {
        return;
    }
    ensureDocument();

    // As typed text, in an edit block of its own, which parts it from the text typed before
    // it in the undo history; autocorrect does not look at it
    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    const QTextCharFormat pending = pendingFormat();
    if (hasSelection()) {
        const SelectionRange sel = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
        clearSelection();
    }
    cursor.beginEditBlock();
    insertKeepingAnnotations(cursor, characters, pending);
    cursor.endEditBlock();
    finishEdit(cursor);
}

void BookEditor::deleteBackward()
{
    if (refusesChange()) {
        return;
    }
    ensureDocument();

    if (hasSelection()) {
        deleteSelectedText();
        return;
    }
    if (removeListMarkAtCursor()) {
        return;  // at the start of a list item its number goes, then the item leaves its list
    }

    // If at start of document, nothing to delete
    if (m_cursorPosition.paragraph == 0 && m_cursorPosition.offset == 0) {
        return;
    }
    if (deleteAtObject(true)) {
        return;
    }

    const int oldPara = m_cursorPosition.paragraph;
    const bool wasAtBlockStart = (m_cursorPosition.offset == 0);

    // Removes the previous character, OR merges with the previous paragraph when at the
    // start of a block. Recorded by QTextDocument's native undo.
    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    deleteCharacterKeepingAnnotations(cursor, true);

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
    if (refusesChange()) {
        return;
    }
    ensureDocument();

    if (hasSelection()) {
        deleteSelectedText();
        return;
    }

    if (deleteAtObject(false)) {
        return;
    }

    const int paraLen = paragraphText(m_textBuffer.get(), m_cursorPosition.paragraph).length();
    const bool atBlockEnd = (m_cursorPosition.offset >= paraLen);
    const bool hasNextBlock = (m_cursorPosition.paragraph + 1 < m_textBuffer->blockCount());

    if (!atBlockEnd || hasNextBlock) {
        // Removes the character at the cursor, OR merges with the next paragraph at the
        // end of a block. Recorded by QTextDocument's native undo.
        QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
        deleteCharacterKeepingAnnotations(cursor, false);

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

bool BookEditor::deleteAtObject(bool backward)
{
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    if (!block.isValid()) {
        return false;
    }
    const bool inObject = blockObject(block) != TextObject::None;
    const bool emptyParagraph = !inObject && block.length() == 1;
    // The paragraph break is next to the cursor in the key's direction
    const bool atBreak = backward ? m_cursorPosition.offset == 0
                                  : m_cursorPosition.offset >= block.length() - 1;
    const QTextBlock neighbour = backward ? block.previous() : block.next();
    const bool besideObject = atBreak && blockObject(neighbour) != TextObject::None;

    QTextBlock removed;
    if ((emptyParagraph && besideObject) || (inObject && !atBreak)) {
        removed = block;  // the empty paragraph (not the object next to it), or the object
    } else if (besideObject) {
        removed = neighbour;  // the object next to the cursor
    } else if (inObject && atBreak && neighbour.isValid()) {
        if (neighbour.length() > 1) {
            // An object and text do not join: the cursor goes over the break to the text
            setCursorPosition({neighbour.blockNumber(), backward ? neighbour.length() - 1 : 0});
            return true;
        }
        removed = neighbour;  // an empty paragraph
    } else {
        return false;
    }

    // One undo step; the cursor stays at the text on its side of the removed block
    const int position = block.position() + m_cursorPosition.offset;
    const int removedNumber = removed.blockNumber();
    const bool removedAfter = removed.position() > block.position();
    const bool removedOwnParagraph = removed == block && emptyParagraph;
    QTextCursor cursor(m_textBuffer.get());
    removeBlock(cursor, removed);
    if (removedAfter) {
        cursor.setPosition(position);
    } else if (removedOwnParagraph && backward) {
        cursor.setPosition(position - 1);  // after the object before it
    }
    finishEdit(cursor);
    emit paragraphRemoved(removedNumber);
    return true;
}

void BookEditor::deleteTo(const CursorPosition& position)
{
    if (refusesChange()) {
        return;
    }
    if (hasSelection()) {
        deleteSelectedText();
        return;
    }
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }
    const CursorPosition target = validateCursorPosition(position);
    if (target == m_cursorPosition) {
        return;
    }

    // One step of QTextDocument's native undo
    const int paragraphs = m_textBuffer->blockCount();
    QTextCursor cursor = createCursor(m_textBuffer.get(), std::min(target, m_cursorPosition),
                                      std::max(target, m_cursorPosition));
    removeKeepingAnnotations(cursor);

    m_cursorPosition.paragraph = cursor.blockNumber();
    m_cursorPosition.offset = cursor.positionInBlock();

    ensureCursorVisible();
    syncPipelineCursor();
    update();
    emit contentChanged();
    // The paragraphs joined to the cursor's one go, as with Backspace and Delete
    for (int removed = paragraphs - m_textBuffer->blockCount(); removed > 0; --removed) {
        emit paragraphRemoved(m_cursorPosition.paragraph + 1);
    }
    emit paragraphModified(m_cursorPosition.paragraph);
}

void BookEditor::deleteDiacritic()
{
    if (refusesChange()) {
        return;
    }
    if (hasSelection() || !m_textBuffer || m_cursorPosition.offset == 0) {
        deleteBackward();  // The selection, or the paragraph break before the cursor
        return;
    }

    // The character before the cursor, with its marks, written as a letter and its marks
    const QString text = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph).text();
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    finder.setPosition(m_cursorPosition.offset);
    const qsizetype start = finder.toPreviousBoundary();
    if (start < 0) {
        deleteBackward();
        return;
    }
    QString character = text.mid(start, m_cursorPosition.offset - start)
                            .normalized(QString::NormalizationForm_D);
    if (character.size() < 2 || !character.back().isMark()) {
        deleteBackward();  // No mark: the character goes
        return;
    }
    character.chop(1);

    // The character keeps its format and annotations: one step of the native undo
    QTextCursor cursor = createCursor(m_textBuffer.get(),
                                      {m_cursorPosition.paragraph, static_cast<int>(start)},
                                      m_cursorPosition);
    const QTextCharFormat format = cursor.charFormat();
    cursor.insertText(character.normalized(QString::NormalizationForm_C), format);
    finishEdit(cursor);
}

void BookEditor::killToParagraphEnd()
{
    if (refusesChange()) {
        return;
    }
    if (!m_textBuffer || m_textBuffer->blockCount() == 0) {
        return;
    }

    // The selection; or to the end of the paragraph, or the paragraph break at its end
    const bool selected = hasSelection();
    CursorPosition from = m_cursorPosition;
    CursorPosition to = m_cursorPosition;
    if (selected) {
        const SelectionRange selection = m_selection.normalized();
        from = selection.start;
        to = selection.end;
    } else if (const int length = paragraphLength(m_textBuffer.get(), from.paragraph);
               from.offset < length) {
        to.offset = length;
    } else if (from.paragraph + 1 < m_textBuffer->blockCount()) {
        to = {from.paragraph + 1, 0};
    } else {
        return;  // The end of the text
    }

    QString text = createCursor(m_textBuffer.get(), from, to).selectedText();
    text.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    const bool inARow = !selected && m_killCursor == m_cursorPosition &&
                        m_killRevision == m_textBuffer->revision();
    killedText() = inARow ? killedText() + text : text;

    if (selected) {
        deleteSelectedText();
    } else {
        deleteTo(to);
    }
    m_killCursor = m_cursorPosition;
    m_killRevision = m_textBuffer->revision();
}

void BookEditor::yank()
{
    if (refusesChange()) {
        return;
    }
    if (killedText().isEmpty()) {
        return;
    }
    ensureDocument();

    QTextCursor cursor = createCursor(m_textBuffer.get(), m_cursorPosition);
    if (hasSelection()) {
        const SelectionRange selection = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), selection.start, selection.end);
        clearSelection();
    }
    cursor.beginEditBlock();  // One undo step, apart from the typing before it
    insertKeepingAnnotations(cursor, killedText());
    cursor.endEditBlock();
    finishEdit(cursor);
}

void BookEditor::transposeCharacters()
{
    if (refusesChange()) {
        return;
    }
    if (!m_textBuffer || hasSelection()) {
        return;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    const QString text = block.text();
    const int length = static_cast<int>(text.size());
    if (m_cursorPosition.offset == 0 || length < 2) {
        return;
    }

    // The characters on both sides of the cursor; at the end of the paragraph the two
    // before it
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    const auto boundary = [&finder](int offset, bool forward) {
        finder.setPosition(offset);
        const qsizetype next = forward ? finder.toNextBoundary() : finder.toPreviousBoundary();
        return static_cast<int>(next);
    };
    const int middle = m_cursorPosition.offset < length ? m_cursorPosition.offset
                                                        : boundary(length, false);
    const int first = boundary(middle, false);
    const int last = boundary(middle, true);
    if (first < 0 || last < 0 || first >= middle || last <= middle) {
        return;
    }

    // The second goes before the first, each with its format; the cursor goes past both
    QTextDocument* doc = m_textBuffer.get();
    const int base = block.position();
    QTextCursor second(doc);
    second.setPosition(base + middle);
    second.setPosition(base + last, QTextCursor::KeepAnchor);
    const QTextDocumentFragment moved = second.selection();
    second.beginEditBlock();
    second.removeSelectedText();
    QTextCursor before(doc);
    before.setPosition(base + first);
    before.insertFragment(moved);
    second.endEditBlock();

    QTextCursor after(doc);
    after.setPosition(base + last);
    finishEdit(after);
}

// =============================================================================
// Undo/Redo (Phase 4.8)
// =============================================================================

bool BookEditor::canUndo() const
{
    return !m_readOnly && m_textBuffer && m_textBuffer->isUndoAvailable();
}

bool BookEditor::canRedo() const
{
    return !m_readOnly && m_textBuffer && m_textBuffer->isRedoAvailable();
}

void BookEditor::undo()
{
    if (refusesChange()) {
        return;
    }
    if (!m_textBuffer || !m_textBuffer->isUndoAvailable()) {
        return;
    }

    // QTextDocument's native undo is the single source of truth for BOTH text and
    // formatting. undo(&cursor) also positions the cursor at the change — mirror it
    // into the editor's own cursor model. A format given for the text typed next goes.
    // A change of autocorrect undone right after it: nothing was done since (no new undo
    // step, nothing undone before)
    std::optional<LearnableChange> learned;
    if (m_learnableChange &&
        m_textBuffer->availableUndoSteps() == m_learnableChange->undoSteps &&
        m_textBuffer->availableRedoSteps() == 0) {
        learned = m_learnableChange;
    }
    m_learnableChange.reset();

    m_stepCursor.reset();
    m_pendingFormat = QTextCharFormat();
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
    if (learned) {
        emit autoCorrectionUndone(learned->list, learned->exception);
    }
}

void BookEditor::redo()
{
    if (refusesChange()) {
        return;
    }
    if (!m_textBuffer || !m_textBuffer->isRedoAvailable()) {
        return;
    }

    m_stepCursor.reset();
    m_pendingFormat = QTextCharFormat();  // as undo() does
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

void BookEditor::undoWithoutRedo()
{
    if (refusesChange()) {
        return;
    }
    if (!m_textBuffer || !m_textBuffer->isUndoAvailable()) {
        return;
    }
    undo();
    m_textBuffer->clearUndoRedoStacks(QTextDocument::RedoStack);
}

void BookEditor::clearUndoStack()
{
    if (m_textBuffer) {
        // QTextDocument would take the start of the new history for the saved content: it is
        // told whether the content is the saved one (setModified() does nothing when the
        // document already says so, hence the opposite first)
        const bool modified = isModified();
        m_textBuffer->clearUndoRedoStacks();
        m_learnableChange.reset();  // no longer undone
        m_textBuffer->setModified(!modified);
        m_textBuffer->setModified(modified);
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
    if (refusesChange()) {
        return;
    }
    if (!hasSelection() || !m_textBuffer) {
        return;
    }

    // Copy first
    copy();

    // Then delete selection (one undo step). The annotations go to the clipboard with
    // the text: pasted, they come back.
    removeSelection(false);
}

void BookEditor::paste()
{
    insertFromMimeData(QGuiApplication::clipboard()->mimeData());
}

void BookEditor::pasteAs(PasteMode mode)
{
    const QMimeData* clipboard = QGuiApplication::clipboard()->mimeData();
    if (mode != PasteMode::Text || clipboard == nullptr ||
        !clipboard->hasFormat(QString::fromLatin1(MIME_KML))) {
        insertMimeDataAs(clipboard, mode);
        return;
    }
    // Kalahari's own text without its formatting: its plain text, with the HTML that tells
    // its line breaks from its paragraph ends
    QMimeData plain;
    plain.setText(clipboard->text());
    if (clipboard->hasHtml()) {
        plain.setHtml(clipboard->html());
    }
    insertMimeDataAs(&plain, mode);
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

    // The list items with the numbers the editor shows: a part of a list goes on from the
    // number of its first item
    ListMarks marks;
    const std::vector<QString> shownMarks = listMarkers(*m_textBuffer);
    const std::vector<int> shownNumbers = listNumbers(*m_textBuffer);
    const QTextBlock lastBlock = m_textBuffer->findBlock(range.selectionEnd());
    for (QTextBlock block = m_textBuffer->findBlock(range.selectionStart()); block.isValid();
         block = block.next()) {
        if (blockObject(block) == TextObject::None && isListItem(block.blockFormat())) {
            const auto index = static_cast<size_t>(block.blockNumber());
            marks.marks.push_back(index < shownMarks.size() ? shownMarks[index] : QString());
            marks.numbers.push_back(index < shownNumbers.size() ? shownNumbers[index] : -1);
        }
        if (block == lastBlock) {
            break;
        }
    }

    // Plain text: the paragraphs and the line breaks in them end a line
    auto mimeData = std::make_unique<QMimeData>();
    mimeData->setText(ClipboardHandler::kmlToText(kml, QString(), &marks));
    mimeData->setHtml(
        ClipboardHandler::htmlDocument(ClipboardHandler::kmlToHtml(kml, QString(), &marks)));
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
    insertMimeDataAs(source, m_pasteMode);
}

void BookEditor::insertMimeDataAs(const QMimeData* source, PasteMode mode)
{
    if (refusesChange()) {
        return;
    }
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
    const QTextCharFormat pending = pendingFormat();
    if (hasSelection()) {
        const SelectionRange sel = m_selection.normalized();
        cursor = createCursor(m_textBuffer.get(), sel.start, sel.end);
        clearSelection();
    }
    insertMimeData(cursor, *source, mode, pending);
    finishEdit(cursor);
}

void BookEditor::setPasteMode(PasteMode mode)
{
    m_pasteMode = mode;
}

bool BookEditor::dropMimeData(const QMimeData* source, const CursorPosition& position,
                              bool moveSelection)
{
    if (refusesChange()) {
        return false;
    }
    if (!canInsertFromMimeData(source)) {
        return false;
    }
    ensureDocument();
    if (moveSelection && (!hasSelection() || isInSelection(position))) {
        return false;
    }

    const SelectionRange moved = m_selection.normalized();
    clearSelection();

    // One undo step: moved text leaves its place, with its annotations, and lands at the
    // drop point. Both cursors stay at the same text while the other one edits the document.
    QTextCursor cursor = createCursor(m_textBuffer.get(), validateCursorPosition(position));
    QTextCursor oldPlace;
    cursor.beginEditBlock();
    if (moveSelection) {
        oldPlace = createCursor(m_textBuffer.get(), moved.start, moved.end);
        removeSelectionKeepingObjects(oldPlace);
    }
    const int insertionStart = cursor.position();
    insertMimeData(cursor, *source, m_pasteMode);
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
    return !m_readOnly && ClipboardHandler::canPaste();
}

}  // namespace kalahari::editor
