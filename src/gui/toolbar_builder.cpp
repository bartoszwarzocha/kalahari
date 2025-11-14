/// @file toolbar_builder.cpp
/// @brief Implementation of ToolbarBuilder class
///
/// @author Claude (AI Assistant)
/// @date 2025-11-13
/// @task #00032 - ToolbarBuilder Class (Zagadnienie 1.2)

#include "kalahari/gui/toolbar_builder.h"
#include "kalahari/gui/icon_registry.h"
#include "kalahari/core/logger.h"
#include <wx/artprov.h>
#include <stdexcept>

namespace kalahari {
namespace gui {

// ============================================================================
// Public Methods
// ============================================================================

wxToolBar* ToolbarBuilder::buildToolBar(CommandRegistry& registry,
                                         wxWindow* window,
                                         wxFrame* parent) {
    core::Logger::getInstance().debug("ToolbarBuilder: Building toolbar from CommandRegistry with event binding");

    if (!window) {
        core::Logger::getInstance().error("ToolbarBuilder: Cannot build toolbar - window pointer is null");
        throw std::runtime_error("ToolbarBuilder::buildToolBar: window parameter cannot be null");
    }
    if (!parent) {
        core::Logger::getInstance().error("ToolbarBuilder: Cannot build toolbar - parent frame is null");
        throw std::runtime_error("ToolbarBuilder::buildToolBar: parent parameter cannot be null");
    }

    // Create toolbar with horizontal orientation and text labels
    wxToolBar* toolbar = parent->CreateToolBar(wxTB_HORIZONTAL | wxTB_TEXT);

    // Define toolbar structure with explicit category order
    // (CommandRegistry uses unordered_map, so order must be explicit)
    std::vector<std::string> toolbarOrder = {
        "file",    // New, Open, Save
        "edit",    // Undo, Redo, Cut, Copy, Paste
        "format"   // Bold, Italic, Underline
    };

    // Add tools for each category
    bool firstCategory = true;
    for (const auto& category : toolbarOrder) {
        // Add separator before each category (except first)
        if (!firstCategory) {
            addSeparator(toolbar);
        }
        firstCategory = false;

        // Add all tools from this category
        addToolsFromCategory(toolbar, registry, category, window);
    }

    // Realize toolbar (required to display tools)
    toolbar->Realize();

    int toolCount = toolbar->GetToolsCount();
    core::Logger::getInstance().info("ToolbarBuilder: Toolbar created with {} tools, events bound to window",
                                    toolCount);

    return toolbar;
}

void ToolbarBuilder::addToolsFromCategory(wxToolBar* toolbar,
                                           CommandRegistry& registry,
                                           const std::string& category,
                                           wxWindow* window) {
    core::Logger::getInstance().debug("ToolbarBuilder: Adding tools from category '{}'", category);

    if (!toolbar) {
        core::Logger::getInstance().error("ToolbarBuilder: Cannot add tools - toolbar is null");
        return;
    }
    if (!window) {
        core::Logger::getInstance().error("ToolbarBuilder: Cannot add tools - window pointer is null");
        return;
    }

    // Get current icon size from IconRegistry
    IconRegistry& iconReg = IconRegistry::getInstance();
    int iconSize = iconReg.getSizes().toolbar;
    wxSize toolbarIconSize(iconSize, iconSize);

    // Query CommandRegistry for all commands in this category
    std::vector<Command> commands = registry.getCommandsByCategory(category);

    if (commands.empty()) {
        core::Logger::getInstance().debug("ToolbarBuilder: Category '{}' has no commands (empty)",
                                         category);
        return;
    }

    // Add each command as toolbar tool (if showInToolbar=true)
    int addedCount = 0;
    for (const auto& command : commands) {
        // Skip commands not marked for toolbar
        if (!command.showInToolbar) {
            core::Logger::getInstance().debug("ToolbarBuilder: Skipping command '{}' (showInToolbar=false)",
                                             command.id);
            continue;
        }

        createToolItem(toolbar, command, window, toolbarIconSize);
        addedCount++;
    }

    core::Logger::getInstance().debug("ToolbarBuilder: Added {} tools from category '{}'",
                                     addedCount, category);
}

void ToolbarBuilder::addSeparator(wxToolBar* toolbar) {
    if (!toolbar) {
        core::Logger::getInstance().error("ToolbarBuilder: Cannot add separator to null toolbar");
        return;
    }
    toolbar->AddSeparator();
}

// ============================================================================
// Private Helper Methods
// ============================================================================

void ToolbarBuilder::createToolItem(wxToolBar* toolbar,
                                     const Command& command,
                                     wxWindow* window,
                                     const wxSize& iconSize) {
    if (!toolbar) {
        core::Logger::getInstance().error("ToolbarBuilder: Cannot create tool item - toolbar is null");
        return;
    }
    if (!window) {
        core::Logger::getInstance().error("ToolbarBuilder: Cannot create tool item - window pointer is null");
        return;
    }

    // Generate unique ID for this tool
    int itemId = wxWindow::NewControlId();

    // Get icon bitmap
    wxBitmap icon = getIcon(command, iconSize.GetWidth());

    // Create tool item
    // Note: wxToolBar::AddTool automatically handles wxBitmap (not wxBitmapBundle)
    wxString label = wxString::FromUTF8(command.label);
    wxString tooltip = wxString::FromUTF8(command.tooltip);

    toolbar->AddTool(itemId, label, icon, tooltip);

    // Bind event handler to CommandRegistry::executeCommand()
    // Use lambda to capture command ID and execute through registry
    std::string commandId = command.id;  // Copy for lambda capture
    window->Bind(wxEVT_TOOL, [commandId](wxCommandEvent&) {
        CommandRegistry::getInstance().executeCommand(commandId);
    }, itemId);

    core::Logger::getInstance().debug("ToolbarBuilder: Tool '{}' created with ID {} (event bound to '{}')",
                                     command.label, itemId, commandId);
}

wxBitmap ToolbarBuilder::getIcon(const Command& command, int size) {
    // Priority 1: Use icon24 for small toolbars (size <= 24)
    if (size <= 24 && !command.icons.isEmpty() && command.icons.icon24.IsOk()) {
        return command.icons.icon24;
    }

    // Priority 2: Use icon32 for large toolbars (size > 24)
    if (size > 24 && !command.icons.isEmpty() && command.icons.icon32.IsOk()) {
        return command.icons.icon32;
    }

    // Priority 3: Fallback to icon24 if icon32 not available
    if (!command.icons.isEmpty() && command.icons.icon24.IsOk()) {
        return command.icons.icon24;
    }

    // Priority 4: No icon - create transparent placeholder bitmap
    // Note: wxToolBar on Windows Debug mode asserts if bitmap is invalid (empty)
    // We create a valid 1x1 transparent bitmap as placeholder
    core::Logger::getInstance().debug("ToolbarBuilder: Command '{}' has no icon, using placeholder", command.id);

    wxBitmap placeholder(size, size, 32);  // 32-bit with alpha channel
    wxMemoryDC dc(placeholder);
    dc.SetBackground(*wxTRANSPARENT_BRUSH);
    dc.Clear();
    dc.SelectObject(wxNullBitmap);

    return placeholder;
}

} // namespace gui
} // namespace kalahari
