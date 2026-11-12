/// @file test_footnote_coordinator.cpp
/// @brief Notes: the frame their text is written in, Insert > Footnote and Endnote, editing a
///        note again, Esc, and the text kept when the frame is left

#include <catch2/catch_test_macros.hpp>
#include "document_window.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_footnotes.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/footnote_coordinator.h"
#include "kalahari/gui/panels/note_frame.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QTextCursor>
#include <QTextEdit>

using namespace kalahari;
using editor::NoteKind;
using gui::CommandRegistry;
using gui::FootnoteCoordinator;
using gui::KeyboardShortcut;
using gui::NoteFrame;
using test::OpenNovel;
using test::Window;

namespace {

/// A key pressed in a widget, with the text it types
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
           const QString& text = QString()) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(widget, &event);
}

/// Letters typed in a widget, one key each
void type(QWidget* widget, const QString& letters) {
    for (const QChar letter : letters) {
        press(widget, letter.toUpper().unicode(), Qt::NoModifier, QString(letter));
    }
}

/// The text box of a frame
QTextEdit* textOf(const NoteFrame& frame) {
    auto* text = frame.findChild<QTextEdit*>(QStringLiteral("noteFrameText"));
    REQUIRE(text != nullptr);
    return text;
}

/// Pastes into a text box what @p mime holds, as Ctrl+V does
class PastingEdit : public QTextEdit {
public:
    static void paste(QTextEdit& edit, const QMimeData& mime) {
        static_cast<PastingEdit&>(edit).insertFromMimeData(&mime);
    }
};

/// The notes of an editor, as "kind:content" in text order
QStringList notesOf(const editor::BookEditor& text) {
    QStringList notes;
    for (const editor::NotePlace& place : editor::notesIn(*text.textDocument())) {
        notes << (place.note.kind == NoteKind::Endnote ? QStringLiteral("e:")
                                                       : QStringLiteral("f:")) +
                     place.note.content;
    }
    return notes;
}

}  // namespace

TEST_CASE("Notes: the frame keeps a note's formatting, its links and its paragraphs",
          "[gui][footnotes]") {
    gui::registerAllCommands(gui::CommandCallbacks{});  // the keys of Bold, Italic, Underline
    QWidget editor;
    editor.resize(800, 600);
    auto* frame = new NoteFrame(&editor);
    frame->setNote(NoteKind::Footnote, QStringLiteral("3"));
    CHECK(frame->findChild<QLabel*>(QStringLiteral("noteFrameKind"))->text() ==
          QStringLiteral("Footnote 3"));
    CHECK(frame->findChild<QLabel*>(QStringLiteral("noteFrameHint"))
              ->text()
              .contains(NoteFrame::saveKeysText()));
    frame->setNote(NoteKind::Endnote, QStringLiteral("ii"));
    CHECK(frame->findChild<QLabel*>(QStringLiteral("noteFrameKind"))->text() ==
          QStringLiteral("Endnote ii"));

    // What it starts with comes back as it was
    const QString content = QStringLiteral(
        "<p>One <b>bold</b> and <a href=\"https://example.com\">a link</a></p><p>Two</p>");
    frame->setContent(content);
    CHECK(frame->content() == content);

    // Ctrl+B, Ctrl+I, Ctrl+U on the selection
    QTextEdit* text = textOf(*frame);
    frame->setContent(QStringLiteral("Plain"));
    QTextCursor all = text->textCursor();
    all.select(QTextCursor::Document);
    text->setTextCursor(all);
    press(text, Qt::Key_B, Qt::ControlModifier);
    CHECK(frame->content() == QStringLiteral("<b>Plain</b>"));
    press(text, Qt::Key_I, Qt::ControlModifier);
    press(text, Qt::Key_B, Qt::ControlModifier);
    CHECK(frame->content() == QStringLiteral("<i>Plain</i>"));
    press(text, Qt::Key_U, Qt::ControlModifier);
    press(text, Qt::Key_I, Qt::ControlModifier);
    CHECK(frame->content() == QStringLiteral("<u>Plain</u>"));

    // The keys the user gives Bold (Settings > Keyboard Shortcuts) make it bold in the frame
    // too; its old keys do nothing there
    CommandRegistry::getInstance().setCustomShortcuts(
        {{"format.bold", KeyboardShortcut(Qt::Key_F12, Qt::ControlModifier)}});
    frame->setContent(QStringLiteral("Plain"));
    all.select(QTextCursor::Document);
    text->setTextCursor(all);
    press(text, Qt::Key_B, Qt::ControlModifier);
    CHECK(frame->content() == QStringLiteral("Plain"));
    press(text, Qt::Key_F12, Qt::ControlModifier);
    CHECK(frame->content() == QStringLiteral("<b>Plain</b>"));
    CommandRegistry::getInstance().setCustomShortcuts({});

    // Pasted: only what a note keeps, no picture, no color, no font
    frame->setContent(QString());
    QMimeData pasted;
    pasted.setHtml(QStringLiteral(
        "<p><span style=\"color:#ff0000; font-family:Arial; font-size:20pt\">Red</span> "
        "<b>bold</b><img src=\"x.png\"/></p>"));
    PastingEdit::paste(*text, pasted);
    CHECK(frame->content() == QStringLiteral("Red <b>bold</b>"));

    // Enter: a new paragraph, Shift+Enter a line break
    frame->setContent(QString());
    frame->startTyping();
    type(text, QStringLiteral("a"));
    press(text, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
    type(text, QStringLiteral("b"));
    press(text, Qt::Key_Return, Qt::ShiftModifier, QStringLiteral("\r"));
    type(text, QStringLiteral("c"));
    CHECK(frame->content() == QStringLiteral("<p>a</p><p>b<br/>c</p>"));
}

TEST_CASE("Notes: Insert > Footnote, the text kept with Ctrl+Enter as one undo step, Esc",
          "[gui][footnotes]") {
    OpenNovel book(QStringLiteral("Notes"));
    Window window;
    window.navigator.onElementSelected(book.chapterId, QStringLiteral("The Storm"));
    editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
    REQUIRE(text != nullptr);
    text->fromKml(QStringLiteral("<kml><p>Rain fell.</p><p>Then it stopped.</p></kml>"));
    text->setCursorPosition({0, 4});
    FootnoteCoordinator coordinator(window.tabs, [text]() { return text; });
    coordinator.connectCommands();

    // A new note: its number in the text, its frame open with the keys
    REQUIRE(coordinator.insertNote(NoteKind::Footnote));
    NoteFrame* frame = coordinator.frame();
    REQUIRE(frame != nullptr);
    CHECK(frame->findChild<QLabel*>(QStringLiteral("noteFrameKind"))->text() ==
          QStringLiteral("Footnote 1"));
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:")});
    type(textOf(*frame), QStringLiteral("Cold"));
    press(textOf(*frame), Qt::Key_Return, Qt::ControlModifier);
    QCoreApplication::processEvents();
    CHECK(coordinator.frame() == nullptr);
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:Cold")});

    // One undo step takes the number and its text
    text->undo();
    CHECK(notesOf(*text).isEmpty());
    CHECK(text->textDocument()->firstBlock().text() == QStringLiteral("Rain fell."));
    text->redo();
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:Cold")});

    // Esc on a new note: nothing is left, nothing to redo
    text->setCursorPosition({1, 4});
    REQUIRE(coordinator.insertNote(NoteKind::Endnote));
    REQUIRE(coordinator.frame() != nullptr);
    CHECK(coordinator.frame()->findChild<QLabel*>(QStringLiteral("noteFrameKind"))->text() ==
          QStringLiteral("Endnote i"));
    type(textOf(*coordinator.frame()), QStringLiteral("Gone"));
    press(textOf(*coordinator.frame()), Qt::Key_Escape);
    QCoreApplication::processEvents();
    CHECK(coordinator.frame() == nullptr);
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:Cold")});
    CHECK_FALSE(text->textDocument()->isRedoAvailable());

    // Edited again: Esc drops the changes, the note stays
    const QString id = editor::notesIn(*text->textDocument()).front().note.id;
    REQUIRE(coordinator.editNote(*text, id));
    REQUIRE(coordinator.frame() != nullptr);
    CHECK(textOf(*coordinator.frame())->toPlainText() == QStringLiteral("Cold"));
    type(textOf(*coordinator.frame()), QStringLiteral("er"));
    press(textOf(*coordinator.frame()), Qt::Key_Escape);
    QCoreApplication::processEvents();
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:Cold")});

    // The text asks for the frame (a click on the note's text): kept when the frame is left
    emit text->noteEditRequested(id);
    REQUIRE(coordinator.frame() != nullptr);
    type(textOf(*coordinator.frame()), QStringLiteral("er"));
    coordinator.finishWriting(text);
    CHECK(coordinator.frame() == nullptr);
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:Colder")});
    text->undo();
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:Cold")});

    // A read-only text takes no note
    text->setReadOnly(true);
    CHECK_FALSE(coordinator.insertNote(NoteKind::Footnote));
    CHECK_FALSE(coordinator.editNote(*text, id));
    CHECK(coordinator.frame() == nullptr);
}
