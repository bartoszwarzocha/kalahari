/// @file widget_wheel.h
/// @brief The mouse wheel over Qt's fields: it changes only the field one works in
///
/// Qt turns the value of a spin box, a combo box or a slider with the mouse wheel as soon
/// as the mouse is over it, and gives it the keyboard focus: scrolling a page of the
/// settings with the wheel changed every field that passed under the mouse. On Windows the
/// wheel changes only the field with the keyboard focus, and over any other field it
/// scrolls the page around it. WidgetWheel, a filter of the whole application, does the
/// same on every system, and as in web browsers the wheel that scrolls a page goes on
/// scrolling it when the field with the focus passes under the mouse.

#pragma once

#include <QObject>
#include <QPointF>

class QApplication;
class QEvent;
class QWheelEvent;

namespace kalahari::gui {

/// @brief A filter of the application: the wheel changes only the field with the focus
///
/// A spin box, a combo box or a slider (not a scroll bar) takes the focus from a click or
/// Tab, no longer from the wheel. The wheel over it while it has no focus goes on to the
/// widget around it: a page, a list or a panel that scrolls. With the focus the field turns
/// as Qt turns it on the system: on macOS a combo box does not, as there.
///
/// While the wheel scrolls a page, the field with the focus does not take it either when the
/// page brings the field under the mouse: the notches that follow within half a second, with
/// the mouse where it was, go on scrolling the page. After a pause, or once the mouse moves,
/// the wheel over the field turns it again.
class WidgetWheel : public QObject {
public:
    /// @brief The filter, to install on the application
    explicit WidgetWheel(QObject* parent = nullptr);

    /// @brief Whether the wheel turns the value of @p object: a spin box, a combo box or a
    ///        slider, not a scroll bar
    [[nodiscard]] static bool turnsValue(const QObject* object);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Whether @p wheel goes on scrolling a page: it comes soon after the last notch
    ///        that reached one, with the mouse where it was
    [[nodiscard]] bool scrollsPage(const QWheelEvent& wheel) const;

    /// @brief Remember @p wheel as a notch that reached a page, a list or a text that scrolls
    void notePageScroll(const QWheelEvent& wheel);

    bool m_pageScrolled = false;   ///< A notch has reached a page since the start
    quint64 m_pageScrollTime = 0;  ///< Time of the last such notch, in milliseconds
    QPointF m_pageScrollPosition;  ///< Where the mouse was then, on the screen
};

/// @brief Let the wheel change only the field with the focus, in the whole application
void installWidgetWheel(QApplication& app);

}  // namespace kalahari::gui
