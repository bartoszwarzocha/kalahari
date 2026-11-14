// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_text_captions.cpp
/// @brief Numbered captions of images, cross-references and the list of figures: KML, the
///        numbers, editing, the clipboard and the layout

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/editor_appearance.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/editor/text_captions.h>
#include <kalahari/editor/text_images.h>
#include "editor_test_utils.h"

#include <QDir>
#include <QImage>
#include <QMimeData>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextDocument>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// A book's main folder with two pictures in its folder of images
struct BookFolder {
    QTemporaryDir dir;

    BookFolder() {
        QDir(dir.path()).mkpath(QStringLiteral("images"));
        for (const char* name : {"a.png", "b.png"}) {
            QImage picture(QSize(40, 20), QImage::Format_RGB32);
            picture.fill(Qt::gray);
            REQUIRE(picture.save(dir.path() + QStringLiteral("/images/") + QLatin1String(name)));
        }
    }

    QString path() const { return dir.path(); }
};

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

/// KML with the ids of the numbers and the targets of the cross-references as "*": new ids are
/// random
QString withoutIds(QString kml) {
    static const QRegularExpression ids(QStringLiteral(" (id|to)=\"[^\"]*\""));
    return kml.replace(ids, QStringLiteral(" \\1=\"*\""));
}

/// The texts every field of the editor shows, in text order, parted by "|"
std::string shownFields(BookEditor& editor) {
    const QTextDocument& document = *editor.textDocument();
    QStringList texts;
    for (const QString& text :
         fieldTexts(document, editor.captionNumbers(), 0, document.characterCount())) {
        texts << text;
    }
    return texts.join(QLatin1Char('|')).toStdString();
}

/// The text of the first field of the editor
std::string firstField(BookEditor& editor) {
    const std::string fields = shownFields(editor);
    return fields.substr(0, fields.find('|'));
}

/// Whether a block is a caption: in the style Caption
bool isCaptionBlock(const QTextBlock& block) {
    return paragraphStyleOf(block.blockFormat()) == QLatin1String(CAPTION_STYLE_ID);
}

/// The position of the first image of the document
int firstImage(const QTextDocument& document) {
    const std::vector<int> images = imagePositions(document, 0, document.characterCount());
    REQUIRE_FALSE(images.empty());
    return images.front();
}

/// An image with a numbered caption under it, and one with a map above it
const QString TWO_IMAGES = QStringLiteral(
    "<kml><p>See <cross-reference to=\"f1\"/> and <cross-reference to=\"m1\" show=\"label\"/>.</p>"
    "<p align=\"center\"><img src=\"images/a.png\"/></p>"
    "<p style=\"caption\" align=\"center\">Figure <caption-number label=\"Figure\" id=\"f1\"/>. "
    "The bridge</p>"
    "<p style=\"caption\" caption=\"above\">Map <caption-number label=\"Map\" id=\"m1\"/>. The "
    "town</p>"
    "<p align=\"center\"><img src=\"images/b.png\"/></p>"
    "<p>As <cross-reference to=\"f1\" show=\"position\"/>, in <cross-reference to=\"f1\" "
    "show=\"caption\" link=\"false\"/> and <cross-reference to=\"gone\"/>.</p></kml>");

}  // namespace

// =============================================================================
// KML and the numbers
// =============================================================================

TEST_CASE("Captions: numbers and cross-references survive KML", "[editor][captions][kml]") {
    BookFolder book;
    auto editor = editorWith(TWO_IMAGES, book.path());
    CHECK(editor->toKml() == TWO_IMAGES);

    // Each a field: one character of its paragraph
    const QTextDocument& document = *editor->textDocument();
    CHECK(document.findBlockByNumber(2).text() ==
          QStringLiteral("Figure \uFFFC. The bridge"));
    CHECK(isCaptionAbove(document.findBlockByNumber(3).blockFormat()));

    // A number without a label and a cross-reference without a target are nothing; a number
    // without an id gets one
    auto loose = editorWith(QStringLiteral(
        "<kml><p>a<caption-number id=\"x\"/>b<cross-reference/>c<caption-number "
        "label=\"Figure\"/></p></kml>"));
    CHECK(withoutIds(loose->toKml()) ==
          QStringLiteral("<kml><p>abc<caption-number label=\"Figure\" id=\"*\"/></p></kml>"));
    REQUIRE(loose->captionEntries().size() == 1);
    CHECK_FALSE(loose->captionEntries().front().id.isEmpty());
}

TEST_CASE("Captions: each label is numbered on its own, the fields show their numbers",
          "[editor][captions][numbers]") {
    BookFolder book;
    auto editor = editorWith(TWO_IMAGES, book.path());
    CHECK(shownFields(*editor) == "1|Map 1|1|1|above|Figure 1. The bridge|?");

    // The book around the text: the captions before it count, the numbering of each label
    CaptionContext context;
    context.chapter = 2;
    context.before.push_back({QStringLiteral("e1"), QStringLiteral("Figure"), QStringLiteral("1"),
                              {}, QStringLiteral("chapter-1"), -1});
    context.before.push_back({QStringLiteral("e2"), QStringLiteral("Figure"), QStringLiteral("2"),
                              {}, QStringLiteral("chapter-1"), -1});
    CaptionNumbering roman;
    roman.format = ListNumberFormat::UpperRoman;
    context.numbering.insert(QStringLiteral("Map"), roman);
    editor->setCaptionContext(context);
    CHECK(shownFields(*editor) == "3|Map I|3|I|above|Figure 3. The bridge|?");

    // Anew in each chapter, with the chapter's number
    CaptionNumbering chapters;
    chapters.perChapter = true;
    chapters.withChapter = true;
    chapters.separator = QStringLiteral("–");
    context.numbering.insert(QStringLiteral("Figure"), chapters);
    editor->setCaptionContext(context);
    CHECK(firstField(*editor) == "2–1");

    // Counted by the book: the numbers it gives
    context.counted.insert(QStringLiteral("Figure"), 6);
    editor->setCaptionContext(context);
    CHECK(firstField(*editor) == "2–7");
    CHECK(editor->captionNumberAt(firstImage(*editor->textDocument()), QStringLiteral("Figure")) ==
          QStringLiteral("2–7"));

    // Numbers and texts in plain text
    CHECK(captionNumberText(4, CaptionNumbering{ListNumberFormat::LowerAlpha}, 0) == QStringLiteral("d"));
    CHECK(captionNumberText(3, chapters, 5) == QStringLiteral("5–3"));
    CHECK(captionNumberText(3, chapters, 0) == QStringLiteral("3"));
}

TEST_CASE("Captions: a new caption before others moves their numbers and cross-references",
          "[editor][captions][numbers]") {
    BookFolder book;
    const QString kml = QStringLiteral("<kml><p><img src=\"images/b.png\"/></p>") + TWO_IMAGES.mid(5);
    auto editor = editorWith(kml, book.path());
    CHECK(shownFields(*editor) == "1|Map 1|1|1|above|Figure 1. The bridge|?");

    // A figure above the first image: the bridge becomes figure 2, the cross-references show it
    ImageCaption caption{true, true, QStringLiteral("Figure"), QStringLiteral("The river"), true};
    REQUIRE(editor->setImageCaption(0, caption));
    CHECK(shownFields(*editor) == "1|2|Map 1|2|1|above|Figure 2. The bridge|?");
    CHECK(editor->captionNumbers().figureList().size() == 3);

    // Undone: as it was
    editor->undo();
    CHECK(editor->toKml() == kml);
    CHECK(firstField(*editor) == "1");
}

// =============================================================================
// Editing
// =============================================================================

TEST_CASE("Captions: Insert Caption gives an image its caption, below or above it",
          "[editor][captions][editing]") {
    BookFolder book;
    auto editor = editorWith(
        QStringLiteral("<kml><p>Text</p><p align=\"center\"><img src=\"images/a.png\"/></p></kml>"),
        book.path());
    const int image = firstImage(*editor->textDocument());

    // The image the commands act on: just before the cursor, in its paragraph
    editor->setCursorPosition({1, 1});
    REQUIRE(editor->captionTargetImage());
    CHECK(*editor->captionTargetImage() == image);
    editor->setCursorPosition({0, 2});
    CHECK_FALSE(editor->captionTargetImage());
    REQUIRE(editor->imageCaptionOf(image));
    CHECK_FALSE(editor->imageCaptionOf(image)->shown);
    CHECK(editor->captionNumberAt(image, QStringLiteral("Figure")) == QStringLiteral("1"));

    // Below, with its number, aligned as the image
    REQUIRE(editor->setImageCaption(
        image, {true, true, QStringLiteral("Figure"), QStringLiteral(" The  bridge\n"), false}));
    CHECK(withoutIds(editor->toKml()) ==
          QStringLiteral("<kml><p>Text</p><p align=\"center\"><img src=\"images/a.png\"/></p>"
                         "<p style=\"caption\" align=\"center\">Figure <caption-number "
                         "label=\"Figure\" id=\"*\"/>. The  bridge</p></kml>"));
    const QString id = editor->captionEntries().front().id;
    std::optional<ImageCaption> shown = editor->imageCaptionOf(image);
    REQUIRE(shown);
    CHECK(*shown == ImageCaption{true, true, QStringLiteral("Figure"), QStringLiteral("The  bridge"), false});
    CHECK(editor->cursorPosition() == CursorPosition{2, 21});

    // Above it: the paragraph moves, its number keeps its id
    REQUIRE(editor->setImageCaption(
        image, {true, true, QStringLiteral("Figure"), QStringLiteral("The  bridge"), true}));
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><p>Text</p><p style=\"caption\" align=\"center\" caption=\"above\">"
                         "Figure <caption-number label=\"Figure\" id=\"") +
              id + QStringLiteral("\"/>. The  bridge</p><p align=\"center\"><img "
                                  "src=\"images/a.png\"/></p></kml>"));
    int moved = firstImage(*editor->textDocument());
    REQUIRE(editor->imageCaptionOf(moved));
    CHECK(editor->imageCaptionOf(moved)->above);

    // Another label and text: the number keeps its id
    REQUIRE(editor->setImageCaption(moved, {true, true, QStringLiteral("Map"), QString(), true}));
    CHECK(editor->captionEntries().front().id == id);
    CHECK(editor->captionEntries().front().label == QStringLiteral("Map"));
    CHECK(editor->textDocument()->findBlockByNumber(1).text() == QStringLiteral("Map \uFFFC."));

    // Its text alone (the image moved with the shorter caption before it)
    moved = firstImage(*editor->textDocument());
    REQUIRE(editor->setImageCaption(moved, {true, false, QString(), QStringLiteral("Bridge"), true}));
    CHECK(editor->captionEntries().empty());
    CHECK(editor->textDocument()->findBlockByNumber(1).text() == QStringLiteral("Bridge"));

    // None: the paragraph goes
    moved = firstImage(*editor->textDocument());
    REQUIRE(editor->setImageCaption(moved, ImageCaption{}));
    CHECK(editor->toKml() == QStringLiteral(
                                 "<kml><p>Text</p><p align=\"center\"><img src=\"images/a.png\"/></p></kml>"));

    // Each change one undo step
    editor->undo();
    CHECK(editor->textDocument()->findBlockByNumber(1).text() == QStringLiteral("Bridge"));
    editor->undo();
    editor->undo();
    editor->undo();
    CHECK(editor->textDocument()->findBlockByNumber(2).text() == QStringLiteral("Figure \uFFFC. The  bridge"));
    CHECK(editor->captionEntries().front().id == id);
}

TEST_CASE("Captions: a caption's own formatting stays when only its label changes",
          "[editor][captions][editing]") {
    BookFolder book;
    auto editor = editorWith(QStringLiteral(
        "<kml><p><img src=\"images/a.png\"/></p><p style=\"caption\">Figure <caption-number "
        "label=\"Figure\" id=\"f1\"/>. The <i>Bridge</i></p></kml>"), book.path());
    REQUIRE(editor->setImageCaption(0, {true, true, QStringLiteral("Photo"), QStringLiteral("The Bridge"), false}));
    CHECK(editor->toKml() == QStringLiteral(
                                 "<kml><p><img src=\"images/a.png\"/></p><p style=\"caption\">Photo "
                                 "<caption-number label=\"Photo\" id=\"f1\"/>. The <i>Bridge</i></p></kml>"));
}

TEST_CASE("Captions: a new image gets a numbered caption in a paragraph of its own",
          "[editor][captions][editing]") {
    BookFolder book;
    QTemporaryDir outside;
    QImage picture(QSize(8, 8), QImage::Format_RGB32);
    picture.fill(Qt::red);
    const QString file = outside.path() + QStringLiteral("/photo.png");
    REQUIRE(picture.save(file));

    auto editor = editorWith(QStringLiteral("<kml><p>One two</p><p>Three</p></kml>"), book.path());
    BookEditor::NewImageOptions options;
    options.alignment = Qt::AlignRight;
    options.widthPercent = 50;
    options.captionLabel = QStringLiteral("Figure");
    editor->setNewImageOptions(options);
    editor->setCursorPosition({0, 3});
    REQUIRE(editor->insertImageFile(file));
    CHECK(withoutIds(editor->toKml()) ==
          QStringLiteral("<kml><p>One two</p><p align=\"right\"><img src=\"images/photo.png\" "
                         "width=\"50%\"/></p><p style=\"caption\" align=\"right\">Figure "
                         "<caption-number label=\"Figure\" id=\"*\"/>.</p><p>Three</p></kml>"));
    CHECK(editor->selectedImage() == 8);

    // The next one after the caption, not between the image and its caption
    editor->clearSelection();
    editor->setCursorPosition({1, 1});
    REQUIRE(editor->insertImageFile(file));
    const QTextDocument& document = *editor->textDocument();
    CHECK(document.blockCount() == 6);
    CHECK(isCaptionBlock(document.findBlockByNumber(2)));
    CHECK(hasImages(document.findBlockByNumber(3)));
    CHECK(isCaptionBlock(document.findBlockByNumber(4)));
    CHECK(shownFields(*editor) == "1|2");

    // One undo step each
    editor->undo();
    editor->undo();
    CHECK(editor->toKml() == QStringLiteral("<kml><p>One two</p><p>Three</p></kml>"));
}

TEST_CASE("Captions: an image deleted with its caption, after asking for its cross-references",
          "[editor][captions][editing]") {
    BookFolder book;
    auto editor = editorWith(TWO_IMAGES, book.path());
    const int image = firstImage(*editor->textDocument());
    CHECK(editor->crossReferenceCount({QStringLiteral("f1")}) == 3);

    QStringList asked;
    bool keep = true;
    editor->setCaptionRemovalConfirmation([&](const QStringList& ids) {
        asked = ids;
        return !keep;
    });

    // Kept: nothing goes
    editor->selectImage(image);
    editor->deleteSelectedText();
    CHECK(asked == QStringList({QStringLiteral("f1")}));
    CHECK(editor->toKml() == TWO_IMAGES);

    // Let go: the image and its caption, one undo step
    keep = false;
    editor->selectImage(image);
    editor->deleteSelectedText();
    const QTextDocument& document = *editor->textDocument();
    CHECK(document.blockCount() == 5);
    CHECK(document.findBlockByNumber(1).text().isEmpty());
    CHECK(firstField(*editor) == "?");
    editor->undo();
    CHECK(editor->toKml() == TWO_IMAGES);

    // Backspace after the image, Delete before it
    editor->clearSelection();
    editor->setCursorPosition({1, 1});
    editor->deleteBackward();
    CHECK(document.blockCount() == 5);
    editor->undo();
    editor->setCursorPosition({4, 0});
    editor->deleteForward();
    CHECK(document.blockCount() == 5);
    CHECK_FALSE(document.toPlainText().contains(QStringLiteral("town")));
    editor->undo();
    CHECK(editor->toKml() == TWO_IMAGES);
}

TEST_CASE("Captions: cross-references and the list of figures are inserted at the cursor",
          "[editor][captions][editing]") {
    BookFolder book;
    auto editor = editorWith(TWO_IMAGES, book.path());

    editor->setCursorPosition({0, 0});
    REQUIRE(editor->insertCrossReference({QStringLiteral("m1"), ReferenceShows::Number, true}));
    CHECK(editor->toKml().startsWith(
        QStringLiteral("<kml><p><cross-reference to=\"m1\"/>See <cross-reference to=\"f1\"/>")));
    CHECK(editor->cursorPosition() == CursorPosition{0, 1});
    CHECK(firstField(*editor) == "1");
    editor->undo();
    CHECK(editor->toKml() == TWO_IMAGES);

    // The list at the end, with its heading; none after a heading
    editor->setCursorPosition({5, 3});
    REQUIRE(editor->insertFigureList(QStringLiteral("List of Figures")));
    CHECK(editor->toKml().endsWith(QStringLiteral(
        "<cross-reference to=\"gone\"/>.</p><h2>List of Figures</h2><list-of-figures/><p></p></kml>")));
    const std::vector<FigureListRow> rows = editor->captionNumbers().figureList();
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].text == QStringLiteral("Figure 1. The bridge"));
    CHECK(rows[0].numberStart == 7);
    CHECK(rows[1].text == QStringLiteral("Map 1. The town"));
    editor->undo();
    CHECK(editor->toKml() == TWO_IMAGES);

    auto headed = editorWith(QStringLiteral("<kml><h1>Figures</h1><p></p></kml>"));
    headed->setCursorPosition({1, 0});
    REQUIRE(headed->insertFigureList(QStringLiteral("List of Figures")));
    CHECK(headed->toKml() == QStringLiteral("<kml><h1>Figures</h1><list-of-figures/><p></p></kml>"));
}

// =============================================================================
// The clipboard
// =============================================================================

TEST_CASE("Captions: copied, fields are their text; pasted, a copy gets a number of its own",
          "[editor][captions][clipboard]") {
    BookFolder book;
    auto editor = editorWith(TWO_IMAGES, book.path());

    // Plain text and HTML with the numbers the editor shows
    editor->setSelection({{0, 0}, {0, 11}});
    std::unique_ptr<QMimeData> data = editor->createMimeDataFromSelection();
    REQUIRE(data);
    CHECK(data->text() == QStringLiteral("See 1 and Map 1"));
    CHECK(data->html().contains(QStringLiteral("Map 1")));

    // A copy of a caption: a new id
    editor->setSelection({{2, 0}, {2, 20}});
    data = editor->createMimeDataFromSelection();
    editor->clearSelection();
    editor->setCursorPosition({5, 0});
    editor->insertFromMimeData(data.get());
    std::vector<CaptionEntry> entries = editor->captionEntries();
    REQUIRE(entries.size() == 3);
    CHECK(entries[0].id == QStringLiteral("f1"));
    CHECK(entries[2].label == QStringLiteral("Figure"));
    CHECK(entries[2].id != QStringLiteral("f1"));
    CHECK(entries[2].id != QStringLiteral("m1"));
    editor->undo();
    CHECK(editor->toKml() == TWO_IMAGES);

    // Cut and pasted: its id stays, its cross-references with it
    editor->setSelection({{2, 0}, {2, 20}});
    data = editor->createMimeDataFromSelection();
    editor->deleteSelectedText();
    editor->clearSelection();
    editor->setCursorPosition({5, 0});
    editor->insertFromMimeData(data.get());
    entries = editor->captionEntries();
    REQUIRE(entries.size() == 2);
    CHECK(entries[1].id == QStringLiteral("f1"));
    CHECK(entries[1].label == QStringLiteral("Figure"));
    CHECK(firstField(*editor) == "1");
}

TEST_CASE("Captions: the list of figures copied as its lines", "[editor][captions][clipboard]") {
    BookFolder book;
    auto editor = editorWith(QStringLiteral("<kml><list-of-figures/>") + TWO_IMAGES.mid(5), book.path());
    editor->setSelection({{0, 0}, {1, 3}});
    const std::unique_ptr<QMimeData> data = editor->createMimeDataFromSelection();
    REQUIRE(data);
    CHECK(data->text().startsWith(QStringLiteral("Figure 1. The bridge\nMap 1. The town\nSee")));
}

// =============================================================================
// The layout
// =============================================================================

TEST_CASE("Captions: the list of figures has a line for each caption", "[editor][captions][layout]") {
    BookFolder book;
    auto editor = editorWith(QStringLiteral("<kml><list-of-figures/>") + TWO_IMAGES.mid(5), book.path());
    KalahariTextDocumentLayout* layout = layoutOf(*editor);
    REQUIRE(layout);
    const std::optional<QRectF> rect = layout->figureListRect(0);
    REQUIRE(rect);
    CHECK(rect->height() > 0);

    const std::optional<FigureListRow> first = layout->figureListRowAt(0, QPointF(5, 2));
    REQUIRE(first);
    CHECK(first->id == QStringLiteral("f1"));
    const std::optional<FigureListRow> second =
        layout->figureListRowAt(0, QPointF(5, rect->height() - 2));
    REQUIRE(second);
    CHECK(second->id == QStringLiteral("m1"));

    // A new caption: a line more
    const qreal before = rect->height();
    editor->setCursorPosition({1, 0});
    REQUIRE(editor->insertImageFile(book.path() + QStringLiteral("/images/b.png")));
    REQUIRE(editor->selectedImage());
    REQUIRE(editor->setImageCaption(*editor->selectedImage(),
                                    {true, true, QStringLiteral("Figure"), QStringLiteral("River"), false}));
    CHECK(editor->captionNumbers().figureList().size() == 3);
    const std::optional<QRectF> longer = layout->figureListRect(0);
    REQUIRE(longer);
    CHECK(longer->height() > before);

    // Without captions: a line that says so
    auto empty = editorWith(QStringLiteral("<kml><list-of-figures/><p>a</p></kml>"));
    const std::optional<QRectF> none = layoutOf(*empty)->figureListRect(0);
    REQUIRE(none);
    CHECK(none->height() > 0);
    CHECK_FALSE(layoutOf(*empty)->figureListRowAt(0, QPointF(5, 2)));
}

TEST_CASE("Captions: the editor announces a change of its captions", "[editor][captions]") {
    BookFolder book;
    auto editor = editorWith(TWO_IMAGES, book.path());
    int changes = 0;
    QObject::connect(editor.get(), &BookEditor::captionsChanged, [&changes] { ++changes; });
    runEventLoop(10);
    changes = 0;

    // Text elsewhere: no change
    editor->setCursorPosition({0, 0});
    editor->insertText(QStringLiteral("x"));
    runEventLoop(10);
    CHECK(changes == 0);

    // The text of a caption: a change
    editor->setCursorPosition({2, 20});
    editor->insertText(QStringLiteral("s"));
    runEventLoop(10);
    CHECK(changes == 1);
}
