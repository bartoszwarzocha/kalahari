/// @file character_dialogs.h
/// @brief The small windows of Insert > Special Character...: the keys of a character, and a
///        replacement of auto-correct that gives a character

#pragma once

#include "kalahari/gui/command.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QKeyCombination>
#include <QString>

#include <string>

class QLabel;
class QLineEdit;
class QPushButton;

namespace kalahari {
namespace gui {

class ShortcutRecorder;

namespace dialogs {

/// @brief Assign Shortcut...: the keys that insert a character
///
/// It records the keys pressed at once. Keys no command can have (ShortcutRules) are refused,
/// saying why, and the next keys are recorded; keys another command has are given here only
/// after the window says which command will be left without them. Remove Shortcut takes the
/// keys from the character. The keys are then on the page Settings > Keyboard Shortcuts too.
class CharacterShortcutDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param code The character
    /// @param commandId The command that inserts it (character_commands.h)
    /// @param keysNow The keys it has now; none: none
    /// @param parent Parent widget
    CharacterShortcutDialog(char32_t code, std::string commandId,
                            const KeyboardShortcut& keysNow, QWidget* parent = nullptr);

    /// @brief The keys chosen; none: the character is left without keys
    [[nodiscard]] KeyboardShortcut keys() const { return m_keys; }

    /// @brief Take a key press as recorded (as the field does)
    void record(QKeyCombination keys);

private:
    /// @brief Record the next key press
    void startRecording();

    /// @brief Show a message under the field; empty: none
    void showMessage(const QString& html);

    std::string m_commandId;
    KeyboardShortcut m_keysNow;
    KeyboardShortcut m_keys;
    ShortcutRecorder* m_field = nullptr;
    QLabel* m_message = nullptr;
    QPushButton* m_recordButton = nullptr;
    QPushButton* m_removeButton = nullptr;
};

/// @brief Add Replacement...: a word auto-correct replaces with a character as it is typed
///
/// The pair goes to the replacement table (Settings > Editor > Auto-correct). A word with a
/// space cannot be one; a word the table has already gets the character in place of its
/// replacement, and the window says so.
class ReplacementDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param code The character
    /// @param parent Parent widget
    explicit ReplacementDialog(char32_t code, QWidget* parent = nullptr);

    /// @brief The word typed, without spaces around it
    [[nodiscard]] QString word() const;

    /// @brief The character, as text
    [[nodiscard]] QString replacement() const { return m_replacement; }

    /// @brief The field of the word
    [[nodiscard]] QLineEdit* wordField() const { return m_field; }

private:
    /// @brief Say what Add will do, and whether it can
    void updateNote();

    QString m_replacement;
    QLineEdit* m_field = nullptr;
    QLabel* m_note = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
