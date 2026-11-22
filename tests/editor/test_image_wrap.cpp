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
    // A line whose box (its line and the room above it) still reaches the image or the room
    // below it is beside it: once a line is across, so are the lines after it, and a line
    // three lines' height below the image is across in any case (with any font)
    const QTextLayout* lines = text.layout();
    REQUIRE(lines->lineCount() > 10);
    int beside = 0;
    int below = 0;
    bool across = false;
    for (int i = 0; i < lines->lineCount(); ++i) {
        const QTextLine line = lines->lineAt(i);
        const qreal top = lineTop(*layout, text, i);
        INFO("line " << i);
        if (top + line.height() <= box.bottom()) {
            ++beside;
            CHECK(line.x() >= box.right() + 5.0);
            CHECK(line.x() + line.width() <= textWidth + 1.0);
        } else if (across || top >= box.bottom() + 3.0 * line.height()) {
            ++below;
            CHECK(line.width() > textWidth * 0.9);
            CHECK(line.x() < 1.0);
        }
        across = across || line.x() < 1.0;
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

TEST_CASE("Image wrap: no line goes on an image above and below the text higher than the "
          "page, with the lines of a paragraph kept together",
          "[editor][images][wrap][pages]") {
    // The text after the image fills the rest of its page and goes on after the image's
    // page; the two last lines of a paragraph kept together went to the top of the image's
    // page, on the image
    BookFolder book;
    book.addPicture(QStringLiteral("tall.png"), QSize(500, 3000));
    int checked = 0;
    for (int sentences = 2; sentences <= 26; ++sentences) {
        const QString kml = QStringLiteral("<kml>") + longParagraph(7) + longParagraph(7) +
                            QStringLiteral("<p><img src=\"images/tall.png\" width=\"50%\" "
                                           "wrap=\"top-bottom\"/></p>") +
                            longParagraph(sentences) + longParagraph(sentences) +
                            longParagraph(6) + QStringLiteral("</kml>");
        auto editor = editorWith(kml, book.path());
        editor->setViewMode(ViewMode::Page);
        layOutAll(*editor->textDocument());
        const KalahariTextDocumentLayout* layout = layoutOf(*editor);
        const qreal pitch = layout->pageFlow().pitch;
        const QTextBlock image = editor->textDocument()->findBlockByNumber(2);
        const QRectF box = imageBox(*layout, image);
        REQUIRE(box.top() >= pitch - 1.0);  // on the next page
        for (QTextBlock block = image.next(); block.isValid(); block = block.next()) {
            for (int i = 0; i < block.layout()->lineCount(); ++i) {
                const qreal top = lineTop(*layout, block, i);
                INFO("sentences " << sentences << ", block " << block.blockNumber() << ", line "
                                  << i << " at " << top << ", the image " << box.top() << "-"
                                  << box.bottom());
                CHECK_FALSE((top >= box.top() - 1.0 && top < box.bottom() - 1.0));
                ++checked;
            }
        }
    }
    CHECK(checked > 0);
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

// =============================================================================
// The room around an image
// =============================================================================

TEST_CASE("Image space: KML and the format keep the room around an image unless the default",
          "[editor][images][wrap]") {
    const ImageProperties plain = imageOfElement(QStringLiteral("<img src=\"images/a.png\"/>"));
    CHECK(plain.spaceAbove == Approx(DEFAULT_IMAGE_SPACE_ABOVE));
    CHECK(plain.spaceBelow == Approx(DEFAULT_IMAGE_SPACE_BELOW));
    CHECK(plain.spaceBeside == Approx(DEFAULT_IMAGE_SPACE_BESIDE));
    CHECK(imageKml(plain) == QStringLiteral("<img src=\"images/a.png\"/>"));

    const ImageProperties own = imageOfElement(QStringLiteral(
        "<img src=\"images/a.png\" space-above=\"12pt\" space-below=\"0pt\" space-beside=\"1cm\"/>"));
    CHECK(own.spaceAbove == Approx(12.0));
    CHECK(own.spaceBelow == Approx(0.0));
    CHECK(own.spaceBeside == Approx(28.35).margin(0.01));
    const ImageProperties again = imageOfElement(imageKml(own));
    CHECK(again == own);

    // Clamped: no less than nothing, no more than an inch
    ImageProperties wide = plain;
    wide.spaceAbove = -4.0;
    wide.spaceBelow = 500.0;
    CHECK(normalizedImage(wide).spaceAbove == Approx(0.0));
    CHECK(normalizedImage(wide).spaceBelow == Approx(MAX_IMAGE_SPACE));

    // In the format of its character only when it is not the default
    QTextCharFormat format = imageCharFormat(plain);
    CHECK_FALSE(format.hasProperty(IMAGE_SPACE_ABOVE_PROPERTY));
    setImage(format, own);
    CHECK(format.doubleProperty(IMAGE_SPACE_ABOVE_PROPERTY) == Approx(12.0));
    CHECK(imageOf(format)->spaceBelow == Approx(0.0));
    clearImage(format);
    CHECK_FALSE(format.hasProperty(IMAGE_SPACE_BESIDE_PROPERTY));
}

TEST_CASE("Image space: the room is around the image and its caption, outside the caption",
          "[editor][images][wrap]") {
    BookFolder book;
    book.addPicture(QStringLiteral("photo.png"), QSize(100, 100));
    auto editor = editorWith(
        QStringLiteral("<kml><p>Text.</p>"
                       "<p><img src=\"images/photo.png\" space-above=\"10pt\" space-below=\"8pt\"/></p>"
                       "<p><img src=\"images/photo.png\" space-above=\"10pt\" space-below=\"8pt\"/></p>"
                       "<p style=\"caption\">Below it</p>"
                       "<p style=\"caption\" caption=\"above\">Above it</p>"
                       "<p><img src=\"images/photo.png\" space-above=\"10pt\" space-below=\"8pt\"/></p>"
                       "<p>Text <img src=\"images/photo.png\" space-above=\"10pt\"/></p></kml>"),
        book.path());
    const QTextDocument& document = *editor->textDocument();
    REQUIRE(document.blockCount() == 7);
    const auto spaceOf = [&document](int block) {
        return imageBlockSpace(document.findBlockByNumber(block));
    };
    CHECK(spaceOf(0) == BlockSpace{});
    CHECK(spaceOf(1) == BlockSpace{10.0, 8.0});  // alone
    REQUIRE(captionBlockOf(document.findBlockByNumber(2)).blockNumber() == 3);
    CHECK(spaceOf(2) == BlockSpace{10.0, 0.0});  // its caption below it
    CHECK(spaceOf(3) == BlockSpace{0.0, 8.0});   // the caption: below it
    REQUIRE(captionBlockOf(document.findBlockByNumber(5)).blockNumber() == 4);
    CHECK(spaceOf(4) == BlockSpace{10.0, 0.0});  // the caption above: above it
    CHECK(spaceOf(5) == BlockSpace{0.0, 8.0});
    CHECK(spaceOf(6) == BlockSpace{});  // a part of a line of text
}

TEST_CASE("Image space: the layout leaves the room above and below an image and beside it",
          "[editor][images][wrap][layout]") {
    BookFolder book;
    book.addPicture(QStringLiteral("photo.png"), QSize(120, 120));
    const auto imageTop = [&book](const QString& space) {
        auto editor = editorWith(QStringLiteral("<kml><p>Text above.</p><p><img src=\"images/photo.png\" ") +
                                     space + QStringLiteral("/></p><p>Text below.</p></kml>"),
                                 book.path());
        KalahariTextDocumentLayout* layout = layoutOf(*editor);
        const QTextDocument& document = *editor->textDocument();
        layOutAll(document);
        const QRectF box = imageBox(*layout, document.findBlockByNumber(1));
        const qreal below = layout->blockY(2) - box.bottom();
        return std::pair<qreal, qreal>(box.top() - layout->blockY(1), below);
    };
    const auto [noneAbove, noneBelow] = imageTop(QStringLiteral("space-above=\"0pt\" space-below=\"0pt\""));
    const auto [someAbove, someBelow] = imageTop(QStringLiteral("space-above=\"12pt\" space-below=\"12pt\""));
    const auto [moreAbove, moreBelow] = imageTop(QStringLiteral("space-above=\"24pt\" space-below=\"6pt\""));
    CHECK(someAbove - noneAbove > 8.0);
    CHECK(moreAbove - noneAbove == Approx(2.0 * (someAbove - noneAbove)).margin(1.0));
    CHECK(someBelow - noneBelow == Approx(someAbove - noneAbove).margin(1.0));
    CHECK(moreBelow - noneBelow == Approx((someBelow - noneBelow) / 2.0).margin(1.0));

    // Beside the text: the lines keep the room beside it from the image
    const auto gapBeside = [&book](const QString& space) {
        auto editor = editorWith(QStringLiteral("<kml><p><img src=\"images/photo.png\" wrap=\"left\" ") +
                                     space + QStringLiteral("/></p>") + longParagraph(20) +
                                     QStringLiteral("</kml>"),
                                 book.path());
        KalahariTextDocumentLayout* layout = layoutOf(*editor);
        const QTextDocument& document = *editor->textDocument();
        layOutAll(document);
        const QRectF box = imageBox(*layout, document.findBlockByNumber(0));
        return document.findBlockByNumber(1).layout()->lineAt(1).x() - box.right();
    };
    const qreal narrow = gapBeside(QStringLiteral("space-beside=\"4pt\""));
    const qreal wide = gapBeside(QStringLiteral("space-beside=\"28pt\""));
    CHECK(narrow > 2.0);
    CHECK(wide - narrow > 20.0);
}

TEST_CASE("Image space: a caption added or taken away moves the room of its image",
          "[editor][images][wrap][layout]") {
    BookFolder book;
    book.addPicture(QStringLiteral("photo.png"), QSize(100, 100));
    auto editor = editorWith(
        QStringLiteral("<kml><p><img src=\"images/photo.png\" space-above=\"0pt\" space-below=\"30pt\"/></p>"
                       "<p style=\"caption\">The well</p><p>Text.</p></kml>"),
        book.path());
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    QTextDocument& document = *editor->textDocument();
    layOutAll(document);
    // With the caption: close under the image, the room under the caption
    const qreal captionGap = layout->blockY(1) - imageBox(*layout, document.findBlockByNumber(0)).bottom();
    const qreal textGap = layout->blockY(2) - layout->blockY(1);
    // Without it (the caption's paragraph taken away): the room under the image
    QTextCursor cursor(&document);
    cursor.setPosition(document.findBlockByNumber(1).position());
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    REQUIRE(document.blockCount() == 2);
    layOutAll(document);
    const qreal imageGap = layout->blockY(1) - imageBox(*layout, document.findBlockByNumber(0)).bottom();
    CHECK(imageGap > captionGap + 20.0);
    CHECK(textGap > 20.0);
}

TEST_CASE("Image space: a new image keeps the room of the settings unless it has its own",
          "[editor][images][wrap][editing]") {
    BookFolder book;
    auto editor = editorWith(QStringLiteral("<kml><p></p></kml>"), book.path());
    QTemporaryDir outside;
    QImage picture(QSize(40, 40), QImage::Format_RGB32);
    picture.fill(Qt::blue);
    const QString file = outside.path() + QStringLiteral("/new.png");
    REQUIRE(picture.save(file));
    BookEditor::NewImageOptions options = editor->newImageOptions();
    options.spaceAbove = 14.0;
    options.spaceBelow = 2.0;
    options.spaceBeside = 20.0;
    editor->setNewImageOptions(options);
    editor->setCursorPosition({0, 0});
    REQUIRE(editor->insertImageFile(file));
    REQUIRE(editor->selectedImage());
    const std::optional<ImageProperties> image = imageAt(*editor->textDocument(), *editor->selectedImage());
    REQUIRE(image);
    CHECK(image->spaceAbove == Approx(14.0));
    CHECK(image->spaceBelow == Approx(2.0));
    CHECK(image->spaceBeside == Approx(20.0));
    CHECK(editor->toKml().contains(QStringLiteral("space-above=\"14pt\"")));
}
