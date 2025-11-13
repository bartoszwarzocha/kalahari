/// @file toolbar_builder.h
/// @brief ToolbarBuilder class - dynamically builds wxToolBar from CommandRegistry
///
/// @author Claude (AI Assistant)
/// @date 2025-11-13
/// @task #00032 - ToolbarBuilder Class (Zagadnienie 1.2)

#pragma once

#include "command_registry.h"
#include <wx/toolbar.h>
#include <wx/frame.h>
#include <wx/bitmap.h>
#include <wx/window.h>
#include <string>
#include <vector>

namespace kalahari {
namespace gui {

/// @brief Dynamically builds wxToolBar from CommandRegistry
///
/// ToolbarBuilder provides abstraction layer between CommandRegistry (single source of truth
/// for commands) and wxWidgets toolbar system. Instead of hardcoding toolbar creation,
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
/// wxToolBar* toolbar = builder.buildToolBar(registry, this, this);
/// SetToolBar(toolbar);
/// @endcode
///
/// @note ToolbarBuilder does NOT own created toolbar. Parent frame manages lifetime.
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
    /// Creates wxToolBar with tools from commands marked showInToolbar=true.
    /// Toolbar structure:
    /// - File tools (New, Open, Save)
    /// - Separator
    /// - Edit tools (Undo, Redo, Cut, Copy, Paste)
    /// - Separator
    /// - Format tools (Bold, Italic, Underline)
    ///
    /// Tool order is explicit (not dependent on unordered_map iteration).
    /// Event handlers are automatically bound to window for each tool.
    ///
    /// @param registry CommandRegistry to read commands from
    /// @param window Target window for event binding (typically MainWindow)
    /// @param parent Parent frame for toolbar ownership
    /// @return Fully constructed wxToolBar (parent owns)
    /// @throws std::runtime_error if CommandRegistry is empty or invalid
    wxToolBar* buildToolBar(CommandRegistry& registry,
                            wxWindow* window,
                            wxFrame* parent);

    /// @brief Add tools from category to existing toolbar
    ///
    /// Queries CommandRegistry for all commands in given category where
    /// showInToolbar=true and creates toolbar tools for each.
    ///
    /// @param toolbar Target toolbar (must not be null)
    /// @param registry CommandRegistry to query
    /// @param category Tool category (e.g., "file", "edit", "format")
    /// @param window Target window for event binding
    /// @note Does nothing if category has no commands with showInToolbar=true
    void addToolsFromCategory(wxToolBar* toolbar,
                              CommandRegistry& registry,
                              const std::string& category,
                              wxWindow* window);

    /// @brief Add separator to toolbar
    ///
    /// Simple wrapper around wxToolBar::AddSeparator() for consistency.
    ///
    /// @param toolbar Target toolbar (must not be null)
    void addSeparator(wxToolBar* toolbar);

private:
    /// @brief Create toolbar tool from Command descriptor
    ///
    /// Converts Command struct to wxToolBar tool with:
    /// - Label (from command.label)
    /// - Icon bitmap (from command.icons.icon24 or icon32)
    /// - Tooltip (from command.tooltip)
    /// - Event handler bound to CommandRegistry::executeCommand()
    ///
    /// @param toolbar Target toolbar
    /// @param command Command descriptor from CommandRegistry
    /// @param window Target window for event binding
    /// @param iconSize Icon size for toolbar (typically 24x24 or 32x32)
    void createToolItem(wxToolBar* toolbar,
                        const Command& command,
                        wxWindow* window,
                        const wxSize& iconSize);

    /// @brief Get icon bitmap for toolbar tool
    ///
    /// Retrieves icon from:
    /// 1. Command.icons.icon24 if size <= 24
    /// 2. Command.icons.icon32 if size > 24
    /// 3. Returns empty wxBitmap if no icon
    ///
    /// Icon size is determined by IconRegistry.getSizes().toolbar.
    ///
    /// @param command Command descriptor
    /// @param size Icon size (typically 24 or 32)
    /// @return Icon bitmap (may be invalid if command has no icon)
    wxBitmap getIcon(const Command& command, int size);
};

} // namespace gui
} // namespace kalahari
