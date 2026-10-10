/// @file distraction_free_layout.h
/// @brief The main window's layout for Distraction-Free writing

#ifndef KALAHARI_GUI_UTILS_DISTRACTION_FREE_LAYOUT_H
#define KALAHARI_GUI_UTILS_DISTRACTION_FREE_LAYOUT_H

#include <QByteArray>
#include <QList>
#include <QObject>

class QMainWindow;
class QMenuBar;
class QPoint;
class QWidget;

namespace kalahari {
namespace gui {
namespace utils {

/// @brief Hides the main window's parts around the text and brings them back
///
/// While it is on, the window fills the screen and its menu bar, toolbars, panels and
/// status bar are hidden, like any other widgets of the window given (the chapter tabs).
/// Turned off, it brings back the layout from before: the window's size and state, the
/// places and visibility of the toolbars and panels, and the widgets it hid (one deleted
/// meanwhile is simply gone).
/// While it is on, Esc that no widget used turns it off, and the shortcuts of the menu
/// commands keep working although the menu bar is hidden. The menus show over the top of
/// the window while the mouse is at its top edge (a native menu bar, as on macOS, shows
/// them by itself).
class DistractionFreeLayout : public QObject {
    Q_OBJECT

public:
    /// @param window The window to lay out (also the parent)
    explicit DistractionFreeLayout(QMainWindow* window);

    /// @brief Whether the window's parts are hidden
    [[nodiscard]] bool isActive() const { return m_active; }

    /// @brief Hide the window's parts or bring them back
    ///
    /// Emits activeChanged if the state changes.
    /// @param active true to hide them
    /// @param widgets Other widgets in the window to hide while it is on (when turning it on)
    /// @param fullScreen Fill the screen while it is on (when turning it on)
    void setActive(bool active, const QList<QWidget*>& widgets = {}, bool fullScreen = true);

    /// @brief While it is on, let the keys the menu commands got meanwhile work too
    ///        (Settings > Keyboard Shortcuts)
    void updateShortcuts();

signals:
    /// @brief The window's parts were hidden (true) or brought back (false)
    void activeChanged(bool active);

protected:
    /// @brief Esc that reaches the window turns the layout off; the mouse in the window
    ///        shows and hides the menus at its top
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void hideParts(const QList<QWidget*>& widgets, bool fullScreen);
    void showParts();

    /// @brief The window carries the menu commands with keys while the menu bar is hidden
    void carryShortcuts();

    /// @brief Show the menus at the top edge, hide them away from it
    /// @param pos The mouse in the window (moved or pressed)
    /// @param buttonPressed A mouse button is held (a selection dragged to the top)
    void followMouse(const QPoint& pos, bool buttonPressed);

    /// @brief Show the menus of the window's menu bar over the top of the window
    void showMenus();

    /// @brief Lay the menus over the top of the window, as wide as the window
    void placeMenus();

    QMainWindow* m_window;
    bool m_active{false};
    bool m_fullScreen{false};  ///< Took the window to full screen
    QByteArray m_geometry;     ///< The window's size and state from before
    QByteArray m_state;        ///< The toolbars and panels from before
    QMenuBar* m_menus{nullptr};  ///< The menus over the top meanwhile (a child of the window)
    bool m_endingMenuKeys{false};  ///< Esc sent to the menus to give the keys back to the text
};

} // namespace utils
} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_UTILS_DISTRACTION_FREE_LAYOUT_H
