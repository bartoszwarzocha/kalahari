/// @file widget_wheel.h
/// @brief The mouse wheel over Qt's fields: it changes only the field one works in
///
/// Qt turns the value of a spin box, a combo box or a slider with the mouse wheel as soon
/// as the mouse is over it, and gives it the keyboard focus: scrolling a page of the
/// settings with the wheel changed every field that passed under the mouse. On Windows the
/// wheel changes only the field with the keyboard focus, and over any other field it
/// scrolls the page around it. WidgetWheel, a filter of the whole application, does the
/// same on every system.

#pragma once

#include <QObject>

class QApplication;
class QEvent;

namespace kalahari::gui {

/// @brief A filter of the application: the wheel changes only the field with the focus
///
/// A spin box, a combo box or a slider (not a scroll bar) takes the focus from a click or
/// Tab, no longer from the wheel. The wheel over it while it has no focus goes on to the
/// widget around it: a page, a list or a panel that scrolls.
class WidgetWheel : public QObject {
public:
    /// @brief The filter, to install on the application
    explicit WidgetWheel(QObject* parent = nullptr);

    /// @brief Whether the wheel turns the value of @p object: a spin box, a combo box or a
    ///        slider, not a scroll bar
    [[nodiscard]] static bool turnsValue(const QObject* object);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

/// @brief Let the wheel change only the field with the focus, in the whole application
void installWidgetWheel(QApplication& app);

}  // namespace kalahari::gui
