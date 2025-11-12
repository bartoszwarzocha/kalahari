/// @file shortcut_manager.cpp
/// @brief ShortcutManager implementation

#include "kalahari/gui/shortcut_manager.h"
#include "kalahari/gui/command_registry.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace kalahari {
namespace gui {

// ============================================================================
// Singleton
// ============================================================================

ShortcutManager& ShortcutManager::getInstance() {
    // Meyers singleton - thread-safe in C++11+
    static ShortcutManager instance;
    return instance;
}

// ============================================================================
// Binding
// ============================================================================

bool ShortcutManager::bindShortcut(const KeyboardShortcut& shortcut, const std::string& commandId) {
    // Reject empty shortcuts
    if (shortcut.isEmpty()) {
        return false;
    }

    // Bind (override if exists)
    m_bindings[shortcut] = commandId;
    return true;
}

void ShortcutManager::unbindShortcut(const KeyboardShortcut& shortcut) {
    // Safe to call even if not bound
    m_bindings.erase(shortcut);
}

bool ShortcutManager::isShortcutBound(const KeyboardShortcut& shortcut) const {
    return m_bindings.find(shortcut) != m_bindings.end();
}

// ============================================================================
// Query
// ============================================================================

std::optional<std::string> ShortcutManager::getCommandForShortcut(const KeyboardShortcut& shortcut) const {
    auto it = m_bindings.find(shortcut);
    if (it != m_bindings.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::map<KeyboardShortcut, std::string> ShortcutManager::getAllBindings() const {
    return m_bindings; // Return copy
}

size_t ShortcutManager::getBindingCount() const {
    return m_bindings.size();
}

// ============================================================================
// Execution
// ============================================================================

CommandExecutionResult ShortcutManager::executeShortcut(const KeyboardShortcut& shortcut) {
    // Get command ID for shortcut
    auto commandId = getCommandForShortcut(shortcut);
    if (!commandId.has_value()) {
        return CommandExecutionResult::CommandNotFound;
    }

    // Execute command via CommandRegistry
    return CommandRegistry::getInstance().executeCommand(commandId.value());
}

// ============================================================================
// Persistence
// ============================================================================

bool ShortcutManager::saveToFile(const std::string& filePath) {
    try {
        // Create JSON array
        json j;
        json shortcuts = json::array();

        for (const auto& [shortcut, commandId] : m_bindings) {
            json binding;
            binding["shortcut"] = shortcut.toString().ToStdString();
            binding["commandId"] = commandId;
            shortcuts.push_back(binding);
        }

        j["shortcuts"] = shortcuts;

        // Write to file
        std::ofstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        file << j.dump(2); // Pretty print with 2-space indent
        return true;

    } catch (...) {
        return false;
    }
}

bool ShortcutManager::loadFromFile(const std::string& filePath) {
    try {
        // Read file
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        // Parse JSON
        json j;
        file >> j;

        // Validate structure
        if (!j.contains("shortcuts") || !j["shortcuts"].is_array()) {
            return false;
        }

        // Clear existing bindings
        m_bindings.clear();

        // Load bindings
        for (const auto& binding : j["shortcuts"]) {
            if (!binding.contains("shortcut") || !binding.contains("commandId")) {
                continue; // Skip invalid entries
            }

            std::string shortcutStr = binding["shortcut"];
            std::string commandId = binding["commandId"];

            // Parse shortcut
            KeyboardShortcut shortcut = KeyboardShortcut::fromString(wxString(shortcutStr));
            if (!shortcut.isEmpty()) {
                m_bindings[shortcut] = commandId;
            }
        }

        return true;

    } catch (...) {
        return false;
    }
}

// ============================================================================
// Utility
// ============================================================================

void ShortcutManager::clear() {
    m_bindings.clear();
}

} // namespace gui
} // namespace kalahari
