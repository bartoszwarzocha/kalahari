// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file image_properties_dialog.cpp
/// @brief Image > Image Properties...: the file, size, crop, border, colors and description
///        of the selected image

#include "kalahari/gui/dialogs/image_properties_dialog.h"

#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/image_store.h"
#include "kalahari/gui/dialogs/color_palettes.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/utils/access_keys.h"
#include "kalahari/gui/widgets/color_menu.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QShortcut>
#include <QSlider>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

using editor::ImageBorderLine;
using editor::ImageCrop;
using editor::ImageFilter;
using editor::ImageProperties;
using editor::ImageWidthUnit;

constexpr double POINTS_PER_CENTIMETER = 72.0 / 2.54;

/// The points of a pixel of a file (96 an inch)
constexpr double POINTS_PER_PIXEL = 72.0 / editor::IMAGE_PIXELS_PER_INCH;

/// How far a shadow falls and how opaque it is, as the editor draws it
constexpr double SHADOW_POINTS = 3.0;
constexpr float SHADOW_OPACITY = 0.35F;

/// The longest side of the picture the window keeps for its previews
constexpr int PREVIEW_PICTURE_PIXELS = 480;

/// The sizes of the previews: the large one of the pages Image, Crop and Border, the one
/// of Colors and the tiles of the filters
const QSize LARGE_PREVIEW(220, 150);
const QSize COLORS_PREVIEW(170, 110);
const QSize CAPTION_PREVIEW(150, 80);
const QSize FILTER_TILE(72, 44);

/// The width of the list of the pages
constexpr int PAGE_LIST_WIDTH = 160;

/// The room an item of the list of the pages has above and below its name, together
constexpr int PAGE_ITEM_ROOM = 8;

/// The width of the fields of numbers
constexpr int NUMBER_FIELD_WIDTH = 90;

/// The widths of a border's line (points)
constexpr double LINE_WIDTHS[] = {0.25, 0.5, 0.75, 1.0, 1.5, 2.25, 3.0, 4.5, 6.0};

/// The width units of the Size page
enum WidthUnit { WidthOfText, WidthOfOriginal, WidthLength };
enum HeightUnit { HeightOfOriginal, HeightLength };

/// The ratios of the Crop page: free, the original, then width to height
constexpr double ASPECTS[] = {0.0, 0.0, 1.0, 4.0 / 3.0, 3.0 / 2.0, 16.0 / 9.0};

/// A whole percent with its sign: "+10%", "0%", "-5%"
QString signedPercent(int value)
{
    const QString number = QLocale().toString(value);
    return (value > 0 ? QStringLiteral("+") : QString()) + number + QStringLiteral("%");
}

/// A swatch for a color button
QIcon swatchIcon(const QColor& color, const QColor& border, int side)
{
    QPixmap pixmap(side, side);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    if (color.isValid()) {
        painter.fillRect(pixmap.rect().adjusted(1, 1, -1, -1), color);
    }
    painter.setPen(border);
    painter.drawRect(pixmap.rect().adjusted(1, 1, -2, -2));
    return QIcon(pixmap);
}

/// A note under the fields, in the color of a placeholder, in a few lines
QLabel* hintLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

/// A width of a line, in the way of the language: "0,5 pt"
QString lineWidthText(double points)
{
    //: The width of the line of an image's border, in points: "0.5 pt". In Polish: %1 pt
    return ImagePropertiesDialog::tr("%1 pt").arg(QLocale().toString(points, 'g', 3));
}

/// The picture of a file, made small for the previews; null when it is not one
QImage previewPicture(const QString& path)
{
    if (path.isEmpty()) {
        return QImage();
    }
    const std::optional<QSize> size = editor::ImageStore::fileSize(path);
    if (!size) {
        return QImage();
    }
    const QSize small = size->scaled(QSize(PREVIEW_PICTURE_PIXELS, PREVIEW_PICTURE_PIXELS),
                                     Qt::KeepAspectRatio)
                            .boundedTo(*size)
                            .expandedTo(QSize(1, 1));
    // The store reads a file as the editor does (an SVG drawn, a photo turned as it was taken)
    editor::ImageStore store;
    const QFileInfo file(path);
    store.setFolder(file.absolutePath());
    return store.picture(file.fileName(), small);
}

/// The frame of an image around its picture, in points: its line and the room inside it
double frameSide(const ImageProperties& image)
{
    const double line = image.borderLine == ImageBorderLine::None ? 0.0
                        : image.borderLine == ImageBorderLine::Double
                            ? 3.0 * image.borderWidth
                            : image.borderWidth;
    return image.hasFrame() ? line + image.borderPadding : 0.0;
}

/// The names in the list of the pages with their access keys, underlined as a label's
class PageNameDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    /// A little more room than a line of a list: the pages are few
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const override
    {
        return QStyledItemDelegate::sizeHint(option, index) + QSize(0, PAGE_ITEM_ROOM);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem item = option;
        initStyleOption(&item, index);
        item.text.clear();
        const QWidget* widget = option.widget;
        QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);
        // The text where the item has it, with the room the style leaves around it
        const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;
        const QRect text = style->subElementRect(QStyle::SE_ItemViewItemText, &item, widget)
                               .adjusted(margin, 0, -margin, 0);
        const int flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine |
                          (style->styleHint(QStyle::SH_UnderlineShortcut, &item, widget) != 0
                               ? Qt::TextShowMnemonic
                               : Qt::TextHideMnemonic);
        style->drawItemText(painter, text, flags, item.palette,
                            (item.state & QStyle::State_Enabled) != 0,
                            index.data(Qt::UserRole).toString(),
                            (item.state & QStyle::State_Selected) != 0 ? QPalette::HighlightedText
                                                                       : QPalette::Text);
    }
};

}  // namespace

// =============================================================================
// ImagePreview
// =============================================================================

ImagePreview::ImagePreview(const QSize& size, QWidget* parent)
    : QFrame(parent)
    , m_size(size)
{
    setFrameShape(QFrame::StyledPanel);
    setFixedSize(size);
    setFocusPolicy(Qt::NoFocus);
}

void ImagePreview::setPicture(const QImage& picture, const QColor& automaticColor)
{
    m_picture = picture;
    m_automaticColor = automaticColor;
    update();
}

void ImagePreview::setImage(const ImageProperties& image, const QSizeF& shape)
{
    m_image = image;
    m_shape = shape;
    update();
}

void ImagePreview::setShowsCrop(bool shows)
{
    m_showsCrop = shows;
    update();
}

QSize ImagePreview::sizeHint() const
{
    return m_size;
}

void ImagePreview::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);
    QPainter painter(this);
    const QRectF area = QRectF(contentsRect()).adjusted(6.0, 6.0, -6.0, -6.0);
    painter.fillRect(contentsRect(), palette().color(QPalette::Base));
    if (m_picture.isNull()) {
        //: The preview of an image whose file is missing. In Polish: Brak pliku obrazu
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(area, Qt::AlignCenter | Qt::TextWordWrap, tr("Image file missing"));
        return;
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (m_showsCrop) {
        // Turned and mirrored but whole, its cut off parts shaded
        ImageProperties whole = m_image;
        whole.crop = ImageCrop{};
        const QSize pixels = editor::shownPixels(m_picture.size(), whole);
        const QSizeF fitted = QSizeF(pixels).scaled(area.size(), Qt::KeepAspectRatio);
        const QRectF rect(area.center().x() - fitted.width() / 2.0,
                          area.center().y() - fitted.height() / 2.0, fitted.width(), fitted.height());
        painter.drawImage(rect, editor::shownPicture(m_picture, whole, fitted.toSize().expandedTo(QSize(1, 1))));
        const ImageCrop crop = editor::shownCrop(m_image);
        const QRectF kept = rect.adjusted(rect.width() * crop.left / 100.0, rect.height() * crop.top / 100.0,
                                          -rect.width() * crop.right / 100.0,
                                          -rect.height() * crop.bottom / 100.0);
        QColor shade = palette().color(QPalette::Window);
        shade.setAlphaF(0.75);
        QPainterPath cut;
        cut.addRect(rect);
        QPainterPath part;
        part.addRect(kept);
        painter.fillPath(cut.subtracted(part), shade);
        painter.setPen(QPen(palette().color(QPalette::Highlight), 1.0, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(kept.adjusted(0.5, 0.5, -0.5, -0.5));
        return;
    }

    // With its look and frame, in its shape (an own height stretches it), as large as fits
    const QSizeF picture = m_shape.isEmpty()
                               ? QSizeF(editor::shownPixels(m_picture.size(), m_image)) * POINTS_PER_PIXEL
                               : m_shape;
    const double side = frameSide(m_image);
    const double shadow = m_image.shadow ? SHADOW_POINTS : 0.0;
    const QSizeF box(picture.width() + 2.0 * side + shadow, picture.height() + 2.0 * side + shadow);
    const double scale = std::min(area.width() / box.width(), area.height() / box.height());
    const QSizeF shownBox = box * scale;
    const QRectF outer(area.center().x() - shownBox.width() / 2.0,
                       area.center().y() - shownBox.height() / 2.0,
                       shownBox.width() - shadow * scale, shownBox.height() - shadow * scale);
    const QColor color = m_image.borderColor.isValid() ? m_image.borderColor : m_automaticColor;
    if (m_image.shadow) {
        QColor shade = color;
        shade.setAlphaF(SHADOW_OPACITY);
        painter.fillRect(outer.translated(shadow * scale, shadow * scale), shade);
        painter.fillRect(outer, palette().color(QPalette::Base));
    }
    if (m_image.borderLine != ImageBorderLine::None) {
        const double width = std::max(1.0, m_image.borderWidth * scale);
        Qt::PenStyle style = Qt::SolidLine;
        if (m_image.borderLine == ImageBorderLine::Dashed) {
            style = Qt::DashLine;
        } else if (m_image.borderLine == ImageBorderLine::Dotted) {
            style = Qt::DotLine;
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(color, width, style, Qt::FlatCap, Qt::MiterJoin));
        painter.drawRect(outer.adjusted(width / 2.0, width / 2.0, -width / 2.0, -width / 2.0));
        if (m_image.borderLine == ImageBorderLine::Double) {
            const double inner = 2.0 * width;
            painter.drawRect(outer.adjusted(inner + width / 2.0, inner + width / 2.0,
                                            -inner - width / 2.0, -inner - width / 2.0));
        }
    }
    const QRectF rect = outer.adjusted(side * scale, side * scale, -side * scale, -side * scale);
    if (!rect.isEmpty()) {
        painter.drawImage(rect, editor::shownPicture(m_picture, m_image,
                                                     rect.size().toSize().expandedTo(QSize(1, 1))));
    }
}

// =============================================================================
// ImagePropertiesDialog
// =============================================================================

ImagePropertiesDialog::ImagePropertiesDialog(const editor::ImagePropertiesData& data,
                                             const QColor& automaticColor, QWidget* parent)
    : KalahariDialog(parent)
    , m_data(data)
    , m_image(data.image)
    , m_picture(previewPicture(data.filePath))
    , m_automaticColor(automaticColor.isValid() ? automaticColor : palette().color(QPalette::Text))
    , m_caption(data.caption)
{
    //: In Polish: Właściwości obrazu
    setHeading(tr("Image Properties"),
               //: In Polish: Wygląd zaznaczonego obrazu: plik, rozmiar, przycięcie, obramowanie,
               //: kolory, opis i podpis.
               tr("The look of the selected image: its file, size, crop, border, colors, "
                  "description and caption."));
    setHeadingIcon(QStringLiteral("image.properties"));
    setCompactHeading(true);
    setAcceptText(tr("OK"));
    //: The tooltip of the gear of the windows of images. In Polish: Ustawienia obrazów
    addSettingsButton(QString::fromLatin1(SettingsDialog::IMAGES_PAGE), tr("Image Settings"));
    if (!m_caption.label.isEmpty()) {
        m_captionLabels << m_caption.label;
    }

    auto* body = new QHBoxLayout();
    body->setSpacing(12);
    m_pageList = new QListWidget(this);
    m_pageList->setFixedWidth(PAGE_LIST_WIDTH);
    //: The list of the pages of Image Properties. In Polish: Części okna
    m_pageList->setAccessibleName(tr("Pages"));
    m_pageList->setItemDelegate(new PageNameDelegate(m_pageList));
    m_pages = new QStackedWidget(this);
    // Each page takes the room of the largest: the window has one size on all of them
    m_pages->addWidget(createImagePage());
    m_pages->addWidget(createSizePage());
    m_pages->addWidget(createCropPage());
    m_pages->addWidget(createBorderPage());
    m_pages->addWidget(createColorsPage());
    m_pages->addWidget(createDescriptionPage());
    m_pages->addWidget(createCaptionPage());
    const QString names[] = {
        //: The pages of Image Properties. In Polish: &Obraz
        tr("&Image"),
        //: In Polish: &Rozmiar i obrót
        tr("&Size and Rotation"),
        //: In Polish: &Przycięcie
        tr("&Crop"),
        //: In Polish: Obramo&wanie
        tr("&Border"),
        //: In Polish: &Kolory
        tr("C&olors"),
        //: In Polish: Op&is
        tr("&Description"),
        //: In Polish: Po&dpis
        tr("Ca&ption")};
    for (const QString& name : names) {
        auto* item = new QListWidgetItem(withoutAccessKey(name), m_pageList);
        item->setData(Qt::UserRole, name);
    }
    body->addWidget(m_pageList);
    body->addWidget(m_pages, 1);
    contentLayout()->addLayout(body, 1);

    connect(m_pageList, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_pageList->setCurrentRow(0);
    addPageKeys();
    refresh();
}

void ImagePropertiesDialog::addPageKeys()
{
    // Alt with the letter of a page shows it; Ctrl+Tab and Ctrl+Shift+Tab go through them
    for (int row = 0; row < m_pageList->count(); ++row) {
        const QChar key = accessKeyOf(m_pageList->item(row)->data(Qt::UserRole).toString());
        if (key.isNull()) {
            continue;
        }
        auto* shortcut = new QShortcut(QKeySequence(QStringLiteral("Alt+") + key), this);
        connect(shortcut, &QShortcut::activated, this, [this, row]() {
            m_pageList->setCurrentRow(row);
            m_pageList->setFocus(Qt::ShortcutFocusReason);
        });
    }
    const auto step = [this](int by) {
        const int count = m_pageList->count();
        m_pageList->setCurrentRow((m_pageList->currentRow() + by + count) % count);
    };
    auto* next = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab), this);
    connect(next, &QShortcut::activated, this, [step]() { step(1); });
    auto* previous = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab), this);
    connect(previous, &QShortcut::activated, this, [step]() { step(-1); });
    auto* backtab = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Backtab), this);
    connect(backtab, &QShortcut::activated, this, [step]() { step(-1); });
}

editor::ImagePropertiesData ImagePropertiesDialog::data() const
{
    editor::ImagePropertiesData data = m_data;
    data.image = editor::normalizedImage(m_image);
    data.caption = m_caption;
    data.caption.text = m_caption.text.trimmed();
    // A caption of its text alone needs one
    data.caption.shown = m_caption.shown && (m_caption.numbered ? !m_caption.label.isEmpty()
                                                                : !data.caption.text.isEmpty());
    // The size and the crop of a new file were set when it was chosen
    data.keepSizeAndCrop = true;
    return data;
}

void ImagePropertiesDialog::setFileChooser(std::function<QString()> chooser)
{
    m_fileChooser = std::move(chooser);
    m_changeButton->setVisible(static_cast<bool>(m_fileChooser));
}

void ImagePropertiesDialog::setImageSaver(std::function<void()> saver)
{
    m_imageSaver = std::move(saver);
    refreshImagePage();
}

void ImagePropertiesDialog::setCropInTextAvailable(bool available)
{
    m_cropInText->setVisible(available);
    refreshCropPage();
}

void ImagePropertiesDialog::showPage(Page page)
{
    m_pageList->setCurrentRow(std::clamp(static_cast<int>(page), 0, PageCount - 1));
}

ImagePropertiesDialog::Page ImagePropertiesDialog::currentPage() const
{
    return static_cast<Page>(std::max(0, m_pages->currentIndex()));
}

// -----------------------------------------------------------------------------
// Sizes
// -----------------------------------------------------------------------------

QSizeF ImagePropertiesDialog::originalSize() const
{
    if (m_data.filePixels.isEmpty()) {
        return QSizeF();
    }
    return QSizeF(editor::shownPixels(m_data.filePixels, m_image)) * POINTS_PER_PIXEL;
}

QSizeF ImagePropertiesDialog::pointSize() const
{
    const QSizeF original = originalSize();
    double width = 0.0;
    switch (m_image.unit) {
        case ImageWidthUnit::Natural:
            width = original.isEmpty() ? m_data.shownWidth : original.width();
            break;
        case ImageWidthUnit::Percent:
            width = m_data.lineWidth * m_image.width / 100.0;
            break;
        case ImageWidthUnit::Centimeters:
            width = m_image.width * POINTS_PER_CENTIMETER;
            break;
    }
    double height = 0.0;
    if (m_image.height > 0.0) {
        height = m_image.height * POINTS_PER_CENTIMETER;
    } else if (!original.isEmpty()) {
        height = width * original.height() / original.width();
    } else {
        height = width / 4.0;  // a missing file: the frame the editor shows
    }
    return QSizeF(width, height);
}

void ImagePropertiesDialog::setPointWidth(double points)
{
    const double before = pointSize().width();
    points = std::max(points, 1.0);
    const QSizeF original = originalSize();
    switch (m_widthUnit->currentIndex()) {
        case WidthOfText:
            m_image.unit = ImageWidthUnit::Percent;
            m_image.width = m_data.lineWidth > 0.0 ? points / m_data.lineWidth * 100.0 : 100.0;
            break;
        case WidthOfOriginal:
            if (!original.isEmpty() && std::abs(points - original.width()) < 0.5) {
                m_image.unit = ImageWidthUnit::Natural;
                m_image.width = 0.0;
                break;
            }
            [[fallthrough]];
        default:
            m_image.unit = ImageWidthUnit::Centimeters;
            m_image.width = points / POINTS_PER_CENTIMETER;
            break;
    }
    // An own height in proportion: it changes with the width
    if (m_keepProportions->isChecked() && m_image.height > 0.0 && before > 0.0) {
        m_image.height *= points / before;
    }
    m_image = editor::normalizedImage(m_image);
}

void ImagePropertiesDialog::setPointHeight(double points)
{
    const QSizeF size = pointSize();
    points = std::max(points, 1.0);
    if (m_keepProportions->isChecked()) {
        if (size.height() > 0.0) {
            setPointWidth(size.width() * points / size.height());
        }
        return;
    }
    m_image.height = points / POINTS_PER_CENTIMETER;
    m_image = editor::normalizedImage(m_image);
}

void ImagePropertiesDialog::turn(int quarters)
{
    const QSizeF before = pointSize();
    m_image.rotation = ((m_image.rotation + quarters) % 4 + 4) % 4;
    // An image of its own shape turns with it: its width becomes its height
    if (m_image.height > 0.0 && quarters % 2 != 0) {
        const bool kept = m_keepProportions->isChecked();
        m_keepProportions->setChecked(false);
        setPointWidth(before.height());
        m_image.height = before.width() / POINTS_PER_CENTIMETER;
        m_keepProportions->setChecked(kept);
    }
    m_image = editor::normalizedImage(m_image);
}

void ImagePropertiesDialog::setShownCrop(const ImageCrop& crop)
{
    m_image = editor::withShownCrop(m_image, crop, m_keepScale->isChecked(),
                                    pointSize().width() / POINTS_PER_CENTIMETER);
}

// -----------------------------------------------------------------------------
// The pages
// -----------------------------------------------------------------------------

QWidget* ImagePropertiesDialog::createImagePage()
{
    auto* page = new QWidget(this);
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    m_previews[0] = new ImagePreview(LARGE_PREVIEW, page);
    //: In Polish: Podgląd obrazu
    m_previews[0]->setAccessibleName(tr("Preview of the image"));
    layout->addWidget(m_previews[0], 0, Qt::AlignTop);

    auto* column = new QVBoxLayout();
    column->setSpacing(8);
    m_fileInfo = new QLabel(page);
    m_fileInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_fileName = new QLabel(page);
    m_fileName->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_fileName->setWordWrap(true);
    column->addWidget(m_fileInfo);
    column->addWidget(m_fileName);

    auto* buttons = new QHBoxLayout();
    //: In Polish: Z&mień obraz...
    m_changeButton = new QPushButton(tr("C&hange Image..."), page);
    m_changeButton->setAutoDefault(false);
    m_changeButton->setVisible(false);
    //: In Polish: Zapisz j&ako obraz...
    m_saveButton = new QPushButton(tr("Sa&ve as Image..."), page);
    m_saveButton->setAutoDefault(false);
    m_saveButton->setVisible(false);
    buttons->addWidget(m_changeButton);
    buttons->addWidget(m_saveButton);
    buttons->addStretch(1);
    column->addLayout(buttons);

    //: In Polish: Przy zmianie obrazu zac&howaj rozmiar i przycięcie
    m_keepSizeAndCrop = new QCheckBox(tr("&Keep the size and crop when the image is changed"), page);
    m_keepSizeAndCrop->setChecked(
        core::SettingsManager::getInstance().get<bool>("editor.images.keepSizeOnChange", true));
    column->addWidget(m_keepSizeAndCrop);
    //: In Polish: Plik jest w folderze obrazów książki (images/). Obraz wstawiony z innego miejsca
    //: trafia tam jako kopia, a oryginał zostaje nietknięty.
    column->addWidget(hintLabel(tr("The file is in the book's folder of images (images/). An "
                                   "image inserted from elsewhere goes there as a copy; the "
                                   "original stays as it was."),
                                page));
    column->addStretch(1);
    layout->addLayout(column, 1);

    connect(m_keepSizeAndCrop, &QCheckBox::toggled, this, [](bool keep) {
        core::SettingsManager::getInstance().set<bool>("editor.images.keepSizeOnChange", keep);
    });
    connect(m_changeButton, &QPushButton::clicked, this, [this]() {
        if (!m_fileChooser) {
            return;
        }
        const QString path = m_fileChooser();
        if (path.isEmpty()) {
            return;
        }
        const std::optional<QSize> pixels = editor::ImageStore::fileSize(path);
        if (!pixels || !editor::isImageFileName(path)) {
            MessageDialog::warning(
                this,
                //: In Polish: Nie można zmienić obrazu
                tr("Image Not Changed"),
                //: In Polish: Plik „%1” nie jest obrazem, który Kalahari potrafi pokazać (PNG,
                //: JPEG, GIF, BMP, SVG).
                tr("The file \"%1\" is not a picture Kalahari can show (PNG, JPEG, GIF, BMP, SVG).")
                    .arg(QFileInfo(path).fileName()));
            return;
        }
        m_data.newFile = path;
        m_data.filePath = path;
        m_data.filePixels = *pixels;
        m_data.fileBytes = QFileInfo(path).size();
        m_picture = previewPicture(path);
        if (!m_keepSizeAndCrop->isChecked()) {
            m_image.unit = ImageWidthUnit::Natural;
            m_image.width = 0.0;
            m_image.height = 0.0;
            m_image.crop = ImageCrop{};
        }
        refresh();
    });
    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        if (m_imageSaver) {
            m_imageSaver();
        }
    });
    return page;
}

QWidget* ImagePropertiesDialog::createSizePage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    //: In Polish: Rozmiar
    auto* sizeGroup = new QGroupBox(tr("Size"), page);
    auto* grid = new QGridLayout(sizeGroup);
    m_widthPercent = new QDoubleSpinBox(sizeGroup);
    m_widthPercent->setRange(1.0, 1000.0);
    m_widthPercent->setDecimals(0);
    m_widthPercent->setSuffix(QStringLiteral(" %"));
    m_widthPercent->setFixedWidth(NUMBER_FIELD_WIDTH);
    m_widthLength = new LengthSpinBox(LengthUnit::Centimeters, editor::MIN_IMAGE_CENTIMETERS,
                                      editor::MAX_IMAGE_CENTIMETERS, sizeGroup);
    m_widthLength->setFixedWidth(NUMBER_FIELD_WIDTH);
    m_widthUnit = new QComboBox(sizeGroup);
    //: The units of an image's width. In Polish: % szerokości tekstu
    m_widthUnit->addItem(tr("% of the text width"));
    //: In Polish: % oryginału
    m_widthUnit->addItem(tr("% of the original"));
    //: The width of an image as a length (cm, mm, in...). In Polish: Długość
    m_widthUnit->addItem(tr("Length"));
    //: In Polish: Jednostka szerokości
    m_widthUnit->setAccessibleName(tr("Unit of the width"));
    m_heightPercent = new QDoubleSpinBox(sizeGroup);
    m_heightPercent->setRange(1.0, 1000.0);
    m_heightPercent->setDecimals(0);
    m_heightPercent->setSuffix(QStringLiteral(" %"));
    m_heightPercent->setFixedWidth(NUMBER_FIELD_WIDTH);
    m_heightLength = new LengthSpinBox(LengthUnit::Centimeters, editor::MIN_IMAGE_CENTIMETERS,
                                       editor::MAX_IMAGE_CENTIMETERS, sizeGroup);
    m_heightLength->setFixedWidth(NUMBER_FIELD_WIDTH);
    m_heightUnit = new QComboBox(sizeGroup);
    m_heightUnit->addItem(tr("% of the original"));
    m_heightUnit->addItem(tr("Length"));
    //: In Polish: Jednostka wysokości
    m_heightUnit->setAccessibleName(tr("Unit of the height"));

    //: In Polish: &Szerokość
    auto* widthLabel = new QLabel(tr("&Width"), sizeGroup);
    //: In Polish: W&ysokość
    auto* heightLabel = new QLabel(tr("&Height"), sizeGroup);
    auto* widthFields = new QHBoxLayout();
    widthFields->addWidget(m_widthPercent);
    widthFields->addWidget(m_widthLength);
    widthFields->addWidget(m_widthUnit);
    widthFields->addStretch(1);
    auto* heightFields = new QHBoxLayout();
    heightFields->addWidget(m_heightPercent);
    heightFields->addWidget(m_heightLength);
    heightFields->addWidget(m_heightUnit);
    heightFields->addStretch(1);
    grid->addWidget(widthLabel, 0, 0);
    grid->addLayout(widthFields, 0, 1);
    grid->addWidget(heightLabel, 1, 0);
    grid->addLayout(heightFields, 1, 1);
    //: In Polish: Zac&howaj proporcje
    m_keepProportions = new QCheckBox(tr("&Keep proportions"), sizeGroup);
    m_keepProportions->setChecked(m_image.height <= 0.0);
    m_scale = new QLabel(sizeGroup);
    auto* proportions = new QHBoxLayout();
    proportions->addWidget(m_keepProportions);
    proportions->addStretch(1);
    proportions->addWidget(m_scale);
    grid->addLayout(proportions, 2, 1);
    //: In Polish: Rozmiar ory&ginalny
    m_originalButton = new QPushButton(tr("Original Si&ze"), sizeGroup);
    m_originalButton->setAutoDefault(false);
    m_originalInfo = new QLabel(sizeGroup);
    m_originalInfo->setForegroundRole(QPalette::PlaceholderText);
    m_originalInfo->setWordWrap(true);
    auto* originalRow = new QHBoxLayout();
    originalRow->addWidget(m_originalButton);
    originalRow->addWidget(m_originalInfo, 1);
    grid->addLayout(originalRow, 3, 0, 1, 2);
    grid->setColumnStretch(1, 1);
    layout->addWidget(sizeGroup);

    //: In Polish: Obrót i odbicie
    auto* turnGroup = new QGroupBox(tr("Rotation and Flip"), sizeGroup);
    auto* turnGrid = new QGridLayout(turnGroup);
    m_angle = new QComboBox(turnGroup);
    for (int quarter = 0; quarter < 4; ++quarter) {
        //: An angle of an image's rotation: "90°". In Polish: %1°
        m_angle->addItem(tr("%1°").arg(QLocale().toString(quarter * 90)));
    }
    //: In Polish: Ką&t
    auto* angleLabel = new QLabel(tr("&Angle"), turnGroup);
    angleLabel->setBuddy(m_angle);
    //: In Polish: Obróć w &lewo o 90°
    auto* left = new QPushButton(tr("Rotate &Left 90°"), turnGroup);
    left->setAutoDefault(false);
    //: In Polish: Obróć w pr&awo o 90°
    auto* right = new QPushButton(tr("Rotate &Right 90°"), turnGroup);
    right->setAutoDefault(false);
    //: In Polish: Odbij w poziomi&e
    m_flipHorizontal = new QCheckBox(tr("Flip Horizo&ntally"), turnGroup);
    //: In Polish: Odbij w pio&nie
    m_flipVertical = new QCheckBox(tr("Flip &Vertically"), turnGroup);
    auto* angleRow = new QHBoxLayout();
    angleRow->addWidget(m_angle);
    angleRow->addWidget(left);
    angleRow->addWidget(right);
    angleRow->addStretch(1);
    turnGrid->addWidget(angleLabel, 0, 0);
    turnGrid->addLayout(angleRow, 0, 1);
    auto* flips = new QHBoxLayout();
    flips->addWidget(m_flipHorizontal);
    flips->addWidget(m_flipVertical);
    flips->addStretch(1);
    turnGrid->addLayout(flips, 1, 1);
    turnGrid->setColumnStretch(1, 1);
    layout->addWidget(turnGroup);
    layout->addStretch(1);

    // The label of a field goes to the one shown
    const auto buddies = [this, widthLabel, heightLabel]() {
        widthLabel->setBuddy(m_widthUnit->currentIndex() == WidthLength
                                 ? static_cast<QWidget*>(m_widthLength)
                                 : m_widthPercent);
        heightLabel->setBuddy(m_heightUnit->currentIndex() == HeightLength
                                  ? static_cast<QWidget*>(m_heightLength)
                                  : m_heightPercent);
    };
    // The unit the image has: a percent of the text, its own size, a length
    m_widthUnit->setCurrentIndex(m_image.unit == ImageWidthUnit::Percent ? WidthOfText
                                 : m_image.unit == ImageWidthUnit::Natural && !m_data.filePixels.isEmpty()
                                     ? WidthOfOriginal
                                     : WidthLength);
    m_heightUnit->setCurrentIndex(m_image.height > 0.0 || m_data.filePixels.isEmpty() ? HeightLength
                                                                                       : HeightOfOriginal);
    buddies();
    // Another unit of the width: the image keeps its width, now in that unit (a percent of
    // the text follows the text). The height's unit only shows it otherwise.
    connect(m_widthUnit, &QComboBox::currentIndexChanged, this, [this, buddies]() {
        buddies();
        if (!m_updating) {
            setPointWidth(pointSize().width());
        }
        refresh();
    });
    connect(m_heightUnit, &QComboBox::currentIndexChanged, this, [this, buddies]() {
        buddies();
        refresh();
    });
    const auto widthEdited = [this]() {
        if (m_updating) {
            return;
        }
        const QSizeF original = originalSize();
        switch (m_widthUnit->currentIndex()) {
            case WidthOfText:
                setPointWidth(m_data.lineWidth * m_widthPercent->value() / 100.0);
                break;
            case WidthOfOriginal:
                setPointWidth(original.width() * m_widthPercent->value() / 100.0);
                break;
            default:
                setPointWidth(m_widthLength->storedValue() * POINTS_PER_CENTIMETER);
                break;
        }
        refresh();
    };
    const auto heightEdited = [this]() {
        if (m_updating) {
            return;
        }
        if (m_heightUnit->currentIndex() == HeightOfOriginal) {
            setPointHeight(originalSize().height() * m_heightPercent->value() / 100.0);
        } else {
            setPointHeight(m_heightLength->storedValue() * POINTS_PER_CENTIMETER);
        }
        refresh();
    };
    connect(m_widthPercent, &QDoubleSpinBox::valueChanged, this, widthEdited);
    connect(m_widthLength, &QDoubleSpinBox::valueChanged, this, widthEdited);
    connect(m_heightPercent, &QDoubleSpinBox::valueChanged, this, heightEdited);
    connect(m_heightLength, &QDoubleSpinBox::valueChanged, this, heightEdited);
    connect(m_keepProportions, &QCheckBox::toggled, this, [this](bool keep) {
        if (m_updating) {
            return;
        }
        // Without proportions the height is the image's own: it stays when the width changes
        if (!keep && m_image.height <= 0.0) {
            m_image.height = pointSize().height() / POINTS_PER_CENTIMETER;
            m_image = editor::normalizedImage(m_image);
        }
        refresh();
    });
    connect(m_originalButton, &QPushButton::clicked, this, [this]() {
        m_image.unit = ImageWidthUnit::Natural;
        m_image.width = 0.0;
        m_image.height = 0.0;
        m_keepProportions->setChecked(true);
        refresh();
    });
    connect(m_angle, &QComboBox::activated, this, [this](int quarter) {
        turn(quarter - m_image.rotation);
        refresh();
    });
    connect(left, &QPushButton::clicked, this, [this]() {
        turn(-1);
        refresh();
    });
    connect(right, &QPushButton::clicked, this, [this]() {
        turn(1);
        refresh();
    });
    connect(m_flipHorizontal, &QCheckBox::toggled, this, [this](bool flip) {
        if (!m_updating) {
            m_image.flipHorizontal = flip;
            refresh();
        }
    });
    connect(m_flipVertical, &QCheckBox::toggled, this, [this](bool flip) {
        if (!m_updating) {
            m_image.flipVertical = flip;
            refresh();
        }
    });
    return page;
}

QWidget* ImagePropertiesDialog::createCropPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    m_previews[1] = new ImagePreview(LARGE_PREVIEW, page);
    m_previews[1]->setShowsCrop(true);
    //: In Polish: Podgląd przycięcia
    m_previews[1]->setAccessibleName(tr("Preview of the crop"));
    layout->addWidget(m_previews[1], 0, Qt::AlignTop);

    auto* column = new QVBoxLayout();
    auto* grid = new QGridLayout();
    const QString names[] = {
        //: The sides of an image's crop. In Polish: Z &lewej
        tr("&Left"),
        //: In Polish: Z pr&awej
        tr("&Right"),
        //: In Polish: Z &góry
        tr("&Top"),
        //: In Polish: Z doł&u
        tr("Botto&m")};
    for (int side = 0; side < 4; ++side) {
        auto* field = new QDoubleSpinBox(page);
        field->setRange(0.0, editor::MAX_IMAGE_CROP);
        field->setDecimals(1);
        field->setSuffix(QStringLiteral(" %"));
        field->setFixedWidth(NUMBER_FIELD_WIDTH);
        m_cropSides[static_cast<size_t>(side)] = field;
        auto* label = new QLabel(names[side], page);
        label->setBuddy(field);
        grid->addWidget(label, side / 2, (side % 2) * 2);
        grid->addWidget(field, side / 2, (side % 2) * 2 + 1);
        connect(field, &QDoubleSpinBox::valueChanged, this, [this]() {
            if (m_updating) {
                return;
            }
            setShownCrop(ImageCrop{m_cropSides[0]->value(), m_cropSides[2]->value(),
                                   m_cropSides[1]->value(), m_cropSides[3]->value()});
            m_aspect->setCurrentIndex(0);
            refresh();
        });
    }
    grid->setColumnStretch(4, 1);
    column->addLayout(grid);

    m_aspect = new QComboBox(page);
    //: The proportions of a crop. In Polish: Dowolne
    m_aspect->addItem(tr("Free"));
    //: In Polish: Jak oryginał
    m_aspect->addItem(tr("As the original"));
    for (const char* ratio : {"1 : 1", "4 : 3", "3 : 2", "16 : 9"}) {
        m_aspect->addItem(QString::fromLatin1(ratio));
    }
    //: In Polish: Propor&cje
    auto* aspectLabel = new QLabel(tr("&Aspect ratio"), page);
    aspectLabel->setBuddy(m_aspect);
    auto* aspectRow = new QHBoxLayout();
    aspectRow->addWidget(aspectLabel);
    aspectRow->addWidget(m_aspect);
    aspectRow->addStretch(1);
    column->addLayout(aspectRow);

    //: In Polish: Zac&howaj rozmiar obrazu w tekście
    m_keepSize = new QRadioButton(tr("Keep the image's si&ze in the text"), page);
    //: In Polish: Zachowaj &skalę (obraz się zmniejsza)
    m_keepScale = new QRadioButton(tr("Keep the scal&e (the image gets smaller)"), page);
    m_keepSize->setChecked(true);
    column->addWidget(m_keepSize);
    column->addWidget(m_keepScale);
    //: Closes the window and shows the marks of the crop on the image in the text. In
    //: Polish: Przytnij w &tekście
    m_cropInText = new QPushButton(tr("Crop in the Te&xt"), page);
    m_cropInText->setAutoDefault(false);
    m_cropInText->setVisible(false);
    //: In Polish: &Bez przycięcia
    m_removeCrop = new QPushButton(tr("Remo&ve Crop"), page);
    m_removeCrop->setAutoDefault(false);
    auto* buttons = new QHBoxLayout();
    buttons->addWidget(m_cropInText);
    buttons->addWidget(m_removeCrop);
    buttons->addStretch(1);
    column->addLayout(buttons);
    //: In Polish: Przycięcie niczego nie usuwa z pliku; można je zmienić albo zdjąć w każdej chwili.
    column->addWidget(hintLabel(
        tr("The crop takes nothing out of the file; it can be changed or removed at any time."), page));
    column->addStretch(1);
    layout->addLayout(column, 1);

    connect(m_aspect, &QComboBox::activated, this, [this](int index) {
        if (index <= 0) {
            return;
        }
        ImageCrop crop;
        if (index > 1) {
            // The largest part of the turned picture of the ratio, in its middle
            ImageProperties whole = m_image;
            whole.crop = ImageCrop{};
            const QSize file = m_data.filePixels.isEmpty() ? m_picture.size() : m_data.filePixels;
            const QSizeF shown(editor::shownPixels(file, whole));
            const double ratio = ASPECTS[index];
            if (shown.height() > 0.0 && shown.width() / shown.height() > ratio) {
                const double cut = (1.0 - shown.height() * ratio / shown.width()) * 50.0;
                crop.left = crop.right = cut;
            } else if (shown.width() > 0.0) {
                const double cut = (1.0 - shown.width() / ratio / shown.height()) * 50.0;
                crop.top = crop.bottom = cut;
            }
        }
        setShownCrop(crop);
        refresh();
    });
    connect(m_cropInText, &QPushButton::clicked, this, [this]() {
        // The changes made so far are kept, as with OK
        m_cropInTextAsked = true;
        accept();
    });
    connect(m_removeCrop, &QPushButton::clicked, this, [this]() {
        setShownCrop(ImageCrop{});
        m_aspect->setCurrentIndex(0);
        refresh();
    });
    return page;
}

QWidget* ImagePropertiesDialog::createBorderPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    m_previews[2] = new ImagePreview(LARGE_PREVIEW, page);
    //: In Polish: Podgląd obramowania
    m_previews[2]->setAccessibleName(tr("Preview of the border"));
    layout->addWidget(m_previews[2], 0, Qt::AlignTop);

    auto* column = new QVBoxLayout();
    auto* grid = new QGridLayout();
    m_line = new QComboBox(page);
    //: The lines of an image's border. In Polish: Brak, Ciągła, Kreskowana, Kropkowana, Podwójna
    for (const QString& name : {tr("None"), tr("Solid"), tr("Dashed"), tr("Dotted"), tr("Double")}) {
        m_line->addItem(name);
    }
    m_lineWidth = new QComboBox(page);
    for (const double width : LINE_WIDTHS) {
        m_lineWidth->addItem(lineWidthText(width), width);
    }
    if (m_lineWidth->findData(m_image.borderWidth) < 0) {
        const auto place = std::count_if(std::begin(LINE_WIDTHS), std::end(LINE_WIDTHS),
                                         [this](double width) { return width < m_image.borderWidth; });
        m_lineWidth->insertItem(static_cast<int>(place), lineWidthText(m_image.borderWidth),
                                m_image.borderWidth);
    }
    m_colorButton = new QPushButton(page);
    m_colorButton->setAutoDefault(false);
    m_colorMenu = new ColorMenu(ColorMenu::Kind::Text, [this]() { return m_image.borderColor; }, this);
    m_colorButton->setMenu(m_colorMenu);
    m_padding = new LengthSpinBox(LengthUnit::Points, 0.0, editor::MAX_IMAGE_BORDER_PADDING, page);
    m_padding->setFixedWidth(NUMBER_FIELD_WIDTH);
    const std::pair<QString, QWidget*> fields[] = {
        //: In Polish: Li&nia
        {tr("&Line"), m_line},
        //: The width of the border's line. In Polish: Gr&ubość
        {tr("&Width", "line"), m_lineWidth},
        //: In Polish: &Barwa
        {tr("Colo&r"), m_colorButton},
        //: The room between an image's border and its picture. In Polish: Ods&tęp od obrazu
        {tr("Space i&nside"), m_padding}};
    int row = 0;
    for (const auto& [text, field] : fields) {
        auto* label = new QLabel(text, page);
        label->setBuddy(field);
        grid->addWidget(label, row, 0);
        grid->addWidget(field, row, 1, Qt::AlignLeft);
        ++row;
    }
    grid->setColumnStretch(1, 1);
    column->addLayout(grid);
    //: In Polish: &Cień
    m_shadow = new QCheckBox(tr("S&hadow"), page);
    column->addWidget(m_shadow);
    //: In Polish: Barwa „Automatyczna” to kolor tekstu. Listę barw do wyboru można zmienić
    //: w Ustawieniach.
    column->addWidget(hintLabel(tr("The color Automatic is the color of the text. The colors to "
                                   "choose from are set in Settings."),
                                page));
    column->addStretch(1);
    layout->addLayout(column, 1);

    connect(m_line, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!m_updating) {
            m_image.borderLine = static_cast<ImageBorderLine>(std::max(0, index));
            refresh();
        }
    });
    connect(m_lineWidth, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!m_updating) {
            m_image.borderWidth = m_lineWidth->itemData(index).toDouble();
            refresh();
        }
    });
    connect(m_colorMenu, &ColorMenu::colorChosen, this, [this](const QColor& color) {
        m_image.borderColor = color;
        refresh();
    });
    connect(m_padding, &QDoubleSpinBox::valueChanged, this, [this]() {
        if (!m_updating) {
            m_image.borderPadding = m_padding->storedValue();
            refresh();
        }
    });
    connect(m_shadow, &QCheckBox::toggled, this, [this](bool shadow) {
        if (!m_updating) {
            m_image.shadow = shadow;
            refresh();
        }
    });
    return page;
}

QWidget* ImagePropertiesDialog::createColorsPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    // The filters as tiles, each a picture of its look
    m_filters = new QButtonGroup(this);
    auto* tiles = new QHBoxLayout();
    const QString names[] = {
        //: The filters of an image. In Polish: Or&yginał
        tr("Origin&al"),
        //: In Polish: Odcienie s&zarości
        tr("&Grayscale"),
        //: In Polish: S&epia
        tr("S&epia"),
        //: In Polish: &Czarno-biały
        tr("Black and &White"),
        //: A pale picture, to stand behind the text. In Polish: Z&nak wodny
        tr("Was&h")};
    for (int filter = 0; filter < 5; ++filter) {
        auto* tile = new QToolButton(page);
        tile->setText(names[filter]);
        tile->setCheckable(true);
        tile->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        tile->setIconSize(FILTER_TILE);
        tile->setAutoRaise(true);
        m_filters->addButton(tile, filter);
        tiles->addWidget(tile);
    }
    tiles->addStretch(1);
    layout->addLayout(tiles);

    auto* body = new QHBoxLayout();
    m_previews[3] = new ImagePreview(COLORS_PREVIEW, page);
    //: In Polish: Podgląd obrazu z filtrem
    m_previews[3]->setAccessibleName(tr("Preview of the image with its filter"));
    body->addWidget(m_previews[3], 0, Qt::AlignTop);
    auto* grid = new QGridLayout();
    struct Slider {
        QSlider** slider;
        QString label;
        int minimum;
        int maximum;
    };
    const Slider sliders[] = {
        //: In Polish: &Jasność
        {&m_brightness, tr("B&rightness"), editor::MIN_IMAGE_CORRECTION, editor::MAX_IMAGE_CORRECTION},
        //: In Polish: Kon&trast
        {&m_contrast, tr("Co&ntrast"), editor::MIN_IMAGE_CORRECTION, editor::MAX_IMAGE_CORRECTION},
        //: The gray that becomes black in Black and White. In Polish: Pró&g czerni
        {&m_threshold, tr("&Threshold"), editor::MIN_IMAGE_THRESHOLD, editor::MAX_IMAGE_THRESHOLD},
        //: In Polish: Przezroczy&stość
        {&m_transparency, tr("Transparenc&y"), 0, editor::MAX_IMAGE_TRANSPARENCY}};
    int row = 0;
    for (const Slider& each : sliders) {
        auto* slider = new QSlider(Qt::Horizontal, page);
        slider->setRange(each.minimum, each.maximum);
        slider->setSingleStep(1);
        slider->setPageStep(10);
        *each.slider = slider;
        auto* label = new QLabel(each.label, page);
        label->setBuddy(slider);
        auto* value = new QLabel(page);
        value->setMinimumWidth(value->fontMetrics().horizontalAdvance(QStringLiteral("+100%")));
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_sliderValues[static_cast<size_t>(row)] = value;
        grid->addWidget(label, row, 0);
        grid->addWidget(slider, row, 1);
        grid->addWidget(value, row, 2);
        connect(slider, &QSlider::valueChanged, this, [this]() {
            if (m_updating) {
                return;
            }
            m_image.brightness = m_brightness->value();
            m_image.contrast = m_contrast->value();
            m_image.threshold = m_threshold->value();
            m_image.transparency = m_transparency->value();
            refresh();
        });
        ++row;
    }
    //: In Polish: Przywróć orygin&ał
    auto* reset = new QPushButton(tr("Reset to Origina&l"), page);
    reset->setAutoDefault(false);
    grid->addWidget(reset, row, 0, 1, 3, Qt::AlignLeft);
    grid->setColumnStretch(1, 1);
    body->addLayout(grid, 1);
    layout->addLayout(body);
    //: In Polish: Filtr nie zmienia pliku: jest zapisany w książce i trafia do eksportu.
    layout->addWidget(hintLabel(
        tr("The filter does not change the file: it is kept in the book and goes to the export."), page));
    layout->addStretch(1);

    connect(m_filters, &QButtonGroup::idClicked, this, [this](int filter) {
        m_image.filter = static_cast<ImageFilter>(filter);
        refresh();
    });
    connect(reset, &QPushButton::clicked, this, [this]() {
        m_image.filter = ImageFilter::None;
        m_image.brightness = 0;
        m_image.contrast = 0;
        m_image.threshold = editor::DEFAULT_IMAGE_THRESHOLD;
        m_image.transparency = 0;
        refresh();
    });
    return page;
}

QWidget* ImagePropertiesDialog::createDescriptionPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto hint = [page](const QString& text) { return hintLabel(text, page); };
    m_name = new QLineEdit(m_image.name, page);
    //: In Polish: &Nazwa
    auto* nameLabel = new QLabel(tr("&Name"), page);
    nameLabel->setBuddy(m_name);
    layout->addWidget(nameLabel);
    layout->addWidget(m_name);
    //: In Polish: W Nawigatorze, w oknie wstawiania odsyłacza i w wynikach wyszukiwania.
    layout->addWidget(hint(tr("In the Navigator, the window of a cross-reference and the results of a "
                              "search.")));
    m_alt = new QLineEdit(m_image.alt, page);
    //: In Polish: Tekst &alternatywny
    auto* altLabel = new QLabel(tr("&Alternative text"), page);
    altLabel->setBuddy(m_alt);
    layout->addWidget(altLabel);
    layout->addWidget(m_alt);
    //: In Polish: Czytnik ekranu odczytuje go zamiast obrazu; trafia też do EPUB, DOCX i PDF.
    layout->addWidget(hint(tr("A screen reader reads it instead of the image; it goes to EPUB, DOCX "
                              "and PDF too.")));
    m_description = new QPlainTextEdit(m_image.description, page);
    m_description->setTabChangesFocus(true);
    m_description->setFixedHeight(m_description->fontMetrics().lineSpacing() * 3 +
                                  2 * m_description->frameWidth() + 8);
    //: In Polish: Opis &szczegółowy
    auto* descriptionLabel = new QLabel(tr("&Long description"), page);
    descriptionLabel->setBuddy(m_description);
    layout->addWidget(descriptionLabel);
    layout->addWidget(m_description);
    //: In Polish: Obraz &tylko ozdobny – czytnik ekranu go pomija
    m_decorative = new QCheckBox(tr("Only decorati&ve – screen readers skip it"), page);
    m_decorative->setChecked(m_image.decorative);
    layout->addWidget(m_decorative);
    layout->addStretch(1);

    connect(m_name, &QLineEdit::textEdited, this, [this](const QString& text) { m_image.name = text; });
    connect(m_alt, &QLineEdit::textEdited, this, [this](const QString& text) { m_image.alt = text; });
    connect(m_description, &QPlainTextEdit::textChanged, this,
            [this]() { m_image.description = m_description->toPlainText(); });
    connect(m_decorative, &QCheckBox::toggled, this, [this](bool decorative) {
        m_image.decorative = decorative;
        m_alt->setEnabled(!decorative);
        m_description->setEnabled(!decorative);
    });
    m_alt->setEnabled(!m_image.decorative);
    m_description->setEnabled(!m_image.decorative);
    return page;
}

QWidget* ImagePropertiesDialog::createCaptionPage()
{
    // The preview on the left, as on the other pages, the fields on the right: the page no
    // higher than the others
    auto* page = new QWidget(this);
    auto* body = new QHBoxLayout(page);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(12);
    auto* layout = new QVBoxLayout();

    //: In Polish: Pokaż podpi&s
    m_captionShown = new QCheckBox(tr("S&how a caption"), page);
    layout->addWidget(m_captionShown);

    auto* grid = new QGridLayout();
    grid->setColumnStretch(1, 1);
    m_captionLabel = new QComboBox(page);
    m_captionLabel->setMinimumContentsLength(12);
    m_captionLabel->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    //: In Polish: &Nowa etykieta…
    m_newCaptionLabel = new QPushButton(tr("&New Label..."), page);
    m_captionNumber = hintLabel(QString(), page);  // in two lines when the row is narrow
    auto* labelRow = new QHBoxLayout();
    labelRow->addWidget(m_captionLabel);
    labelRow->addWidget(m_newCaptionLabel);
    labelRow->addWidget(m_captionNumber, 1);
    //: In Polish: &Etykieta
    auto* labelLabel = new QLabel(tr("&Label"), page);
    labelLabel->setBuddy(m_captionLabel);
    grid->addWidget(labelLabel, 0, 0);
    grid->addLayout(labelRow, 0, 1);

    m_captionText = new QLineEdit(m_caption.text, page);
    //: In Polish: &Tekst
    auto* textLabel = new QLabel(tr("&Text"), page);
    textLabel->setBuddy(m_captionText);
    grid->addWidget(textLabel, 1, 0);
    grid->addWidget(m_captionText, 1, 1);

    //: In Polish: Pod obr&azem
    m_captionBelow = new QRadioButton(tr("B&elow the image"), page);
    //: In Polish: Nad obra&zem
    m_captionAbove = new QRadioButton(tr("&Above the image"), page);
    auto* sides = new QButtonGroup(page);
    sides->addButton(m_captionBelow);
    sides->addButton(m_captionAbove);
    auto* positionRow = new QHBoxLayout();
    positionRow->addWidget(m_captionBelow);
    positionRow->addWidget(m_captionAbove);
    positionRow->addStretch(1);
    //: Where the caption is. In Polish: Położenie
    grid->addWidget(new QLabel(tr("Position"), page), 2, 0);
    grid->addLayout(positionRow, 2, 1);
    layout->addLayout(grid);

    //: In Polish: &Bez etykiety i numeru – sam tekst
    m_captionUnnumbered = new QCheckBox(tr("&Without a label and number – the text alone"), page);
    layout->addWidget(m_captionUnnumbered);

    // The image with its caption above or below it
    auto* preview = new QVBoxLayout();
    preview->setSpacing(4);
    m_captionPreviewAbove = new QLabel(page);
    m_captionPreviewBelow = new QLabel(page);
    m_previews[4] = new ImagePreview(CAPTION_PREVIEW, page);
    //: In Polish: Podgląd obrazu z podpisem
    m_previews[4]->setAccessibleName(tr("Preview of the image with its caption"));
    for (QLabel* caption : {m_captionPreviewAbove, m_captionPreviewBelow}) {
        caption->setTextFormat(Qt::RichText);
        caption->setWordWrap(true);
        // A little wider than the picture: the page no wider than the others
        caption->setFixedWidth(CAPTION_PREVIEW.width() + 2 * fontMetrics().averageCharWidth() * 3);
        caption->setAlignment(Qt::AlignHCenter);
    }
    preview->addWidget(m_captionPreviewAbove, 0, Qt::AlignHCenter);
    preview->addWidget(m_previews[4], 0, Qt::AlignHCenter);
    preview->addWidget(m_captionPreviewBelow, 0, Qt::AlignHCenter);
    preview->addStretch(1);
    body->addLayout(preview);
    body->addLayout(layout, 1);

    //: In Polish: Numer na szarym tle nadaje program – nie da się go nadpisać. Jak liczy ta
    //: książka: Wstaw > Podpis… > Numeracja…; etykiety i numeracja nowych książek: koło zębate.
    layout->addWidget(hintLabel(
        tr("The program gives the number on gray: it cannot be typed over. How this book "
           "counts: Insert > Caption... > Numbering...; the labels and numbering of new books: "
           "the gear."),
        page));
    layout->addStretch(1);

    connect(m_captionShown, &QCheckBox::toggled, this, [this](bool shown) {
        if (!m_updating) {
            m_caption.shown = shown;
            refreshCaptionPage();
        }
    });
    connect(m_captionLabel, &QComboBox::currentIndexChanged, this, [this]() {
        if (!m_updating) {
            m_caption.label = m_captionLabel->currentText();
            refreshCaptionPage();
        }
    });
    connect(m_captionText, &QLineEdit::textEdited, this, [this](const QString& text) {
        m_caption.text = text;
        refreshCaptionPage();
    });
    connect(m_captionAbove, &QRadioButton::toggled, this, [this](bool above) {
        if (!m_updating) {
            m_caption.above = above;
            refreshCaptionPage();
        }
    });
    connect(m_captionUnnumbered, &QCheckBox::toggled, this, [this](bool unnumbered) {
        if (!m_updating) {
            m_caption.numbered = !unnumbered;
            refreshCaptionPage();
        }
    });
    connect(m_newCaptionLabel, &QPushButton::clicked, this, &ImagePropertiesDialog::addCaptionLabel);
    return page;
}

void ImagePropertiesDialog::setCaptionLabels(const QStringList& labels,
                                             std::function<QString(const QString& label)> numberOf)
{
    m_captionLabels = labels;
    if (!m_caption.label.isEmpty() && !m_captionLabels.contains(m_caption.label)) {
        m_captionLabels << m_caption.label;  // of an older caption
    }
    if (m_caption.label.isEmpty()) {
        m_caption.label = m_captionLabels.value(0);
    }
    m_captionNumberOf = std::move(numberOf);
    refresh();
}

void ImagePropertiesDialog::addCaptionLabel()
{
    //: The window of a new label of captions. In Polish: Nowa etykieta / Etykieta
    const std::optional<QString> typed = TextInputDialog::getText(this, tr("New Label"), tr("Label"));
    if (!typed) {
        return;
    }
    const QString label = editor::normalizedLabel(*typed);
    if (label.isEmpty()) {
        return;
    }
    // A label of the list, in other letters: that one
    const auto found = std::find_if(m_captionLabels.cbegin(), m_captionLabels.cend(),
                                    [&label](const QString& each) {
                                        return each.compare(label, Qt::CaseInsensitive) == 0;
                                    });
    // Chosen before the list grows: an append leaves the iterator behind
    const QString chosen = found == m_captionLabels.cend() ? label : *found;
    if (!m_captionLabels.contains(chosen)) {
        m_captionLabels << chosen;
    }
    m_caption.label = chosen;
    m_caption.numbered = true;
    m_caption.shown = true;
    refresh();
}

// -----------------------------------------------------------------------------
// The fields follow the image
// -----------------------------------------------------------------------------

void ImagePropertiesDialog::refresh()
{
    m_updating = true;
    for (ImagePreview* preview : m_previews) {
        preview->setPicture(m_picture, m_automaticColor);
        preview->setImage(m_image, pointSize());
    }
    refreshImagePage();
    refreshSizePage();
    refreshCropPage();
    refreshBorderPage();
    refreshColorsPage();
    refreshCaptionPage();
    m_updating = false;
}

void ImagePropertiesDialog::refreshImagePage()
{
    const QFileInfo file(m_data.filePath);
    if (m_data.filePixels.isEmpty()) {
        //: In Polish: Brak pliku obrazu
        m_fileInfo->setText(tr("Image file missing"));
    } else {
        const QString format = QString::fromLatin1(QImageReader::imageFormat(m_data.filePath)).toUpper();
        //: The file of an image: its format, its pixels and its size ("PNG · 1600 × 864 pixels ·
        //: 412 KB"). In Polish: %1 · %2 × %3 pikseli · %4
        m_fileInfo->setText(tr("%1 · %2 × %3 pixels · %4")
                                .arg(format.isEmpty() ? file.suffix().toUpper() : format)
                                .arg(m_data.filePixels.width())
                                .arg(m_data.filePixels.height())
                                .arg(QLocale().formattedDataSize(m_data.fileBytes, 0,
                                                                 QLocale::DataSizeTraditionalFormat)));
    }
    //: The file of an image, in the book's folder. In Polish: Plik: %1
    m_fileName->setText(m_data.newFile.isEmpty()
                            ? tr("File: %1").arg(m_image.source)
                            //: A file chosen to replace an image's: copied with OK. In Polish:
                            //: Nowy plik: %1 (trafi do folderu obrazów po OK)
                            : tr("New file: %1 (copied into the folder of images with OK)")
                                  .arg(file.fileName()));
    m_saveButton->setVisible(static_cast<bool>(m_imageSaver));
    m_saveButton->setEnabled(m_data.newFile.isEmpty() && file.isFile());
}

void ImagePropertiesDialog::refreshSizePage()
{
    const QSizeF size = pointSize();
    const QSizeF original = originalSize();
    const bool hasOriginal = !original.isEmpty();
    const int widthUnit = m_widthUnit->currentIndex();
    const int heightUnit = m_heightUnit->currentIndex();
    m_widthPercent->setVisible(widthUnit != WidthLength);
    m_widthLength->setVisible(widthUnit == WidthLength);
    m_heightPercent->setVisible(heightUnit == HeightOfOriginal);
    m_heightLength->setVisible(heightUnit == HeightLength);
    // A percent of the text up to the whole of it, of the original up to the longest length
    m_widthPercent->setMaximum(widthUnit == WidthOfText ? editor::MAX_IMAGE_PERCENT : 1000.0);
    if (widthUnit == WidthOfText) {
        m_widthPercent->setValue(m_data.lineWidth > 0.0 ? size.width() / m_data.lineWidth * 100.0 : 100.0);
    } else if (widthUnit == WidthOfOriginal) {
        m_widthPercent->setValue(hasOriginal ? size.width() / original.width() * 100.0 : 100.0);
    }
    m_widthLength->setStoredValue(size.width() / POINTS_PER_CENTIMETER);
    m_heightPercent->setValue(hasOriginal ? size.height() / original.height() * 100.0 : 100.0);
    m_heightLength->setStoredValue(size.height() / POINTS_PER_CENTIMETER);

    // The units of the original need its file
    for (QComboBox* combo : {m_widthUnit, m_heightUnit}) {
        const int item = combo == m_widthUnit ? static_cast<int>(WidthOfOriginal)
                                              : static_cast<int>(HeightOfOriginal);
        combo->setItemData(item, hasOriginal ? QVariant() : QVariant(0), Qt::UserRole - 1);
    }
    m_originalButton->setEnabled(hasOriginal &&
                                 (m_image.unit != ImageWidthUnit::Natural || m_image.height > 0.0));
    if (hasOriginal) {
        const double byWidth = size.width() / original.width() * 100.0;
        const double byHeight = size.height() / original.height() * 100.0;
        //: The size of an image against its file's own. In Polish: Skala: %1% oryginału
        m_scale->setText(std::abs(byWidth - byHeight) < 0.5
                             ? tr("Scale: %1% of the original").arg(QLocale().toString(qRound(byWidth)))
                             //: The width and the height of a stretched image against its file's.
                             //: In Polish: Skala: %1% × %2% oryginału
                             : tr("Scale: %1% × %2% of the original")
                                   .arg(QLocale().toString(qRound(byWidth)),
                                        QLocale().toString(qRound(byHeight))));
        const QSize pixels = editor::shownPixels(m_data.filePixels, m_image);
        //: The file's own size of an image: its pixels and their length at 96 an inch; an
        //: image is never wider than its line. In Polish: %1 × %2 pikseli = %3 × %4 przy
        //: 96 dpi; w tekście nie szerzej niż wiersz
        m_originalInfo->setText(tr("%1 × %2 pixels = %3 × %4 at 96 dpi; in the text at most "
                                   "its width")
                                    .arg(pixels.width())
                                    .arg(pixels.height())
                                    .arg(lengthText(original.width() / POINTS_PER_CENTIMETER,
                                                    LengthUnit::Centimeters),
                                         lengthText(original.height() / POINTS_PER_CENTIMETER,
                                                    LengthUnit::Centimeters)));
    } else {
        m_scale->clear();
        m_originalInfo->setText(tr("Image file missing"));
    }
    m_angle->setCurrentIndex(m_image.rotation);
    m_flipHorizontal->setChecked(m_image.flipHorizontal);
    m_flipVertical->setChecked(m_image.flipVertical);
}

void ImagePropertiesDialog::refreshCropPage()
{
    const ImageCrop crop = editor::shownCrop(m_image);
    const double sides[] = {crop.left, crop.right, crop.top, crop.bottom};
    for (size_t side = 0; side < m_cropSides.size(); ++side) {
        m_cropSides[side]->setValue(sides[side]);
    }
    m_removeCrop->setEnabled(!m_image.crop.isEmpty());
    // The marks in the text are of the image's own file
    m_cropInText->setEnabled(m_data.newFile.isEmpty() && !m_picture.isNull());
}

void ImagePropertiesDialog::refreshBorderPage()
{
    m_line->setCurrentIndex(static_cast<int>(m_image.borderLine));
    const int width = m_lineWidth->findData(m_image.borderWidth);
    if (width >= 0) {
        m_lineWidth->setCurrentIndex(width);
    }
    m_padding->setStoredValue(m_image.borderPadding);
    m_shadow->setChecked(m_image.shadow);
    const bool line = m_image.borderLine != ImageBorderLine::None;
    m_lineWidth->setEnabled(line);
    m_colorButton->setEnabled(line || m_image.shadow);
    m_padding->setEnabled(m_image.hasFrame());

    const int side = m_colorButton->fontMetrics().height();
    const QColor border = palette().color(QPalette::Mid);
    if (m_image.borderColor.isValid()) {
        m_colorButton->setIcon(swatchIcon(m_image.borderColor, border, side));
        const QString name = paletteColorName(colorPalettes(), m_image.borderColor);
        m_colorButton->setText(name.isEmpty() ? m_image.borderColor.name().toUpper() : name);
    } else {
        m_colorButton->setIcon(swatchIcon(m_automaticColor, border, side));
        //: The color of an image's border: the color of the text. In Polish: Automatyczna
        m_colorButton->setText(tr("Automatic"));
    }
}

void ImagePropertiesDialog::refreshColorsPage()
{
    // Each tile a picture of its filter, with the corrections as they are
    for (QAbstractButton* button : m_filters->buttons()) {
        const int filter = m_filters->id(button);
        button->setChecked(filter == static_cast<int>(m_image.filter));
        if (m_picture.isNull()) {
            continue;
        }
        ImageProperties look = m_image;
        look.filter = static_cast<ImageFilter>(filter);
        look.crop = m_image.crop;
        const QSize pixels = QSizeF(editor::shownPixels(m_picture.size(), look))
                                 .scaled(QSizeF(FILTER_TILE), Qt::KeepAspectRatio)
                                 .toSize()
                                 .expandedTo(QSize(1, 1));
        button->setIcon(QIcon(QPixmap::fromImage(editor::shownPicture(m_picture, look, pixels))));
    }
    m_brightness->setValue(m_image.brightness);
    m_contrast->setValue(m_image.contrast);
    m_threshold->setValue(m_image.threshold);
    m_transparency->setValue(m_image.transparency);
    m_threshold->setEnabled(m_image.filter == ImageFilter::BlackWhite);
    m_sliderValues[0]->setText(signedPercent(m_image.brightness));
    m_sliderValues[1]->setText(signedPercent(m_image.contrast));
    m_sliderValues[2]->setText(QLocale().toString(m_image.threshold) + QStringLiteral("%"));
    m_sliderValues[3]->setText(QLocale().toString(m_image.transparency) + QStringLiteral("%"));
}

void ImagePropertiesDialog::refreshCaptionPage()
{
    const bool updating = m_updating;
    m_updating = true;
    const bool shown = m_caption.shown;
    const bool numbered = m_caption.numbered;
    m_captionShown->setChecked(shown);
    QStringList listed;
    for (int i = 0; i < m_captionLabel->count(); ++i) {
        listed << m_captionLabel->itemText(i);
    }
    if (listed != m_captionLabels) {
        m_captionLabel->clear();
        m_captionLabel->addItems(m_captionLabels);
    }
    m_captionLabel->setCurrentIndex(
        std::max(0, static_cast<int>(m_captionLabels.indexOf(m_caption.label))));
    if (m_captionText->text() != m_caption.text) {
        m_captionText->setText(m_caption.text);
    }
    (m_caption.above ? m_captionAbove : m_captionBelow)->setChecked(true);
    m_captionUnnumbered->setChecked(!numbered);

    m_captionLabel->setEnabled(shown && numbered);
    m_newCaptionLabel->setEnabled(shown && numbered);
    m_captionText->setEnabled(shown);
    m_captionBelow->setEnabled(shown);
    m_captionAbove->setEnabled(shown);
    m_captionUnnumbered->setEnabled(shown);

    // The number the program gives: of the label chosen
    const QString label = m_caption.label;
    QString number = m_captionNumberOf && !label.isEmpty() ? m_captionNumberOf(label) : QString();
    if (number.isEmpty() && label == m_data.caption.label) {
        number = m_data.captionNumber;
    }
    //: Beside the label of a caption. In Polish: numer nadaje program: %1
    m_captionNumber->setText(shown && numbered && !number.isEmpty()
                                 ? tr("the program gives the number: %1").arg(number)
                                 : QString());

    // The caption as the text shows it, its number on gray
    QString caption = m_caption.text.trimmed().toHtmlEscaped();
    if (numbered && !label.isEmpty()) {
        const QString field = QStringLiteral("<span style=\"background-color:%1\">&nbsp;%2&nbsp;</span>")
                                  .arg(palette().color(QPalette::Midlight).name(),
                                       (number.isEmpty() ? QStringLiteral("?") : number).toHtmlEscaped());
        caption = label.toHtmlEscaped() + QStringLiteral(" ") + field +
                  (caption.isEmpty() ? QStringLiteral(".") : QStringLiteral(". ") + caption);
    }
    const bool visible = shown && !caption.isEmpty();
    m_captionPreviewAbove->setText(visible && m_caption.above ? caption : QString());
    m_captionPreviewBelow->setText(visible && !m_caption.above ? caption : QString());
    m_updating = updating;
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
