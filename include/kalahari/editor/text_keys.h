/// @file text_keys.h
/// @brief The keys of the text: what each key does in the editor, on each system
///
/// One table serves the editor (BookEditor::keyPressEvent) and the list of keys the
/// shortcuts of the commands cannot take (Settings > Keyboard Shortcuts). Windows and
/// Linux have the keys of Windows word processors; macOS has the keys of its own text
/// fields. On macOS Qt reports Cmd as Ctrl, Option as Alt and Control as Meta.

#pragma once

#include <QKeyCombination>
#include <Qt>

#include <vector>

namespace kalahari::editor {

/// @brief What a key does in the text
enum class TextKeyAction {
    None,

    // The cursor (with Shift the selection goes with it)
    CharacterLeft,
    CharacterRight,
    WordLeft,             ///< To the start of the word (Windows, Linux: Ctrl+Left)
    WordRight,            ///< To the start of the next word (Windows, Linux: Ctrl+Right)
    WordStartBackward,    ///< To the start of the word, across paragraphs (macOS: Option+Left)
    WordEndForward,       ///< To the end of the word, across paragraphs (macOS: Option+Right)
    LineUp,
    LineDown,
    LineStart,            ///< To the start of the line on the screen
    LineEnd,              ///< To the end of the line on the screen
    ParagraphStart,       ///< To the start of the paragraph (macOS: Control+A)
    ParagraphEnd,         ///< To the end of the paragraph (macOS: Control+E)
    ParagraphUp,          ///< To the start of the paragraph; there already, of the one before
    ParagraphDown,        ///< To the start of the next paragraph; in the last, to its end
    ParagraphEndForward,  ///< To the end of the paragraph; there already, of the next one
    DocumentStart,
    DocumentEnd,
    PageUp,               ///< The cursor and the view one view height up
    PageDown,             ///< The cursor and the view one view height down

    // The view only: the cursor stays (macOS)
    ScrollPageUp,         ///< One view height up (Page Up)
    ScrollPageDown,       ///< One view height down (Page Down)
    ScrollToStart,        ///< To the start of the text (Home)
    ScrollToEnd,          ///< To the end of the text (End)
    CenterCursor,         ///< The cursor's line to the middle of the view (Control+L)

    // Deleting (a selection goes whole)
    DeleteBackward,       ///< The character before the cursor
    DeleteForward,        ///< The character after the cursor
    DeleteDiacritic,      ///< The last mark of the character before the cursor, or the
                          ///< character (macOS: Control+Backspace)
    DeleteWordBackward,   ///< To where WordLeft goes (Windows, Linux: Ctrl+Backspace)
    DeleteWordForward,    ///< To where WordRight goes (Windows, Linux: Ctrl+Delete)
    DeleteToWordStart,    ///< To where WordStartBackward goes (macOS: Option+Backspace)
    DeleteToWordEnd,      ///< To where WordEndForward goes (macOS: Option+Delete)
    DeleteToLineStart,    ///< To the start of the line on the screen (macOS: Cmd+Backspace)
    KillToParagraphEnd,   ///< To the end of the paragraph, kept for Yank (macOS: Control+K)

    // Typing
    NewParagraph,
    OpenLine,             ///< A paragraph break after the cursor (macOS: Control+O)
    Transpose,            ///< The characters around the cursor change places (Control+T)
    Yank,                 ///< The text the last Control+K took (macOS: Control+Y)

    // The second keys of commands, and the context menu
    Undo,                 ///< Alt+Backspace (Windows, Linux)
    Redo,                 ///< Ctrl+Shift+Z; Alt+Shift+Backspace (Windows, Linux)
    Cut,                  ///< Shift+Delete (Windows, Linux)
    Copy,                 ///< Ctrl+Insert (Windows, Linux)
    Paste,                ///< Shift+Insert (Windows, Linux)
    ContextMenu,          ///< Shift+F10, as the menu key (Windows, Linux)
};

/// @brief One key of the text
struct TextKey {
    QKeyCombination keys;                       ///< The key and its modifiers
    TextKeyAction action = TextKeyAction::None;
    bool extendSelection = false;               ///< The move selects (the key with Shift)
};

/// @brief The keys of the text on macOS, or on Windows and Linux
[[nodiscard]] const std::vector<TextKey>& textKeys(bool macOS);

/// @brief What a key press does in the text
///
/// The keypad and the group switch make no difference (macOS reports the arrows as keypad
/// keys); the other modifiers must be exactly those of the key.
/// @return The key of the text, or one with TextKeyAction::None
[[nodiscard]] TextKey textKeyFor(int key, Qt::KeyboardModifiers modifiers, bool macOS);

/// @brief Whether the program runs on macOS (its keys of the text)
constexpr bool isMacOS() {
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

}  // namespace kalahari::editor
