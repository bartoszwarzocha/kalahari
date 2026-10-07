/// @file distraction_free_layout.cpp
/// @brief Implementation of the main window's layout for Distraction-Free writing

#include "kalahari/gui/utils/distraction_free_layout.h"

#include <QAction>
#include <QDockWidget>
#include <QEvent>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
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
    // A key no widget used goes up to the window
    if (watched == m_window && m_active && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape && key->modifiers() == Qt::NoModifier) {
            setActive(false);
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
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
    // commands meanwhile (a native menu bar, as on macOS, keeps working by itself)
    auto* menuBar = qobject_cast<QMenuBar*>(m_window->menuWidget());
    if (menuBar != nullptr && !menuBar->isNativeMenuBar()) {
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

    m_window->installEventFilter(this);

    m_fullScreen = fullScreen && !m_window->isFullScreen();
    if (m_fullScreen) {
        m_window->showFullScreen();
    }
}

void DistractionFreeLayout::showParts() {
    m_window->removeEventFilter(this);
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
