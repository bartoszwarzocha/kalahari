/// @file edit_coordinator.h
/// @brief Undo, Redo, Cut, Copy, Paste, Delete and Select All work on the element with the
///        keys; the commands of the book's text work only there

#pragma once

#include <QMetaObject>
#include <QObject>

#include <functional>
#include <string>
#include <utility>
#include <vector>

class QWidget;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

/// @brief Gives Undo, Redo, Cut, Copy, Paste, Delete and Select All the element they work on:
///        the one with the keys
///
/// Their keys, the Edit menu and the toolbar work on the element with the keys and on no
/// other: the book's text, a field of a panel, a list. A read-only field (the Log, a field
/// that only shows) copies and selects all; Cut, Paste, Undo and Redo do nothing there. A
/// list copies the text of its current item, and selects all its items where it can select
/// more than one. Where what the writer does is a step of the text's undo history, Undo and
/// Redo work on the text: in the Annotations panel (done, removed), in the Spelling and Grammar
/// panel (a suggestion put in place) and in the find bar (Replace, Replace All), outside their
/// fields. Elsewhere (a button, the Dashboard) the commands do nothing.
///
/// The commands are enabled where they can do something: they follow the keys from element
/// to element, the element's selection and text, and the clipboard.
///
/// The commands of the book's text (addTextCommands(): Select Word, Insert > Page Break...)
/// work only with the keys in the text: in a field (the find bar, the Navigator's filter)
/// they are greyed out, and their keys stay with the field.
class EditCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief The commands, in the order of the Edit menu
    enum class Edit { Undo, Redo, Cut, Copy, Paste, Delete, SelectAll };

    /// @brief The book's text in front (nullptr: none)
    using EditorInFront = std::function<editor::BookEditor*()>;

    /// @brief Constructor
    /// @param editorInFront The text the Annotations and the Spelling and Grammar panels
    ///        change: the one in front
    /// @param parent Parent object
    explicit EditCoordinator(EditorInFront editorInFront, QObject* parent = nullptr);

    /// @brief Destructor: the commands get back the state they had before connectCommands()
    ///
    /// They stay in the registry, which outlives the window.
    ~EditCoordinator() override;

    /// @brief Enable the commands where they can work, and follow the keys
    void connectCommands();

    /// @brief The element with the keys
    ///
    /// The menu bar takes the keys in its keyboard mode (Alt, F10), and a click on a floating
    /// toolbar may give them to its buttons: the commands given there work on the element
    /// that had the keys before.
    [[nodiscard]] QWidget* focusedElement() const;

    /// @brief Whether a command can do something on an element
    [[nodiscard]] bool canDo(Edit edit, QWidget* element) const;

    /// @brief Do a command on an element
    /// @return Whether it did something (where it cannot, it does nothing)
    bool execute(Edit edit, QWidget* element);

    /// @brief Do a command on the element with the keys
    bool execute(Edit edit) { return execute(edit, focusedElement()); }

    /// @brief Enable the commands again, as the element with the keys allows
    void updateStates();

    /// @brief Whether a text command can work in the text with the keys
    using TextCan = std::function<bool(const editor::BookEditor&)>;

    /// @brief The book's text with the keys, or nullptr (the keys in a field, a list, a panel,
    ///        nowhere)
    [[nodiscard]] editor::BookEditor* textWithKeys() const;

    /// @brief Commands that work only in the book's text with the keys: enabled there as
    ///        @p can allows, greyed out elsewhere; they follow the keys with the Edit commands
    /// @param ids The commands' ids in the registry
    void addTextCommands(const std::vector<std::string>& ids, const TextCan& can);

private:
    struct Target;

    /// @brief What a command works on, for the element with the keys
    [[nodiscard]] Target targetOf(QWidget* element) const;

    /// @brief Remember the element that had the keys last, outside the menus and toolbars
    void setLastFocus(QWidget* element);

    /// @brief Follow the changes of the element with the keys that enable or disable commands
    void follow(QWidget* element);

    /// @brief Follow no element and no text
    void stopFollowing();

    EditorInFront m_editorInFront;
    // The elements are forgotten when they go: another one may come at the same address
    QWidget* m_lastFocus = nullptr;  ///< Had the keys last, outside the menus and toolbars
    QMetaObject::Connection m_lastFocusGone;  ///< Forgets m_lastFocus when it goes
    QWidget* m_followed = nullptr;       ///< The element whose changes are followed
    QObject* m_followedText = nullptr;   ///< The book's text whose changes are followed
    std::vector<QMetaObject::Connection> m_connections;  ///< To the followed element and text
    std::vector<std::pair<std::string, TextCan>> m_textCommands;  ///< addTextCommands()
};

}  // namespace kalahari::gui
