/// @file text_images.h
/// @brief Images in the text: a picture that is a character of its line
///
/// An image is the object replacement character (U+FFFC) of the document with a character
/// format of the object type IMAGE_OBJECT. The format keeps the image's file (a path
/// relative to the book's main folder, "images/map.png"), its alternative text and its
/// width; the picture itself stays in the file. The image is a part of its line, as a large
/// character: it stands on the baseline, the paragraph's alignment places it, and the
/// document's undo, the clipboard and drag and drop take it with the text around it. An
/// image is not a word.
///
/// The width is the file's own (its pixels at 96 per inch), a percentage of the width of
/// the text, or a length in centimeters; the height follows in proportion. An image wider
/// than its line is shown as wide as the line, one higher than a page as high as a page.
///
/// KML writes an image as <img src="images/map.png" alt="Map" width="60%"/> in its
/// paragraph (docs/kml_format.md).

#pragma once

#include <QString>
#include <QTextFormat>

#include <optional>
#include <utility>
#include <vector>

class QTextBlock;
class QTextCharFormat;
class QTextDocument;
class QXmlStreamAttributes;

namespace kalahari::editor {

/// @brief The object type of an image's character
inline constexpr int IMAGE_OBJECT = QTextFormat::UserObject + 4;

/// @brief The image's file, relative to the book's main folder (QString)
inline constexpr int IMAGE_SOURCE_PROPERTY = QTextFormat::UserProperty + 170;

/// @brief The text a screen reader reads instead of the image (QString)
inline constexpr int IMAGE_ALT_PROPERTY = QTextFormat::UserProperty + 171;

/// @brief The unit of the image's width (int, ImageWidthUnit); without it: the file's own
inline constexpr int IMAGE_WIDTH_UNIT_PROPERTY = QTextFormat::UserProperty + 172;

/// @brief The image's width in its unit (double)
inline constexpr int IMAGE_WIDTH_PROPERTY = QTextFormat::UserProperty + 173;

/// @brief The folder of a book's images, in its main folder
inline constexpr const char* IMAGES_FOLDER = "images";

/// @brief The style of the caption a paragraph after an image is
inline constexpr const char* CAPTION_STYLE_ID = "caption";

/// @brief The narrowest width of an image (percent of the text, or centimeters)
inline constexpr double MIN_IMAGE_PERCENT = 1.0;
inline constexpr double MAX_IMAGE_PERCENT = 100.0;
inline constexpr double MIN_IMAGE_CENTIMETERS = 0.1;
inline constexpr double MAX_IMAGE_CENTIMETERS = 100.0;

/// @brief The pixels of an image's file in an inch of the text: its own size
inline constexpr double IMAGE_PIXELS_PER_INCH = 96.0;

/// @brief How the width of an image is given
enum class ImageWidthUnit {
    Natural,      ///< The file's own width (its pixels at IMAGE_PIXELS_PER_INCH)
    Percent,      ///< A percentage of the width of the text (of the cell, in a table)
    Centimeters,  ///< A length
};

/// @brief What the format of an image's character keeps
struct ImageProperties {
    QString source;  ///< Its file, relative to the book's main folder ("images/map.png")
    QString alt;     ///< Its alternative text
    ImageWidthUnit unit = ImageWidthUnit::Natural;
    double width = 0.0;  ///< In its unit (Natural: none)

    bool operator==(const ImageProperties&) const = default;
};

/// @brief Whether the format is of an image's character
bool isImageFormat(const QTextFormat& format);

/// @brief The image of a character with this format; std::nullopt: not an image
std::optional<ImageProperties> imageOf(const QTextFormat& format);

/// @brief Make the format one of an image's character with these properties (the other
///        properties of the format, its link or annotations, stay)
void setImage(QTextCharFormat& format, const ImageProperties& image);

/// @brief Take the image off the format: its object type and properties (the format of
///        text typed after an image)
void clearImage(QTextCharFormat& format);

/// @brief The format of a new image's character
QTextCharFormat imageCharFormat(const ImageProperties& image);

/// @brief The image at a position of the document: its character there is one
std::optional<ImageProperties> imageAt(const QTextDocument& document, int position);

/// @brief The positions of the images in [from, to) of the document, in text order
std::vector<int> imagePositions(const QTextDocument& document, int from, int to);

/// @brief Whether a block holds an image
bool hasImages(const QTextBlock& block);

/// @brief The width clamped to what its unit allows (a width of 0 or less: the file's own)
ImageProperties normalizedImage(const ImageProperties& image);

// =============================================================================
// Files
// =============================================================================

/// @brief Whether the source names a file in the book's main folder: a relative path
///        without "..", a drive or a scheme ("images/map.png"); anything else is shown as a
///        missing file and is never read or copied
bool isSafeImageSource(const QString& source);

/// @brief Whether the file's name is of a kind the editor shows: PNG, JPEG, GIF, BMP or
///        SVG
bool isImageFileName(const QString& fileName);

/// @brief The file name filter of the system's window that opens an image ("Images
///        (*.png *.jpg ...)")
QString imageFileFilter(const QString& imagesTitle);

// =============================================================================
// The properties in KML
// =============================================================================

/// @brief The attributes of <img>: src, alt (when it has one) and width ("60%",
///        "4.5cm"; none: the file's own)
std::vector<std::pair<QString, QString>> imageKmlAttributes(const ImageProperties& image);

/// @brief The whole element of an image: <img src="..." alt="..." width="..."/>
QString imageKml(const ImageProperties& image);

/// @brief The properties of an image from the attributes of its <img>: a width in percent,
///        or a length of KML (cm, mm, in, pt, px, pc) in centimeters; one that is not known
///        gives the file's own
ImageProperties imagePropertiesOfKml(const QXmlStreamAttributes& attributes);

}  // namespace kalahari::editor
