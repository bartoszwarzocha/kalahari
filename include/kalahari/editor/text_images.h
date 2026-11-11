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

#include <QColor>
#include <QSize>
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

/// @brief The image's own height in centimeters (double); without it: in proportion
inline constexpr int IMAGE_HEIGHT_PROPERTY = QTextFormat::UserProperty + 174;

/// @brief The image's name (QString): in the Navigator, the cross-references and search
inline constexpr int IMAGE_NAME_PROPERTY = QTextFormat::UserProperty + 175;

/// @brief The image's long description for screen readers (QString)
inline constexpr int IMAGE_DESCRIPTION_PROPERTY = QTextFormat::UserProperty + 176;

/// @brief Whether the image is only decorative: screen readers skip it (bool)
inline constexpr int IMAGE_DECORATIVE_PROPERTY = QTextFormat::UserProperty + 177;

/// @brief The image's rotation clockwise, in quarter turns 0 to 3 (int)
inline constexpr int IMAGE_ROTATION_PROPERTY = QTextFormat::UserProperty + 178;

/// @brief Whether the image is mirrored left to right, top to bottom (bool)
inline constexpr int IMAGE_FLIP_HORIZONTAL_PROPERTY = QTextFormat::UserProperty + 179;
inline constexpr int IMAGE_FLIP_VERTICAL_PROPERTY = QTextFormat::UserProperty + 180;

/// @brief The cut off parts of the file's picture, in percent of its width or height
///        (double each)
inline constexpr int IMAGE_CROP_LEFT_PROPERTY = QTextFormat::UserProperty + 181;
inline constexpr int IMAGE_CROP_TOP_PROPERTY = QTextFormat::UserProperty + 182;
inline constexpr int IMAGE_CROP_RIGHT_PROPERTY = QTextFormat::UserProperty + 183;
inline constexpr int IMAGE_CROP_BOTTOM_PROPERTY = QTextFormat::UserProperty + 184;

/// @brief The image's filter (int, ImageFilter) and its corrections (int each, in percent)
inline constexpr int IMAGE_FILTER_PROPERTY = QTextFormat::UserProperty + 185;
inline constexpr int IMAGE_BRIGHTNESS_PROPERTY = QTextFormat::UserProperty + 186;
inline constexpr int IMAGE_CONTRAST_PROPERTY = QTextFormat::UserProperty + 187;
inline constexpr int IMAGE_THRESHOLD_PROPERTY = QTextFormat::UserProperty + 188;
inline constexpr int IMAGE_TRANSPARENCY_PROPERTY = QTextFormat::UserProperty + 189;

/// @brief The image's border: its line (int, ImageBorderLine), width in points (double),
///        color (QColor; without it: the color of the text), the room between the border
///        and the picture in points (double) and a shadow (bool)
inline constexpr int IMAGE_BORDER_LINE_PROPERTY = QTextFormat::UserProperty + 190;
inline constexpr int IMAGE_BORDER_WIDTH_PROPERTY = QTextFormat::UserProperty + 191;
inline constexpr int IMAGE_BORDER_COLOR_PROPERTY = QTextFormat::UserProperty + 192;
inline constexpr int IMAGE_BORDER_PADDING_PROPERTY = QTextFormat::UserProperty + 193;
inline constexpr int IMAGE_SHADOW_PROPERTY = QTextFormat::UserProperty + 194;

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

/// @brief The corrections of an image's colors: from MIN_IMAGE_CORRECTION to
///        MAX_IMAGE_CORRECTION percent (0: none)
inline constexpr int MIN_IMAGE_CORRECTION = -50;
inline constexpr int MAX_IMAGE_CORRECTION = 50;

/// @brief The share of gray that becomes black in the filter BlackWhite (percent)
inline constexpr int MIN_IMAGE_THRESHOLD = 20;
inline constexpr int MAX_IMAGE_THRESHOLD = 80;
inline constexpr int DEFAULT_IMAGE_THRESHOLD = 50;

/// @brief The most transparent an image is (percent)
inline constexpr int MAX_IMAGE_TRANSPARENCY = 90;

/// @brief The most of the file's picture that is cut off, from both sides of a direction
///        together (percent): something of the picture always stays
inline constexpr double MAX_IMAGE_CROP = 95.0;

/// @brief The widths of an image's border line (points)
inline constexpr double MIN_IMAGE_BORDER_WIDTH = 0.25;
inline constexpr double MAX_IMAGE_BORDER_WIDTH = 12.0;
inline constexpr double DEFAULT_IMAGE_BORDER_WIDTH = 0.5;

/// @brief The most room between an image's border and its picture (points)
inline constexpr double MAX_IMAGE_BORDER_PADDING = 72.0;

/// @brief How much Ctrl+] and Ctrl+[ make an image larger or smaller: a part of its width
inline constexpr double IMAGE_KEY_RESIZE = 0.05;

/// @brief How the width of an image is given
enum class ImageWidthUnit {
    Natural,      ///< The file's own width (its pixels at IMAGE_PIXELS_PER_INCH)
    Percent,      ///< A percentage of the width of the text (of the cell, in a table)
    Centimeters,  ///< A length
};

/// @brief The look an image's colors are given; the file stays as it is
enum class ImageFilter {
    None,        ///< Its own colors
    Grayscale,   ///< Shades of gray, as in black and white print
    Sepia,       ///< The brown tones of an old photograph
    BlackWhite,  ///< Black and white only, as a photocopy or a woodcut (at a threshold)
    Wash,        ///< Pale, to stand behind the text
};

/// @brief The line of an image's border
enum class ImageBorderLine {
    None,
    Solid,
    Dashed,
    Dotted,
    Double,
};

/// @brief The parts of the file's picture an image does not show, in percent of the
///        picture's width (left, right) or height (top, bottom)
struct ImageCrop {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;

    bool isEmpty() const { return left <= 0.0 && top <= 0.0 && right <= 0.0 && bottom <= 0.0; }
    bool operator==(const ImageCrop&) const = default;
};

/// @brief What the format of an image's character keeps
struct ImageProperties {
    QString source;  ///< Its file, relative to the book's main folder ("images/map.png")
    QString alt;     ///< Its alternative text
    ImageWidthUnit unit = ImageWidthUnit::Natural;
    double width = 0.0;   ///< In its unit (Natural: none)
    double height = 0.0;  ///< Its own height in centimeters; 0: in proportion to the width

    QString name;             ///< Its name (in the Navigator, the cross-references, search)
    QString description;      ///< A long description for screen readers
    bool decorative = false;  ///< Only an ornament: screen readers skip it

    int rotation = 0;  ///< Clockwise, in quarter turns (0 to 3)
    bool flipHorizontal = false;
    bool flipVertical = false;
    ImageCrop crop;

    ImageFilter filter = ImageFilter::None;
    int brightness = 0;    ///< Percent, MIN_IMAGE_CORRECTION to MAX_IMAGE_CORRECTION
    int contrast = 0;      ///< Percent, MIN_IMAGE_CORRECTION to MAX_IMAGE_CORRECTION
    int threshold = DEFAULT_IMAGE_THRESHOLD;  ///< Of the filter BlackWhite
    int transparency = 0;  ///< Percent, 0 to MAX_IMAGE_TRANSPARENCY

    ImageBorderLine borderLine = ImageBorderLine::None;
    double borderWidth = DEFAULT_IMAGE_BORDER_WIDTH;  ///< Points
    QColor borderColor;          ///< Invalid: the color of the text
    double borderPadding = 0.0;  ///< Points between the border and the picture
    bool shadow = false;

    /// @brief Whether the picture is drawn as the file has it: not cropped, turned, mirrored
    ///        or recolored
    bool hasPlainPicture() const;

    /// @brief Whether the image has a border or a shadow around its picture
    bool hasFrame() const { return borderLine != ImageBorderLine::None || shadow; }

    bool operator==(const ImageProperties&) const = default;
};

/// @brief What the window Image Properties shows of an image and gives back
struct ImagePropertiesData {
    int position = -1;      ///< The image's character in the document
    ImageProperties image;  ///< Its properties
    QString filePath;       ///< Its file on the disk; empty: none
    QSize filePixels;       ///< The pixels of its file; empty: missing, or not an image
    qint64 fileBytes = 0;   ///< The size of its file
    double lineWidth = 0.0;   ///< Points: the width of its lines, of which a percent is
    double shownWidth = 0.0;  ///< Points: the width of its picture as it is shown now
    QString newFile;          ///< A file chosen in the window for the image to show; empty: none
    bool keepSizeAndCrop = true;  ///< With a new file: its width, height and crop stay
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

/// @brief The properties clamped to what they allow (a width of 0 or less: the file's own,
///        a rotation to a quarter turn 0 to 3, the crops leaving some of the picture)
ImageProperties normalizedImage(const ImageProperties& image);

/// @brief The pixels of the file's picture an image shows: the crop taken off
QRect croppedRect(const QSize& filePixels, const ImageCrop& crop);

/// @brief The crop of an image as it is shown: its sides turned and mirrored with the
///        picture (a quarter turn makes the file's left side the top one)
ImageCrop shownCrop(const ImageProperties& image);

/// @brief The crop of the file's sides that shows an image cropped as @p shown says
///        (the opposite of shownCrop())
ImageCrop fileCrop(const ImageCrop& shown, const ImageProperties& image);

/// @brief The image with another crop of its sides as they are shown (shownCrop())
/// @param keepScale true: the rest of the picture keeps its size, the image gets smaller;
///        false: the image keeps its width (an own height its stretch), the rest of the
///        picture gets larger
/// @param widthCentimeters The image's width now, what a width of the file's own becomes
///        when the image keeps its width; 0: it stays the file's own
ImageProperties withShownCrop(const ImageProperties& image, const ImageCrop& crop, bool keepScale,
                              double widthCentimeters = 0.0);

/// @brief The pixels an image has as the file's picture, cropped and turned: its own size
QSize shownPixels(const QSize& filePixels, const ImageProperties& image);

/// @brief Draw the file's picture as the image shows it: cropped, turned, mirrored, with
///        its filter, corrections and transparency, in @p pixels
/// @param picture The file's picture (all of it), at any size
QImage shownPicture(const QImage& picture, const ImageProperties& image, const QSize& pixels);

/// @brief A key of what shownPicture() makes of a file's picture (its crop, turn, mirror,
///        filter, corrections and transparency): pictures with the same key are the same
QString pictureLookKey(const ImageProperties& image);

/// @brief The name of a filter in KML ("grayscale"); empty for None
QString imageFilterKml(ImageFilter filter);

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

/// @brief The attributes of <img>: src, alt (when it has one), width ("60%", "4.5cm";
///        none: the file's own) and those of the look the image has (docs/kml_format.md)
std::vector<std::pair<QString, QString>> imageKmlAttributes(const ImageProperties& image);

/// @brief The whole element of an image: <img src="..." alt="..." width="..."/>
QString imageKml(const ImageProperties& image);

/// @brief The properties of an image from the attributes of its <img>: a width in percent,
///        or a length of KML (cm, mm, in, pt, px, pc) in centimeters; one that is not known
///        gives the file's own
ImageProperties imagePropertiesOfKml(const QXmlStreamAttributes& attributes);

}  // namespace kalahari::editor
