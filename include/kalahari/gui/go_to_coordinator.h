/// @file go_to_coordinator.h
/// @brief Edit > Go To...: the window of the places of the text, over the text that had the
///        keys last

#pragma once

#include <QMetaObject>
#include <QObject>

#include <functional>

class QWidget;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

namespace dialogs {
class GoToDialog;
}

/// @brief Opens the window of Go To and gives it the text it goes in
///
/// The window stays open while the writer goes from place to place: it goes in the book's text
/// that had the keys last (in the main window or in another one), and follows it when another
/// text gets them.
class GoToCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief The book's text with the keys; null: none
    using TextWithKeys = std::function<editor::BookEditor*()>;

    /// @brief The command this coordinator gives its callback
    static constexpr const char* COMMAND = "edit.goTo";

    /// @param textWithKeys The text with the keys
    /// @param window Over which the window opens
    /// @param parent Parent object
    GoToCoordinator(TextWithKeys textWithKeys, QWidget* window, QObject* parent = nullptr);

    GoToCoordinator(const GoToCoordinator&) = delete;
    GoToCoordinator& operator=(const GoToCoordinator&) = delete;

    /// @brief Give the command its callback
    /// @note Call after the command is registered
    void connectCommands();

    /// @brief Edit > Go To...: the window, in front, its number to type
    void open();

    /// @brief The window; nullptr: not opened yet
    [[nodiscard]] dialogs::GoToDialog* dialog() const { return m_dialog; }

    /// @brief The text the window goes in: the one with the keys, else the one that had them last
    [[nodiscard]] editor::BookEditor* text() const;

private:
    /// @brief Remember the text with the keys
    void followKeys();

    TextWithKeys m_textWithKeys;
    QWidget* m_window = nullptr;
    dialogs::GoToDialog* m_dialog = nullptr;
    editor::BookEditor* m_lastText = nullptr;
    QMetaObject::Connection m_lastTextGone;
};

}  // namespace kalahari::gui
