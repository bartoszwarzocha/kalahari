// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_text_images.cpp
/// @brief Images in the text: KML, the files of a book, the layout, drawing and editing

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/image_store.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/kml_serializer.h>
#include <kalahari/editor/paragraph_format.h>
#include <kalahari/editor/text_images.h>
#include "editor_test_utils.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QMimeData>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QUrl>
#include <QXmlStreamReader>
#include <cmath>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;
using Catch::Approx;

namespace {

/// A book's main folder with its folder of images
struct BookFolder {
    QTemporaryDir dir;

    QString path() const { return dir.path(); }

    /// Save a picture of one color in the folder of images
    QString addPicture(const QString& name, const QSize& size, const QColor& color) const {
        QDir(dir.path()).mkpath(QStringLiteral("images"));
        QImage picture(size, QImage::Format_RGB32);
        picture.fill(color);
        const QString source = QStringLiteral("images/") + name;
        REQUIRE(picture.save(dir.path() + QLatin1Char('/') + source));
        return source;
    }
};

/// A picture of one color saved outside a book
QString pictureFile(const QTemporaryDir& dir, const QString& name, const QSize& size,
                    const QColor& color) {
    QImage picture(size, QImage::Format_RGB32);
    picture.fill(color);
    const QString path = dir.path() + QLatin1Char('/') + name;
    REQUIRE(picture.save(path));
    return path;
}

std::unique_ptr<BookEditor> editorWith(const QString& kml, const QString& folder = QString()) {
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

/// The position of the first image of the document
int firstImage(const QTextDocument& document) {
    const std::vector<int> positions = imagePositions(document, 0, document.characterCount());
    REQUIRE_FALSE(positions.empty());
    return positions.front();
}

/// The properties of an <img> element
ImageProperties imageOfElement(const QString& element) {
    QXmlStreamReader reader(element);
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }
    return imagePropertiesOfKml(reader.attributes());
}

int pixelsOf(const QImage& image, const QColor& color) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y) == color) {
                ++count;
            }
        }
    }
    return count;
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

QImage renderOf(BookEditor& editor) {
    QImage image(editor.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    editor.render(&image);
    return image;
}

}  // namespace

// =============================================================================
// KML
// =============================================================================

TEST_CASE("Images: KML keeps an image of a paragraph with its file, text and width",
          "[editor][images][kml]") {
    const QString kml = QStringLiteral(
        "<kml><p>Before <img src=\"images/map.png\" alt=\"The map &amp; the &quot;road&quot;\" "
        "width=\"60%\"/> after</p><p align=\"center\"><img src=\"images/bridge.jpg\" "
        "width=\"4.5cm\"/></p><p><img src=\"images/plain.gif\"/></p></kml>");
    auto editor = editorWith(kml);
    const QTextDocument& document = *editor->textDocument();
    CHECK(document.findBlockByNumber(0).text() ==
          QStringLiteral("Before ") + QChar(QChar::ObjectReplacementCharacter) +
              QStringLiteral(" after"));

    const std::optional<ImageProperties> map = imageAt(document, 7);
    REQUIRE(map);
    CHECK(map->source == QStringLiteral("images/map.png"));
    CHECK(map->alt == QStringLiteral("The map & the \"road\""));
    CHECK(map->unit == ImageWidthUnit::Percent);
    CHECK(map->width == Approx(60.0));
    const std::optional<ImageProperties> bridge =
        imageAt(document, document.findBlockByNumber(1).position());
    REQUIRE(bridge);
    CHECK(bridge->unit == ImageWidthUnit::Centimeters);
    CHECK(bridge->width == Approx(4.5));
    const std::optional<ImageProperties> plain =
        imageAt(document, document.findBlockByNumber(2).position());
    REQUIRE(plain);
    CHECK(plain->unit == ImageWidthUnit::Natural);

    // Written as read
    CHECK(editor->toKml() == kml);
}

TEST_CASE("Images: an image keeps the link and formatting of its place in KML",
          "[editor][images][kml]") {
    // A link is written for each run of one formatting, as in the text
    const QString kml = QStringLiteral(
        "<kml><p><a href=\"https://example.com\">see </a><a href=\"https://example.com\">"
        "<img src=\"images/a.png\"/></a> and <b><img src=\"images/b.png\"/>"
        "<img src=\"images/b.png\"/></b></p></kml>");
    auto editor = editorWith(kml);
    CHECK(editor->toKml() == kml);
    const QTextDocument& document = *editor->textDocument();
    CHECK(imagePositions(document, 0, document.characterCount()).size() == 3);
}

TEST_CASE("Images: an image of a heading or a cell, and the plain text without it",
          "[editor][images][kml]") {
    const QString kml = QStringLiteral(
        "<kml><h1>A <img src=\"images/a.png\" alt=\"Sun\"/> title</h1><table><tr><td><p>"
        "<img src=\"images/b.png\" width=\"50%\"/></p></td></tr></table><p></p></kml>");
    auto editor = editorWith(kml);
    CHECK(editor->toKml() == kml);
    const QTextDocument& document = *editor->textDocument();
    CHECK(imagePositions(document, 0, document.characterCount()).size() == 2);

    // For other programs' plain text an image is nothing
    CHECK(ClipboardHandler::kmlToText(QStringLiteral(
              "<kml><p>a<img src=\"images/a.png\" alt=\"Sun\"/>b</p></kml>")) ==
          QStringLiteral("ab"));
}

TEST_CASE("Images: widths of KML in other units, and those KML does not know",
          "[editor][images][kml]") {
    const auto widthOf = [](const QString& width) {
        return imageOfElement(QStringLiteral("<img src=\"a.png\" width=\"%1\"/>").arg(width));
    };
    CHECK(widthOf(QStringLiteral("45mm")).unit == ImageWidthUnit::Centimeters);
    CHECK(widthOf(QStringLiteral("45mm")).width == Approx(4.5));
    CHECK(widthOf(QStringLiteral("2in")).width == Approx(5.08));
    CHECK(widthOf(QStringLiteral("72pt")).width == Approx(2.54));
    CHECK(widthOf(QStringLiteral("150%")).width == Approx(MAX_IMAGE_PERCENT));
    CHECK(widthOf(QStringLiteral("0%")).unit == ImageWidthUnit::Natural);
    CHECK(widthOf(QStringLiteral("wide")).unit == ImageWidthUnit::Natural);
    CHECK(widthOf(QStringLiteral("-3cm")).unit == ImageWidthUnit::Natural);

    // Written with two decimals at most
    ImageProperties image;
    image.source = QStringLiteral("images/a.png");
    image.unit = ImageWidthUnit::Centimeters;
    image.width = 4.5678;
    CHECK(imageKml(image) == QStringLiteral("<img src=\"images/a.png\" width=\"4.57cm\"/>"));
}

TEST_CASE("Images: an <img> without a file is nothing, its alternative text one line",
          "[editor][images][kml]") {
    auto editor = editorWith(QStringLiteral("<kml><p>a<img alt=\"x\"/>b</p></kml>"));
    CHECK(editor->textDocument()->findBlockByNumber(0).text() == QStringLiteral("ab"));

    // Line breaks and tabs of an alternative text are spaces: an attribute keeps no others
    ImageProperties image;
    image.source = QStringLiteral("images/a.png");
    image.alt = QStringLiteral("one\ntwo\tthree");
    CHECK(normalizedImage(image).alt == QStringLiteral("one two three"));
}

TEST_CASE("Images: only the book's own files are read", "[editor][images][files]") {
    CHECK(isSafeImageSource(QStringLiteral("images/map.png")));
    CHECK(isSafeImageSource(QStringLiteral("images\\map.png")));
    CHECK_FALSE(isSafeImageSource(QString()));
    CHECK_FALSE(isSafeImageSource(QStringLiteral("../other/map.png")));
    CHECK_FALSE(isSafeImageSource(QStringLiteral("images/../../map.png")));
    CHECK_FALSE(isSafeImageSource(QStringLiteral("/etc/map.png")));
    CHECK_FALSE(isSafeImageSource(QStringLiteral("C:/map.png")));
    CHECK_FALSE(isSafeImageSource(QStringLiteral("https://example.com/map.png")));

    CHECK(isImageFileName(QStringLiteral("Map.PNG")));
    CHECK(isImageFileName(QStringLiteral("photo.jpeg")));
    CHECK(isImageFileName(QStringLiteral("drawing.svg")));
    CHECK_FALSE(isImageFileName(QStringLiteral("notes.txt")));
}

// =============================================================================
// The files of a book
// =============================================================================

TEST_CASE("Images: the store copies files into the folder of images", "[editor][images][files]") {
    BookFolder book;
    QTemporaryDir outside;
    ImageStore store;
    store.setFolder(book.path());

    // A new file is copied under its name; the same content again is the same file
    const QString red = pictureFile(outside, QStringLiteral("photo.png"), QSize(40, 20), Qt::red);
    const QString first = store.addFile(red);
    CHECK(first == QStringLiteral("images/photo.png"));
    CHECK(QFile::exists(book.path() + QStringLiteral("/images/photo.png")));
    CHECK(store.addFile(red) == first);

    // Another file of the name gets a number
    QTemporaryDir other;
    const QString blue = pictureFile(other, QStringLiteral("photo.png"), QSize(40, 20), Qt::blue);
    CHECK(store.addFile(blue) == QStringLiteral("images/photo-2.png"));

    // A file of the folder of images is used as it is
    CHECK(store.addFile(book.path() + QStringLiteral("/images/photo-2.png")) ==
          QStringLiteral("images/photo-2.png"));

    // Its size, and its picture in the pixels asked for
    REQUIRE(store.pixelSize(first));
    CHECK(*store.pixelSize(first) == QSize(40, 20));
    const QImage picture = store.picture(first, QSize(20, 10));
    CHECK(picture.size() == QSize(20, 10));
    CHECK(picture.pixelColor(10, 5) == QColor(Qt::red));
    CHECK_FALSE(store.pixelSize(QStringLiteral("images/missing.png")));
    CHECK(store.picture(QStringLiteral("images/missing.png"), QSize(20, 10)).isNull());

    // A pasted picture as PNG
    QImage pasted(QSize(8, 8), QImage::Format_RGB32);
    pasted.fill(Qt::green);
    const QString saved = store.addPicture(pasted, QStringLiteral("image"));
    CHECK(saved == QStringLiteral("images/image.png"));
    CHECK(store.addPicture(pasted, QStringLiteral("image")) == saved);

    // Without a folder nothing is added or read
    ImageStore none;
    CHECK(none.addFile(red).isEmpty());
    CHECK_FALSE(none.pixelSize(first));
}

TEST_CASE("Images: the copy of a large picture made smaller, in its format", "[editor][images][files]") {
    BookFolder book;
    QTemporaryDir outside;
    ImageStore store;
    store.setFolder(book.path());

    // Wider than allowed: as wide as allowed, in proportion, with the file's name and format
    const QString photo = pictureFile(outside, QStringLiteral("photo.jpg"), QSize(400, 200), Qt::red);
    const QString smaller = store.addFile(photo, 100);
    CHECK(smaller == QStringLiteral("images/photo.jpg"));
    REQUIRE(store.pixelSize(smaller));
    CHECK(*store.pixelSize(smaller) == QSize(100, 50));
    QImageReader reader(book.path() + QLatin1Char('/') + smaller);
    CHECK(reader.format() == QByteArrayLiteral("jpeg"));
    // The same again: the same copy
    CHECK(store.addFile(photo, 100) == smaller);

    // A PNG, and a picture not wider: as it is
    const QString png = pictureFile(outside, QStringLiteral("plan.png"), QSize(300, 300), Qt::blue);
    const QString png100 = store.addFile(png, 100);
    REQUIRE(store.pixelSize(png100));
    CHECK(*store.pixelSize(png100) == QSize(100, 100));
    QTemporaryDir other;
    const QString small = pictureFile(other, QStringLiteral("small.png"), QSize(80, 40), Qt::green);
    const QString kept = store.addFile(small, 100);
    QFile original(small);
    QFile copy(book.path() + QLatin1Char('/') + kept);
    REQUIRE(original.open(QIODevice::ReadOnly));
    REQUIRE(copy.open(QIODevice::ReadOnly));
    CHECK(copy.readAll() == original.readAll());

    // A drawing has no pixels to lose
    QFile svg(outside.path() + QStringLiteral("/drawing.svg"));
    REQUIRE(svg.open(QIODevice::WriteOnly));
    svg.write("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"400\">"
              "<rect width=\"800\" height=\"400\" fill=\"red\"/></svg>");
    svg.close();
    const QString drawing = store.addFile(svg.fileName(), 100);
    REQUIRE(store.pixelSize(drawing));
    CHECK(*store.pixelSize(drawing) == QSize(800, 400));
}

TEST_CASE("Images: Insert > Image makes the copy smaller above the dots per inch of the settings",
          "[editor][images][editing]") {
    BookFolder book;
    QTemporaryDir outside;
    const QString file = pictureFile(outside, QStringLiteral("wide.png"), QSize(3000, 1000), Qt::red);
    auto editor = editorWith(QStringLiteral("<kml><p></p></kml>"), book.path());
    const auto copyWidth = [&book](const QString& source) {
        const std::optional<QSize> pixels = ImageStore::fileSize(book.path() + QLatin1Char('/') + source);
        REQUIRE(pixels);
        return pixels->width();
    };
    const auto sourceOf = [&editor]() {
        const std::optional<ImageProperties> image = imageAt(*editor->textDocument(), 0);
        REQUIRE(image);
        return image->source;
    };
    const auto lineWidth = [&editor]() {
        const std::optional<ImagePropertiesData> data = editor->imagePropertiesData(0);
        REQUIRE(data);
        REQUIRE(data->lineWidth > 72.0);
        return data->lineWidth;
    };

    // Without a limit: the file as it is
    REQUIRE(editor->newImageOptions().maxDpi == 0);
    CHECK(editor->newImageCopyWidth(file) == 0);
    REQUIRE(editor->insertImageFile(file));
    CHECK(copyWidth(sourceOf()) == 3000);
    const double line = lineWidth();
    editor->undo();

    // 150 dpi at the width of the lines (the file is wider than them)
    BookEditor::NewImageOptions options = editor->newImageOptions();
    options.maxDpi = 150;
    editor->setNewImageOptions(options);
    const int wanted = static_cast<int>(std::ceil(line * 150.0 / 72.0));
    CHECK(editor->newImageCopyWidth(file) == wanted);
    QDir(book.path() + QStringLiteral("/images")).removeRecursively();
    REQUIRE(editor->insertImageFile(file));
    CHECK(copyWidth(sourceOf()) == wanted);
    editor->undo();

    // Half the width of the lines: half the pixels
    options.widthPercent = 50;
    editor->setNewImageOptions(options);
    CHECK(editor->newImageCopyWidth(file) == static_cast<int>(std::ceil(line * 0.5 * 150.0 / 72.0)));

    // A picture with fewer dots at its own size stays as it is
    const QString small = pictureFile(outside, QStringLiteral("small.png"), QSize(200, 100), Qt::blue);
    options.widthPercent = 0;
    editor->setNewImageOptions(options);
    CHECK(editor->newImageCopyWidth(small) == 0);
    CHECK(editor->newImageCopyWidth(outside.path() + QStringLiteral("/missing.png")) == 0);
}

TEST_CASE("Images: an image of another book is copied with its file", "[editor][images][files]") {
    BookFolder from;
    BookFolder to;
    const QString source = from.addPicture(QStringLiteral("map.png"), QSize(10, 10), Qt::red);
    to.addPicture(QStringLiteral("map.png"), QSize(10, 10), Qt::blue);  // another one of the name
    ImageStore store;
    store.setFolder(to.path());
    CHECK(store.adoptSource(to.path(), source) == source);  // the same book
    CHECK(store.adoptSource(from.path(), source) == QStringLiteral("images/map-2.png"));
    CHECK(store.adoptSource(from.path(), QStringLiteral("images/gone.png")) ==
          QStringLiteral("images/gone.png"));  // missing there: it stays missing
}

// =============================================================================
// The layout
// =============================================================================

TEST_CASE("Images: an image is as wide as its file, a share of the text or centimeters",
          "[editor][images][layout]") {
    BookFolder book;
    book.addPicture(QStringLiteral("wide.png"), QSize(200, 100), Qt::red);
    auto editor = editorWith(
        QStringLiteral("<kml><p align=\"center\"><img src=\"images/wide.png\"/></p>"
                       "<p align=\"center\"><img src=\"images/wide.png\" width=\"50%\"/></p>"
                       "<p align=\"center\"><img src=\"images/wide.png\" width=\"2.54cm\"/></p>"
                       "<p align=\"center\"><img src=\"images/wide.png\" width=\"500cm\"/></p></kml>"),
        book.path());
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    REQUIRE(layout != nullptr);
    const QTextDocument& document = *editor->textDocument();
    const qreal pixelsPerPoint = layout->pixelsPerPoint();
    const qreal textWidth = document.textWidth();
    const auto sizeOf = [&](int number) {
        const QTextBlock block = document.findBlockByNumber(number);
        return layout->imageSize(block, *imageAt(document, block.position()));
    };

    // 200 pixels at 96 an inch are 150 points
    CHECK(sizeOf(0).width() == Approx(std::round(150.0 * pixelsPerPoint)).margin(1.0));
    CHECK(sizeOf(0).height() == Approx(sizeOf(0).width() / 2.0).margin(1.0));
    CHECK(sizeOf(1).width() == Approx(std::round(textWidth / 2.0)).margin(1.0));
    CHECK(sizeOf(2).width() == Approx(std::round(72.0 * pixelsPerPoint)).margin(1.0));
    CHECK(sizeOf(3).width() == Approx(textWidth).margin(1.0));  // never wider than the line

    // The line is as high as the image: the image stands on its baseline
    const QTextBlock first = document.findBlockByNumber(0);
    const QTextLayout* lines = KalahariTextDocumentLayout::blockLayout(first);
    REQUIRE(lines->lineCount() == 1);
    CHECK(lines->lineAt(0).ascent() == Approx(sizeOf(0).height()));
    CHECK(lines->lineAt(0).naturalTextWidth() == Approx(sizeOf(0).width()).margin(1.0));
}

TEST_CASE("Images: an image of a cell is never wider than the cell", "[editor][images][layout]") {
    BookFolder book;
    book.addPicture(QStringLiteral("wide.png"), QSize(2000, 1000), Qt::red);
    auto editor = editorWith(
        QStringLiteral("<kml><table><tr><td><p><img src=\"images/wide.png\"/></p></td>"
                       "<td><p>b</p></td></tr></table></kml>"),
        book.path());
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    const QTextDocument& document = *editor->textDocument();
    const int position = firstImage(document);
    const QTextBlock block = document.findBlock(position);
    const QSizeF size = layout->imageSize(block, *imageAt(document, position));
    CHECK(size.width() < document.textWidth() / 2.0);
    CHECK(size.width() > document.textWidth() / 2.0 - 4.0 * layout->cellPadding());
}

TEST_CASE("Images: an image higher than a page is as high as a page's text",
          "[editor][images][layout]") {
    BookFolder book;
    book.addPicture(QStringLiteral("tall.png"), QSize(100, 4000), Qt::red);
    auto editor = editorWith(QStringLiteral("<kml><p align=\"center\"><img src=\"images/tall.png\"/></p></kml>"),
                             book.path());
    editor->setViewMode(ViewMode::Page);
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    const QTextDocument& document = *editor->textDocument();
    const int position = firstImage(document);
    const QSizeF size = layout->imageSize(document.findBlock(position), *imageAt(document, position));
    const qreal pageText = layout->pageFlow().textHeight;
    REQUIRE(pageText > 100.0);
    CHECK(size.height() < pageText);
    CHECK(size.height() > pageText * 0.8);
    CHECK(size.width() == Approx(size.height() / 40.0).margin(1.0));  // in proportion
}

TEST_CASE("Images: a missing file is a frame with room for its name", "[editor][images][layout]") {
    BookFolder book;
    auto editor = editorWith(QStringLiteral("<kml><p align=\"center\"><img src=\"images/gone.png\"/></p>"
                                            "<p align=\"center\"><img src=\"images/gone.png\" width=\"80%\"/></p></kml>"),
                             book.path());
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    const QTextDocument& document = *editor->textDocument();
    const std::vector<int> positions = imagePositions(document, 0, document.characterCount());
    REQUIRE(positions.size() == 2);
    const ImageProperties gone = *imageAt(document, positions[0]);
    CHECK(layout->isImageMissing(gone));
    const QSizeF natural = layout->imageSize(document.findBlock(positions[0]), gone);
    const QSizeF wide = layout->imageSize(document.findBlock(positions[1]), *imageAt(document, positions[1]));
    CHECK(natural.width() == Approx(std::round(6.0 * 72.0 / 2.54 * layout->pixelsPerPoint())).margin(1.0));
    CHECK(natural.height() >= 3.0 * QFontMetricsF(document.defaultFont()).height());
    CHECK(wide.width() == Approx(std::round(document.textWidth() * 0.8)).margin(1.0));

    // Drawn without the file: its frame and its name, nothing breaks
    const QImage shown = renderOf(*editor);
    CHECK_FALSE(shown.isNull());
}

TEST_CASE("Images: the line spacing is not taken from the image's height", "[editor][images][layout]") {
    BookFolder book;
    book.addPicture(QStringLiteral("big.png"), QSize(100, 200), Qt::red);
    qreal imageHeight = 0.0;
    const auto lineBoxOf = [&book, &imageHeight](const QString& spacing) {
        auto editor = editorWith(QStringLiteral("<kml><p %1>Text <img src=\"images/big.png\"/></p></kml>")
                                     .arg(spacing),
                                 book.path());
        KalahariTextDocumentLayout* layout = layoutOf(*editor);
        const QTextBlock block = editor->textDocument()->findBlockByNumber(0);
        const QTextLayout* lines = KalahariTextDocumentLayout::blockLayout(block);
        REQUIRE(lines->lineCount() == 1);
        const std::optional<QRectF> image = layout->imageRect(firstImage(*editor->textDocument()));
        REQUIRE(image);
        imageHeight = image->height();
        return layout->lineBoxOf(block, lines->lineAt(0));
    };
    const QRectF single = lineBoxOf(QStringLiteral("line-height=\"1\""));
    const QRectF doubled = lineBoxOf(QStringLiteral("line-height=\"2\""));
    const QRectF exactly = lineBoxOf(QStringLiteral("line-height-exact=\"12\""));
    REQUIRE(imageHeight > 0.0);

    // Double spacing adds a line of text, not the image's height again
    CHECK(single.height() > imageHeight);
    CHECK(doubled.height() > single.height());
    CHECK(doubled.height() < single.height() * 1.4);
    // An exact height never cuts the image
    CHECK(exactly.height() >= imageHeight);
    CHECK(exactly.height() < single.height());
}

// =============================================================================
// Drawing
// =============================================================================

TEST_CASE("Images: an image is drawn with its file's picture", "[editor][images][view]") {
    BookFolder book;
    const QColor red(Qt::red);
    book.addPicture(QStringLiteral("red.png"), QSize(120, 60), red);
    auto editor = editorWith(QStringLiteral("<kml><p align=\"center\"><img src=\"images/red.png\"/></p></kml>"),
                             book.path());
    const int position = firstImage(*editor->textDocument());
    const std::optional<QRectF> rect = editor->imageRect(position);
    REQUIRE(rect);
    const int expected = static_cast<int>(rect->width() * rect->height());
    const int drawn = pixelsOf(renderOf(*editor), red);
    CHECK(drawn > expected * 9 / 10);
    CHECK(drawn <= expected + 2 * static_cast<int>(rect->width() + rect->height()));

    // Without the folder the same image is a missing file
    editor->setImagesFolder(QString());
    CHECK(pixelsOf(renderOf(*editor), red) == 0);
    editor->setImagesFolder(book.path());
    CHECK(pixelsOf(renderOf(*editor), red) > expected * 9 / 10);
}

// =============================================================================
// Editing
// =============================================================================

TEST_CASE("Images: Insert > Image copies the file and inserts it, as one undo step",
          "[editor][images][editing]") {
    BookFolder book;
    QTemporaryDir outside;
    const QString file = pictureFile(outside, QStringLiteral("photo.png"), QSize(64, 32), Qt::red);
    auto editor = editorWith(QStringLiteral("<kml><p>First</p><p></p></kml>"), book.path());
    REQUIRE(editor->canInsertImage());

    // Into the empty paragraph: centered, the image selected
    editor->setCursorPosition({1, 0});
    REQUIRE(editor->insertImageFile(file));
    CHECK(QFile::exists(book.path() + QStringLiteral("/images/photo.png")));
    const QTextDocument& document = *editor->textDocument();
    const QTextBlock block = document.findBlockByNumber(1);
    CHECK(block.text() == QString(QChar(QChar::ObjectReplacementCharacter)));
    CHECK(ownAlignment(block.blockFormat()) == Qt::AlignHCenter);
    REQUIRE(editor->selectedImage());
    CHECK(*editor->selectedImage() == block.position());
    CHECK(editor->toKml() == QStringLiteral(
                                 "<kml><p>First</p><p align=\"center\"><img src=\"images/photo.png\"/></p></kml>"));

    // Typed after it, text is no image
    editor->clearSelection();
    editor->setCursorPosition({1, 1});
    editor->insertText(QStringLiteral("ab"));
    CHECK(editor->toKml() == QStringLiteral("<kml><p>First</p><p align=\"center\"><img "
                                            "src=\"images/photo.png\"/>ab</p></kml>"));
    editor->undo();
    editor->undo();
    CHECK(editor->toKml() == QStringLiteral("<kml><p>First</p><p></p></kml>"));
    CHECK(editor->cursorPosition() == CursorPosition{1, 0});

    // Into a paragraph of text: in a paragraph of its own after it, or before it from its
    // start; the text is not broken
    editor->setCursorPosition({0, 3});
    REQUIRE(editor->insertImageFile(file));
    CHECK(editor->toKml() == QStringLiteral("<kml><p>First</p><p align=\"center\"><img "
                                            "src=\"images/photo.png\"/></p><p></p></kml>"));
    editor->undo();
    editor->setCursorPosition({0, 0});
    REQUIRE(editor->insertImageFile(file));
    CHECK(editor->toKml() == QStringLiteral("<kml><p align=\"center\"><img src=\"images/photo.png\"/></p>"
                                            "<p>First</p><p></p></kml>"));
    CHECK(*editor->selectedImage() == 0);
    editor->undo();

    // Not an image, or a read-only text: nothing
    QFile text(outside.path() + QStringLiteral("/notes.png"));
    REQUIRE(text.open(QIODevice::WriteOnly));
    text.write("not a picture");
    text.close();
    CHECK_FALSE(editor->insertImageFile(text.fileName()));
    editor->setReadOnly(true);
    CHECK_FALSE(editor->canInsertImage());
    CHECK_FALSE(editor->insertImageFile(file));
}

TEST_CASE("Images: without a folder of images no image is inserted", "[editor][images][editing]") {
    QTemporaryDir outside;
    const QString file = pictureFile(outside, QStringLiteral("photo.png"), QSize(4, 4), Qt::red);
    auto editor = editorWith(QStringLiteral("<kml><p>a</p></kml>"));
    CHECK_FALSE(editor->canInsertImage());
    CHECK_FALSE(editor->insertImageFile(file));
    CHECK(editor->toKml() == QStringLiteral("<kml><p>a</p></kml>"));
}

TEST_CASE("Images: the commands of an image", "[editor][images][editing]") {
    BookFolder book;
    book.addPicture(QStringLiteral("a.png"), QSize(40, 20), Qt::red);
    QTemporaryDir outside;
    const QString other = pictureFile(outside, QStringLiteral("b.png"), QSize(30, 30), Qt::blue);
    auto editor = editorWith(
        QStringLiteral("<kml><p align=\"center\"><img src=\"images/a.png\" alt=\"A\" width=\"50%\"/></p>"
                       "<p>After</p></kml>"),
        book.path());
    const int position = firstImage(*editor->textDocument());
    editor->selectImage(position);

    // Original Size
    REQUIRE(editor->setImageOriginalSize(position));
    CHECK(editor->toKml().startsWith(
        QStringLiteral("<kml><p align=\"center\"><img src=\"images/a.png\" alt=\"A\"/></p>")));
    CHECK_FALSE(editor->setImageOriginalSize(position));  // nothing to change
    editor->undo();
    CHECK(editor->toKml().contains(QStringLiteral("width=\"50%\"")));
    REQUIRE(editor->selectedImage());

    // Change Image: another file, the width and the text stay
    REQUIRE(editor->changeImageFile(position, other));
    CHECK(editor->toKml().startsWith(QStringLiteral(
        "<kml><p align=\"center\"><img src=\"images/b.png\" alt=\"A\" width=\"50%\"/></p>")));
    CHECK(editor->imageFilePath(position) == book.path() + QStringLiteral("/images/b.png"));

    // Add Caption: a new paragraph in the style Caption, the cursor in it; again: the same one
    CHECK_FALSE(editor->imageCaption(position));
    REQUIRE(editor->addImageCaption(position));
    CHECK(editor->cursorPosition() == CursorPosition{1, 0});
    editor->insertText(QStringLiteral("Fig. 1"));
    REQUIRE(editor->imageCaption(position));
    CHECK(*editor->imageCaption(position) == QStringLiteral("Fig. 1"));
    CHECK(editor->toKml().contains(QStringLiteral("<p style=\"caption\" align=\"center\">Fig. 1</p><p>After</p>")));
    editor->setCursorPosition({2, 0});
    REQUIRE(editor->addImageCaption(position));
    CHECK(editor->cursorPosition() == CursorPosition{1, 6});
    CHECK(editor->textDocument()->blockCount() == 3);
}

TEST_CASE("Images: a click selects an image", "[editor][images][editing]") {
    BookFolder book;
    book.addPicture(QStringLiteral("a.png"), QSize(120, 80), Qt::red);
    auto editor = editorWith(QStringLiteral("<kml><p align=\"center\">x<img src=\"images/a.png\"/>y</p></kml>"),
                             book.path());
    const int position = firstImage(*editor->textDocument());
    const std::optional<QRectF> rect = editor->imageRect(position);
    REQUIRE(rect);
    CHECK(editor->imagePositionAt(rect->center()) == position);
    CHECK_FALSE(editor->imagePositionAt(rect->topLeft() - QPointF(20.0, 20.0)));
    editor->selectImage(position);
    CHECK(editor->selectedImage() == position);
    editor->clearSelection();
    CHECK_FALSE(editor->selectedImage());
}

// =============================================================================
// The mouse: selecting, resizing by the handles, the properties
// =============================================================================

TEST_CASE("Images: a click on an image selects it, a click elsewhere places the cursor",
          "[editor][images][mouse]") {
    BookFolder book;
    book.addPicture(QStringLiteral("a.png"), QSize(120, 80), Qt::red);
    auto editor = editorWith(QStringLiteral("<kml><p align=\"center\">x<img src=\"images/a.png\"/>y</p>"
                                            "<p>After the image</p></kml>"),
                             book.path());
    const int position = firstImage(*editor->textDocument());
    const std::optional<QRectF> rect = editor->imageRect(position);
    REQUIRE(rect);

    click(*editor, rect->center());
    CHECK(editor->selectedImage() == position);

    // Elsewhere, below the image: the cursor, the image no longer selected
    const std::optional<QRectF> after = editor->imageRect(position);
    REQUIRE(after);
    click(*editor, QPointF(after->center().x(), after->bottom() + 30.0));
    CHECK_FALSE(editor->selectedImage());
}

TEST_CASE("Images: a corner's handle resizes the image in proportion, one undo step",
          "[editor][images][mouse]") {
    BookFolder book;
    book.addPicture(QStringLiteral("a.png"), QSize(400, 200), Qt::red);
    auto editor = editorWith(
        QStringLiteral("<kml><p align=\"center\"><img src=\"images/a.png\" width=\"40%\"/></p></kml>"),
        book.path());
    const int position = firstImage(*editor->textDocument());
    editor->selectImage(position);
    const std::optional<QRectF> rect = editor->imageRect(position);
    REQUIRE(rect);

    // The bottom right handle dragged right by a fifth of the image's width
    const QPointF handle = rect->bottomRight();
    mouse(*editor, QEvent::MouseButtonPress, handle);
    mouse(*editor, QEvent::MouseMove, handle + QPointF(rect->width() / 4.0, 0.0));
    CHECK(renderOf(*editor).width() > 0);  // the preview and its label are drawn
    mouse(*editor, QEvent::MouseButtonRelease, handle + QPointF(rect->width() / 4.0, 0.0));

    const std::optional<ImageProperties> resized = imageAt(*editor->textDocument(), position);
    REQUIRE(resized);
    CHECK(resized->unit == ImageWidthUnit::Percent);
    CHECK(resized->width == Approx(50.0));
    const std::optional<QRectF> bigger = editor->imageRect(position);
    REQUIRE(bigger);
    CHECK(bigger->height() / bigger->width() == Approx(0.5).margin(0.02));
    CHECK(editor->selectedImage() == position);

    editor->undo();
    CHECK(imageAt(*editor->textDocument(), position)->width == Approx(40.0));

    // Never wider than the line
    editor->selectImage(position);
    const std::optional<QRectF> again = editor->imageRect(position);
    REQUIRE(again);
    mouse(*editor, QEvent::MouseButtonPress, again->bottomRight());
    mouse(*editor, QEvent::MouseMove, again->bottomRight() + QPointF(2000.0, 0.0));
    mouse(*editor, QEvent::MouseButtonRelease, again->bottomRight() + QPointF(2000.0, 0.0));
    CHECK(imageAt(*editor->textDocument(), position)->width == Approx(100.0));

    // In a read-only text there are no handles. The mouse moves less than a drag: a press
    // there is on the selected image, and a drag of it waits for the real mouse on Windows.
    editor->undo();
    editor->setReadOnly(true);
    editor->selectImage(position);
    const std::optional<QRectF> locked = editor->imageRect(position);
    REQUIRE(locked);
    const QPointF nudge(QApplication::startDragDistance() - 1, 0.0);
    mouse(*editor, QEvent::MouseButtonPress, locked->bottomRight());
    mouse(*editor, QEvent::MouseMove, locked->bottomRight() + nudge);
    mouse(*editor, QEvent::MouseButtonRelease, locked->bottomRight() + nudge);
    CHECK(imageAt(*editor->textDocument(), position)->width == Approx(40.0));
}

TEST_CASE("Images: a width in centimeters stays in centimeters when resized",
          "[editor][images][mouse]") {
    BookFolder book;
    book.addPicture(QStringLiteral("a.png"), QSize(100, 100), Qt::red);
    auto editor = editorWith(
        QStringLiteral("<kml><p align=\"center\"><img src=\"images/a.png\" width=\"2cm\"/></p></kml>"),
        book.path());
    const int position = firstImage(*editor->textDocument());
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    const QTextBlock block = editor->textDocument()->findBlock(position);
    const qreal twoCentimeters = layout->imageSize(block, *imageAt(*editor->textDocument(), position)).width();
    REQUIRE(editor->setImageLayoutWidth(position, twoCentimeters * 1.5));
    const std::optional<ImageProperties> resized = imageAt(*editor->textDocument(), position);
    REQUIRE(resized);
    CHECK(resized->unit == ImageWidthUnit::Centimeters);
    CHECK(resized->width == Approx(3.0).margin(0.02));

    // Its file's own width becomes a percent of the line (centered: as wide as the widest)
    ImageProperties natural;
    natural.source = QStringLiteral("images/a.png");
    const ImageProperties half = layout->imageOfWidth(block, natural, layout->widestImage(block) / 2.0);
    CHECK(half.unit == ImageWidthUnit::Percent);
    CHECK(half.width == Approx(50.0));
}

TEST_CASE("Images: a double click on an image asks for its properties", "[editor][images][mouse]") {
    BookFolder book;
    book.addPicture(QStringLiteral("a.png"), QSize(120, 80), Qt::red);
    auto editor = editorWith(QStringLiteral("<kml><p>Text <img src=\"images/a.png\"/> text</p></kml>"),
                             book.path());
    const int position = firstImage(*editor->textDocument());
    const std::optional<QRectF> rect = editor->imageRect(position);
    REQUIRE(rect);
    std::vector<int> asked;
    QObject::connect(editor.get(), &BookEditor::imagePropertiesAsked,
                     [&asked](int at, int) { asked.push_back(at); });
    click(*editor, rect->center());
    mouse(*editor, QEvent::MouseButtonDblClick, rect->center());
    mouse(*editor, QEvent::MouseButtonRelease, rect->center());
    REQUIRE(asked.size() == 1);
    CHECK(asked.front() == position);
    CHECK(editor->selectedImage() == position);
}

// =============================================================================
// The clipboard, dropped files and the folder of images
// =============================================================================

TEST_CASE("Images: an image copied into another book takes its file along, one undo step",
          "[editor][images][clipboard]") {
    BookFolder from;
    BookFolder to;
    from.addPicture(QStringLiteral("map.png"), QSize(30, 20), Qt::red);
    auto first = editorWith(
        QStringLiteral("<kml><p>a<img src=\"images/map.png\" alt=\"Map\"/>b</p></kml>"), from.path());
    first->selectImage(firstImage(*first->textDocument()));
    const std::unique_ptr<QMimeData> copied = first->createMimeDataFromSelection();
    REQUIRE(copied);

    // The book it comes from, the address of the file for other programs, the picture alone
    CHECK(QString::fromUtf8(copied->data(QString::fromLatin1(MIME_IMAGES_FOLDER))) == from.path());
    CHECK(copied->html().contains(
        QUrl::fromLocalFile(from.path() + QStringLiteral("/images/map.png")).toString()));
    CHECK(copied->html().contains(QStringLiteral("alt=\"Map\"")));
    REQUIRE(copied->hasImage());
    CHECK(qvariant_cast<QImage>(copied->imageData()).size() == QSize(30, 20));

    // Pasted into another book: the file copied into its folder of images
    auto second = editorWith(QStringLiteral("<kml><p>xy</p></kml>"), to.path());
    second->setCursorPosition({0, 1});
    second->insertFromMimeData(copied.get());
    CHECK(QFile::exists(to.path() + QStringLiteral("/images/map.png")));
    CHECK(second->toKml() ==
          QStringLiteral("<kml><p>x<img src=\"images/map.png\" alt=\"Map\"/>y</p></kml>"));
    second->undo();
    CHECK(second->toKml() == QStringLiteral("<kml><p>xy</p></kml>"));

    // Another file of the name there: the copy gets a number, the pasted image names it
    BookFolder third;
    third.addPicture(QStringLiteral("map.png"), QSize(5, 5), Qt::blue);
    auto other = editorWith(QStringLiteral("<kml><p></p></kml>"), third.path());
    other->insertFromMimeData(copied.get());
    CHECK(other->toKml() ==
          QStringLiteral("<kml><p><img src=\"images/map-2.png\" alt=\"Map\"/></p></kml>"));
    other->undo();
    CHECK(other->toKml() == QStringLiteral("<kml><p></p></kml>"));

    // Pasted into its own book: the same file
    first->clearSelection();
    first->setCursorPosition({0, 0});
    first->insertFromMimeData(copied.get());
    CHECK(first->toKml() == QStringLiteral("<kml><p><img src=\"images/map.png\" alt=\"Map\"/>a<img "
                                           "src=\"images/map.png\" alt=\"Map\"/>b</p></kml>"));
    CHECK_FALSE(QFile::exists(from.path() + QStringLiteral("/images/map-2.png")));
}

TEST_CASE("Images: a picture from the clipboard becomes an image of the book",
          "[editor][images][clipboard]") {
    BookFolder book;
    QImage picture(QSize(16, 8), QImage::Format_RGB32);
    picture.fill(Qt::green);

    // A screenshot: a picture without text
    auto editor = editorWith(QStringLiteral("<kml><p>ab</p></kml>"), book.path());
    editor->setCursorPosition({0, 1});
    QMimeData screenshot;
    screenshot.setImageData(picture);
    editor->insertFromMimeData(&screenshot);
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><p>ab</p><p align=\"center\"><img src=\"images/image.png\"/></p></kml>"));
    CHECK(QImage(book.path() + QStringLiteral("/images/image.png")).size() == QSize(16, 8));
    editor->undo();
    CHECK(editor->toKml() == QStringLiteral("<kml><p>ab</p></kml>"));

    // A picture with text: the text
    QMimeData withText;
    withText.setImageData(picture);
    withText.setText(QStringLiteral("cd"));
    editor->setCursorPosition({0, 1});
    editor->insertFromMimeData(&withText);
    CHECK(editor->toKml() == QStringLiteral("<kml><p>acdb</p></kml>"));

    // Without a folder of images a picture is not pasted
    auto loose = editorWith(QStringLiteral("<kml><p>ab</p></kml>"));
    loose->insertFromMimeData(&screenshot);
    CHECK(loose->toKml() == QStringLiteral("<kml><p>ab</p></kml>"));
}

TEST_CASE("Images: files of pictures pasted or dropped become images, one after another",
          "[editor][images][clipboard]") {
    BookFolder book;
    QTemporaryDir outside;
    const QString red = pictureFile(outside, QStringLiteral("red.png"), QSize(8, 8), Qt::red);
    const QString blue = pictureFile(outside, QStringLiteral("blue.png"), QSize(8, 8), Qt::blue);
    QFile notes(outside.path() + QStringLiteral("/notes.txt"));
    REQUIRE(notes.open(QIODevice::WriteOnly));
    notes.write("text");
    notes.close();
    QMimeData files;
    files.setUrls({QUrl::fromLocalFile(red), QUrl::fromLocalFile(notes.fileName()),
                   QUrl::fromLocalFile(blue)});

    // Pasted: both pictures in their order, the other file left out
    auto editor = editorWith(QStringLiteral("<kml><p>ab</p></kml>"), book.path());
    editor->setCursorPosition({0, 1});
    editor->insertFromMimeData(&files);
    CHECK(editor->toKml() == QStringLiteral("<kml><p>ab</p><p align=\"center\"><img src=\"images/red.png\"/>"
                                            "</p><p align=\"center\"><img src=\"images/blue.png\"/></p></kml>"));

    // Dropped: at the drop point
    auto dropped = editorWith(QStringLiteral("<kml><p>ab</p><p>cd</p></kml>"), book.path());
    REQUIRE(dropped->dropMimeData(&files, {1, 1}, false));
    CHECK(dropped->toKml() == QStringLiteral("<kml><p>ab</p><p>cd</p><p align=\"center\"><img "
                                             "src=\"images/red.png\"/></p><p align=\"center\"><img "
                                             "src=\"images/blue.png\"/></p></kml>"));

    // Without a folder of images nothing is dropped
    auto loose = editorWith(QStringLiteral("<kml><p>ab</p></kml>"));
    CHECK_FALSE(loose->dropMimeData(&files, {0, 1}, false));
    CHECK(loose->toKml() == QStringLiteral("<kml><p>ab</p></kml>"));
}

TEST_CASE("Images: a file saved in another folder takes its images along",
          "[editor][images][files]") {
    BookFolder before;
    BookFolder after;
    before.addPicture(QStringLiteral("x.png"), QSize(4, 4), Qt::red);
    before.addPicture(QStringLiteral("y.png"), QSize(4, 4), Qt::green);
    after.addPicture(QStringLiteral("x.png"), QSize(4, 4), Qt::blue);  // another one of the name
    auto editor = editorWith(QStringLiteral("<kml><p><img src=\"images/x.png\"/>a<img "
                                            "src=\"images/y.png\"/>b<img src=\"images/x.png\"/></p></kml>"),
                             before.path());
    int changes = 0;
    QObject::connect(editor.get(), &BookEditor::contentChanged, [&changes]() { ++changes; });
    editor->moveImagesFolder(after.path());
    CHECK(editor->imagesFolder() == after.path());
    CHECK(QFile::exists(after.path() + QStringLiteral("/images/y.png")));
    CHECK(QImage(after.path() + QStringLiteral("/images/x-2.png")).pixelColor(0, 0) == QColor(Qt::red));
    CHECK(editor->toKml() == QStringLiteral("<kml><p><img src=\"images/x-2.png\"/>a<img "
                                            "src=\"images/y.png\"/>b<img src=\"images/x-2.png\"/></p></kml>"));
    CHECK(changes == 1);

    // The same folder again: nothing
    editor->moveImagesFolder(after.path());
    CHECK(changes == 1);
}
