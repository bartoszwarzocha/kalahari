/// @file test_dock_layout.cpp
/// @brief The panels on the right of the main window: the Annotations panel is on top by
/// default, and choosing an element in the Navigator does not cover it

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/navigator_panel.h"

#include <QApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QMetaObject>
#include <QStatusBar>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItem>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

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
        CHECK(panels.onTop(panels.docks->annotationsDock()));
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
