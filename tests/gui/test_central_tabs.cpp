/// @file test_central_tabs.cpp
/// @brief The tabs in the middle of the window: Qt's one row by default; in more rows the
/// tabs of Qt's tab bar wrapped to the width, following its changes, and a click, a close
/// button and a drag acting on it; back in one row Qt's close buttons and arrows again; the
/// button in the corner that switches the rows and keeps the setting

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/widgets/central_tabs.h"
#include "kalahari/gui/widgets/panel_tab_bar.h"

#include <QAbstractButton>
#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QTabBar>
#include <QToolButton>

#include <cstdio>
#include <string>
#include <vector>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// A mouse event on @p widget at @p pos
void mouse(QWidget* widget, QEvent::Type type, const QPoint& pos, Qt::MouseButton button,
           Qt::MouseButtons buttons) {
    QMouseEvent event(type, QPointF(pos), widget->mapToGlobal(QPointF(pos)), button, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

/// A click of the left button
void click(QWidget* widget, const QPoint& pos) {
    mouse(widget, QEvent::MouseButtonPress, pos, Qt::LeftButton, Qt::LeftButton);
    mouse(widget, QEvent::MouseButtonRelease, pos, Qt::LeftButton, Qt::NoButton);
}

/// A drag with the left button from @p from to @p to
void drag(QWidget* widget, const QPoint& from, const QPoint& to) {
    mouse(widget, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    mouse(widget, QEvent::MouseMove, (from + to) / 2, Qt::NoButton, Qt::LeftButton);
    mouse(widget, QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
    mouse(widget, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
}

/// The layouts of a shown window settle
void settle() {
    for (int pass = 0; pass < 5; ++pass) {
        QApplication::processEvents();
    }
}

/// An icon of one color
QIcon iconOf(Qt::GlobalColor color) {
    QPixmap pixmap(16, 16);
    pixmap.fill(color);
    return QIcon(pixmap);
}

/// The names of the tabs, in their order
std::string tabNames(const PanelTabBar& bar) {
    QStringList names;
    for (int i = 0; i < bar.count(); ++i) {
        names.append(bar.tab(i).name);
    }
    return names.join(QStringLiteral(", ")).toStdString();
}

/// The texts of Qt's tabs, in their order
std::string tabNames(const QTabWidget& tabs) {
    QStringList names;
    for (int i = 0; i < tabs.count(); ++i) {
        names.append(tabs.tabText(i));
    }
    return names.join(QStringLiteral(", ")).toStdString();
}

/// The close buttons of Qt's tabs that show
int shownCloseButtons(const QTabBar& bar) {
    int shown = 0;
    for (int i = 0; i < bar.count(); ++i) {
        const QWidget* button = bar.tabButton(i, QTabBar::RightSide);
        if (button == nullptr) {
            button = bar.tabButton(i, QTabBar::LeftSide);
        }
        if (button != nullptr && button->isVisible()) {
            ++shown;
        }
    }
    return shown;
}

/// Tabs as in the middle of the program's window, with six chapters, shown
struct Workspace {
    CentralTabs tabs;

    Workspace() {
        tabs.setTabsClosable(true);
        tabs.setMovable(true);
        tabs.setDocumentMode(true);
        for (const char* name : {"Dashboard", "Chapter One: The Caravan", "Chapter Two: Sand",
                                 "Chapter Three: The Well", "Quotes", "Images"}) {
            const QString text = QString::fromLatin1(name);
            tabs.addTab(new QLabel(text), iconOf(Qt::darkYellow), text);
        }
        tabs.resize(420, 300);
        tabs.show();
        settle();
    }
};

} // anonymous namespace

TEST_CASE("Central tabs: one row of Qt's tabs by default", "[gui][centraltabs]") {
    registerAllIcons();  // the icon of the button of the rows
    Workspace space;
    CHECK(space.tabs.rows() == PanelTabBar::Rows::Single);
    CHECK(space.tabs.tabRows() == nullptr);
    CHECK(shownCloseButtons(*space.tabs.tabBar()) == 6);

    // The button of the rows in the right corner, up, with an icon as big as the tabs' icons
    QToolButton* button = space.tabs.rowsButton();
    REQUIRE(button != nullptr);
    CHECK(space.tabs.cornerWidget(Qt::TopRightCorner) == button);
    CHECK(button->isVisible());
    CHECK(button->isCheckable());
    CHECK_FALSE(button->isChecked());
    CHECK_FALSE(button->icon().isNull());
    CHECK_FALSE(button->toolTip().isEmpty());
    CHECK(button->iconSize() == space.tabs.tabBar()->iconSize());
    CHECK(space.tabs.tabBar()->geometry().right() < button->geometry().left());
}

TEST_CASE("Central tabs in rows: Qt's tabs wrapped to the width, the pages under them",
          "[gui][centraltabs]") {
    Workspace space;
    space.tabs.setRows(PanelTabBar::Rows::Wrap);
    settle();
    PanelTabBar* rows = space.tabs.tabRows();
    REQUIRE(rows != nullptr);
    CHECK(rows->drawing() == PanelTabBar::Drawing::Style);
    CHECK(rows->isClosable());
    CHECK(tabNames(*rows) == tabNames(space.tabs));
    CHECK(rows->currentIndex() == 0);

    // Every name whole, in more rows; the bar as high as they are, the page under it
    CHECK(rows->rowCount() > 1);
    for (int i = 0; i < rows->count(); ++i) {
        INFO("tab " << i);
        CHECK(rows->shownName(i) == space.tabs.tabText(i));
        CHECK(rows->tabRect(i).right() < space.tabs.width());
    }
    QTabBar* bar = space.tabs.tabBar();
    CHECK(bar->height() == rows->heightForWidth(bar->width()));
    CHECK(rows->geometry() == bar->rect());
    const QWidget* page = space.tabs.currentWidget();
    CHECK(page->mapTo(&space.tabs, QPoint(0, 0)).y() >= bar->geometry().bottom());

    // The button of the rows down, after the rows, by the last of them
    const QToolButton* button = space.tabs.rowsButton();
    CHECK(button->isChecked());
    CHECK(button->isVisible());
    CHECK(bar->geometry().right() < button->geometry().left());
    CHECK(button->geometry().top() >= bar->geometry().top());
    CHECK(button->geometry().bottom() <= bar->geometry().bottom());

    // Qt's close buttons and arrows do not show through
    CHECK(shownCloseButtons(*bar) == 0);
    CHECK_FALSE(bar->usesScrollButtons());

    // Wide enough for one row, the bar is one row high
    const int twoRows = bar->height();
    space.tabs.resize(2000, 300);
    settle();
    CHECK(rows->rowCount() == 1);
    CHECK(bar->height() < twoRows);
    CHECK(bar->height() == rows->heightForWidth(bar->width()));
}

TEST_CASE("Central tabs in rows: they follow Qt's tabs", "[gui][centraltabs]") {
    Workspace space;
    space.tabs.setRows(PanelTabBar::Rows::Wrap);
    settle();
    PanelTabBar* rows = space.tabs.tabRows();
    REQUIRE(rows != nullptr);

    // A chapter renamed and changed ("*"), a tab added and one closed
    space.tabs.setTabText(1, QStringLiteral("*Chapter One: The Caravan"));
    space.tabs.addTab(new QLabel(QStringLiteral("Notes")), iconOf(Qt::darkYellow),
                      QStringLiteral("Notes"));
    space.tabs.removeTab(4);
    settle();
    CHECK(tabNames(*rows) == tabNames(space.tabs));
    CHECK(rows->tab(rows->count() - 1).key == space.tabs.widget(space.tabs.count() - 1));
    CHECK(shownCloseButtons(*space.tabs.tabBar()) == 0);  // also of the tab added

    // Another icon (only painted by Qt's tab bar) and a tooltip
    const QIcon red = iconOf(Qt::red);
    space.tabs.setTabIcon(2, red);
    space.tabs.setTabToolTip(2, QStringLiteral("The second chapter"));
    settle();
    CHECK(rows->tab(2).icon.cacheKey() == red.cacheKey());
    CHECK(rows->tab(2).toolTip == QStringLiteral("The second chapter"));

    // Another tab in front, and a tab moved by the program
    space.tabs.setCurrentIndex(3);
    space.tabs.tabBar()->moveTab(3, 0);
    settle();
    CHECK(tabNames(*rows) == tabNames(space.tabs));
    CHECK(rows->currentIndex() == space.tabs.currentIndex());
    CHECK(rows->tab(0).key == space.tabs.widget(0));
}

TEST_CASE("Central tabs in rows: a click, a close button and a drag act on Qt's tabs",
          "[gui][centraltabs]") {
    Workspace space;
    space.tabs.setRows(PanelTabBar::Rows::Wrap);
    settle();
    PanelTabBar* rows = space.tabs.tabRows();
    REQUIRE(rows != nullptr);
    std::vector<int> clicked;
    std::vector<int> closing;
    QObject::connect(&space.tabs, &QTabWidget::tabBarClicked, &space.tabs,
                     [&clicked](int index) { clicked.push_back(index); });
    QObject::connect(&space.tabs, &QTabWidget::tabCloseRequested, &space.tabs,
                     [&closing](int index) { closing.push_back(index); });

    SECTION("a click brings a tab to the front, as Qt's") {
        click(rows, rows->tabRect(3).center());
        CHECK(space.tabs.currentIndex() == 3);
        CHECK(space.tabs.currentWidget() == space.tabs.widget(3));
        CHECK(rows->currentIndex() == 3);
        CHECK(clicked == std::vector<int>{3});
        CHECK(closing.empty());
    }

    SECTION("its close button asks to close it, without bringing it to the front") {
        const QRect close = rows->closeRect(2);
        REQUIRE(close.isValid());
        CHECK(rows->tabRect(2).contains(close));
        click(rows, close.center());
        CHECK(closing == std::vector<int>{2});
        CHECK(space.tabs.currentIndex() == 0);
        CHECK(clicked.empty());
    }

    SECTION("a drag moves a tab with its page") {
        QWidget* moved = space.tabs.widget(0);
        const QRect last = rows->tabRect(2);
        drag(rows, rows->tabRect(0).center(), QPoint(last.right() - 2, last.center().y()));
        settle();
        CHECK(space.tabs.indexOf(moved) == 2);
        CHECK(space.tabs.tabText(2) == QStringLiteral("Dashboard"));
        CHECK(tabNames(*rows) == tabNames(space.tabs));
        CHECK(closing.empty());
    }
}

TEST_CASE("Central tabs: back in one row, Qt's close buttons and arrows again, and the setting",
          "[gui][centraltabs]") {
    Workspace space;
    const bool arrows = space.tabs.tabBar()->usesScrollButtons();
    space.tabs.setRows(PanelTabBar::Rows::Wrap);
    settle();
    space.tabs.setRows(PanelTabBar::Rows::Single);
    settle();
    CHECK(space.tabs.tabRows() == nullptr);
    CHECK(shownCloseButtons(*space.tabs.tabBar()) == 6);
    CHECK(space.tabs.tabBar()->usesScrollButtons() == arrows);
    CHECK(space.tabs.tabBar()->focusPolicy() == Qt::TabFocus);

    // The setting, as the Settings dialog sets it
    auto& settings = core::SettingsManager::getInstance();
    const std::string saved = settings.get<std::string>(CentralTabs::ROWS_KEY, std::string());
    CHECK(CentralTabs::rowsSetting() == PanelTabBar::Rows::Single);  // the default
    settings.set<std::string>(CentralTabs::ROWS_KEY, CentralTabs::ROWS_WRAP);
    space.tabs.applySettings();
    CHECK(space.tabs.tabRows() != nullptr);
    settings.set<std::string>(CentralTabs::ROWS_KEY, CentralTabs::ROWS_SINGLE);
    space.tabs.applySettings();
    CHECK(space.tabs.tabRows() == nullptr);
    settings.set<std::string>(CentralTabs::ROWS_KEY, saved);
}

TEST_CASE("Central tabs: the button switches the rows at once and keeps the setting",
          "[gui][centraltabs]") {
    Workspace space;
    auto& settings = core::SettingsManager::getInstance();
    const std::string saved = settings.get<std::string>(CentralTabs::ROWS_KEY, std::string());
    settings.set<std::string>(CentralTabs::ROWS_KEY, CentralTabs::ROWS_SINGLE);
    QToolButton* button = space.tabs.rowsButton();
    REQUIRE(button != nullptr);

    // Clicked: more rows, and the setting says so
    button->click();
    settle();
    CHECK(space.tabs.rows() == PanelTabBar::Rows::Wrap);
    CHECK(space.tabs.tabRows() != nullptr);
    CHECK(button->isChecked());
    CHECK(settings.get<std::string>(CentralTabs::ROWS_KEY, std::string()) ==
          CentralTabs::ROWS_WRAP);
    CHECK(CentralTabs::rowsSetting() == PanelTabBar::Rows::Wrap);

    // Clicked again: one row with Qt's close buttons, and the setting
    button->click();
    settle();
    CHECK(space.tabs.rows() == PanelTabBar::Rows::Single);
    CHECK_FALSE(button->isChecked());
    CHECK(shownCloseButtons(*space.tabs.tabBar()) == 6);
    CHECK(settings.get<std::string>(CentralTabs::ROWS_KEY, std::string()) ==
          CentralTabs::ROWS_SINGLE);

    // The rows set by the program or by the Settings dialog move the button, not the setting
    space.tabs.setRows(PanelTabBar::Rows::Wrap);
    CHECK(button->isChecked());
    CHECK(settings.get<std::string>(CentralTabs::ROWS_KEY, std::string()) ==
          CentralTabs::ROWS_SINGLE);
    settings.set<std::string>(CentralTabs::ROWS_KEY, CentralTabs::ROWS_SINGLE);
    space.tabs.applySettings();
    CHECK_FALSE(button->isChecked());

    // Hidden with the tabs (Distraction-Free), it leaves the page the whole width
    space.tabs.tabBar()->hide();
    button->hide();
    settle();
    CHECK(space.tabs.currentWidget()->width() == space.tabs.width());
    space.tabs.tabBar()->show();
    button->show();
    settle();
    CHECK(space.tabs.tabBar()->geometry().right() < button->geometry().left());

    settings.set<std::string>(CentralTabs::ROWS_KEY, saved);
}
