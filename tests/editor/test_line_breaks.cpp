/// @file test_line_breaks.cpp
/// @brief A line break in a paragraph (Shift+Enter): typing it, moving over it, and the
///        chapter files, the clipboard and the other programs it goes to
///
/// As in Word: the break starts a new line in the same paragraph, which keeps its format
/// and spacing. The text holds it as QChar::LineSeparator (U+2028), a chapter file as <br/>.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <kalahari/core/chapter_document.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/book_editor_accessible.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/find_replace_bar.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/search_engine.h>
#include <kalahari/editor/spell_check_service.h>
#include <kalahari/editor/tag_detector.h>
#include <kalahari/editor/text_keys.h>
#include "editor_test_utils.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QMimeData>
#include <QTextBlock>
#include <QTextLayout>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;
using kalahari::core::ChapterDocument;

namespace {

using Action = TextKeyAction;

const QChar LINE_BREAK(QChar::LineSeparator);

/// An editor with KML loaded
std::unique_ptr<BookEditor> editorWith(const QString& kml) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(kml);
    return editor;
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

QTextLayout* layoutOf(BookEditor& editor, int paragraph) {
    return KalahariTextDocumentLayout::blockLayout(
        editor.textDocument()->findBlockByNumber(paragraph));
}

/// The line of the layout a position is on
int lineOf(BookEditor& editor, const CursorPosition& position) {
    return layoutOf(editor, position.paragraph)->lineForTextPosition(position.offset).lineNumber();
}

QString kmlData(const QMimeData& mimeData) {
    return QString::fromUtf8(mimeData.data(QString::fromLatin1(MIME_KML)));
}

}  // anonymous namespace

// =============================================================================
// Typing
// =============================================================================

TEST_CASE("Line breaks: Shift+Enter starts a new line in the paragraph", "[editor][linebreak]") {
    auto editor = editorWith(
        QStringLiteral("<kml><p align=\"center\">Roses are red, violets are blue</p></kml>"));
    editor->setSelection({{0, 14}, {0, 15}});  // the space after the comma

    SECTION("Shift+Enter of the main keyboard") {
        press(*editor, Qt::Key_Return, Qt::ShiftModifier);
    }
    SECTION("Shift+Enter of the keypad") {
        press(*editor, Qt::Key_Enter, Qt::ShiftModifier | Qt::KeypadModifier);
    }

    // The break replaced the selection; the paragraph is one, with its alignment
    CHECK(editor->paragraphCount() == 1);
    CHECK(editor->paragraphPlainText(0) ==
          QStringLiteral("Roses are red,") + LINE_BREAK + QStringLiteral("violets are blue"));
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><p align=\"center\">Roses are red,<br/>violets are blue</p></kml>"));

    // The cursor is at the start of the new line
    CHECK_FALSE(editor->hasSelection());
    CHECK(editor->cursorPosition() == CursorPosition{0, 15});
    CHECK(layoutOf(*editor, 0)->lineCount() == 2);
    CHECK(lineOf(*editor, editor->cursorPosition()) == 1);
}

TEST_CASE("Line breaks: Enter still starts a new paragraph", "[editor][linebreak]") {
    auto editor = editorWith(kmlOf({QStringLiteral("One two")}));
    editor->setCursorPosition({0, 3});
    press(*editor, Qt::Key_Return);
    CHECK(editor->paragraphCount() == 2);
    press(*editor, Qt::Key_Return, Qt::ControlModifier);
    CHECK(editor->paragraphCount() == 3);
}

TEST_CASE("Line breaks: a break at the end of a paragraph starts an empty line",
          "[editor][linebreak]") {
    auto editor = editorWith(kmlOf({QStringLiteral("Last line")}));
    editor->setCursorPosition({0, 9});
    press(*editor, Qt::Key_Return, Qt::ShiftModifier);

    CHECK(editor->cursorPosition() == CursorPosition{0, 10});
    CHECK(layoutOf(*editor, 0)->lineCount() == 2);
    CHECK(lineOf(*editor, editor->cursorPosition()) == 1);
    CHECK(editor->toKml() == QStringLiteral("<kml><p>Last line<br/></p></kml>"));
}

TEST_CASE("Line breaks: the break and the text typed after it are one undo step",
          "[editor][linebreak]") {
    auto editor = editorWith(kmlOf({QString()}));
    type(*editor, QStringLiteral("abc"));
    press(*editor, Qt::Key_Return, Qt::ShiftModifier);
    type(*editor, QStringLiteral("def"));
    const QString typed = QStringLiteral("abc") + LINE_BREAK + QStringLiteral("def");
    REQUIRE(editor->paragraphPlainText(0) == typed);

    editor->undo();
    CHECK(editor->paragraphPlainText(0) == QStringLiteral("abc"));
    CHECK(editor->cursorPosition() == CursorPosition{0, 3});
    editor->undo();
    CHECK(editor->paragraphPlainText(0).isEmpty());

    editor->redo();
    editor->redo();
    CHECK(editor->paragraphPlainText(0) == typed);
}

TEST_CASE("Line breaks: the new line has the format of the place", "[editor][linebreak]") {
    SECTION("Inside bold text the break and the text after it are bold") {
        auto editor = editorWith(QStringLiteral("<kml><p>A <b>bold</b> word</p></kml>"));
        editor->setCursorPosition({0, 4});  // A bo|ld
        press(*editor, Qt::Key_Return, Qt::ShiftModifier);
        type(*editor, QStringLiteral("x"));
        CHECK(editor->toKml() == QStringLiteral("<kml><p>A <b>bo<br/>xld</b> word</p></kml>"));
    }

    SECTION("A format given without a selection goes on the text typed on the new line") {
        auto editor = editorWith(kmlOf({QStringLiteral("One")}));
        editor->setCursorPosition({0, 3});
        editor->toggleItalic();
        press(*editor, Qt::Key_Return, Qt::ShiftModifier);
        CHECK(editor->isItalic());
        type(*editor, QStringLiteral("Two"));
        CHECK(editor->toKml() == QStringLiteral("<kml><p>One<i><br/>Two</i></p></kml>"));
    }
}

// =============================================================================
// Moving and deleting
// =============================================================================

TEST_CASE("Line breaks: the keys of the cursor go over a break as over a line end",
          "[editor][linebreak]") {
    // "One two", the break at 7, "three four" from 8 to 18
    auto editor =
        editorWith(QStringLiteral("<kml><p>One two<br/>three four</p><p>Next</p></kml>"));
    const auto lineText = [&editor] {
        return editor->paragraphPlainText(0);
    };

    SECTION("Ctrl+Right stops at the end of the line, then goes over the break") {
        editor->setCursorPosition({0, 4});  // One |two
        editor->performTextKey(Action::WordRight);
        CHECK(editor->cursorPosition() == CursorPosition{0, 7});
        editor->performTextKey(Action::WordRight);
        CHECK(editor->cursorPosition() == CursorPosition{0, 8});
        editor->performTextKey(Action::WordRight);
        CHECK(editor->cursorPosition() == CursorPosition{0, 14});
    }

    SECTION("Ctrl+Left stops at the start of the line, then goes over the break") {
        editor->setCursorPosition({0, 10});  // th|ree
        editor->performTextKey(Action::WordLeft);
        CHECK(editor->cursorPosition() == CursorPosition{0, 8});
        editor->performTextKey(Action::WordLeft);
        CHECK(editor->cursorPosition() == CursorPosition{0, 7});
        editor->performTextKey(Action::WordLeft);
        CHECK(editor->cursorPosition() == CursorPosition{0, 4});
    }

    SECTION("Ctrl+Shift+Right selects the last word of a line without the break") {
        editor->setCursorPosition({0, 4});
        editor->performTextKey(Action::WordRight, true);
        CHECK(editor->selectedText() == QStringLiteral("two"));
    }

    SECTION("Ctrl+Backspace at the start of a line joins it to the line before") {
        editor->setCursorPosition({0, 8});
        editor->performTextKey(Action::DeleteWordBackward);
        CHECK(lineText() == QStringLiteral("One twothree four"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 7});
    }

    SECTION("Ctrl+Delete in the last word of a line leaves the break") {
        editor->setCursorPosition({0, 5});  // One t|wo
        editor->performTextKey(Action::DeleteWordForward);
        CHECK(lineText() == QStringLiteral("One t") + LINE_BREAK + QStringLiteral("three four"));
        editor->performTextKey(Action::DeleteWordForward);  // at the end of the line
        CHECK(lineText() == QStringLiteral("One tthree four"));
    }

    SECTION("Backspace and Delete delete the break as a character") {
        editor->setCursorPosition({0, 8});
        editor->performTextKey(Action::DeleteBackward);
        CHECK(lineText() == QStringLiteral("One twothree four"));
        editor->undo();
        editor->setCursorPosition({0, 7});
        editor->performTextKey(Action::DeleteForward);
        CHECK(lineText() == QStringLiteral("One twothree four"));
        CHECK(editor->paragraphCount() == 2);
    }

    SECTION("Home and End go to the ends of the line") {
        editor->setCursorPosition({0, 10});
        editor->performTextKey(Action::LineStart);
        CHECK(editor->cursorPosition() == CursorPosition{0, 8});
        editor->performTextKey(Action::LineEnd);
        CHECK(editor->cursorPosition() == CursorPosition{0, 18});

        editor->setCursorPosition({0, 2});
        editor->performTextKey(Action::LineEnd);
        CHECK(editor->cursorPosition() == CursorPosition{0, 7});  // before the break
        CHECK(lineOf(*editor, editor->cursorPosition()) == 0);
        editor->performTextKey(Action::LineStart);
        CHECK(editor->cursorPosition() == CursorPosition{0, 0});
    }

    SECTION("Right and Left go over the break to the other line") {
        editor->setCursorPosition({0, 7});
        editor->performTextKey(Action::CharacterRight);
        CHECK(editor->cursorPosition() == CursorPosition{0, 8});
        CHECK(lineOf(*editor, editor->cursorPosition()) == 1);
        editor->performTextKey(Action::CharacterLeft);
        CHECK(editor->cursorPosition() == CursorPosition{0, 7});
        CHECK(lineOf(*editor, editor->cursorPosition()) == 0);
    }

    SECTION("Down and Up go between the lines of the paragraph") {
        editor->setCursorPosition({0, 2});
        editor->performTextKey(Action::LineDown);
        CHECK(editor->cursorPosition().paragraph == 0);
        CHECK(lineOf(*editor, editor->cursorPosition()) == 1);
        editor->performTextKey(Action::LineUp);
        CHECK(editor->cursorPosition() == CursorPosition{0, 2});

        // From the end of a longer line up to a shorter one: before its break, not after it
        // (the first line here is indented, so its line below is much longer)
        auto longer = editorWith(QStringLiteral("<kml><p>One<br/>three four five six</p></kml>"));
        longer->setCursorPosition({0, 23});
        longer->performTextKey(Action::LineUp);
        CHECK(longer->cursorPosition() == CursorPosition{0, 3});
    }

    SECTION("macOS: Option+Right and Option+Left go over the break to the next word") {
        editor->setCursorPosition({0, 7});
        editor->performTextKey(Action::WordEndForward);
        CHECK(editor->cursorPosition() == CursorPosition{0, 13});
        editor->setCursorPosition({0, 8});
        editor->performTextKey(Action::WordStartBackward);
        CHECK(editor->cursorPosition() == CursorPosition{0, 4});
    }
}

// =============================================================================
// Chapter files and statistics
// =============================================================================

TEST_CASE("Line breaks: a chapter file keeps them as <br/>", "[editor][linebreak][kml]") {
    const QString kml = GENERATE(
        QStringLiteral("<kml><p>Roses are red,<br/>violets are blue.</p></kml>"),
        QStringLiteral("<kml><p><br/>A break first</p></kml>"),
        QStringLiteral("<kml><p>A break last<br/></p></kml>"),
        QStringLiteral("<kml><p><br/></p></kml>"),
        QStringLiteral("<kml><p>Two<br/><br/>breaks</p></kml>"),
        QStringLiteral("<kml><p>A <b>bold<br/>break</b> and <i>more</i></p></kml>"),
        QStringLiteral("<kml><p align=\"justify\">Justified<br/>lines</p><p>Next</p></kml>"),
        QStringLiteral("<kml><annotations><annotation id=\"c1\" kind=\"comment\">Note"
                       "</annotation></annotations><p>A <anchor ref=\"c1\">noted<br/>"
                       "break</anchor></p></kml>"));
    CAPTURE(kml);

    auto editor = editorWith(kml);
    CHECK(editor->toKml() == kml);
    CHECK(editor->paragraphPlainText(0).contains(LINE_BREAK));
}

TEST_CASE("Line breaks: other ways a chapter file may hold them", "[editor][linebreak][kml]") {
    SECTION("<br/> in a text run") {
        auto editor = editorWith(QStringLiteral("<kml><p><t>One<br/>two</t></p></kml>"));
        CHECK(editor->paragraphPlainText(0) == QStringLiteral("One") + LINE_BREAK +
                                                   QStringLiteral("two"));
        CHECK(editor->toKml() == QStringLiteral("<kml><p>One<br/>two</p></kml>"));
    }

    SECTION("U+2028 written as a character") {
        auto editor = editorWith(QStringLiteral("<kml><p>One&#x2028;two</p></kml>"));
        CHECK(editor->paragraphPlainText(0) == QStringLiteral("One") + LINE_BREAK +
                                                   QStringLiteral("two"));
        CHECK(editor->toKml() == QStringLiteral("<kml><p>One<br/>two</p></kml>"));
    }
}

TEST_CASE("Line breaks: the statistics count the words on both sides of a break",
          "[editor][linebreak][kml]") {
    const QString kml =
        QStringLiteral("<kml><p>Roses are red,<br/>violets are blue.</p><p>Next</p></kml>");
    const QString text =
        QStringLiteral("Roses are red,") + LINE_BREAK + QStringLiteral("violets are blue.\nNext");

    CHECK(ChapterDocument::kmlToPlainText(kml) == text);
    ChapterDocument chapter;
    chapter.setKml(kml);
    CHECK(chapter.wordCount() == 7);
    CHECK(chapter.paragraphCount() == 2);  // a break is no paragraph

    // The editor's text and counts are the chapter's
    auto editor = editorWith(kml);
    CHECK(editor->plainText() == text);
    CHECK(editor->wordCount() == 7U);
}

// =============================================================================
// The clipboard
// =============================================================================

TEST_CASE("Line breaks: copied, they are line ends for other programs and <br> in HTML",
          "[editor][linebreak][clipboard]") {
    const QString kml = QStringLiteral("<kml><p>Roses are red,<br/>violets are blue.</p>"
                                       "<p>Next<br/></p></kml>");
    auto editor = editorWith(kml);
    editor->selectAll();
    const auto mimeData = editor->createMimeDataFromSelection();
    REQUIRE(mimeData);

    CHECK(mimeData->text() == QStringLiteral("Roses are red,\nviolets are blue.\nNext\n"));
    CHECK(kmlData(*mimeData) == kml);
    // The empty line after a break at a paragraph's end keeps a no-break space, as an
    // empty paragraph does: most programs would drop it
    CHECK(mimeData->html() == QStringLiteral("<p>Roses are red,<br/>violets are blue.</p>"
                                             "<p>Next<br/> </p>"));

    SECTION("Pasted in the editor, they are line breaks again") {
        auto other = editorWith(kmlOf({QString()}));
        other->insertFromMimeData(mimeData.get());
        CHECK(other->toKml() == kml);
    }
}

TEST_CASE("Line breaks: text from Word and the browsers keeps its line breaks",
          "[editor][linebreak][clipboard]") {
    auto editor = editorWith(kmlOf({QString()}));
    QMimeData pasted;

    SECTION("Word: its HTML has <br> where its plain text has a line end") {
        pasted.setText(QStringLiteral("Roses are red,\nviolets are blue.\nNext"));
        pasted.setHtml(QStringLiteral(
            "<html xmlns:o=\"urn:schemas-microsoft-com:office:office\"><head><style>"
            "p.MsoNormal{margin:0cm;}</style></head><body lang=PL><!--StartFragment-->"
            "<p class=MsoNormal>Roses are red,<br>\nviolets are blue.<o:p></o:p></p>\n"
            "<p class=MsoNormal>Next<o:p></o:p></p><!--EndFragment--></body></html>"));
    }

    SECTION("Word: a vertical tab in its plain text") {
        pasted.setText(QStringLiteral("Roses are red,\vviolets are blue.\nNext"));
    }

    SECTION("A browser: a line end at <br>, a blank line between paragraphs, which goes") {
        pasted.setText(QStringLiteral("Roses are red,\nviolets are blue.\n\nNext"));
        pasted.setHtml(QStringLiteral("<meta charset=\"utf-8\"><p>Roses are red,<br>violets "
                                      "are blue.</p><p>Next</p>"));
    }

    editor->insertFromMimeData(&pasted);
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><p>Roses are red,<br/>violets are blue.</p><p>Next</p></kml>"));
}

// =============================================================================
// Elsewhere in the program
// =============================================================================

TEST_CASE("Line breaks: Find takes no selected text with a break", "[editor][linebreak]") {
    auto editor = editorWith(QStringLiteral("<kml><p>One word<br/>word two</p></kml>"));
    editor->setSelection({{0, 4}, {0, 8}});  // "word"
    editor->showFind();
    auto* bar = editor->findChild<FindReplaceBar*>();
    REQUIRE(bar != nullptr);
    CHECK(bar->searchText() == QStringLiteral("word"));
    CHECK(editor->searchEngine()->totalMatchCount() == 2);  // on both lines

    // The field would show the break as a space, which it would not find
    editor->setSelection({{0, 4}, {0, 13}});  // "word", the break, "word"
    editor->showFind();
    CHECK(bar->searchText() == QStringLiteral("word"));
}

TEST_CASE("Line breaks: the spell check takes the words on both sides of a break",
          "[editor][linebreak][spell_check]") {
    SpellCheckService service;
    REQUIRE(service.loadDictionary("en_US"));
    const auto errors = service.checkParagraph(QStringLiteral("writer") + LINE_BREAK +
                                               QStringLiteral("chaptre") + LINE_BREAK +
                                               QStringLiteral("chapter"));
    REQUIRE(errors.size() == 1);
    CHECK(errors.first().word == QStringLiteral("chaptre"));
    CHECK(errors.first().startPos == 7);
}

TEST_CASE("Line breaks: screen readers get a line end", "[editor][linebreak]") {
    auto editor = editorWith(QStringLiteral("<kml><p>One<br/>two</p></kml>"));
    BookEditorAccessible accessible(editor.get());
    CHECK(accessible.text(QAccessible::Value) == QStringLiteral("One\ntwo"));
    CHECK(accessible.text(0, accessible.characterCount()) == QStringLiteral("One\ntwo"));
}

TEST_CASE("Line breaks: a TODO in the text ends with its line", "[editor][linebreak]") {
    auto editor = editorWith(
        QStringLiteral("<kml><p>TODO: check the rhyme<br/>Violets are blue</p></kml>"));
    TagDetector detector;
    detector.setBookEditor(editor.get());
    detector.scan();
    REQUIRE(detector.allTags().size() == 1);
    CHECK(detector.allTags().front().content == QStringLiteral("check the rhyme"));
}
