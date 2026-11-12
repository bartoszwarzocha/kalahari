/// @file test_image_look.cpp
/// @brief The look of an image: its description, turn, mirror, crop, colors and frame, in
///        its format, in KML, as its picture is drawn and as the layout sizes it

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/image_store.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/text_images.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QXmlStreamReader>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;
using Catch::Approx;

namespace {

/// The properties of an <img> element
ImageProperties imageOfElement(const QString& element) {
    QXmlStreamReader reader(element);
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }
    return imagePropertiesOfKml(reader.attributes());
}

/// A picture of two halves: red on the left, blue on the right
QImage halves(const QSize& size) {
    QImage picture(size, QImage::Format_ARGB32);
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            picture.setPixelColor(x, y, x < size.width() / 2 ? QColor(Qt::red) : QColor(Qt::blue));
        }
    }
    return picture;
}

/// A picture of one color
QImage filled(const QSize& size, const QColor& color) {
    QImage picture(size, QImage::Format_ARGB32);
    picture.fill(color);
    return picture;
}

/// A book's main folder with pictures in its folder of images
struct BookFolder {
    QTemporaryDir dir;

    QString path() const { return dir.path(); }

    QString add(const QString& name, const QImage& picture) const {
        QDir(dir.path()).mkpath(QStringLiteral("images"));
        const QString source = QStringLiteral("images/") + name;
        REQUIRE(picture.save(dir.path() + QLatin1Char('/') + source));
        return source;
    }
};

std::unique_ptr<BookEditor> editorWith(const QString& kml, const QString& folder) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(700, 500));
    EditorAppearance appearance;
    appearance.cursor.blinking = false;
    editor->setAppearance(appearance);
    editor->setImagesFolder(folder);
    editor->fromKml(kml);
    return editor;
}

KalahariTextDocumentLayout* layoutOf(BookEditor& editor) {
    return qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
}

/// The size of the box of the image of the first paragraph
QSizeF boxOf(BookEditor& editor) {
    const QTextBlock block = editor.textDocument()->firstBlock();
    const std::optional<ImageProperties> image = imageAt(*editor.textDocument(), block.position());
    REQUIRE(image);
    return layoutOf(editor)->imageSize(block, *image);
}

/// The left button pressed, the mouse moved with it and the button let go
void mouse(BookEditor& editor, QEvent::Type type, const QPointF& point) {
    const Qt::MouseButtons buttons =
        type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons(Qt::LeftButton);
    QMouseEvent event(type, point, editor.mapToGlobal(point),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &event);
}

void click(BookEditor& editor, const QPointF& point) {
    mouse(editor, QEvent::MouseButtonPress, point);
    mouse(editor, QEvent::MouseButtonRelease, point);
}

/// A mark dragged from one point to another
void drag(BookEditor& editor, const QPointF& from, const QPointF& to) {
    mouse(editor, QEvent::MouseButtonPress, from);
    mouse(editor, QEvent::MouseMove, (from + to) / 2.0);
    mouse(editor, QEvent::MouseMove, to);
    mouse(editor, QEvent::MouseButtonRelease, to);
}

void key(BookEditor& editor, int key) {
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(&editor, &press);
}

/// The image of the first paragraph as it is in the text
ImageProperties firstImageOf(BookEditor& editor) {
    const std::optional<ImageProperties> image =
        imageAt(*editor.textDocument(), editor.textDocument()->firstBlock().position());
    REQUIRE(image);
    return *image;
}

/// The look of an image with every property set
ImageProperties everything() {
    ImageProperties image;
    image.source = QStringLiteral("images/bridge.jpg");
    image.alt = QStringLiteral("The bridge");
    image.unit = ImageWidthUnit::Percent;
    image.width = 80.0;
    image.height = 5.0;
    image.name = QStringLiteral("Bridge 1931");
    image.description = QStringLiteral("A steel bridge of three arches");
    image.decorative = true;
    image.rotation = 1;
    image.flipHorizontal = true;
    image.flipVertical = true;
    image.crop = ImageCrop{5.0, 10.0, 15.0, 20.0};
    image.filter = ImageFilter::BlackWhite;
    image.brightness = 10;
    image.contrast = -20;
    image.threshold = 60;
    image.transparency = 30;
    image.borderLine = ImageBorderLine::Dashed;
    image.borderWidth = 1.5;
    image.borderColor = QColor(QStringLiteral("#336699"));
    image.borderPadding = 2.0;
    image.shadow = true;
    return image;
}

}  // namespace

// =============================================================================
// The format and KML
// =============================================================================

TEST_CASE("Image look: the format of the character keeps all of it", "[editor][images][look]") {
    const ImageProperties image = everything();
    const std::optional<ImageProperties> kept = imageOf(imageCharFormat(image));
    REQUIRE(kept);
    CHECK(*kept == image);

    // The defaults take no properties: two plain images have the same format
    ImageProperties plain;
    plain.source = QStringLiteral("images/a.png");
    QTextCharFormat format = imageCharFormat(plain);
    CHECK(format == imageCharFormat(*imageOf(format)));
    CHECK_FALSE(format.hasProperty(IMAGE_THRESHOLD_PROPERTY));
    CHECK_FALSE(format.hasProperty(IMAGE_BORDER_WIDTH_PROPERTY));
    CHECK(plain.hasPlainPicture());
    CHECK_FALSE(plain.hasFrame());

    // Taken off with the image: text typed after it has none of them
    format = imageCharFormat(image);
    clearImage(format);
    for (const int property : {IMAGE_NAME_PROPERTY, IMAGE_CROP_LEFT_PROPERTY,
                               IMAGE_FILTER_PROPERTY, IMAGE_SHADOW_PROPERTY}) {
        CHECK_FALSE(format.hasProperty(property));
    }
}

TEST_CASE("Image look: KML writes each property, and reads it back", "[editor][images][look][kml]") {
    const QString kml = imageKml(everything());
    CHECK(kml == QStringLiteral(
                     "<img src=\"images/bridge.jpg\" alt=\"The bridge\" width=\"80%\" "
                     "height=\"5cm\" name=\"Bridge 1931\" description=\"A steel bridge of three "
                     "arches\" decorative=\"true\" rotate=\"90\" flip=\"horizontal vertical\" "
                     "crop=\"10% 15% 20% 5%\" filter=\"black-white\" brightness=\"10%\" "
                     "contrast=\"-20%\" threshold=\"60%\" transparency=\"30%\" border=\"dashed\" "
                     "border-width=\"1.5pt\" border-color=\"#336699\" border-padding=\"2pt\" "
                     "shadow=\"true\"/>"));
    CHECK(imageOfElement(kml) == everything());

    // A plain image writes only its file, text and width
    ImageProperties plain;
    plain.source = QStringLiteral("images/a.png");
    CHECK(imageKml(plain) == QStringLiteral("<img src=\"images/a.png\"/>"));

    // The threshold only with black and white, the line's width and color only with a line
    ImageProperties gray = plain;
    gray.filter = ImageFilter::Grayscale;
    gray.threshold = 30;
    gray.borderWidth = 3.0;
    gray.borderColor = QColor(Qt::red);
    CHECK(imageKml(gray) == QStringLiteral("<img src=\"images/a.png\" filter=\"grayscale\"/>"));
}

TEST_CASE("Image look: KML of other forms, and values beyond what an image takes",
          "[editor][images][look][kml]") {
    // The crop as CSS gives the sides of a box: one value for all four, two for top and
    // bottom then left and right
    CHECK(imageOfElement(QStringLiteral("<img src=\"a.png\" crop=\"10%\"/>")).crop ==
          ImageCrop{10.0, 10.0, 10.0, 10.0});
    CHECK(imageOfElement(QStringLiteral("<img src=\"a.png\" crop=\"5 20\"/>")).crop ==
          ImageCrop{20.0, 5.0, 20.0, 5.0});
    CHECK(imageOfElement(QStringLiteral("<img src=\"a.png\" crop=\"wide\"/>")).crop.isEmpty());
    // Something of the picture always stays: both sides together at most 95%
    const ImageCrop most = imageOfElement(QStringLiteral("<img src=\"a.png\" crop=\"0 80% 0 80%\"/>")).crop;
    CHECK(most.left + most.right == Approx(MAX_IMAGE_CROP));
    CHECK(most.left == Approx(most.right));

    // A turn to the nearest quarter, whole turns left out
    CHECK(imageOfElement(QStringLiteral("<img src=\"a.png\" rotate=\"270\"/>")).rotation == 3);
    CHECK(imageOfElement(QStringLiteral("<img src=\"a.png\" rotate=\"450\"/>")).rotation == 1);
    CHECK(imageOfElement(QStringLiteral("<img src=\"a.png\" rotate=\"-90\"/>")).rotation == 3);
    CHECK(imageOfElement(QStringLiteral("<img src=\"a.png\" rotate=\"100\"/>")).rotation == 1);
    CHECK(imageOfElement(QStringLiteral("<img src=\"a.png\" rotate=\"x\"/>")).rotation == 0);

    // Flags as KML has them, names in any letter case
    const ImageProperties flags = imageOfElement(QStringLiteral(
        "<img src=\"a.png\" decorative=\"1\" shadow=\"TRUE\" flip=\"Vertical\" filter=\"Sepia\" "
        "border=\"DOUBLE\"/>"));
    CHECK(flags.decorative);
    CHECK(flags.shadow);
    CHECK_FALSE(flags.flipHorizontal);
    CHECK(flags.flipVertical);
    CHECK(flags.filter == ImageFilter::Sepia);
    CHECK(flags.borderLine == ImageBorderLine::Double);

    // Values clamped: the corrections, the threshold, the transparency, the border
    const ImageProperties clamped = imageOfElement(QStringLiteral(
        "<img src=\"a.png\" brightness=\"90\" contrast=\"-300%\" filter=\"black-white\" "
        "threshold=\"5\" transparency=\"100%\" border=\"solid\" border-width=\"1in\" "
        "border-padding=\"5cm\" border-color=\"red\" filter-x=\"y\"/>"));
    CHECK(clamped.brightness == MAX_IMAGE_CORRECTION);
    CHECK(clamped.contrast == MIN_IMAGE_CORRECTION);
    CHECK(clamped.threshold == MIN_IMAGE_THRESHOLD);
    CHECK(clamped.transparency == MAX_IMAGE_TRANSPARENCY);
    CHECK(clamped.borderWidth == Approx(MAX_IMAGE_BORDER_WIDTH));
    CHECK(clamped.borderPadding == Approx(MAX_IMAGE_BORDER_PADDING));
    CHECK_FALSE(clamped.borderColor.isValid());  // only "#rrggbb"

    // An unknown filter or line is none; a height in another unit is in centimeters
    const ImageProperties other = imageOfElement(
        QStringLiteral("<img src=\"a.png\" filter=\"neon\" border=\"wavy\" height=\"20mm\"/>"));
    CHECK(other.filter == ImageFilter::None);
    CHECK(other.borderLine == ImageBorderLine::None);
    CHECK(other.height == Approx(2.0));

    // Line breaks of the name and description are spaces
    CHECK(imageOfElement(QStringLiteral("<img src=\"a.png\" name=\" Map&#10;of the river \"/>")).name ==
          QStringLiteral("Map of the river"));
}

TEST_CASE("Image look: an image with its look keeps it in the editor's KML",
          "[editor][images][look][kml]") {
    const QString kml = QStringLiteral("<kml><p><img src=\"images/a.png\" alt=\"A\" width=\"50%\" "
                                       "rotate=\"180\" crop=\"0% 10% 0% 10%\" filter=\"sepia\" "
                                       "border=\"solid\" shadow=\"true\"/></p></kml>");
    BookFolder book;
    auto editor = editorWith(kml, book.path());
    CHECK(editor->toKml() == kml);
}

// =============================================================================
// The picture
// =============================================================================

TEST_CASE("Image look: the pixels of a cropped and turned image", "[editor][images][look]") {
    ImageProperties image;
    CHECK(shownPixels(QSize(200, 100), image) == QSize(200, 100));
    image.crop = ImageCrop{25.0, 10.0, 25.0, 0.0};
    CHECK(croppedRect(QSize(200, 100), image.crop) == QRect(50, 10, 100, 90));
    CHECK(shownPixels(QSize(200, 100), image) == QSize(100, 90));
    image.rotation = 1;
    CHECK(shownPixels(QSize(200, 100), image) == QSize(90, 100));
    image.rotation = 2;
    CHECK(shownPixels(QSize(200, 100), image) == QSize(100, 90));
}

TEST_CASE("Image look: the crop of the sides as the image is shown", "[editor][images][look]") {
    ImageProperties image;
    image.crop = ImageCrop{1.0, 2.0, 3.0, 4.0};  // left, top, right, bottom
    CHECK(shownCrop(image) == image.crop);

    // A quarter turn clockwise: the file's left side on top, its top on the right
    image.rotation = 1;
    CHECK(shownCrop(image) == ImageCrop{4.0, 1.0, 2.0, 3.0});
    // Mirrored left to right first: its right side on top
    image.flipHorizontal = true;
    CHECK(shownCrop(image) == ImageCrop{4.0, 3.0, 2.0, 1.0});

    // And back: each look, each side
    for (const int rotation : {0, 1, 2, 3}) {
        for (const bool horizontal : {false, true}) {
            for (const bool vertical : {false, true}) {
                image.rotation = rotation;
                image.flipHorizontal = horizontal;
                image.flipVertical = vertical;
                INFO(rotation << horizontal << vertical);
                CHECK(fileCrop(shownCrop(image), image) == image.crop);
            }
        }
    }
}

TEST_CASE("Image look: the picture mirrored, turned and cropped", "[editor][images][look]") {
    const QImage picture = halves(QSize(40, 20));
    ImageProperties image;

    // Mirrored left to right: blue on the left
    image.flipHorizontal = true;
    QImage shown = shownPicture(picture, image, QSize(40, 20));
    CHECK(shown.pixelColor(5, 10) == QColor(Qt::blue));
    CHECK(shown.pixelColor(35, 10) == QColor(Qt::red));

    // A quarter turn clockwise: the left half on top, the picture upright
    image.flipHorizontal = false;
    image.rotation = 1;
    shown = shownPicture(picture, image, QSize(20, 40));
    REQUIRE(shown.size() == QSize(20, 40));
    CHECK(shown.pixelColor(10, 5) == QColor(Qt::red));
    CHECK(shown.pixelColor(10, 35) == QColor(Qt::blue));

    // Mirrored first, then turned: the right half on top
    image.flipHorizontal = true;
    shown = shownPicture(picture, image, QSize(20, 40));
    CHECK(shown.pixelColor(10, 5) == QColor(Qt::blue));

    // The right half cut off: all red, in the pixels asked for
    image = ImageProperties();
    image.crop.right = 50.0;
    shown = shownPicture(picture, image, QSize(30, 30));
    REQUIRE(shown.size() == QSize(30, 30));
    CHECK(shown.pixelColor(29, 15) == QColor(Qt::red));
}

TEST_CASE("Image look: the filters, the corrections and the transparency",
          "[editor][images][look]") {
    const auto colorOf = [](const QColor& color, const ImageProperties& image) {
        return shownPicture(filled(QSize(4, 4), color), image, QSize(4, 4)).pixelColor(1, 1);
    };
    ImageProperties image;

    image.filter = ImageFilter::Grayscale;
    const QColor gray = colorOf(Qt::red, image);
    CHECK(gray.red() == gray.green());
    CHECK(gray.green() == gray.blue());
    CHECK(gray.red() == 54);  // the luminance of red

    image.filter = ImageFilter::Sepia;
    CHECK(colorOf(Qt::white, image) == QColor(255, 255, 239));

    // Black and white at the threshold: a gray of 40% darker than half is black, lighter
    // than 30% white
    image.filter = ImageFilter::BlackWhite;
    const QColor darkGray(102, 102, 102);
    CHECK(colorOf(darkGray, image) == QColor(Qt::black));
    image.threshold = 30;
    CHECK(colorOf(darkGray, image) == QColor(Qt::white));
    // The corrections come before the threshold: a lighter scan has less black
    image.threshold = DEFAULT_IMAGE_THRESHOLD;
    image.brightness = 50;
    CHECK(colorOf(darkGray, image) == QColor(Qt::white));

    // A pale wash: a dark color light, the paper white
    image = ImageProperties();
    image.filter = ImageFilter::Wash;
    CHECK(colorOf(QColor(40, 60, 80), image).lightness() > 180);
    CHECK(colorOf(Qt::black, image).lightness() > 170);
    CHECK(colorOf(Qt::white, image) == QColor(Qt::white));

    // Brightness and contrast without a filter
    image = ImageProperties();
    image.brightness = -50;
    CHECK(colorOf(Qt::white, image).red() == 128);
    image.brightness = 0;
    image.contrast = 50;
    CHECK(colorOf(QColor(192, 192, 192), image).red() == 224);

    // Transparency: the picture shows less
    image = ImageProperties();
    image.transparency = 50;
    CHECK(colorOf(Qt::red, image).alpha() == 128);

    // Each look its own key; the plain one none
    CHECK(pictureLookKey(ImageProperties()).isEmpty());
    ImageProperties sepia;
    sepia.filter = ImageFilter::Sepia;
    ImageProperties turned;
    turned.rotation = 1;
    CHECK_FALSE(pictureLookKey(sepia).isEmpty());
    CHECK(pictureLookKey(sepia) != pictureLookKey(turned));
}

TEST_CASE("Image look: the store draws the image as it is shown", "[editor][images][look]") {
    BookFolder book;
    ImageStore store;
    store.setFolder(book.path());
    ImageProperties image;
    image.source = book.add(QStringLiteral("halves.png"), halves(QSize(200, 100)));
    image.crop.left = 50.0;
    image.rotation = 1;

    // Its own pixels: the right half, turned
    REQUIRE(store.shownPixelSize(image));
    CHECK(*store.shownPixelSize(image) == QSize(100, 100));
    const QImage shown = store.shownImage(image, QSize(50, 50));
    REQUIRE(shown.size() == QSize(50, 50));
    CHECK(shown.pixelColor(25, 10) == QColor(Qt::blue));
    CHECK(shown.pixelColor(25, 40) == QColor(Qt::blue));

    // A missing file: nothing
    image.source = QStringLiteral("images/none.png");
    CHECK_FALSE(store.shownPixelSize(image));
    CHECK(store.shownImage(image, QSize(50, 50)).isNull());
}

// =============================================================================
// Layout and drawing
// =============================================================================

TEST_CASE("Image look: the frame takes room around the picture", "[editor][images][look][layout]") {
    BookFolder book;
    book.add(QStringLiteral("a.png"), filled(QSize(96, 48), Qt::green));
    auto plain = editorWith(QStringLiteral("<kml><p><img src=\"images/a.png\" width=\"3cm\"/></p></kml>"),
                            book.path());
    auto framed = editorWith(QStringLiteral("<kml><p><img src=\"images/a.png\" width=\"3cm\" "
                                            "border=\"solid\" border-width=\"1pt\" "
                                            "border-padding=\"2pt\" shadow=\"true\"/></p></kml>"),
                             book.path());
    const QSizeF picture = boxOf(*plain);
    const QSizeF box = boxOf(*framed);
    const std::optional<ImageProperties> image =
        imageAt(*framed->textDocument(), framed->textDocument()->firstBlock().position());
    REQUIRE(image);
    const QMarginsF margins = layoutOf(*framed)->imageFrameMargins(*image);
    CHECK(margins.left() > 0.0);
    CHECK(margins.left() == margins.top());
    CHECK(margins.right() > margins.left());  // and the shadow
    CHECK(box.width() == Approx(picture.width() + margins.left() + margins.right()));
    CHECK(box.height() == Approx(picture.height() + margins.top() + margins.bottom()));

    // A double line is three times as wide
    ImageProperties doubled = *image;
    doubled.borderLine = ImageBorderLine::Double;
    doubled.shadow = false;
    doubled.borderPadding = 0.0;
    ImageProperties single = doubled;
    single.borderLine = ImageBorderLine::Solid;
    const KalahariTextDocumentLayout* layout = layoutOf(*framed);
    CHECK(layout->imageFrameMargins(doubled).left() ==
          Approx(3.0 * layout->imageFrameMargins(single).left()).margin(1.0));

    // Resized by a handle, the box's width gives the picture's: the frame stays
    const QTextBlock block = framed->textDocument()->firstBlock();
    const ImageProperties resized = layout->imageOfWidth(block, *image, box.width());
    CHECK(resized.width == Approx(3.0).margin(0.02));
}

TEST_CASE("Image look: an own height, a crop and a turn give the image its shape",
          "[editor][images][look][layout]") {
    BookFolder book;
    book.add(QStringLiteral("wide.png"), filled(QSize(200, 100), Qt::green));
    const auto box = [&book](const QString& attributes) {
        auto editor = editorWith(QStringLiteral("<kml><p><img src=\"images/wide.png\" ") + attributes +
                                     QStringLiteral("/></p></kml>"),
                                 book.path());
        return boxOf(*editor);
    };
    const QSizeF natural = box(QString());
    CHECK(natural.width() == Approx(2.0 * natural.height()).margin(1.0));

    // Cropped: the file's own size of its part
    const QSizeF half = box(QStringLiteral("crop=\"0 50% 0 0\""));
    CHECK(half.width() == Approx(natural.width() / 2.0).margin(1.0));
    CHECK(half.height() == Approx(natural.height()).margin(1.0));

    // Turned a quarter: upright
    const QSizeF upright = box(QStringLiteral("rotate=\"90\""));
    CHECK(upright.width() == Approx(natural.height()).margin(1.0));
    CHECK(upright.height() == Approx(natural.width()).margin(1.0));

    // An own height: not in proportion
    const QSizeF square = box(QStringLiteral("width=\"4cm\" height=\"4cm\""));
    CHECK(square.width() == Approx(square.height()).margin(1.0));

    // Wider than the line: smaller, in its own shape
    const QSizeF wide = box(QStringLiteral("width=\"100cm\" height=\"50cm\""));
    CHECK(wide.width() == Approx(2.0 * wide.height()).margin(2.0));
}

TEST_CASE("Image look: the editor draws the picture mirrored and its border",
          "[editor][images][look][view]") {
    BookFolder book;
    book.add(QStringLiteral("halves.png"), halves(QSize(200, 100)));
    auto editor = editorWith(QStringLiteral("<kml><p><img src=\"images/halves.png\" "
                                            "flip=\"horizontal\" border=\"solid\" "
                                            "border-width=\"3pt\" border-color=\"#00ff00\"/></p></kml>"),
                             book.path());
    const int position = editor->textDocument()->firstBlock().position();
    const std::optional<QRectF> rect = editor->imageRect(position);
    REQUIRE(rect);
    QImage image(editor->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    editor->render(&image);

    // Blue on the left, red on the right, inside a green line
    const QRect box = rect->toAlignedRect();
    const int middle = box.center().y();
    CHECK(image.pixelColor(box.left() + box.width() / 4, middle) == QColor(Qt::blue));
    CHECK(image.pixelColor(box.right() - box.width() / 4, middle) == QColor(Qt::red));
    CHECK(image.pixelColor(box.left() + 1, middle) == QColor(0, 255, 0));
}

// =============================================================================
// The commands of the Image menu
// =============================================================================

TEST_CASE("Image look: Rotate turns the image, an own shape with it", "[editor][images][look][commands]") {
    BookFolder book;
    book.add(QStringLiteral("wide.png"), filled(QSize(200, 100), Qt::green));
    auto editor = editorWith(QStringLiteral("<kml><p><img src=\"images/wide.png\" width=\"4cm\" "
                                            "height=\"1cm\"/></p></kml>"),
                             book.path());
    const int position = editor->textDocument()->firstBlock().position();
    const QSizeF before = boxOf(*editor);
    editor->selectImage(position);

    REQUIRE(editor->rotateImage(position, 1));
    // Still selected, for the next command of the menu
    CHECK(editor->selectedImage() == position);
    ImageProperties turned = firstImageOf(*editor);
    CHECK(turned.rotation == 1);
    // As wide as it was high and as high as it was wide
    CHECK(turned.unit == ImageWidthUnit::Centimeters);
    CHECK(turned.width == Approx(1.0).margin(0.02));
    CHECK(turned.height == Approx(4.0).margin(0.02));
    const QSizeF after = boxOf(*editor);
    CHECK(after.width() == Approx(before.height()).margin(1.0));
    CHECK(after.height() == Approx(before.width()).margin(1.0));

    // Left from a quarter: as it was; a whole turn counts round
    REQUIRE(editor->rotateImage(position, -1));
    CHECK(firstImageOf(*editor).rotation == 0);
    REQUIRE(editor->rotateImage(position, -1));
    CHECK(firstImageOf(*editor).rotation == 3);
    REQUIRE(editor->rotateImage(position, 2));
    CHECK(firstImageOf(*editor).rotation == 1);

    // Each one a step of Undo
    editor->undo();
    editor->undo();
    editor->undo();
    editor->undo();
    CHECK(firstImageOf(*editor) ==
          imageOfElement(QStringLiteral("<img src=\"images/wide.png\" width=\"4cm\" height=\"1cm\"/>")));

    // Not an image: nothing
    CHECK_FALSE(editor->rotateImage(position + 1, 1));
}

TEST_CASE("Image look: Flip mirrors the image as it is shown", "[editor][images][look][commands]") {
    BookFolder book;
    book.add(QStringLiteral("wide.png"), filled(QSize(200, 100), Qt::green));
    auto editor = editorWith(QStringLiteral("<kml><p><img src=\"images/wide.png\"/></p></kml>"), book.path());
    const int position = editor->textDocument()->firstBlock().position();

    REQUIRE(editor->flipImage(position, Qt::Horizontal));
    CHECK(firstImageOf(*editor).flipHorizontal);
    CHECK_FALSE(firstImageOf(*editor).flipVertical);
    REQUIRE(editor->flipImage(position, Qt::Horizontal));
    CHECK_FALSE(firstImageOf(*editor).flipHorizontal);

    // Turned a quarter: across the image shown is down its file
    REQUIRE(editor->rotateImage(position, 1));
    REQUIRE(editor->flipImage(position, Qt::Horizontal));
    CHECK_FALSE(firstImageOf(*editor).flipHorizontal);
    CHECK(firstImageOf(*editor).flipVertical);
    REQUIRE(editor->flipImage(position, Qt::Vertical));
    CHECK(firstImageOf(*editor).flipHorizontal);
}

TEST_CASE("Image look: Filter gives the image its colors", "[editor][images][look][commands]") {
    BookFolder book;
    book.add(QStringLiteral("wide.png"), filled(QSize(200, 100), Qt::green));
    auto editor = editorWith(QStringLiteral("<kml><p><img src=\"images/wide.png\"/></p></kml>"), book.path());
    const int position = editor->textDocument()->firstBlock().position();

    REQUIRE(editor->setImageFilter(position, ImageFilter::Sepia));
    CHECK(firstImageOf(*editor).filter == ImageFilter::Sepia);
    // The same filter again: no change, no step of Undo
    CHECK_FALSE(editor->setImageFilter(position, ImageFilter::Sepia));
    editor->undo();
    CHECK(firstImageOf(*editor).filter == ImageFilter::None);
    CHECK_FALSE(editor->canUndo());
}

TEST_CASE("Image look: Ctrl+] and Ctrl+[ make the image larger and smaller", "[editor][images][look][commands]") {
    BookFolder book;
    book.add(QStringLiteral("wide.png"), filled(QSize(200, 100), Qt::green));
    auto editor = editorWith(QStringLiteral("<kml><p><img src=\"images/wide.png\" width=\"4cm\"/></p></kml>"),
                             book.path());
    const int position = editor->textDocument()->firstBlock().position();

    REQUIRE(editor->resizeImageByStep(position, true));
    ImageProperties image = firstImageOf(*editor);
    CHECK(image.unit == ImageWidthUnit::Centimeters);
    CHECK(image.width == Approx(4.0 * (1.0 + IMAGE_KEY_RESIZE)).margin(0.02));
    REQUIRE(editor->resizeImageByStep(position, false));
    CHECK(firstImageOf(*editor).width == Approx(4.0 * (1.0 + IMAGE_KEY_RESIZE) * (1.0 - IMAGE_KEY_RESIZE)).margin(0.02));

    // As wide as its line: no larger; smaller from the width it is shown, in its own unit
    auto full = editorWith(QStringLiteral("<kml><p><img src=\"images/wide.png\" width=\"100%\"/></p></kml>"),
                           book.path());
    const qreal widest = boxOf(*full).width();
    CHECK_FALSE(full->resizeImageByStep(position, true));
    CHECK_FALSE(full->canUndo());
    REQUIRE(full->resizeImageByStep(position, false));
    CHECK(firstImageOf(*full).unit == ImageWidthUnit::Percent);
    CHECK(boxOf(*full).width() == Approx(widest * (1.0 - IMAGE_KEY_RESIZE)).margin(widest / 100.0));
}

TEST_CASE("Image look: Esc on the selected image goes back to the text", "[editor][images][look][commands]") {
    BookFolder book;
    book.add(QStringLiteral("wide.png"), filled(QSize(200, 100), Qt::green));
    auto editor = editorWith(QStringLiteral("<kml><p>Ab<img src=\"images/wide.png\"/>cd</p></kml>"), book.path());
    const int position = editor->textDocument()->firstBlock().position() + 2;
    editor->selectImage(position);
    REQUIRE(editor->selectedImage() == position);

    // Esc is the editor's, before the shortcuts of the window
    QKeyEvent override(QEvent::ShortcutOverride, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(editor.get(), &override);
    CHECK(override.isAccepted());

    key(*editor, Qt::Key_Escape);
    CHECK_FALSE(editor->selectedImage());
    CHECK_FALSE(editor->hasSelection());
    CHECK(editor->cursorPosition() == CursorPosition{0, 3});

    // Without a selected image Esc is not the editor's
    QKeyEvent again(QEvent::ShortcutOverride, Qt::Key_Escape, Qt::NoModifier);
    again.ignore();
    QCoreApplication::sendEvent(editor.get(), &again);
    CHECK_FALSE(again.isAccepted());
}

TEST_CASE("Image look: Crop in the text", "[editor][images][look][commands][crop]") {
    BookFolder book;
    book.add(QStringLiteral("wide.png"), filled(QSize(200, 100), Qt::green));
    const QString kml = QStringLiteral("<kml><p><img src=\"images/wide.png\"/></p><p>Text after it</p></kml>");
    auto editor = editorWith(kml, book.path());
    const int position = editor->textDocument()->firstBlock().position();
    const std::optional<QRectF> rect = editor->imageRect(position);
    REQUIRE(rect);
    const QSizeF before = boxOf(*editor);
    // The mark of the left side, just outside it, and of the bottom right corner
    const QPointF left(rect->left() - 1.0, rect->center().y());
    const QPointF corner(rect->right() + 1.0, rect->bottom() + 1.0);

    REQUIRE(editor->startImageCrop(position));
    REQUIRE(editor->imageCropInProgress());
    CHECK(*editor->imageCropInProgress() == ImageCrop{});
    CHECK(editor->selectedImage() == position);

    SECTION("Enter crops the image to where the marks are") {
        drag(*editor, left, left + QPointF(rect->width() / 4.0, 0.0));
        CHECK(editor->imageCropInProgress()->left == Approx(25.0).margin(1.0));
        // A press on the picture, its cut part too: still cropping
        click(*editor, QPointF(rect->left() + 5.0, rect->center().y()));
        REQUIRE(editor->imageCropInProgress());
        CHECK_FALSE(editor->canUndo());

        key(*editor, Qt::Key_Return);
        CHECK_FALSE(editor->imageCropInProgress());
        const ImageProperties image = firstImageOf(*editor);
        CHECK(image.crop.left == Approx(25.0).margin(1.0));
        CHECK(image.crop.right == 0.0);
        // The rest of the picture as large as it was
        CHECK(boxOf(*editor).width() == Approx(before.width() * 0.75).margin(2.0));
        CHECK(boxOf(*editor).height() == Approx(before.height()).margin(1.0));
        // One step of Undo
        editor->undo();
        CHECK(firstImageOf(*editor).crop == ImageCrop{});
        CHECK_FALSE(editor->canUndo());
    }

    SECTION("A corner takes two sides; a click elsewhere crops the image") {
        drag(*editor, corner, corner - QPointF(rect->width() / 10.0, rect->height() / 5.0));
        CHECK(editor->imageCropInProgress()->right == Approx(10.0).margin(1.0));
        CHECK(editor->imageCropInProgress()->bottom == Approx(20.0).margin(1.0));
        CHECK(editor->imageCropInProgress()->left == 0.0);
        // In the text below it
        click(*editor, QPointF(rect->left() + 5.0, rect->bottom() + 40.0));
        CHECK_FALSE(editor->imageCropInProgress());
        CHECK(firstImageOf(*editor).crop.right == Approx(10.0).margin(1.0));
        CHECK(firstImageOf(*editor).crop.bottom == Approx(20.0).margin(1.0));
    }

    SECTION("Esc leaves the image as it was") {
        drag(*editor, left, left + QPointF(rect->width() / 2.0, 0.0));
        QKeyEvent override(QEvent::ShortcutOverride, Qt::Key_Escape, Qt::NoModifier);
        QCoreApplication::sendEvent(editor.get(), &override);
        CHECK(override.isAccepted());
        key(*editor, Qt::Key_Escape);
        CHECK_FALSE(editor->imageCropInProgress());
        CHECK(firstImageOf(*editor).crop == ImageCrop{});
        CHECK_FALSE(editor->canUndo());
        // The image stays selected: a second Esc goes back to the text
        CHECK(editor->selectedImage() == position);
    }

    SECTION("Some of the picture always stays") {
        drag(*editor, left, QPointF(rect->right() + 50.0, rect->center().y()));
        CHECK(editor->imageCropInProgress()->left == Approx(MAX_IMAGE_CROP));
    }

    SECTION("The keys of the text wait, the image changed ends the cropping") {
        key(*editor, Qt::Key_A);
        CHECK(editor->toKml() == editorWith(kml, book.path())->toKml());
        REQUIRE(editor->imageCropInProgress());
        REQUIRE(editor->rotateImage(position, 1));
        CHECK_FALSE(editor->imageCropInProgress());
    }

    SECTION("Not in a read-only text, nor without its picture") {
        editor->finishImageCrop(false);
        editor->setReadOnly(true);
        CHECK_FALSE(editor->startImageCrop(position));
        auto missing = editorWith(QStringLiteral("<kml><p><img src=\"images/none.png\"/></p></kml>"), book.path());
        CHECK_FALSE(missing->startImageCrop(position));
    }
}


// =============================================================================
// One filter for every image
// =============================================================================

TEST_CASE("Image look: one filter for every image, in KML and in the document",
          "[editor][images][look][filter]") {
    // An image in the filter takes only the filter, and with Black and White its threshold
    ImageProperties image;
    image.source = QStringLiteral("images/a.png");
    image.brightness = 20;
    image.threshold = 30;
    const ImageProperties sepia = withImageFilter(image, ImageFilter::Sepia, 70);
    CHECK(sepia.filter == ImageFilter::Sepia);
    CHECK(sepia.threshold == 30);
    CHECK(sepia.brightness == 20);
    const ImageProperties blackWhite = withImageFilter(image, ImageFilter::BlackWhite, 70);
    CHECK(blackWhite.threshold == 70);
    CHECK(blackWhite.brightness == 20);

    // KML: the images in another filter change, the others keep their elements as they are
    const QString kml = QStringLiteral(
        "<kml><p>One <img src=\"images/a.png\" brightness=\"20%\"/> and "
        "<img alt=\"The &quot;tower&quot;\" src=\"images/b.png\" filter=\"sepia\"/></p>"
        "<table><tr><td><p><img src=\"images/c.png\" filter=\"black-white\"/></p></td></tr></table>"
        "<p><img/>Two</p><p>Three <img src=\"images/d.png\"></img> four</p></kml>");
    int count = -1;
    const QString filtered = withImageFilterKml(kml, ImageFilter::Sepia, DEFAULT_IMAGE_THRESHOLD, &count);
    CHECK(count == 3);
    CHECK(filtered.toStdString() == QStringLiteral(
                          "<kml><p>One <img src=\"images/a.png\" filter=\"sepia\" brightness=\"20%\"/> and "
                          "<img alt=\"The &quot;tower&quot;\" src=\"images/b.png\" filter=\"sepia\"/></p>"
                          "<table><tr><td><p><img src=\"images/c.png\" filter=\"sepia\"/></p></td></tr></table>"
                          "<p><img/>Two</p><p>Three <img src=\"images/d.png\" filter=\"sepia\"/> "
                          "four</p></kml>").toStdString());
    // Nothing to change, or a text that is not KML: the text as it is
    CHECK(withImageFilterKml(filtered, ImageFilter::Sepia, DEFAULT_IMAGE_THRESHOLD, &count) == filtered);
    CHECK(count == 0);
    const QString broken = QStringLiteral("<kml><p><img src=\"images/a.png\"/></kml>");
    CHECK(withImageFilterKml(broken, ImageFilter::Sepia, DEFAULT_IMAGE_THRESHOLD, &count) == broken);
    CHECK(count == 0);
    CHECK(withImageFilterKml(QString(), ImageFilter::Sepia, DEFAULT_IMAGE_THRESHOLD, nullptr).isEmpty());

    // Black and White: its threshold is a change too
    CHECK(withImageFilterKml(QStringLiteral("<kml><p><img src=\"images/c.png\" filter=\"black-white\"/></p></kml>"),
                             ImageFilter::BlackWhite, 60, &count) ==
          QStringLiteral("<kml><p><img src=\"images/c.png\" filter=\"black-white\" threshold=\"60%\"/></p></kml>"));
    CHECK(count == 1);
}

TEST_CASE("Image look: one filter for every image of the text, one step of Undo",
          "[editor][images][look][filter]") {
    BookFolder book;
    book.add(QStringLiteral("a.png"), filled(QSize(40, 20), Qt::green));
    book.add(QStringLiteral("b.png"), filled(QSize(40, 20), Qt::blue));
    const QString kml = QStringLiteral(
        "<kml><p><img src=\"images/a.png\" contrast=\"10%\"/></p>"
        "<p>Text</p><p><img src=\"images/b.png\" filter=\"grayscale\"/></p>"
        "<p><img src=\"images/a.png\" filter=\"wash\"/></p></kml>");
    auto editor = editorWith(kml, book.path());
    const QTextDocument& document = *editor->textDocument();
    CHECK(imagesWithOtherFilter(document, ImageFilter::Grayscale, DEFAULT_IMAGE_THRESHOLD).size() == 2);
    CHECK(imagesWithOtherFilter(document, ImageFilter::None, DEFAULT_IMAGE_THRESHOLD).size() == 2);

    editor->setCursorPosition({1, 2});
    int changed = 0;
    QObject::connect(editor.get(), &BookEditor::contentChanged, [&changed]() { ++changed; });
    CHECK(editor->setAllImagesFilter(ImageFilter::Grayscale, DEFAULT_IMAGE_THRESHOLD) == 2);
    CHECK(changed == 1);
    CHECK(imagesWithOtherFilter(document, ImageFilter::Grayscale, DEFAULT_IMAGE_THRESHOLD).empty());
    CHECK(editor->toKml() == QStringLiteral(
                                 "<kml><p><img src=\"images/a.png\" filter=\"grayscale\" contrast=\"10%\"/></p>"
                                 "<p>Text</p><p><img src=\"images/b.png\" filter=\"grayscale\"/></p>"
                                 "<p><img src=\"images/a.png\" filter=\"grayscale\"/></p></kml>"));
    // The cursor stays where it was
    CHECK(editor->cursorPosition().paragraph == 1);
    CHECK(editor->cursorPosition().offset == 2);
    // Every image already has it: nothing changes
    CHECK(editor->setAllImagesFilter(ImageFilter::Grayscale, DEFAULT_IMAGE_THRESHOLD) == 0);

    // One step of Undo gives every image its own filter back
    editor->undo();
    CHECK(editor->toKml() == kml);
    CHECK_FALSE(editor->canUndo());

    // A text that cannot change keeps its images
    editor->setReadOnly(true);
    CHECK(editor->setAllImagesFilter(ImageFilter::Sepia, DEFAULT_IMAGE_THRESHOLD) == 0);
    CHECK(editor->toKml() == kml);
}
