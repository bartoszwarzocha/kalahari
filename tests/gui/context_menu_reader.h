/// @file context_menu_reader.h
/// @brief Reads the context menus a widget opens, as they are about to show

#pragma once

#include <QAction>
#include <QChildEvent>
#include <QEvent>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QWidget>

namespace kalahari::test {

/// @brief An item of a menu as it was while the menu was open
struct MenuItem {
    QString text;
    bool separator = false;
    bool enabled = true;
    QList<MenuItem> items;  ///< The items of its submenu
};

/// @brief The items of a menu, and of its submenus
inline QList<MenuItem> menuItems(const QList<QAction*>& actions) {
    QList<MenuItem> items;
    for (const QAction* action : actions) {
        MenuItem item;
        item.text = action->text();
        item.separator = action->isSeparator();
        item.enabled = action->isEnabled();
        if (const QMenu* submenu = action->menu()) {
            item.items = menuItems(submenu->actions());
        }
        items.append(item);
    }
    return items;
}

/// @brief Reads each context menu a widget opens as the menu is about to show, and closes it
///
/// The menu runs as for the writer, with the texts the program and the filters of the
/// application gave its items as it was polished. It gets no window on the screen, where the
/// platform decides about a popup (on macOS and Windows the popup of a test window was
/// sometimes closed before the test could read it). A style that polishes a menu more than
/// once (Windows 11) has it read once.
class ContextMenuReader : public QObject {
public:
    /// @param owner The widget the menus belong to: the parent Qt or the program gives them
    explicit ContextMenuReader(QWidget* owner) : m_owner(owner) {
        m_owner->installEventFilter(this);
    }
    ~ContextMenuReader() override { m_owner->removeEventFilter(this); }
    ContextMenuReader(const ContextMenuReader&) = delete;
    ContextMenuReader& operator=(const ContextMenuReader&) = delete;

    /// @brief How many menus were read
    [[nodiscard]] int menusRead() const { return m_menusRead; }

    /// @brief The items of the last menu read
    [[nodiscard]] const QList<MenuItem>& items() const { return m_items; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == m_owner && event->type() == QEvent::ChildPolished) {
            if (auto* menu = qobject_cast<QMenu*>(static_cast<QChildEvent*>(event)->child())) {
                connect(menu, &QMenu::aboutToShow, this, &ContextMenuReader::read,
                        Qt::UniqueConnection);
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void read() {
        auto* menu = qobject_cast<QMenu*>(sender());
        if (menu == nullptr) {
            return;
        }
        menu->setAttribute(Qt::WA_DontShowOnScreen);
        m_items = menuItems(menu->actions());
        ++m_menusRead;
        // Closed once it is open, the menu ends its event loop
        QTimer::singleShot(0, menu, [menu]() { menu->close(); });
    }

    QWidget* m_owner;
    QList<MenuItem> m_items;
    int m_menusRead = 0;
};

}  // namespace kalahari::test
