/// @file menu_builder.cpp
/// @brief Implementation of MenuBuilder class
///
/// @author Claude (AI Assistant)
/// @date 2025-11-13
/// @task #00031 - MenuBuilder Class (Zagadnienie 1.2)

#include "kalahari/gui/menu_builder.h"
#include "kalahari/core/logger.h"
#include <wx/artprov.h>
#include <stdexcept>

namespace kalahari {
namespace gui {

// ============================================================================
// Public Methods
// ============================================================================

wxMenuBar* MenuBuilder::buildMenuBar(CommandRegistry& registry, wxWindow* window) {
    core::Logger::getInstance().debug("MenuBuilder: Building menubar from CommandRegistry with event binding");

    if (!window) {
        core::Logger::getInstance().error("MenuBuilder: Cannot build menubar - window pointer is null");
        throw std::runtime_error("MenuBuilder::buildMenuBar: window parameter cannot be null");
    }

    wxMenuBar* menuBar = new wxMenuBar();

    // Define menu structure with explicit order
    // (CommandRegistry uses unordered_map, so order must be explicit)
    std::vector<std::pair<std::string, std::string>> menuStructure = {
        {"file", "&File"},
        {"edit", "&Edit"},
        {"format", "F&ormat"},
        {"view", "&View"},
        {"help", "&Help"}
    };

    // Build each menu and append to menubar
    for (const auto& [category, label] : menuStructure) {
        wxMenu* menu = buildMenu(registry, category, label, window);
        if (menu) {
            menuBar->Append(menu, label);
            core::Logger::getInstance().debug("MenuBuilder: Added menu '{}' with {} items",
                                             category, menu->GetMenuItemCount());
        } else {
            core::Logger::getInstance().warn("MenuBuilder: Failed to build menu '{}'", category);
        }
    }

    int menuCount = menuBar->GetMenuCount();
    core::Logger::getInstance().info("MenuBuilder: Menubar created with {} menus, events bound to window",
                                    menuCount);

    return menuBar;
}

wxMenu* MenuBuilder::buildMenu(CommandRegistry& registry,
                                const std::string& category,
                                const std::string& menuLabel,
                                wxWindow* window) {
    core::Logger::getInstance().debug("MenuBuilder: Building menu '{}' (label: '{}') with event binding",
                                     category, menuLabel);

    if (!window) {
        core::Logger::getInstance().error("MenuBuilder: Cannot build menu - window pointer is null");
        return nullptr;
    }

    wxMenu* menu = new wxMenu();

    // Query CommandRegistry for all commands in this category
    std::vector<Command> commands = registry.getCommandsByCategory(category);

    if (commands.empty()) {
        core::Logger::getInstance().debug("MenuBuilder: Category '{}' has no commands (empty menu)",
                                         category);
        // Return empty menu (not an error - some categories might be empty)
        return menu;
    }

    // Add each command as menu item
    for (const auto& command : commands) {
        // Skip commands marked as not showing in menu
        if (!command.showInMenu) {
            core::Logger::getInstance().debug("MenuBuilder: Skipping command '{}' (showInMenu=false)",
                                             command.id);
            continue;
        }

        wxMenuItem* item = createMenuItem(command, menu, window);
        if (item) {
            menu->Append(item);
            core::Logger::getInstance().debug("MenuBuilder: Added command '{}' to menu '{}' (event bound)",
                                             command.id, category);
        }
    }

    return menu;
}

void MenuBuilder::addSeparator(wxMenu* menu) {
    if (!menu) {
        core::Logger::getInstance().error("MenuBuilder: Cannot add separator to null menu");
        return;
    }
    menu->AppendSeparator();
}

void MenuBuilder::addSubmenu(wxMenu* parent, wxMenu* submenu,
                              const std::string& label,
                              const std::string& help) {
    if (!parent) {
        core::Logger::getInstance().error("MenuBuilder: Cannot add submenu - parent menu is null");
        return;
    }
    if (!submenu) {
        core::Logger::getInstance().error("MenuBuilder: Cannot add submenu - submenu is null");
        return;
    }

    // wxWidgets takes ownership of submenu when appending
    parent->AppendSubMenu(submenu, label, help);
    core::Logger::getInstance().debug("MenuBuilder: Added submenu '{}' to parent menu", label);
}

// ============================================================================
// Private Helper Methods
// ============================================================================

wxMenuItem* MenuBuilder::createMenuItem(const Command& command, wxMenu* parent, wxWindow* window) {
    if (!parent) {
        core::Logger::getInstance().error("MenuBuilder: Cannot create menu item - parent menu is null");
        return nullptr;
    }
    if (!window) {
        core::Logger::getInstance().error("MenuBuilder: Cannot create menu item - window pointer is null");
        return nullptr;
    }

    // Format label with keyboard shortcut (if present)
    std::string label = formatLabel(command.label, command.shortcut);

    // Generate unique ID for this menu item
    int itemId = wxWindow::NewControlId();

    // Create menu item with unique ID
    wxMenuItem* item = new wxMenuItem(parent, itemId, label, command.tooltip);

    // Set icon if present
    wxBitmapBundle icon = getIcon(command);
    if (icon.IsOk()) {
        item->SetBitmap(icon);
    }

    // Bind event handler to CommandRegistry::executeCommand()
    // Use lambda to capture command ID and execute through registry
    std::string commandId = command.id;  // Copy for lambda capture
    window->Bind(wxEVT_MENU, [commandId](wxCommandEvent&) {
        CommandRegistry::getInstance().executeCommand(commandId);
    }, itemId);

    core::Logger::getInstance().debug("MenuBuilder: Menu item '{}' created with ID {} (event bound to '{}')",
                                     command.label, itemId, commandId);

    return item;
}

wxBitmapBundle MenuBuilder::getIcon(const Command& command) {
    // Priority 1: Use icon from Command.icons (IconSet)
    if (!command.icons.isEmpty() && command.icons.icon16.IsOk()) {
        // Menu icons are typically 16x16
        return wxBitmapBundle::FromBitmap(command.icons.icon16);
    }

    // Priority 2: No icon - return empty bundle
    // (wxArtProvider fallback would be here if Command had artId hint)
    return wxBitmapBundle();
}

std::string MenuBuilder::formatLabel(const std::string& label,
                                      const KeyboardShortcut& shortcut) const {
    // If no shortcut, return label as-is
    if (shortcut.isEmpty()) {
        return label;
    }

    // Format: "Label\tShortcut" (wxWidgets convention)
    // Example: "Save\tCtrl+S"
    wxString shortcutStr = shortcut.toString();
    std::string result = label + "\t" + shortcutStr.ToStdString();

    return result;
}

} // namespace gui
} // namespace kalahari
