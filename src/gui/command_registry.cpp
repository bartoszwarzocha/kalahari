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
    // Override if exists (allows updating commands)
    m_commands[command.id] = command;
}

void CommandRegistry::unregisterCommand(const std::string& commandId) {
    // Safe to call even if command doesn't exist
    m_commands.erase(commandId);
}

bool CommandRegistry::isCommandRegistered(const std::string& commandId) const {
    return m_commands.find(commandId) != m_commands.end();
}

// ============================================================================
// Query
// ============================================================================

const Command* CommandRegistry::getCommand(const std::string& commandId) const {
    auto it = m_commands.find(commandId);
    return (it != m_commands.end()) ? &it->second : nullptr;
}

Command* CommandRegistry::getCommand(const std::string& commandId) {
    auto it = m_commands.find(commandId);
    return (it != m_commands.end()) ? &it->second : nullptr;
}

std::vector<Command> CommandRegistry::getCommandsByCategory(const std::string& category) const {
    std::vector<Command> result;

    for (const auto& pair : m_commands) {
        if (pair.second.category == category) {
            result.push_back(pair.second);
        }
    }

    return result;
}

std::vector<Command> CommandRegistry::getAllCommands() const {
    std::vector<Command> result;
    result.reserve(m_commands.size());

    for (const auto& pair : m_commands) {
        result.push_back(pair.second);
    }

    return result;
}

std::vector<std::string> CommandRegistry::getCategories() const {
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
    // 1. Check if command exists
    auto it = m_commands.find(commandId);
    if (it == m_commands.end()) {
        if (m_errorHandler) {
            m_errorHandler(commandId, "Command not found");
        }
        return CommandExecutionResult::CommandNotFound;
    }

    Command& cmd = it->second;

    // 2. Check if command has execute callback
    if (!cmd.canExecute()) {
        if (m_errorHandler) {
            m_errorHandler(commandId, "Command has no execute callback");
        }
        return CommandExecutionResult::NoExecuteCallback;
    }

    // 3. Check if command is enabled
    if (!cmd.checkEnabled()) {
        if (m_errorHandler) {
            m_errorHandler(commandId, "Command is disabled");
        }
        return CommandExecutionResult::CommandDisabled;
    }

    // 4. Execute with exception handling
    try {
        cmd.execute();
        return CommandExecutionResult::Success;
    } catch (const std::exception& e) {
        if (m_errorHandler) {
            m_errorHandler(commandId, std::string("Execution failed: ") + e.what());
        }
        return CommandExecutionResult::ExecutionFailed;
    } catch (...) {
        if (m_errorHandler) {
            m_errorHandler(commandId, "Execution failed: Unknown exception");
        }
        return CommandExecutionResult::ExecutionFailed;
    }
}

bool CommandRegistry::canExecute(const std::string& commandId) const {
    const Command* cmd = getCommand(commandId);
    return cmd && cmd->canExecute() && cmd->checkEnabled();
}

bool CommandRegistry::isChecked(const std::string& commandId) const {
    const Command* cmd = getCommand(commandId);
    return cmd && cmd->checkChecked();
}

// ============================================================================
// Error Handling
// ============================================================================

void CommandRegistry::setErrorHandler(CommandErrorHandler handler) {
    m_errorHandler = handler;
}

CommandErrorHandler CommandRegistry::getErrorHandler() const {
    return m_errorHandler;
}

// ============================================================================
// Utility
// ============================================================================

size_t CommandRegistry::getCommandCount() const {
    return m_commands.size();
}

void CommandRegistry::clear() {
    m_commands.clear();
}

} // namespace gui
} // namespace kalahari
