/// @file widget_wheel.cpp
/// @brief Implementation of WidgetWheel

#include "kalahari/gui/widget_wheel.h"

#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QScrollBar>
#include <QWheelEvent>

namespace kalahari::gui {

namespace {

/// A longer pause between two notches ends a turn of the wheel, as in web browsers
constexpr quint64 PAGE_SCROLL_PAUSE_MS = 500;

/// A longer move of the mouse between two notches ends a turn of the wheel, as in browsers
constexpr qreal PAGE_SCROLL_SLOP = 10.0;

/// Whether @p object is the viewport of a scroll area: a page, a list or a text that the
/// wheel scrolls
bool isScrolledViewport(const QObject* object)
{
    const auto* area = qobject_cast<const QAbstractScrollArea*>(
        object != nullptr ? object->parent() : nullptr);
    return area != nullptr && area->viewport() == object;
}

}  // anonymous namespace

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
    } else if (event->type() == QEvent::Wheel) {
        const auto& wheel = *static_cast<QWheelEvent*>(event);
        if (turnsValue(watched)) {
            if (!static_cast<QWidget*>(watched)->hasFocus() || scrollsPage(wheel)) {
                // Not taken: Qt gives the wheel to the widget around the field, which scrolls
                event->ignore();
                return true;
            }
            // The field turns: a new turn of the wheel, which began over it
            m_pageScrolled = false;
        } else if (isScrolledViewport(watched)) {
            notePageScroll(wheel);
        }
    }
    return QObject::eventFilter(watched, event);
}

bool WidgetWheel::scrollsPage(const QWheelEvent& wheel) const
{
    // The time of the system goes on; an earlier time (a new count) ends the turn too
    return m_pageScrolled && wheel.timestamp() - m_pageScrollTime < PAGE_SCROLL_PAUSE_MS &&
           (wheel.globalPosition() - m_pageScrollPosition).manhattanLength() < PAGE_SCROLL_SLOP;
}

void WidgetWheel::notePageScroll(const QWheelEvent& wheel)
{
    m_pageScrolled = true;
    m_pageScrollTime = wheel.timestamp();
    m_pageScrollPosition = wheel.globalPosition();
}

void installWidgetWheel(QApplication& app)
{
    app.installEventFilter(new WidgetWheel(&app));
}

}  // namespace kalahari::gui
