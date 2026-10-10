/// @file test_text_keys.cpp
/// @brief The keys of the text: the tables of Windows and Linux and of macOS, and what each
///        key does in the editor

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/book_editor_accessible.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <kalahari/editor/text_keys.h>
#include "editor_test_utils.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QSet>
#include <QTextBlock>
#include <QTextLayout>
#include <cmath>
#include <memory>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

using Action = TextKeyAction;

constexpr Qt::KeyboardModifiers NONE = Qt::NoModifier;
constexpr Qt::KeyboardModifiers SHIFT = Qt::ShiftModifier;
constexpr Qt::KeyboardModifiers CTRL = Qt::ControlModifier;
constexpr Qt::KeyboardModifiers ALT = Qt::AltModifier;
constexpr Qt::KeyboardModifiers META = Qt::MetaModifier;

constexpr bool WINDOWS = false;  // and Linux
constexpr bool MAC = true;

bool isKey(bool macOS, Qt::Key key, Qt::KeyboardModifiers modifiers, Action action,
           bool select = false) {
    const TextKey textKey = textKeyFor(key, modifiers, macOS);
    return textKey.action == action && textKey.extendSelection == select;
}

std::unique_ptr<BookEditor> editorWith(const QStringList& paragraphs, QSize size = {500, 300}) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, size);
    editor->fromKml(kmlOf(paragraphs));
    return editor;
}

QString textOf(const BookEditor& editor) {
    return editor.plainText();
}

void press(BookEditor& editor, int key, Qt::KeyboardModifiers modifiers, const QString& text = {}) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QCoreApplication::sendEvent(&editor, &event);
}

/// The key the program's system has for an action (without Shift)
TextKey keyFor(Action action) {
    for (const TextKey& textKey : textKeys(isMacOS())) {
        if (textKey.action == action && !textKey.extendSelection) {
            return textKey;
        }
    }
    return {};
}

bool clipboardWorks() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        return false;
    }
    clipboard->setText(QStringLiteral("__kalahari_text_keys__"));
    QCoreApplication::processEvents();
    return clipboard->text() == QStringLiteral("__kalahari_text_keys__");
}

/// The line of the layout a position is on
int lineOf(BookEditor& editor, const CursorPosition& position) {
    const QTextBlock block = editor.textDocument()->findBlockByNumber(position.paragraph);
    return KalahariTextDocumentLayout::blockLayout(block)
        ->lineForTextPosition(position.offset)
        .lineNumber();
}

}  // namespace

// =============================================================================
// The tables
// =============================================================================

TEST_CASE("Text keys: Windows and Linux have the keys of Windows word processors",
          "[editor][textkeys]") {
    CHECK(isKey(WINDOWS, Qt::Key_Left, CTRL, Action::WordLeft));
    CHECK(isKey(WINDOWS, Qt::Key_Right, CTRL | SHIFT, Action::WordRight, true));
    CHECK(isKey(WINDOWS, Qt::Key_Up, CTRL, Action::ParagraphUp));
    CHECK(isKey(WINDOWS, Qt::Key_Down, CTRL, Action::ParagraphDown));
    CHECK(isKey(WINDOWS, Qt::Key_Down, CTRL | SHIFT, Action::ParagraphDown, true));
    CHECK(isKey(WINDOWS, Qt::Key_Home, NONE, Action::LineStart));
    CHECK(isKey(WINDOWS, Qt::Key_End, SHIFT, Action::LineEnd, true));
    CHECK(isKey(WINDOWS, Qt::Key_Home, CTRL, Action::DocumentStart));
    CHECK(isKey(WINDOWS, Qt::Key_PageDown, NONE, Action::PageDown));
    CHECK(isKey(WINDOWS, Qt::Key_Backspace, CTRL, Action::DeleteWordBackward));
    CHECK(isKey(WINDOWS, Qt::Key_Delete, CTRL, Action::DeleteWordForward));
    CHECK(isKey(WINDOWS, Qt::Key_Backspace, SHIFT, Action::DeleteBackward));
    CHECK(isKey(WINDOWS, Qt::Key_Return, SHIFT, Action::NewParagraph));
    CHECK(isKey(WINDOWS, Qt::Key_Enter, Qt::KeypadModifier, Action::NewParagraph));

    // The second keys of the commands, those of every Windows text field
    CHECK(isKey(WINDOWS, Qt::Key_Z, CTRL | SHIFT, Action::Redo));
    CHECK(isKey(WINDOWS, Qt::Key_Backspace, ALT, Action::Undo));
    CHECK(isKey(WINDOWS, Qt::Key_Backspace, ALT | SHIFT, Action::Redo));
    CHECK(isKey(WINDOWS, Qt::Key_Delete, SHIFT, Action::Cut));
    CHECK(isKey(WINDOWS, Qt::Key_Insert, CTRL, Action::Copy));
    CHECK(isKey(WINDOWS, Qt::Key_Insert, SHIFT, Action::Paste));
    CHECK(isKey(WINDOWS, Qt::Key_F10, SHIFT, Action::ContextMenu));

    // The commands' keys and the free ones are not the text's
    CHECK(isKey(WINDOWS, Qt::Key_A, CTRL, Action::None));
    CHECK(isKey(WINDOWS, Qt::Key_Z, CTRL, Action::None));
    CHECK(isKey(WINDOWS, Qt::Key_Down, ALT, Action::None));  // Next To Do
    CHECK(isKey(WINDOWS, Qt::Key_PageDown, CTRL, Action::None));
    CHECK(isKey(WINDOWS, Qt::Key_Return, ALT, Action::None));
    CHECK(isKey(WINDOWS, Qt::Key_Backspace, CTRL | SHIFT, Action::None));
}

TEST_CASE("Text keys: macOS has the keys of its text fields", "[editor][textkeys]") {
    // Option moves by words and paragraphs, Cmd to the ends of the line and of the text
    CHECK(isKey(MAC, Qt::Key_Left, ALT, Action::WordStartBackward));
    CHECK(isKey(MAC, Qt::Key_Right, ALT | SHIFT, Action::WordEndForward, true));
    CHECK(isKey(MAC, Qt::Key_Left, CTRL, Action::LineStart));
    CHECK(isKey(MAC, Qt::Key_Right, CTRL, Action::LineEnd));
    CHECK(isKey(MAC, Qt::Key_Up, ALT, Action::ParagraphUp));
    CHECK(isKey(MAC, Qt::Key_Down, ALT, Action::ParagraphEndForward));
    CHECK(isKey(MAC, Qt::Key_Up, CTRL, Action::DocumentStart));
    CHECK(isKey(MAC, Qt::Key_Down, CTRL | SHIFT, Action::DocumentEnd, true));

    // Home, End, Page Up and Page Down move the view; with Shift they select, with Option
    // Page Up and Page Down move the cursor
    CHECK(isKey(MAC, Qt::Key_Home, NONE, Action::ScrollToStart));
    CHECK(isKey(MAC, Qt::Key_End, NONE, Action::ScrollToEnd));
    CHECK(isKey(MAC, Qt::Key_Home, SHIFT, Action::DocumentStart, true));
    CHECK(isKey(MAC, Qt::Key_PageDown, NONE, Action::ScrollPageDown));
    CHECK(isKey(MAC, Qt::Key_PageDown, ALT, Action::PageDown));
    CHECK(isKey(MAC, Qt::Key_PageUp, SHIFT, Action::PageUp, true));

    CHECK(isKey(MAC, Qt::Key_Backspace, ALT, Action::DeleteToWordStart));
    CHECK(isKey(MAC, Qt::Key_Delete, ALT, Action::DeleteToWordEnd));
    CHECK(isKey(MAC, Qt::Key_Backspace, CTRL, Action::DeleteToLineStart));
    CHECK(isKey(MAC, Qt::Key_Backspace, META, Action::DeleteDiacritic));
    CHECK(isKey(MAC, Qt::Key_Z, CTRL | SHIFT, Action::Redo));
    CHECK(isKey(MAC, Qt::Key_Return, ALT, Action::NewParagraph));

    // The Control keys
    CHECK(isKey(MAC, Qt::Key_A, META, Action::ParagraphStart));
    CHECK(isKey(MAC, Qt::Key_E, META | SHIFT, Action::ParagraphEnd, true));
    CHECK(isKey(MAC, Qt::Key_B, META, Action::CharacterLeft));
    CHECK(isKey(MAC, Qt::Key_F, META, Action::CharacterRight));
    CHECK(isKey(MAC, Qt::Key_P, META, Action::LineUp));
    CHECK(isKey(MAC, Qt::Key_N, META, Action::LineDown));
    CHECK(isKey(MAC, Qt::Key_V, META, Action::PageDown));
    CHECK(isKey(MAC, Qt::Key_H, META, Action::DeleteBackward));
    CHECK(isKey(MAC, Qt::Key_D, META, Action::DeleteForward));
    CHECK(isKey(MAC, Qt::Key_K, META, Action::KillToParagraphEnd));
    CHECK(isKey(MAC, Qt::Key_Y, META, Action::Yank));
    CHECK(isKey(MAC, Qt::Key_T, META, Action::Transpose));
    CHECK(isKey(MAC, Qt::Key_O, META, Action::OpenLine));
    CHECK(isKey(MAC, Qt::Key_L, META, Action::CenterCursor));

    // macOS reports the arrows as keys of the keypad
    CHECK(isKey(MAC, Qt::Key_Left, ALT | Qt::KeypadModifier, Action::WordStartBackward));

    // Windows' keys are not there; the commands' keys are not the text's
    CHECK(isKey(MAC, Qt::Key_Left, META, Action::None));
    CHECK(isKey(MAC, Qt::Key_Insert, SHIFT, Action::None));
    CHECK(isKey(MAC, Qt::Key_F10, SHIFT, Action::None));
    CHECK(isKey(MAC, Qt::Key_A, CTRL, Action::None));
    CHECK(isKey(MAC, Qt::Key_Down, CTRL | ALT, Action::None));  // Next To Do
}

TEST_CASE("Text keys: each key is in a table once", "[editor][textkeys]") {
    for (const bool macOS : {WINDOWS, MAC}) {
        QSet<int> seen;
        for (const TextKey& textKey : textKeys(macOS)) {
            CAPTURE(macOS, textKey.keys.toCombined());
            CHECK_FALSE(seen.contains(textKey.keys.toCombined()));
            seen.insert(textKey.keys.toCombined());
            CHECK(textKey.action != Action::None);
        }
    }
}

// =============================================================================
// What the keys do
// =============================================================================

TEST_CASE("Text keys: Ctrl+Backspace and Ctrl+Delete delete a word", "[editor][textkeys]") {
    auto editor = editorWith({QStringLiteral("Ala ma kota."), QString(),
                              QStringLiteral("Drugi akapit.")});

    SECTION("Ctrl+Backspace: to the start of the word before the cursor") {
        editor->setCursorPosition({0, 6});  // Ala ma| kota.
        editor->performTextKey(Action::DeleteWordBackward);
        CHECK(textOf(*editor) == QStringLiteral("Ala  kota.\n\nDrugi akapit."));
        CHECK(editor->cursorPosition() == CursorPosition{0, 4});

        // One undo step
        editor->undo();
        CHECK(textOf(*editor) == QStringLiteral("Ala ma kota.\n\nDrugi akapit."));
    }

    SECTION("Ctrl+Backspace at the start of a paragraph joins it to the one before") {
        editor->setCursorPosition({2, 0});
        editor->performTextKey(Action::DeleteWordBackward);
        CHECK(textOf(*editor) == QStringLiteral("Ala ma kota.\nDrugi akapit."));
        CHECK(editor->cursorPosition() == CursorPosition{1, 0});
    }

    SECTION("Ctrl+Delete: to the start of the next word") {
        editor->setCursorPosition({0, 4});  // Ala |ma kota.
        editor->performTextKey(Action::DeleteWordForward);
        CHECK(textOf(*editor) == QStringLiteral("Ala kota.\n\nDrugi akapit."));
        CHECK(editor->cursorPosition() == CursorPosition{0, 4});
    }

    SECTION("Ctrl+Delete at the end of a paragraph joins the next one to it") {
        editor->setCursorPosition({0, 12});
        editor->performTextKey(Action::DeleteWordForward);
        CHECK(textOf(*editor) == QStringLiteral("Ala ma kota.\nDrugi akapit."));
        CHECK(editor->cursorPosition() == CursorPosition{0, 12});
    }

    SECTION("A selection goes whole") {
        editor->setCursorPosition({0, 0});
        editor->performTextKey(Action::CharacterRight, true);
        editor->performTextKey(Action::CharacterRight, true);
        editor->performTextKey(Action::DeleteWordForward);
        CHECK(textOf(*editor) == QStringLiteral("a ma kota.\n\nDrugi akapit."));
    }
}

TEST_CASE("Text keys: Ctrl+Up and Ctrl+Down move by paragraphs", "[editor][textkeys]") {
    auto editor = editorWith({QStringLiteral("Pierwszy."), QStringLiteral("Drugi."),
                              QStringLiteral("Trzeci.")});

    editor->setCursorPosition({1, 3});
    editor->performTextKey(Action::ParagraphUp);
    CHECK(editor->cursorPosition() == CursorPosition{1, 0});  // the start of its paragraph
    editor->performTextKey(Action::ParagraphUp);
    CHECK(editor->cursorPosition() == CursorPosition{0, 0});  // there: the one before
    editor->performTextKey(Action::ParagraphUp);
    CHECK(editor->cursorPosition() == CursorPosition{0, 0});

    editor->setCursorPosition({0, 4});
    editor->performTextKey(Action::ParagraphDown);
    CHECK(editor->cursorPosition() == CursorPosition{1, 0});
    editor->performTextKey(Action::ParagraphDown);
    CHECK(editor->cursorPosition() == CursorPosition{2, 0});
    editor->performTextKey(Action::ParagraphDown);
    CHECK(editor->cursorPosition() == CursorPosition{2, 7});  // the last: its end

    // With Shift the selection goes with the cursor
    editor->setCursorPosition({0, 4});
    editor->performTextKey(Action::ParagraphDown, true);
    CHECK(editor->selectedText() == QStringLiteral("wszy.") + QChar(QChar::ParagraphSeparator));

    // macOS: Option+Down to the end of the paragraph, there to the end of the next one
    editor->setCursorPosition({0, 4});
    editor->performTextKey(Action::ParagraphEndForward);
    CHECK(editor->cursorPosition() == CursorPosition{0, 9});
    editor->performTextKey(Action::ParagraphEndForward);
    CHECK(editor->cursorPosition() == CursorPosition{1, 6});

    // macOS: Control+A and Control+E
    editor->performTextKey(Action::ParagraphStart);
    CHECK(editor->cursorPosition() == CursorPosition{1, 0});
    editor->performTextKey(Action::ParagraphEnd);
    CHECK(editor->cursorPosition() == CursorPosition{1, 6});
}

TEST_CASE("Text keys: Home and End go to the ends of the line on the screen",
          "[editor][textkeys]") {
    const QString text = QStringLiteral("Ten akapit jest na tyle długi, że w wąskim oknie "
                                        "zajmuje kilka wierszy, a Home i End idą do początku "
                                        "i końca wiersza, w którym stoi kursor.");
    auto editor = editorWith({text, QStringLiteral("Krótki.")}, {220, 300});
    const QTextBlock block = editor->textDocument()->firstBlock();
    const QTextLayout* layout = KalahariTextDocumentLayout::blockLayout(block);
    REQUIRE(layout->lineCount() >= 3);
    // The second line (QTextLine reads the layout of the moment: taken before the edits)
    const int secondStart = layout->lineAt(1).textStart();
    const int secondLength = layout->lineAt(1).textLength();

    // In the middle of the second line
    editor->setCursorPosition({0, secondStart + 3});
    editor->performTextKey(Action::LineStart);
    CHECK(editor->cursorPosition() == CursorPosition{0, secondStart});

    editor->performTextKey(Action::LineEnd);
    CHECK(lineOf(*editor, editor->cursorPosition()) == 1);  // before the space it wraps at
    CHECK(editor->cursorPosition().offset == secondStart + secondLength - 1);

    // The last line ends with the paragraph
    const int length = static_cast<int>(text.size());
    editor->setCursorPosition({0, length - 4});
    editor->performTextKey(Action::LineEnd);
    CHECK(editor->cursorPosition() == CursorPosition{0, length});

    // With Shift the line is selected
    editor->setCursorPosition({0, secondStart});
    editor->performTextKey(Action::LineEnd, true);
    CHECK(editor->selectedText() == text.mid(secondStart, secondLength - 1));

    // macOS: Cmd+Backspace deletes to the start of the line; there, the character before it
    editor->clearSelection();
    editor->setCursorPosition({0, secondStart + 3});
    editor->performTextKey(Action::DeleteToLineStart);
    CHECK(textOf(*editor).startsWith(text.left(secondStart) +
                                     text.mid(secondStart + 3)));
    editor->setCursorPosition({1, 0});
    editor->performTextKey(Action::DeleteToLineStart);
    CHECK(editor->paragraphCount() == 1);
}

TEST_CASE("Text keys: macOS moves and deletes by words across paragraphs",
          "[editor][textkeys]") {
    auto editor = editorWith({QStringLiteral("Ala ma kota,"), QString(),
                              QStringLiteral("  a kot ma Alę.")});

    // Option+Left: to the start of the word, over the breaks of empty paragraphs
    editor->setCursorPosition({2, 2});
    editor->performTextKey(Action::WordStartBackward);
    CHECK(editor->cursorPosition() == CursorPosition{0, 7});
    editor->performTextKey(Action::WordStartBackward);
    CHECK(editor->cursorPosition() == CursorPosition{0, 4});

    // Option+Right: to the end of the word
    editor->setCursorPosition({0, 7});
    editor->performTextKey(Action::WordEndForward);
    CHECK(editor->cursorPosition() == CursorPosition{0, 11});
    editor->performTextKey(Action::WordEndForward);
    CHECK(editor->cursorPosition() == CursorPosition{2, 3});

    // At the ends of the text the cursor stays
    editor->setCursorPosition({0, 0});
    editor->performTextKey(Action::WordStartBackward);
    CHECK(editor->cursorPosition() == CursorPosition{0, 0});
    editor->setCursorPosition({2, 15});
    editor->performTextKey(Action::WordEndForward);
    CHECK(editor->cursorPosition() == CursorPosition{2, 15});

    // Option+Backspace and Option+Delete delete as far as those moves go
    editor->setCursorPosition({0, 6});
    editor->performTextKey(Action::DeleteToWordStart);
    CHECK(textOf(*editor) == QStringLiteral("Ala  kota,\n\n  a kot ma Alę."));
    editor->setCursorPosition({0, 9});  // before the comma
    editor->performTextKey(Action::DeleteToWordEnd);
    CHECK(textOf(*editor) == QStringLiteral("Ala  kota kot ma Alę."));
}

TEST_CASE("Text keys: the arrows go over a character with its marks", "[editor][textkeys]") {
    // a with a combining ogonek, and a character outside the basic plane
    const QString text = QStringLiteral("ąb") + QString::fromUcs4(U"\U0001F600") +
                         QStringLiteral("c");
    auto editor = editorWith({text});
    editor->setCursorPosition({0, 0});
    editor->performTextKey(Action::CharacterRight);
    CHECK(editor->cursorPosition() == CursorPosition{0, 2});
    editor->performTextKey(Action::CharacterRight);
    editor->performTextKey(Action::CharacterRight);
    CHECK(editor->cursorPosition() == CursorPosition{0, 5});
    editor->performTextKey(Action::CharacterLeft);
    CHECK(editor->cursorPosition() == CursorPosition{0, 3});
    editor->performTextKey(Action::CharacterLeft);
    editor->performTextKey(Action::CharacterLeft);
    CHECK(editor->cursorPosition() == CursorPosition{0, 0});
}

TEST_CASE("Text keys: Left and Right end a selection at its start and its end",
          "[editor][textkeys]") {
    auto editor = editorWith({QStringLiteral("Ala ma kota.")});
    editor->setCursorPosition({0, 4});
    editor->performTextKey(Action::WordRight, true);
    REQUIRE(editor->hasSelection());
    editor->performTextKey(Action::CharacterLeft);
    CHECK_FALSE(editor->hasSelection());
    CHECK(editor->cursorPosition() == CursorPosition{0, 4});

    editor->performTextKey(Action::WordRight, true);
    editor->performTextKey(Action::CharacterRight);
    CHECK_FALSE(editor->hasSelection());
    CHECK(editor->cursorPosition() == CursorPosition{0, 7});
}

TEST_CASE("Text keys: macOS Control keys that change the text", "[editor][textkeys]") {
    SECTION("Control+K deletes to the end of the paragraph and Control+Y brings it back") {
        auto editor = editorWith({QStringLiteral("Ala ma kota."), QStringLiteral("Kot ma Alę.")});
        editor->setCursorPosition({0, 4});
        editor->performTextKey(Action::KillToParagraphEnd);
        CHECK(textOf(*editor) == QStringLiteral("Ala \nKot ma Alę."));
        // At the end of the paragraph: its break; the presses in a row keep all they took
        editor->performTextKey(Action::KillToParagraphEnd);
        CHECK(textOf(*editor) == QStringLiteral("Ala Kot ma Alę."));

        editor->setCursorPosition({0, 15});
        editor->performTextKey(Action::Yank);
        CHECK(textOf(*editor) == QStringLiteral("Ala Kot ma Alę.ma kota.\n"));

        // Another press, not in a row, keeps only its own text
        editor->setCursorPosition({0, 0});
        editor->performTextKey(Action::KillToParagraphEnd);
        editor->setCursorPosition({1, 0});
        editor->performTextKey(Action::Yank);
        CHECK(textOf(*editor) == QStringLiteral("\nAla Kot ma Alę.ma kota."));

        // The yank is one undo step
        editor->undo();
        CHECK(textOf(*editor) == QStringLiteral("\n"));
    }

    SECTION("Control+T swaps the characters around the cursor") {
        auto editor = editorWith({QStringLiteral("abc")});
        editor->setCursorPosition({0, 1});
        editor->performTextKey(Action::Transpose);
        CHECK(textOf(*editor) == QStringLiteral("bac"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 2});
        // At the end of the paragraph, the two before it
        editor->setCursorPosition({0, 3});
        editor->performTextKey(Action::Transpose);
        CHECK(textOf(*editor) == QStringLiteral("bca"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 3});
        // At the start nothing changes
        editor->setCursorPosition({0, 0});
        editor->performTextKey(Action::Transpose);
        CHECK(textOf(*editor) == QStringLiteral("bca"));
    }

    SECTION("Control+O breaks the paragraph after the cursor") {
        auto editor = editorWith({QStringLiteral("Alama")});
        editor->setCursorPosition({0, 3});
        editor->performTextKey(Action::OpenLine);
        CHECK(textOf(*editor) == QStringLiteral("Ala\nma"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 3});
    }

    SECTION("Control+Backspace takes the mark off the character before the cursor") {
        auto editor = editorWith({QStringLiteral("ząb")});
        editor->setCursorPosition({0, 2});
        editor->performTextKey(Action::DeleteDiacritic);
        CHECK(textOf(*editor) == QStringLiteral("zab"));
        CHECK(editor->cursorPosition() == CursorPosition{0, 2});
        // Without a mark the character goes
        editor->performTextKey(Action::DeleteDiacritic);
        CHECK(textOf(*editor) == QStringLiteral("zb"));
    }
}

TEST_CASE("Text keys: macOS Home, End, Page Up and Page Down move the view only",
          "[editor][textkeys]") {
    QStringList paragraphs;
    for (int i = 0; i < 80; ++i) {
        paragraphs
            << QStringLiteral("Akapit %1, dość długi, żeby tekst nie mieścił się w oknie.").arg(i);
    }
    auto editor = editorWith(paragraphs);
    editor->setCursorPosition({3, 2});
    const CursorPosition cursor = editor->cursorPosition();
    const qreal top = editor->scrollOffset();

    editor->performTextKey(Action::ScrollPageDown);
    CHECK(editor->scrollOffset() > top);
    CHECK(editor->cursorPosition() == cursor);

    editor->performTextKey(Action::ScrollToEnd);
    const qreal bottom = editor->scrollOffset();
    CHECK(bottom > top);
    CHECK(editor->cursorPosition() == cursor);
    editor->performTextKey(Action::ScrollPageDown);
    CHECK(editor->scrollOffset() == bottom);  // already at the end

    editor->performTextKey(Action::ScrollPageUp);
    CHECK(editor->scrollOffset() < bottom);
    editor->performTextKey(Action::ScrollToStart);
    CHECK(editor->scrollOffset() == 0.0);
    CHECK(editor->cursorPosition() == cursor);

    // Control+L: the cursor's line to the middle of the view
    editor->performTextKey(Action::ScrollToEnd);
    editor->setCursorPosition({40, 0});
    editor->performTextKey(Action::ScrollToStart);
    editor->performTextKey(Action::CenterCursor);
    const QRectF caret = editor->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    CHECK(std::abs(caret.center().y() - editor->height() / 2.0) < caret.height());
}

TEST_CASE("Text keys: the keys of the program's system reach the editor", "[editor][textkeys]") {
    auto editor = editorWith({QStringLiteral("Ala ma kota."), QStringLiteral("Kot.")});

    // The word before the cursor: Ctrl+Backspace, Option+Backspace on macOS
    const TextKey deleteWord =
        keyFor(isMacOS() ? Action::DeleteToWordStart : Action::DeleteWordBackward);
    REQUIRE(deleteWord.action != Action::None);
    editor->setCursorPosition({0, 6});
    press(*editor, deleteWord.keys.key(), deleteWord.keys.keyboardModifiers());
    CHECK(textOf(*editor) == QStringLiteral("Ala  kota.\nKot."));

    // The next paragraph: Ctrl+Down, Option+Down to the end of it on macOS
    editor->setCursorPosition({0, 2});
    press(*editor, Qt::Key_Down, isMacOS() ? ALT : CTRL);
    CHECK(editor->cursorPosition() == (isMacOS() ? CursorPosition{0, 10} : CursorPosition{1, 0}));

    // macOS reports the arrows as keys of the keypad
    editor->setCursorPosition({0, 2});
    press(*editor, Qt::Key_Right, Qt::KeypadModifier);
    CHECK(editor->cursorPosition() == CursorPosition{0, 3});

    // The commands' keys are the window's: the editor alone does nothing with them
    editor->setCursorPosition({0, 0});
    press(*editor, Qt::Key_A, CTRL, QStringLiteral("\x01"));
    CHECK_FALSE(editor->hasSelection());
    CHECK(textOf(*editor) == QStringLiteral("Ala  kota.\nKot."));

    // Typing still types
    press(*editor, Qt::Key_X, NONE, QStringLiteral("x"));
    CHECK(textOf(*editor) == QStringLiteral("xAla  kota.\nKot."));
}

TEST_CASE("Text keys: the second keys of the clipboard commands", "[editor][textkeys]") {
    if (!clipboardWorks()) {
        SKIP("Clipboard not functional in headless environment");
    }
    auto editor = editorWith({QStringLiteral("Ala ma kota.")});
    editor->setCursorPosition({0, 0});
    editor->performTextKey(Action::WordRight, true);  // "Ala "

    editor->performTextKey(Action::Copy);  // Ctrl+Insert
    CHECK(QGuiApplication::clipboard()->text() == QStringLiteral("Ala "));

    editor->performTextKey(Action::Cut);  // Shift+Delete
    CHECK(textOf(*editor) == QStringLiteral("ma kota."));

    editor->setCursorPosition({0, 8});
    editor->performTextKey(Action::Paste);  // Shift+Insert
    CHECK(textOf(*editor) == QStringLiteral("ma kota.Ala "));

    editor->performTextKey(Action::Undo);  // Alt+Backspace
    CHECK(textOf(*editor) == QStringLiteral("ma kota."));
    editor->performTextKey(Action::Redo);  // Alt+Shift+Backspace, Ctrl+Shift+Z
    CHECK(textOf(*editor) == QStringLiteral("ma kota.Ala "));

    // Shift+Delete without a selection cuts nothing
    editor->performTextKey(Action::Cut);
    CHECK(textOf(*editor) == QStringLiteral("ma kota.Ala "));
}

TEST_CASE("Text keys: the help of the screen readers names the keys of the system",
          "[editor][textkeys]") {
    auto editor = editorWith({QStringLiteral("Ala ma kota.")});
    BookEditorAccessible accessible(editor.get());
    const QString help = accessible.text(QAccessible::Help);

    const auto native = [](Action action) {
        return QKeySequence(keyFor(action).keys).toString(QKeySequence::NativeText);
    };
    CHECK(help.contains(native(Action::DocumentStart)));
    CHECK(help.contains(native(Action::DocumentEnd)));
    if constexpr (isMacOS()) {
        CHECK(keyFor(Action::DocumentStart).keys == QKeyCombination(CTRL, Qt::Key_Up));
    } else {
        CHECK(keyFor(Action::DocumentStart).keys == QKeyCombination(CTRL, Qt::Key_Home));
    }
}
