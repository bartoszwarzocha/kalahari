/// @file test_command_shortcuts.cpp
/// @brief The keyboard shortcuts of the application's commands

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"

#include <QKeySequence>
#include <map>
#include <string>

using namespace kalahari::gui;

TEST_CASE("Command shortcuts: no two commands share a shortcut", "[gui][command][shortcuts]") {
    // Regression: F3 was both Find Next and the Properties panel toggle. Find Next had no
    // callback, so its action was disabled; once it got one, F3 was an ambiguous shortcut
    // that triggered neither command.
    registerAllCommands(CommandCallbacks{});

    std::map<std::string, std::string> owners;
    for (const Command& command : CommandRegistry::getInstance().getAllCommands()) {
        const QKeySequence keys = command.shortcut.toQKeySequence();
        if (keys.isEmpty()) {
            continue;
        }
        const std::string text = keys.toString(QKeySequence::PortableText).toStdString();
        const auto [owner, added] = owners.emplace(text, command.id);
        INFO(text << " belongs to " << owner->second << " and " << command.id);
        CHECK(added);
    }
    CHECK(owners.size() > 20);
}
