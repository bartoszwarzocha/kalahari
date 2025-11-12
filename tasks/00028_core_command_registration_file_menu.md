# Task #00028: Core Command Registration - File Menu

**Status:** ✅ COMPLETE
**Priority:** P1 (HIGH - Implementation)
**Estimated:** 60 minutes
**Actual:** 55 minutes
**Dependencies:** #00025 (CommandRegistry), #00027 (ShortcutManager)
**Phase:** Phase 1 - Command Registry Architecture
**Created:** 2025-11-12
**Completed:** 2025-11-12

---

## Goal

Register File menu commands in CommandRegistry and integrate with ShortcutManager, establishing the pattern for all future command registration.

---

## Requirements

### 1. Add Command Registration Method to MainWindow

**In `src/gui/main_window.h`:**
```cpp
private:
    // Command Registry initialization (add to Helper Methods section)
    void registerFileCommands();
```

**In `src/gui/main_window.cpp`:**
- Implement `registerFileCommands()` method
- Call it from MainWindow constructor (after menu creation)

### 2. Register 6 File Menu Commands

Register the following commands in CommandRegistry:

| Command ID | Label | Shortcut | Tooltip | Category | Toolbar |
|------------|-------|----------|---------|----------|---------|
| `file.new` | "New" | Ctrl+N | "Create a new document" | "File" | Yes |
| `file.open` | "Open..." | Ctrl+O | "Open an existing document" | "File" | Yes |
| `file.save` | "Save" | Ctrl+S | "Save the current document" | "File" | Yes |
| `file.save_as` | "Save As..." | Ctrl+Shift+S | "Save document with a new name" | "File" | No |
| `file.settings` | "Settings..." | Ctrl+, | "Open application settings" | "File" | No |
| `file.exit` | "Exit" | Alt+F4 | "Exit Kalahari" | "File" | No |

**Command Registration Pattern:**
```cpp
void MainWindow::registerFileCommands() {
    CommandRegistry& registry = CommandRegistry::getInstance();
    ShortcutManager& shortcuts = ShortcutManager::getInstance();

    // file.new
    {
        Command cmd;
        cmd.id = "file.new";
        cmd.label = _("New").ToStdString();
        cmd.tooltip = _("Create a new document").ToStdString();
        cmd.category = "File";
        cmd.showInMenu = true;
        cmd.showInToolbar = true;
        cmd.shortcut = KeyboardShortcut('N', true);  // Ctrl+N
        cmd.execute = [this]() {
            // Delegate to existing event handler logic
            core::Logger::getInstance().info("File -> New executed via CommandRegistry");
            m_statusBar->SetStatusText(_("New document (stub)"), 0);
            wxMessageBox(
                _("New document functionality will be implemented in Phase 1.\n\n"
                  "Phase 1 Week 13: Command Registry Integration"),
                _("New Document"),
                wxOK | wxICON_INFORMATION,
                this
            );
        };

        registry.registerCommand(cmd);
        shortcuts.bindShortcut(cmd.shortcut, cmd.id);
    }

    // ... repeat for other 5 commands
}
```

### 3. Icons (Optional - Use wxArtProvider for now)

- **Phase 1:** Use existing wxArtProvider icons (wxART_NEW, wxART_FILE_OPEN, etc.)
- **Phase 2+:** Populate Command::icons with IconSet from IconRegistry
- **For now:** Leave IconSet empty, icons already work via wxArtProvider in menu

### 4. Update Event Handlers (Keep Existing EVT_MENU Bindings)

**Strategy:** Minimal disruption approach
- **Keep** existing EVT_MENU bindings (wxID_NEW → onFileNew)
- **Refactor** event handlers to call CommandRegistry::executeCommand()
- **Benefit:** No menu rebuild needed, commands accessible via registry

**Example refactoring:**
```cpp
// BEFORE (main_window.cpp)
void MainWindow::onFileNew([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("File -> New clicked");
    m_statusBar->SetStatusText(_("New document (stub)"), 0);
    wxMessageBox(...);
}

// AFTER
void MainWindow::onFileNew([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("file.new");
}
```

### 5. Acceptance Criteria Checklist

- [x] `registerFileCommands()` method added to MainWindow
- [x] Method called from MainWindow constructor
- [x] 6 File commands registered in CommandRegistry
- [x] All commands have correct IDs, labels, tooltips, categories
- [x] All keyboard shortcuts bound in ShortcutManager
- [x] Event handlers refactored to use CommandRegistry::executeCommand()
- [x] File -> New shows message box via CommandRegistry
- [x] File -> Open shows message box via CommandRegistry
- [x] File -> Save shows message box via CommandRegistry
- [x] File -> Exit closes application via CommandRegistry
- [x] Code compiles with no warnings
- [x] Full test suite passes (91 test cases, 655 assertions)

**Note:** File -> Settings handler NOT refactored - complex state management requires separate task

---

## Implementation Notes

### Command ID Naming Convention
- **Pattern:** `{category}.{action}` (lowercase, underscores for multi-word)
- **Examples:** `file.new`, `file.save_as`, `edit.undo`, `view.toggle_navigator`
- **Consistency:** All future commands must follow this pattern

### Shortcut Binding Order
1. Register command in CommandRegistry (includes shortcut in Command struct)
2. Bind shortcut in ShortcutManager (maps shortcut → command ID)
3. **Future:** GUI keyboard events → ShortcutManager → CommandRegistry

### Event Handler Refactoring Strategy
- **Phase 1 (this task):** Keep EVT_MENU bindings, refactor handlers to call registry
- **Phase 2:** Remove EVT_MENU bindings, build menus dynamically from registry
- **Benefit:** Incremental migration, no breaking changes

### Exit Command Special Case
```cpp
// file.exit - Preserve existing close logic
cmd.execute = [this]() {
    Close(true);  // Trigger wxCloseEvent (calls onClose handler)
};
```

### Settings Command Integration
```cpp
// file.settings - Keep existing SettingsDialog code
cmd.execute = [this]() {
    SettingsDialog dialog(this, m_launchedWithDiagFlag);
    dialog.ShowModal();
};
```

---

## Files to Modify

1. **`src/gui/main_window.h`** (+5 LOC)
   - Add `registerFileCommands()` declaration

2. **`src/gui/main_window.cpp`** (+120 LOC)
   - Implement `registerFileCommands()` (100 LOC for 6 commands)
   - Call from constructor (1 LOC)
   - Refactor 6 event handlers to use registry (15 LOC changes)

**Total:** ~125 LOC changes

---

## Testing Strategy

**Manual Testing (No Unit Tests for This Task):**
1. Launch Kalahari
2. Verify File menu items still appear correctly
3. Click File -> New (should show message box)
4. Click File -> Open (should show message box)
5. Click File -> Save (should show message box)
6. Click File -> Save As (should show message box)
7. Click File -> Settings (should open Settings dialog)
8. Click File -> Exit (should close application)
9. Press Ctrl+N (should show New message box)
10. Press Ctrl+O (should show Open message box)
11. Press Ctrl+S (should show Save message box)
12. Press Ctrl+Shift+S (should show Save As message box)
13. Run full test suite: `./build-linux-wsl/bin/kalahari-tests`
14. Verify all 91 test cases pass (655 assertions)

**Future Unit Tests (Phase 2):**
- Command execution via registry
- Shortcut resolution
- Menu building from registry

---

## Next Task

**Task #00029:** Core Command Registration - Edit Menu
- Register Cut, Copy, Paste, Select All, Undo, Redo commands
- Similar pattern to Task #00028
- Estimated: 45 minutes

---

## Architecture Notes

### Why Not Update Menus Dynamically Yet?
- **Current:** Menu created in createMenuBar(), commands registered afterwards
- **Phase 1:** Keep existing menu creation code, just register commands
- **Phase 2:** Refactor createMenuBar() to read from CommandRegistry
- **Reason:** Minimize risk, incremental migration

### Command Registry Integration Stages

**Stage 1 (Tasks #00024-27):** ✅ COMPLETE
- Infrastructure (Command struct, CommandRegistry, ShortcutManager)

**Stage 2 (Tasks #00028-32):** 📋 IN PROGRESS
- Register existing commands (File, Edit, Format, View, Help menus)
- Keep existing event handlers, refactor to call registry

**Stage 3 (Tasks #00033-36):** 📋 PLANNED
- Dynamic menu/toolbar builders
- Remove hardcoded menu creation
- Full integration with Settings (customizable shortcuts, toolbar)

---

**Created:** 2025-11-12
**Author:** Architecture Planning (Task #00023 follow-up)
