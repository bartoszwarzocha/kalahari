/// @file test_command_registry_execution.cpp
/// @brief Unit tests for CommandRegistry execution API (Task #00026)
///
/// Tests cover:
/// - executeCommand() with various scenarios
/// - canExecute() precondition checking
/// - isChecked() toggle state checking
/// - Error handler integration
/// - Exception handling during execution

#include <catch2/catch_test_macros.hpp>
#include "kalahari/gui/command_registry.h"

using namespace kalahari::gui;

// =============================================================================
// Helper Functions and State Tracking
// =============================================================================

/// @brief Global counter for command execution
static int g_executionCount = 0;

/// @brief Reset global counter
static void resetExecutionCount() {
    g_executionCount = 0;
}

/// @brief Sample command that increments counter
static void incrementCounter() {
    g_executionCount++;
}

/// @brief Command that throws std::exception
static void throwStdException() {
    throw std::runtime_error("Test exception");
}

/// @brief Command that throws unknown exception
static void throwUnknownException() {
    throw 42; // Non-std::exception
}

/// @brief Error handler state tracker
struct ErrorHandlerState {
    std::string lastCommandId;
    std::string lastErrorMessage;
    int callCount = 0;

    void reset() {
        lastCommandId.clear();
        lastErrorMessage.clear();
        callCount = 0;
    }

    void handleError(const std::string& cmdId, const std::string& msg) {
        lastCommandId = cmdId;
        lastErrorMessage = msg;
        callCount++;
    }
};

/// @brief Global error handler state
static ErrorHandlerState g_errorState;

// =============================================================================
// Test: Successful Execution
// =============================================================================

TEST_CASE("CommandRegistry executeCommand - success", "[gui][command][registry][execution]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();
    resetExecutionCount();

    Command cmd;
    cmd.id = "test.increment";
    cmd.execute = incrementCounter;
    registry.registerCommand(cmd);

    SECTION("executeCommand returns Success for valid command") {
        CommandExecutionResult result = registry.executeCommand("test.increment");

        REQUIRE(result == CommandExecutionResult::Success);
        REQUIRE(g_executionCount == 1);
    }

    SECTION("executeCommand can be called multiple times") {
        registry.executeCommand("test.increment");
        registry.executeCommand("test.increment");
        registry.executeCommand("test.increment");

        REQUIRE(g_executionCount == 3);
    }

    registry.clear();
}

// =============================================================================
// Test: Command Not Found
// =============================================================================

TEST_CASE("CommandRegistry executeCommand - not found", "[gui][command][registry][execution]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();

    SECTION("executeCommand returns CommandNotFound for non-existent command") {
        CommandExecutionResult result = registry.executeCommand("non.existent");

        REQUIRE(result == CommandExecutionResult::CommandNotFound);
    }

    SECTION("executeCommand returns CommandNotFound with empty registry") {
        CommandExecutionResult result = registry.executeCommand("any.command");

        REQUIRE(result == CommandExecutionResult::CommandNotFound);
    }

    registry.clear();
}

// =============================================================================
// Test: Command Disabled
// =============================================================================

TEST_CASE("CommandRegistry executeCommand - disabled", "[gui][command][registry][execution]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();
    resetExecutionCount();

    Command cmd;
    cmd.id = "test.disabled";
    cmd.execute = incrementCounter;
    cmd.isEnabled = []() { return false; }; // Always disabled
    registry.registerCommand(cmd);

    SECTION("executeCommand returns CommandDisabled when isEnabled returns false") {
        CommandExecutionResult result = registry.executeCommand("test.disabled");

        REQUIRE(result == CommandExecutionResult::CommandDisabled);
        REQUIRE(g_executionCount == 0); // Not executed
    }

    SECTION("executeCommand respects dynamic enable state") {
        bool enabled = false;
        Command dynCmd;
        dynCmd.id = "test.dynamic";
        dynCmd.execute = incrementCounter;
        dynCmd.isEnabled = [&enabled]() { return enabled; };
        registry.registerCommand(dynCmd);

        // Try when disabled
        CommandExecutionResult result1 = registry.executeCommand("test.dynamic");
        REQUIRE(result1 == CommandExecutionResult::CommandDisabled);
        REQUIRE(g_executionCount == 0);

        // Enable and try again
        enabled = true;
        CommandExecutionResult result2 = registry.executeCommand("test.dynamic");
        REQUIRE(result2 == CommandExecutionResult::Success);
        REQUIRE(g_executionCount == 1);
    }

    registry.clear();
}

// =============================================================================
// Test: No Execute Callback
// =============================================================================

TEST_CASE("CommandRegistry executeCommand - no callback", "[gui][command][registry][execution]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();

    Command cmd;
    cmd.id = "test.nocallback";
    // No execute callback set
    registry.registerCommand(cmd);

    SECTION("executeCommand returns NoExecuteCallback when execute is nullptr") {
        CommandExecutionResult result = registry.executeCommand("test.nocallback");

        REQUIRE(result == CommandExecutionResult::NoExecuteCallback);
    }

    registry.clear();
}

// =============================================================================
// Test: Exception Handling
// =============================================================================

TEST_CASE("CommandRegistry executeCommand - exceptions", "[gui][command][registry][execution]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();

    SECTION("executeCommand catches std::exception") {
        Command cmd;
        cmd.id = "test.throws.std";
        cmd.execute = throwStdException;
        registry.registerCommand(cmd);

        CommandExecutionResult result = registry.executeCommand("test.throws.std");

        REQUIRE(result == CommandExecutionResult::ExecutionFailed);
    }

    SECTION("executeCommand catches unknown exceptions") {
        Command cmd;
        cmd.id = "test.throws.unknown";
        cmd.execute = throwUnknownException;
        registry.registerCommand(cmd);

        CommandExecutionResult result = registry.executeCommand("test.throws.unknown");

        REQUIRE(result == CommandExecutionResult::ExecutionFailed);
    }

    registry.clear();
}

// =============================================================================
// Test: canExecute()
// =============================================================================

TEST_CASE("CommandRegistry canExecute", "[gui][command][registry][execution]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();

    SECTION("canExecute returns true for executable command") {
        Command cmd;
        cmd.id = "test.executable";
        cmd.execute = incrementCounter;
        registry.registerCommand(cmd);

        REQUIRE(registry.canExecute("test.executable"));
    }

    SECTION("canExecute returns false for non-existent command") {
        REQUIRE_FALSE(registry.canExecute("non.existent"));
    }

    SECTION("canExecute returns false when no execute callback") {
        Command cmd;
        cmd.id = "test.nocallback";
        registry.registerCommand(cmd);

        REQUIRE_FALSE(registry.canExecute("test.nocallback"));
    }

    SECTION("canExecute returns false when disabled") {
        Command cmd;
        cmd.id = "test.disabled";
        cmd.execute = incrementCounter;
        cmd.isEnabled = []() { return false; };
        registry.registerCommand(cmd);

        REQUIRE_FALSE(registry.canExecute("test.disabled"));
    }

    SECTION("canExecute respects all preconditions") {
        Command cmd;
        cmd.id = "test.full";
        cmd.execute = incrementCounter;
        cmd.isEnabled = []() { return true; };
        registry.registerCommand(cmd);

        REQUIRE(registry.canExecute("test.full"));
    }

    registry.clear();
}

// =============================================================================
// Test: isChecked()
// =============================================================================

TEST_CASE("CommandRegistry isChecked", "[gui][command][registry][execution]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();

    SECTION("isChecked returns true when isChecked callback returns true") {
        Command cmd;
        cmd.id = "test.checked";
        cmd.isChecked = []() { return true; };
        registry.registerCommand(cmd);

        REQUIRE(registry.isChecked("test.checked"));
    }

    SECTION("isChecked returns false when isChecked callback returns false") {
        Command cmd;
        cmd.id = "test.unchecked";
        cmd.isChecked = []() { return false; };
        registry.registerCommand(cmd);

        REQUIRE_FALSE(registry.isChecked("test.unchecked"));
    }

    SECTION("isChecked returns false for non-existent command") {
        REQUIRE_FALSE(registry.isChecked("non.existent"));
    }

    SECTION("isChecked returns false when no isChecked callback") {
        Command cmd;
        cmd.id = "test.nocallback";
        registry.registerCommand(cmd);

        REQUIRE_FALSE(registry.isChecked("test.nocallback"));
    }

    SECTION("isChecked respects dynamic state") {
        bool checked = false;
        Command cmd;
        cmd.id = "test.toggle";
        cmd.isChecked = [&checked]() { return checked; };
        registry.registerCommand(cmd);

        REQUIRE_FALSE(registry.isChecked("test.toggle"));

        checked = true;
        REQUIRE(registry.isChecked("test.toggle"));
    }

    registry.clear();
}

// =============================================================================
// Test: Error Handler Integration
// =============================================================================

TEST_CASE("CommandRegistry error handler", "[gui][command][registry][execution][error]") {
    CommandRegistry& registry = CommandRegistry::getInstance();
    registry.clear();
    g_errorState.reset();

    SECTION("setErrorHandler and getErrorHandler") {
        auto handler = [](const std::string&, const std::string&) {};
        registry.setErrorHandler(handler);

        auto retrieved = registry.getErrorHandler();
        REQUIRE(retrieved != nullptr);

        // Clear
        registry.setErrorHandler(nullptr);
        REQUIRE(registry.getErrorHandler() == nullptr);
    }

    SECTION("error handler called on CommandNotFound") {
        registry.setErrorHandler([](const std::string& id, const std::string& msg) {
            g_errorState.handleError(id, msg);
        });

        registry.executeCommand("non.existent");

        REQUIRE(g_errorState.callCount == 1);
        REQUIRE(g_errorState.lastCommandId == "non.existent");
        REQUIRE(g_errorState.lastErrorMessage == "Command not found");
    }

    SECTION("error handler called on CommandDisabled") {
        Command cmd;
        cmd.id = "test.disabled";
        cmd.execute = incrementCounter;
        cmd.isEnabled = []() { return false; };
        registry.registerCommand(cmd);

        registry.setErrorHandler([](const std::string& id, const std::string& msg) {
            g_errorState.handleError(id, msg);
        });

        registry.executeCommand("test.disabled");

        REQUIRE(g_errorState.callCount == 1);
        REQUIRE(g_errorState.lastCommandId == "test.disabled");
        REQUIRE(g_errorState.lastErrorMessage == "Command is disabled");
    }

    SECTION("error handler called on NoExecuteCallback") {
        Command cmd;
        cmd.id = "test.nocallback";
        registry.registerCommand(cmd);

        registry.setErrorHandler([](const std::string& id, const std::string& msg) {
            g_errorState.handleError(id, msg);
        });

        registry.executeCommand("test.nocallback");

        REQUIRE(g_errorState.callCount == 1);
        REQUIRE(g_errorState.lastCommandId == "test.nocallback");
        REQUIRE(g_errorState.lastErrorMessage == "Command has no execute callback");
    }

    SECTION("error handler called on ExecutionFailed (std::exception)") {
        Command cmd;
        cmd.id = "test.throws";
        cmd.execute = throwStdException;
        registry.registerCommand(cmd);

        registry.setErrorHandler([](const std::string& id, const std::string& msg) {
            g_errorState.handleError(id, msg);
        });

        registry.executeCommand("test.throws");

        REQUIRE(g_errorState.callCount == 1);
        REQUIRE(g_errorState.lastCommandId == "test.throws");
        REQUIRE(g_errorState.lastErrorMessage.find("Execution failed") != std::string::npos);
        REQUIRE(g_errorState.lastErrorMessage.find("Test exception") != std::string::npos);
    }

    SECTION("error handler NOT called on success") {
        Command cmd;
        cmd.id = "test.success";
        cmd.execute = incrementCounter;
        registry.registerCommand(cmd);

        registry.setErrorHandler([](const std::string& id, const std::string& msg) {
            g_errorState.handleError(id, msg);
        });

        registry.executeCommand("test.success");

        REQUIRE(g_errorState.callCount == 0);
    }

    registry.clear();
    registry.setErrorHandler(nullptr);
}
