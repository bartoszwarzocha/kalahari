/// @file toolbar_keys.cpp
/// @brief Implementation of ToolbarKeys

#include "kalahari/gui/toolbar_keys.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QChildEvent>
#include <QComboBox>
#include <QCursor>
#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QToolBar>
#include <QWidget>
#include <QWindow>

namespace kalahari::gui {

ToolbarKeys::ToolbarKeys(QWidget* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow) {
    for (QToolBar* toolbar : mainWindow->findChildren<QToolBar*>(Qt::FindDirectChildrenOnly)) {
        watchToolbar(toolbar);
    }
    mainWindow->installEventFilter(this);  // toolbars made later

    connect(qApp, &QGuiApplication::focusWindowChanged, this,
            &ToolbarKeys::onActiveWindowChanged);
    connect(qApp, &QApplication::focusChanged, this, &ToolbarKeys::onFocusChanged);
    onFocusChanged(nullptr, QApplication::focusWidget());
    onActiveWindowChanged();
}

bool ToolbarKeys::isField(const QWidget* widget, const QToolBar* toolbar) {
    if (widget == nullptr || toolbar == nullptr || !toolbar->isAncestorOf(widget)) {
        return false;
    }

    // A size field's own area is its arrows, which work as buttons: its text is a child
    if (qobject_cast<const QAbstractSpinBox*>(widget) != nullptr) {
        return false;
    }

    // The font box takes the keys at a click, its text and its arrow alike; the buttons do not
    for (const QWidget* part = widget; part != toolbar; part = part->parentWidget()) {
        if (part->isEnabled() && (part->focusPolicy() & Qt::ClickFocus) != 0) {
            return true;
        }
    }
    return false;
}

bool ToolbarKeys::eventFilter(QObject* watched, QEvent* event) {
    switch (event->type()) {
    case QEvent::ChildAdded:
    case QEvent::ChildPolished: {
        // A toolbar or a field made later. ChildAdded comes before a child made with its parent
        // is made (it is not a toolbar yet), and ChildPolished does not come again for one
        // moved there
        QObject* child = static_cast<QChildEvent*>(event)->child();
        if (watched == m_mainWindow) {
            if (auto* toolbar = qobject_cast<QToolBar*>(child)) {
                watchToolbar(toolbar);
            }
        } else if (child->isWidgetType() && qobject_cast<QToolBar*>(watched) != nullptr) {
            watchField(static_cast<QWidget*>(child));
        }
        break;
    }
    case QEvent::FocusIn:
        // A size field took the keys at a click: onFocusChanged() sees whether on its arrows
        if (static_cast<QFocusEvent*>(event)->reason() == Qt::MouseFocusReason
            && qobject_cast<QAbstractSpinBox*>(watched) != nullptr) {
            m_clickedField = watched;
        }
        break;
    case QEvent::KeyPress: {
        // Enter and Escape close a field, its value taken or left: the keys go back once the
        // field has done with them. Only the field with the keys: the key comes to the toolbar
        // and the window too, when the field leaves it
        const int key = static_cast<QKeyEvent*>(event)->key();
        if (watched != QApplication::focusWidget()
            || (key != Qt::Key_Return && key != Qt::Key_Enter && key != Qt::Key_Escape)) {
            break;
        }
        auto* combo = qobject_cast<QComboBox*>(watched);
        if (key == Qt::Key_Escape && combo != nullptr && combo->isEditable()
            && combo->currentIndex() >= 0) {
            combo->setEditText(combo->itemText(combo->currentIndex()));  // what was typed goes
        }
        giveBackKeysLater();
        break;
    }
    case QEvent::Show:
    case QEvent::Hide: {
        // A field's list, a window of its own over everything
        const auto* list = qobject_cast<QWidget*>(watched);
        const auto* combo =
            list != nullptr && list->isWindow() ? qobject_cast<QComboBox*>(list->parentWidget())
                                                : nullptr;
        if (combo == nullptr) {
            break;
        }
        if (event->type() == QEvent::Show) {
            m_openList = combo;
        } else {
            onListClosed(combo);
        }
        break;
    }
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

void ToolbarKeys::watchToolbar(QToolBar* toolbar) {
    // Shown floating (at the start, from the View menu) it does not take the keys
    toolbar->setAttribute(Qt::WA_ShowWithoutActivating);
    connect(toolbar, &QToolBar::actionTriggered, this, &ToolbarKeys::onCommand,
            Qt::UniqueConnection);

    toolbar->installEventFilter(this);  // fields made later
    for (QWidget* field : toolbar->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
        watchField(field);
    }
}

void ToolbarKeys::watchField(QWidget* field) {
    auto* combo = qobject_cast<QComboBox*>(field);
    if (combo == nullptr && qobject_cast<QAbstractSpinBox*>(field) == nullptr) {
        return;
    }

    // The keys that close it, and a click on its arrows; its text passes the keys to it
    field->installEventFilter(this);
    if (combo != nullptr) {
        combo->view()->window()->installEventFilter(this);  // its list opens and closes
        connect(combo, &QComboBox::activated, this, &ToolbarKeys::onItemChosen,
                Qt::UniqueConnection);
    }
}

void ToolbarKeys::onActiveWindowChanged() {
    // A click on a floating toolbar not in one of its fields (a button, the arrows of a size
    // field, its handle), or the system giving it the keys, gives them back once the click is
    // taken: on Windows the toolbar takes them before the click comes
    const QToolBar* toolbar = floatingToolbar(QApplication::activeWindow());
    if (toolbar != nullptr && !isField(QApplication::widgetAt(QCursor::pos()), toolbar)) {
        giveBackKeysLater();
    }
}

void ToolbarKeys::onFocusChanged(QWidget* old, QWidget* now) {
    if (now == nullptr) {
        return;
    }
    if (toolbarOf(now) == nullptr) {
        // Not a menu over everything: it is not where the keys go back
        if (now->window()->windowType() != Qt::Popup) {
            setLastElement(now);
        }
        return;
    }

    // A click on the arrows of a size field: they work as buttons and do not take the keys,
    // which go back to the element of this window that had them. A field of a floating
    // toolbar that was not active takes them with the window (onActiveWindowChanged())
    if (now == m_clickedField) {
        m_clickedField = nullptr;
        if (old != nullptr && old->window() == now->window()
            && QApplication::widgetAt(QCursor::pos()) == now) {
            QTimer::singleShot(0, old, [old]() {
                if (old->isVisible() && old->isEnabled()) {
                    old->setFocus(Qt::OtherFocusReason);
                }
            });
        }
    }
}

void ToolbarKeys::onCommand() {
    giveBackKeys();
}

void ToolbarKeys::onItemChosen() {
    // Only a choice in the list: the arrows in the field go through the items, the keys there
    const QObject* combo = sender();
    if (combo == nullptr || combo != m_openList) {
        return;
    }
    m_openList = nullptr;

    // The list may still be shown: it flashes the item first on macOS
    const auto* box = qobject_cast<const QComboBox*>(combo);
    if (box != nullptr && box->view()->window()->isVisible()) {
        m_chosenInList = combo;
    } else {
        giveBackKeysLater();
    }
}

void ToolbarKeys::onListClosed(const QComboBox* combo) {
    if (m_chosenInList == combo) {
        m_chosenInList = nullptr;
        giveBackKeysLater();
    }

    // The choice comes right after the list is closed (Windows, Linux): the list counts as
    // open until then
    QTimer::singleShot(0, this, [this, combo]() {
        if (m_openList == combo) {
            m_openList = nullptr;
        }
    });
}

void ToolbarKeys::giveBackKeys() {
    // A menu over everything (the toolbar's context menu, a field's list) keeps them while it
    // is open
    if (QApplication::activePopupWidget() != nullptr) {
        return;
    }
    const QToolBar* floating = floatingToolbar(QApplication::activeWindow());
    if (floating == nullptr && toolbarOf(QApplication::focusWidget()) == nullptr) {
        return;  // no toolbar has them
    }

    // The element takes them, where they were in it, once its window is active
    QWidget* element = m_lastElement;
    if (element != nullptr && element->isVisible() && element->isEnabled()
        && !element->window()->isMinimized()) {
        element->setFocus(Qt::OtherFocusReason);
        if (!element->isActiveWindow()) {
            element->activateWindow();
        }
    } else if (floating != nullptr) {
        m_mainWindow->activateWindow();
    }
}

void ToolbarKeys::giveBackKeysLater() {
    QTimer::singleShot(0, this, [this]() { giveBackKeys(); });
}

const QToolBar* ToolbarKeys::toolbarOf(const QWidget* widget) const {
    // Up through the windows too: a field's list is a window of its own
    for (const QWidget* part = widget; part != nullptr; part = part->parentWidget()) {
        if (part->parentWidget() == m_mainWindow) {
            return qobject_cast<const QToolBar*>(part);
        }
    }
    return nullptr;
}

QToolBar* ToolbarKeys::floatingToolbar(QWidget* window) const {
    auto* toolbar = qobject_cast<QToolBar*>(window);
    return toolbar != nullptr && toolbar->isFloating() && toolbar->parentWidget() == m_mainWindow
               ? toolbar
               : nullptr;
}

void ToolbarKeys::setLastElement(QWidget* element) {
    if (element == m_lastElement) {
        return;
    }
    disconnect(m_lastElementGone);
    m_lastElement = element;
    m_lastElementGone =
        connect(element, &QObject::destroyed, this, [this]() { m_lastElement = nullptr; });
}

}  // namespace kalahari::gui
