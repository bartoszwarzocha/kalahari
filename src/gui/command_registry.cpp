/// @file command_registry.cpp
/// @brief CommandRegistry implementation

#include "kalahari/gui/command_registry.h"
#include "kalahari/core/art_provider.h"
#include <QAction>
#include <QString>
#include <algorithm>
#include <set>

namespace kalahari {
namespace gui {

// ============================================================================
// Singleton
// ============================================================================

CommandRegistry::CommandRegistry() : QObject(nullptr) {
    // Parent is nullptr - singleton lifetime managed by static storage
}

CommandRegistry::~CommandRegistry() {
    // Clean up all owned QAction instances
    qDeleteAll(m_actions);
    m_actions.clear();
}

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

void CommandRegistry::registerCommand(Command&& command) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Override if exists (allows updating commands)
    // Move command to avoid unnecessary copy
    std::string cmdId = command.id;  // Copy ID before moving
    m_commands[cmdId] = std::move(command);
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
    // Clean up actions first
    qDeleteAll(m_actions);
    m_actions.clear();
    m_commands.clear();
}

// ============================================================================
// QAction Management (OpenSpec #00040 - Phase 1)
// ============================================================================

QAction* CommandRegistry::getAction(const QString& commandId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if action already exists in cache
    auto actionIt = m_actions.find(commandId);
    if (actionIt != m_actions.end()) {
        return actionIt.value();
    }

    // Find the command
    std::string cmdIdStd = commandId.toStdString();
    auto cmdIt = m_commands.find(cmdIdStd);
    if (cmdIt == m_commands.end()) {
        return nullptr;  // Command not registered
    }

    // Create new action for this command
    QAction* action = createActionForCommand(commandId, cmdIt->second);
    m_actions.insert(commandId, action);
    return action;
}

QAction* CommandRegistry::getAction(const std::string& commandId) {
    return getAction(QString::fromStdString(commandId));
}

QAction* CommandRegistry::createActionForCommand(const QString& commandId, const Command& cmd) {
    // Use ArtProvider to create self-updating action with icon
    auto& artProvider = core::ArtProvider::getInstance();
    QAction* action = artProvider.createAction(
        commandId,
        QString::fromStdString(cmd.label),
        this,  // CommandRegistry owns the action
        core::IconContext::Toolbar  // Default context, menu/toolbar will use appropriate size
    );

    // Configure shortcut
    if (!cmd.shortcut.isEmpty()) {
        action->setShortcut(cmd.shortcut.toQKeySequence());
    }

    // Configure tooltip
    if (!cmd.tooltip.empty()) {
        action->setToolTip(QString::fromStdString(cmd.tooltip));
    } else if (!cmd.label.empty()) {
        // Fallback: use label as tooltip
        action->setToolTip(QString::fromStdString(cmd.label));
    }

    // Configure checkable state
    if (cmd.isChecked) {
        action->setCheckable(true);
        action->setChecked(cmd.checkChecked());
    }

    // Configure enabled state
    action->setEnabled(cmd.checkEnabled());

    // Store command ID in action's data for later retrieval
    action->setData(commandId);

    // Connect triggered signal to executeCommand
    // Capture commandId by value (QString copy)
    connect(action, &QAction::triggered, this, [this, cmdId = commandId.toStdString()](bool /*checked*/) {
        executeCommand(cmdId);
    });

    return action;
}

QList<QAction*> CommandRegistry::getAllActions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_actions.values();
}

QList<QAction*> CommandRegistry::getActionsByCategory(const std::string& category) {
    QList<QAction*> result;

    // Get commands in category (need to release lock before calling getAction)
    std::vector<std::string> commandIds;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& pair : m_commands) {
            if (pair.second.category == category) {
                commandIds.push_back(pair.first);
            }
        }
    }

    // Get/create actions for each command
    for (const auto& cmdId : commandIds) {
        QAction* action = getAction(cmdId);
        if (action) {
            result.append(action);
        }
    }

    return result;
}

void CommandRegistry::updateActionState(const std::string& commandId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    QString qCmdId = QString::fromStdString(commandId);
    auto actionIt = m_actions.find(qCmdId);
    if (actionIt == m_actions.end()) {
        return;  // Action not yet created
    }

    auto cmdIt = m_commands.find(commandId);
    if (cmdIt == m_commands.end()) {
        return;  // Command not registered
    }

    QAction* action = actionIt.value();
    Command& cmd = cmdIt->second;

    // Update enabled state
    action->setEnabled(cmd.checkEnabled());

    // Update checked state - make checkable if isChecked callback was added later
    if (cmd.isChecked) {
        if (!action->isCheckable()) {
            action->setCheckable(true);
        }
        action->setChecked(cmd.checkChecked());
    }
}

void CommandRegistry::updateAllActionStates() {
    // Get list of action keys (need to release lock before calling updateActionState)
    std::vector<std::string> commandIds;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_actions.constBegin(); it != m_actions.constEnd(); ++it) {
            commandIds.push_back(it.key().toStdString());
        }
    }

    // Update each action
    for (const auto& cmdId : commandIds) {
        updateActionState(cmdId);
    }
}

} // namespace gui
} // namespace kalahari
