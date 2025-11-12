# Task #00029: Core Command Registration - Edit Menu

**Status:** ✅ COMPLETE
**Priority:** P1 (HIGH - Implementation)
**Estimated:** 45 minutes
**Actual:** 42 minutes
**Dependencies:** #00025 (CommandRegistry), #00027 (ShortcutManager), #00028 (File Menu pattern)
**Phase:** Phase 1 - Command Registry Architecture
**Created:** 2025-11-12
**Completed:** 2025-11-12

---

## Goal

Register 6 Edit menu commands in CommandRegistry following the pattern established in Task #00028.

---

## Requirements

### 1. Add Command Registration Method to MainWindow

**In `src/gui/main_window.h`:**
```cpp
private:
    void registerEditCommands();  // After registerFileCommands()
```

**In `src/gui/main_window.cpp`:**
- Implement `registerEditCommands()` method
- Call it from MainWindow constructor (after registerFileCommands())

### 2. Register 6 Edit Menu Commands

| Command ID | Label | Shortcut | Tooltip | Category | Toolbar | Handler Type |
|------------|-------|----------|---------|----------|---------|--------------|
| `edit.undo` | "Undo" | Ctrl+Z | "Undo last action" | "Edit" | Yes | Stub |
| `edit.redo` | "Redo" | Ctrl+Y | "Redo last undone action" | "Edit" | Yes | Stub |
| `edit.cut` | "Cut" | Ctrl+X | "Cut selection to clipboard" | "Edit" | Yes | Delegate |
| `edit.copy` | "Copy" | Ctrl+C | "Copy selection to clipboard" | "Edit" | Yes | Delegate |
| `edit.paste` | "Paste" | Ctrl+V | "Paste from clipboard" | "Edit" | Yes | Delegate |
| `edit.select_all` | "Select All" | Ctrl+A | "Select all text" | "Edit" | No | Delegate |

**Handler Types:**
- **Stub:** Shows message box (Undo, Redo)
- **Delegate:** Forwards to EditorPanel (Cut, Copy, Paste, Select All)

### 3. Command Execute Callbacks

**For Stub commands (Undo, Redo):**
```cpp
cmd.execute = [this]() {
    core::Logger::getInstance().info("Edit -> Undo executed via CommandRegistry");
    m_statusBar->SetStatusText(_("Undo (stub)"), 0);
    wxMessageBox(
        _("Undo functionality will be implemented in Phase 1.\n\n"
          "Phase 1 Week 13: Command Registry Integration"),
        _("Undo"),
        wxOK | wxICON_INFORMATION,
        this
    );
};
```

**For Delegate commands (Cut, Copy, Paste, Select All):**
```cpp
cmd.execute = [this]() {
    core::Logger::getInstance().debug("Edit -> Cut executed via CommandRegistry");

    if (m_editorPanel) {
        wxCommandEvent event(wxEVT_MENU, wxID_CUT);
        m_editorPanel->onEditCut(event);
    } else {
        core::Logger::getInstance().warn("No EditorPanel available for Edit -> Cut");
    }
};
```

**Note:** We create dummy wxCommandEvent because EditorPanel handlers expect it.

### 4. Refactor Event Handlers

**Pattern (all 6 handlers):**
```cpp
// BEFORE (e.g., onEditUndo - 10 lines)
void MainWindow::onEditUndo([[maybe_unused]] wxCommandEvent& event) {
    core::Logger::getInstance().info("Edit -> Undo clicked");
    m_statusBar->SetStatusText(_("Undo (stub)"), 0);
    wxMessageBox(...);
}

// AFTER (1 line)
void MainWindow::onEditUndo([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("edit.undo");
}
```

### 5. Acceptance Criteria

- [x] `registerEditCommands()` method added to MainWindow
- [x] Method called from MainWindow constructor
- [x] 6 Edit commands registered in CommandRegistry
- [x] All commands have correct IDs, labels, tooltips, categories
- [x] All keyboard shortcuts bound in ShortcutManager
- [x] Event handlers refactored to use CommandRegistry::executeCommand()
- [x] Edit -> Undo shows message box via CommandRegistry
- [x] Edit -> Redo shows message box via CommandRegistry
- [x] Edit -> Cut delegates to EditorPanel via CommandRegistry
- [x] Edit -> Copy delegates to EditorPanel via CommandRegistry
- [x] Edit -> Paste delegates to EditorPanel via CommandRegistry
- [x] Edit -> Select All delegates to EditorPanel via CommandRegistry
- [x] Code compiles with no warnings
- [x] Full test suite passes (91 test cases, 655 assertions)

**Technical Note:** Added TODO comments for future EditorPanel refactoring (Phase 2) to replace event-based delegation with direct method calls.

---

## Implementation Notes

### wxCommandEvent for EditorPanel Delegation

EditorPanel handlers expect wxCommandEvent:
```cpp
void EditorPanel::onEditCut(wxCommandEvent& event);
```

So we create dummy event in command callback:
```cpp
wxCommandEvent event(wxEVT_MENU, wxID_CUT);
m_editorPanel->onEditCut(event);
```

**Alternative approach (Phase 2):**
Refactor EditorPanel to have direct methods:
```cpp
void EditorPanel::cut();
void EditorPanel::copy();
void EditorPanel::paste();
```

### Command ID Convention

Following pattern from Task #00028:
- Format: `{category}.{action}` (lowercase, underscores)
- Examples: `edit.undo`, `edit.select_all`

### Toolbar Integration

All 6 commands have `showInToolbar = true` because they're common editing operations. In Phase 2, toolbar will be dynamically built from CommandRegistry.

---

## Files to Modify

1. **`src/gui/main_window.h`** (+5 LOC)
   - Add `registerEditCommands()` declaration

2. **`src/gui/main_window.cpp`** (+135 LOC implementation, -42 LOC from handlers = +93 net)
   - Implement `registerEditCommands()` (~125 LOC for 6 commands)
   - Call from constructor (1 LOC)
   - Refactor 6 event handlers (6 LOC, remove ~42 LOC)

**Total:** ~98 LOC net change

---

## Testing Strategy

**Manual Testing:**
1. Launch Kalahari
2. Verify Edit menu items appear correctly
3. Click Edit -> Undo (should show message box)
4. Click Edit -> Redo (should show message box)
5. Type text in editor, select text
6. Click Edit -> Cut (should work via EditorPanel delegation)
7. Click Edit -> Copy (should work)
8. Click Edit -> Paste (should work)
9. Click Edit -> Select All (should work)
10. Test keyboard shortcuts:
    - Ctrl+Z (Undo message)
    - Ctrl+Y (Redo message)
    - Ctrl+X (Cut)
    - Ctrl+C (Copy)
    - Ctrl+V (Paste)
    - Ctrl+A (Select All)
11. Run full test suite: `./build-linux-wsl/bin/kalahari-tests`
12. Verify all 91 test cases pass (655 assertions)

---

## Next Task

**Task #00030:** Core Command Registration - Format Menu
- Register Bold, Italic, Underline, Font, Clear Formatting commands
- Similar pattern to Task #00029
- Estimated: 40 minutes

---

## Architecture Notes

### Why Keep EVT_MENU Bindings?

Same minimal disruption approach as Task #00028:
- Menus already built via createMenuBar()
- EVT_MENU bindings work, no reason to break them
- Handlers just delegate to CommandRegistry
- Phase 2: Dynamic menu building from registry

### Command vs UI Action

Edit commands are **real operations on data:**
- ✅ Undo/Redo manipulate document state
- ✅ Cut/Copy/Paste manipulate clipboard + selection
- ✅ Select All changes selection state

**Not like Settings dialog** (which is pure UI navigation).

---

**Created:** 2025-11-12
**Author:** Atomic Task Implementation (following Task #00028 pattern)
