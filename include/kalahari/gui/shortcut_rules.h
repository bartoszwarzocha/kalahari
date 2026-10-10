/// @file shortcut_rules.h
/// @brief Which keys a command can have: the keys of the text and of the system are not theirs
///
/// Settings > Keyboard Shortcuts checks every key the user gives a command, and the
/// program checks the keys it reads from the settings. A key is refused when it types
/// text, works in the text or in a bar of the program (the fixed keys) or belongs to the
/// system; a key some desktops take for themselves only brings a warning. The fixed keys
/// and the system's are also shown at the end of the list of the shortcuts.

#pragma once

#include <QCoreApplication>
#include <QKeyCombination>
#include <QList>
#include <QString>
#include <QStringList>

#include <string>
#include <vector>

class QKeyEvent;

namespace kalahari {
namespace editor {
enum class TextKeyAction;
}

namespace gui {

/// @brief The system whose keys the rules follow
enum class ShortcutPlatform { Windows, Linux, MacOS };

/// @brief The system the program runs on
[[nodiscard]] constexpr ShortcutPlatform currentShortcutPlatform() {
#if defined(Q_OS_MACOS)
    return ShortcutPlatform::MacOS;
#elif defined(Q_OS_WIN)
    return ShortcutPlatform::Windows;
#else
    return ShortcutPlatform::Linux;
#endif
}

/// @brief Keys that do one thing outside the commands
struct FixedKeys {
    QString label;                                  ///< What they do ("Word left / right")
    QList<QKeyCombination> keys;                    ///< The keys
    Qt::KeyboardModifiers anyKeyWith = Qt::NoModifier;  ///< Also every key with these modifiers
    QString anyKeyText;                             ///< How those keys are written ("Ctrl+Alt+…")
    QString reason;                                 ///< Why a command cannot have them
                                                    ///< (%1: the keys)
    std::string allowedFor;                         ///< The one command that may have them
    QString summary = {};                           ///< The keys as the list shows them, when
                                                    ///< a list of them all would not do
    QStringList keyReasons = {};                    ///< Why, key by key, in the order of keys
                                                    ///< (in place of reason)
    QString brief = {};                             ///< Why, in a few words, for a summary

    /// @brief Whether the keys are among these
    [[nodiscard]] bool covers(QKeyCombination pressed) const;

    /// @brief The keys as the list shows them, as the system writes them (or the summary)
    [[nodiscard]] QString keysText() const;

    /// @brief Why a command cannot have keys among these: what they do
    [[nodiscard]] QString reasonFor(QKeyCombination pressed) const;
};

/// @brief The fixed keys of one place of the program, or the keys of the system
struct FixedKeyGroup {
    QString title;                ///< As the list shows it ("Fixed: in the text")
    std::vector<FixedKeys> rows;
    bool system = false;          ///< The keys of the system: the program does not get them
    bool local = false;           ///< They work only in a list with the keys: a command may
                                  ///< have them too
};

/// @brief What the rules say of a key for a command
struct KeyCheck {
    enum class Result {
        Allowed,  ///< The command may have it
        Warning,  ///< It may not reach the program on some desktops; the user decides
        Refused   ///< The command cannot have it
    };
    Result result = Result::Allowed;
    QString reason;      ///< Why (Warning, Refused)
    QString brief = {};  ///< Why, in a few words, for a summary (Refused)
};

/// @brief The rules of the keys of the commands on one system
class ShortcutRules {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::ShortcutRules)

public:
    /// @brief The rules of a system (by default the one the program runs on)
    explicit ShortcutRules(ShortcutPlatform platform = currentShortcutPlatform());

    /// @brief The system the rules follow
    [[nodiscard]] ShortcutPlatform platform() const { return m_platform; }

    /// @brief The keys that are not the commands', grouped where they work; the system's last
    [[nodiscard]] std::vector<FixedKeyGroup> fixedGroups() const;

    /// @brief Whether a command may have the keys
    /// @param keys The keys (keypad or not makes no difference)
    /// @param commandId The command (only Exit may have the key that closes the window)
    [[nodiscard]] KeyCheck check(QKeyCombination keys, const std::string& commandId) const;

    /// @brief Keys as the system writes them (on macOS with the symbols of the keys)
    ///
    /// The Windows key and the Super key, which Qt writes as Meta, go by their own names.
    [[nodiscard]] static QString keysText(QKeyCombination keys);

    /// @brief The keys of a key press as the shortcuts are written
    ///
    /// The keypad makes no difference; Shift+Tab is Tab with Shift; a key that already is
    /// the character Shift types (! for Shift+1) is without Shift, as Qt matches it.
    /// @return The keys, or Qt::Key_unknown for a modifier pressed alone
    [[nodiscard]] static QKeyCombination keysOf(const QKeyEvent* event);

private:
    std::vector<FixedKeyGroup> textGroups() const;
    FixedKeyGroup systemGroup() const;

    /// @brief Why the keys type text, or empty when they do not
    QString typingReason(QKeyCombination keys) const;

    /// @brief A warning about keys some desktops take, or empty
    QString desktopWarning(QKeyCombination keys) const;

    /// @brief Why a key of the text is fixed: what it does there
    static QString textKeyReason(QKeyCombination keys, editor::TextKeyAction action,
                                 bool selecting);

    /// @brief What a fixed key does, and that a command cannot have it
    static QString fixedKeyReason(const QString& does);

    ShortcutPlatform m_platform;
};

} // namespace gui
} // namespace kalahari
