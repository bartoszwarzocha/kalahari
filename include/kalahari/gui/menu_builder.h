/// @file menu_builder.h
/// @brief MenuBuilder class - dynamically builds wxMenuBar from CommandRegistry
///
/// @author Claude (AI Assistant)
/// @date 2025-11-13
/// @task #00031 - MenuBuilder Class (Zagadnienie 1.2)

#pragma once

#include "command_registry.h"
#include <wx/menu.h>
#include <wx/menuitem.h>
#include <wx/bitmap.h>
#include <string>
#include <vector>
#include <utility>

namespace kalahari {
namespace gui {

/// @brief Dynamically builds wxMenuBar and wxMenu objects from CommandRegistry
///
/// MenuBuilder provides abstraction layer between CommandRegistry (single source of truth
/// for commands) and wxWidgets menu system. Instead of hardcoding menu creation,
/// MenuBuilder queries CommandRegistry and generates menus dynamically.
///
/// Key benefits:
/// - Single source of truth (CommandRegistry)
/// - Automatic icon/shortcut/label synchronization
/// - Plugin-friendly (plugins register commands → menus update automatically)
/// - Maintainable (change command → menu updates automatically)
///
/// Example usage:
/// @code
/// MenuBuilder builder;
/// CommandRegistry& registry = CommandRegistry::getInstance();
/// wxMenuBar* menuBar = builder.buildMenuBar(registry);
/// frame->SetMenuBar(menuBar);
/// @endcode
///
/// @note MenuBuilder does NOT own created menus. Caller must manage lifetime.
/// @note For wxWidgets, SetMenuBar() takes ownership of wxMenuBar.
class MenuBuilder {
public:
    /// @brief Default constructor
    MenuBuilder() = default;

    /// @brief Destructor
    ~MenuBuilder() = default;

    // Disable copy and move (stateless class, not needed)
    MenuBuilder(const MenuBuilder&) = delete;
    MenuBuilder& operator=(const MenuBuilder&) = delete;
    MenuBuilder(MenuBuilder&&) = delete;
    MenuBuilder& operator=(MenuBuilder&&) = delete;

    /// @brief Build complete menubar from CommandRegistry
    ///
    /// Creates wxMenuBar with standard menu structure:
    /// - File (category: "file")
    /// - Edit (category: "edit")
    /// - Format (category: "format")
    /// - View (category: "view")
    /// - Help (category: "help")
    ///
    /// Menu order is explicit (not dependent on unordered_map iteration).
    /// Event handlers are automatically bound to window for each menu item.
    ///
    /// @param registry CommandRegistry to read commands from
    /// @param window Target window for event binding (typically MainWindow)
    /// @return Fully constructed wxMenuBar (caller owns, but SetMenuBar takes ownership)
    /// @throws std::runtime_error if CommandRegistry is empty or invalid
    wxMenuBar* buildMenuBar(CommandRegistry& registry, wxWindow* window);

    /// @brief Build single menu from category
    ///
    /// Queries CommandRegistry for all commands in given category and creates
    /// wxMenu with menu items for each command.
    /// Event handlers are automatically bound to window for each menu item.
    ///
    /// @param registry CommandRegistry to query
    /// @param category Menu category (e.g., "file", "edit", "format")
    /// @param menuLabel Label for menu with mnemonic (e.g., "&File", "&Edit")
    /// @param window Target window for event binding (typically MainWindow)
    /// @return Constructed wxMenu (caller owns)
    /// @note Returns empty menu if category has no commands (not an error)
    wxMenu* buildMenu(CommandRegistry& registry,
                      const std::string& category,
                      const std::string& menuLabel,
                      wxWindow* window);

    /// @brief Add separator to menu
    ///
    /// Simple wrapper around wxMenu::AppendSeparator() for consistency.
    ///
    /// @param menu Target menu (must not be null)
    void addSeparator(wxMenu* menu);

    /// @brief Add submenu to menu
    ///
    /// Attaches submenu as child of parent menu. Parent menu takes ownership
    /// of submenu (wxWidgets manages lifetime).
    ///
    /// @param parent Parent menu (must not be null)
    /// @param submenu Child menu (must not be null, parent takes ownership)
    /// @param label Submenu label with mnemonic (e.g., "Editor &Mode")
    /// @param help Help text shown in status bar (optional)
    void addSubmenu(wxMenu* parent, wxMenu* submenu,
                    const std::string& label,
                    const std::string& help = "");

private:
    /// @brief Create wxMenuItem from Command descriptor
    ///
    /// Converts Command struct to wxMenuItem with:
    /// - Label with mnemonic (from command.label)
    /// - Keyboard shortcut (from command.shortcut, appended as \t)
    /// - Help text (from command.tooltip)
    /// - Icon bitmap (from command.icons via getIcon())
    /// - Event handler bound to CommandRegistry::executeCommand()
    ///
    /// @param command Command descriptor from CommandRegistry
    /// @param parent Parent menu (needed for wxMenuItem construction)
    /// @param window Target window for event binding
    /// @return Constructed wxMenuItem (parent menu owns after Append())
    wxMenuItem* createMenuItem(const Command& command, wxMenu* parent, wxWindow* window);

    /// @brief Get icon bitmap for command
    ///
    /// Retrieves icon from:
    /// 1. Command.icon (IconSet) if present
    /// 2. wxArtProvider fallback if command has artId hint
    /// 3. Returns empty wxBitmapBundle if no icon
    ///
    /// Icon size is always wxART_MENU (typically 16x16 on most platforms).
    ///
    /// @param command Command descriptor
    /// @return Icon bitmap bundle (may be empty if command has no icon)
    wxBitmapBundle getIcon(const Command& command);

    /// @brief Format menu item label with shortcut
    ///
    /// Combines label and keyboard shortcut into wxWidgets menu format:
    /// - "Label" → "Label" (no shortcut)
    /// - "Label" + "Ctrl+S" → "Label\tCtrl+S"
    ///
    /// @param label Menu item label (may contain mnemonic &)
    /// @param shortcut KeyboardShortcut (may be empty)
    /// @return Formatted label string for wxMenuItem
    std::string formatLabel(const std::string& label,
                           const KeyboardShortcut& shortcut) const;
};

} // namespace gui
} // namespace kalahari
