# Task #00030: Core Command Registration - Format Menu

**Status:** ✅ COMPLETE
**Priority:** P1 (HIGH - Implementation)
**Estimated:** 40 minutes
**Actual:** 38 minutes
**Dependencies:** #00025 (CommandRegistry), #00027 (ShortcutManager), #00029 (Edit Menu pattern)
**Phase:** Phase 1 - Command Registry Architecture
**Created:** 2025-11-12
**Completed:** 2025-11-12

---

## Goal

Register 5 Format menu commands in CommandRegistry following the pattern established in Task #00028 (File menu) and Task #00029 (Edit menu).

---

## Requirements

### 1. Add Command Registration Method to MainWindow

**In `src/gui/main_window.h`:**
```cpp
private:
    void registerFormatCommands();  // After registerEditCommands()
```

**In `src/gui/main_window.cpp`:**
- Implement `registerFormatCommands()` method
- Call it from MainWindow constructor (after registerEditCommands())

### 2. Register 5 Format Menu Commands

| Command ID | Label | Shortcut | Tooltip | Category | Toolbar | Handler Type |
|------------|-------|----------|---------|----------|---------|--------------|
| `format.bold` | "Bold" | Ctrl+B | "Toggle bold formatting" | "Format" | Yes | Delegate |
| `format.italic` | "Italic" | Ctrl+I | "Toggle italic formatting" | "Format" | Yes | Delegate |
| `format.underline` | "Underline" | Ctrl+U | "Toggle underline" | "Format" | Yes | Delegate |
| `format.font` | "Font..." | (none) | "Choose font and size" | "Format" | No | Delegate |
| `format.clear_formatting` | "Clear Formatting" | (none) | "Remove all formatting" | "Format" | No | Delegate |

**Handler Types:**
- **Delegate:** Forwards to EditorPanel (all 5 commands)

### 3. Command Execute Callbacks

**For all Delegate commands (Bold, Italic, Underline, Font, Clear Formatting):**
```cpp
cmd.execute = [this]() {
    core::Logger::getInstance().debug("Format -> Bold executed via CommandRegistry");

    if (m_editorPanel) {
        // TODO (Phase 2): Refactor EditorPanel to have direct formatBold() method
        wxCommandEvent event(wxEVT_MENU, ID_FORMAT_BOLD);
        m_editorPanel->onFormatBold(event);
    } else {
        core::Logger::getInstance().warn("No EditorPanel available for Format -> Bold");
    }
};
```

**Note:** We create dummy wxCommandEvent because EditorPanel handlers expect it.

### 4. Refactor Event Handlers

**Pattern (all 5 handlers):**
```cpp
// BEFORE (e.g., onFormatBold - 8 lines)
void MainWindow::onFormatBold(wxCommandEvent& event) {
    core::Logger::getInstance().debug("Format -> Bold clicked");

    if (m_editorPanel) {
        m_editorPanel->onFormatBold(event);
    } else {
        core::Logger::getInstance().warn("No EditorPanel available");
    }
}

// AFTER (1 line)
void MainWindow::onFormatBold([[maybe_unused]] wxCommandEvent& event) {
    CommandRegistry::getInstance().executeCommand("format.bold");
}
```

### 5. Acceptance Criteria

- [x] `registerFormatCommands()` method added to MainWindow
- [x] Method called from MainWindow constructor
- [x] 5 Format commands registered in CommandRegistry
- [x] All commands have correct IDs, labels, tooltips, categories
- [x] Keyboard shortcuts bound in ShortcutManager (Ctrl+B, Ctrl+I, Ctrl+U)
- [x] Event handlers refactored to use CommandRegistry::executeCommand()
- [x] Format -> Bold delegates to EditorPanel via CommandRegistry
- [x] Format -> Italic delegates to EditorPanel via CommandRegistry
- [x] Format -> Underline delegates to EditorPanel via CommandRegistry
- [x] Format -> Font delegates to EditorPanel via CommandRegistry
- [x] Format -> Clear Formatting delegates to EditorPanel via CommandRegistry
- [x] Code compiles with no warnings
- [x] Full test suite passes (91 test cases, 655 assertions)

**Technical Note:** Added TODO comments for future EditorPanel refactoring (Phase 2) to replace event-based delegation with direct method calls.

---

## Implementation Notes

### wxCommandEvent for EditorPanel Delegation

EditorPanel handlers expect wxCommandEvent:
```cpp
void EditorPanel::onFormatBold(wxCommandEvent& event);
```

So we create dummy event in command callback:
```cpp
wxCommandEvent event(wxEVT_MENU, ID_FORMAT_BOLD);
m_editorPanel->onFormatBold(event);
```

**Alternative approach (Phase 2):**
Refactor EditorPanel to have direct methods:
```cpp
void EditorPanel::formatBold();
void EditorPanel::formatItalic();
void EditorPanel::formatUnderline();
void EditorPanel::formatFont();
void EditorPanel::clearFormatting();
```

### Command ID Convention

Following pattern from Task #00028 and #00029:
- Format: `{category}.{action}` (lowercase, underscores)
- Examples: `format.bold`, `format.clear_formatting`

### Toolbar Integration

Bold, Italic, Underline have `showInToolbar = true` (common formatting operations). Font and Clear Formatting have `showInToolbar = false`. In Phase 2, toolbar will be dynamically built from CommandRegistry.

---

## Files to Modify

1. **`src/gui/main_window.h`** (+5 LOC)
   - Add `registerFormatCommands()` declaration

2. **`src/gui/main_window.cpp`** (+115 LOC implementation, -35 LOC from handlers = +80 net)
   - Implement `registerFormatCommands()` (~105 LOC for 5 commands)
   - Call from constructor (1 LOC)
   - Refactor 5 event handlers (5 LOC, remove ~35 LOC)

**Total:** ~85 LOC net change

---

## Testing Strategy

**Manual Testing:**
1. Launch Kalahari
2. Verify Format menu items appear correctly
3. Type text in editor, select text
4. Click Format -> Bold (should work via EditorPanel delegation)
5. Click Format -> Italic (should work)
6. Click Format -> Underline (should work)
7. Click Format -> Font (should open font dialog)
8. Click Format -> Clear Formatting (should work)
9. Test keyboard shortcuts:
    - Ctrl+B (Bold)
    - Ctrl+I (Italic)
    - Ctrl+U (Underline)
10. Run full test suite: `./build-linux-wsl/bin/kalahari-tests`
11. Verify all 91 test cases pass (655 assertions)

---

## Next Task

**Task #00031:** Core Command Registration - View Menu
- Register panel visibility toggle commands (Navigator, Properties, Statistics, Search, Assistant, Log)
- Register Editor Mode commands (Full, Page, Typewriter, Publisher)
- Register Perspective commands (Load, Save, Manage)
- Similar pattern to Task #00029, estimated 60 minutes

---

## Architecture Notes

### Why Keep EVT_MENU Bindings?

Same minimal disruption approach as Task #00028 and #00029:
- Menus already built via createMenuBar()
- EVT_MENU bindings work, no reason to break them
- Handlers just delegate to CommandRegistry
- Phase 2: Dynamic menu building from registry

### Command vs UI Action

Format commands are **real operations on data:**
- ✅ Bold/Italic/Underline manipulate text formatting state
- ✅ Font changes text appearance
- ✅ Clear Formatting removes formatting attributes

**Not like Settings dialog** (which is pure UI navigation).

---

**Created:** 2025-11-12
**Author:** Atomic Task Implementation (following Task #00028 and #00029 pattern)
