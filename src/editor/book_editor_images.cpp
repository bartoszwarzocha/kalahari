// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_editor_images.cpp
/// @brief BookEditor: images of the text, Insert > Image and the commands of an image

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/image_store.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/render_context.h>
#include <kalahari/editor/text_images.h>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QHash>
#include <QLocale>
#include <QMenu>
#include <QMetaMethod>
#include <QPainter>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

/// The side of a handle in a corner of the selected image, in the widget's pixels
constexpr qreal IMAGE_HANDLE_SIZE = 7.0;

/// How far from a handle a press still takes it, in the widget's pixels
constexpr qreal IMAGE_HANDLE_REACH = 3.0;

/// The narrowest an image is resized to, in the widget's pixels
constexpr qreal MIN_RESIZED_IMAGE = 8.0;

/// The space between the label of a resized image and its corner, and inside the label
constexpr qreal IMAGE_LABEL_GAP = 6.0;
constexpr qreal IMAGE_LABEL_PADDING = 4.0;

/// The dashes of the outline of a resize's preview
constexpr qreal IMAGE_PREVIEW_DASH = 3.0;

/// Narrower lines (in points) are those of a text not laid out yet: a copy is not made
/// smaller for them
constexpr double MIN_LINE_POINTS_FOR_COPY = 72.0;

/// Whether a corner is on the right of its image, and at its bottom
bool isRightCorner(int corner) { return corner == 1 || corner == 3; }
bool isBottomCorner(int corner) { return corner >= 2; }

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

void BookEditor::moveImagesFolder(const QString& folder)
{
    const QString before = m_imageStore->folder();
    setImagesFolder(folder);
    if (before.isEmpty() || m_imageStore->folder() == before || !m_textBuffer) {
        return;
    }
    // Each file copied once; an image whose copy has another name names it (a change of the
    // text, as the image's file is part of it)
    QHash<QString, QString> adopted;
    bool changed = false;
    QTextCursor cursor(m_textBuffer.get());
    for (const int position : imagePositions(*m_textBuffer, 0, m_textBuffer->characterCount())) {
        std::optional<ImageProperties> image = editor::imageAt(*m_textBuffer, position);
        if (!image) {
            continue;
        }
        auto known = adopted.constFind(image->source);
        if (known == adopted.cend()) {
            known = adopted.insert(image->source, m_imageStore->adoptSource(before, image->source));
        }
        if (*known == image->source) {
            continue;
        }
        image->source = *known;
        cursor.setPosition(position);
        cursor.setPosition(position + 1, QTextCursor::KeepAnchor);
        QTextCharFormat format = cursor.charFormat();
        setImage(format, *image);
        cursor.setCharFormat(format);
        changed = true;
    }
    if (changed) {
        if (auto* layout = qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())) {
            layout->refreshImages();
        }
        if (m_renderPipeline) {
            m_renderPipeline->markAllDirty();
        }
        update();
        emit contentChanged();
    }
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
    image.source = m_imageStore->addFile(path, newImageCopyWidth(path));
    return !image.source.isEmpty() && insertImage(image);
}

int BookEditor::newImageCopyWidth(const QString& path) const
{
    if (m_newImageOptions.maxDpi <= 0 || !m_textBuffer) {
        return 0;
    }
    const std::optional<QSize> pixels = ImageStore::fileSize(path);
    const auto* layout = qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout());
    if (!pixels || pixels->width() <= 0 || layout == nullptr || layout->pixelsPerPoint() <= 0.0) {
        return 0;
    }
    // The width the image gets in the lines at the cursor: the settings' share of them, else
    // its file's own, at most theirs
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    const double line = layout->imageLineWidth(block) / layout->pixelsPerPoint();
    if (!block.isValid() || line < MIN_LINE_POINTS_FOR_COPY) {
        return 0;  // not laid out yet: the copy is the file
    }
    const double natural = pixels->width() * POINTS_PER_INCH / IMAGE_PIXELS_PER_INCH;
    const double shown = m_newImageOptions.widthPercent > 0
                             ? line * std::min(m_newImageOptions.widthPercent, 100) / 100.0
                             : std::min(natural, line);
    const int wanted = static_cast<int>(std::ceil(shown * m_newImageOptions.maxDpi / POINTS_PER_INCH));
    return wanted > 0 && wanted < pixels->width() ? wanted : 0;
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

    // In a paragraph of its own, aligned as the settings say: never in the text of a
    // paragraph, nor between an image and its caption (at the start of a paragraph a new
    // image goes before it, else after it, after the caption of an image)
    if (const QTextBlock block = cursor.block(); block.length() > 1) {
        QTextBlock first = block;
        QTextBlock last = block;
        QTextBlock imageBlock = imageBlockOfCaption(block);
        if (!imageBlock.isValid() && hasImages(block)) {
            imageBlock = block;
        }
        if (const QTextBlock caption = captionBlockOf(imageBlock); caption.isValid()) {
            const bool above = caption.position() < imageBlock.position();
            first = above ? caption : imageBlock;
            last = above ? imageBlock : caption;
        }
        if (cursor.position() == first.position()) {
            cursor.insertBlock(first.blockFormat(), first.charFormat());
            cursor.movePosition(QTextCursor::PreviousBlock);
            cursor.setBlockFormat(paragraphBlockFormat());
            cursor.setBlockCharFormat(QTextCharFormat());
        } else {
            cursor.setPosition(last.position() + last.length() - 1);
            cursor.insertBlock(paragraphBlockFormat(), QTextCharFormat());
        }
    }
    QTextBlockFormat aligned;
    aligned.setAlignment(m_newImageOptions.alignment);
    cursor.mergeBlockFormat(aligned);

    // No formatting of its own: only the fragments of annotations and the link of its place;
    // the width of the settings unless it has one
    const QTextCharFormat place = insertionFormat(cursor);
    ImageProperties shown = image;
    if (m_newImageOptions.widthPercent > 0 && shown.unit == ImageWidthUnit::Natural) {
        shown.unit = ImageWidthUnit::Percent;
        shown.width = m_newImageOptions.widthPercent;
    }
    QTextCharFormat format = imageCharFormat(normalizedImage(shown));
    setAnnotations(format, replacing ? fragments : annotationsOf(place));
    setLinkAddress(format, linkAddressOf(place));
    const int position = cursor.position();
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), format);

    // A numbered caption under it, of the label of the settings
    if (const QString label = normalizedLabel(m_newImageOptions.captionLabel); !label.isEmpty()) {
        QTextCursor caption(document);
        const QTextBlock captionBlock = newCaptionBlock(caption, cursor.block(), false);
        writeCaption(caption, captionBlock, ImageCaption{true, true, label, QString(), false},
                     ImageCaption{});
    }
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
    image->height = 0.0;
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
    // The same picture (its own file, or a copy of it): nothing to change
    if (source == image->source) {
        return true;
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

std::optional<ImagePropertiesData> BookEditor::imagePropertiesData(int position) const
{
    const std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, position) : std::nullopt;
    if (!image) {
        return std::nullopt;
    }
    ImagePropertiesData propertiesData;
    propertiesData.position = position;
    propertiesData.image = *image;
    propertiesData.filePath = m_imageStore->filePath(image->source);
    if (const std::optional<QSize> pixels = m_imageStore->pixelSize(image->source)) {
        propertiesData.filePixels = *pixels;
        propertiesData.fileBytes = QFileInfo(propertiesData.filePath).size();
    }
    // The widths in points, as the layout measures them
    const auto* layout = qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout());
    if (layout != nullptr && layout->pixelsPerPoint() > 0.0) {
        const QTextBlock block = m_textBuffer->findBlock(position);
        const QMarginsF frame = layout->imageFrameMargins(*image);
        propertiesData.lineWidth = layout->imageLineWidth(block) / layout->pixelsPerPoint();
        const qreal box = layout->imageSize(block, *image).width();
        propertiesData.shownWidth = (box - frame.left() - frame.right()) / layout->pixelsPerPoint();
    }
    // Its caption, and the number it has or would get
    propertiesData.caption = imageCaptionOf(position).value_or(ImageCaption{});
    if (!propertiesData.caption.label.isEmpty()) {
        propertiesData.captionNumber = captionNumberAt(position, propertiesData.caption.label);
    }
    return propertiesData;
}

bool BookEditor::setImagePropertiesData(const ImagePropertiesData& propertiesData)
{
    const std::optional<ImageProperties> old =
        m_textBuffer ? editor::imageAt(*m_textBuffer, propertiesData.position) : std::nullopt;
    if (!old || refusesChange()) {
        return false;
    }
    ImageProperties image = propertiesData.image;
    if (!propertiesData.newFile.isEmpty()) {
        // Another picture: its file copied in first, its own size unless the size stays
        if (!m_imageStore->hasFolder() || !ImageStore::fileSize(propertiesData.newFile)) {
            return false;
        }
        const QString source = m_imageStore->addFile(propertiesData.newFile);
        if (source.isEmpty()) {
            return false;
        }
        image.source = source;
        if (!propertiesData.keepSizeAndCrop) {
            image.unit = ImageWidthUnit::Natural;
            image.width = 0.0;
            image.height = 0.0;
            image.crop = ImageCrop{};
        }
    }
    const int position = propertiesData.position;
    const std::optional<ImageCaption> caption = imageCaptionOf(position);
    ImageCaption wanted = propertiesData.caption;
    if (caption && !wanted.shown && !caption->shown) {
        wanted = *caption;  // none, and none wanted
    }
    const bool imageChanges = normalizedImage(image) != *old;
    const bool captionChanges = caption && wanted != *caption;
    if (!imageChanges && !captionChanges) {
        return true;  // nothing changed: no step of Undo, and nothing went wrong
    }

    // Its number goes, and cross-references name it: asked first; kept, the caption stays
    QTextDocument* document = m_textBuffer.get();
    bool applyCaption = captionChanges;
    if (captionChanges && caption->shown && caption->numbered && (!wanted.shown || !wanted.numbered)) {
        const QTextBlock block = captionBlockOf(document->findBlock(position));
        applyCaption = !block.isValid() || confirmCaptionRemoval(block);
    }
    if (!imageChanges && !applyCaption) {
        return true;
    }

    // One step of Undo: the image, then its caption (which may move the image's place)
    QTextCursor step(document);
    step.beginEditBlock();
    QTextCursor imagePlace(document);
    imagePlace.setPosition(position);
    bool done = !imageChanges || setImageProperties(position, image);
    if (done && applyCaption) {
        const QScopedValueRollback<bool> asked(m_captionRemovalAsked, true);
        done = setImageCaption(position, wanted);
    }
    step.endEditBlock();
    if (done) {
        selectImage(imagePlace.position());
    }
    return done;
}

bool BookEditor::setImageLayoutWidth(int position, qreal width)
{
    const std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, position) : std::nullopt;
    auto* layout = m_textBuffer
                       ? qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())
                       : nullptr;
    if (!image || layout == nullptr || !(width > 0.0)) {
        return false;
    }
    return setImageProperties(
        position, layout->imageOfWidth(m_textBuffer->findBlock(position), *image, width));
}

// =============================================================================
// The selected image: its handles, resizing, drawing and context menu
// =============================================================================

QRectF BookEditor::imageHandleRect(const QRectF& image, ImageCorner corner)
{
    const int index = static_cast<int>(corner);
    const QPointF center(isRightCorner(index) ? image.right() : image.left(),
                         isBottomCorner(index) ? image.bottom() : image.top());
    const qreal half = IMAGE_HANDLE_SIZE / 2.0;
    return QRectF(center - QPointF(half, half), QSizeF(IMAGE_HANDLE_SIZE, IMAGE_HANDLE_SIZE));
}

std::optional<BookEditor::ImageCorner> BookEditor::imageHandleAt(const QPointF& widgetPos) const
{
    const std::optional<int> position = selectedImage();
    if (!position || m_readOnly || imageCropping() != nullptr) {
        return std::nullopt;
    }
    const std::optional<QRectF> rect = imageRect(*position);
    if (!rect) {
        return std::nullopt;
    }
    for (const ImageCorner corner : {ImageCorner::TopLeft, ImageCorner::TopRight,
                                     ImageCorner::BottomLeft, ImageCorner::BottomRight}) {
        const QRectF handle = imageHandleRect(*rect, corner).adjusted(
            -IMAGE_HANDLE_REACH, -IMAGE_HANDLE_REACH, IMAGE_HANDLE_REACH, IMAGE_HANDLE_REACH);
        if (handle.contains(widgetPos)) {
            return corner;
        }
    }
    return std::nullopt;
}

bool BookEditor::pressImage(const QPointF& widgetPos)
{
    const std::optional<ImageCorner> corner = imageHandleAt(widgetPos);
    if (const std::optional<int> selected = selectedImage(); corner && selected) {
        const std::optional<QRectF> rect = imageRect(*selected);
        if (rect && !rect->isEmpty()) {
            m_imageResize = ImageResize{*selected, *corner, *rect, widgetPos, *rect};
            update();
            return true;
        }
    }
    const std::optional<int> position = imagePositionAt(widgetPos);
    if (!position) {
        return false;
    }
    // Selected; a drag moves it as selected text, let go it stays selected
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);
    selectImage(*position);
    m_imagePressed = true;
    m_textDragPending = true;
    m_textDragStartPos = widgetPos;
    return true;
}

void BookEditor::dragImageHandle(const QPointF& widgetPos)
{
    if (!m_imageResize || !m_textBuffer || !m_renderPipeline) {
        return;
    }
    ImageResize& resize = *m_imageResize;
    auto* layout = qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout());
    const QTextBlock block = m_textBuffer->findBlock(resize.position);
    if (layout == nullptr || !block.isValid()) {
        return;
    }
    const int corner = static_cast<int>(resize.corner);
    const QRectF& start = resize.start;
    const qreal ratio = start.height() / start.width();

    // In proportion: the width of the handle's move across, or of its move down, whichever
    // makes the image larger; never wider than its line
    const qreal across = (widgetPos.x() - resize.pressed.x()) * (isRightCorner(corner) ? 1.0 : -1.0);
    const qreal down = (widgetPos.y() - resize.pressed.y()) * (isBottomCorner(corner) ? 1.0 : -1.0);
    const qreal widest = layout->widestImage(block) * m_renderPipeline->context().computed.viewScale;
    const qreal width = std::clamp(std::max(start.width() + across, (start.height() + down) / ratio),
                                   std::min(MIN_RESIZED_IMAGE, widest), std::max(widest, 1.0));
    const qreal height = width * ratio;

    // From the corner across from the handle
    const qreal left = isRightCorner(corner) ? start.left() : start.right() - width;
    const qreal top = isBottomCorner(corner) ? start.top() : start.bottom() - height;
    resize.preview = QRectF(left, top, width, height);
    update();
}

void BookEditor::finishImageResize()
{
    if (!m_imageResize) {
        return;
    }
    const ImageResize resize = *m_imageResize;
    m_imageResize.reset();
    const qreal scale = m_renderPipeline ? m_renderPipeline->context().computed.viewScale : 1.0;
    if (resize.preview.size() != resize.start.size() && scale > 0.0) {
        setImageLayoutWidth(resize.position, resize.preview.width() / scale);
    }
    update();
}

QString BookEditor::imageWidthLabel(int position, qreal widgetWidth) const
{
    const std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, position) : std::nullopt;
    auto* layout = m_textBuffer
                       ? qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())
                       : nullptr;
    const qreal scale = m_renderPipeline ? m_renderPipeline->context().computed.viewScale : 0.0;
    if (!image || layout == nullptr || !(scale > 0.0)) {
        return QString();
    }
    const ImageProperties resized =
        layout->imageOfWidth(m_textBuffer->findBlock(position), *image, widgetWidth / scale);
    if (resized.unit == ImageWidthUnit::Centimeters) {
        //: The width of an image being resized by a handle. In Polish: %1 cm
        return tr("%1 cm").arg(QLocale().toString(resized.width, 'f', 2));
    }
    //: The width of an image being resized by a handle, a whole percent of the lines of its
    //: paragraph. In Polish: %1% szerokości tekstu
    return tr("%1% of the text width").arg(QLocale().toString(qRound(resized.width)));
}

void BookEditor::paintImageSelection(QPainter& painter) const
{
    if (!m_textBuffer || !m_renderPipeline || !hasSelection()) {
        return;
    }
    QColor line = m_appearance.colors.selection;
    line.setAlpha(255);
    painter.save();

    // Among selected text the images take the selection's color, as the letters' background
    const std::optional<int> selected = selectedImage();
    if (!selected) {
        const SelectionRange sel = m_selection.normalized();
        const auto& computed = m_renderPipeline->context().computed;
        const QTextBlock first =
            m_textBuffer->findBlockByNumber(static_cast<int>(computed.firstVisibleParagraph));
        const QTextBlock last =
            m_textBuffer->findBlockByNumber(static_cast<int>(computed.lastVisibleParagraph));
        const int from = std::max(calculateAbsolutePosition(sel.start),
                                  first.isValid() ? first.position() : 0);
        const int to = std::min(calculateAbsolutePosition(sel.end),
                                last.isValid() ? last.position() + last.length()
                                               : m_textBuffer->characterCount());
        for (const int position : imagePositions(*m_textBuffer, from, to)) {
            if (const std::optional<QRectF> rect = imageRect(position)) {
                painter.fillRect(*rect, m_appearance.colors.selection);
            }
        }
        painter.restore();
        return;
    }

    // Being cropped: its marks instead of the frame
    if (const ImageCropping* cropping = imageCropping(); cropping != nullptr &&
                                                         cropping->position == *selected) {
        painter.restore();
        paintImageCropping(painter);
        return;
    }

    // The image alone: a frame in the selection's color, a handle in each corner where it can
    // be resized
    const std::optional<QRectF> rect = imageRect(*selected);
    if (!rect) {
        painter.restore();
        return;
    }
    const QColor paper = m_appearance.colors.background(m_appearance.colorMode);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(line, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect->adjusted(-0.5, -0.5, -0.5, -0.5));
    if (!m_readOnly) {
        painter.setBrush(paper);
        for (const ImageCorner corner : {ImageCorner::TopLeft, ImageCorner::TopRight,
                                         ImageCorner::BottomLeft, ImageCorner::BottomRight}) {
            painter.drawRect(imageHandleRect(*rect, corner).adjusted(0.0, 0.0, -1.0, -1.0));
        }
    }

    // Being resized: the outline of its new size and a label with its width, beside the handle
    if (m_imageResize && m_imageResize->position == *selected) {
        const QRectF& preview = m_imageResize->preview;
        QPen dashed(line, 1.0, Qt::CustomDashLine);
        dashed.setDashPattern({IMAGE_PREVIEW_DASH, IMAGE_PREVIEW_DASH});
        painter.setPen(dashed);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(preview.adjusted(-0.5, -0.5, -0.5, -0.5));

        const QString label = imageWidthLabel(*selected, preview.width());
        if (!label.isEmpty()) {
            const QSizeF size = imageLabelSize(label);
            const int corner = static_cast<int>(m_imageResize->corner);
            const qreal gap = IMAGE_LABEL_GAP;
            const qreal x = isRightCorner(corner) ? preview.right() + gap
                                                  : preview.left() - gap - size.width();
            const qreal y = isBottomCorner(corner) ? preview.bottom() + gap
                                                   : preview.top() - gap - size.height();
            paintImageLabel(painter, label, QPointF(x, y));
        }
    }

    // Resized by Ctrl+] or Ctrl+[: its new width beside its bottom right corner for a moment
    if (!m_imageResize && m_keyResizedImage == *selected) {
        const QString label = imageWidthLabel(*selected, rect->width());
        if (!label.isEmpty()) {
            paintImageLabel(painter, label,
                            rect->bottomRight() + QPointF(IMAGE_LABEL_GAP, IMAGE_LABEL_GAP));
        }
    }
    painter.restore();
}

QSizeF BookEditor::imageLabelSize(const QString& label) const
{
    const QFontMetricsF metrics(font());
    return QSizeF(metrics.horizontalAdvance(label) + 2.0 * IMAGE_LABEL_PADDING,
                  metrics.height() + IMAGE_LABEL_PADDING);
}

void BookEditor::paintImageLabel(QPainter& painter, const QString& label, const QPointF& at) const
{
    const QSizeF size = imageLabelSize(label);
    // Inside the view, off its scroll bars, where the image is near its edge
    const bool rightBar = m_verticalScrollBar != nullptr && !m_verticalScrollBar->isHidden();
    const bool bottomBar = m_horizontalScrollBar != nullptr && !m_horizontalScrollBar->isHidden();
    const qreal right = width() - (rightBar ? m_verticalScrollBar->width() : 0);
    const qreal bottom = height() - (bottomBar ? m_horizontalScrollBar->height() : 0);
    const QRectF box(QPointF(std::clamp(at.x(), 0.0, std::max(0.0, right - size.width())),
                             std::clamp(at.y(), 0.0, std::max(0.0, bottom - size.height()))),
                     size);
    QColor line = m_appearance.colors.selection;
    line.setAlpha(255);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(line, 1.0));
    painter.setBrush(m_appearance.colors.background(m_appearance.colorMode));
    painter.drawRect(box.adjusted(0.5, 0.5, -0.5, -0.5));
    painter.setPen(m_appearance.colors.textColor(m_appearance.colorMode));
    painter.setFont(font());
    painter.drawText(box, Qt::AlignCenter, label);
    painter.restore();
}

void BookEditor::addImageActions(QMenu& menu, int position)
{
    const std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, position) : std::nullopt;
    if (!image) {
        return;
    }
    // The commands of the Image menu, as there, where the application gives them
    const ContextMenuImageActions& commands = m_contextMenuImageActions;
    if (std::any_of(commands.top.cbegin(), commands.top.cend(),
                    [](const QAction* action) { return action != nullptr; })) {
        const auto addActions = [](QMenu& to, const QList<QAction*>& actions) {
            for (QAction* action : actions) {
                if (action != nullptr) {
                    to.addAction(action);
                } else {
                    to.addSeparator();
                }
            }
        };
        const auto addSubmenu = [&menu, &addActions](const QString& title,
                                                     const QList<QAction*>& actions) {
            if (actions.isEmpty()) {
                return;
            }
            QMenu* submenu = menu.addMenu(title);
            submenu->setToolTipsVisible(true);
            addActions(*submenu, actions);
        };
        addActions(menu, commands.top);
        menu.addSeparator();
        //: The context menu of an image, as the Image menu's submenus. In Polish: Ob&róć
        addSubmenu(tr("R&otate"), commands.rotate);
        //: In Polish: &Filtr
        addSubmenu(tr("&Filter"), commands.filter);
        addActions(menu, commands.bottom);
        return;
    }

    // The windows and file pickers are the application's: their items are there when it
    // gives them
    if (isSignalConnected(QMetaMethod::fromSignal(&BookEditor::imagePropertiesAsked))) {
        //: The context menu of an image. In Polish: Właściwości &obrazu...
        disableWhileReadOnly(menu.addAction(tr("Image Pr&operties..."), this, [this, position]() {
            emit imagePropertiesAsked(position, 0);
        }));
    }
    // The window of the caption where the application gives it, else a caption at once
    if (isSignalConnected(QMetaMethod::fromSignal(&BookEditor::imageCaptionAsked))) {
        //: In Polish: Wstaw po&dpis...
        disableWhileReadOnly(menu.addAction(tr("Insert Capt&ion..."), this, [this, position]() {
            emit imageCaptionAsked(position);
        }));
    } else {
        //: In Polish: &Dodaj podpis
        disableWhileReadOnly(menu.addAction(tr("A&dd Caption"), this,
                                            [this, position]() { addImageCaption(position); }));
    }
    if (isSignalConnected(QMetaMethod::fromSignal(&BookEditor::imageFileAsked))) {
        //: In Polish: Z&mień obraz...
        QAction* change = menu.addAction(tr("C&hange Image..."), this,
                                         [this, position]() { emit imageFileAsked(position); });
        disableWhileReadOnly(change);
        change->setEnabled(change->isEnabled() && m_imageStore->hasFolder());
    }
    //: In Polish: Ro&zmiar oryginalny
    QAction* original = menu.addAction(tr("Original Si&ze"), this,
                                       [this, position]() { setImageOriginalSize(position); });
    disableWhileReadOnly(original);
    original->setEnabled(original->isEnabled() && image->unit != ImageWidthUnit::Natural);
    if (isSignalConnected(QMetaMethod::fromSignal(&BookEditor::imageSaveAsked))) {
        //: In Polish: Zapisz &jako obraz...
        QAction* save = menu.addAction(tr("Sa&ve as Image..."), this,
                                       [this, position]() { emit imageSaveAsked(position); });
        save->setEnabled(QFileInfo(imageFilePath(position)).isFile());
    }
}

}  // namespace kalahari::editor
