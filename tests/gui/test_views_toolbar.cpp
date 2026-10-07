/// @file test_views_toolbar.cpp
/// @brief The Views toolbar: the view modes, the switches and the zoom of the View menu

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/icon_registry.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/toolbar_manager.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QLabel>
#include <QMainWindow>
#include <QToolBar>

using namespace kalahari::gui;

namespace {

/// The toolbar of @p window named @p id
QToolBar* toolbarOf(const QMainWindow& window, const QString& id) {
    return window.findChild<QToolBar*>(id);
}

/// The layout of the toolbars that a window saved before the Views toolbar existed
QByteArray layoutWithoutViewsToolbar() {
    QMainWindow window;
    window.setCentralWidget(new QLabel(QStringLiteral("text")));
    const auto add = [&window](const char* id, bool visible) {
        auto* toolbar = new QToolBar(QString::fromLatin1(id), &window);
        toolbar->setObjectName(QString::fromLatin1(id));
        toolbar->addAction(QString::fromLatin1(id));
        window.addToolBar(Qt::TopToolBarArea, toolbar);
        toolbar->setVisible(visible);
    };
    for (const char* id : {"quickActions", "edit", "format", "insert"}) {
        add(id, true);
    }
    window.addToolBarBreak(Qt::TopToolBarArea);
    for (const char* id : {"book", "styles", "tools", "help"}) {
        add(id, true);
    }
    add("file", false);
    add("view", false);
    return window.saveState();
}

}  // namespace

TEST_CASE("Views toolbar: the view modes, switches and zoom of the View menu", "[gui][toolbar]") {
    registerAllCommands(CommandCallbacks{});
    registerAllIcons();
    CommandRegistry& registry = CommandRegistry::getInstance();

    QMainWindow window;
    ToolbarManager manager(&window);
    manager.createToolbars(registry);

    QToolBar* toolbar = manager.getToolbar("views");
    REQUIRE(toolbar != nullptr);
    CHECK(window.toolBarArea(toolbar) == Qt::TopToolBarArea);
    CHECK_FALSE(toolbar->isHidden());

    // The very actions of the View menu, so a button is checked with its menu item
    const QList<QAction*> actions = toolbar->actions();
    REQUIRE(actions.size() == 10);
    CHECK(actions[0] == registry.getAction(std::string("view.mode.continuous")));
    CHECK(actions[1] == registry.getAction(std::string("view.mode.page")));
    CHECK(actions[2] == registry.getAction(std::string("view.mode.distraction-free")));
    CHECK(actions[3]->isSeparator());
    CHECK(actions[4] == registry.getAction(std::string("view.focus")));
    CHECK(actions[5] == registry.getAction(std::string("view.typewriter")));
    CHECK(actions[6]->isSeparator());
    CHECK(actions[7] == registry.getAction(std::string("view.zoomOut")));
    CHECK(actions[8] == registry.getAction(std::string("view.zoomIn")));
    CHECK(actions[9] == registry.getAction(std::string("view.resetZoom")));

    // Every button has an icon of the icon theme
    auto& icons = kalahari::core::IconRegistry::getInstance();
    for (const char* id : {"view.mode.continuous", "view.mode.page", "view.mode.distraction-free",
                           "view.focus", "view.typewriter", "view.zoomOut", "view.zoomIn",
                           "view.resetZoom"}) {
        INFO(id);
        CHECK(icons.hasIcon(QString::fromLatin1(id)));
    }
}

TEST_CASE("Views toolbar: shown in the second row of a layout saved before it existed",
          "[gui][toolbar]") {
    // A window keeps its toolbar layout between sessions; the layout saved by an earlier
    // version has no Views toolbar, which still has to show, next to the other toolbars
    // of its row, without the layout being reset
    const QByteArray savedLayout = layoutWithoutViewsToolbar();

    registerAllCommands(CommandCallbacks{});
    QMainWindow window;
    window.setCentralWidget(new QLabel(QStringLiteral("text")));
    ToolbarManager manager(&window);
    manager.createToolbars(CommandRegistry::getInstance());
    REQUIRE(window.restoreState(savedLayout));

    window.resize(1600, 900);
    window.show();
    QApplication::processEvents();

    QToolBar* views = toolbarOf(window, QStringLiteral("views"));
    QToolBar* book = toolbarOf(window, QStringLiteral("book"));
    QToolBar* edit = toolbarOf(window, QStringLiteral("edit"));
    REQUIRE(views != nullptr);
    REQUIRE(book != nullptr);
    REQUIRE(edit != nullptr);
    CHECK(window.toolBarArea(views) == Qt::TopToolBarArea);
    CHECK(views->isVisible());
    CHECK(views->y() == book->y());
    CHECK(views->y() > edit->y());
}
