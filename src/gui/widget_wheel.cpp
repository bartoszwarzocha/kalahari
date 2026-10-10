/// @file widget_wheel.cpp
/// @brief Implementation of WidgetWheel

#include "kalahari/gui/widget_wheel.h"

#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QScrollBar>

namespace kalahari::gui {

WidgetWheel::WidgetWheel(QObject* parent)
    : QObject(parent)
{
}

bool WidgetWheel::turnsValue(const QObject* object)
{
    return qobject_cast<const QAbstractSpinBox*>(object) != nullptr ||
           qobject_cast<const QComboBox*>(object) != nullptr ||
           (qobject_cast<const QAbstractSlider*>(object) != nullptr &&
            qobject_cast<const QScrollBar*>(object) == nullptr);
}

bool WidgetWheel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Polish && turnsValue(watched)) {
        // A click or Tab gives such a field the focus; the wheel no longer does, before the
        // field gets it
        auto* field = static_cast<QWidget*>(watched);
        if (field->focusPolicy() == Qt::WheelFocus) {
            field->setFocusPolicy(Qt::StrongFocus);
        }
    } else if (event->type() == QEvent::Wheel && turnsValue(watched) &&
               !static_cast<QWidget*>(watched)->hasFocus()) {
        // Not taken: Qt gives the wheel to the widget around the field, which scrolls
        event->ignore();
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void installWidgetWheel(QApplication& app)
{
    app.installEventFilter(new WidgetWheel(&app));
}

}  // namespace kalahari::gui
