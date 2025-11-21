/// @file toolbar_builder.h
/// @brief ToolbarBuilder class - dynamically builds QToolBar from CommandRegistry (Qt6)
///
/// @author Claude (AI Assistant)
/// @date 2025-11-21 (Qt6 migration)
/// @task #00047 - Command Registry Qt Migration

#pragma once

#include "command_registry.h"
#include <QToolBar>
#include <QMainWindow>
#include <QWidget>
#include <string>
#include <vector>

namespace kalahari {
namespace gui {

/// @brief Dynamically builds QToolBar from CommandRegistry
///
/// ToolbarBuilder provides abstraction layer between CommandRegistry (single source of truth
/// for commands) and Qt toolbar system. Instead of hardcoding toolbar creation,
/// ToolbarBuilder queries CommandRegistry and generates toolbar dynamically.
///
/// Key benefits:
/// - Single source of truth (CommandRegistry)
/// - Automatic icon/tooltip/label synchronization
/// - Plugin-friendly (plugins register commands → toolbar updates automatically)
/// - Maintainable (change command → toolbar updates automatically)
///
/// Example usage:
/// @code
/// ToolbarBuilder builder;
/// CommandRegistry& registry = CommandRegistry::getInstance();
/// QToolBar* toolbar = builder.buildToolBar(registry, this);
/// addToolBar(toolbar);
/// @endcode
///
/// @note ToolbarBuilder does NOT own created toolbar. Parent window manages lifetime.
class ToolbarBuilder {
public:
    /// @brief Default constructor
    ToolbarBuilder() = default;

    /// @brief Destructor
    ~ToolbarBuilder() = default;

    // Disable copy and move (stateless class, not needed)
    ToolbarBuilder(const ToolbarBuilder&) = delete;
    ToolbarBuilder& operator=(const ToolbarBuilder&) = delete;
    ToolbarBuilder(ToolbarBuilder&&) = delete;
    ToolbarBuilder& operator=(ToolbarBuilder&&) = delete;

    /// @brief Build complete toolbar from CommandRegistry
    ///
    /// Creates QToolBar with actions from commands marked showInToolbar=true.
    /// Toolbar structure:
    /// - File actions (New, Open, Save)
    /// - Separator
    /// - Edit actions (Undo, Redo, Cut, Copy, Paste)
    /// - Separator
    /// - Format actions (Bold, Italic, Underline)
    ///
    /// Action order is explicit (not dependent on unordered_map iteration).
    /// QAction signals are automatically connected to CommandRegistry::executeCommand().
    ///
    /// @param registry CommandRegistry to read commands from
    /// @param parent Parent window for toolbar ownership (typically MainWindow)
    /// @return Fully constructed QToolBar (parent owns)
    /// @throws std::runtime_error if CommandRegistry is empty or invalid
    QToolBar* buildToolBar(CommandRegistry& registry, QMainWindow* parent);

    /// @brief Add actions from category to existing toolbar
    ///
    /// Queries CommandRegistry for all commands in given category where
    /// showInToolbar=true and creates toolbar actions for each.
    ///
    /// @param toolbar Target toolbar (must not be null)
    /// @param registry CommandRegistry to query
    /// @param category Action category (e.g., "File", "Edit", "Format")
    /// @note Does nothing if category has no commands with showInToolbar=true
    void addActionsFromCategory(QToolBar* toolbar,
                                CommandRegistry& registry,
                                const std::string& category);

    /// @brief Add separator to toolbar
    ///
    /// Simple wrapper around QToolBar::addSeparator() for consistency.
    ///
    /// @param toolbar Target toolbar (must not be null)
    void addSeparator(QToolBar* toolbar);

private:
    /// @brief Create toolbar action from Command descriptor
    ///
    /// Converts Command struct to QAction with:
    /// - Label (from command.label)
    /// - Icon (from command.icons via toQIcon())
    /// - Tooltip (from command.tooltip)
    /// - Signal connected to CommandRegistry::executeCommand()
    ///
    /// @param toolbar Target toolbar
    /// @param command Command descriptor from CommandRegistry
    /// @param registry CommandRegistry reference for signal connection
    void createToolAction(QToolBar* toolbar,
                          const Command& command,
                          CommandRegistry& registry);
};

} // namespace gui
} // namespace kalahari
