// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file text_images.cpp
/// @brief Images in the text: a picture that is a character of its line

#include "kalahari/editor/text_images.h"
#include "kalahari/editor/kml_format_registry.h"
#include "kalahari/editor/text_tables.h"

#include <QFileInfo>
#include <QImage>
#include <QTransform>
#include <QStringList>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

namespace kalahari::editor {

namespace {

/// The image properties of a character's format
constexpr int IMAGE_PROPERTIES[] = {
    IMAGE_SOURCE_PROPERTY,          IMAGE_ALT_PROPERTY,           IMAGE_WIDTH_UNIT_PROPERTY,
    IMAGE_WIDTH_PROPERTY,           IMAGE_HEIGHT_PROPERTY,        IMAGE_NAME_PROPERTY,
    IMAGE_DESCRIPTION_PROPERTY,     IMAGE_DECORATIVE_PROPERTY,    IMAGE_ROTATION_PROPERTY,
    IMAGE_FLIP_HORIZONTAL_PROPERTY, IMAGE_FLIP_VERTICAL_PROPERTY, IMAGE_CROP_LEFT_PROPERTY,
    IMAGE_CROP_TOP_PROPERTY,        IMAGE_CROP_RIGHT_PROPERTY,    IMAGE_CROP_BOTTOM_PROPERTY,
    IMAGE_FILTER_PROPERTY,          IMAGE_BRIGHTNESS_PROPERTY,    IMAGE_CONTRAST_PROPERTY,
    IMAGE_THRESHOLD_PROPERTY,       IMAGE_TRANSPARENCY_PROPERTY,  IMAGE_BORDER_LINE_PROPERTY,
    IMAGE_BORDER_WIDTH_PROPERTY,    IMAGE_BORDER_COLOR_PROPERTY,  IMAGE_BORDER_PADDING_PROPERTY,
    IMAGE_SHADOW_PROPERTY};

/// The names of the filters and border lines in KML, in the order of their enums
constexpr const char* FILTER_NAMES[] = {"", "grayscale", "sepia", "black-white", "wash"};
constexpr const char* BORDER_NAMES[] = {"", "solid", "dashed", "dotted", "double"};

/// The degrees of a quarter turn
constexpr int QUARTER_TURN = 90;

/// The filter Wash: how much of the colors goes to gray, how much of the dark is kept
constexpr double WASH_GRAY = 0.3;
constexpr double WASH_DEPTH = 0.3;

/// The points of a centimeter
constexpr double POINTS_PER_CENTIMETER = 72.0 / 2.54;

/// The extensions of the files the editor shows
constexpr const char* IMAGE_EXTENSIONS[] = {"png", "jpg", "jpeg", "gif", "bmp", "svg"};

/// A number of KML: at most two decimals, without trailing zeros
QString kmlNumber(double value)
{
    return QString::number(std::round(value * 100.0) / 100.0, 'g', 8);
}

/// Text an attribute of XML can hold: each control character (a line break, a tab) a space
QString withoutControls(const QString& text)
{
    QString kept = text;
    for (QChar& ch : kept) {
        if (ch.unicode() < 0x20 || ch.unicode() == 0x7F || ch == QChar::LineSeparator ||
            ch == QChar::ParagraphSeparator || ch == QChar::ObjectReplacementCharacter ||
            ch.unicode() == 0xFFFE || ch.unicode() == 0xFFFF) {
            ch = QLatin1Char(' ');
        }
    }
    return kept;
}

/// A flag of KML: "true" or "1", in any letter case
bool kmlFlag(const QXmlStreamAttributes& attributes, const char* name)
{
    const QString value = attributes.value(QLatin1String(name)).toString().trimmed().toLower();
    return value == QStringLiteral("true") || value == QStringLiteral("1");
}

/// A whole number of KML, with or without "%"; std::nullopt when it is none
std::optional<int> kmlPercent(const QXmlStreamAttributes& attributes, const char* name)
{
    QString value = attributes.value(QLatin1String(name)).toString().trimmed();
    if (value.endsWith(QLatin1Char('%'))) {
        value.chop(1);
    }
    bool ok = false;
    const double number = value.trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(number)) {
        return std::nullopt;
    }
    return static_cast<int>(std::lround(std::clamp(number, -1000.0, 1000.0)));
}

/// The index of a name of KML among the names of an enum (0: none of them)
template <size_t N>
int kmlName(const QXmlStreamAttributes& attributes, const char* name, const char* const (&names)[N])
{
    const QString value = attributes.value(QLatin1String(name)).toString().trimmed().toLower();
    for (size_t i = 1; i < N; ++i) {
        if (value == QLatin1String(names[i])) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

/// The crop of KML: one to four percentages as CSS gives the sides of a box (top, right,
/// bottom, left; "5%" all four, "5% 10%" top and bottom, then left and right)
ImageCrop kmlCrop(const QString& text)
{
    QList<double> values;
    for (QString part : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (part.endsWith(QLatin1Char('%'))) {
            part.chop(1);
        }
        bool ok = false;
        const double value = part.toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            return ImageCrop{};
        }
        values << value;
    }
    if (values.isEmpty() || values.size() > 4) {
        return ImageCrop{};
    }
    ImageCrop crop;
    crop.top = values.at(0);
    crop.right = values.size() > 1 ? values.at(1) : crop.top;
    crop.bottom = values.size() > 2 ? values.at(2) : crop.top;
    crop.left = values.size() > 3 ? values.at(3) : crop.right;
    return crop;
}

/// Clamp a crop: each side 0 or more, both of a direction together at most MAX_IMAGE_CROP
ImageCrop normalizedCrop(const ImageCrop& crop)
{
    const auto side = [](double value) {
        return std::isfinite(value) ? std::clamp(value, 0.0, MAX_IMAGE_CROP) : 0.0;
    };
    ImageCrop clamped{side(crop.left), side(crop.top), side(crop.right), side(crop.bottom)};
    const auto pair = [](double& first, double& second) {
        const double both = first + second;
        if (both > MAX_IMAGE_CROP) {
            first *= MAX_IMAGE_CROP / both;
            second *= MAX_IMAGE_CROP / both;
        }
    };
    pair(clamped.left, clamped.right);
    pair(clamped.top, clamped.bottom);
    // Two decimals, as KML writes them
    for (double* value : {&clamped.left, &clamped.top, &clamped.right, &clamped.bottom}) {
        *value = std::round(*value * 100.0) / 100.0;
    }
    return clamped;
}

/// A channel of a pixel (0 to 1) clamped
double channel(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

}  // namespace

bool ImageProperties::hasPlainPicture() const
{
    return rotation == 0 && !flipHorizontal && !flipVertical && crop.isEmpty() &&
           filter == ImageFilter::None && brightness == 0 && contrast == 0 && transparency == 0;
}

bool isImageFormat(const QTextFormat& format)
{
    return format.objectType() == IMAGE_OBJECT;
}

std::optional<ImageProperties> imageOf(const QTextFormat& format)
{
    if (!isImageFormat(format)) {
        return std::nullopt;
    }
    ImageProperties image;
    image.source = format.stringProperty(IMAGE_SOURCE_PROPERTY);
    image.alt = format.stringProperty(IMAGE_ALT_PROPERTY);
    const int unit = format.intProperty(IMAGE_WIDTH_UNIT_PROPERTY);
    if (unit == static_cast<int>(ImageWidthUnit::Percent) ||
        unit == static_cast<int>(ImageWidthUnit::Centimeters)) {
        image.unit = static_cast<ImageWidthUnit>(unit);
        image.width = format.doubleProperty(IMAGE_WIDTH_PROPERTY);
    }
    image.height = format.doubleProperty(IMAGE_HEIGHT_PROPERTY);
    image.name = format.stringProperty(IMAGE_NAME_PROPERTY);
    image.description = format.stringProperty(IMAGE_DESCRIPTION_PROPERTY);
    image.decorative = format.boolProperty(IMAGE_DECORATIVE_PROPERTY);
    image.rotation = format.intProperty(IMAGE_ROTATION_PROPERTY);
    image.flipHorizontal = format.boolProperty(IMAGE_FLIP_HORIZONTAL_PROPERTY);
    image.flipVertical = format.boolProperty(IMAGE_FLIP_VERTICAL_PROPERTY);
    image.crop = ImageCrop{format.doubleProperty(IMAGE_CROP_LEFT_PROPERTY),
                           format.doubleProperty(IMAGE_CROP_TOP_PROPERTY),
                           format.doubleProperty(IMAGE_CROP_RIGHT_PROPERTY),
                           format.doubleProperty(IMAGE_CROP_BOTTOM_PROPERTY)};
    image.filter = static_cast<ImageFilter>(format.intProperty(IMAGE_FILTER_PROPERTY));
    image.brightness = format.intProperty(IMAGE_BRIGHTNESS_PROPERTY);
    image.contrast = format.intProperty(IMAGE_CONTRAST_PROPERTY);
    if (format.hasProperty(IMAGE_THRESHOLD_PROPERTY)) {
        image.threshold = format.intProperty(IMAGE_THRESHOLD_PROPERTY);
    }
    image.transparency = format.intProperty(IMAGE_TRANSPARENCY_PROPERTY);
    image.borderLine = static_cast<ImageBorderLine>(format.intProperty(IMAGE_BORDER_LINE_PROPERTY));
    if (format.hasProperty(IMAGE_BORDER_WIDTH_PROPERTY)) {
        image.borderWidth = format.doubleProperty(IMAGE_BORDER_WIDTH_PROPERTY);
    }
    if (format.hasProperty(IMAGE_BORDER_COLOR_PROPERTY)) {
        image.borderColor = format.colorProperty(IMAGE_BORDER_COLOR_PROPERTY);
    }
    image.borderPadding = format.doubleProperty(IMAGE_BORDER_PADDING_PROPERTY);
    image.shadow = format.boolProperty(IMAGE_SHADOW_PROPERTY);
    return normalizedImage(image);
}

void setImage(QTextCharFormat& format, const ImageProperties& image)
{
    const ImageProperties normalized = normalizedImage(image);
    format.setObjectType(IMAGE_OBJECT);
    format.setProperty(IMAGE_SOURCE_PROPERTY, normalized.source);
    if (normalized.alt.isEmpty()) {
        format.clearProperty(IMAGE_ALT_PROPERTY);
    } else {
        format.setProperty(IMAGE_ALT_PROPERTY, normalized.alt);
    }
    if (normalized.unit == ImageWidthUnit::Natural) {
        format.clearProperty(IMAGE_WIDTH_UNIT_PROPERTY);
        format.clearProperty(IMAGE_WIDTH_PROPERTY);
    } else {
        format.setProperty(IMAGE_WIDTH_UNIT_PROPERTY, static_cast<int>(normalized.unit));
        format.setProperty(IMAGE_WIDTH_PROPERTY, normalized.width);
    }
    // The rest only where it is not the default: formats of the same image compare equal
    const auto set = [&format](int property, const QVariant& value, bool isDefault) {
        if (isDefault) {
            format.clearProperty(property);
        } else {
            format.setProperty(property, value);
        }
    };
    const ImageProperties plain;
    set(IMAGE_HEIGHT_PROPERTY, normalized.height, normalized.height <= 0.0);
    set(IMAGE_NAME_PROPERTY, normalized.name, normalized.name.isEmpty());
    set(IMAGE_DESCRIPTION_PROPERTY, normalized.description, normalized.description.isEmpty());
    set(IMAGE_DECORATIVE_PROPERTY, true, !normalized.decorative);
    set(IMAGE_ROTATION_PROPERTY, normalized.rotation, normalized.rotation == 0);
    set(IMAGE_FLIP_HORIZONTAL_PROPERTY, true, !normalized.flipHorizontal);
    set(IMAGE_FLIP_VERTICAL_PROPERTY, true, !normalized.flipVertical);
    set(IMAGE_CROP_LEFT_PROPERTY, normalized.crop.left, normalized.crop.left <= 0.0);
    set(IMAGE_CROP_TOP_PROPERTY, normalized.crop.top, normalized.crop.top <= 0.0);
    set(IMAGE_CROP_RIGHT_PROPERTY, normalized.crop.right, normalized.crop.right <= 0.0);
    set(IMAGE_CROP_BOTTOM_PROPERTY, normalized.crop.bottom, normalized.crop.bottom <= 0.0);
    set(IMAGE_FILTER_PROPERTY, static_cast<int>(normalized.filter),
        normalized.filter == ImageFilter::None);
    set(IMAGE_BRIGHTNESS_PROPERTY, normalized.brightness, normalized.brightness == 0);
    set(IMAGE_CONTRAST_PROPERTY, normalized.contrast, normalized.contrast == 0);
    set(IMAGE_THRESHOLD_PROPERTY, normalized.threshold, normalized.threshold == plain.threshold);
    set(IMAGE_TRANSPARENCY_PROPERTY, normalized.transparency, normalized.transparency == 0);
    set(IMAGE_BORDER_LINE_PROPERTY, static_cast<int>(normalized.borderLine),
        normalized.borderLine == ImageBorderLine::None);
    set(IMAGE_BORDER_WIDTH_PROPERTY, normalized.borderWidth,
        normalized.borderWidth == plain.borderWidth);
    set(IMAGE_BORDER_COLOR_PROPERTY, normalized.borderColor, !normalized.borderColor.isValid());
    set(IMAGE_BORDER_PADDING_PROPERTY, normalized.borderPadding, normalized.borderPadding <= 0.0);
    set(IMAGE_SHADOW_PROPERTY, true, !normalized.shadow);
}

void clearImage(QTextCharFormat& format)
{
    if (isImageFormat(format)) {
        format.clearProperty(QTextFormat::ObjectType);
    }
    for (const int property : IMAGE_PROPERTIES) {
        format.clearProperty(property);
    }
}

QTextCharFormat imageCharFormat(const ImageProperties& image)
{
    QTextCharFormat format;
    setImage(format, image);
    return format;
}

std::optional<ImageProperties> imageAt(const QTextDocument& document, int position)
{
    if (position < 0 || position >= document.characterCount() - 1 ||
        document.characterAt(position) != QChar::ObjectReplacementCharacter) {
        return std::nullopt;
    }
    // The format of the character before the cursor's position, here the image's one
    QTextCursor cursor(const_cast<QTextDocument*>(&document));
    cursor.setPosition(position + 1);
    return imageOf(cursor.charFormat());
}

std::vector<int> imagePositions(const QTextDocument& document, int from, int to)
{
    std::vector<int> positions;
    for (QTextBlock block = document.findBlock(std::max(0, from));
         block.isValid() && block.position() < to; block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid() || !isImageFormat(fragment.charFormat())) {
                continue;
            }
            const QString text = fragment.text();
            for (qsizetype i = 0; i < text.size(); ++i) {
                const int position = fragment.position() + static_cast<int>(i);
                if (text.at(i) == QChar::ObjectReplacementCharacter && position >= from &&
                    position < to) {
                    positions.push_back(position);
                }
            }
        }
    }
    return positions;
}

bool hasImages(const QTextBlock& block)
{
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        if (it.fragment().isValid() && isImageFormat(it.fragment().charFormat())) {
            return true;
        }
    }
    return false;
}

ImageProperties normalizedImage(const ImageProperties& image)
{
    ImageProperties normalized = image;
    normalized.source = withoutControls(image.source).trimmed();
    normalized.alt = withoutControls(image.alt);
    if (normalized.unit == ImageWidthUnit::Natural || !(normalized.width > 0.0)) {
        normalized.unit = ImageWidthUnit::Natural;
        normalized.width = 0.0;
    } else if (normalized.unit == ImageWidthUnit::Percent) {
        normalized.width = std::clamp(normalized.width, MIN_IMAGE_PERCENT, MAX_IMAGE_PERCENT);
    } else {
        normalized.width =
            std::clamp(normalized.width, MIN_IMAGE_CENTIMETERS, MAX_IMAGE_CENTIMETERS);
    }
    normalized.height = std::isfinite(normalized.height) && normalized.height > 0.0
                            ? std::clamp(normalized.height, MIN_IMAGE_CENTIMETERS,
                                         MAX_IMAGE_CENTIMETERS)
                            : 0.0;
    normalized.name = withoutControls(image.name).trimmed();
    normalized.description = withoutControls(image.description).trimmed();
    normalized.rotation = ((image.rotation % 4) + 4) % 4;
    normalized.crop = normalizedCrop(image.crop);
    const int filter = static_cast<int>(image.filter);
    normalized.filter = filter >= 0 && filter < static_cast<int>(std::size(FILTER_NAMES))
                            ? image.filter
                            : ImageFilter::None;
    normalized.brightness = std::clamp(image.brightness, MIN_IMAGE_CORRECTION, MAX_IMAGE_CORRECTION);
    normalized.contrast = std::clamp(image.contrast, MIN_IMAGE_CORRECTION, MAX_IMAGE_CORRECTION);
    normalized.threshold = std::clamp(image.threshold, MIN_IMAGE_THRESHOLD, MAX_IMAGE_THRESHOLD);
    normalized.transparency = std::clamp(image.transparency, 0, MAX_IMAGE_TRANSPARENCY);
    const int line = static_cast<int>(image.borderLine);
    normalized.borderLine = line >= 0 && line < static_cast<int>(std::size(BORDER_NAMES))
                                ? image.borderLine
                                : ImageBorderLine::None;
    normalized.borderWidth =
        std::isfinite(image.borderWidth)
            ? std::round(std::clamp(image.borderWidth, MIN_IMAGE_BORDER_WIDTH,
                                    MAX_IMAGE_BORDER_WIDTH) * 100.0) / 100.0
            : DEFAULT_IMAGE_BORDER_WIDTH;
    normalized.borderColor = image.borderColor.isValid() ? image.borderColor.toRgb() : QColor();
    if (normalized.borderColor.isValid()) {
        normalized.borderColor.setAlpha(255);
    }
    normalized.borderPadding =
        std::isfinite(image.borderPadding)
            ? std::round(std::clamp(image.borderPadding, 0.0, MAX_IMAGE_BORDER_PADDING) * 100.0) /
                  100.0
            : 0.0;
    return normalized;
}

QRect croppedRect(const QSize& filePixels, const ImageCrop& crop)
{
    const ImageCrop clamped = normalizedCrop(crop);
    const int left = static_cast<int>(std::lround(filePixels.width() * clamped.left / 100.0));
    const int right = static_cast<int>(std::lround(filePixels.width() * clamped.right / 100.0));
    const int top = static_cast<int>(std::lround(filePixels.height() * clamped.top / 100.0));
    const int bottom = static_cast<int>(std::lround(filePixels.height() * clamped.bottom / 100.0));
    return QRect(left, top, std::max(1, filePixels.width() - left - right),
                 std::max(1, filePixels.height() - top - bottom));
}

namespace {

/// The sides of a crop clockwise from the top: top, right, bottom, left
std::array<double, 4> sidesOf(const ImageCrop& crop)
{
    return {crop.top, crop.right, crop.bottom, crop.left};
}

ImageCrop cropOf(const std::array<double, 4>& sides)
{
    return ImageCrop{sides[3], sides[0], sides[1], sides[2]};
}

}  // namespace

ImageCrop shownCrop(const ImageProperties& image)
{
    std::array<double, 4> sides = sidesOf(image.crop);
    // Mirrored first, then turned clockwise: each side goes a quarter on
    if (image.flipHorizontal) {
        std::swap(sides[1], sides[3]);
    }
    if (image.flipVertical) {
        std::swap(sides[0], sides[2]);
    }
    std::array<double, 4> shown{};
    for (size_t side = 0; side < sides.size(); ++side) {
        shown[(side + static_cast<size_t>(image.rotation)) % 4] = sides[side];
    }
    return cropOf(shown);
}

ImageCrop fileCrop(const ImageCrop& shown, const ImageProperties& image)
{
    const std::array<double, 4> sides = sidesOf(shown);
    std::array<double, 4> file{};
    for (size_t side = 0; side < sides.size(); ++side) {
        file[side] = sides[(side + static_cast<size_t>(image.rotation)) % 4];
    }
    if (image.flipHorizontal) {
        std::swap(file[1], file[3]);
    }
    if (image.flipVertical) {
        std::swap(file[0], file[2]);
    }
    return cropOf(file);
}

ImageProperties withShownCrop(const ImageProperties& image, const ImageCrop& crop, bool keepScale,
                              double widthCentimeters)
{
    ImageProperties cropped = normalizedImage(image);
    const ImageCrop before = shownCrop(cropped);
    cropped.crop = normalizedCrop(fileCrop(crop, cropped));
    const ImageCrop after = shownCrop(cropped);
    // The part of the whole picture shown, across and down, after against before
    const double byWidth =
        (100.0 - after.left - after.right) / (100.0 - before.left - before.right);
    const double byHeight =
        (100.0 - after.top - after.bottom) / (100.0 - before.top - before.bottom);
    if (keepScale) {
        // A width of the file's own follows its cropped pixels by itself
        if (cropped.unit != ImageWidthUnit::Natural) {
            cropped.width *= byWidth;
        }
        if (cropped.height > 0.0) {
            cropped.height *= byHeight;
        }
    } else {
        if (cropped.unit == ImageWidthUnit::Natural && widthCentimeters > 0.0) {
            cropped.unit = ImageWidthUnit::Centimeters;
            cropped.width = widthCentimeters;
        }
        if (cropped.height > 0.0) {
            cropped.height *= byHeight / byWidth;
        }
    }
    return normalizedImage(cropped);
}

QSize shownPixels(const QSize& filePixels, const ImageProperties& image)
{
    const QSize cropped = croppedRect(filePixels, image.crop).size();
    return image.rotation % 2 == 1 ? cropped.transposed() : cropped;
}

QImage shownPicture(const QImage& picture, const ImageProperties& image, const QSize& pixels)
{
    if (picture.isNull() || pixels.isEmpty()) {
        return QImage();
    }
    QImage shown = image.crop.isEmpty() ? picture : picture.copy(croppedRect(picture.size(), image.crop));
    if (image.rotation != 0 || image.flipHorizontal || image.flipVertical) {
        // Mirrored first, as the file shows it, then turned
        QTransform transform;
        transform.rotate(QUARTER_TURN * image.rotation);
        transform.scale(image.flipHorizontal ? -1.0 : 1.0, image.flipVertical ? -1.0 : 1.0);
        shown = shown.transformed(transform);
    }
    if (shown.size() != pixels) {
        shown = shown.scaled(pixels, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    const bool recolored = image.filter != ImageFilter::None || image.brightness != 0 ||
                           image.contrast != 0;
    if (!recolored && image.transparency == 0) {
        return shown;
    }

    // The colors as CSS filters change them: the corrections first (a dark scan made
    // lighter before its threshold), then the filter, then the transparency
    shown = shown.convertToFormat(QImage::Format_ARGB32);
    const double brightness = 1.0 + image.brightness / 100.0;
    const double contrast = 1.0 + image.contrast / 100.0;
    const double threshold = image.threshold / 100.0;
    const double opacity = 1.0 - image.transparency / 100.0;
    for (int y = 0; y < shown.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(shown.scanLine(y));
        for (int x = 0; x < shown.width(); ++x) {
            const QRgb pixel = line[x];
            double r = qRed(pixel) / 255.0;
            double g = qGreen(pixel) / 255.0;
            double b = qBlue(pixel) / 255.0;
            if (recolored) {
                for (double* c : {&r, &g, &b}) {
                    *c = channel((*c * brightness - 0.5) * contrast + 0.5);
                }
                const double luma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
                switch (image.filter) {
                    case ImageFilter::None:
                        break;
                    case ImageFilter::Grayscale:
                        r = g = b = luma;
                        break;
                    case ImageFilter::Sepia: {
                        const double sr = 0.393 * r + 0.769 * g + 0.189 * b;
                        const double sg = 0.349 * r + 0.686 * g + 0.168 * b;
                        const double sb = 0.272 * r + 0.534 * g + 0.131 * b;
                        r = channel(sr);
                        g = channel(sg);
                        b = channel(sb);
                        break;
                    }
                    case ImageFilter::BlackWhite:
                        r = g = b = luma < threshold ? 0.0 : 1.0;
                        break;
                    case ImageFilter::Wash:
                        // A little gray, and most of the way to white: the paper stays
                        // white, the darkest parts become a light gray
                        for (double* c : {&r, &g, &b}) {
                            const double grayer = *c + (luma - *c) * WASH_GRAY;
                            *c = channel(1.0 - (1.0 - grayer) * WASH_DEPTH);
                        }
                        break;
                }
            }
            const int alpha = static_cast<int>(std::lround(qAlpha(pixel) * opacity));
            line[x] = qRgba(static_cast<int>(std::lround(r * 255.0)),
                            static_cast<int>(std::lround(g * 255.0)),
                            static_cast<int>(std::lround(b * 255.0)), alpha);
        }
    }
    return shown;
}

QString pictureLookKey(const ImageProperties& image)
{
    if (image.hasPlainPicture()) {
        return QString();
    }
    const ImageCrop& crop = image.crop;
    return QStringLiteral("%1|%2%3|%4,%5,%6,%7|%8,%9,%10,%11,%12")
        .arg(image.rotation)
        .arg(image.flipHorizontal ? 1 : 0)
        .arg(image.flipVertical ? 1 : 0)
        .arg(crop.left)
        .arg(crop.top)
        .arg(crop.right)
        .arg(crop.bottom)
        .arg(static_cast<int>(image.filter))
        .arg(image.brightness)
        .arg(image.contrast)
        .arg(image.filter == ImageFilter::BlackWhite ? image.threshold : 0)
        .arg(image.transparency);
}

QString imageFilterKml(ImageFilter filter)
{
    const int index = static_cast<int>(filter);
    return index > 0 && index < static_cast<int>(std::size(FILTER_NAMES))
               ? QLatin1String(FILTER_NAMES[index])
               : QString();
}

// =============================================================================
// Files
// =============================================================================

bool isSafeImageSource(const QString& source)
{
    if (source.isEmpty() || source.startsWith(QLatin1Char('/')) ||
        source.startsWith(QLatin1Char('\\')) || source.contains(QLatin1Char(':')) ||
        source.contains(QChar(0))) {
        return false;  // a path of the whole disk, a drive ("C:"), a scheme ("https:")
    }
    const QStringList parts = QString(source).replace(QLatin1Char('\\'), QLatin1Char('/'))
                                  .split(QLatin1Char('/'), Qt::SkipEmptyParts);
    return !parts.isEmpty() &&
           std::none_of(parts.cbegin(), parts.cend(),
                        [](const QString& part) { return part == QStringLiteral(".."); });
}

bool isImageFileName(const QString& fileName)
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    return std::any_of(std::begin(IMAGE_EXTENSIONS), std::end(IMAGE_EXTENSIONS),
                       [&suffix](const char* extension) { return suffix == QLatin1String(extension); });
}

QString imageFileFilter(const QString& imagesTitle)
{
    QStringList patterns;
    for (const char* extension : IMAGE_EXTENSIONS) {
        patterns << QStringLiteral("*.") + QLatin1String(extension);
    }
    return imagesTitle + QStringLiteral(" (") + patterns.join(QLatin1Char(' ')) +
           QLatin1Char(')');
}

// =============================================================================
// The properties in KML
// =============================================================================

std::vector<std::pair<QString, QString>> imageKmlAttributes(const ImageProperties& image)
{
    const ImageProperties normalized = normalizedImage(image);
    std::vector<std::pair<QString, QString>> attributes;
    attributes.emplace_back(QStringLiteral("src"), normalized.source);
    if (!normalized.alt.isEmpty()) {
        attributes.emplace_back(QStringLiteral("alt"), normalized.alt);
    }
    if (normalized.unit == ImageWidthUnit::Percent) {
        attributes.emplace_back(QStringLiteral("width"),
                                kmlNumber(normalized.width) + QLatin1Char('%'));
    } else if (normalized.unit == ImageWidthUnit::Centimeters) {
        attributes.emplace_back(QStringLiteral("width"),
                                kmlNumber(normalized.width) + QStringLiteral("cm"));
    }
    if (normalized.height > 0.0) {
        attributes.emplace_back(QStringLiteral("height"),
                                kmlNumber(normalized.height) + QStringLiteral("cm"));
    }
    const auto add = [&attributes](const char* name, const QString& value) {
        attributes.emplace_back(QLatin1String(name), value);
    };
    if (!normalized.name.isEmpty()) {
        add("name", normalized.name);
    }
    if (!normalized.description.isEmpty()) {
        add("description", normalized.description);
    }
    if (normalized.decorative) {
        add("decorative", QStringLiteral("true"));
    }
    if (normalized.rotation != 0) {
        add("rotate", QString::number(normalized.rotation * QUARTER_TURN));
    }
    if (normalized.flipHorizontal || normalized.flipVertical) {
        QStringList flips;
        if (normalized.flipHorizontal) {
            flips << QStringLiteral("horizontal");
        }
        if (normalized.flipVertical) {
            flips << QStringLiteral("vertical");
        }
        add("flip", flips.join(QLatin1Char(' ')));
    }
    if (!normalized.crop.isEmpty()) {
        const ImageCrop& crop = normalized.crop;
        QStringList sides;
        for (const double side : {crop.top, crop.right, crop.bottom, crop.left}) {
            sides << kmlNumber(side) + QLatin1Char('%');
        }
        add("crop", sides.join(QLatin1Char(' ')));
    }
    if (normalized.filter != ImageFilter::None) {
        add("filter", imageFilterKml(normalized.filter));
    }
    if (normalized.brightness != 0) {
        add("brightness", QString::number(normalized.brightness) + QLatin1Char('%'));
    }
    if (normalized.contrast != 0) {
        add("contrast", QString::number(normalized.contrast) + QLatin1Char('%'));
    }
    if (normalized.filter == ImageFilter::BlackWhite &&
        normalized.threshold != DEFAULT_IMAGE_THRESHOLD) {
        add("threshold", QString::number(normalized.threshold) + QLatin1Char('%'));
    }
    if (normalized.transparency != 0) {
        add("transparency", QString::number(normalized.transparency) + QLatin1Char('%'));
    }
    if (normalized.borderLine != ImageBorderLine::None) {
        add("border", QLatin1String(BORDER_NAMES[static_cast<int>(normalized.borderLine)]));
        if (normalized.borderWidth != DEFAULT_IMAGE_BORDER_WIDTH) {
            add("border-width", kmlLength(normalized.borderWidth));
        }
        if (normalized.borderColor.isValid()) {
            add("border-color", normalized.borderColor.name());
        }
    }
    if (normalized.hasFrame() && normalized.borderPadding > 0.0) {
        add("border-padding", kmlLength(normalized.borderPadding));
    }
    if (normalized.shadow) {
        add("shadow", QStringLiteral("true"));
    }
    return attributes;
}

QString imageKml(const ImageProperties& image)
{
    QString kml = QStringLiteral("<img");
    for (const auto& [name, value] : imageKmlAttributes(image)) {
        kml += QLatin1Char(' ') + name + QStringLiteral("=\"") +
               KmlFormatRegistry::escapeXml(value) + QLatin1Char('"');
    }
    return kml + QStringLiteral("/>");
}

ImageProperties imagePropertiesOfKml(const QXmlStreamAttributes& attributes)
{
    ImageProperties image;
    image.source = attributes.value(QStringLiteral("src")).toString().trimmed();
    image.alt = attributes.value(QStringLiteral("alt")).toString();
    const QString width = attributes.value(QStringLiteral("width")).toString().trimmed();
    if (width.endsWith(QLatin1Char('%'))) {
        bool ok = false;
        const double percent = width.chopped(1).trimmed().toDouble(&ok);
        if (ok && percent > 0.0) {
            image.unit = ImageWidthUnit::Percent;
            image.width = percent;
        }
    } else if (!width.isEmpty()) {
        if (const std::optional<double> points = pointsOfKmlLength(width); points && *points > 0.0) {
            image.unit = ImageWidthUnit::Centimeters;
            image.width = *points / POINTS_PER_CENTIMETER;
        }
    }
    const QString height = attributes.value(QStringLiteral("height")).toString().trimmed();
    if (const std::optional<double> points = pointsOfKmlLength(height); points && *points > 0.0) {
        image.height = *points / POINTS_PER_CENTIMETER;
    }
    image.name = attributes.value(QStringLiteral("name")).toString();
    image.description = attributes.value(QStringLiteral("description")).toString();
    image.decorative = kmlFlag(attributes, "decorative");
    if (const std::optional<int> degrees = kmlPercent(attributes, "rotate")) {
        // A quarter turn the nearest to the angle, as an image of the line turns
        image.rotation = static_cast<int>(std::lround(*degrees / double(QUARTER_TURN)));
    }
    const QStringList flips = attributes.value(QStringLiteral("flip")).toString().toLower().split(
        QLatin1Char(' '), Qt::SkipEmptyParts);
    image.flipHorizontal = flips.contains(QStringLiteral("horizontal"));
    image.flipVertical = flips.contains(QStringLiteral("vertical"));
    image.crop = kmlCrop(attributes.value(QStringLiteral("crop")).toString());
    image.filter = static_cast<ImageFilter>(kmlName(attributes, "filter", FILTER_NAMES));
    image.brightness = kmlPercent(attributes, "brightness").value_or(0);
    image.contrast = kmlPercent(attributes, "contrast").value_or(0);
    image.threshold = kmlPercent(attributes, "threshold").value_or(DEFAULT_IMAGE_THRESHOLD);
    image.transparency = kmlPercent(attributes, "transparency").value_or(0);
    image.borderLine = static_cast<ImageBorderLine>(kmlName(attributes, "border", BORDER_NAMES));
    const QString borderWidth = attributes.value(QStringLiteral("border-width")).toString();
    image.borderWidth = pointsOfKmlLength(borderWidth).value_or(DEFAULT_IMAGE_BORDER_WIDTH);
    const QString color = attributes.value(QStringLiteral("border-color")).toString().trimmed();
    image.borderColor = color.startsWith(QLatin1Char('#')) ? QColor(color) : QColor();
    const QString padding = attributes.value(QStringLiteral("border-padding")).toString();
    image.borderPadding = pointsOfKmlLength(padding).value_or(0.0);
    image.shadow = kmlFlag(attributes, "shadow");
    return normalizedImage(image);
}

// =============================================================================
// One filter for every image
// =============================================================================

ImageProperties withImageFilter(const ImageProperties& image, ImageFilter filter, int threshold)
{
    ImageProperties changed = image;
    changed.filter = filter;
    if (filter == ImageFilter::BlackWhite) {
        changed.threshold = threshold;
    }
    return normalizedImage(changed);
}

std::vector<int> imagesWithOtherFilter(const QTextDocument& document, ImageFilter filter,
                                       int threshold)
{
    std::vector<int> changed;
    for (const int position : imagePositions(document, 0, document.characterCount())) {
        const std::optional<ImageProperties> image = imageAt(document, position);
        if (image && withImageFilter(*image, filter, threshold) != normalizedImage(*image)) {
            changed.push_back(position);
        }
    }
    return changed;
}

QString withImageFilterKml(const QString& kml, ImageFilter filter, int threshold, int* count)
{
    int changed = 0;
    QString result;
    qsizetype done = 0;
    QXmlStreamReader reader(kml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.name() != u"img") {
            continue;
        }
        // The reader is past the start tag, and a text before it took its "<" along: the tag
        // begins at the last "<" (an attribute holds none)
        const qsizetype start = kml.lastIndexOf(QLatin1Char('<'), reader.characterOffset() - 1);
        const ImageProperties image = imagePropertiesOfKml(reader.attributes());
        reader.skipCurrentElement();
        const qsizetype end = reader.characterOffset();
        // The element where the reader says it is (an image without a file stays)
        if (image.source.isEmpty() || start < 0 || start < done || end <= start ||
            !QStringView(kml).mid(start).startsWith(u"<img")) {
            continue;
        }
        const ImageProperties filtered = withImageFilter(image, filter, threshold);
        if (filtered == image) {
            continue;
        }
        result += QStringView(kml).mid(done, start - done);
        result += imageKml(filtered);
        done = end;
        ++changed;
    }
    if (count != nullptr) {
        *count = changed;
    }
    if (changed == 0 || reader.hasError()) {
        if (count != nullptr && reader.hasError()) {
            *count = 0;
        }
        return kml;  // nothing to change, or a text that is not KML
    }
    result += QStringView(kml).mid(done);
    return result;
}

}  // namespace kalahari::editor
