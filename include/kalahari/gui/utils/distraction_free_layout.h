/// @file distraction_free_layout.h
/// @brief The main window's layout for Distraction-Free writing

#ifndef KALAHARI_GUI_UTILS_DISTRACTION_FREE_LAYOUT_H
#define KALAHARI_GUI_UTILS_DISTRACTION_FREE_LAYOUT_H

#include <QByteArray>
#include <QList>
#include <QObject>

class QMainWindow;
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
/// commands keep working although the menu bar is hidden.
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

signals:
    /// @brief The window's parts were hidden (true) or brought back (false)
    void activeChanged(bool active);

protected:
    /// @brief Esc that reaches the window turns the layout off
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void hideParts(const QList<QWidget*>& widgets, bool fullScreen);
    void showParts();

    QMainWindow* m_window;
    bool m_active{false};
    bool m_fullScreen{false};  ///< Took the window to full screen
    QByteArray m_geometry;     ///< The window's size and state from before
    QByteArray m_state;        ///< The toolbars and panels from before
};

} // namespace utils
} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_UTILS_DISTRACTION_FREE_LAYOUT_H
