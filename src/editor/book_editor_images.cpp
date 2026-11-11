/// @file book_editor_images.cpp
/// @brief BookEditor: images of the text, Insert > Image and the commands of an image

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/image_store.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/text_images.h>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <algorithm>

namespace kalahari::editor {

namespace {

/// @brief The block after an image's block when it is its caption: in the style Caption
QTextBlock captionBlockOf(const QTextBlock& imageBlock)
{
    const QTextBlock next = imageBlock.next();
    if (!next.isValid() || tableOf(next) != tableOf(imageBlock) ||
        paragraphStyleOf(next.blockFormat()) != QLatin1String(CAPTION_STYLE_ID)) {
        return QTextBlock();
    }
    // In a table: in the same cell
    if (const QTextTable* table = tableOf(imageBlock);
        table != nullptr &&
        table->cellAt(next.position()) != table->cellAt(imageBlock.position())) {
        return QTextBlock();
    }
    return next;
}

}  // namespace

void BookEditor::setImagesFolder(const QString& folder)
{
    const QString before = m_imageStore->folder();
    m_imageStore->setFolder(folder);
    if (m_imageStore->folder() == before || !m_textBuffer) {
        return;
    }
    if (auto* layout = qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())) {
        layout->refreshImages();  // their files' sizes
    }
    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    update();
}

QString BookEditor::imagesFolder() const
{
    return m_imageStore->folder();
}

bool BookEditor::canInsertImage() const
{
    return !m_readOnly && m_imageStore->hasFolder();
}

bool BookEditor::insertImageFile(const QString& path)
{
    if (!m_imageStore->hasFolder() || refusesChange() || !ImageStore::fileSize(path)) {
        return false;
    }
    ImageProperties image;
    image.source = m_imageStore->addFile(path);
    return !image.source.isEmpty() && insertImage(image);
}

bool BookEditor::insertPicture(const QImage& picture)
{
    if (!m_imageStore->hasFolder() || picture.isNull() || refusesChange()) {
        return false;
    }
    ImageProperties image;
    image.source = m_imageStore->addPicture(picture, QStringLiteral("image"));
    return !image.source.isEmpty() && insertImage(image);
}

bool BookEditor::insertImage(const ImageProperties& image)
{
    if (image.source.isEmpty() || refusesChange()) {
        return false;
    }
    ensureDocument();
    QTextDocument* document = m_textBuffer.get();

    // One undo step: undone, it brings back the cursor and the selection it was made with;
    // redone, the image selected
    const StepCursor before{m_cursorPosition, m_selection};
    QTextCursor step(document);
    step.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, before] { m_stepCursor = before; }));
    QTextCursor cursor = createCursor(document, m_cursorPosition);
    bool replacing = false;
    AnnotationList fragments;
    if (hasSelection()) {
        cursor = selectionToReplace();
        clearSelection();
        replacing = cursor.hasSelection();
        if (replacing) {
            fragments = removeForReplacement(cursor);  // next to an object: a new paragraph
        }
    } else {
        leaveObject(cursor);  // never in the block of a line or a break
    }

    // No formatting of its own: only the fragments of annotations and the link of its place
    const QTextCharFormat place = insertionFormat(cursor);
    QTextCharFormat format = imageCharFormat(image);
    setAnnotations(format, replacing ? fragments : annotationsOf(place));
    setLinkAddress(format, linkAddressOf(place));

    // An image of its own paragraph stands in the middle
    if (cursor.block().length() <= 1) {
        QTextBlockFormat centered;
        centered.setAlignment(Qt::AlignHCenter);
        cursor.mergeBlockFormat(centered);
    }
    const int position = cursor.position();
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), format);
    const SelectionRange selected{absoluteToCursorPosition(document, position),
                                  absoluteToCursorPosition(document, position + 1)};
    document->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{selected.end, selected}] { m_stepCursor = state; }));
    step.endEditBlock();
    m_learnableChange.reset();

    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    finishEdit(cursor);
    selectImage(position);
    return true;
}

std::optional<int> BookEditor::selectedImage() const
{
    if (!m_textBuffer || !hasSelection()) {
        return std::nullopt;
    }
    const SelectionRange sel = m_selection.normalized();
    const int from = calculateAbsolutePosition(sel.start);
    const int to = calculateAbsolutePosition(sel.end);
    if (to - from != 1 || !editor::imageAt(*m_textBuffer, from)) {
        return std::nullopt;
    }
    return from;
}

std::optional<int> BookEditor::imagePositionAt(const QPointF& widgetPos) const
{
    if (!m_textBuffer || !m_renderPipeline) {
        return std::nullopt;
    }
    const CursorPosition place = positionFromPoint(widgetPos);
    const QTextBlock block = m_textBuffer->findBlockByNumber(place.paragraph);
    if (!block.isValid()) {
        return std::nullopt;
    }
    // The point is nearest to a place next to the image: before it or after it
    for (const int offset : {place.offset - 1, place.offset}) {
        if (offset < 0 || offset >= block.length() - 1) {
            continue;
        }
        const int position = block.position() + offset;
        if (const std::optional<QRectF> rect = imageRect(position); rect && rect->contains(widgetPos)) {
            return position;
        }
    }
    return std::nullopt;
}

void BookEditor::selectImage(int position)
{
    if (!m_textBuffer || !editor::imageAt(*m_textBuffer, position)) {
        return;
    }
    const CursorPosition start = absoluteToCursorPosition(m_textBuffer.get(), position);
    const CursorPosition end = absoluteToCursorPosition(m_textBuffer.get(), position + 1);
    m_selectionAnchor = start;
    setCursorPosition(end);
    setSelection({start, end});
    update();
}

std::optional<QRectF> BookEditor::imageRect(int position) const
{
    if (!m_textBuffer || !m_renderPipeline) {
        return std::nullopt;
    }
    auto* layout = qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout());
    const QTextBlock block = m_textBuffer->findBlock(position);
    if (layout == nullptr || !block.isValid() || !editor::imageAt(*m_textBuffer, position)) {
        return std::nullopt;
    }
    KalahariTextDocumentLayout::blockLayout(block);  // its lines, laid out first if waiting
    const std::optional<QRectF> local = layout->imageRect(position);
    if (!local) {
        return std::nullopt;
    }
    return m_renderPipeline->paragraphRectToWidget(static_cast<size_t>(block.blockNumber()), *local);
}

bool BookEditor::setImageProperties(int position, const ImageProperties& image)
{
    if (!m_textBuffer || image.source.isEmpty()) {
        return false;
    }
    const std::optional<ImageProperties> old = editor::imageAt(*m_textBuffer, position);
    const ImageProperties changed = normalizedImage(image);
    if (!old || *old == changed || refusesChange()) {
        return false;
    }
    QTextDocument* document = m_textBuffer.get();

    // One undo step, the image selected before and after it
    const StepCursor before{m_cursorPosition, m_selection};
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, before] { m_stepCursor = before; }));
    cursor.setPosition(position);
    cursor.setPosition(position + 1, QTextCursor::KeepAnchor);
    QTextCharFormat format = cursor.charFormat();
    setImage(format, changed);
    cursor.setCharFormat(format);
    const SelectionRange selected{absoluteToCursorPosition(document, position),
                                  absoluteToCursorPosition(document, position + 1)};
    document->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{selected.end, selected}] { m_stepCursor = state; }));
    cursor.endEditBlock();
    m_learnableChange.reset();

    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    update();
    emit contentChanged();
    emit paragraphModified(selected.start.paragraph);
    selectImage(position);
    return true;
}

bool BookEditor::setImageOriginalSize(int position)
{
    std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, position) : std::nullopt;
    if (!image) {
        return false;
    }
    image->unit = ImageWidthUnit::Natural;
    image->width = 0.0;
    return setImageProperties(position, *image);
}

bool BookEditor::changeImageFile(int position, const QString& path)
{
    std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, position) : std::nullopt;
    if (!image || !m_imageStore->hasFolder() || refusesChange() || !ImageStore::fileSize(path)) {
        return false;
    }
    const QString source = m_imageStore->addFile(path);
    if (source.isEmpty()) {
        return false;
    }
    image->source = source;
    return setImageProperties(position, *image);
}

bool BookEditor::addImageCaption(int position)
{
    if (!m_textBuffer || !editor::imageAt(*m_textBuffer, position)) {
        return false;
    }
    QTextDocument* document = m_textBuffer.get();
    const QTextBlock block = document->findBlock(position);
    if (const QTextBlock caption = captionBlockOf(block); caption.isValid()) {
        // The caption it has: the cursor at its end
        clearSelection();
        const CursorPosition end{caption.blockNumber(), caption.length() - 1};
        m_selectionAnchor = end;
        setCursorPosition(end);
        update();
        return true;
    }
    if (refusesChange()) {
        return false;
    }

    // A new paragraph in the style Caption after the image's, in the middle as the image
    // (a caption is centered by its style; the image's paragraph may be aligned otherwise)
    const StepCursor before{m_cursorPosition, m_selection};
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, before] { m_stepCursor = before; }));
    cursor.setPosition(block.position() + block.length() - 1);
    QTextBlockFormat captionFormat = paragraphBlockFormat();
    setParagraphStyle(captionFormat, QLatin1String(CAPTION_STYLE_ID));
    if (const Qt::Alignment own = ownAlignment(block.blockFormat()); own) {
        captionFormat.setAlignment(own);
    }
    cursor.insertBlock(captionFormat, QTextCharFormat());
    const CursorPosition after = absoluteToCursorPosition(document, cursor.position());
    document->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{after, {}}] { m_stepCursor = state; }));
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

std::optional<QString> BookEditor::imageCaption(int position) const
{
    if (!m_textBuffer || !editor::imageAt(*m_textBuffer, position)) {
        return std::nullopt;
    }
    const QTextBlock caption = captionBlockOf(m_textBuffer->findBlock(position));
    if (!caption.isValid()) {
        return std::nullopt;
    }
    return caption.text();
}

QString BookEditor::imageFilePath(int position) const
{
    const std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, position) : std::nullopt;
    return image ? m_imageStore->filePath(image->source) : QString();
}

}  // namespace kalahari::editor
