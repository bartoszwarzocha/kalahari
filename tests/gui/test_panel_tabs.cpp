/// @file test_panel_tabs.cpp
/// @brief The tabs of the panels docked one over another: icons alone by default with the
/// names in the tooltips, names that wrap into more rows or shorten in one row, a click or
/// the keys choose a tab and a drag moves it; in a window the program's tabs follow Qt's
/// groups, Qt's take no room, and the settings put the tabs above or under the panels

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/panel_tabs.h"
#include "kalahari/gui/widgets/panel_tab_bar.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDockWidget>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QTabBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdio>
#include <deque>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// Tabs with these names, each standing for itself
QList<PanelTabBar::Tab> tabsNamed(const QStringList& names) {
    static std::deque<QString> keys;  // in place, for the whole test run
    QList<PanelTabBar::Tab> tabs;
    for (const QString& name : names) {
        auto key = std::find(keys.cbegin(), keys.cend(), name);
        if (key == keys.cend()) {
            keys.push_back(name);
            key = std::prev(keys.cend());
        }
        tabs.append({&*key, QIcon(), name, name});
    }
    return tabs;
}

/// The names of the tabs, in their order
std::string tabNames(const PanelTabBar& bar) {
    QStringList names;
    for (int i = 0; i < bar.count(); ++i) {
        names.append(bar.tab(i).name);
    }
    return names.join(QStringLiteral(", ")).toStdString();
}

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

/// A key pressed on @p widget
void press(QWidget* widget, int key) {
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

/// The tabs' choices and moves, as they ask for them
struct Requests {
    std::vector<int> chosen;
    std::vector<std::pair<int, int>> moved;

    explicit Requests(PanelTabBar& bar) {
        QObject::connect(&bar, &PanelTabBar::tabChosen, &bar,
                         [this](int index) { chosen.push_back(index); });
        QObject::connect(&bar, &PanelTabBar::tabMoveRequested, &bar,
                         [this](int from, int to) { moved.emplace_back(from, to); });
    }
};

/// The settings of the tabs as a test sets them; as they were again when it ends
class TabSettings {
public:
    TabSettings() {
        auto& settings = core::SettingsManager::getInstance();
        for (const char* key : {PanelTabs::LOOK_KEY, PanelTabs::ROWS_KEY, PanelTabs::SIDE_KEY}) {
            m_saved.emplace_back(key, settings.get<std::string>(key, std::string()));
        }
        set(PanelTabs::LOOK_ICONS, PanelTabs::ROWS_WRAP, PanelTabs::SIDE_ABOVE);
    }
    ~TabSettings() {
        try {
            for (const auto& [key, value] : m_saved) {
                core::SettingsManager::getInstance().set<std::string>(key, value);
            }
        } catch (...) {
            std::fputs("The settings of the panel tabs could not be given back\n", stderr);
        }
    }
    TabSettings(const TabSettings&) = delete;
    TabSettings& operator=(const TabSettings&) = delete;

    /// Set what the tabs show, their rows and their side, as the Settings dialog does
    static void set(const char* look, const char* rows, const char* side) {
        auto& settings = core::SettingsManager::getInstance();
        settings.set<std::string>(PanelTabs::LOOK_KEY, look);
        settings.set<std::string>(PanelTabs::ROWS_KEY, rows);
        settings.set<std::string>(PanelTabs::SIDE_KEY, side);
    }

private:
    std::vector<std::pair<const char*, std::string>> m_saved;
};

/// The layouts of a shown window settle, and the tabs follow them
void settle() {
    for (int pass = 0; pass < 5; ++pass) {
        QApplication::processEvents();
    }
}

/// A panel with a title bar like the program's: its row in a vertical layout
QDockWidget* newPanel(const QString& name, QMainWindow& window) {
    auto* dock = new QDockWidget(name, &window);
    dock->setObjectName(name);
    auto* titleBar = new QWidget(dock);
    auto* layout = new QVBoxLayout(titleBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(new QLabel(name, titleBar));
    dock->setTitleBarWidget(titleBar);
    dock->setWidget(new QLabel(name.left(1)));  // a panel as narrow as its tabs let it
    return dock;
}

/// Whether the title bar of @p dock is as high as its tabs and its title, and its tabs as
/// high as their rows
bool titleBarFits(const QDockWidget* dock, const PanelTabBar* tabs) {
    const auto* title = dock->titleBarWidget()->findChild<QLabel*>();
    REQUIRE(title != nullptr);
    CAPTURE(tabs->width(), tabs->height(), tabs->rowCount(), dock->titleBarWidget()->height(),
            title->sizeHint().height());
    return tabs->height() == tabs->heightForWidth(tabs->width()) &&
           dock->titleBarWidget()->height() == tabs->height() + title->sizeHint().height();
}

/// Qt's tabs of the group of @p dock (nullptr without a group)
QTabBar* qtTabsOf(const QMainWindow& window, const QDockWidget* dock) {
    for (QTabBar* bar : window.findChildren<QTabBar*>(Qt::FindDirectChildrenOnly)) {
        for (int i = 0; i < bar->count(); ++i) {
            if (bar->tabText(i) == dock->windowTitle()) {
                return bar;
            }
        }
    }
    return nullptr;
}

/// The names of Qt's tabs, in their order
std::string qtTabNames(const QTabBar& bar) {
    QStringList names;
    for (int i = 0; i < bar.count(); ++i) {
        names.append(bar.tabText(i));
    }
    return names.join(QStringLiteral(", ")).toStdString();
}

/// Three panels docked one over another on the right of a window, with the program's tabs
struct TabbedPanels {
    TabSettings settings;
    QMainWindow window;
    PanelTabs* tabs = nullptr;  // the window's, as in DockCoordinator
    QList<QDockWidget*> docks;

    TabbedPanels() {
        window.setCentralWidget(new QLabel(QStringLiteral("text")));
        for (const char* name : {"Alpha", "Beta", "Gamma"}) {
            QDockWidget* dock = newPanel(QString::fromLatin1(name), window);
            window.addDockWidget(Qt::RightDockWidgetArea, dock);
            docks.append(dock);
        }
        window.tabifyDockWidget(docks.at(0), docks.at(1));
        window.tabifyDockWidget(docks.at(1), docks.at(2));
        tabs = new PanelTabs(&window, &window);
        for (QDockWidget* dock : std::as_const(docks)) {
            tabs->addPanel(dock, "test.panelTabs." + dock->objectName().toStdString());
        }
        window.resize(900, 600);
        window.show();
        settle();
    }

    /// The panel in front of the group, as Qt's tabs say
    [[nodiscard]] QDockWidget* front() const {
        const QTabBar* bar = qtTabsOf(window, docks.at(0));
        REQUIRE(bar != nullptr);
        const QString name = bar->tabText(bar->currentIndex());
        for (QDockWidget* dock : docks) {
            if (dock->windowTitle() == name) {
                return dock;
            }
        }
        return nullptr;
    }
};

}  // namespace

TEST_CASE("Panel tabs: icons alone by default, each panel's name in its tooltip",
          "[gui][panels][paneltabs]") {
    PanelTabBar bar;
    bar.show();  // a resize reaches the tabs at once
    bar.setTabs(tabsNamed({QStringLiteral("Navigator"), QStringLiteral("Properties"),
                           QStringLiteral("Annotations")}),
                0);
    bar.resize(600, bar.heightForWidth(600));

    CHECK(bar.look() == PanelTabBar::Look::Icons);
    CHECK(bar.rows() == PanelTabBar::Rows::Wrap);
    CHECK(bar.side() == PanelTabBar::Side::Above);
    CHECK(bar.rowCount() == 1);
    for (int i = 0; i < bar.count(); ++i) {
        CHECK(bar.shownName(i).isEmpty());
        CHECK(bar.toolTipOf(i) == bar.tab(i).name);
        // Icons alone keep their size
        CHECK(bar.tabRect(i).width() == bar.tabRect(0).width());
    }

    SECTION("names seen whole need no tooltip, unless it says the panel's keys") {
        QList<PanelTabBar::Tab> tabs = tabsNamed({QStringLiteral("Navigator"),
                                                  QStringLiteral("Properties")});
        tabs[0].toolTip = QStringLiteral("Navigator (F2)");
        bar.setTabs(tabs, 0);
        bar.setLook(PanelTabBar::Look::IconsAndNames);
        bar.resize(600, bar.heightForWidth(600));
        CHECK(bar.shownName(0) == QStringLiteral("Navigator"));
        CHECK(bar.toolTipOf(0) == QStringLiteral("Navigator (F2)"));
        CHECK(bar.shownName(1) == QStringLiteral("Properties"));
        CHECK(bar.toolTipOf(1).isEmpty());
        // Names fill the row
        CHECK(bar.tabRect(1).right() >= bar.width() - 10);
    }
}

TEST_CASE("Panel tabs: tabs that do not fit wrap into more rows, or keep one row with "
          "shortened names",
          "[gui][panels][paneltabs]") {
    PanelTabBar bar;
    bar.show();  // a resize reaches the tabs at once
    bar.setLook(PanelTabBar::Look::IconsAndNames);
    bar.setTabs(tabsNamed({QStringLiteral("Navigator"), QStringLiteral("Properties"),
                           QStringLiteral("Paragraph and Styles"), QStringLiteral("Annotations"),
                           QStringLiteral("Spelling and Grammar")}),
                2);
    const int narrow = 300;

    SECTION("more rows, every name whole") {
        bar.resize(narrow, bar.heightForWidth(narrow));
        CAPTURE(bar.rowCount());
        CHECK(bar.rowCount() > 1);
        CHECK(bar.heightForWidth(narrow) > bar.heightForWidth(2000));
        for (int i = 0; i < bar.count(); ++i) {
            INFO(bar.tab(i).name.toStdString());
            CHECK(bar.shownName(i) == bar.tab(i).name);
            CHECK(bar.tabRect(i).left() >= 0);
            CHECK(bar.tabRect(i).right() < narrow);
        }
        // The rows follow the width: wide, they are one again
        bar.resize(2000, bar.heightForWidth(2000));
        CHECK(bar.rowCount() == 1);
    }

    SECTION("one row, the names shortened") {
        bar.setRows(PanelTabBar::Rows::Single);
        CHECK(bar.minimumSizeHint().width() <= narrow);
        bar.resize(narrow, bar.heightForWidth(narrow));
        CHECK(bar.rowCount() == 1);
        CHECK(bar.heightForWidth(narrow) == bar.heightForWidth(2000));
        bool shortened = false;
        for (int i = 0; i < bar.count(); ++i) {
            INFO(bar.tab(i).name.toStdString() << " shown as " << bar.shownName(i).toStdString());
            CHECK(bar.tabRect(i).right() < narrow);
            CHECK_FALSE(bar.shownName(i).isEmpty());
            if (bar.shownName(i) != bar.tab(i).name) {
                CHECK(bar.shownName(i).endsWith(QChar(0x2026)));
                // The whole name in the tooltip
                CHECK(bar.toolTipOf(i) == bar.tab(i).name);
                shortened = true;
            }
        }
        CHECK(shortened);
    }

    SECTION("one row too narrow for the names: the icons alone") {
        // Regression: every tab kept two letters of its name, so one row of icons and names
        // made the panels on the right wider than a small screen lets them be
        bar.setRows(PanelTabBar::Rows::Single);
        const int tab = bar.tabRect(0).height();  // about an icon with room around it
        const int least = bar.minimumSizeHint().width();
        CAPTURE(least, tab);
        CHECK(least <= 2 * 4 + 5 * tab + 4 * 2);
        bar.resize(least, bar.heightForWidth(least));
        CHECK(bar.rowCount() == 1);
        for (int i = 0; i < bar.count(); ++i) {
            INFO(bar.tab(i).name.toStdString());
            CHECK(bar.shownName(i).isEmpty());
            CHECK(bar.toolTipOf(i) == bar.tab(i).name);
            CHECK(bar.tabRect(i).right() < least);
        }
    }

    SECTION("icons alone wrap too") {
        bar.setLook(PanelTabBar::Look::Icons);
        const int iconsInRow = 3;
        const int width = 2 * 4 + iconsInRow * bar.tabRect(0).width() + 2 * (iconsInRow - 1);
        bar.resize(width, bar.heightForWidth(width));
        CAPTURE(width, bar.tabRect(0).width());
        CHECK(bar.rowCount() == 2);
    }
}

TEST_CASE("Panel tabs: a click or the keys choose a tab, a drag moves one",
          "[gui][panels][paneltabs]") {
    PanelTabBar bar;
    bar.show();  // a resize reaches the tabs at once
    bar.setLook(PanelTabBar::Look::Names);
    bar.setTabs(tabsNamed({QStringLiteral("Navigator"), QStringLiteral("Properties"),
                           QStringLiteral("Annotations")}),
                0);
    bar.resize(600, bar.heightForWidth(600));
    Requests requests(bar);

    SECTION("a click") {
        click(&bar, bar.tabRect(2).center());
        CHECK(requests.chosen == std::vector<int>{2});
        // The tab in front is chosen already
        click(&bar, bar.tabRect(0).center());
        CHECK(requests.chosen == std::vector<int>{2});
        CHECK(requests.moved.empty());
    }

    SECTION("the keys") {
        bar.setTabs(tabsNamed({QStringLiteral("Navigator"), QStringLiteral("Properties"),
                               QStringLiteral("Annotations")}),
                    1);
        press(&bar, Qt::Key_Right);
        press(&bar, Qt::Key_Left);
        press(&bar, Qt::Key_Home);
        press(&bar, Qt::Key_End);
        CHECK(requests.chosen == std::vector<int>{2, 0, 0, 2});
    }

    SECTION("a drag") {
        // The first tab after the last
        drag(&bar, bar.tabRect(0).center(), bar.tabRect(2).center() + QPoint(5, 0));
        // The last tab before the first
        drag(&bar, bar.tabRect(2).center(), bar.tabRect(0).topLeft() + QPoint(2, 2));
        // Next to itself it stays where it is
        drag(&bar, bar.tabRect(1).center(), bar.tabRect(1).center() + QPoint(10, 0));
        CHECK(requests.moved == std::vector<std::pair<int, int>>{{0, 2}, {2, 0}});
        CHECK(requests.chosen.empty());
    }
}

TEST_CASE("Panel tabs: panels docked one over another show the program's tabs in Qt's order, "
          "and Qt's tabs take no room",
          "[gui][panels][paneltabs]") {
    TabbedPanels panels;
    QTabBar* qtTabs = qtTabsOf(panels.window, panels.docks.at(0));
    REQUIRE(qtTabs != nullptr);
    REQUIRE(qtTabs->count() == 3);
    // Qt moves tabs of no size out of the window
    CHECK_FALSE(qtTabs->geometry().intersects(panels.window.rect()));
    CHECK(qtTabs->focusPolicy() == Qt::NoFocus);

    const std::string order = qtTabNames(*qtTabs);
    for (QDockWidget* dock : std::as_const(panels.docks)) {
        INFO(dock->objectName().toStdString());
        PanelTabBar* bar = panels.tabs->tabBar(dock);
        REQUIRE(bar != nullptr);
        CHECK_FALSE(bar->isHidden());
        CHECK(tabNames(*bar) == order);
        CHECK(bar->tab(bar->currentIndex()).name == dock->windowTitle());
        // Above the title, by default with icons alone
        CHECK(dock->titleBarWidget()->layout()->indexOf(bar) == 0);
        CHECK(bar->side() == PanelTabBar::Side::Above);
        CHECK(bar->look() == PanelTabBar::Look::Icons);
    }
    QDockWidget* front = panels.front();
    REQUIRE(front != nullptr);
    PanelTabBar* frontTabs = panels.tabs->tabBar(front);
    CHECK(frontTabs->isVisible());
    CHECK(frontTabs->width() == front->titleBarWidget()->width());
    CHECK(titleBarFits(front, frontTabs));

    SECTION("the title bar is as high as the rows of tabs at its width") {
        // Regression: the title bar took the height of the tabs at the width it would like,
        // not at its own, so a second row was cut off, or a row too many was left empty
        const int tab = frontTabs->tabRect(0).width();
        // Room for two tabs in a row
        panels.window.resizeDocks({front}, {2 * tab + 20}, Qt::Horizontal);
        settle();
        CHECK(frontTabs->rowCount() == 2);
        CHECK(titleBarFits(front, frontTabs));
        CHECK(frontTabs->tabRect(2).bottom() < frontTabs->height());
        // Room for all of them
        panels.window.resizeDocks({front}, {4 * tab + 20}, Qt::Horizontal);
        settle();
        CHECK(frontTabs->rowCount() == 1);
        CHECK(titleBarFits(front, frontTabs));
    }

    SECTION("a floating panel has no tabs, the others keep theirs") {
        QDockWidget* floating = panels.docks.at(2);
        floating->setFloating(true);
        settle();
        CHECK(panels.tabs->tabBar(floating)->isHidden());
        for (QDockWidget* dock : {panels.docks.at(0), panels.docks.at(1)}) {
            INFO(dock->objectName().toStdString());
            CHECK_FALSE(panels.tabs->tabBar(dock)->isHidden());
            CHECK(panels.tabs->tabBar(dock)->count() == 2);
        }
    }

    SECTION("a panel left alone has no tabs") {
        panels.docks.at(1)->close();
        panels.docks.at(2)->close();
        settle();
        CHECK(panels.tabs->tabBar(panels.docks.at(0))->isHidden());
    }

    SECTION("a click on a tab brings its panel to the front") {
        PanelTabBar* bar = panels.tabs->tabBar(front);
        const int other = (bar->currentIndex() + 1) % bar->count();
        const QString name = bar->tab(other).name;
        click(bar, bar->tabRect(other).center());
        settle();
        CHECK(qtTabs->tabText(qtTabs->currentIndex()) == name);
        QDockWidget* newFront = panels.front();
        REQUIRE(newFront != nullptr);
        CHECK(newFront->windowTitle() == name);
        PanelTabBar* newBar = panels.tabs->tabBar(newFront);
        CHECK(newBar->isVisible());
        CHECK(newBar->tab(newBar->currentIndex()).name == name);
    }

    SECTION("a dragged tab moves its panel in the group, also past its neighbour") {
        PanelTabBar* bar = panels.tabs->tabBar(front);
        const std::string before = qtTabNames(*qtTabs);
        const std::string moved =
            QStringList{qtTabs->tabText(1), qtTabs->tabText(2), qtTabs->tabText(0)}
                .join(QStringLiteral(", "))
                .toStdString();
        drag(bar, bar->tabRect(0).center(), bar->tabRect(2).center() + QPoint(5, 0));
        settle();
        CHECK(qtTabNames(*qtTabs) == moved);
        for (QDockWidget* dock : std::as_const(panels.docks)) {
            INFO(dock->objectName().toStdString());
            CHECK(tabNames(*panels.tabs->tabBar(dock)) == moved);
        }

        bar = panels.tabs->tabBar(panels.front());
        drag(bar, bar->tabRect(2).center(), bar->tabRect(0).topLeft() + QPoint(2, 2));
        settle();
        CHECK(qtTabNames(*qtTabs) == before);
        CHECK(tabNames(*panels.tabs->tabBar(panels.front())) == before);
        // The panel in front stays in front
        CHECK(panels.front() == front);
    }

    SECTION("the settings put the tabs under the panels, with names in one row") {
        TabSettings::set(PanelTabs::LOOK_NAMES, PanelTabs::ROWS_SINGLE, PanelTabs::SIDE_BELOW);
        panels.tabs->applySettings();
        settle();
        for (QDockWidget* dock : std::as_const(panels.docks)) {
            INFO(dock->objectName().toStdString());
            PanelTabBar* bar = panels.tabs->tabBar(dock);
            CHECK(bar->look() == PanelTabBar::Look::Names);
            CHECK(bar->rows() == PanelTabBar::Rows::Single);
            CHECK(bar->side() == PanelTabBar::Side::Below);
            CHECK(dock->titleBarWidget()->layout()->indexOf(bar) < 0);
            QLayout* body = dock->widget()->layout();
            REQUIRE(body != nullptr);
            CHECK(body->indexOf(bar) == body->count() - 1);
            CHECK_FALSE(bar->isHidden());
            CHECK(bar->count() == 3);
        }
        // Under the content of the panel in front
        PanelTabBar* bar = panels.tabs->tabBar(panels.front());
        CHECK(bar->geometry().top() >= panels.front()->widget()->height() / 2);

        TabSettings::set(PanelTabs::LOOK_ICONS, PanelTabs::ROWS_WRAP, PanelTabs::SIDE_ABOVE);
        panels.tabs->applySettings();
        settle();
        for (QDockWidget* dock : std::as_const(panels.docks)) {
            INFO(dock->objectName().toStdString());
            PanelTabBar* tabs = panels.tabs->tabBar(dock);
            CHECK(dock->titleBarWidget()->layout()->indexOf(tabs) == 0);
            CHECK(dock->widget()->layout()->indexOf(tabs) < 0);
            CHECK(tabs->side() == PanelTabBar::Side::Above);
        }
    }
}

TEST_CASE("Panel tabs: a group with a panel without the program's tabs keeps Qt's tabs",
          "[gui][panels][paneltabs]") {
    TabbedPanels panels;
    QDockWidget* other = newPanel(QStringLiteral("Delta"), panels.window);
    panels.window.addDockWidget(Qt::RightDockWidgetArea, other);
    panels.window.tabifyDockWidget(panels.docks.at(2), other);
    settle();

    QTabBar* qtTabs = qtTabsOf(panels.window, other);
    REQUIRE(qtTabs != nullptr);
    REQUIRE(qtTabs->count() == 4);
    CHECK(qtTabs->height() > 0);
    CHECK(qtTabs->geometry().intersects(panels.window.rect()));
    for (QDockWidget* dock : std::as_const(panels.docks)) {
        INFO(dock->objectName().toStdString());
        CHECK(panels.tabs->tabBar(dock)->isHidden());
    }

    SECTION("and hides them again once the panel goes") {
        other->close();
        settle();
        CHECK_FALSE(qtTabs->geometry().intersects(panels.window.rect()));
        CHECK_FALSE(panels.tabs->tabBar(panels.front())->isHidden());
    }
}
