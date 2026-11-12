/// @file text_images.cpp
/// @brief Images in the text: a picture that is a character of its line

#include "kalahari/editor/text_images.h"
#include "kalahari/editor/kml_format_registry.h"
#include "kalahari/editor/text_tables.h"

#include <QFileInfo>
#include <QStringList>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QXmlStreamAttributes>

#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

/// The image properties of a character's format
constexpr int IMAGE_PROPERTIES[] = {IMAGE_SOURCE_PROPERTY, IMAGE_ALT_PROPERTY,
                                    IMAGE_WIDTH_UNIT_PROPERTY, IMAGE_WIDTH_PROPERTY};

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

}  // namespace

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
    return normalized;
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
    return normalizedImage(image);
}

}  // namespace kalahari::editor
