// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_image_wrap.cpp
/// @brief The text flowing around an image: KML, which images it flows around, the lines
///        beside an image, an image above and below the text on the pages, the commands

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/text_images.h>
#include "kalahari_tests/editor/editor_test_utils.h"

#include <QDir>
#include <QImage>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextLayout>
#include <QXmlStreamReader>
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
    QString addPicture(const QString& name, const QSize& size) const {
        QDir(dir.path()).mkpath(QStringLiteral("images"));
        QImage picture(size, QImage::Format_RGB32);
        picture.fill(Qt::red);
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

KalahariTextDocumentLayout* layoutOf(const BookEditor& editor) {
    return qobject_cast<KalahariTextDocumentLayout*>(editor.textDocument()->documentLayout());
}

/// Every block laid out (none waits)
void layOutAll(const QTextDocument& document) {
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        KalahariTextDocumentLayout::blockLayout(block);
    }
}

/// The properties of an <img> element
ImageProperties imageOfElement(const QString& element) {
    QXmlStreamReader reader(element);
    while (!reader.atEnd() && !reader.isStartElement()) {
        reader.readNext();
    }
    return imagePropertiesOfKml(reader.attributes());
}

/// A paragraph of many lines
QString longParagraph(int sentences) {
    QString text;
    for (int i = 0; i < sentences; ++i) {
        text += QStringLiteral("The wind came over the dunes and the old well sang to it. ");
    }
    return QStringLiteral("<p>") + text.trimmed() + QStringLiteral("</p>");
}

/// The top of a line of a block in the document
qreal lineTop(const KalahariTextDocumentLayout& layout, const QTextBlock& block, int line) {
    return layout.blockY(block.blockNumber()) + block.layout()->lineAt(line).y();
}

/// The box of the image of a block in the document
QRectF imageBox(const KalahariTextDocumentLayout& layout, const QTextBlock& block) {
    const std::optional<QRectF> local = layout.imageRect(block.position());
    REQUIRE(local);
    return local->translated(0.0, layout.blockY(block.blockNumber()));
}

}  // namespace

// =============================================================================
// The model
// =============================================================================

TEST_CASE("Image wrap: KML keeps how the text flows around an image, written last",
          "[editor][images][wrap]") {
    const ImageProperties left =
        imageOfElement(QStringLiteral("<img src=\"images/a.png\" wrap=\"left\"/>"));
    CHECK(left.wrap == ImageWrap::SquareLeft);
    CHECK(imageOfElement(QStringLiteral("<img src=\"images/a.png\" wrap=\"right\"/>")).wrap ==
          ImageWrap::SquareRight);
    CHECK(imageOfElement(QStringLiteral("<img src=\"images/a.png\" wrap=\"Top-Bottom\"/>")).wrap ==
          ImageWrap::TopBottom);
    CHECK(imageOfElement(QStringLiteral("<img src=\"images/a.png\" wrap=\"around\"/>")).wrap ==
          ImageWrap::Inline);
    CHECK(imageOfElement(QStringLiteral("<img src=\"images/a.png\"/>")).wrap == ImageWrap::Inline);

    ImageProperties image = left;
    image.shadow = true;
    CHECK(imageKml(image) ==
          QStringLiteral("<img src=\"images/a.png\" shadow=\"true\" wrap=\"left\"/>"));
    image.wrap = ImageWrap::Inline;
    CHECK(imageKml(image) == QStringLiteral("<img src=\"images/a.png\" shadow=\"true\"/>"));

    // In the format of its character only when it is not the default
    QTextCharFormat format = imageCharFormat(left);
    CHECK(imageOf(format)->wrap == ImageWrap::SquareLeft);
    setImage(format, image);
    CHECK_FALSE(format.hasProperty(IMAGE_WRAP_PROPERTY));
    format.setProperty(IMAGE_WRAP_PROPERTY, 42);  // a value no wrap has: in the text
    CHECK(imageOf(format)->wrap == ImageWrap::Inline);
}

TEST_CASE("Image wrap: the text flows only around an image alone in its paragraph, outside a "
          "table",
          "[editor][images][wrap]") {
    BookFolder book;
    book.addPicture(QStringLiteral("photo.png"), QSize(100, 100));
    auto editor = editorWith(
        QStringLiteral("<kml><p><img src=\"images/photo.png\" wrap=\"left\"/></p>"
                       "<p>Text <img src=\"images/photo.png\" wrap=\"left\"/></p>"
                       "<table><tr><td><p><img src=\"images/photo.png\" wrap=\"left\"/></p></td>"
                       "</tr></table>"
                       "<p><img src=\"images/photo.png\"/></p></kml>"),
        book.path());
    const QTextDocument& document = *editor->textDocument();
    const QTextBlock alone = document.findBlockByNumber(0);
    const QTextBlock inText = document.findBlockByNumber(1);
    const QTextBlock inCell = document.findBlockByNumber(2);
    const QTextBlock inLine = document.findBlockByNumber(3);
    REQUIRE(inLine.isValid());

    CHECK(holdsImageAlone(alone));
    CHECK(floatingImage(alone).has_value());
    CHECK_FALSE(holdsImageAlone(inText));
    CHECK_FALSE(floatingImage(inText).has_value());
    CHECK_FALSE(holdsImageAlone(inCell));
    CHECK_FALSE(floatingImage(inCell).has_value());
    CHECK(holdsImageAlone(inLine));
    CHECK_FALSE(floatingImage(inLine).has_value());  // a part of its line

    CHECK(editor->canWrapImage(alone.position()));
    CHECK_FALSE(editor->canWrapImage(inText.position() + 5));
    CHECK_FALSE(editor->canWrapImage(inCell.position()));
    CHECK_FALSE(editor->canWrapImage(inText.position()));  // no image there

    // The image in the text keeps its wrap in KML
    CHECK(editor->toKml().count(QStringLiteral("wrap=\"left\"")) == 3);
}

// =============================================================================
// Beside the text
// =============================================================================

TEST_CASE("Image wrap: the next paragraph goes beside an image on the left, below it across",
          "[editor][images][wrap][layout]") {
    BookFolder book;
    book.addPicture(QStringLiteral("photo.png"), QSize(200, 200));
    auto editor =
        editorWith(QStringLiteral("<kml><p><img src=\"images/photo.png\" wrap=\"left\"/></p>") +
                       longParagraph(40) + QStringLiteral("</kml>"),
                   book.path());
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    REQUIRE(layout != nullptr);
    const QTextDocument& document = *editor->textDocument();
    layOutAll(document);
    const QTextBlock image = document.findBlockByNumber(0);
    const QTextBlock text = document.findBlockByNumber(1);
    const qreal textWidth = document.textWidth();

    // The image on the left, at the top of the paragraph, which takes no room in the flow
    const QRectF box = imageBox(*layout, image);
    CHECK(box.left() == Approx(0.0).margin(1.0));
    CHECK(box.width() < textWidth / 2.0);
    CHECK(layout->blockY(1) == Approx(layout->blockY(0)).margin(1.0));

    // Beside it the lines start after it and the room between; below it across the column.
    // A line whose box (its line and the room above it) still reaches the image is beside it:
    // below it only a line a line's height lower
    const QTextLayout* lines = text.layout();
    REQUIRE(lines->lineCount() > 10);
    int beside = 0;
    int below = 0;
    for (int i = 0; i < lines->lineCount(); ++i) {
        const QTextLine line = lines->lineAt(i);
        const qreal top = lineTop(*layout, text, i);
        INFO("line " << i);
        if (top + line.height() <= box.bottom()) {
            ++beside;
            CHECK(line.x() >= box.right() + 5.0);
            CHECK(line.x() + line.width() <= textWidth + 1.0);
        } else if (top >= box.bottom() + line.height()) {
            ++below;
            CHECK(line.width() > textWidth * 0.9);
            CHECK(line.x() < 1.0);
        }
    }
    CHECK(beside >= 3);
    CHECK(below >= 3);

    // A point on the image is the image's paragraph, beside it the text's
    CHECK(layout->blockNumberAt(box.center()) == 0);
    CHECK(layout->blockNumberAt(QPointF(box.right() + textWidth / 4.0, box.center().y())) == 1);

    // In the text again, its lines across
    REQUIRE(editor->setImageWrap(image.position(), ImageWrap::Inline));
    layOutAll(document);
    CHECK(layout->blockY(1) > layout->blockY(0) + box.height() - 1.0);
    CHECK(text.layout()->lineAt(1).x() < 1.0);
}

TEST_CASE("Image wrap: beside an image on the right the lines end before it",
          "[editor][images][wrap][layout]") {
    BookFolder book;
    book.addPicture(QStringLiteral("photo.png"), QSize(200, 200));
    auto editor =
        editorWith(QStringLiteral("<kml><p><img src=\"images/photo.png\" wrap=\"right\"/></p>") +
                       longParagraph(30) + QStringLiteral("</kml>"),
                   book.path());
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    const QTextDocument& document = *editor->textDocument();
    layOutAll(document);
    const QTextBlock text = document.findBlockByNumber(1);
    const qreal textWidth = document.textWidth();
    const QRectF box = imageBox(*layout, document.findBlockByNumber(0));
    CHECK(box.right() == Approx(textWidth).margin(1.0));

    const QTextLayout* lines = text.layout();
    REQUIRE(lines->lineCount() > 4);
    const QTextLine second = lines->lineAt(1);
    REQUIRE(lineTop(*layout, text, 1) + second.height() <= box.bottom());
    CHECK(second.x() < 1.0);
    CHECK(second.x() + second.width() <= box.left() - 5.0);
}

TEST_CASE("Image wrap: a table goes below an image beside the text",
          "[editor][images][wrap][layout]") {
    BookFolder book;
    book.addPicture(QStringLiteral("photo.png"), QSize(200, 200));
    auto editor = editorWith(
        QStringLiteral("<kml><p><img src=\"images/photo.png\" wrap=\"left\"/></p><p>Short.</p>"
                       "<table><tr><td><p>a</p></td><td><p>b</p></td></tr></table></kml>"),
        book.path());
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    const QTextDocument& document = *editor->textDocument();
    layOutAll(document);
    const QRectF box = imageBox(*layout, document.findBlockByNumber(0));
    CHECK(layout->blockY(2) >= box.bottom() - 1.0);
}

// =============================================================================
// Above and below the text, on the pages
// =============================================================================

TEST_CASE("Image wrap: an image above and below the text that does not fit goes to the next "
          "page, the text after it fills the page",
          "[editor][images][wrap][pages]") {
    BookFolder book;
    book.addPicture(QStringLiteral("tall.png"), QSize(500, 1100));
    const auto document = [&book](int fillers, const QString& wrap) {
        QString kml = QStringLiteral("<kml>");
        for (int i = 0; i < fillers; ++i) {
            kml += longParagraph(4);
        }
        kml += QStringLiteral("<p align=\"center\"><img src=\"images/tall.png\" width=\"50%\"") +
               wrap + QStringLiteral("/></p>") + longParagraph(60) + QStringLiteral("</kml>");
        auto editor = editorWith(kml, book.path());
        editor->setViewMode(ViewMode::Page);
        layOutAll(*editor->textDocument());
        return editor;
    };

    // As many paragraphs before it as fill more than a fifth of the page and at most 3/5
    int fillers = 1;
    qreal textHeight = 0.0;
    for (; fillers < 40; ++fillers) {
        auto probe = document(fillers, QString());
        const KalahariTextDocumentLayout* layout = layoutOf(*probe);
        textHeight = layout->pageFlow().textHeight;
        if (layout->blockY(fillers) > textHeight * 0.6) {
            --fillers;
            break;
        }
    }
    REQUIRE(fillers >= 1);
    REQUIRE(textHeight > 100.0);

    // In the text: the image and the text after it on the next page, room left empty
    auto inLine = document(fillers, QString());
    const KalahariTextDocumentLayout* lineLayout = layoutOf(*inLine);
    const qreal pitch = lineLayout->pageFlow().pitch;
    const QTextBlock lineAfter = inLine->textDocument()->findBlockByNumber(fillers + 1);
    const QRectF lineBox = imageBox(*lineLayout, lineAfter.previous());
    REQUIRE(lineBox.height() > textHeight - lineLayout->blockY(fillers));  // it does not fit
    CHECK(lineBox.top() >= pitch - 1.0);
    CHECK(lineTop(*lineLayout, lineAfter, 0) >= pitch);

    // Above and below the text: the image on the next page, the text after it before it
    auto wrapped = document(fillers, QStringLiteral(" wrap=\"top-bottom\""));
    const KalahariTextDocumentLayout* layout = layoutOf(*wrapped);
    const QTextBlock image = wrapped->textDocument()->findBlockByNumber(fillers);
    const QTextBlock after = image.next();
    const QRectF box = imageBox(*layout, image);
    CHECK(box.top() >= pitch - 1.0);
    CHECK(box.top() < pitch + textHeight * 0.1);
    const QTextBlock filler = image.previous();
    CHECK(lineTop(*layout, after, 0) < pitch);
    CHECK(lineTop(*layout, after, 0) >
          lineTop(*layout, filler, filler.layout()->lineCount() - 1));

    // Its lines on the next page go below the image
    const QTextLayout* lines = after.layout();
    int onNext = 0;
    for (int i = 0; i < lines->lineCount(); ++i) {
        const qreal top = lineTop(*layout, after, i);
        if (top >= pitch && top < 2.0 * pitch) {
            ++onNext;
            INFO("line " << i);
            CHECK(top >= box.bottom() - 1.0);
        }
    }
    CHECK(onNext > 0);

    // A point on the image is its paragraph
    CHECK(layout->blockNumberAt(box.center()) == fillers);
}

// =============================================================================
// Commands
// =============================================================================

TEST_CASE("Image wrap: Image > Wrap Text is one undo step, new images take the settings'",
          "[editor][images][wrap][editing]") {
    BookFolder book;
    book.addPicture(QStringLiteral("photo.png"), QSize(100, 100));
    auto editor = editorWith(
        QStringLiteral("<kml><p><img src=\"images/photo.png\"/></p>"
                       "<p>Text <img src=\"images/photo.png\"/></p><p></p></kml>"),
        book.path());
    const QTextDocument& document = *editor->textDocument();
    const int alone = document.findBlockByNumber(0).position();
    const int inText = document.findBlockByNumber(1).position() + 5;

    REQUIRE(editor->setImageWrap(alone, ImageWrap::SquareRight));
    CHECK(imageAt(document, alone)->wrap == ImageWrap::SquareRight);
    CHECK(editor->toKml().startsWith(
        QStringLiteral("<kml><p><img src=\"images/photo.png\" wrap=\"right\"/></p>")));
    REQUIRE(editor->selectedImage());
    CHECK(*editor->selectedImage() == alone);
    CHECK_FALSE(editor->setImageWrap(inText, ImageWrap::SquareLeft));
    CHECK_FALSE(editor->setImageWrap(inText, ImageWrap::Inline));  // already in its line
    editor->undo();
    CHECK(imageAt(document, alone)->wrap == ImageWrap::Inline);
    CHECK_FALSE(editor->toKml().contains(QStringLiteral("wrap=")));

    // A new image: as the settings say, unless it has a wrap of its own
    QTemporaryDir outside;
    QImage picture(QSize(40, 40), QImage::Format_RGB32);
    picture.fill(Qt::blue);
    const QString file = outside.path() + QStringLiteral("/new.png");
    REQUIRE(picture.save(file));
    BookEditor::NewImageOptions options = editor->newImageOptions();
    options.wrap = ImageWrap::TopBottom;
    editor->setNewImageOptions(options);
    editor->clearSelection();
    editor->setCursorPosition({2, 0});
    REQUIRE(editor->insertImageFile(file));
    REQUIRE(editor->selectedImage());
    CHECK(imageAt(document, *editor->selectedImage())->wrap == ImageWrap::TopBottom);
    CHECK(editor->toKml().contains(QStringLiteral("wrap=\"top-bottom\"")));
}
