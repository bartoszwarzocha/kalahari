/// @file text_keys.cpp
/// @brief The keys of the text on Windows and Linux, and on macOS

#include <kalahari/editor/text_keys.h>

#include <initializer_list>

namespace kalahari::editor {

namespace {

using Action = TextKeyAction;

constexpr Qt::KeyboardModifiers NONE = Qt::NoModifier;
constexpr Qt::KeyboardModifiers SHIFT = Qt::ShiftModifier;
constexpr Qt::KeyboardModifiers CTRL = Qt::ControlModifier;  // Cmd on macOS
constexpr Qt::KeyboardModifiers ALT = Qt::AltModifier;       // Option on macOS
constexpr Qt::KeyboardModifiers META = Qt::MetaModifier;     // Control on macOS

/// @brief Builds a table of keys
class Table {
public:
    /// A key that does something
    Table& key(Qt::Key key, Qt::KeyboardModifiers modifiers, Action action, bool select = false) {
        m_keys.push_back({QKeyCombination(modifiers, key), action, select});
        return *this;
    }

    /// A key that moves the cursor, and the same with Shift, which selects
    Table& move(Qt::Key key, Qt::KeyboardModifiers modifiers, Action action) {
        return this->key(key, modifiers, action).key(key, modifiers | SHIFT, action, true);
    }

    /// Enter of the main keyboard and of the keypad starts a paragraph
    Table& newParagraph(std::initializer_list<Qt::KeyboardModifiers> modifiers) {
        for (const Qt::Key enter : {Qt::Key_Return, Qt::Key_Enter}) {
            for (const Qt::KeyboardModifiers modifier : modifiers) {
                key(enter, modifier, Action::NewParagraph);
            }
        }
        return *this;
    }

    [[nodiscard]] std::vector<TextKey> keys() const { return m_keys; }

private:
    std::vector<TextKey> m_keys;
};

/// Windows and Linux: the keys of Windows word processors and text fields
std::vector<TextKey> windowsKeys() {
    Table table;
    table.move(Qt::Key_Left, NONE, Action::CharacterLeft)
        .move(Qt::Key_Right, NONE, Action::CharacterRight)
        .move(Qt::Key_Left, CTRL, Action::WordLeft)
        .move(Qt::Key_Right, CTRL, Action::WordRight)
        .move(Qt::Key_Up, NONE, Action::LineUp)
        .move(Qt::Key_Down, NONE, Action::LineDown)
        .move(Qt::Key_Up, CTRL, Action::ParagraphUp)
        .move(Qt::Key_Down, CTRL, Action::ParagraphDown)
        .move(Qt::Key_Home, NONE, Action::LineStart)
        .move(Qt::Key_End, NONE, Action::LineEnd)
        .move(Qt::Key_Home, CTRL, Action::DocumentStart)
        .move(Qt::Key_End, CTRL, Action::DocumentEnd)
        .move(Qt::Key_PageUp, NONE, Action::PageUp)
        .move(Qt::Key_PageDown, NONE, Action::PageDown)
        .key(Qt::Key_Backspace, NONE, Action::DeleteBackward)
        .key(Qt::Key_Backspace, SHIFT, Action::DeleteBackward)
        .key(Qt::Key_Delete, NONE, Action::DeleteForward)
        .key(Qt::Key_Backspace, CTRL, Action::DeleteWordBackward)
        .key(Qt::Key_Delete, CTRL, Action::DeleteWordForward)
        .newParagraph({NONE, SHIFT, CTRL})
        // The second keys of the commands, those of every Windows text field
        .key(Qt::Key_Z, CTRL | SHIFT, Action::Redo)
        .key(Qt::Key_Backspace, ALT, Action::Undo)
        .key(Qt::Key_Backspace, ALT | SHIFT, Action::Redo)
        .key(Qt::Key_Delete, SHIFT, Action::Cut)
        .key(Qt::Key_Insert, CTRL, Action::Copy)
        .key(Qt::Key_Insert, SHIFT, Action::Paste)
        .key(Qt::Key_F10, SHIFT, Action::ContextMenu);
    return table.keys();
}

/// macOS: the keys of its text fields (Cmd is Ctrl, Option is Alt, Control is Meta)
std::vector<TextKey> macKeys() {
    Table table;
    table.move(Qt::Key_Left, NONE, Action::CharacterLeft)
        .move(Qt::Key_Right, NONE, Action::CharacterRight)
        .move(Qt::Key_Left, ALT, Action::WordStartBackward)
        .move(Qt::Key_Right, ALT, Action::WordEndForward)
        .move(Qt::Key_Left, CTRL, Action::LineStart)
        .move(Qt::Key_Right, CTRL, Action::LineEnd)
        .move(Qt::Key_Up, NONE, Action::LineUp)
        .move(Qt::Key_Down, NONE, Action::LineDown)
        .move(Qt::Key_Up, ALT, Action::ParagraphUp)
        .move(Qt::Key_Down, ALT, Action::ParagraphEndForward)
        .move(Qt::Key_Up, CTRL, Action::DocumentStart)
        .move(Qt::Key_Down, CTRL, Action::DocumentEnd)
        // Home and End, Page Up and Page Down move the view only; with Shift they select,
        // with Option Page Up and Page Down move the cursor
        .key(Qt::Key_Home, NONE, Action::ScrollToStart)
        .key(Qt::Key_End, NONE, Action::ScrollToEnd)
        .key(Qt::Key_Home, SHIFT, Action::DocumentStart, true)
        .key(Qt::Key_End, SHIFT, Action::DocumentEnd, true)
        .key(Qt::Key_PageUp, NONE, Action::ScrollPageUp)
        .key(Qt::Key_PageDown, NONE, Action::ScrollPageDown)
        .key(Qt::Key_PageUp, ALT, Action::PageUp)
        .key(Qt::Key_PageDown, ALT, Action::PageDown)
        .key(Qt::Key_PageUp, SHIFT, Action::PageUp, true)
        .key(Qt::Key_PageDown, SHIFT, Action::PageDown, true)
        .key(Qt::Key_Backspace, NONE, Action::DeleteBackward)
        .key(Qt::Key_Backspace, SHIFT, Action::DeleteBackward)
        .key(Qt::Key_Delete, NONE, Action::DeleteForward)
        .key(Qt::Key_Delete, SHIFT, Action::DeleteForward)
        .key(Qt::Key_Backspace, ALT, Action::DeleteToWordStart)
        .key(Qt::Key_Delete, ALT, Action::DeleteToWordEnd)
        .key(Qt::Key_Backspace, CTRL, Action::DeleteToLineStart)
        .key(Qt::Key_Backspace, META, Action::DeleteDiacritic)
        .newParagraph({NONE, SHIFT, CTRL, ALT})
        .key(Qt::Key_Z, CTRL | SHIFT, Action::Redo)
        // The Control keys of the text fields of macOS
        .move(Qt::Key_A, META, Action::ParagraphStart)
        .move(Qt::Key_E, META, Action::ParagraphEnd)
        .move(Qt::Key_B, META, Action::CharacterLeft)
        .move(Qt::Key_F, META, Action::CharacterRight)
        .move(Qt::Key_P, META, Action::LineUp)
        .move(Qt::Key_N, META, Action::LineDown)
        .move(Qt::Key_V, META, Action::PageDown)
        .key(Qt::Key_H, META, Action::DeleteBackward)
        .key(Qt::Key_D, META, Action::DeleteForward)
        .key(Qt::Key_K, META, Action::KillToParagraphEnd)
        .key(Qt::Key_Y, META, Action::Yank)
        .key(Qt::Key_T, META, Action::Transpose)
        .key(Qt::Key_O, META, Action::OpenLine)
        .key(Qt::Key_L, META, Action::CenterCursor);
    return table.keys();
}

}  // namespace

const std::vector<TextKey>& textKeys(bool macOS) {
    static const std::vector<TextKey> windows = windowsKeys();
    static const std::vector<TextKey> mac = macKeys();
    return macOS ? mac : windows;
}

TextKey textKeyFor(int key, Qt::KeyboardModifiers modifiers, bool macOS) {
    const Qt::KeyboardModifiers relevant =
        modifiers & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);
    for (const TextKey& textKey : textKeys(macOS)) {
        if (textKey.keys.key() == key && textKey.keys.keyboardModifiers() == relevant) {
            return textKey;
        }
    }
    return {};
}

}  // namespace kalahari::editor
