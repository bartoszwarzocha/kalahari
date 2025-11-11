# Task #00028: Core Command Registration - File Menu

**Status:** 📋 Planned
**Priority:** P1 (HIGH - Architecture Foundation)
**Estimated:** 60 minutes
**Dependencies:** #00025 (CommandRegistry), #00024 (Command structure)
**Phase:** Phase 0 - Architecture

---

## Goal

Register all File menu commands in CommandRegistry with proper icons, shortcuts, and execution handlers.

---

## Requirements

### 1. Create `src/gui/core_commands.cpp`

Implement function to register File menu commands:

```cpp
void registerFileCommands(CommandRegistry& registry, MainWindow* mainWindow) {
    // File -> New
    Command newCmd;
    newCmd.id = "file.new";
    newCmd.label = _("New");
    newCmd.tooltip = _("Create new document");
    newCmd.category = "File";
    newCmd.icons = IconSet(wxART_NEW);
    newCmd.shortcut.ctrl = true;
    newCmd.shortcut.keyCode = 'N';
    newCmd.execute = [mainWindow]() {
        mainWindow->onFileNew(wxCommandEvent());
    };
    newCmd.showInToolbar = true;
    registry.registerCommand(newCmd);

    // File -> Open
    Command openCmd;
    openCmd.id = "file.open";
    openCmd.label = _("Open");
    openCmd.tooltip = _("Open existing document");
    openCmd.category = "File";
    openCmd.icons = IconSet(wxART_FILE_OPEN);
    openCmd.shortcut.ctrl = true;
    openCmd.shortcut.keyCode = 'O';
    openCmd.execute = [mainWindow]() {
        mainWindow->onFileOpen(wxCommandEvent());
    };
    openCmd.showInToolbar = true;
    registry.registerCommand(openCmd);

    // File -> Save
    // File -> Save As
    // File -> Settings
    // File -> Exit
    // ... (similar pattern)
}
```

### 2. Create `include/kalahari/gui/core_commands.h`

```cpp
#pragma once

namespace kalahari {
namespace gui {

class CommandRegistry;
class MainWindow;

/// Register all File menu commands
void registerFileCommands(CommandRegistry& registry, MainWindow* mainWindow);

} // namespace gui
} // namespace kalahari
```

### 3. Commands to Register

**File Menu:**
- `file.new` - New document (Ctrl+N)
- `file.open` - Open document (Ctrl+O)
- `file.save` - Save document (Ctrl+S)
- `file.saveas` - Save As... (Ctrl+Shift+S)
- `file.settings` - Settings... (no shortcut)
- `file.exit` - Exit (Ctrl+Q on Linux/Windows, Cmd+Q on macOS)

---

## Implementation Notes

**Icons:**
- Use wxArtProvider constants: wxART_NEW, wxART_FILE_OPEN, wxART_FILE_SAVE
- IconSet constructor should handle wxArtProvider IDs

**Shortcuts:**
- Follow platform conventions (Cmd vs Ctrl on macOS)
- Use KeyboardShortcut structure from Task #00024

**Execution Handlers:**
- Lambda captures MainWindow pointer
- Calls existing event handlers (onFileNew, onFileOpen, etc.)
- Phase 1+: Direct business logic instead of event handlers

**Toolbar Visibility:**
- Set showInToolbar=true only for: New, Open, Save
- Settings and Exit are menu-only

---

## Acceptance Criteria

- [ ] `core_commands.h` created with registerFileCommands declaration
- [ ] `core_commands.cpp` implements all 6 File commands
- [ ] All commands have proper icons from wxArtProvider
- [ ] All commands have keyboard shortcuts where appropriate
- [ ] Execution handlers call MainWindow methods
- [ ] Code compiles, no warnings
- [ ] Commands can be queried from CommandRegistry

---

## Testing

Manual verification in MainWindow constructor:
```cpp
auto& registry = CommandRegistry::getInstance();
registerFileCommands(registry, this);

assert(registry.isCommandRegistered("file.new"));
assert(registry.isCommandRegistered("file.save"));

auto* saveCmd = registry.getCommand("file.save");
assert(saveCmd != nullptr);
assert(saveCmd->label == _("Save"));
assert(saveCmd->shortcut.ctrl == true);
assert(saveCmd->shortcut.keyCode == 'S');

auto fileCommands = registry.getCommandsByCategory("File");
assert(fileCommands.size() == 6);
```

---

## Related Files

- `include/kalahari/gui/core_commands.h` (new)
- `src/gui/core_commands.cpp` (new)
- `src/CMakeLists.txt` (add core_commands.cpp)
- `src/gui/main_window.cpp` (call registerFileCommands in constructor)

---

## Next Task

Task #00029 - Core Command Registration - Edit Menu

---

**Created:** 2025-11-11
**Author:** Architecture Planning
