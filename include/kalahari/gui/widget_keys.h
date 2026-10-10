/// @file widget_keys.h
/// @brief The keys of Qt's own widgets: the same on Windows and Linux, those of Windows
///
/// Qt gives its text fields, lists and buttons the keys of the system it runs on, and on
/// Linux those of the desktop: there Ctrl+E goes to the end of a field's line and Ctrl+U
/// deletes it (the commands Center and Underline do not get them), Ctrl+Y is no Redo, an
/// arrow over a selection jumps to its edge, Shift+F10 opens no menu, and on GNOME and Xfce
/// Enter presses the button that has the keys instead of the window's default button.
/// WidgetKeys, a filter of the whole application, gives the widgets on Linux the keys they
/// have on Windows. On both it lets read-only texts copy what is selected in them and shows
/// the keys in the context menus of the fields. macOS keeps the keys of its own fields.

#pragma once

#include "kalahari/gui/shortcut_rules.h"

#include <QKeyCombination>
#include <QList>
#include <QObject>

class QApplication;
class QEvent;
class QKeyEvent;
class QMenu;
class QWidget;

namespace kalahari {
namespace gui {

/// @brief A filter of the application that gives Qt's widgets the keys of Windows
///
/// It takes only the keys it changes; the filters the program installs on its widgets see
/// every other key first (Enter on a button too: it goes on to the button again, through its
/// filters). A field that records shortcuts keeps every key while it records.
class WidgetKeys : public QObject {
    Q_OBJECT

public:
    /// @brief The filter for the keys of a system
    /// @param platform Linux: also the keys Qt gives the widgets of Windows only; macOS: none
    /// @param parent Parent object
    explicit WidgetKeys(ShortcutPlatform platform = currentShortcutPlatform(),
                        QObject* parent = nullptr);

    /// @brief The keys Qt gives the widgets on Linux only: on Windows they do nothing in a
    ///        field or a list, so a command may have them
    ///
    /// Ctrl+D (delete), Ctrl+E (end of the line), Ctrl+K (delete to the end of the line),
    /// Ctrl+U (delete the line), Ctrl+Shift+A (deselect), Ctrl+Shift+Insert (paste what is
    /// selected anywhere on the screen) and the Undo, Copy, Paste and Cut keys of Sun
    /// keyboards (F14, F16, F18, F20).
    [[nodiscard]] static const QList<QKeyCombination>& linuxOnlyKeys();

    /// @brief The keys of Windows to undo (Alt+Backspace) and to redo (Ctrl+Y,
    ///        Alt+Shift+Backspace) in a field; Qt gives them Windows only
    [[nodiscard]] static const QList<QKeyCombination>& windowsUndoKeys();
    [[nodiscard]] static const QList<QKeyCombination>& windowsRedoKeys();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Whether a field keeps a key from the window's shortcuts
    bool shortcutOverride(QWidget* widget, QKeyEvent* event);

    /// @brief A key pressed in a widget
    bool keyPress(QWidget* widget, QKeyEvent* event);

    /// @brief Shift+F10: the context menu of the widget with the keys, as the Menu key opens
    ///        it; on Windows the key does not reach the widgets either
    bool openContextMenu(QWidget* widget, QKeyEvent* event);

    /// @brief Enter on a button: on to the window as on Windows, through the button's
    ///        filters
    bool passEnterOn(QWidget* widget, QKeyEvent* event);

    /// @brief Deliver a copy of a key again, past this filter
    void deliverAgain(QWidget* widget, QKeyEvent* event);

    /// @brief The keys of Windows in the context menu of a field
    static void showKeys(QMenu* menu);

    bool m_active;                          ///< Not on macOS
    bool m_linux;                           ///< Also the keys Qt gives Windows only
    const QEvent* m_delivering = nullptr;   ///< The copy being delivered again
};

/// @brief Give the application's widgets the keys of Windows (on Windows and Linux)
void installWidgetKeys(QApplication& app);

} // namespace gui
} // namespace kalahari
