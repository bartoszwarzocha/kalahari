/// @file command_registry.cpp
/// @brief CommandRegistry implementation

#include "kalahari/gui/command_registry.h"
#include <algorithm>
#include <set>

namespace kalahari {
namespace gui {

// ============================================================================
// Singleton
// ============================================================================

CommandRegistry& CommandRegistry::getInstance() {
    // Meyers singleton - thread-safe in C++11+
    static CommandRegistry instance;
    return instance;
}

// ============================================================================
// Registration
// ============================================================================

void CommandRegistry::registerCommand(const Command& command) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Override if exists (allows updating commands)
    m_commands[command.id] = command;
}

void CommandRegistry::unregisterCommand(const std::string& commandId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Safe to call even if command doesn't exist
    m_commands.erase(commandId);
}

bool CommandRegistry::isCommandRegistered(const std::string& commandId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_commands.find(commandId) != m_commands.end();
}

// ============================================================================
// Query
// ============================================================================

const Command* CommandRegistry::getCommand(const std::string& commandId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_commands.find(commandId);
    return (it != m_commands.end()) ? &it->second : nullptr;
}

Command* CommandRegistry::getCommand(const std::string& commandId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_commands.find(commandId);
    return (it != m_commands.end()) ? &it->second : nullptr;
}

std::vector<Command> CommandRegistry::getCommandsByCategory(const std::string& category) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Command> result;

    for (const auto& pair : m_commands) {
        if (pair.second.category == category) {
            result.push_back(pair.second);
        }
    }

    return result;
}

std::vector<Command> CommandRegistry::getAllCommands() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Command> result;
    result.reserve(m_commands.size());

    for (const auto& pair : m_commands) {
        result.push_back(pair.second);
    }

    return result;
}

std::vector<std::string> CommandRegistry::getCategories() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Use set for automatic sorting and uniqueness
    std::set<std::string> categories;

    for (const auto& pair : m_commands) {
        if (!pair.second.category.empty()) {
            categories.insert(pair.second.category);
        }
    }

    // Convert to vector
    return std::vector<std::string>(categories.begin(), categories.end());
}

// ============================================================================
// Execution
// ============================================================================

CommandExecutionResult CommandRegistry::executeCommand(const std::string& commandId) {
    // Copy command and error handler under lock, then execute outside lock
    // This avoids holding lock while calling external code (prevents deadlocks)
    Command cmdCopy;
    CommandErrorHandler errorHandler;
    bool commandFound = false;
    bool canExec = false;
    bool isEnabled = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_commands.find(commandId);
        if (it != m_commands.end()) {
            commandFound = true;
            cmdCopy = it->second;
            canExec = cmdCopy.canExecute();
            isEnabled = cmdCopy.checkEnabled();
        }
        errorHandler = m_errorHandler;
    }

    // 1. Check if command exists
    if (!commandFound) {
        if (errorHandler) {
            errorHandler(commandId, "Command not found");
        }
        return CommandExecutionResult::CommandNotFound;
    }

    // 2. Check if command has execute callback
    if (!canExec) {
        if (errorHandler) {
            errorHandler(commandId, "Command has no execute callback");
        }
        return CommandExecutionResult::NoExecuteCallback;
    }

    // 3. Check if command is enabled
    if (!isEnabled) {
        if (errorHandler) {
            errorHandler(commandId, "Command is disabled");
        }
        return CommandExecutionResult::CommandDisabled;
    }

    // 4. Execute with exception handling (outside lock)
    try {
        cmdCopy.execute();
        return CommandExecutionResult::Success;
    } catch (const std::exception& e) {
        if (errorHandler) {
            errorHandler(commandId, std::string("Execution failed: ") + e.what());
        }
        return CommandExecutionResult::ExecutionFailed;
    } catch (...) {
        if (errorHandler) {
            errorHandler(commandId, "Execution failed: Unknown exception");
        }
        return CommandExecutionResult::ExecutionFailed;
    }
}

bool CommandRegistry::canExecute(const std::string& commandId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_commands.find(commandId);
    if (it == m_commands.end()) {
        return false;
    }
    return it->second.canExecute() && it->second.checkEnabled();
}

bool CommandRegistry::isChecked(const std::string& commandId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_commands.find(commandId);
    if (it == m_commands.end()) {
        return false;
    }
    return it->second.checkChecked();
}

// ============================================================================
// Error Handling
// ============================================================================

void CommandRegistry::setErrorHandler(CommandErrorHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorHandler = handler;
}

CommandErrorHandler CommandRegistry::getErrorHandler() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_errorHandler;
}

// ============================================================================
// Utility
// ============================================================================

size_t CommandRegistry::getCommandCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_commands.size();
}

void CommandRegistry::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_commands.clear();
}

} // namespace gui
} // namespace kalahari
