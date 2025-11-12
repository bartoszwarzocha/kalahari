/// @file test_command_registry.cpp
/// @brief Unit tests for CommandRegistry (Task #00025)
///
/// Tests cover:
/// - Singleton pattern (getInstance returns same instance)
/// - Command registration (registerCommand)
/// - Command retrieval (getCommand)
/// - Command unregistration (unregisterCommand)
/// - Command existence check (isCommandRegistered)
/// - Category filtering (getCommandsByCategory)
/// - All commands retrieval (getAllCommands)
/// - Category listing (getCategories)

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/command_registry.h"

using namespace kalahari::gui;

// =============================================================================
// Helper Functions
// =============================================================================

/// @brief Create sample command for testing
static Command createTestCommand(const std::string& id, const std::string& category = "Test") {
    Command cmd;
    cmd.id = id;
    cmd.label = "Test Command " + id;
    cmd.tooltip = "Tooltip for " + id;
    cmd.category = category;
    cmd.execute = []() { /* no-op */ };
    return cmd;
}

// =============================================================================
// Singleton Pattern Tests
// =============================================================================

TEST_CASE("CommandRegistry singleton pattern", "[gui][command][registry][singleton]") {
    SECTION("getInstance returns same instance") {
        CommandRegistry& instance1 = CommandRegistry::getInstance();
        CommandRegistry& instance2 = CommandRegistry::getInstance();

        REQUIRE(&instance1 == &instance2);
    }
}

// =============================================================================
// Registration Tests
// =============================================================================

TEST_CASE("CommandRegistry registration", "[gui][command][registry][registration]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear(); // Start with clean state

    SECTION("registerCommand adds command to registry") {
        Command cmd = createTestCommand("test.command1");
        registry.registerCommand(cmd);

        REQUIRE(registry.isCommandRegistered("test.command1"));
        REQUIRE(registry.getCommandCount() == 1);
    }

    SECTION("registerCommand with duplicate ID replaces existing") {
        Command cmd1 = createTestCommand("test.command1");
        cmd1.label = "First Label";
        registry.registerCommand(cmd1);

        Command cmd2 = createTestCommand("test.command1");
        cmd2.label = "Second Label";
        registry.registerCommand(cmd2);

        REQUIRE(registry.getCommandCount() == 1);

        const Command* retrieved = registry.getCommand("test.command1");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->label == "Second Label");
    }

    SECTION("registerCommand with multiple commands") {
        registry.registerCommand(createTestCommand("cmd1", "File"));
        registry.registerCommand(createTestCommand("cmd2", "Edit"));
        registry.registerCommand(createTestCommand("cmd3", "View"));

        REQUIRE(registry.getCommandCount() == 3);
        REQUIRE(registry.isCommandRegistered("cmd1"));
        REQUIRE(registry.isCommandRegistered("cmd2"));
        REQUIRE(registry.isCommandRegistered("cmd3"));
    }

    registry.clear();
}

// =============================================================================
// Unregistration Tests
// =============================================================================

TEST_CASE("CommandRegistry unregistration", "[gui][command][registry][unregistration]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();

    SECTION("unregisterCommand removes command from registry") {
        Command cmd = createTestCommand("test.command1");
        registry.registerCommand(cmd);

        REQUIRE(registry.isCommandRegistered("test.command1"));

        registry.unregisterCommand("test.command1");

        REQUIRE_FALSE(registry.isCommandRegistered("test.command1"));
        REQUIRE(registry.getCommandCount() == 0);
    }

    SECTION("unregisterCommand with non-existent ID is safe") {
        registry.unregisterCommand("non.existent.command");
        // Should not crash or throw
        REQUIRE(registry.getCommandCount() == 0);
    }

    registry.clear();
}

// =============================================================================
// Query Tests
// =============================================================================

TEST_CASE("CommandRegistry command retrieval", "[gui][command][registry][query]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();

    SECTION("getCommand returns valid pointer for existing command") {
        Command cmd = createTestCommand("test.command1");
        cmd.label = "Test Label";
        registry.registerCommand(cmd);

        const Command* retrieved = registry.getCommand("test.command1");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->id == "test.command1");
        REQUIRE(retrieved->label == "Test Label");
    }

    SECTION("getCommand returns nullptr for non-existent command") {
        const Command* retrieved = registry.getCommand("non.existent");
        REQUIRE(retrieved == nullptr);
    }

    SECTION("getCommand non-const allows modification") {
        Command cmd = createTestCommand("test.command1");
        cmd.label = "Original Label";
        registry.registerCommand(cmd);

        Command* retrieved = registry.getCommand("test.command1");
        REQUIRE(retrieved != nullptr);

        retrieved->label = "Modified Label";

        const Command* check = registry.getCommand("test.command1");
        REQUIRE(check->label == "Modified Label");
    }

    registry.clear();
}

TEST_CASE("CommandRegistry category filtering", "[gui][command][registry][category]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();

    SECTION("getCommandsByCategory returns commands in category") {
        registry.registerCommand(createTestCommand("file.new", "File"));
        registry.registerCommand(createTestCommand("file.open", "File"));
        registry.registerCommand(createTestCommand("edit.cut", "Edit"));
        registry.registerCommand(createTestCommand("edit.copy", "Edit"));
        registry.registerCommand(createTestCommand("view.zoom", "View"));

        std::vector<Command> fileCommands = registry.getCommandsByCategory("File");
        REQUIRE(fileCommands.size() == 2);

        std::vector<Command> editCommands = registry.getCommandsByCategory("Edit");
        REQUIRE(editCommands.size() == 2);

        std::vector<Command> viewCommands = registry.getCommandsByCategory("View");
        REQUIRE(viewCommands.size() == 1);
    }

    SECTION("getCommandsByCategory returns empty for non-existent category") {
        registry.registerCommand(createTestCommand("test.cmd", "File"));

        std::vector<Command> result = registry.getCommandsByCategory("NonExistent");
        REQUIRE(result.empty());
    }

    registry.clear();
}

TEST_CASE("CommandRegistry all commands retrieval", "[gui][command][registry][all]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();

    SECTION("getAllCommands returns all registered commands") {
        registry.registerCommand(createTestCommand("cmd1", "File"));
        registry.registerCommand(createTestCommand("cmd2", "Edit"));
        registry.registerCommand(createTestCommand("cmd3", "View"));

        std::vector<Command> allCommands = registry.getAllCommands();
        REQUIRE(allCommands.size() == 3);
    }

    SECTION("getAllCommands returns empty when no commands registered") {
        std::vector<Command> allCommands = registry.getAllCommands();
        REQUIRE(allCommands.empty());
    }

    registry.clear();
}

TEST_CASE("CommandRegistry category listing", "[gui][command][registry][categories]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();

    SECTION("getCategories returns unique sorted categories") {
        registry.registerCommand(createTestCommand("cmd1", "File"));
        registry.registerCommand(createTestCommand("cmd2", "Edit"));
        registry.registerCommand(createTestCommand("cmd3", "View"));
        registry.registerCommand(createTestCommand("cmd4", "File")); // duplicate category
        registry.registerCommand(createTestCommand("cmd5", "Edit")); // duplicate category

        std::vector<std::string> categories = registry.getCategories();

        REQUIRE(categories.size() == 3); // Unique: Edit, File, View

        // Check sorted order
        REQUIRE(categories[0] == "Edit");
        REQUIRE(categories[1] == "File");
        REQUIRE(categories[2] == "View");
    }

    SECTION("getCategories returns empty when no commands registered") {
        std::vector<std::string> categories = registry.getCategories();
        REQUIRE(categories.empty());
    }

    registry.clear();
}

// =============================================================================
// Utility Tests
// =============================================================================

TEST_CASE("CommandRegistry utility methods", "[gui][command][registry][utility]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();

    SECTION("getCommandCount returns correct count") {
        REQUIRE(registry.getCommandCount() == 0);

        registry.registerCommand(createTestCommand("cmd1"));
        REQUIRE(registry.getCommandCount() == 1);

        registry.registerCommand(createTestCommand("cmd2"));
        REQUIRE(registry.getCommandCount() == 2);

        registry.unregisterCommand("cmd1");
        REQUIRE(registry.getCommandCount() == 1);
    }

    SECTION("clear removes all commands") {
        registry.registerCommand(createTestCommand("cmd1"));
        registry.registerCommand(createTestCommand("cmd2"));
        registry.registerCommand(createTestCommand("cmd3"));

        REQUIRE(registry.getCommandCount() == 3);

        registry.clear();

        REQUIRE(registry.getCommandCount() == 0);
        REQUIRE_FALSE(registry.isCommandRegistered("cmd1"));
        REQUIRE_FALSE(registry.isCommandRegistered("cmd2"));
        REQUIRE_FALSE(registry.isCommandRegistered("cmd3"));
    }

    registry.clear();
}
