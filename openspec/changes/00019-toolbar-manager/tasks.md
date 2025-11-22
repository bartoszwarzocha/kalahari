# Tasks: Toolbar Manager System

**Change ID:** `00019-toolbar-manager`
**Status:** In Progress
**Created:** 2025-11-22

---

## Task Breakdown

### Task 1: Create ToolbarManager Class (90 min)

**Files:**
- Create `include/kalahari/gui/toolbar_manager.h`
- Create `src/gui/toolbar_manager.cpp`

**Implementation:**
- ToolbarManager class with ToolbarConfig struct
- Constructor taking QMainWindow*
- `createToolbars(CommandRegistry&)` - creates 6 toolbars
- `getToolbar(id)` - retrieve toolbar by ID
- `showToolbar(id, bool)` - toggle visibility
- `saveState()` / `restoreState()` - QSettings persistence
- `createViewMenuActions(QMenu*)` - View > Toolbars submenu
- Private: m_toolbars, m_configs, m_viewActions maps

**Toolbar Definitions:**
1. File: New, Open, Save, SaveAs, Close
2. Edit: Undo, Redo, Cut, Copy, Paste, SelectAll
3. Book: NewChapter, NewCharacter, NewLocation, BookProperties
4. View: Navigator, Properties, Search, Assistant, Log toggles
5. Tools: Spellcheck, WordCount, FocusMode

**Acceptance:**
- ✅ ToolbarManager compiles
- ✅ 6 toolbars created with correct command IDs
- ✅ Toolbars movable, floatable, dockable
- ✅ All toolbars visible by default

---

### Task 2: Add IconSet Factory Methods (45 min)

**Files:**
- Modify `include/kalahari/gui/command.h`
- Modify `src/gui/command.cpp`

**Implementation:**
- `IconSet::fromStandardIcon(QStyle::StandardPixmap)` - Qt standard icons
- `IconSet::createPlaceholder(QString letter, QColor color)` - colored square with letter
- Helper `createPixmap(int size)` using QPainter

**Standard Icons Mapping:**
- SP_FileIcon (New)
- SP_DirOpenIcon (Open)
- SP_DialogSaveButton (Save)
- SP_ArrowBack (Undo)
- SP_ArrowForward (Redo)
- SP_DialogCancelButton (Close)

**Placeholder Icons:**
- "A" blue (SaveAs)
- "X" red (Cut)
- "C" blue (Copy)
- "V" green (Paste)
- "S" purple (SelectAll)
- "B" blue (Bold)
- "I" green (Italic)
- "U" orange (Underline)

**Acceptance:**
- ✅ Both factory methods implemented
- ✅ Standard icons load correctly (16/24/32px)
- ✅ Placeholder icons render correctly (letter centered, white on color)
- ✅ No null pixmaps

---

### Task 3: Add REG_CMD_TOOL_ICON Macro (15 min)

**Files:**
- Modify `src/gui/register_commands.hpp`

**Implementation:**
- New macro `REG_CMD_TOOL_ICON` with 9 parameters
- Parameters: id, label_tr, path, order, sep, phase, shortcut, icon, callback
- Sets cmd.icons = icon parameter

**Acceptance:**
- ✅ Macro compiles without errors
- ✅ Icon parameter correctly assigned to Command

---

### Task 4: Assign Icons to Toolbar Commands (60 min)

**Files:**
- Modify `src/gui/main_window.cpp` (registerCommands)

**Implementation:**
- Replace REG_CMD_TOOL with REG_CMD_TOOL_ICON for ~20 commands
- Assign appropriate icons:

**File Toolbar (5 commands):**
- file.new → SP_FileIcon
- file.open → SP_DirOpenIcon
- file.save → SP_DialogSaveButton
- file.saveAs → Placeholder "A" blue
- file.close → SP_DialogCancelButton

**Edit Toolbar (6 commands):**
- edit.undo → SP_ArrowBack
- edit.redo → SP_ArrowForward
- edit.cut → Placeholder "X" red
- edit.copy → Placeholder "C" blue
- edit.paste → Placeholder "V" green
- edit.selectAll → Placeholder "S" purple

**Book Toolbar (4 commands):**
- book.newChapter → Placeholder "CH" green
- book.newCharacter → Placeholder "P" orange (Person)
- book.newLocation → Placeholder "L" blue
- book.properties → Placeholder "i" gray (info)

**View Toolbar (5 commands):**
- view.navigator → Placeholder "N" blue
- view.properties → Placeholder "P" green
- view.search → Placeholder "S" orange
- view.assistant → Placeholder "A" purple
- view.log → Placeholder "L" gray

**Tools Toolbar (3 commands):**
- tools.spellcheck → Placeholder "ABC" red
- tools.stats.wordCount → Placeholder "#" blue
- tools.focus.normal → Placeholder "F" green

**Acceptance:**
- ✅ All toolbar commands have icons assigned
- ✅ Icons display in toolbar (24x24)
- ✅ No broken/missing icons

---

### Task 5: Integrate ToolbarManager into MainWindow (45 min)

**Files:**
- Modify `include/kalahari/gui/main_window.h`
- Modify `src/gui/main_window.cpp`

**Header Changes:**
- Remove: `QToolBar* m_fileToolbar;`
- Add: `ToolbarManager* m_toolbarManager;`
- Add: `#include "kalahari/gui/toolbar_manager.h"`

**Implementation Changes:**
- Constructor: Initialize `m_toolbarManager(nullptr)`
- `createToolbars()`:
  - Create ToolbarManager
  - Call `m_toolbarManager->createToolbars(registry)`
- `closeEvent()`:
  - Call `m_toolbarManager->saveState()`
- `showEvent()`:
  - Call `m_toolbarManager->restoreState()` (if m_firstShow)

**Acceptance:**
- ✅ MainWindow compiles
- ✅ 6 toolbars appear on startup
- ✅ All toolbars visible by default
- ✅ Toolbar positions in Top area

---

### Task 6: Add View > Toolbars Submenu (30 min)

**Files:**
- Modify `src/gui/main_window.cpp` (createMenus)

**Implementation:**
- In createMenus(), after creating View menu:
  - Call `m_toolbarManager->createViewMenuActions(m_viewMenu)`
- ToolbarManager creates:
  - Separator before "Toolbars" section
  - 6 checkable actions (File Toolbar, Edit Toolbar, etc.)
  - Each action toggles corresponding toolbar
  - Checkmark synced with visibility

**Menu Structure:**
```
VIEW
├─ Panels ▼
│  ├─ □ Navigator
│  ├─ □ Properties
│  └─ ...
├─ ───────────────
├─ Toolbars ▼
│  ├─ ☑ File Toolbar
│  ├─ ☑ Edit Toolbar
│  ├─ ☑ Book Toolbar
│  ├─ ☑ View Toolbar
│  └─ ☑ Tools Toolbar
└─ ───────────────
   └─ Reset Layout
```

**Acceptance:**
- ✅ View > Toolbars submenu appears
- ✅ 6 checkable actions present
- ✅ Clicking action toggles toolbar visibility
- ✅ Checkmark updates when toolbar shown/hidden

---

### Task 7: Implement State Persistence (45 min)

**Files:**
- Modify `src/gui/toolbar_manager.cpp`

**Implementation:**
- `saveState()`:
  - For each toolbar: save visibility to QSettings "Toolbars/{id}/visible"
  - Qt automatically saves positions in windowState
- `restoreState()`:
  - For each toolbar: restore visibility from QSettings
  - Default to true if key doesn't exist
  - Qt automatically restores positions from windowState

**QSettings Keys:**
```
[Toolbars]
file/visible=true
edit/visible=true
book/visible=true
view/visible=true
tools/visible=true
```

**Acceptance:**
- ✅ Toolbar visibility saved on close
- ✅ Toolbar positions saved on close
- ✅ Toolbars restored on next launch
- ✅ Floating toolbars restored correctly

---

### Task 8: Testing & Debugging (45 min)

**Test Cases:**
1. **Toolbar Creation:**
   - ✅ All 6 toolbars appear
   - ✅ Correct commands in each toolbar
   - ✅ Icons display correctly
   - ✅ Tooltips on hover

2. **Toolbar Movement:**
   - ✅ Drag toolbar to different position
   - ✅ Double-click title to float
   - ✅ Close and reopen - position restored

3. **Toolbar Visibility:**
   - ✅ View > Toolbars > uncheck "Edit Toolbar"
   - ✅ Edit toolbar disappears
   - ✅ Close and reopen - Edit toolbar still hidden
   - ✅ Re-check "Edit Toolbar" - toolbar reappears

4. **Command Execution:**
   - ✅ Click toolbar button executes command
   - ✅ Status bar shows feedback

5. **Edge Cases:**
   - ✅ Hide all toolbars - can re-show via View menu
   - ✅ Float all toolbars - restore on restart
   - ✅ Reset Layout - toolbars return to default

**Acceptance:**
- ✅ All test cases pass
- ✅ No crashes or errors
- ✅ Consistent behavior across sessions

---

### Task 9: Update CHANGELOG.md (15 min)

**Files:**
- Modify `CHANGELOG.md`

**Entry:**
```markdown
- **Task #00019:** Toolbar Manager System - 2025-11-22
  - Replaced single toolbar with ToolbarManager managing 6 toolbars
  - Toolbars: File (5 actions), Edit (6), Book (4), View (5), Tools (3)
  - Icon system: Qt Standard Icons + Placeholder icons (colored squares)
  - View > Toolbars submenu with toggle actions for each toolbar
  - State persistence: toolbar positions and visibility saved to QSettings
  - All toolbars visible by default, movable, floatable, dockable
  - Files: toolbar_manager.h/cpp (450 LOC), command.h/cpp (IconSet factories)
  - Modified: main_window.h/cpp, register_commands.hpp (REG_CMD_TOOL_ICON)
  - OpenSpec: Change ID 00019-toolbar-manager
```

**Acceptance:**
- ✅ CHANGELOG entry added to [Unreleased] section
- ✅ Accurate description of changes

---

### Task 10: Commit Changes (10 min)

**Git Operations:**
1. Stage all modified files
2. Commit with message:
   ```
   feat(toolbars): Add ToolbarManager with 6 toolbars and icons (Task #00019)

   - ToolbarManager class manages 6 toolbars: File, Edit, Book, View, Tools
   - IconSet factory methods: fromStandardIcon(), createPlaceholder()
   - View > Toolbars submenu for visibility control
   - State persistence (positions + visibility via QSettings)
   - All toolbars visible by default, movable, floatable

   Files: toolbar_manager.h/cpp, command.h/cpp, main_window.h/cpp,
          register_commands.hpp, CHANGELOG.md

   🤖 Generated with Claude Code
   Co-Authored-By: Claude <noreply@anthropic.com>
   ```

**Acceptance:**
- ✅ All files committed
- ✅ Clean working directory

---

## Summary

**Total Tasks:** 10
**Estimated Time:** ~6-7 hours
**Status:** Ready to start

**Critical Path:**
1. Task 1 (ToolbarManager) → Task 5 (Integration)
2. Task 2 (IconSet) → Task 4 (Icon Assignment)
3. Task 3 (Macro) → Task 4 (Icon Assignment)
4. Task 6 depends on Task 5
5. Task 7 depends on Task 5
6. Task 8 depends on all above
7. Tasks 9-10 final

**Generated with Claude Code** | Task #00019 | 2025-11-22
