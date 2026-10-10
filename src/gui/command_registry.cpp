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
    registerCommand(Command(command));
}

void CommandRegistry::registerCommand(Command&& command) {
    std::vector<ShortcutChange> changes;
    bool othersChanged = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Override if exists (allows updating commands)
        // Move command to avoid unnecessary copy
        std::string cmdId = command.id;  // Copy ID before moving
        // The keys it comes with are the program's; the user's may take their place
        m_defaultShortcuts[cmdId] = command.shortcut;
        m_commands[cmdId] = std::move(command);
        changes = applyShortcuts(cmdId, &othersChanged);
    }
    finishShortcutChanges(changes, othersChanged);
}

void CommandRegistry::unregisterCommand(const std::string& commandId) {
    std::vector<ShortcutChange> changes;
    bool othersChanged = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Safe to call even if command doesn't exist
        m_commands.erase(commandId);
        // Its keys may go back to the command the program gave them
        if (m_defaultShortcuts.erase(commandId) > 0) {
            changes = applyShortcuts(commandId, &othersChanged);
        }
    }
    finishShortcutChanges(changes, othersChanged);
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
    m_defaultShortcuts.clear();
    m_customShortcuts.clear();
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
    // Labels like "Find & Replace..." hold a literal '&'; QAction treats '&' as a
    // mnemonic marker, so escape it (the tooltip fallback strips "&&" back to "&")
    auto& artProvider = core::ArtProvider::getInstance();
    QAction* action = artProvider.createAction(
        commandId,
        QString::fromStdString(cmd.label).replace('&', QStringLiteral("&&")),
        this,  // CommandRegistry owns the action
        core::IconContext::Toolbar  // Default context, menu/toolbar will use appropriate size
    );

    // Configure shortcut
    if (!cmd.shortcut.isEmpty()) {
        action->setShortcut(cmd.shortcut.toQKeySequence());
    }

    // Configure checkable state
    if (cmd.isChecked) {
        action->setCheckable(true);
        action->setChecked(cmd.checkChecked());
    }

    // Configure enabled state and tooltip
    applyActionAvailability(action, cmd);

    // NOTE: do NOT call action->setData(commandId) here. ArtProvider::createAction
    // has already stored a QVariantMap {"cmdId", "context"} in the action's data,
    // which ArtProvider::refreshAction reads to repaint the icon on resourcesChanged.
    // Overwriting it with a bare QString made refreshAction read an empty map and
    // bail out, so toolbar/menu icons never re-themed. The command id is available
    // to executeCommand via the lambda capture below, and to anyone else via
    // action->data().toMap().value("cmdId").

    // Connect triggered signal to executeCommand
    // Capture commandId by value (QString copy)
    connect(action, &QAction::triggered, this, [this, cmdId = commandId.toStdString()](bool /*checked*/) {
        executeCommand(cmdId);
    });

    return action;
}

void CommandRegistry::applyActionAvailability(QAction* action, const Command& cmd) const {
    // Fallback: use label as tooltip
    QString tooltip = QString::fromStdString(cmd.tooltip.empty() ? cmd.label : cmd.tooltip);

    if (!cmd.canExecute()) {
        tooltip = tr("%1 (not available yet)").arg(tooltip);
    } else if (tooltip == QString::fromStdString(cmd.label)) {
        // Menus show explicit tooltips, so do not repeat the label there;
        // toolbars still fall back to the action text
        tooltip.clear();
    }

    action->setToolTip(tooltip);
    action->setEnabled(cmd.canExecute() && cmd.checkEnabled());
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

    // Update enabled state and tooltip
    applyActionAvailability(action, cmd);

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

// ============================================================================
// Keyboard shortcuts
// ============================================================================

void CommandRegistry::setCustomShortcuts(const ShortcutMap& custom) {
    std::vector<ShortcutChange> changes;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_customShortcuts = custom;
        changes = applyShortcuts(std::string(), nullptr);
    }
    finishShortcutChanges(changes, true);
}

CommandRegistry::ShortcutMap CommandRegistry::customShortcuts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_customShortcuts;
}

KeyboardShortcut CommandRegistry::defaultShortcut(const std::string& commandId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_defaultShortcuts.find(commandId);
    return it != m_defaultShortcuts.end() ? it->second : KeyboardShortcut();
}

CommandRegistry::ShortcutMap CommandRegistry::defaultShortcuts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_defaultShortcuts;
}

CommandRegistry::ShortcutMap CommandRegistry::resolveShortcuts(const ShortcutMap& defaults,
                                                               const ShortcutMap& custom) {
    // The keys the user gave registered commands, and the first command by id with them
    std::map<KeyboardShortcut, std::string> userKeys;
    for (const auto& [id, keys] : custom) {
        if (!keys.isEmpty() && defaults.count(id) > 0) {
            userKeys.emplace(keys, id);
        }
    }

    ShortcutMap resolved;
    for (const auto& [id, programKeys] : defaults) {
        const auto own = custom.find(id);
        if (own != custom.end()) {
            const bool keeps = own->second.isEmpty() || userKeys.at(own->second) == id;
            resolved[id] = keeps ? own->second : KeyboardShortcut();
        } else {
            const bool taken = !programKeys.isEmpty() && userKeys.count(programKeys) > 0;
            resolved[id] = taken ? KeyboardShortcut() : programKeys;
        }
    }
    return resolved;
}

std::vector<CommandRegistry::ShortcutChange>
CommandRegistry::applyShortcuts(const std::string& skipId, bool* othersChanged) {
    std::vector<ShortcutChange> changes;
    const ShortcutMap resolved = resolveShortcuts(m_defaultShortcuts, m_customShortcuts);
    for (auto& [id, command] : m_commands) {
        const auto it = resolved.find(id);
        const KeyboardShortcut keys = it != resolved.end() ? it->second : KeyboardShortcut();
        if (command.shortcut == keys) {
            continue;
        }
        command.shortcut = keys;
        if (othersChanged != nullptr && id != skipId) {
            *othersChanged = true;
        }
        const auto action = m_actions.find(QString::fromStdString(id));
        if (action != m_actions.end()) {
            changes.emplace_back(action.value(), keys.toQKeySequence());
        }
    }
    return changes;
}

void CommandRegistry::finishShortcutChanges(const std::vector<ShortcutChange>& changes,
                                            bool notify) {
    // Outside the lock: an action's changed() may reach code that asks the registry
    for (const auto& [action, keys] : changes) {
        action->setShortcut(keys);
    }
    if (notify) {
        emit shortcutsChanged();
    }
}

} // namespace gui
} // namespace kalahari
