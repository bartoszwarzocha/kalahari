# Task #00029: Core Command Registration - Edit Menu

**Status:** 📋 Planned
**Priority:** P1 (HIGH - Architecture Foundation)
**Estimated:** 60 minutes
**Dependencies:** #00028 (File commands), #00025 (CommandRegistry)
**Phase:** Phase 0 - Architecture

---

## Goal

Register all Edit menu commands in CommandRegistry with proper icons, shortcuts, and execution handlers.

---

## Requirements

### 1. Extend `src/gui/core_commands.cpp`

Add function to register Edit menu commands:

```cpp
void registerEditCommands(CommandRegistry& registry, MainWindow* mainWindow) {
    // Edit -> Undo
    Command undoCmd;
    undoCmd.id = "edit.undo";
    undoCmd.label = _("Undo");
    undoCmd.tooltip = _("Undo last action");
    undoCmd.category = "Edit";
    undoCmd.icons = IconSet(wxART_UNDO);
    undoCmd.shortcut.ctrl = true;
    undoCmd.shortcut.keyCode = 'Z';
    undoCmd.execute = [mainWindow]() {
        mainWindow->onEditUndo(wxCommandEvent());
    };
    undoCmd.showInToolbar = true;
    registry.registerCommand(undoCmd);

    // Edit -> Redo
    Command redoCmd;
    redoCmd.id = "edit.redo";
    redoCmd.label = _("Redo");
    redoCmd.tooltip = _("Redo last undone action");
    redoCmd.category = "Edit";
    redoCmd.icons = IconSet(wxART_REDO);
    redoCmd.shortcut.ctrl = true;
    redoCmd.shortcut.shift = true;
    redoCmd.shortcut.keyCode = 'Z';
    redoCmd.execute = [mainWindow]() {
        mainWindow->onEditRedo(wxCommandEvent());
    };
    redoCmd.showInToolbar = true;
    registry.registerCommand(redoCmd);

    // Edit -> Cut
    // Edit -> Copy
    // Edit -> Paste
    // Edit -> Select All
    // ... (similar pattern)
}
```

### 2. Extend `include/kalahari/gui/core_commands.h`

```cpp
/// Register all Edit menu commands
void registerEditCommands(CommandRegistry& registry, MainWindow* mainWindow);
```

### 3. Commands to Register

**Edit Menu:**
- `edit.undo` - Undo (Ctrl+Z)
- `edit.redo` - Redo (Ctrl+Shift+Z or Ctrl+Y)
- `edit.cut` - Cut (Ctrl+X)
- `edit.copy` - Copy (Ctrl+C)
- `edit.paste` - Paste (Ctrl+V)
- `edit.selectall` - Select All (Ctrl+A)

---

## Implementation Notes

**Icons:**
- wxART_UNDO, wxART_REDO for undo/redo
- wxART_CUT, wxART_COPY, wxART_PASTE for clipboard operations
- No icon for Select All (menu-only)

**Shortcuts:**
- Undo: Ctrl+Z (standard across platforms)
- Redo: Ctrl+Shift+Z (standard on macOS/Linux), Ctrl+Y (Windows alternative)
- Clipboard: Ctrl+X/C/V (universal)
- Select All: Ctrl+A (universal)

**Execution Handlers:**
- Call existing MainWindow event handlers
- Phase 1+: Direct wxRichTextCtrl operations

**Toolbar Visibility:**
- Set showInToolbar=true for: Undo, Redo
- Clipboard operations: toolbar optional (user customizable)
- Select All: menu-only (showInToolbar=false)

---

## Acceptance Criteria

- [ ] registerEditCommands function implemented
- [ ] All 6 Edit commands registered with proper icons
- [ ] All commands have standard keyboard shortcuts
- [ ] Execution handlers call MainWindow methods
- [ ] Undo/Redo marked for toolbar display
- [ ] Code compiles, no warnings
- [ ] Commands can be queried from CommandRegistry

---

## Testing

Manual verification in MainWindow constructor:
```cpp
auto& registry = CommandRegistry::getInstance();
registerEditCommands(registry, this);

assert(registry.isCommandRegistered("edit.undo"));
assert(registry.isCommandRegistered("edit.copy"));

auto* undoCmd = registry.getCommand("edit.undo");
assert(undoCmd != nullptr);
assert(undoCmd->shortcut.ctrl == true);
assert(undoCmd->shortcut.keyCode == 'Z');
assert(undoCmd->showInToolbar == true);

auto editCommands = registry.getCommandsByCategory("Edit");
assert(editCommands.size() == 6);
```

---

## Related Files

- `include/kalahari/gui/core_commands.h` (extend)
- `src/gui/core_commands.cpp` (extend)
- `src/gui/main_window.cpp` (call registerEditCommands in constructor)

---

## Next Task

Task #00030 - ToolbarManager Singleton

---

**Created:** 2025-11-11
**Author:** Architecture Planning
