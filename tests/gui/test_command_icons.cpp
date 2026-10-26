/// @file test_command_icons.cpp
/// @brief Every command in a menu has an icon, and every submenu, in the icon colors

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/art_provider.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/recent_books_menu.h"
#include "kalahari/gui/toolbar_manager.h"

#include <QAction>
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QList>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPair>
#include <QString>

using namespace kalahari::gui;

namespace {

/// The submenus in @p menu and in its submenus, each with its path in the menus
void collectSubmenus(const QMenu* menu, const QString& path,
                     QList<QPair<QString, const QAction*>>& found) {
    for (const QAction* item : menu->actions()) {
        if (const QMenu* submenu = item->menu()) {
            const QString inner = path + QStringLiteral(" > ") + item->text();
            found.append({inner, item});
            collectSubmenus(submenu, inner, found);
        }
    }
}

/// Whether @p icon, drawn at its largest size, has pixels of @p color
bool drawnIn(const QIcon& icon, const QColor& color) {
    const QList<QSize> sizes = icon.availableSizes();
    const QSize size = sizes.isEmpty() ? QSize(24, 24) : sizes.last();
    const QImage image = icon.pixmap(size).toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixel(x, y) == color.rgba()) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

TEST_CASE("Menus: every command in a menu has an icon", "[gui][icons]") {
    // Regression: a menu item without an icon among items with icons, and a warning in the
    // log for each one when the menus were built
    registerAllCommands(CommandCallbacks{});
    registerAllIcons();
    const auto& icons = kalahari::core::IconRegistry::getInstance();

    for (const Command& command : CommandRegistry::getInstance().getAllCommands()) {
        if (!command.showInMenu) {
            continue;
        }
        INFO(command.id);
        CHECK(icons.hasIcon(QString::fromStdString(command.id)));
    }
}

TEST_CASE("Menus: every submenu has an icon, in the icon colors after they change",
          "[gui][icons]") {
    // Import, Export, Panels, Zoom and the other submenus had none; the one of Recent Books
    // kept the colors that the icons had at the start
    registerAllCommands(CommandCallbacks{});
    registerAllIcons();
    CommandRegistry& registry = CommandRegistry::getInstance();

    // The main window's menus, as MainWindow makes them
    QMainWindow window;
    MenuBuilder builder;
    builder.buildMenuBar(registry, &window);
    QMenu* fileMenu = builder.getMenu("FILE");
    REQUIRE(fileMenu != nullptr);
    auto* recentBooks = new RecentBooksMenu(fileMenu);
    recentBooks->insertInto(fileMenu);
    ToolbarManager toolbars(&window);
    toolbars.createToolbars(registry);
    toolbars.createViewMenuActions(builder.getMenu("VIEW"));

    QList<QPair<QString, const QAction*>> submenus;
    for (const QAction* menu : window.menuBar()->actions()) {
        REQUIRE(menu->menu() != nullptr);
        collectSubmenus(menu->menu(), menu->text(), submenus);
    }
    // Recent Books, Import, Export, Text Style, Statistics, Plugins, Assistant Actions,
    // View Mode, Zoom, Margin Numbering, Panels, Perspectives, Toolbars
    CHECK(submenus.size() == 13);
    for (const auto& [path, submenu] : submenus) {
        INFO(path.toStdString());
        CHECK_FALSE(submenu->icon().isNull());
    }

    // New icon colors: the submenus show them, as the commands
    auto& artProvider = kalahari::core::ArtProvider::getInstance();
    const QColor primary(QStringLiteral("#C2185B"));
    REQUIRE(artProvider.getPrimaryColor() != primary);
    artProvider.setPrimaryColor(primary);
    const QAction* save = registry.getAction(std::string("file.save"));
    REQUIRE(save != nullptr);
    REQUIRE(drawnIn(save->icon(), primary));
    for (const auto& [path, submenu] : submenus) {
        INFO(path.toStdString());
        CHECK(drawnIn(submenu->icon(), primary));
    }
}

TEST_CASE("Save All: its icon is two diskettes, as Save's, in every icon style", "[gui][icons]") {
    // Its layers did not look like Save and Save As: the diskette of Save comes in front of a
    // second one behind it, up to the right
    registerAllIcons();
    auto& icons = kalahari::core::IconRegistry::getInstance();
    const QColor primary(QStringLiteral("#C2185B"));
    const QColor secondary(QStringLiteral("#1565C0"));
    for (const char* style : {"twotone", "outlined", "rounded", "filled"}) {
        INFO(style);
        const QIcon icon = icons.getIconWithColors(QStringLiteral("file.saveAll"),
                                                   QString::fromLatin1(style), 24, primary,
                                                   secondary);
        REQUIRE_FALSE(icon.isNull());
        const QImage image =
            icon.pixmap(QSize(24, 24)).toImage().convertToFormat(QImage::Format_ARGB32);
        REQUIRE(image.size() == QSize(24, 24));
        // The left edge of the diskette in front, smaller than Save's, and the right edge of
        // the one behind it
        CHECK(image.pixelColor(1, 15) == primary);
        CHECK(image.pixelColor(20, 10) == primary);
        // Between them, under the corner of the one behind, nothing
        CHECK(image.pixelColor(16, 22).alpha() == 0);
    }
}
