/// @file command_registry.h
/// @brief Central command registration and management system
///
/// CommandRegistry is the heart of Kalahari's command system. It provides:
/// - Centralized command registration (core + plugins)
/// - Command lookup by ID or category
/// - Unified execution path (menu, toolbar, keyboard shortcuts)
/// - Thread-safe singleton pattern (Meyers)
///
/// Example usage:
/// @code
/// // Register command
/// Command cmd;
/// cmd.id = "file.save";
/// cmd.label = "Save";
/// cmd.execute = []() { /* save logic */ };
/// CommandRegistry::getInstance().registerCommand(cmd);
///
/// // Execute command
/// if (auto* cmd = CommandRegistry::getInstance().getCommand("file.save")) {
///     if (cmd->canExecute() && cmd->checkEnabled()) {
///         cmd->execute();
///     }
/// }
/// @endcode

#pragma once

#include "kalahari/gui/command.h"
#include <unordered_map>
#include <vector>
#include <string>

namespace kalahari {
namespace gui {

// ============================================================================
// Command Execution Types
// ============================================================================

/// @brief Command execution result enumeration
enum class CommandExecutionResult {
    Success,           ///< Command executed successfully
    CommandNotFound,   ///< Command ID not registered
    CommandDisabled,   ///< Command exists but is disabled (isEnabled returned false)
    NoExecuteCallback, ///< Command has no execute callback
    ExecutionFailed    ///< Execution threw exception
};

/// @brief Error handler callback type
/// @param commandId Command ID that failed
/// @param errorMessage Error description
using CommandErrorHandler = std::function<void(const std::string& commandId, const std::string& errorMessage)>;

// ============================================================================
// CommandRegistry - Central Command Management (Singleton)
// ============================================================================

/// @brief Central command registry for unified command execution
///
/// Singleton managing all command registrations, lookups, and queries.
/// Uses Meyers singleton pattern for thread-safe initialization (C++11+).
///
/// Architecture:
/// - Storage: std::unordered_map<std::string, Command>
/// - Registration: Core commands at startup, plugins during initialization
/// - Execution: Menu/toolbar/keyboard all route through getCommand() + execute()
///
/// Thread-safety:
/// - Singleton initialization is thread-safe (C++11 guarantee)
/// - Command registration should happen in main thread at startup
/// - Command execution can happen from any thread (callbacks handle threading)
class CommandRegistry {
public:
    /// @brief Get singleton instance (thread-safe, C++11+)
    /// @return Reference to the single CommandRegistry instance
    static CommandRegistry& getInstance();

    // ========================================================================
    // Registration (called at startup or plugin load)
    // ========================================================================

    /// @brief Register command in registry
    /// @param command Command to register
    /// @note If command with same ID exists, it will be replaced (override)
    /// @note Thread-safety: Should be called from main thread only
    void registerCommand(const Command& command);

    /// @brief Unregister command from registry
    /// @param commandId Command ID to unregister
    /// @note If command doesn't exist, this is a no-op (safe to call)
    void unregisterCommand(const std::string& commandId);

    /// @brief Check if command is registered
    /// @param commandId Command ID to check
    /// @return true if command exists in registry
    bool isCommandRegistered(const std::string& commandId) const;

    // ========================================================================
    // Query (called by menu/toolbar builders, execution paths)
    // ========================================================================

    /// @brief Get command by ID (const version)
    /// @param commandId Command ID to retrieve
    /// @return Pointer to Command or nullptr if not found
    /// @note Returned pointer is valid until command is unregistered
    const Command* getCommand(const std::string& commandId) const;

    /// @brief Get command by ID (non-const version)
    /// @param commandId Command ID to retrieve
    /// @return Pointer to Command or nullptr if not found
    /// @note Use for modifying command state (e.g., updating callbacks)
    Command* getCommand(const std::string& commandId);

    /// @brief Get all commands in specific category
    /// @param category Category name ("File", "Edit", "View", etc.)
    /// @return Vector of commands matching category (may be empty)
    /// @note Returns copies (not references) for safety
    std::vector<Command> getCommandsByCategory(const std::string& category) const;

    /// @brief Get all registered commands
    /// @return Vector of all commands (may be empty)
    /// @note Returns copies (not references) for safety
    std::vector<Command> getAllCommands() const;

    /// @brief Get all unique category names
    /// @return Vector of category names (sorted alphabetically)
    /// @note Useful for building category-based menus
    std::vector<std::string> getCategories() const;

    // ========================================================================
    // Execution (called by menu/toolbar/keyboard handlers)
    // ========================================================================

    /// @brief Execute command by ID
    /// @param commandId Command ID to execute
    /// @return Execution result
    /// @note Checks: command exists, has execute callback, is enabled
    /// @note Catches exceptions and returns ExecutionFailed on error
    /// @note Calls error handler if set when execution fails
    CommandExecutionResult executeCommand(const std::string& commandId);

    /// @brief Check if command can be executed
    /// @param commandId Command ID to check
    /// @return true if command exists, has execute callback, and is enabled
    /// @note Does not execute, only checks preconditions
    bool canExecute(const std::string& commandId) const;

    /// @brief Check if command is checked (for toggle menu items)
    /// @param commandId Command ID to check
    /// @return true if command has isChecked callback that returns true
    /// @note Returns false if command not found or has no isChecked callback
    bool isChecked(const std::string& commandId) const;

    // ========================================================================
    // Error Handling
    // ========================================================================

    /// @brief Set custom error handler
    /// @param handler Error callback (nullptr to clear)
    /// @note Error handler is called on execution failures
    void setErrorHandler(CommandErrorHandler handler);

    /// @brief Get current error handler
    /// @return Current error handler or nullptr if not set
    CommandErrorHandler getErrorHandler() const;

    // ========================================================================
    // Utility
    // ========================================================================

    /// @brief Get number of registered commands
    /// @return Command count
    size_t getCommandCount() const;

    /// @brief Clear all registered commands
    /// @note Primarily for testing, not for production use
    void clear();

private:
    // Singleton pattern (Meyers)
    CommandRegistry() = default;
    ~CommandRegistry() = default;
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;

    /// @brief Command storage (key = command ID)
    std::unordered_map<std::string, Command> m_commands;

    /// @brief Custom error handler (nullptr if not set)
    CommandErrorHandler m_errorHandler = nullptr;
};

} // namespace gui
} // namespace kalahari
