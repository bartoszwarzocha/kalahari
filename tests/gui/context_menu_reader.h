/// @file context_menu_reader.h
/// @brief Reads the context menus a widget opens, as they are about to show

#pragma once

#include <QAction>
#include <QChar>
#include <QChildEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <utility>

namespace kalahari::test {

/// @brief An item of a menu as it was while the menu was open
struct MenuItem {
    QString text;
    bool separator = false;
    bool enabled = true;
    QString toolTip;        ///< What it says under the mouse (its text when it says nothing more)
    QList<MenuItem> items;  ///< The items of its submenu
    /// Its action, only to compare: one the menu made for itself is gone with the menu
    const QAction* action = nullptr;
};

/// @brief The items of a menu, and of its submenus
inline QList<MenuItem> menuItems(const QList<QAction*>& actions) {
    QList<MenuItem> items;
    for (const QAction* action : actions) {
        MenuItem item;
        item.text = action->text();
        item.separator = action->isSeparator();
        item.enabled = action->isEnabled();
        item.toolTip = action->toolTip();
        item.action = action;
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

    /// @brief The next menu read: choose its item with this access key, as the writer does,
    ///        instead of closing the menu (a key no item has, or a disabled item's, chooses
    ///        nothing, and the menu closes)
    void chooseWith(QChar key) { m_key = key; }

    /// @brief The next menu read: choose its item whose text begins with @p text, as with the
    ///        mouse, instead of closing the menu (macOS has no access keys); a disabled item
    ///        chooses nothing
    void chooseItem(const QString& text) { m_item = text; }

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
        // Closed once it is open, the menu ends its event loop; an item chosen closes it
        QTimer::singleShot(0, menu, [menu, key = std::exchange(m_key, QChar()),
                                     text = std::exchange(m_item, QString())]() {
            if (!key.isNull()) {
                QKeyEvent press(QEvent::KeyPress, key.toUpper().unicode(), Qt::NoModifier,
                                QString(key));
                QCoreApplication::sendEvent(menu, &press);
            }
            // As a click: the menu closes, then its item does what it does
            const QList<QAction*> actions = text.isEmpty() ? QList<QAction*>() : menu->actions();
            for (QAction* action : actions) {
                if (action->isEnabled() && action->text().startsWith(text)) {
                    menu->close();
                    action->trigger();
                    break;
                }
            }
            if (menu->isVisible()) {
                menu->close();
            }
        });
    }

    QWidget* m_owner;
    QList<MenuItem> m_items;
    int m_menusRead = 0;
    QChar m_key;  ///< The access key that chooses an item of the next menu (none: closes it)
    QString m_item;  ///< The beginning of the text of the item to choose in the next menu
};

}  // namespace kalahari::test
