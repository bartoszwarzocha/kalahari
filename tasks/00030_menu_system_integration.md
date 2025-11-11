# Task #00030: Menu System Integration with CommandRegistry

**Status:** 📋 Planned
**Priority:** P1 (HIGH - Architecture Foundation)
**Estimated:** 90 minutes
**Dependencies:** #00028 (File commands), #00029 (Edit commands)
**Phase:** Phase 0 - Architecture

---

## Goal

Replace hardcoded menu system with command-based menu generation. Menu items execute commands via CommandRegistry, enabling unified command handling across menu, toolbar, keyboard shortcuts, and Command Palette.

---

## Problem with Current Implementation

**Current (WRONG):**
```
Menu "File->Save" → onFileSave() → business logic
Toolbar "Save"    → (different path)
Keyboard Ctrl+S   → (different path)
```

**Target (CORRECT):**
```
Menu "File->Save"  ─┐
Toolbar "Save"     ├─→ CommandRegistry::executeCommand("file.save") → business logic
Keyboard Ctrl+S    ─┘
Command Palette    ─┘
```

---

## Requirements

### 1. Create `include/kalahari/gui/menu_builder.h`

```cpp
#pragma once

#include <wx/menu.h>
#include <string>
#include <map>

namespace kalahari {
namespace gui {

class CommandRegistry;

/// Helper class for building menus from CommandRegistry
class MenuBuilder {
public:
    MenuBuilder(wxWindow* parent);

    /// Create menu from commands in specific category
    /// @param category Command category (e.g., "File", "Edit")
    /// @param sortOrder Optional list of command IDs for custom order
    /// @return wxMenu populated with commands from category
    wxMenu* createMenuFromCategory(const std::string& category,
                                   const std::vector<std::string>& sortOrder = {});

    /// Add single command to existing menu
    /// @param menu Target menu
    /// @param commandId Command ID to add
    /// @param separator If true, add separator after this item
    void addCommandToMenu(wxMenu* menu, const std::string& commandId, bool separator = false);

    /// Get mapping of wxID to command ID (for event handling)
    const std::map<int, std::string>& getCommandIdMap() const { return m_commandIdMap; }

private:
    wxWindow* m_parent;
    std::map<int, std::string> m_commandIdMap;  // wxID -> command ID
};

} // namespace gui
} // namespace kalahari
```

### 2. Implement in `src/gui/menu_builder.cpp`

**createMenuFromCategory:**
```cpp
wxMenu* MenuBuilder::createMenuFromCategory(const std::string& category,
                                           const std::vector<std::string>& sortOrder) {
    auto& registry = CommandRegistry::getInstance();
    wxMenu* menu = new wxMenu;

    // Get commands from category
    auto commands = registry.getCommandsByCategory(category);

    // Sort if order specified
    std::vector<const Command*> sortedCommands;
    if (!sortOrder.empty()) {
        for (const auto& cmdId : sortOrder) {
            auto* cmd = registry.getCommand(cmdId);
            if (cmd && cmd->showInMenu) {
                sortedCommands.push_back(cmd);
            }
        }
    } else {
        sortedCommands = commands;
    }

    // Create menu items
    for (const Command* cmd : sortedCommands) {
        if (!cmd->showInMenu) continue;

        int wxId = wxWindow::NewControlId();
        m_commandIdMap[wxId] = cmd->id;

        wxString label = cmd->label;
        if (!cmd->shortcut.keyCode == 0) {
            label += "\t" + cmd->shortcut.ToString();
        }

        wxMenuItem* item = menu->Append(wxId, label, cmd->tooltip);

        // Bind event to execute command
        m_parent->Bind(wxEVT_MENU, [cmdId = cmd->id](wxCommandEvent&) {
            CommandRegistry::getInstance().executeCommand(cmdId);
        }, wxId);
    }

    return menu;
}
```

**addCommandToMenu:**
```cpp
void MenuBuilder::addCommandToMenu(wxMenu* menu, const std::string& commandId, bool separator) {
    auto& registry = CommandRegistry::getInstance();
    auto* cmd = registry.getCommand(commandId);

    if (!cmd) {
        KALAHARI_WARN("Command '{}' not found for menu", commandId);
        return;
    }

    if (!cmd->showInMenu) {
        return;
    }

    int wxId = wxWindow::NewControlId();
    m_commandIdMap[wxId] = cmd->id;

    wxString label = cmd->label;
    if (cmd->shortcut.keyCode != 0) {
        label += "\t" + cmd->shortcut.ToString();
    }

    wxMenuItem* item = menu->Append(wxId, label, cmd->tooltip);

    m_parent->Bind(wxEVT_MENU, [cmdId = cmd->id](wxCommandEvent&) {
        CommandRegistry::getInstance().executeCommand(cmdId);
    }, wxId);

    if (separator) {
        menu->AppendSeparator();
    }
}
```

### 3. Modify `src/gui/main_window.cpp` - createMenuBar()

Replace hardcoded menu creation with MenuBuilder:

```cpp
void MainWindow::createMenuBar() {
    MenuBuilder builder(this);
    wxMenuBar* menuBar = new wxMenuBar;

    // File menu with custom order
    std::vector<std::string> fileOrder = {
        "file.new",
        "file.open",
        "file.save",
        "file.saveas",
        // separator will be added manually
        "file.settings",
        "file.exit"
    };

    wxMenu* fileMenu = new wxMenu;
    builder.addCommandToMenu(fileMenu, "file.new", false);
    builder.addCommandToMenu(fileMenu, "file.open", false);
    builder.addCommandToMenu(fileMenu, "file.save", false);
    builder.addCommandToMenu(fileMenu, "file.saveas", true);  // separator after
    builder.addCommandToMenu(fileMenu, "file.settings", true);
    builder.addCommandToMenu(fileMenu, "file.exit", false);

    menuBar->Append(fileMenu, _("&File"));

    // Edit menu
    wxMenu* editMenu = new wxMenu;
    builder.addCommandToMenu(editMenu, "edit.undo", false);
    builder.addCommandToMenu(editMenu, "edit.redo", true);  // separator
    builder.addCommandToMenu(editMenu, "edit.cut", false);
    builder.addCommandToMenu(editMenu, "edit.copy", false);
    builder.addCommandToMenu(editMenu, "edit.paste", true);
    builder.addCommandToMenu(editMenu, "edit.selectall", false);

    menuBar->Append(editMenu, _("&Edit"));

    // View menu (existing panels - keep for now)
    wxMenu* viewMenu = createViewMenu();
    menuBar->Append(viewMenu, _("&View"));

    // Help menu (existing - keep for now)
    wxMenu* helpMenu = createHelpMenu();
    menuBar->Append(helpMenu, _("&Help"));

    SetMenuBar(menuBar);
}
```

---

## Implementation Notes

**Event Binding:**
- Use lambda captures with command ID
- Lambda calls `CommandRegistry::executeCommand(cmdId)`
- No need for event table entries (dynamic binding)

**Menu Item State (Phase 1+):**
- Enable/disable: Query `Command::isEnabled()` via update events
- Checkmarks: Query `Command::isChecked()` for toggle commands
- Phase 0: All items always enabled (simplified)

**Keyboard Shortcuts:**
- Displayed in menu automatically (label + "\t" + shortcut)
- Actual execution via ShortcutManager (Task #00027)
- Menu shows shortcut hint only

**Old Event Handlers:**
- Keep for now (View menu, Help->About, etc.)
- Migrate to commands in Phase 1
- Phase 0 focus: File and Edit menus only

---

## Acceptance Criteria

- [ ] `menu_builder.h` created with MenuBuilder class
- [ ] `menu_builder.cpp` implements createMenuFromCategory
- [ ] `menu_builder.cpp` implements addCommandToMenu
- [ ] createMenuBar() uses MenuBuilder for File menu
- [ ] createMenuBar() uses MenuBuilder for Edit menu
- [ ] Clicking menu items executes commands via CommandRegistry
- [ ] Keyboard shortcuts displayed in menu
- [ ] Code compiles, no warnings
- [ ] Menu items work identically to old implementation

---

## Testing

**Manual Test Plan:**
1. Launch application
2. **File Menu:**
   - Click File -> New (should create new document)
   - Click File -> Save (should save)
   - Verify keyboard shortcut shown in menu (Ctrl+S)
3. **Edit Menu:**
   - Click Edit -> Undo (should undo)
   - Click Edit -> Copy (should copy)
   - Verify shortcuts shown (Ctrl+Z, Ctrl+C, etc.)
4. **Unified Execution:**
   - Try command from menu: File -> Save
   - Try same command from toolbar: Save button
   - Try keyboard: Ctrl+S
   - All three should execute identical code path

---

## Related Files

- `include/kalahari/gui/menu_builder.h` (new)
- `src/gui/menu_builder.cpp` (new)
- `src/gui/main_window.cpp` (modify createMenuBar)
- `src/CMakeLists.txt` (add menu_builder.cpp)

---

## Next Task

Task #00031 - ToolbarManager Singleton

---

**Created:** 2025-11-11
**Author:** Architecture Planning
