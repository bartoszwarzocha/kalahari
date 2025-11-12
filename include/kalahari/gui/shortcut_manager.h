/// @file shortcut_manager.h
/// @brief Keyboard shortcut management system
///
/// ShortcutManager provides:
/// - Keyboard shortcut binding to commands
/// - Shortcut conflict detection (exact match)
/// - Integration with CommandRegistry for execution
/// - JSON persistence (save/load shortcuts)
///
/// Example usage:
/// @code
/// // Bind Ctrl+S to save command
/// KeyboardShortcut ctrlS('S', true, false, false);
/// ShortcutManager::getInstance().bindShortcut(ctrlS, "file.save");
///
/// // Execute shortcut
/// ShortcutManager::getInstance().executeShortcut(ctrlS);
///
/// // Save shortcuts to file
/// ShortcutManager::getInstance().saveToFile("shortcuts.json");
/// @endcode

#pragma once

#include "kalahari/gui/command.h"
#include "kalahari/gui/command_registry.h"
#include <map>
#include <string>
#include <optional>

namespace kalahari {
namespace gui {

// ============================================================================
// ShortcutManager - Keyboard Shortcut Management (Singleton)
// ============================================================================

/// @brief Central keyboard shortcut manager
///
/// Singleton managing keyboard shortcut bindings to commands.
/// Uses Meyers singleton pattern for thread-safe initialization (C++11+).
///
/// Architecture:
/// - Storage: std::map<KeyboardShortcut, std::string> (shortcut → command ID)
/// - Conflict detection: Exact match (one shortcut = one command)
/// - Execution: Delegates to CommandRegistry::executeCommand()
/// - Persistence: JSON format (~/.kalahari/shortcuts.json)
///
/// Thread-safety:
/// - Singleton initialization is thread-safe (C++11 guarantee)
/// - Binding/unbinding should happen in main thread
/// - Execution can happen from any thread (delegates to CommandRegistry)
class ShortcutManager {
public:
    /// @brief Get singleton instance (thread-safe, C++11+)
    /// @return Reference to the single ShortcutManager instance
    static ShortcutManager& getInstance();

    // ========================================================================
    // Binding (called at startup or from settings dialog)
    // ========================================================================

    /// @brief Bind keyboard shortcut to command
    /// @param shortcut Keyboard shortcut to bind
    /// @param commandId Command ID to execute when shortcut is pressed
    /// @return true if binding succeeded, false if shortcut is empty
    /// @note If shortcut already bound, it will be overridden (no conflict warning)
    bool bindShortcut(const KeyboardShortcut& shortcut, const std::string& commandId);

    /// @brief Unbind keyboard shortcut
    /// @param shortcut Keyboard shortcut to unbind
    /// @note If shortcut not bound, this is a no-op (safe to call)
    void unbindShortcut(const KeyboardShortcut& shortcut);

    /// @brief Check if keyboard shortcut is bound
    /// @param shortcut Keyboard shortcut to check
    /// @return true if shortcut is bound to a command
    bool isShortcutBound(const KeyboardShortcut& shortcut) const;

    // ========================================================================
    // Query
    // ========================================================================

    /// @brief Get command ID for keyboard shortcut
    /// @param shortcut Keyboard shortcut to query
    /// @return Command ID if bound, std::nullopt if not bound
    std::optional<std::string> getCommandForShortcut(const KeyboardShortcut& shortcut) const;

    /// @brief Get all shortcut bindings
    /// @return Map of all shortcut bindings (shortcut → command ID)
    /// @note Returns copy for safety
    std::map<KeyboardShortcut, std::string> getAllBindings() const;

    /// @brief Get number of bound shortcuts
    /// @return Binding count
    size_t getBindingCount() const;

    // ========================================================================
    // Execution (called by main window keyboard event handler)
    // ========================================================================

    /// @brief Execute command bound to keyboard shortcut
    /// @param shortcut Keyboard shortcut pressed
    /// @return Execution result (Success, CommandNotFound, etc.)
    /// @note If shortcut not bound, returns CommandNotFound
    /// @note Delegates to CommandRegistry::executeCommand()
    CommandExecutionResult executeShortcut(const KeyboardShortcut& shortcut);

    // ========================================================================
    // Persistence (JSON format)
    // ========================================================================

    /// @brief Save shortcuts to JSON file
    /// @param filePath Path to JSON file
    /// @return true if save succeeded, false on error
    /// @note Format: {"shortcuts": [{"shortcut": "Ctrl+S", "commandId": "file.save"}, ...]}
    bool saveToFile(const std::string& filePath);

    /// @brief Load shortcuts from JSON file
    /// @param filePath Path to JSON file
    /// @return true if load succeeded, false on error (file not found, parse error)
    /// @note Clears existing bindings before loading
    bool loadFromFile(const std::string& filePath);

    // ========================================================================
    // Utility
    // ========================================================================

    /// @brief Clear all shortcut bindings
    /// @note Primarily for testing, not for production use
    void clear();

private:
    // Singleton pattern (Meyers)
    ShortcutManager() = default;
    ~ShortcutManager() = default;
    ShortcutManager(const ShortcutManager&) = delete;
    ShortcutManager& operator=(const ShortcutManager&) = delete;

    /// @brief Shortcut bindings (key = shortcut, value = command ID)
    std::map<KeyboardShortcut, std::string> m_bindings;
};

} // namespace gui
} // namespace kalahari
