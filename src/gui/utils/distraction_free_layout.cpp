/// @file distraction_free_layout.cpp
/// @brief Implementation of the main window's layout for Distraction-Free writing

#include "kalahari/gui/utils/distraction_free_layout.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QScopedValueRollback>
#include <QStatusBar>
#include <QToolBar>
#include <QVariant>

namespace kalahari {
namespace gui {
namespace utils {

namespace {

/// Marks the widgets the layout hid and the menu commands it gave to the window, so that
/// it brings back exactly those (one deleted meanwhile is simply gone)
constexpr char HIDDEN_PROPERTY[] = "kalahariDistractionFreeHidden";
constexpr char SHORTCUT_PROPERTY[] = "kalahariDistractionFreeShortcut";

/// The mouse this close to the top of the window shows the menus, and this far below them
/// hides them again
constexpr int MENUS_SHOW_DISTANCE = 3;
constexpr int MENUS_HIDE_DISTANCE = 20;

/// The commands with a shortcut in the menus, also in their submenus
void collectShortcutActions(const QList<QAction*>& actions, QList<QAction*>& found) {
    for (QAction* action : actions) {
        if (QMenu* menu = action->menu()) {
            collectShortcutActions(menu->actions(), found);
        } else if (!action->shortcuts().isEmpty()) {
            found.append(action);
        }
    }
}

}  // namespace

DistractionFreeLayout::DistractionFreeLayout(QMainWindow* window)
    : QObject(window)
    , m_window(window)
{
}

void DistractionFreeLayout::setActive(bool active, const QList<QWidget*>& widgets, bool fullScreen) {
    if (active == m_active || m_window == nullptr) {
        return;
    }
    if (active) {
        hideParts(widgets, fullScreen);
    } else {
        showParts();
    }
    m_active = active;
    emit activeChanged(active);
}

bool DistractionFreeLayout::eventFilter(QObject* watched, QEvent* event) {
    // Every event of the program passes here while the layout is on
    switch (event->type()) {
    case QEvent::KeyPress: {
        // A key no widget used goes up to the window
        const auto* key = static_cast<QKeyEvent*>(event);
        if (watched == m_window && m_active && !m_endingMenuKeys && key->key() == Qt::Key_Escape
            && key->modifiers() == Qt::NoModifier) {
            setActive(false);
            return true;
        }
        break;
    }
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress: {
        // The mouse over any widget of the window: the text, the menus at the top
        const auto* widget = qobject_cast<QWidget*>(watched);
        if (widget != nullptr && widget->window() == m_window) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            followMouse(widget->mapTo(m_window, mouse->position()).toPoint(),
                        mouse->buttons() != Qt::NoButton);
        }
        break;
    }
    case QEvent::Resize:
        if (watched == m_window && m_menus != nullptr && !m_menus->isHidden()) {
            placeMenus();
        }
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

void DistractionFreeLayout::followMouse(const QPoint& pos, bool buttonPressed) {
    if (m_menus == nullptr) {
        return;
    }
    if (m_menus->isHidden()) {
        // Not for a selection dragged to the top (the text scrolls under it)
        if (pos.y() < MENUS_SHOW_DISTANCE && !buttonPressed) {
            showMenus();
        }
    } else if (pos.y() >= m_menus->height() + MENUS_HIDE_DISTANCE
               && QApplication::activePopupWidget() == nullptr) {
        // Away from the menus, unless one of them is open
        if (m_menus->hasFocus()) {
            // The keys still move through the menus (Esc closed one of them): a second Esc
            // ends that and gives them back to the text, without turning the layout off
            const QScopedValueRollback<bool> endingMenuKeys(m_endingMenuKeys, true);
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QCoreApplication::sendEvent(m_menus, &escape);
        }
        m_menus->hide();
    }
}

void DistractionFreeLayout::showMenus() {
    // The menus of the window's menu bar as they are now (Diagnostics comes and goes)
    const auto* menuBar = qobject_cast<QMenuBar*>(m_window->menuWidget());
    if (menuBar == nullptr) {
        return;
    }
    const QList<QAction*> menus = menuBar->actions();
    if (m_menus->actions() != menus) {
        const QList<QAction*> shown = m_menus->actions();
        for (QAction* action : shown) {
            m_menus->removeAction(action);
        }
        m_menus->addActions(menus);
    }
    placeMenus();
    m_menus->raise();
    m_menus->show();
}

void DistractionFreeLayout::placeMenus() {
    m_menus->setGeometry(0, 0, m_window->width(), m_menus->sizeHint().height());
}

void DistractionFreeLayout::hideParts(const QList<QWidget*>& widgets, bool fullScreen) {
    m_geometry = m_window->saveGeometry();
    m_state = m_window->saveState();

    const auto hide = [](QWidget* widget) {
        if (widget != nullptr && !widget->isHidden()) {
            widget->hide();
            widget->setProperty(HIDDEN_PROPERTY, true);
        }
    };
    hide(m_window->menuWidget());
    for (QToolBar* toolbar : m_window->findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly)) {
        hide(toolbar);
    }
    for (QDockWidget* dock : m_window->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        hide(dock);
    }
    hide(m_window->findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly));
    for (QWidget* widget : widgets) {
        hide(widget);
    }

    // The shortcuts of a menu bar work only while it shows, so the window carries its
    // commands meanwhile, and its menus show over the top of the window when the mouse
    // comes to the top edge (a native menu bar, as on macOS, does both by itself)
    auto* menuBar = qobject_cast<QMenuBar*>(m_window->menuWidget());
    if (menuBar != nullptr && !menuBar->isNativeMenuBar()) {
        carryShortcuts();

        // Not in the window's layout, so the text stays in its place under them; opaque,
        // in the color of the window behind a menu bar
        m_menus = new QMenuBar(m_window);
        m_menus->setObjectName(QStringLiteral("distractionFreeMenuBar"));
        m_menus->setNativeMenuBar(false);
        m_menus->setBackgroundRole(QPalette::Window);
        m_menus->setAutoFillBackground(true);
        m_menus->hide();
    }

    // Esc that reaches the window, and the mouse over any of its widgets (a filter on the
    // window alone would miss the mouse: each widget takes its own moves)
    QCoreApplication::instance()->installEventFilter(this);

    m_fullScreen = fullScreen && !m_window->isFullScreen();
    if (m_fullScreen) {
        m_window->showFullScreen();
    }
}

void DistractionFreeLayout::carryShortcuts() {
    auto* menuBar = qobject_cast<QMenuBar*>(m_window->menuWidget());
    if (menuBar == nullptr || menuBar->isNativeMenuBar()) {
        return;
    }
    QList<QAction*> actions;
    collectShortcutActions(menuBar->actions(), actions);
    const QList<QAction*> own = m_window->actions();
    for (QAction* action : std::as_const(actions)) {
        if (!own.contains(action)) {
            m_window->addAction(action);
            action->setProperty(SHORTCUT_PROPERTY, true);
        }
    }
}

void DistractionFreeLayout::updateShortcuts() {
    if (m_active) {
        carryShortcuts();
    }
}

void DistractionFreeLayout::showParts() {
    QCoreApplication::instance()->removeEventFilter(this);
    if (m_menus != nullptr) {
        // Deleted later: the layout may be turned off from one of its menus
        m_menus->hide();
        m_menus->deleteLater();
        m_menus = nullptr;
    }
    const QList<QAction*> actions = m_window->actions();
    for (QAction* action : actions) {
        if (action->property(SHORTCUT_PROPERTY).toBool()) {
            action->setProperty(SHORTCUT_PROPERTY, QVariant());
            m_window->removeAction(action);
        }
    }

    // The window's size and state first, then the toolbars and panels in it, with the
    // places and tabs they had; the widgets it hid show again
    if (m_fullScreen) {
        m_window->showNormal();
        m_window->restoreGeometry(m_geometry);
    }
    m_window->restoreState(m_state);
    const QList<QWidget*> widgets = m_window->findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        if (widget->property(HIDDEN_PROPERTY).toBool()) {
            widget->setProperty(HIDDEN_PROPERTY, QVariant());
            widget->show();
        }
    }
}

} // namespace utils
} // namespace gui
} // namespace kalahari
