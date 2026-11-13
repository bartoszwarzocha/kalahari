/// @file book_editor_captions.cpp
/// @brief BookEditor: numbered captions of images, cross-references and the list of figures

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/editor/annotation.h>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/text_images.h>
#include <QScopedValueRollback>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTimer>
#include <algorithm>

namespace kalahari::editor {

namespace {

/// The layout of a document; nullptr: another one
KalahariTextDocumentLayout* layoutOf(const QTextDocument* document)
{
    return document != nullptr
               ? qobject_cast<KalahariTextDocumentLayout*>(document->documentLayout())
               : nullptr;
}

/// The first image of a block; std::nullopt: none
std::optional<int> firstImageOf(const QTextDocument& document, const QTextBlock& block)
{
    if (!block.isValid()) {
        return std::nullopt;
    }
    const std::vector<int> images =
        imagePositions(document, block.position(), block.position() + block.length() - 1);
    return images.empty() ? std::nullopt : std::optional<int>(images.front());
}

/// The text of [from, to) of a block as it is shown: its fields as their text, without
/// images and objects
QString shownText(const QTextBlock& block, int from, int to, const FieldTextOf& fieldText)
{
    QString text;
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid()) {
            continue;
        }
        const int start = std::max(from, fragment.position());
        const int end = std::min(to, fragment.position() + fragment.length());
        if (start >= end) {
            continue;
        }
        const QTextCharFormat format = fragment.charFormat();
        const QString part = fragment.text().mid(start - fragment.position(), end - start);
        if (isFieldFormat(format)) {
            for (int i = 0; i < part.size(); ++i) {
                text += fieldText(start + i, format).text;
            }
        } else if (format.objectType() == QTextFormat::NoObject) {
            text += part;
        }
    }
    return text.replace(QChar::LineSeparator, QLatin1Char(' '));
}

/// The text a caption is written with: one line, without the spaces at its ends
QString captionTextOf(const QString& text)
{
    QString line = text;
    line.replace(QLatin1Char('\n'), QLatin1Char(' '))
        .replace(QLatin1Char('\r'), QLatin1Char(' '))
        .replace(QChar::LineSeparator, QLatin1Char(' '))
        .replace(QChar::ParagraphSeparator, QLatin1Char(' '))
        .remove(QChar::ObjectReplacementCharacter);
    return line.trimmed();
}

/// The ids every number of the book has: the document's and those of the texts around it
QSet<QString> bookCaptionIds(const QTextDocument& document, const CaptionContext& context)
{
    QSet<QString> ids = captionIds(document);
    for (const std::vector<BookCaption>* captions : {&context.before, &context.after}) {
        for (const BookCaption& caption : *captions) {
            ids.insert(caption.id);
        }
    }
    return ids;
}

}  // namespace

// =============================================================================
// The numbers in the book
// =============================================================================

void BookEditor::setCaptionContext(const CaptionContext& context)
{
    m_captionContext = context;
    if (KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get())) {
        layout->setCaptionContext(context);
    }
}

const CaptionContext& BookEditor::captionContext() const
{
    return m_captionContext;
}

const CaptionNumbers& BookEditor::captionNumbers() const
{
    static const CaptionNumbers none;
    const KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    return layout != nullptr ? layout->captionNumbers() : none;
}

std::vector<CaptionEntry> BookEditor::captionEntries() const
{
    std::vector<CaptionEntry> entries;
    for (const BookCaption& caption : captionNumbers().own()) {
        entries.push_back({caption.id, caption.label, caption.pieces, caption.position});
    }
    return entries;
}

void BookEditor::checkCaptionsSoon()
{
    // Once after the edits of a step (an undo step of many changes is one check)
    if (m_captionsCheckPending) {
        return;
    }
    m_captionsCheckPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_captionsCheckPending = false;
        std::vector<CaptionEntry> entries = captionEntries();
        // Their places move as the text before them changes: only what they are counts
        for (CaptionEntry& entry : entries) {
            entry.position = -1;
        }
        if (entries != m_shownCaptions) {
            m_shownCaptions = std::move(entries);
            emit captionsChanged();
        }
    });
}

int BookEditor::crossReferenceCount(const QSet<QString>& ids) const
{
    if (!m_textBuffer || ids.isEmpty()) {
        return 0;
    }
    int count = 0;
    for (QTextBlock block = m_textBuffer->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            if (const std::optional<CrossReference> reference = crossReferenceOf(fragment.charFormat());
                reference && ids.contains(reference->target)) {
                count += fragment.length();
            }
        }
    }
    return count;
}

// =============================================================================
// The caption of an image
// =============================================================================

std::optional<int> BookEditor::captionTargetImage() const
{
    if (!m_textBuffer) {
        return std::nullopt;
    }
    if (const std::optional<int> selected = selectedImage()) {
        return selected;
    }
    if (hasSelection()) {
        return std::nullopt;
    }
    const QTextDocument& document = *m_textBuffer;
    const QTextBlock block = document.findBlockByNumber(m_cursorPosition.paragraph);
    if (!block.isValid()) {
        return std::nullopt;
    }
    // Just before the cursor, then in the cursor's paragraph
    const int position = block.position() + m_cursorPosition.offset;
    if (m_cursorPosition.offset > 0 && editor::imageAt(document, position - 1)) {
        return position - 1;
    }
    if (const std::optional<int> image = firstImageOf(document, block)) {
        return image;
    }
    // The cursor in a caption: its image
    return firstImageOf(document, imageBlockOfCaption(block));
}

std::optional<ImageCaption> BookEditor::imageCaptionOf(int position) const
{
    if (!m_textBuffer || !editor::imageAt(*m_textBuffer, position)) {
        return std::nullopt;
    }
    ImageCaption caption;
    const QTextBlock block = captionBlockOf(m_textBuffer->findBlock(position));
    if (!block.isValid()) {
        return caption;
    }
    caption.shown = true;
    caption.above = isCaptionAbove(block.blockFormat());
    const CaptionParts parts = captionParts(block);
    caption.numbered = parts.numberAt >= 0;
    caption.label = parts.number.label;
    const KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    const CaptionNumbers& numbers = captionNumbers();
    caption.text = captionTextOf(shownText(
        block, parts.textAt, block.position() + block.length() - 1,
        [layout, &numbers](int at, const QTextFormat& format) {
            return layout != nullptr ? layout->fieldText(at, format) : numbers.fieldText(at, format);
        }));
    return caption;
}

QString BookEditor::captionNumberAt(int position, const QString& label) const
{
    if (!m_textBuffer) {
        return QString();
    }
    const CaptionNumbers& numbers = captionNumbers();
    const QTextBlock imageBlock = m_textBuffer->findBlock(position);
    const QTextBlock caption = captionBlockOf(imageBlock);
    const CaptionParts parts = caption.isValid() ? captionParts(caption) : CaptionParts{};
    if (parts.numberAt >= 0 && parts.number.label == label) {
        return numbers.fieldText(parts.numberAt, captionNumberCharFormat(parts.numberAt)).text;
    }

    // The number it would get: after the captions of the label before it
    const CaptionNumbering numbering = m_captionContext.numberingOf(label);
    int counted = 0;
    if (const auto given = m_captionContext.counted.constFind(label);
        given != m_captionContext.counted.cend()) {
        counted = *given;
    } else if (!numbering.perChapter) {
        counted = static_cast<int>(std::count_if(
            m_captionContext.before.begin(), m_captionContext.before.end(),
            [&label](const BookCaption& before) { return before.label == label; }));
    }
    const int place = caption.isValid() && isCaptionAbove(caption.blockFormat())
                          ? caption.position()
                          : imageBlock.position();
    for (const BookCaption& own : numbers.own()) {
        if (own.label == label && own.position < place && own.position != parts.numberAt) {
            ++counted;
        }
    }
    return captionNumberText(counted + 1, numbering, m_captionContext.chapter);
}

QTextCharFormat BookEditor::captionNumberCharFormat(int position) const
{
    QTextCursor cursor(m_textBuffer.get());
    cursor.setPosition(position);
    cursor.setPosition(position + 1, QTextCursor::KeepAnchor);
    return cursor.charFormat();
}

bool BookEditor::confirmCaptionRemoval(const QTextBlock& caption)
{
    const QStringList ids = captionIdsOf(caption);
    return ids.isEmpty() || m_captionRemovalAsked || !m_confirmCaptionRemoval ||
           m_confirmCaptionRemoval(ids);
}

bool BookEditor::setImageCaption(int position, const ImageCaption& caption)
{
    const std::optional<ImageCaption> current = imageCaptionOf(position);
    if (!current) {
        return false;
    }
    ImageCaption wanted = caption;
    wanted.label = normalizedLabel(wanted.label);
    wanted.text = captionTextOf(wanted.text);
    if (wanted.numbered && wanted.label.isEmpty()) {
        wanted.numbered = false;  // a number counts a label
    }
    if (!wanted.shown && !current->shown) {
        return true;
    }
    if (wanted.shown && current->shown && wanted.numbered == current->numbered &&
        wanted.above == current->above && wanted.text == current->text &&
        (!wanted.numbered || wanted.label == current->label)) {
        return true;
    }
    if (refusesChange()) {
        return false;
    }
    QTextDocument* document = m_textBuffer.get();
    const QTextBlock imageBlock = document->findBlock(position);
    QTextBlock block = captionBlockOf(imageBlock);

    // Its number goes: the cross-references to it would show "?"
    if (block.isValid() && (!wanted.shown || !wanted.numbered) && !confirmCaptionRemoval(block)) {
        return false;
    }

    // One undo step: undone, it brings back the cursor and the selection it was made with
    const StepCursor before{m_cursorPosition, m_selection};
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, before] { m_stepCursor = before; }));
    QTextCursor image(document);  // follows the image as the text before it changes
    image.setPosition(position);

    if (!wanted.shown) {
        removeBlock(cursor, block);
        cursor.setPosition(image.position() + 1);
    } else {
        // On the other side of the image: the paragraph moves, with its formatting
        if (block.isValid() && isCaptionAbove(block.blockFormat()) != wanted.above) {
            QTextCursor content(document);
            content.setPosition(block.position());
            content.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
            const QTextDocumentFragment fragment = content.selection();
            QTextBlockFormat format = block.blockFormat();
            setCaptionAbove(format, wanted.above);
            const QTextCharFormat blockCharFormat = block.charFormat();
            removeBlock(cursor, block);
            block = insertCaptionBlock(cursor, image.block(), format, blockCharFormat, wanted.above);
            cursor.insertFragment(fragment);
            block = cursor.block();
        }
        if (!block.isValid()) {
            block = newCaptionBlock(cursor, image.block(), wanted.above);
        }
        writeCaption(cursor, block, wanted, *current);
        cursor.setPosition(block.position() + block.length() - 1);
    }

    const CursorPosition after = absoluteToCursorPosition(document, cursor.position());
    document->appendUndoItem(
        new CallbackUndoItem([this, state = StepCursor{after, {}}] { m_stepCursor = state; }));
    cursor.endEditBlock();
    m_learnableChange.reset();

    clearSelection();
    m_selectionAnchor = after;
    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    finishEdit(cursor);
    return true;
}

QTextBlock BookEditor::newCaptionBlock(QTextCursor& cursor, const QTextBlock& imageBlock, bool above)
{
    QTextBlockFormat format = paragraphBlockFormat();
    setParagraphStyle(format, QLatin1String(CAPTION_STYLE_ID));
    if (const Qt::Alignment own = ownAlignment(imageBlock.blockFormat()); own) {
        format.setAlignment(own);  // as the image (the style centers a caption)
    }
    setCaptionAbove(format, above);
    return insertCaptionBlock(cursor, imageBlock, format, QTextCharFormat(), above);
}

QTextBlock BookEditor::insertCaptionBlock(QTextCursor& cursor, const QTextBlock& imageBlock,
                                          const QTextBlockFormat& format,
                                          const QTextCharFormat& charFormat, bool above)
{
    if (!above) {
        // After the image's paragraph
        cursor.setPosition(imageBlock.position() + imageBlock.length() - 1);
        cursor.insertBlock(format, charFormat);
        return cursor.block();
    }
    // Before it: the image's paragraph goes on in a block of its own, with its formats, and
    // the one left before it becomes the caption
    const QTextBlockFormat imageFormat = imageBlock.blockFormat();
    const QTextCharFormat imageCharFormat = imageBlock.charFormat();
    cursor.setPosition(imageBlock.position());
    cursor.insertBlock(imageFormat, imageCharFormat);
    QTextCursor caption(cursor.block().previous());
    caption.setBlockFormat(format);
    caption.setBlockCharFormat(charFormat);
    cursor.setPosition(caption.block().position());
    return caption.block();
}

void BookEditor::writeCaption(QTextCursor& cursor, const QTextBlock& block,
                              const ImageCaption& wanted, const ImageCaption& current)
{
    const CaptionParts parts = captionParts(block);
    const int end = block.position() + block.length() - 1;

    // The text after the number: written anew only when it changed (its formatting stays)
    if (wanted.text != current.text || !current.shown) {
        cursor.setPosition(parts.textAt);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
        const QTextCharFormat format = insertionFormat(cursor);
        cursor.removeSelectedText();
        cursor.insertText(wanted.text, format);
    }

    // The label and the number before it: "Figure 3. " (a number keeps its id)
    cursor.setPosition(block.position());
    cursor.setPosition(parts.textAt, QTextCursor::KeepAnchor);
    QTextCursor place(cursor.document());
    place.setPosition(block.position());
    QTextCharFormat textFormat = insertionFormat(place);
    clearField(textFormat);
    cursor.removeSelectedText();
    if (!wanted.numbered) {
        return;
    }
    CaptionNumberField number = parts.number;
    if (number.id.isEmpty()) {
        number.id = newCaptionId(bookCaptionIds(*cursor.document(), m_captionContext));
    }
    number.label = wanted.label;
    QTextCharFormat numberFormat = textFormat;
    setCaptionNumber(numberFormat, number);
    cursor.insertText(wanted.label + QLatin1Char(' '), textFormat);
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), numberFormat);
    cursor.insertText(wanted.text.isEmpty() ? QStringLiteral(".") : QStringLiteral(". "),
                      textFormat);
}

bool BookEditor::removeImageWithCaption(int position)
{
    if (!m_textBuffer || !editor::imageAt(*m_textBuffer, position) || refusesChange()) {
        return false;
    }
    QTextDocument* document = m_textBuffer.get();
    const QTextBlock caption = captionBlockOf(document->findBlock(position));
    if (!caption.isValid()) {
        return false;
    }
    if (!confirmCaptionRemoval(caption)) {
        return true;  // the writer kept it
    }

    // One undo step: the image and its caption; the cursor where the image was
    const StepCursor before{m_cursorPosition, m_selection};
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, before] { m_stepCursor = before; }));
    QTextCursor image(document);  // follows the image as the caption before it goes
    image.setPosition(position);
    removeBlock(cursor, caption);
    cursor.setPosition(image.position());
    cursor.setPosition(image.position() + 1, QTextCursor::KeepAnchor);
    removeKeepingAnnotations(cursor);
    const CursorPosition after = absoluteToCursorPosition(document, cursor.position());
    document->appendUndoItem(
        new CallbackUndoItem([this, state = StepCursor{after, {}}] { m_stepCursor = state; }));
    cursor.endEditBlock();
    m_learnableChange.reset();

    clearSelection();
    m_selectionAnchor = after;
    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    finishEdit(cursor);
    return true;
}

// =============================================================================
// Cross-references and the list of figures
// =============================================================================

bool BookEditor::insertCrossReference(const CrossReference& reference)
{
    if (reference.target.isEmpty() || refusesChange()) {
        return false;
    }
    ensureDocument();
    QTextDocument* document = m_textBuffer.get();

    // One undo step with the selection it replaces
    const StepCursor before{m_cursorPosition, m_selection};
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
    setCrossReference(format, reference);
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), format);
    const CursorPosition after = absoluteToCursorPosition(document, cursor.position());
    document->appendUndoItem(
        new CallbackUndoItem([this, state = StepCursor{after, {}}] { m_stepCursor = state; }));
    step.endEditBlock();
    m_learnableChange.reset();

    m_selectionAnchor = after;
    finishEdit(cursor);
    return true;
}

bool BookEditor::insertFigureList(const QString& title)
{
    if (refusesChange() || isInTable()) {
        return false;
    }
    ensureDocument();
    QTextDocument* document = m_textBuffer.get();
    QTextCursor cursor = createCursor(document, m_cursorPosition);
    if (hasSelection()) {
        cursor = selectionToReplace();
        clearSelection();
    }

    // The replaced selection, the heading and the list: one undo step. The paragraph after
    // the list is in the style Enter would start there.
    cursor.beginEditBlock();
    removeKeepingAnnotations(cursor);
    QTextBlockFormat nextFormat = paragraphBlockFormat();
    setParagraphStyle(nextFormat,
                      m_paragraphStyles.nextStyleOf(paragraphStyleOf(cursor.blockFormat())));
    const QString heading = captionTextOf(title);
    if (!heading.isEmpty()) {
        // A heading of the list, unless a heading stands right before it
        const QTextBlock block = cursor.block();
        const bool atStart = cursor.positionInBlock() == 0;
        const QTextBlock before = atStart ? block.previous() : block;
        const bool afterHeading =
            before.isValid() && headingLevelOf(paragraphStyleOf(before.blockFormat())) > 0 &&
            (atStart || cursor.positionInBlock() == block.length() - 1);
        if (!afterHeading) {
            QTextBlockFormat headingFormat = paragraphBlockFormat();
            setParagraphStyle(headingFormat, headingStyleId(2));
            if (block.length() > 1 && atStart) {
                // Before the paragraph: the heading, then the list at the paragraph's start
                cursor.insertBlock(block.blockFormat(), block.charFormat());
                cursor.movePosition(QTextCursor::PreviousBlock);
                cursor.setBlockFormat(headingFormat);
                cursor.setBlockCharFormat(QTextCharFormat());
                cursor.insertText(heading, QTextCharFormat());
                cursor.movePosition(QTextCursor::NextBlock);
            } else {
                // After the paragraph's text, or in place of an empty paragraph
                if (block.length() > 1) {
                    cursor.movePosition(QTextCursor::EndOfBlock);
                    cursor.insertBlock(headingFormat, QTextCharFormat());
                } else {
                    cursor.setBlockFormat(headingFormat);
                }
                cursor.insertText(heading, QTextCharFormat());
                cursor.insertBlock(nextFormat, QTextCharFormat());
            }
        }
    }
    insertObject(cursor, TextObject::FigureList, nextFormat);
    cursor.endEditBlock();
    finishEdit(cursor);
    return true;
}

bool BookEditor::goToCaption(const QString& id)
{
    if (!m_textBuffer) {
        return false;
    }
    for (const BookCaption& caption : captionNumbers().own()) {
        if (caption.id != id || caption.position < 0) {
            continue;
        }
        const QTextBlock block = m_textBuffer->findBlock(caption.position);
        if (const std::optional<int> image = firstImageOf(*m_textBuffer, imageBlockOfCaption(block))) {
            selectImage(*image);
        } else {
            clearSelection();
            const CursorPosition place = absoluteToCursorPosition(m_textBuffer.get(), caption.position);
            m_selectionAnchor = place;
            setCursorPosition(place);
        }
        ensureCursorVisible();
        update();
        return true;
    }
    return false;
}

std::optional<BookEditor::CaptionLink> BookEditor::captionLinkAt(const QPointF& widgetPos) const
{
    const KalahariTextDocumentLayout* layout = layoutOf(m_textBuffer.get());
    if (layout == nullptr || !m_renderPipeline) {
        return std::nullopt;
    }
    const CaptionNumbers& numbers = layout->captionNumbers();
    const QPointF docPoint = m_renderPipeline->widgetToDocument(widgetPos);

    // A cross-reference that is a link: the character under the point
    const int hit = layout->hitTest(docPoint, Qt::ExactHit);
    if (hit >= 0) {
        const QTextFormat format = captionNumberCharFormat(hit);
        if (const std::optional<CrossReference> reference = crossReferenceOf(format);
            reference && reference->link) {
            if (const BookCaption* caption = numbers.find(reference->target)) {
                return CaptionLink{caption->id, caption->elementId};
            }
            return std::nullopt;  // a caption the book does not have
        }
    }

    // A row of a list of figures
    const int number = layout->blockNumberAt(docPoint);
    const QTextBlock block = m_textBuffer->findBlockByNumber(number);
    if (!block.isValid() || blockObject(block) != TextObject::FigureList) {
        return std::nullopt;
    }
    const QPointF inBlock(docPoint.x(), docPoint.y() - layout->blockY(number));
    if (const std::optional<FigureListRow> row = layout->figureListRowAt(block.position(), inBlock)) {
        return CaptionLink{row->id, row->elementId};
    }
    return std::nullopt;
}

void BookEditor::followCaptionLink(const CaptionLink& link)
{
    if (link.elementId.isEmpty()) {
        goToCaption(link.captionId);
    } else {
        emit captionOpenAsked(link.elementId, link.captionId);
    }
}

// =============================================================================
// Pasted numbers
// =============================================================================

void BookEditor::renewPastedCaptionIds(int from, int to)
{
    if (!m_textBuffer || from >= to) {
        return;
    }
    // The ids the text had outside the pasted part, and those of the book around it: a copy of
    // a caption gets a number of its own (a caption cut and pasted keeps its id: it is gone)
    QTextDocument* document = m_textBuffer.get();
    QSet<QString> taken;
    for (const std::vector<BookCaption>* captions : {&m_captionContext.before, &m_captionContext.after}) {
        for (const BookCaption& caption : *captions) {
            taken.insert(caption.id);
        }
    }
    std::vector<std::pair<int, CaptionNumberField>> pasted;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            const std::optional<CaptionNumberField> number = captionNumberOf(fragment.charFormat());
            if (!number) {
                continue;
            }
            for (int i = 0; i < fragment.length(); ++i) {
                const int position = fragment.position() + i;
                if (position >= from && position < to) {
                    pasted.emplace_back(position, *number);
                } else {
                    taken.insert(number->id);
                }
            }
        }
    }
    QTextCursor cursor(document);
    bool joined = false;
    for (auto& [position, number] : pasted) {
        if (!number.id.isEmpty() && !taken.contains(number.id)) {
            taken.insert(number.id);
            continue;
        }
        number.id = newCaptionId(taken);
        taken.insert(number.id);
        if (!joined) {
            cursor.joinPreviousEditBlock();  // the paste's undo step
            joined = true;
        }
        cursor.setPosition(position);
        cursor.setPosition(position + 1, QTextCursor::KeepAnchor);
        QTextCharFormat format = cursor.charFormat();
        setCaptionNumber(format, number);
        cursor.setCharFormat(format);
    }
    if (joined) {
        cursor.endEditBlock();
    }
}

}  // namespace kalahari::editor
