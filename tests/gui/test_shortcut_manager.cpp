/// @file test_shortcut_manager.cpp
/// @brief Unit tests for ShortcutManager (Task #00027)
///
/// Tests cover:
/// - Singleton pattern
/// - Shortcut binding/unbinding
/// - Query operations
/// - Integration with CommandRegistry
/// - JSON persistence (save/load)
/// - Utility methods

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/shortcut_manager.h"
#include "kalahari/gui/command_registry.h"
#include <fstream>

using namespace kalahari::gui;

// =============================================================================
// Helper Functions
// =============================================================================

/// @brief Global counter for test command execution
static int g_testExecutionCount = 0;

/// @brief Reset execution counter
static void resetTestCounter() {
    g_testExecutionCount = 0;
}

/// @brief Test command that increments counter
static void testCommandExecute() {
    g_testExecutionCount++;
}

/// @brief Setup test command in CommandRegistry
static void setupTestCommand(const std::string& id) {
    Command cmd;
    cmd.id = id;
    cmd.execute = testCommandExecute;
    CommandRegistry::getInstance().registerCommand(cmd);
}

/// @brief Check if file exists
static bool fileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

/// @brief Delete file
static void deleteFile(const std::string& path) {
    std::remove(path.c_str());
}

// =============================================================================
// Test: Singleton Pattern
// =============================================================================

TEST_CASE("ShortcutManager singleton pattern", "[gui][shortcut][singleton]") {
    SECTION("getInstance returns same instance") {
        ShortcutManager& instance1 = ShortcutManager::getInstance();
        ShortcutManager& instance2 = ShortcutManager::getInstance();

        REQUIRE(&instance1 == &instance2);
    }
}

// =============================================================================
// Test: Binding
// =============================================================================

TEST_CASE("ShortcutManager binding", "[gui][shortcut][binding]") {
    ShortcutManager& manager = ShortcutManager::getInstance();
    manager.clear();

    SECTION("bindShortcut adds shortcut to bindings") {
        KeyboardShortcut ctrlS('S', true, false, false);
        bool result = manager.bindShortcut(ctrlS, "file.save");

        REQUIRE(result == true);
        REQUIRE(manager.isShortcutBound(ctrlS));
        REQUIRE(manager.getBindingCount() == 1);
    }

    SECTION("bindShortcut rejects empty shortcuts") {
        KeyboardShortcut empty; // keyCode = 0
        bool result = manager.bindShortcut(empty, "some.command");

        REQUIRE(result == false);
        REQUIRE(manager.getBindingCount() == 0);
    }

    SECTION("bindShortcut overrides existing binding") {
        KeyboardShortcut ctrlS('S', true, false, false);
        manager.bindShortcut(ctrlS, "file.save");
        manager.bindShortcut(ctrlS, "file.save.as"); // Override

        REQUIRE(manager.getBindingCount() == 1);

        auto commandId = manager.getCommandForShortcut(ctrlS);
        REQUIRE(commandId.has_value());
        REQUIRE(commandId.value() == "file.save.as");
    }

    SECTION("bindShortcut supports multiple bindings") {
        KeyboardShortcut ctrlS('S', true, false, false);
        KeyboardShortcut ctrlO('O', true, false, false);
        KeyboardShortcut ctrlN('N', true, false, false);

        manager.bindShortcut(ctrlS, "file.save");
        manager.bindShortcut(ctrlO, "file.open");
        manager.bindShortcut(ctrlN, "file.new");

        REQUIRE(manager.getBindingCount() == 3);
    }

    manager.clear();
}

// =============================================================================
// Test: Unbinding
// =============================================================================

TEST_CASE("ShortcutManager unbinding", "[gui][shortcut][unbinding]") {
    ShortcutManager& manager = ShortcutManager::getInstance();
    manager.clear();

    SECTION("unbindShortcut removes binding") {
        KeyboardShortcut ctrlS('S', true, false, false);
        manager.bindShortcut(ctrlS, "file.save");

        REQUIRE(manager.isShortcutBound(ctrlS));

        manager.unbindShortcut(ctrlS);

        REQUIRE_FALSE(manager.isShortcutBound(ctrlS));
        REQUIRE(manager.getBindingCount() == 0);
    }

    SECTION("unbindShortcut with non-existent shortcut is safe") {
        KeyboardShortcut ctrlX('X', true, false, false);
        manager.unbindShortcut(ctrlX); // Should not crash

        REQUIRE(manager.getBindingCount() == 0);
    }

    manager.clear();
}

// =============================================================================
// Test: Query
// =============================================================================

TEST_CASE("ShortcutManager query", "[gui][shortcut][query]") {
    ShortcutManager& manager = ShortcutManager::getInstance();
    manager.clear();

    SECTION("getCommandForShortcut returns command ID") {
        KeyboardShortcut ctrlS('S', true, false, false);
        manager.bindShortcut(ctrlS, "file.save");

        auto commandId = manager.getCommandForShortcut(ctrlS);

        REQUIRE(commandId.has_value());
        REQUIRE(commandId.value() == "file.save");
    }

    SECTION("getCommandForShortcut returns nullopt for unbound shortcut") {
        KeyboardShortcut ctrlX('X', true, false, false);

        auto commandId = manager.getCommandForShortcut(ctrlX);

        REQUIRE_FALSE(commandId.has_value());
    }

    SECTION("getAllBindings returns all bindings") {
        KeyboardShortcut ctrlS('S', true, false, false);
        KeyboardShortcut ctrlO('O', true, false, false);

        manager.bindShortcut(ctrlS, "file.save");
        manager.bindShortcut(ctrlO, "file.open");

        auto bindings = manager.getAllBindings();

        REQUIRE(bindings.size() == 2);
        REQUIRE(bindings[ctrlS] == "file.save");
        REQUIRE(bindings[ctrlO] == "file.open");
    }

    SECTION("isShortcutBound returns correct status") {
        KeyboardShortcut ctrlS('S', true, false, false);
        KeyboardShortcut ctrlO('O', true, false, false);

        manager.bindShortcut(ctrlS, "file.save");

        REQUIRE(manager.isShortcutBound(ctrlS));
        REQUIRE_FALSE(manager.isShortcutBound(ctrlO));
    }

    manager.clear();
}

// =============================================================================
// Test: Execution (Integration with CommandRegistry)
// =============================================================================

TEST_CASE("ShortcutManager execution", "[gui][shortcut][execution]") {
    ShortcutManager& manager = ShortcutManager::getInstance();
    CommandRegistry& registry = CommandRegistry::getInstance();
    manager.clear();
    registry.clear();
    resetTestCounter();

    SECTION("executeShortcut executes bound command") {
        setupTestCommand("test.command");

        KeyboardShortcut ctrlT('T', true, false, false);
        manager.bindShortcut(ctrlT, "test.command");

        CommandExecutionResult result = manager.executeShortcut(ctrlT);

        REQUIRE(result == CommandExecutionResult::Success);
        REQUIRE(g_testExecutionCount == 1);
    }

    SECTION("executeShortcut returns CommandNotFound for unbound shortcut") {
        KeyboardShortcut ctrlX('X', true, false, false);

        CommandExecutionResult result = manager.executeShortcut(ctrlX);

        REQUIRE(result == CommandExecutionResult::CommandNotFound);
        REQUIRE(g_testExecutionCount == 0);
    }

    SECTION("executeShortcut returns CommandNotFound if command not registered") {
        KeyboardShortcut ctrlY('Y', true, false, false);
        manager.bindShortcut(ctrlY, "non.existent.command");

        CommandExecutionResult result = manager.executeShortcut(ctrlY);

        REQUIRE(result == CommandExecutionResult::CommandNotFound);
        REQUIRE(g_testExecutionCount == 0);
    }

    manager.clear();
    registry.clear();
}

// =============================================================================
// Test: JSON Persistence
// =============================================================================

TEST_CASE("ShortcutManager JSON persistence", "[gui][shortcut][persistence]") {
    ShortcutManager& manager = ShortcutManager::getInstance();
    manager.clear();

    const std::string testFile = "/tmp/test_shortcuts.json";

    // Clean up any existing test file
    deleteFile(testFile);

    SECTION("saveToFile creates JSON file") {
        KeyboardShortcut ctrlS('S', true, false, false);
        KeyboardShortcut ctrlO('O', true, false, false);

        manager.bindShortcut(ctrlS, "file.save");
        manager.bindShortcut(ctrlO, "file.open");

        bool result = manager.saveToFile(testFile);

        REQUIRE(result == true);
        REQUIRE(fileExists(testFile));

        deleteFile(testFile);
    }

    SECTION("loadFromFile restores bindings") {
        // Save
        KeyboardShortcut ctrlS('S', true, false, false);
        KeyboardShortcut ctrlO('O', true, false, false);

        manager.bindShortcut(ctrlS, "file.save");
        manager.bindShortcut(ctrlO, "file.open");
        manager.saveToFile(testFile);

        // Clear and load
        manager.clear();
        REQUIRE(manager.getBindingCount() == 0);

        bool result = manager.loadFromFile(testFile);

        REQUIRE(result == true);
        REQUIRE(manager.getBindingCount() == 2);

        auto commandId1 = manager.getCommandForShortcut(ctrlS);
        auto commandId2 = manager.getCommandForShortcut(ctrlO);

        REQUIRE(commandId1.has_value());
        REQUIRE(commandId1.value() == "file.save");
        REQUIRE(commandId2.has_value());
        REQUIRE(commandId2.value() == "file.open");

        deleteFile(testFile);
    }

    SECTION("loadFromFile returns false for non-existent file") {
        bool result = manager.loadFromFile("/tmp/non_existent_file.json");

        REQUIRE(result == false);
    }

    SECTION("save/load round-trip preserves all bindings") {
        // Create multiple bindings
        manager.bindShortcut(KeyboardShortcut('S', true, false, false), "file.save");
        manager.bindShortcut(KeyboardShortcut('O', true, false, false), "file.open");
        manager.bindShortcut(KeyboardShortcut('N', true, false, false), "file.new");
        manager.bindShortcut(KeyboardShortcut(WXK_F5, false, false, false), "view.refresh");

        size_t originalCount = manager.getBindingCount();

        // Save
        manager.saveToFile(testFile);

        // Clear and load
        manager.clear();
        manager.loadFromFile(testFile);

        // Verify
        REQUIRE(manager.getBindingCount() == originalCount);

        deleteFile(testFile);
    }

    manager.clear();
}

// =============================================================================
// Test: Utility
// =============================================================================

TEST_CASE("ShortcutManager utility", "[gui][shortcut][utility]") {
    ShortcutManager& manager = ShortcutManager::getInstance();
    manager.clear();

    SECTION("clear removes all bindings") {
        manager.bindShortcut(KeyboardShortcut('S', true, false, false), "file.save");
        manager.bindShortcut(KeyboardShortcut('O', true, false, false), "file.open");
        manager.bindShortcut(KeyboardShortcut('N', true, false, false), "file.new");

        REQUIRE(manager.getBindingCount() == 3);

        manager.clear();

        REQUIRE(manager.getBindingCount() == 0);
        REQUIRE_FALSE(manager.isShortcutBound(KeyboardShortcut('S', true, false, false)));
    }

    manager.clear();
}
