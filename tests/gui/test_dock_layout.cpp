/// @file test_dock_layout.cpp
/// @brief The panels of the main window: the Annotations panel is on top of the panels on the
/// right by default, choosing an element in the Navigator does not cover it, and on a small
/// screen the panels on the right make room for the text

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/find_replace_bar.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/toolbar_manager.h"
#include "kalahari/gui/widgets/info_bar.h"
#include "kalahari/gui/widgets/standalone_info_bar.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QMetaObject>
#include <QPushButton>
#include <QRect>
#include <QScreen>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// The part of a small screen for windows: 1366×768 at 150% less the taskbar, 911×480
const QRect SMALL_SCREEN(0, 0, 911, 480);

/// The part of a large screen for windows: 1920×1080 less the taskbar
const QRect LARGE_SCREEN(0, 0, 1920, 1040);

/// The title bar and the borders of a window on a small screen, with a margin
constexpr int WINDOW_FRAME_HEIGHT = 46;

/// The setting that the bar about a small screen was shown
constexpr const char* NOTICE_SHOWN = "window.smallScreenNoticeShown";

/// Title of the tab on top of the tab group holding @p dock (empty without a group)
QString raisedTab(const QMainWindow& window, const QDockWidget* dock) {
    for (const QTabBar* tabBar : window.findChildren<QTabBar*>()) {
        for (int i = 0; i < tabBar->count(); ++i) {
            if (tabBar->tabText(i) == dock->windowTitle()) {
                return tabBar->tabText(tabBar->currentIndex());
            }
        }
    }
    return {};
}

/// A book open for the test; closed again when the test ends
struct OpenBook {
    QTemporaryDir dir;

    OpenBook() {
        REQUIRE(dir.isValid());
        REQUIRE(core::ProjectManager::getInstance().createProject(dir.path(), "Panels",
                                                                  "Author", "en", true));
    }
    ~OpenBook() { core::ProjectManager::getInstance().closeProject(false); }

    OpenBook(const OpenBook&) = delete;
    OpenBook& operator=(const OpenBook&) = delete;
};

/// The panels of the main window, with the Navigator's requests handled as in MainWindow
struct Panels {
    // Not the window's own: Qt 6.9.1 crashes destroying a main window that has a status bar but
    // no toolbar once its panels were laid out again (QTBUG-137524). MainWindow has toolbars.
    QStatusBar statusBar;
    QMainWindow window;
    DockCoordinator* docks = nullptr;  // owned by the window, as in MainWindow

    Panels() {
        registerAllCommands(CommandCallbacks{});
        docks = new DockCoordinator(&window, &window);
        docks->createDocks();
        auto* navigator =
            new NavigatorCoordinator(docks->navigatorPanel(), docks->propertiesPanel(),
                                     docks->centralTabs(), &statusBar, &window);
        QObject::connect(docks, &DockCoordinator::navigatorRequestProperties, navigator,
                         &NavigatorCoordinator::onRequestProperties);
        QObject::connect(docks, &DockCoordinator::navigatorRequestSectionProperties, navigator,
                         &NavigatorCoordinator::onRequestSectionProperties);
        QObject::connect(docks, &DockCoordinator::navigatorRequestPartProperties, navigator,
                         &NavigatorCoordinator::onRequestPartProperties);
        window.resize(1200, 800);
        window.show();
        QApplication::processEvents();
    }

    /// Whether @p dock is the tab on top of its tab group
    [[nodiscard]] bool onTop(const QDockWidget* dock) const {
        return raisedTab(window, dock) == dock->windowTitle();
    }

    /// The panels on the right of the window
    [[nodiscard]] QList<QDockWidget*> rightDocks() const {
        return {docks->propertiesDock(), docks->searchDock(), docks->assistantDock(),
                docks->annotationsDock()};
    }

    /// Whether the panels on the right are all open (in their tabs) or all closed
    [[nodiscard]] bool rightDocksOpen(bool open) const {
        for (const QDockWidget* dock : rightDocks()) {
            if (dock->toggleViewAction()->isChecked() != open) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace

TEST_CASE("Panels: the Annotations panel is on top of the right panels by default",
          "[gui][panels]") {
    Panels panels;
    CHECK(panels.onTop(panels.docks->annotationsDock()));

    SECTION("Reset Layout puts it on top again") {
        panels.docks->propertiesDock()->raise();
        QApplication::processEvents();
        REQUIRE(panels.onTop(panels.docks->propertiesDock()));

        panels.docks->resetLayout(false, false);
        QApplication::processEvents();
        const QScreen* screen = panels.window.screen();
        REQUIRE(screen != nullptr);
        if (screen->availableGeometry().width() < DockCoordinator::SMALL_SCREEN_WIDTH) {
            // A small screen (the offscreen screen of the tests) has them hidden by default,
            // and the bar above the text brings them back
            CHECK(panels.rightDocksOpen(false));
            CHECK(panels.docks->layoutNoticeBar()->isVisible());
            panels.docks->showRightPanels();
            QApplication::processEvents();
        } else {
            CHECK_FALSE(panels.docks->layoutNoticeBar()->isVisible());
        }
        CHECK(panels.onTop(panels.docks->annotationsDock()));
    }
}

TEST_CASE("Panels: a small screen leaves the text room", "[gui][panels]") {
    Panels panels;
    QDockWidget* navigator = panels.docks->navigatorDock();
    REQUIRE(panels.rightDocksOpen(true));

    SECTION("A large screen keeps every panel") {
        CHECK_FALSE(panels.docks->fitToScreen(LARGE_SCREEN));
        QApplication::processEvents();
        CHECK(panels.rightDocksOpen(true));
    }

    SECTION("A small screen hides the panels on the right and narrows the Navigator") {
        REQUIRE(panels.docks->fitToScreen(SMALL_SCREEN));
        QApplication::processEvents();
        CHECK(panels.rightDocksOpen(false));
        CHECK(navigator->isVisible());
        // About a fifth of the screen, as far as the Navigator's minimum allows
        CAPTURE(navigator->width(), navigator->minimumSizeHint().width());
        CHECK(navigator->width() <=
              qMax(SMALL_SCREEN.width() / 4, navigator->minimumSizeHint().width()));
        CHECK(navigator->width() >= navigator->minimumSizeHint().width());
        // The bar is for the first start and for Reset Layout
        CHECK_FALSE(panels.docks->layoutNoticeBar()->isVisible());
    }
}

TEST_CASE("Panels: the bar about a small screen says how to show the panels again",
          "[gui][panels]") {
    Panels panels;
    REQUIRE(panels.docks->fitToScreen(SMALL_SCREEN));
    panels.docks->showLayoutNotice();
    QApplication::processEvents();
    InfoBar* bar = panels.docks->layoutNoticeBar();
    REQUIRE(bar->isVisible());
    CHECK(bar->message().contains(QStringLiteral("View > Panels")));

    SECTION("Its button shows the panels on the right, with the Annotations panel on top") {
        auto* button = bar->findChild<QPushButton*>();
        REQUIRE(button != nullptr);
        REQUIRE(button->isVisible());
        button->click();
        QApplication::processEvents();
        CHECK(panels.rightDocksOpen(true));
        CHECK(panels.onTop(panels.docks->annotationsDock()));
        CHECK_FALSE(bar->isVisible());
    }

    SECTION("A panel shown from the View menu ends it") {
        panels.docks->searchDock()->toggleViewAction()->trigger();
        QApplication::processEvents();
        CHECK(panels.docks->searchDock()->isVisible());
        CHECK_FALSE(bar->isVisible());
    }

    SECTION("Its close button ends it, and the panels stay hidden") {
        auto* close = bar->findChild<QToolButton*>();
        REQUIRE(close != nullptr);
        close->click();
        QApplication::processEvents();
        CHECK_FALSE(bar->isVisible());
        CHECK(panels.rightDocksOpen(false));
    }
}

TEST_CASE("Panels: the first layout on a small screen says why once", "[gui][panels]") {
    auto& settings = core::SettingsManager::getInstance();
    settings.set<bool>(NOTICE_SHOWN, false);  // a first start, whatever tests ran before

    SECTION("A large screen keeps every panel and says nothing") {
        Panels panels;
        panels.docks->fitFirstLayout(LARGE_SCREEN);
        QApplication::processEvents();
        CHECK(panels.rightDocksOpen(true));
        CHECK_FALSE(panels.docks->layoutNoticeBar()->isVisible());
        CHECK_FALSE(settings.get<bool>(NOTICE_SHOWN, false));
    }

    SECTION("A small screen: the bar the first time, not on the next start") {
        {
            Panels first;
            first.docks->fitFirstLayout(SMALL_SCREEN);
            QApplication::processEvents();
            CHECK(first.rightDocksOpen(false));
            CHECK(first.docks->layoutNoticeBar()->isVisible());
            CHECK(settings.get<bool>(NOTICE_SHOWN, false));
        }
        Panels next;  // a start without a saved layout again
        next.docks->fitFirstLayout(SMALL_SCREEN);
        QApplication::processEvents();
        CHECK(next.rightDocksOpen(false));
        CHECK_FALSE(next.docks->layoutNoticeBar()->isVisible());
    }

    settings.set<bool>(NOTICE_SHOWN, false);  // the default for the tests that follow
}

TEST_CASE("Panels: every panel and the text fit a small screen side by side", "[gui][panels]") {
    // Regression: the window took the width of the bar of a file outside the book, so it
    // reached beyond a small screen
    Panels panels;
    // The bars of the program's window: the menus, the toolbars and the status bar
    auto* menus = new QMenuBar(&panels.window);
    menus->setNativeMenuBar(false);
    for (const char* title : {"File", "Edit", "Book", "Insert", "Format", "Tools", "Assistant",
                              "View", "Help"}) {
        menus->addMenu(QString::fromLatin1(title));
    }
    panels.window.setMenuBar(menus);
    ToolbarManager toolbars(&panels.window);
    toolbars.createToolbars(CommandRegistry::getInstance());
    panels.window.statusBar()->showMessage(QStringLiteral("Words: 0"));

    panels.docks->logDock()->show();
    panels.docks->standaloneInfoBar()->show();
    panels.docks->showLayoutNotice();
    auto* chapter = new EditorPanel();
    panels.docks->centralTabs()->addTab(chapter, QStringLiteral("Chapter 1"));
    panels.docks->centralTabs()->setCurrentWidget(chapter);
    QApplication::processEvents();

    const QSize needed = panels.window.minimumSizeHint();
    CAPTURE(needed.width());
    CHECK(needed.width() <= SMALL_SCREEN.width());

    SECTION("The bar to find and replace fits the text") {
        panels.window.resize(SMALL_SCREEN.width(), SMALL_SCREEN.height() - WINDOW_FRAME_HEIGHT);
        chapter->getBookEditor()->showFindReplace();
        QApplication::processEvents();
        auto* findBar = chapter->findChild<editor::FindReplaceBar*>();
        REQUIRE(findBar != nullptr);
        REQUIRE(findBar->isVisible());
        CAPTURE(findBar->width(), findBar->minimumSizeHint().width(),
                chapter->getBookEditor()->width());
        CHECK(findBar->minimumSizeHint().width() <= findBar->width());
    }
}

TEST_CASE("Panels: choosing an element in the Navigator keeps the panel on top",
          "[gui][panels][navigator]") {
    // Regression: every element chosen in the Navigator brought the Properties panel to the
    // front, so the Annotations panel seemed gone once a chapter was opened from the Navigator
    OpenBook book;
    Panels panels;
    NavigatorPanel* navigator = panels.docks->navigatorPanel();
    const auto& projects = core::ProjectManager::getInstance();
    REQUIRE(projects.project() != nullptr);
    navigator->loadProject(*projects.project(), projects.bookTypes());
    auto* tree = navigator->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    QTreeWidgetItem* root = tree->topLevelItem(0);
    REQUIRE(root != nullptr);
    REQUIRE(panels.onTop(panels.docks->annotationsDock()));

    SECTION("with the mouse") {
        emit tree->itemClicked(root, 0);
    }
    SECTION("with the keys") {
        tree->setCurrentItem(root);
    }
    QApplication::processEvents();

    CHECK(panels.onTop(panels.docks->annotationsDock()));
}

TEST_CASE("Panels: the Properties command of the Navigator brings the Properties panel forward",
          "[gui][panels][navigator]") {
    OpenBook book;
    Panels panels;
    REQUIRE(panels.onTop(panels.docks->annotationsDock()));

    REQUIRE(QMetaObject::invokeMethod(panels.docks->navigatorPanel(), "onContextMenuProperties",
                                      Qt::DirectConnection));
    QApplication::processEvents();

    CHECK(panels.onTop(panels.docks->propertiesDock()));
}
