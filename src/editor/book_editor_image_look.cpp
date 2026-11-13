/// @file book_editor_image_look.cpp
/// @brief BookEditor: the commands of the Image menu that change the look of an image (turn,
///        mirror, filter, the keys of its size) and cropping it in the text

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/image_store.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/text_images.h>
#include <QKeyEvent>
#include <QPainter>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

constexpr double POINTS_PER_CENTIMETER = 72.0 / 2.54;

/// How long the label of an image resized by Ctrl+] or Ctrl+[ stays, in milliseconds
constexpr int KEY_RESIZE_LABEL_MS = 1500;

/// The sides of an image being cropped, as bits
constexpr int CROP_LEFT = 1;
constexpr int CROP_TOP = 2;
constexpr int CROP_RIGHT = 4;
constexpr int CROP_BOTTOM = 8;

/// The marks of the crop: their length and thickness, and how far from one a press takes it,
/// in the widget's pixels
constexpr qreal CROP_MARK_LENGTH = 16.0;
constexpr qreal CROP_MARK_WIDTH = 3.0;
constexpr qreal CROP_MARK_REACH = 6.0;

/// How strongly the cut parts of a picture being cropped show
constexpr qreal CROP_CUT_OPACITY = 0.35;

/// The space between an image being cropped and the label under it
constexpr qreal CROP_LABEL_GAP = 12.0;

}  // namespace

// =============================================================================
// Turn, mirror, filter, size
// =============================================================================

bool BookEditor::rotateImage(int position, int quarters)
{
    const std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, position) : std::nullopt;
    auto* layout = m_textBuffer
                       ? qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())
                       : nullptr;
    if (!image || layout == nullptr) {
        return false;
    }
    ImageProperties turned = *image;
    if (image->height > 0.0 && quarters % 2 != 0 && layout->pixelsPerPoint() > 0.0) {
        // An image of its own shape turns it: as wide as it was high, as high as it was wide,
        // its width in its own unit
        const QTextBlock block = m_textBuffer->findBlock(position);
        const QMarginsF frame = layout->imageFrameMargins(*image);
        const QSizeF box = layout->imageSize(block, *image);
        const QSizeF picture(box.width() - frame.left() - frame.right(),
                             box.height() - frame.top() - frame.bottom());
        turned = layout->imageOfWidth(block, *image, picture.height() + frame.left() + frame.right());
        turned.height = picture.width() / layout->pixelsPerPoint() / POINTS_PER_CENTIMETER;
    }
    turned.rotation = ((image->rotation + quarters) % 4 + 4) % 4;
    return setImageProperties(position, turned);
}

bool BookEditor::flipImage(int position, Qt::Orientation orientation)
{
    std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, position) : std::nullopt;
    if (!image) {
        return false;
    }
    // As it is shown: across a turned picture is down its file
    const bool acrossFile = (orientation == Qt::Horizontal) == (image->rotation % 2 == 0);
    if (acrossFile) {
        image->flipHorizontal = !image->flipHorizontal;
    } else {
        image->flipVertical = !image->flipVertical;
    }
    return setImageProperties(position, *image);
}

bool BookEditor::setImageFilter(int position, ImageFilter filter)
{
    std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, position) : std::nullopt;
    if (!image) {
        return false;
    }
    image->filter = filter;
    return setImageProperties(position, *image);
}

int BookEditor::setAllImagesFilter(ImageFilter filter, int threshold)
{
    if (!m_textBuffer) {
        return 0;
    }
    QTextDocument* document = m_textBuffer.get();
    const std::vector<int> images = imagesWithOtherFilter(*document, filter, threshold);
    if (images.empty() || refusesChange()) {
        return 0;
    }

    // One undo step; the cursor stays where it is
    const StepCursor state{m_cursorPosition, m_selection};
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    document->appendUndoItem(new CallbackUndoItem([this, state] { m_stepCursor = state; }));
    for (const int position : images) {
        cursor.setPosition(position);
        cursor.setPosition(position + 1, QTextCursor::KeepAnchor);
        QTextCharFormat format = cursor.charFormat();
        if (const std::optional<ImageProperties> image = imageOf(format)) {
            setImage(format, normalizedImage(withImageFilter(*image, filter, threshold)));
            cursor.setCharFormat(format);
        }
    }
    document->appendUndoItem(new CallbackUndoItem([this, state] { m_stepCursor = state; }));
    cursor.endEditBlock();
    m_learnableChange.reset();

    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    update();
    emit contentChanged();
    return static_cast<int>(images.size());
}

bool BookEditor::resizeImageByStep(int position, bool larger)
{
    const std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, position) : std::nullopt;
    auto* layout = m_textBuffer
                       ? qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())
                       : nullptr;
    if (!image || layout == nullptr) {
        return false;
    }
    const QTextBlock block = m_textBuffer->findBlock(position);
    const QMarginsF frame = layout->imageFrameMargins(*image);
    const qreal sides = frame.left() + frame.right();
    const qreal picture = layout->imageSize(block, *image).width() - sides;
    const qreal width = picture * (larger ? 1.0 + IMAGE_KEY_RESIZE : 1.0 - IMAGE_KEY_RESIZE);
    // Never wider than its line: at the line's width larger does nothing
    const qreal widest = layout->widestImage(block) - sides;
    const qreal wanted = std::min(width, widest);
    if (!(wanted > 0.0) || std::abs(wanted - picture) < 0.5) {
        return false;
    }
    if (!setImageProperties(position, layout->imageOfWidth(block, *image, wanted + sides))) {
        return false;
    }
    // Its new width for a moment
    if (m_keyResizeLabelTimer == nullptr) {
        m_keyResizeLabelTimer = new QTimer(this);
        m_keyResizeLabelTimer->setSingleShot(true);
        connect(m_keyResizeLabelTimer, &QTimer::timeout, this, [this]() {
            m_keyResizedImage = -1;
            update();
        });
    }
    m_keyResizedImage = position;
    m_keyResizeLabelTimer->start(KEY_RESIZE_LABEL_MS);
    update();
    return true;
}

std::optional<bool> BookEditor::imageResizeKey(const QKeyEvent& event)
{
    // Shift too: on some keyboards the brackets need it
    if ((event.modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier)) != Qt::ControlModifier) {
        return std::nullopt;
    }
    if (event.key() == Qt::Key_BracketRight) {
        return true;
    }
    if (event.key() == Qt::Key_BracketLeft) {
        return false;
    }
    return std::nullopt;
}

bool BookEditor::leaveSelectedImage()
{
    const std::optional<int> image = selectedImage();
    if (!image || !m_textBuffer) {
        return false;
    }
    const CursorPosition after = absoluteToCursorPosition(m_textBuffer.get(), *image + 1);
    clearSelection();
    m_selectionAnchor = after;
    setCursorPosition(after);
    update();
    return true;
}

// =============================================================================
// Cropping in the text
// =============================================================================

bool BookEditor::canCropImage(int position) const
{
    const std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, position) : std::nullopt;
    return image && !m_readOnly && m_imageStore->pixelSize(image->source).has_value();
}

bool BookEditor::startImageCrop(int position)
{
    const std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, position) : std::nullopt;
    if (!image || !canCropImage(position)) {
        return false;
    }
    m_imageResize.reset();
    selectImage(position);
    m_imageCropping = ImageCropping{position, *image, shownCrop(*image), 0, QPointF(), ImageCrop{}};
    update();
    return true;
}

std::optional<ImageCrop> BookEditor::imageCropInProgress() const
{
    const ImageCropping* cropping = imageCropping();
    return cropping != nullptr ? std::optional<ImageCrop>(cropping->crop) : std::nullopt;
}

void BookEditor::finishImageCrop(bool crop)
{
    const ImageCropping* cropping = imageCropping();
    if (cropping == nullptr) {
        return;
    }
    const ImageCropping done = *cropping;
    m_imageCropping.reset();
    // The rest of the picture as large as it was: the image gets smaller
    if (crop && done.crop != shownCrop(done.image)) {
        setImageProperties(done.position, withShownCrop(done.image, done.crop, true));
    }
    update();
}

const BookEditor::ImageCropping* BookEditor::imageCropping() const
{
    if (!m_imageCropping) {
        return nullptr;
    }
    // The image changed or gone (an undo, an edit elsewhere): nothing to crop
    const std::optional<ImageProperties> image =
        m_textBuffer ? editor::imageAt(*m_textBuffer, m_imageCropping->position) : std::nullopt;
    if (!image || *image != m_imageCropping->image) {
        m_imageCropping.reset();
        return nullptr;
    }
    return &*m_imageCropping;
}

std::optional<std::pair<QRectF, QRectF>> BookEditor::imageCropRects() const
{
    const ImageCropping* cropping = imageCropping();
    auto* layout = m_textBuffer
                       ? qobject_cast<KalahariTextDocumentLayout*>(m_textBuffer->documentLayout())
                       : nullptr;
    if (cropping == nullptr || layout == nullptr || !m_renderPipeline) {
        return std::nullopt;
    }
    const std::optional<QRectF> box = imageRect(cropping->position);
    if (!box) {
        return std::nullopt;
    }
    // The picture inside its frame, as the layout shows it now (cropped as it was)
    const qreal scale = m_renderPipeline->context().computed.viewScale;
    const QMarginsF frame = layout->imageFrameMargins(cropping->image) * scale;
    const QRectF picture = box->marginsRemoved(frame);
    const ImageCrop before = shownCrop(cropping->image);
    const qreal wholeWidth = picture.width() * 100.0 / (100.0 - before.left - before.right);
    const qreal wholeHeight = picture.height() * 100.0 / (100.0 - before.top - before.bottom);
    const QRectF whole(picture.left() - wholeWidth * before.left / 100.0,
                       picture.top() - wholeHeight * before.top / 100.0, wholeWidth, wholeHeight);
    const ImageCrop& crop = cropping->crop;
    const QRectF kept(whole.left() + wholeWidth * crop.left / 100.0,
                      whole.top() + wholeHeight * crop.top / 100.0,
                      wholeWidth * (100.0 - crop.left - crop.right) / 100.0,
                      wholeHeight * (100.0 - crop.top - crop.bottom) / 100.0);
    return std::make_pair(whole, kept);
}

int BookEditor::imageCropSidesAt(const QPointF& widgetPos) const
{
    const std::optional<std::pair<QRectF, QRectF>> rects = imageCropRects();
    if (!rects) {
        return 0;
    }
    const QRectF& kept = rects->second;
    const QRectF reach = kept.adjusted(-CROP_MARK_REACH - CROP_MARK_WIDTH, -CROP_MARK_REACH - CROP_MARK_WIDTH,
                                       CROP_MARK_REACH + CROP_MARK_WIDTH, CROP_MARK_REACH + CROP_MARK_WIDTH);
    if (!reach.contains(widgetPos)) {
        return 0;
    }
    // The nearer of two sides where the part kept is narrow
    const qreal left = std::abs(widgetPos.x() - kept.left());
    const qreal right = std::abs(widgetPos.x() - kept.right());
    const qreal top = std::abs(widgetPos.y() - kept.top());
    const qreal bottom = std::abs(widgetPos.y() - kept.bottom());
    const qreal near = CROP_MARK_REACH + CROP_MARK_WIDTH;
    int sides = 0;
    if (std::min(left, right) <= near) {
        sides |= left <= right ? CROP_LEFT : CROP_RIGHT;
    }
    if (std::min(top, bottom) <= near) {
        sides |= top <= bottom ? CROP_TOP : CROP_BOTTOM;
    }
    return sides;
}

std::optional<Qt::CursorShape> BookEditor::imageCropPointerAt(const QPointF& widgetPos) const
{
    const int sides = m_imageCropping && m_imageCropping->sides != 0 ? m_imageCropping->sides
                                                                       : imageCropSidesAt(widgetPos);
    const bool across = (sides & (CROP_LEFT | CROP_RIGHT)) != 0;
    const bool down = (sides & (CROP_TOP | CROP_BOTTOM)) != 0;
    if (across && down) {
        // A corner: along its diagonal
        const bool falling = ((sides & CROP_LEFT) != 0) == ((sides & CROP_TOP) != 0);
        return falling ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor;
    }
    if (across) {
        return Qt::SizeHorCursor;
    }
    if (down) {
        return Qt::SizeVerCursor;
    }
    const std::optional<std::pair<QRectF, QRectF>> rects = imageCropRects();
    if (rects && rects->first.contains(widgetPos)) {
        return Qt::ArrowCursor;
    }
    return std::nullopt;
}

bool BookEditor::pressImageCrop(const QPointF& widgetPos)
{
    if (imageCropping() == nullptr || !m_imageCropping) {
        return false;
    }
    if (const int sides = imageCropSidesAt(widgetPos); sides != 0) {
        m_imageCropping->sides = sides;
        m_imageCropping->pressed = widgetPos;
        m_imageCropping->pressedCrop = m_imageCropping->crop;
        return true;
    }
    // On the picture, its cut parts too: it is still being cropped
    const std::optional<std::pair<QRectF, QRectF>> rects = imageCropRects();
    if (rects && rects->first.contains(widgetPos)) {
        return true;
    }
    // Elsewhere: done, and the press goes on to the text
    finishImageCrop(true);
    return false;
}

void BookEditor::dragImageCrop(const QPointF& widgetPos)
{
    const std::optional<std::pair<QRectF, QRectF>> rects = imageCropRects();
    if (!rects || !m_imageCropping || m_imageCropping->sides == 0 || rects->first.isEmpty()) {
        return;
    }
    ImageCropping& cropping = *m_imageCropping;
    const QRectF& whole = rects->first;
    const double across = (widgetPos.x() - cropping.pressed.x()) / whole.width() * 100.0;
    const double down = (widgetPos.y() - cropping.pressed.y()) / whole.height() * 100.0;
    // Each side within the picture, two across from each other leaving some of it
    ImageCrop crop = cropping.pressedCrop;
    const auto side = [](double value, double other) {
        return std::clamp(value, 0.0, std::max(0.0, MAX_IMAGE_CROP - other));
    };
    if ((cropping.sides & CROP_LEFT) != 0) {
        crop.left = side(cropping.pressedCrop.left + across, crop.right);
    }
    if ((cropping.sides & CROP_RIGHT) != 0) {
        crop.right = side(cropping.pressedCrop.right - across, crop.left);
    }
    if ((cropping.sides & CROP_TOP) != 0) {
        crop.top = side(cropping.pressedCrop.top + down, crop.bottom);
    }
    if ((cropping.sides & CROP_BOTTOM) != 0) {
        crop.bottom = side(cropping.pressedCrop.bottom - down, crop.top);
    }
    cropping.crop = crop;
    update();
}

void BookEditor::paintImageCropping(QPainter& painter) const
{
    const std::optional<std::pair<QRectF, QRectF>> rects = imageCropRects();
    if (!rects || !m_imageCropping) {
        return;
    }
    const ImageCropping& cropping = *m_imageCropping;
    const auto& [whole, kept] = *rects;
    const QColor paper = m_appearance.colors.background(m_appearance.colorMode);
    painter.save();

    // The image as it is, with its frame, gives way to the whole picture: the cut parts
    // faint, the part kept as it is shown
    if (const std::optional<QRectF> box = imageRect(cropping.position)) {
        painter.fillRect(*box, paper);
    }
    ImageProperties uncropped = cropping.image;
    uncropped.crop = ImageCrop{};
    const QSize pixels = (whole.size() * devicePixelRatioF()).toSize().expandedTo(QSize(1, 1));
    const QImage picture = m_imageStore->shownImage(uncropped, pixels);
    if (!picture.isNull() && !whole.isEmpty()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setOpacity(CROP_CUT_OPACITY);
        painter.drawImage(whole, picture);
        painter.setOpacity(1.0);
        const qreal byX = picture.width() / whole.width();
        const qreal byY = picture.height() / whole.height();
        painter.drawImage(kept, picture,
                          QRectF((kept.left() - whole.left()) * byX, (kept.top() - whole.top()) * byY,
                                 kept.width() * byX, kept.height() * byY));
    }

    // The part kept outlined on the paper's color, the marks just outside its corners and
    // the middles of its sides
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(paper, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(kept.adjusted(-0.5, -0.5, 0.5, 0.5));
    const QColor marks = m_appearance.colors.imageCropMarksColor(m_appearance.colorMode);
    const qreal length = CROP_MARK_LENGTH;
    const qreal thick = CROP_MARK_WIDTH;
    const qreal left = kept.left();
    const qreal right = kept.right();
    const qreal top = kept.top();
    const qreal bottom = kept.bottom();
    const QPointF middle = kept.center();
    const QRectF parts[] = {
        // The corners: an angle each
        {left - thick, top - thick, length, thick},
        {left - thick, top - thick, thick, length},
        {right - length + thick, top - thick, length, thick},
        {right, top - thick, thick, length},
        {left - thick, bottom, length, thick},
        {left - thick, bottom - length + thick, thick, length},
        {right - length + thick, bottom, length, thick},
        {right, bottom - length + thick, thick, length},
        // The sides: a bar each
        {middle.x() - length / 2.0, top - thick, length, thick},
        {middle.x() - length / 2.0, bottom, length, thick},
        {left - thick, middle.y() - length / 2.0, thick, length},
        {right, middle.y() - length / 2.0, thick, length},
    };
    for (const QRectF& part : parts) {
        painter.fillRect(part, marks);
    }
    painter.restore();

    //: What the keys and the mouse do while an image is cropped in the text. In Polish:
    //: Przytnij: przeciągnij znaczniki · Enter – gotowe · Esc – anuluj
    paintImageLabel(painter, tr("Crop: drag the marks · Enter – done · Esc – cancel"),
                    QPointF(std::min(whole.left(), kept.left()),
                            std::max(whole.bottom(), kept.bottom()) + CROP_LABEL_GAP));
}

}  // namespace kalahari::editor
