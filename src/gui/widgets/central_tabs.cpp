// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file central_tabs.cpp
/// @brief The tabs in the middle of the window: Qt's one row, or as many rows as the width needs

#include "kalahari/gui/widgets/central_tabs.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"

#include <QAbstractButton>
#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QPointer>
#include <QTabBar>
#include <QToolButton>

#include <string>

namespace kalahari {
namespace gui {

namespace {

/// @brief Alt+Down: the list of all the tabs, as it opens a list of a field
bool isListKey(const QKeyEvent* event) {
    return event->key() == Qt::Key_Down &&
           (event->modifiers() & ~Qt::KeypadModifier) == Qt::AltModifier;
}

}  // namespace

/// @brief Qt's tab bar of CentralTabs, with the tabs in more rows over it
///
/// In more rows it paints nothing and asks for the height of the rows at the width of the tab
/// widget, which it spans (document mode). The rows show its tabs as they are: they are set
/// again whenever its tabs are laid out (a tab added, closed, moved or renamed), it paints (an
/// icon changed) or a tooltip is about to show.
class CentralTabBar : public QTabBar {
public:
    explicit CentralTabBar(QTabWidget* tabs)
        : QTabBar(tabs), m_tabs(tabs)
    {
        connect(this, &QTabBar::currentChanged, this, [this](int /*index*/) { sync(); });
        // After the tab widget has moved its page (it heard of the move first)
        connect(this, &QTabBar::tabMoved, this, [this](int /*from*/, int /*to*/) { sync(); });
    }

    void setRows(PanelTabBar::Rows rows) {
        const bool wrap = rows == PanelTabBar::Rows::Wrap;
        if (wrap == (m_rows != nullptr)) {
            return;
        }
        if (wrap) {
            m_rows = new PanelTabBar(this);
            m_rows->setObjectName(QStringLiteral("centralTabRows"));
            m_rows->setAccessibleName(CentralTabs::tr("Tabs"));
            m_rows->setDrawing(PanelTabBar::Drawing::Style);
            m_rows->setLook(PanelTabBar::Look::IconsAndNames);
            m_rows->setRows(PanelTabBar::Rows::Wrap);
            m_rows->setClosable(tabsClosable());
            m_rows->installEventFilter(this);
            connect(m_rows, &PanelTabBar::tabChosen, this, &QTabBar::setCurrentIndex);
            connect(m_rows, &PanelTabBar::tabClicked, this, &QTabBar::tabBarClicked);
            connect(m_rows, &PanelTabBar::tabMoveRequested, this, &QTabBar::moveTab);
            connect(m_rows, &PanelTabBar::tabCloseRequested, this, &QTabBar::tabCloseRequested);

            // Qt's arrows and close buttons would show through the rows
            m_scrollButtons = usesScrollButtons();
            setUsesScrollButtons(false);
            showCloseButtons(false);

            // One place in the order of the Tab key, the rows'
            const bool focused = hasFocus();
            m_focusPolicy = focusPolicy();
            setFocusPolicy(Qt::NoFocus);
            setFocusProxy(m_rows);
            m_rows->setGeometry(rect());
            m_rows->show();
            m_rows->raise();
            sync();
            if (focused) {
                m_rows->setFocus(Qt::OtherFocusReason);
            }
        } else {
            const bool focused = m_rows->hasFocus();
            setFocusProxy(nullptr);
            setFocusPolicy(m_focusPolicy);
            m_rows->hide();
            m_rows->deleteLater();
            m_rows = nullptr;
            setUsesScrollButtons(m_scrollButtons);
            showCloseButtons(true);
            if (focused) {
                setFocus(Qt::OtherFocusReason);
            }
        }
        updateGeometry();
        update();
    }

    [[nodiscard]] PanelTabBar* tabRows() const { return m_rows; }

    [[nodiscard]] QSize sizeHint() const override {
        if (m_rows == nullptr) {
            return QTabBar::sizeHint();
        }
        // The bar spans the tab widget but its corners, as QTabWidget lays it out (the style's
        // SE_TabWidgetTabBar): the rows that width needs
        int width = m_tabs->width();
        for (const Qt::Corner corner : {Qt::TopLeftCorner, Qt::TopRightCorner}) {
            if (const QWidget* widget = m_tabs->cornerWidget(corner); widget != nullptr) {
                width -= widget->sizeHint().width();
            }
        }
        return {m_tabs->width(), m_rows->heightForWidth(width)};
    }

    [[nodiscard]] QSize minimumSizeHint() const override {
        if (m_rows == nullptr) {
            return QTabBar::minimumSizeHint();
        }
        return {m_rows->minimumSizeHint().width(), sizeHint().height()};
    }

protected:
    void tabInserted(int index) override {
        QTabBar::tabInserted(index);
        if (m_rows != nullptr) {
            showCloseButtons(false);
        }
        sync();
    }

    void tabRemoved(int index) override {
        QTabBar::tabRemoved(index);
        sync();
    }

    void tabLayoutChange() override {
        QTabBar::tabLayoutChange();
        sync();
    }

    void paintEvent(QPaintEvent* event) override {
        if (m_rows == nullptr) {
            QTabBar::paintEvent(event);
            return;
        }
        // Painting asked for by a tab of its own: an icon or a color changed
        sync();
    }

    void resizeEvent(QResizeEvent* event) override {
        QTabBar::resizeEvent(event);
        if (m_rows != nullptr) {
            m_rows->setGeometry(rect());
        }
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == m_rows && event->type() == QEvent::ToolTip) {
            sync();  // a tooltip set since
        }
        return QTabBar::eventFilter(watched, event);
    }

private:
    /// @brief Show or hide the close buttons of Qt's tabs
    void showCloseButtons(bool shown) {
        for (int i = 0; i < count(); ++i) {
            for (const ButtonPosition side : {LeftSide, RightSide}) {
                if (QWidget* button = tabButton(i, side);
                    qobject_cast<QAbstractButton*>(button) != nullptr) {
                    button->setVisible(shown);
                }
            }
        }
    }

    /// @brief Give the rows the tabs as they are now
    void sync() {
        if (m_rows == nullptr) {
            return;
        }
        QList<PanelTabBar::Tab> tabs;
        tabs.reserve(count());
        for (int i = 0; i < count(); ++i) {
            tabs.append({m_tabs->widget(i), tabIcon(i), tabText(i), tabToolTip(i)});
        }
        m_rows->setIconSize(iconSize().width());
        m_rows->setClosable(tabsClosable());

        // Tabs set again forget the one under the mouse: only when they changed
        bool same = tabs.size() == m_rows->count();
        for (int i = 0; same && i < tabs.size(); ++i) {
            const PanelTabBar::Tab& shown = m_rows->tab(i);
            same = shown.key == tabs.at(i).key && shown.name == tabs.at(i).name &&
                   shown.toolTip == tabs.at(i).toolTip &&
                   shown.icon.cacheKey() == tabs.at(i).icon.cacheKey();
        }
        if (same) {
            m_rows->setCurrentIndex(currentIndex());
            return;
        }
        m_rows->setTabs(tabs, currentIndex());
        updateGeometry();
    }

    QTabWidget* m_tabs;
    PanelTabBar* m_rows = nullptr;  ///< The tabs in more rows, nullptr in one row
    bool m_scrollButtons = true;
    Qt::FocusPolicy m_focusPolicy = Qt::TabFocus;
};

CentralTabs::CentralTabs(QWidget* parent)
    : QTabWidget(parent),
      m_bar(new CentralTabBar(this)),
      m_rowsAction(core::ArtProvider::getInstance().createAction(
          QStringLiteral("tabs.rows"), tr("Tabs in More Rows"), this,
          core::IconContext::TabBar)),
      m_rowsButton(new QToolButton(this)),
      m_listButton(new QToolButton(this))
{
    setObjectName(QStringLiteral("centralTabs"));
    setTabBar(m_bar);

    // The buttons after the tabs, with icons as big as theirs: the list of all the tabs, then
    // the rows (user's test of 16.11: the list, as in Total Commander)
    auto* corner = new QWidget(this);
    corner->setObjectName(QStringLiteral("centralTabButtons"));
    auto* buttons = new QHBoxLayout(corner);
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(0);
    //: The button after the tabs that lists them all. In Polish: Wszystkie karty
    QAction* listAction = core::ArtProvider::getInstance().createAction(
        QStringLiteral("tabs.list"), tr("All Tabs"), this, core::IconContext::TabBar);
    //: %1: the keys (Alt+Down). In Polish: Wszystkie karty – lista do wyboru (%1)
    listAction->setToolTip(tr("All Tabs - a list to choose from (%1)")
                               .arg(QKeySequence(Qt::ALT | Qt::Key_Down)
                                        .toString(QKeySequence::NativeText)));
    connect(listAction, &QAction::triggered, this, [this]() { showTabList(); });
    m_listButton->setObjectName(QStringLiteral("centralTabListButton"));
    m_listButton->setDefaultAction(listAction);
    m_listButton->setAutoRaise(true);
    m_listButton->setFocusPolicy(Qt::NoFocus);  // Alt+Down on the tabs
    m_rowsAction->setCheckable(true);
    m_rowsButton->setObjectName(QStringLiteral("centralTabRowsButton"));
    m_rowsButton->setDefaultAction(m_rowsAction);
    m_rowsButton->setAutoRaise(true);
    for (QToolButton* button : {m_listButton, m_rowsButton}) {
        button->setParent(corner);
        button->setIconSize(tabBar()->iconSize());
        buttons->addWidget(button);
        connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, button,
                [this, button]() { button->setIconSize(tabBar()->iconSize()); });
    }
    setCornerWidget(corner, Qt::TopRightCorner);

    // Clicked (not set by the program): the rows at once, and the setting
    connect(m_rowsAction, &QAction::triggered, this, [this](bool wrap) {
        setRows(wrap ? PanelTabBar::Rows::Wrap : PanelTabBar::Rows::Single);
        core::SettingsManager::getInstance().set<std::string>(ROWS_KEY,
                                                              wrap ? ROWS_WRAP : ROWS_SINGLE);
    });
}

void CentralTabs::setRows(PanelTabBar::Rows rows) {
    m_bar->setRows(rows);
    m_rowsAction->setChecked(rows == PanelTabBar::Rows::Wrap);
}

PanelTabBar::Rows CentralTabs::rows() const {
    return m_bar->tabRows() != nullptr ? PanelTabBar::Rows::Wrap : PanelTabBar::Rows::Single;
}

PanelTabBar* CentralTabs::tabRows() const {
    return m_bar->tabRows();
}

QToolButton* CentralTabs::rowsButton() const {
    return m_rowsButton;
}

QToolButton* CentralTabs::listButton() const {
    return m_listButton;
}

void CentralTabs::showTabList() {
    // Under the button, its right edge at the button's; without the button (the window's
    // bars hidden), at the right end of the tabs
    const QWidget* under = m_listButton->isVisible() ? static_cast<QWidget*>(m_listButton)
                                                     : static_cast<QWidget*>(tabBar());
    showTabList(*this, under->mapToGlobal(under->rect().bottomRight()));
}

void CentralTabs::showTabList(QTabWidget& tabs, const QPoint& globalPos) {
    if (tabs.count() == 0) {
        return;
    }
    QMenu menu(&tabs);
    menu.setObjectName(QStringLiteral("tabList"));
    menu.setToolTipsVisible(true);
    QAction* current = nullptr;
    for (int i = 0; i < tabs.count(); ++i) {
        // The first nine with their numbers as access keys, as the windows of the Window menu
        QString title = tabs.tabText(i);
        title.replace(QLatin1Char('&'), QStringLiteral("&&"));
        const QString text = i < 9 ? QStringLiteral("&%1 %2").arg(i + 1).arg(title)
                                   : QStringLiteral("%1 %2").arg(i + 1).arg(title);
        QAction* action = menu.addAction(tabs.tabIcon(i), text);
        action->setCheckable(true);
        action->setChecked(i == tabs.currentIndex());
        action->setToolTip(tabs.tabToolTip(i));
        action->setData(i);
        if (i == tabs.currentIndex()) {
            current = action;
        }
    }
    menu.setActiveAction(current);

    // Its right edge at the point, within the screen
    const QSize size = menu.sizeHint();
    const QPoint at(globalPos.x() - size.width() + 1, globalPos.y() + 1);
    QPointer<QWidget> page;
    connect(&menu, &QMenu::triggered, &menu, [&page, &tabs](const QAction* chosen) {
        page = tabs.widget(chosen->data().toInt());
    });
    menu.exec(at);
    if (page != nullptr && tabs.indexOf(page) >= 0) {
        tabs.setCurrentWidget(page);
        page->setFocus(Qt::OtherFocusReason);
    }
}

bool CentralTabs::tabsHaveKeys() const {
    // The widget of its window with the keys: the same as the program's while the window is
    // active (the keys come only then), and known also before
    const QWidget* keys = window()->focusWidget();
    return keys != nullptr && (keys == tabBar() || tabBar()->isAncestorOf(keys));
}

bool CentralTabs::event(QEvent* event) {
    // Alt+Down on the tabs is theirs, not a shortcut of the window (in a page it is the
    // page's or the window's: the next to-do)
    if (event->type() == QEvent::ShortcutOverride && tabsHaveKeys() &&
        isListKey(static_cast<QKeyEvent*>(event))) {
        event->accept();
        return true;
    }
    return QTabWidget::event(event);
}

void CentralTabs::keyPressEvent(QKeyEvent* event) {
    if (tabsHaveKeys() && isListKey(event)) {
        showTabList();
        event->accept();
        return;
    }
    QTabWidget::keyPressEvent(event);
}

void CentralTabs::applySettings() {
    setRows(rowsSetting());
}

PanelTabBar::Rows CentralTabs::rowsSetting() {
    return core::SettingsManager::getInstance().get<std::string>(ROWS_KEY, ROWS_SINGLE) ==
                   ROWS_WRAP
               ? PanelTabBar::Rows::Wrap
               : PanelTabBar::Rows::Single;
}

} // namespace gui
} // namespace kalahari
