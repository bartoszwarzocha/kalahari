/// @file test_paragraph_styles.cpp
/// @brief Paragraph styles and the objects of the text (a horizontal line, the scene and page
///        breaks) as the writer edits them
///
/// Enter at the end of a heading starts the text, a heading split in two stays two headings,
/// a style goes on the paragraphs at once as one undo step. An object is one character in a
/// block of its own: text never joins it, Backspace and Delete take it whole.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <kalahari/core/book_type_package.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/paragraph_styles.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QMimeData>
#include <QTextBlock>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// A style of a package
kalahari::core::PackageParagraphStyle packageStyle(const QString& id, const QString& next = {},
                                                   const QVariantMap& properties = {}) {
    kalahari::core::PackageParagraphStyle style;
    style.id = id;
    style.name.values.insert(QStringLiteral("en"), id);
    style.nextStyle = next;
    style.properties = properties;
    return style;
}

/// The styles of a book: Normal, a heading followed by Normal, Dialogue followed by itself,
/// Code without spell checking
ParagraphStyleSet bookStyles() {
    return ParagraphStyleSet::fromPackage(
        {packageStyle(QStringLiteral("normal")),
         packageStyle(QStringLiteral("heading_1"), QStringLiteral("normal"),
                      {{QStringLiteral("fontSize"), 20},
                       {QStringLiteral("alignment"), QStringLiteral("center")},
                       {QStringLiteral("keepWithNext"), true}}),
         packageStyle(QStringLiteral("heading_2"), QStringLiteral("normal")),
         packageStyle(QStringLiteral("dialogue")),
         packageStyle(QStringLiteral("code"), {}, {{QStringLiteral("spellCheck"), false}})},
        QStringLiteral("en"));
}

/// An editor with the book's styles and KML loaded, the cursor at @p cursor
std::unique_ptr<BookEditor> editorWith(const QString& kml, CursorPosition cursor = {0, 0}) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->setParagraphStyles(bookStyles());
    editor->fromKml(kml);
    editor->setCursorPosition(cursor);
    return editor;
}

/// Select from @p from to @p to
void select(BookEditor& editor, CursorPosition from, CursorPosition to) {
    editor.setCursorPosition(to);
    editor.setSelection({from, to});
}

void press(BookEditor& editor, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(&editor, &event);
}

/// Type the text as the keyboard does, a character at a time
void type(BookEditor& editor, const QString& text) {
    for (const QChar ch : text) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
        QCoreApplication::sendEvent(&editor, &event);
    }
}

QString kml(const QString& content) {
    return QStringLiteral("<kml>") + content + QStringLiteral("</kml>");
}

}  // namespace

// =============================================================================
// Styles
// =============================================================================

TEST_CASE("Paragraph styles: Enter", "[editor][styles]") {
    SECTION("At the end of a heading the text follows it") {
        auto editor = editorWith(kml(QStringLiteral("<h1>Title</h1>")), {0, 5});
        press(*editor, Qt::Key_Return);
        type(*editor, QStringLiteral("Text"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<h1>Title</h1><p>Text</p>")));
        REQUIRE(editor->currentParagraphStyle() == QStringLiteral("normal"));
    }

    SECTION("In the middle of a heading it splits into two headings") {
        auto editor = editorWith(kml(QStringLiteral("<h1>Title</h1>")), {0, 2});
        press(*editor, Qt::Key_Return);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<h1>Ti</h1><h1>tle</h1>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
    }

    SECTION("A style without a style after it goes on") {
        auto editor = editorWith(kml(QStringLiteral("<p style=\"dialogue\">A</p>")), {0, 1});
        press(*editor, Qt::Key_Return);
        type(*editor, QStringLiteral("B"));
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<p style=\"dialogue\">A</p><p style=\"dialogue\">B</p>")));
    }

    SECTION("The text after a heading has no alignment of the heading's") {
        auto editor = editorWith(kml(QStringLiteral("<h1 align=\"right\">Title</h1>")), {0, 5});
        press(*editor, Qt::Key_Return);
        type(*editor, QStringLiteral("Text"));
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<h1 align=\"right\">Title</h1><p>Text</p>")));
    }

    SECTION("One undo step takes the new paragraph back") {
        auto editor = editorWith(kml(QStringLiteral("<h1>Title</h1>")), {0, 5});
        press(*editor, Qt::Key_Return);
        editor->undo();
        REQUIRE(editor->toKml() == kml(QStringLiteral("<h1>Title</h1>")));
    }
}

TEST_CASE("Paragraph styles: putting paragraphs in a style", "[editor][styles]") {
    SECTION("The paragraph at the cursor") {
        auto editor = editorWith(kml(QStringLiteral("<p>A</p><p>B</p>")), {1, 0});
        editor->applyParagraphStyle(QStringLiteral("heading_2"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>A</p><h2>B</h2>")));
        REQUIRE(editor->currentParagraphStyle() == QStringLiteral("heading_2"));
    }

    SECTION("The selected paragraphs, the objects among them staying, in one undo step") {
        auto editor = editorWith(kml(QStringLiteral("<p>A</p><hr/><p>B</p><p>C</p>")));
        select(*editor, {0, 0}, {2, 1});
        editor->applyParagraphStyle(QStringLiteral("dialogue"));
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<p style=\"dialogue\">A</p><hr/>"
                                   "<p style=\"dialogue\">B</p><p>C</p>")));
        editor->undo();
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>A</p><hr/><p>B</p><p>C</p>")));
    }

    SECTION("Normal takes the style off") {
        auto editor = editorWith(kml(QStringLiteral("<p style=\"dialogue\">A</p>")));
        editor->applyParagraphStyle(QStringLiteral("normal"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>A</p>")));
    }

    SECTION("A paragraph already in the style: no undo step") {
        auto editor = editorWith(kml(QStringLiteral("<h1>A</h1>")));
        editor->applyParagraphStyle(QStringLiteral("heading_1"));
        REQUIRE_FALSE(editor->canUndo());
    }

    SECTION("The style at the cursor: also one the book does not have, none at an object") {
        auto editor = editorWith(kml(QStringLiteral("<p style=\"poem\">A</p><hr/>")));
        REQUIRE(editor->currentParagraphStyle() == QStringLiteral("poem"));
        editor->setCursorPosition({1, 0});
        REQUIRE(editor->currentParagraphStyle().isEmpty());
    }

    SECTION("The alignment of a heading's style is the paragraph's alignment") {
        auto editor = editorWith(kml(QStringLiteral("<h1>A</h1><p>B</p>")));
        REQUIRE(editor->currentAlignment() == Qt::AlignHCenter);
        editor->setAlignRight();
        REQUIRE(editor->currentAlignment() == Qt::AlignRight);
    }
}

TEST_CASE("Paragraph styles: a style the book does not have shows as Normal",
          "[editor][styles]") {
    auto editor = editorWith(kml(QStringLiteral("<p>A</p><p style=\"poem\">B</p>")));
    const auto* layout =
        qobject_cast<KalahariTextDocumentLayout*>(editor->textDocument()->documentLayout());
    REQUIRE(layout != nullptr);
    const QTextBlock normal = editor->textDocument()->findBlockByNumber(0);
    const QTextBlock unknown = editor->textDocument()->findBlockByNumber(1);
    REQUIRE(layout->blockMetrics(unknown).charFormat == layout->blockMetrics(normal).charFormat);
    REQUIRE(layout->blockMetrics(unknown).spaceBefore == layout->blockMetrics(normal).spaceBefore);
    REQUIRE(editor->toKml() == kml(QStringLiteral("<p>A</p><p style=\"poem\">B</p>")));
}

TEST_CASE("Paragraph styles: the spelling of Code is not checked", "[editor][styles]") {
    const ParagraphStyleSet styles = bookStyles();
    REQUIRE(styles.isChecked(QStringLiteral("normal")));
    REQUIRE(styles.isChecked(QStringLiteral("poem")));
    REQUIRE_FALSE(styles.isChecked(QStringLiteral("code")));
}

// =============================================================================
// Objects
// =============================================================================

TEST_CASE("Objects: inserting", "[editor][objects]") {
    SECTION("In the middle of a paragraph: it splits, the cursor goes to the text after") {
        auto editor = editorWith(kml(QStringLiteral("<p align=\"center\">ab<b>cd</b></p>")), {0, 2});
        editor->insertTextObject(TextObject::HorizontalRule);
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<p align=\"center\">ab</p><hr/>"
                                   "<p align=\"center\"><b>cd</b></p>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 0});
    }

    SECTION("At the end of a paragraph: a new one follows, in the style after it") {
        auto editor = editorWith(kml(QStringLiteral("<h1>Title</h1>")), {0, 5});
        editor->insertTextObject(TextObject::PageBreak);
        type(*editor, QStringLiteral("Text"));
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<h1>Title</h1><page-break/><p>Text</p>")));
    }

    SECTION("At the start of a paragraph: before it") {
        auto editor = editorWith(kml(QStringLiteral("<h2>abcd</h2>")));
        editor->insertTextObject(TextObject::SceneBreak);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<scene-break/><h2>abcd</h2>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
    }

    SECTION("In an empty paragraph: it stays after the object, with the cursor") {
        auto editor = editorWith(kml(QStringLiteral("<p>a</p><p></p><p>b</p>")), {1, 0});
        editor->insertTextObject(TextObject::SceneBreak);
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<p>a</p><scene-break/><p></p><p>b</p>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 0});
    }

    SECTION("Before and after another object") {
        auto editor = editorWith(kml(QStringLiteral("<hr/>")));
        editor->insertTextObject(TextObject::SceneBreak);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<scene-break/><hr/>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
        editor->setCursorPosition({1, 1});
        editor->insertTextObject(TextObject::PageBreak);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<scene-break/><hr/><page-break/>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 1});
    }

    SECTION("In place of the selection, one undo step") {
        auto editor = editorWith(kml(QStringLiteral("<p>abcdef</p>")));
        select(*editor, {0, 2}, {0, 4});
        editor->insertTextObject(TextObject::HorizontalRule);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>ab</p><hr/><p>ef</p>")));
        editor->undo();
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>abcdef</p>")));
    }
}

TEST_CASE("Objects: text never joins them", "[editor][objects]") {
    SECTION("Typed after an object: a new paragraph") {
        auto editor = editorWith(kml(QStringLiteral("<p>a</p><hr/>")), {1, 1});
        type(*editor, QStringLiteral("xy"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>a</p><hr/><p>xy</p>")));
    }

    SECTION("Typed before an object: a new paragraph before it") {
        auto editor = editorWith(kml(QStringLiteral("<h1>T</h1><hr/><p>b</p>")), {1, 0});
        type(*editor, QStringLiteral("x"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<h1>T</h1><p>x</p><hr/><p>b</p>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 1});
    }

    SECTION("Typed over a selection that ends before an object") {
        auto editor = editorWith(kml(QStringLiteral("<p>ab</p><hr/>")));
        select(*editor, {0, 0}, {1, 0});
        type(*editor, QStringLiteral("x"));
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>x</p><hr/>")));
    }

    SECTION("Enter before an object: an empty paragraph before it, the cursor stays") {
        auto editor = editorWith(kml(QStringLiteral("<p>a</p><hr/>")), {1, 0});
        press(*editor, Qt::Key_Return);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>a</p><p></p><hr/>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 0});
    }

    SECTION("Enter or Shift+Enter after an object: a new paragraph after it") {
        const auto modifiers = GENERATE(Qt::NoModifier, Qt::ShiftModifier);
        auto editor = editorWith(kml(QStringLiteral("<p>a</p><hr/>")), {1, 1});
        press(*editor, Qt::Key_Return, modifiers);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>a</p><hr/><p></p>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 0});
    }

    SECTION("An object pasted into a paragraph gets a block of its own") {
        auto editor = editorWith(kml(QStringLiteral("<p>abcd</p>")), {0, 2});
        QMimeData data;
        data.setData(QString::fromLatin1(MIME_KML),
                     kml(QStringLiteral("<p>x</p><hr/><p>y</p>")).toUtf8());
        editor->insertFromMimeData(&data);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>abx</p><hr/><p>ycd</p>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 1});

        QMimeData object;
        object.setData(QString::fromLatin1(MIME_KML), kml(QStringLiteral("<hr/>")).toUtf8());
        editor->setCursorPosition({0, 1});
        editor->insertFromMimeData(&object);
        REQUIRE(editor->toKml() ==
                kml(QStringLiteral("<p>a</p><hr/><p>bx</p><hr/><p>ycd</p>")));
        editor->undo();
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>abx</p><hr/><p>ycd</p>")));
    }

    SECTION("Pasted next to an object: a new paragraph") {
        auto editor = editorWith(kml(QStringLiteral("<hr/>")), {0, 1});
        QMimeData data;
        data.setText(QStringLiteral("text"));
        editor->insertFromMimeData(&data);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<hr/><p>text</p>")));
    }
}

TEST_CASE("Objects: deleting them as one character", "[editor][objects]") {
    const QString text = kml(QStringLiteral("<p>a</p><hr/><p>b</p>"));
    const QString without = kml(QStringLiteral("<p>a</p><p>b</p>"));

    SECTION("Backspace after the object") {
        auto editor = editorWith(text, {1, 1});
        press(*editor, Qt::Key_Backspace);
        REQUIRE(editor->toKml() == without);
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
        editor->undo();
        REQUIRE(editor->toKml() == text);
    }

    SECTION("Backspace at the start of the paragraph after it") {
        auto editor = editorWith(text, {2, 0});
        press(*editor, Qt::Key_Backspace);
        REQUIRE(editor->toKml() == without);
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
    }

    SECTION("Delete before the object") {
        auto editor = editorWith(text, {1, 0});
        press(*editor, Qt::Key_Delete);
        REQUIRE(editor->toKml() == without);
        REQUIRE(editor->cursorPosition() == CursorPosition{1, 0});
    }

    SECTION("Delete at the end of the paragraph before it") {
        auto editor = editorWith(text, {0, 1});
        press(*editor, Qt::Key_Delete);
        REQUIRE(editor->toKml() == without);
        REQUIRE(editor->cursorPosition() == CursorPosition{0, 1});
    }

    SECTION("Between an object and text the keys go over the paragraph break") {
        auto editor = editorWith(text, {1, 0});
        press(*editor, Qt::Key_Backspace);
        REQUIRE(editor->toKml() == text);
        REQUIRE(editor->cursorPosition() == CursorPosition{0, 1});
        editor->setCursorPosition({1, 1});
        press(*editor, Qt::Key_Delete);
        REQUIRE(editor->toKml() == text);
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 0});
    }

    SECTION("An empty paragraph next to an object goes before the object") {
        auto after = editorWith(kml(QStringLiteral("<hr/><p></p>")), {1, 0});
        press(*after, Qt::Key_Backspace);
        REQUIRE(after->toKml() == kml(QStringLiteral("<hr/>")));
        REQUIRE(after->cursorPosition() == CursorPosition{0, 1});

        auto before = editorWith(kml(QStringLiteral("<p></p><hr/>")));
        press(*before, Qt::Key_Delete);
        REQUIRE(before->toKml() == kml(QStringLiteral("<hr/>")));
        REQUIRE(before->cursorPosition() == CursorPosition{0, 0});

        auto fromObject = editorWith(kml(QStringLiteral("<p></p><hr/>")), {1, 0});
        press(*fromObject, Qt::Key_Backspace);
        REQUIRE(fromObject->toKml() == kml(QStringLiteral("<hr/>")));
        REQUIRE(fromObject->cursorPosition() == CursorPosition{0, 0});
    }

    SECTION("The only object of the text leaves an empty paragraph") {
        auto editor = editorWith(kml(QStringLiteral("<page-break/>")), {0, 1});
        press(*editor, Qt::Key_Backspace);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p></p>")));
    }
}

TEST_CASE("Objects: selections that end at them", "[editor][objects]") {
    const QString text = kml(QStringLiteral("<p>ab</p><hr/><h2>cd</h2>"));

    SECTION("Deleted from text to before the object: the object stays") {
        auto editor = editorWith(text);
        select(*editor, {0, 1}, {1, 0});
        press(*editor, Qt::Key_Delete);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>a</p><hr/><h2>cd</h2>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{0, 1});
    }

    SECTION("Deleted from after the object into text: the object stays") {
        auto editor = editorWith(text);
        select(*editor, {1, 1}, {2, 1});
        press(*editor, Qt::Key_Backspace);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>ab</p><hr/><h2>d</h2>")));
        REQUIRE(editor->cursorPosition() == CursorPosition{2, 0});
    }

    SECTION("Deleted from before the object: the text keeps its paragraph's style") {
        auto editor = editorWith(text);
        select(*editor, {1, 0}, {2, 1});
        press(*editor, Qt::Key_Delete);
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>ab</p><h2>d</h2>")));
    }

    SECTION("Cut from text to before the object: the object stays") {
        auto editor = editorWith(text);
        select(*editor, {0, 1}, {1, 0});
        editor->cut();
        REQUIRE(editor->toKml() == kml(QStringLiteral("<p>a</p><hr/><h2>cd</h2>")));
    }
}

TEST_CASE("Objects: selecting a word and a paragraph", "[editor][objects]") {
    auto editor = editorWith(kml(QStringLiteral("<p>one two</p>")), {0, 5});
    editor->selectWord();
    REQUIRE(editor->selectedText() == QStringLiteral("two"));
    editor->selectParagraph();
    REQUIRE(editor->selectedText() == QStringLiteral("one two"));
}
