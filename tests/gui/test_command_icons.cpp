/// @file test_command_icons.cpp
/// @brief Every command in a menu has an icon

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/icon_registry.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/icon_registrar.h"

#include <QString>

using namespace kalahari::gui;

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
