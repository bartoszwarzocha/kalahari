/// @file special_character_coordinator.h
/// @brief Insert > Special Character... and Insert > Quick Character: the window and the
///        palette, the characters they insert in the text in front, the keys of the characters
///        and the replacements of auto-correct the window adds

#pragma once

#include "kalahari/gui/character_commands.h"
#include "kalahari/gui/command.h"
#include "kalahari/gui/dialogs/special_character_dialog.h"

#include <QObject>
#include <QString>

#include <functional>
#include <string>
#include <vector>

class QStatusBar;
class QWidget;

namespace kalahari {
namespace editor {
class BookEditor;
}
namespace gui {

class QuickCharacterPalette;

/// @brief The window of Insert > Special Character..., the palette of Insert > Quick Character
///        and what they do in the program
///
/// The window and the palette insert in the text of the chapter in front, at its cursor, as
/// the commands of Insert > Formatting Character do: an undo step of its own, without
/// auto-correct. The palette opens under the cursor, and its All Characters... opens the
/// window. The characters given keys there (Assign Shortcut...) get commands of their own,
/// outside the menus, which work as the commands of the Insert menu: in the book's text with
/// the keys.
/// The keys are the user's keys of Settings > Keyboard Shortcuts (keyboard.shortcuts).
class SpecialCharacterCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief The book's text in front (nullptr: none)
    using EditorInFront = std::function<editor::BookEditor*()>;

    /// @brief Make commands work only in the book's text with the keys, greyed out in a book
    ///        open read-only (EditCoordinator::addTextCommands())
    using TextCommands = std::function<void(const std::vector<std::string>& ids)>;

    /// @param window The main window: the window of the characters stays in front of it, and
    ///        it gets the actions of the commands of the characters (their keys work there)
    /// @param statusBar Where it says what it did (may be null)
    /// @param editorInFront The text it inserts in
    /// @param insertWithKeys Inserts characters in the text with the keys (the commands of the
    ///        characters)
    /// @param textCommands Makes the commands of the characters commands of the text
    /// @param parent Parent object
    SpecialCharacterCoordinator(QWidget* window, QStatusBar* statusBar,
                                EditorInFront editorInFront, InsertCharacters insertWithKeys,
                                TextCommands textCommands, QObject* parent = nullptr);

    ~SpecialCharacterCoordinator() override;
    SpecialCharacterCoordinator(const SpecialCharacterCoordinator&) = delete;
    SpecialCharacterCoordinator& operator=(const SpecialCharacterCoordinator&) = delete;

    /// @brief Register the commands of the characters the writer gave keys
    /// @note Call after the user's keys are given to the registry
    void connectCommands();

    /// @brief Open the window, or bring it to the front
    void openWindow();

    /// @brief Open the palette under the cursor of the text in front (nothing without a text
    ///        or in a book open read-only, where the status bar says why)
    void openPalette();

    /// @brief The window, once opened (nullptr before)
    [[nodiscard]] dialogs::SpecialCharacterDialog* dialog() const { return m_dialog; }

    /// @brief The palette while it is open (nullptr else)
    [[nodiscard]] QuickCharacterPalette* quickPalette() const { return m_palette; }

    /// @brief Insert a character in the text in front, at the cursor
    /// @return Whether it was inserted (no text, a book open read-only: not)
    bool insert(char32_t code);

    /// @brief Give a character keys (Assign Shortcut...), or take them; another command that
    ///        has them is left without them
    void setCharacterKeys(char32_t code, const KeyboardShortcut& keys);

    /// @brief The keys of a character as the system writes them; empty: none
    [[nodiscard]] static QString keysOf(char32_t code);

    /// @brief Why the window cannot insert now; empty: it can
    [[nodiscard]] QString whyNotInsertable() const;

private:
    /// @brief Assign Shortcut...: the small window, then the keys
    void chooseShortcut(char32_t code);

    /// @brief Add Replacement...: the small window, then the pair in the table
    void chooseReplacement(char32_t code);

    /// @brief The command of a character outside the menus, working with its keys
    void addCharacterCommand(char32_t code);

    /// @brief Make the actions of commands of the characters work in the window
    void attachCommands(const std::vector<std::string>& ids);

    /// @brief Say something in the status bar
    void showMessage(const QString& message);

    QWidget* m_window;
    QStatusBar* m_statusBar;
    EditorInFront m_editorInFront;
    InsertCharacters m_insertWithKeys;
    TextCommands m_textCommands;
    dialogs::SpecialCharacterDialog* m_dialog = nullptr;  ///< Null until opened, and once gone
    QuickCharacterPalette* m_palette = nullptr;            ///< Null while it is not open
    QWidget* m_lastEditor = nullptr;  ///< The text the window inserted in last (null: gone)
    QMetaObject::Connection m_lastEditorGone;
    QMetaObject::Connection m_focusConnection;
};

} // namespace gui
} // namespace kalahari
