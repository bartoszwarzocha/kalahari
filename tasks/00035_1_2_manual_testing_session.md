# Task #00035: Manual Testing Session - Command Registry System

**Status:** ✅ COMPLETE (PASS with known limitations)
**Priority:** P1 (HIGH)
**Estimated:** 60-90 minutes
**Actual:** ~4 hours (including 7 critical bugfixes)
**Dependencies:** #00031 (MenuBuilder), #00032 (ToolbarBuilder), #00033 (Settings), #00034 (Documentation)
**Phase:** 1 (Core Editor)
**Zagadnienie:** 1.2 (Command Registry Architecture)
**Type:** Manual Testing
**Completed:** 2025-11-14

---

## Problem

Command Registry system is fully implemented and documented (Tasks #00031-#00034) but has not been manually tested end-to-end to verify:
- All menu items work correctly (execute commands)
- All toolbar buttons work correctly (execute commands)
- All keyboard shortcuts work correctly (execute commands)
- Enabled/disabled states update correctly based on application state
- Checked/unchecked states update correctly (Format menu toggles)
- Menu and toolbar state synchronization works
- Settings dialog opens via CommandRegistry
- All commands log execution properly

**Need:** Comprehensive manual testing session to verify the entire Command Registry system works as designed before moving to next zagadnienie.

---

## Solution

Conduct structured manual testing session covering:

1. **Menu Testing** - All menu items in File, Edit, Format menus
2. **Toolbar Testing** - All toolbar buttons
3. **Keyboard Shortcut Testing** - All registered shortcuts (Ctrl+N/O/S/Z/Y/X/C/V/B/I/U, etc.)
4. **State Management Testing** - Enable/disable logic, checked states
5. **Settings Integration Testing** - Settings command via menu/toolbar/keyboard
6. **Error Handling Testing** - Invalid command IDs, disabled commands
7. **Cross-UI Consistency Testing** - Same command works from menu/toolbar/keyboard

---

## Test Plan

### Preparation

**Prerequisites:**
- Build latest version (Task #00034 committed)
- Run from build directory: `./bin/kalahari`
- Have text document open OR no document (test both states)
- Have logging visible (check console output)

**Test environment:**
- Platform: Linux (WSL Ubuntu) - primary development environment
- Build type: Debug (verbose logging enabled)
- Configuration: Default settings.json

---

## Test Cases

### 1. Menu Testing (File Menu)

**Test 1.1: File → New**
- [ ] Click File → New
- [ ] Expected: `executeCommand("file.new")` logged
- [ ] Expected: New document created (or "Not implemented" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 1.2: File → Open**
- [ ] Click File → Open
- [ ] Expected: `executeCommand("file.open")` logged
- [ ] Expected: Open dialog appears (or "Not implemented" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 1.3: File → Save**
- [ ] Click File → Save
- [ ] Expected: `executeCommand("file.save")` logged
- [ ] Expected: Save operation executes (or "No document" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 1.4: File → Save As**
- [ ] Click File → Save As
- [ ] Expected: `executeCommand("file.saveAs")` logged
- [ ] Expected: Save As dialog appears (or "Not implemented" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 1.5: File → Settings**
- [ ] Click File → Settings
- [ ] Expected: `executeCommand("file.settings")` logged
- [ ] Expected: Settings dialog opens (SettingsDialog appears)
- [ ] Expected: NO old event handler message
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 1.6: File → Exit**
- [ ] Click File → Exit
- [ ] Expected: `executeCommand("file.exit")` logged
- [ ] Expected: Application exits gracefully
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

---

### 2. Menu Testing (Edit Menu)

**Test 2.1: Edit → Undo**
- [ ] Make change in document (type text)
- [ ] Click Edit → Undo
- [ ] Expected: `executeCommand("edit.undo")` logged
- [ ] Expected: Change undone (or "Nothing to undo" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 2.2: Edit → Redo**
- [ ] After undo, click Edit → Redo
- [ ] Expected: `executeCommand("edit.redo")` logged
- [ ] Expected: Change redone (or "Nothing to redo" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 2.3: Edit → Cut**
- [ ] Select text
- [ ] Click Edit → Cut
- [ ] Expected: `executeCommand("edit.cut")` logged
- [ ] Expected: Text cut to clipboard (or "Not implemented" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 2.4: Edit → Copy**
- [ ] Select text
- [ ] Click Edit → Copy
- [ ] Expected: `executeCommand("edit.copy")` logged
- [ ] Expected: Text copied to clipboard (or "Not implemented" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 2.5: Edit → Paste**
- [ ] After copy, click Edit → Paste
- [ ] Expected: `executeCommand("edit.paste")` logged
- [ ] Expected: Text pasted (or "Not implemented" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 2.6: Edit → Select All**
- [ ] Click Edit → Select All
- [ ] Expected: `executeCommand("edit.selectAll")` logged
- [ ] Expected: All text selected (or "Not implemented" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

---

### 3. Menu Testing (Format Menu)

**Test 3.1: Format → Bold**
- [ ] Select text (or position cursor)
- [ ] Click Format → Bold
- [ ] Expected: `executeCommand("format.bold")` logged
- [ ] Expected: Bold applied (or "Not implemented" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 3.2: Format → Italic**
- [ ] Select text
- [ ] Click Format → Italic
- [ ] Expected: `executeCommand("format.italic")` logged
- [ ] Expected: Italic applied (or "Not implemented" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 3.3: Format → Underline**
- [ ] Select text
- [ ] Click Format → Underline
- [ ] Expected: `executeCommand("format.underline")` logged
- [ ] Expected: Underline applied (or "Not implemented" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 3.4: Format → Font**
- [ ] Click Format → Font
- [ ] Expected: `executeCommand("format.font")` logged
- [ ] Expected: Font dialog appears (or "Not implemented" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 3.5: Format → Clear Formatting**
- [ ] Select formatted text
- [ ] Click Format → Clear Formatting
- [ ] Expected: `executeCommand("format.clearFormatting")` logged
- [ ] Expected: Formatting cleared (or "Not implemented" message)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

---

### 4. Toolbar Testing

**Test 4.1: Toolbar - New Button**
- [ ] Click toolbar New button (first button)
- [ ] Expected: `executeCommand("file.new")` logged
- [ ] Expected: Same behavior as File → New
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 4.2: Toolbar - Open Button**
- [ ] Click toolbar Open button
- [ ] Expected: `executeCommand("file.open")` logged
- [ ] Expected: Same behavior as File → Open
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 4.3: Toolbar - Save Button**
- [ ] Click toolbar Save button
- [ ] Expected: `executeCommand("file.save")` logged
- [ ] Expected: Same behavior as File → Save
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 4.4: Toolbar - Undo Button**
- [ ] Make change, click toolbar Undo button
- [ ] Expected: `executeCommand("edit.undo")` logged
- [ ] Expected: Same behavior as Edit → Undo
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 4.5: Toolbar - Redo Button**
- [ ] After undo, click toolbar Redo button
- [ ] Expected: `executeCommand("edit.redo")` logged
- [ ] Expected: Same behavior as Edit → Redo
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 4.6: Toolbar - Cut/Copy/Paste Buttons**
- [ ] Select text, click Cut
- [ ] Expected: `executeCommand("edit.cut")` logged
- [ ] Click Copy on selected text
- [ ] Expected: `executeCommand("edit.copy")` logged
- [ ] Click Paste
- [ ] Expected: `executeCommand("edit.paste")` logged
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 4.7: Toolbar - Bold/Italic/Underline Buttons**
- [ ] Select text, click Bold
- [ ] Expected: `executeCommand("format.bold")` logged
- [ ] Click Italic
- [ ] Expected: `executeCommand("format.italic")` logged
- [ ] Click Underline
- [ ] Expected: `executeCommand("format.underline")` logged
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 4.8: Toolbar - Separators**
- [ ] Verify toolbar has separators between categories:
- [ ] Separator after File commands (New/Open/Save)
- [ ] Separator after Edit commands (Undo/Redo/Cut/Copy/Paste)
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

---

### 5. Keyboard Shortcut Testing

**Test 5.1: Ctrl+N (New)**
- [ ] Press Ctrl+N
- [ ] Expected: `executeCommand("file.new")` logged
- [ ] Expected: Same behavior as File → New
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 5.2: Ctrl+O (Open)**
- [ ] Press Ctrl+O
- [ ] Expected: `executeCommand("file.open")` logged
- [ ] Expected: Same behavior as File → Open
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 5.3: Ctrl+S (Save)**
- [ ] Press Ctrl+S
- [ ] Expected: `executeCommand("file.save")` logged
- [ ] Expected: Same behavior as File → Save
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 5.4: Ctrl+Z (Undo)**
- [ ] Make change, press Ctrl+Z
- [ ] Expected: `executeCommand("edit.undo")` logged
- [ ] Expected: Same behavior as Edit → Undo
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 5.5: Ctrl+Y (Redo)**
- [ ] After undo, press Ctrl+Y
- [ ] Expected: `executeCommand("edit.redo")` logged
- [ ] Expected: Same behavior as Edit → Redo
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 5.6: Ctrl+X/C/V (Cut/Copy/Paste)**
- [ ] Select text, press Ctrl+X
- [ ] Expected: `executeCommand("edit.cut")` logged
- [ ] Press Ctrl+C on selected text
- [ ] Expected: `executeCommand("edit.copy")` logged
- [ ] Press Ctrl+V
- [ ] Expected: `executeCommand("edit.paste")` logged
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 5.7: Ctrl+A (Select All)**
- [ ] Press Ctrl+A
- [ ] Expected: `executeCommand("edit.selectAll")` logged
- [ ] Expected: All text selected
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 5.8: Ctrl+B/I/U (Bold/Italic/Underline)**
- [ ] Select text, press Ctrl+B
- [ ] Expected: `executeCommand("format.bold")` logged
- [ ] Press Ctrl+I
- [ ] Expected: `executeCommand("format.italic")` logged
- [ ] Press Ctrl+U
- [ ] Expected: `executeCommand("format.underline")` logged
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 5.9: Ctrl+, (Settings)**
- [ ] Press Ctrl+,
- [ ] Expected: `executeCommand("file.settings")` logged
- [ ] Expected: Settings dialog opens
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

---

### 6. State Management Testing

**Test 6.1: Enabled/Disabled States - No Document**
- [ ] Close all documents (or start with no document)
- [ ] Check menu items:
  - [ ] File → Save: Should be DISABLED (grayed out)
  - [ ] Edit → Cut/Copy/Paste: Should be DISABLED
  - [ ] Format → Bold/Italic/Underline: Should be DISABLED
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 6.2: Enabled/Disabled States - With Document**
- [ ] Open or create document
- [ ] Check menu items:
  - [ ] File → Save: Should be ENABLED
  - [ ] Edit → Cut/Copy/Paste: Should be ENABLED
  - [ ] Format → Bold/Italic/Underline: Should be ENABLED
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 6.3: Checked States - Format Menu**
- [ ] Select text with bold formatting
- [ ] Check Format → Bold menu item:
  - [ ] Should have CHECKMARK (✓)
- [ ] Uncheck bold
- [ ] Check menu again:
  - [ ] Should NOT have checkmark
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 6.4: State Propagation - Menu + Toolbar Sync**
- [ ] Apply bold from menu
- [ ] Check toolbar Bold button: should appear PRESSED/HIGHLIGHTED
- [ ] Remove bold from toolbar
- [ ] Check menu: checkmark should disappear
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 6.5: Dynamic State Updates**
- [ ] Open document
- [ ] Verify Save enabled
- [ ] Save document (mark as unmodified)
- [ ] Check File → Save:
  - [ ] Should be DISABLED (no unsaved changes)
- [ ] Make change
- [ ] Check File → Save:
  - [ ] Should be ENABLED again
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

---

### 7. Settings Integration Testing

**Test 7.1: Settings via Menu**
- [ ] File → Settings
- [ ] Expected: Settings dialog opens
- [ ] Expected: All 3 panels visible (Editor, Log, Appearance)
- [ ] Expected: Apply button works
- [ ] Expected: OK button saves and closes
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 7.2: Settings via Keyboard**
- [ ] Press Ctrl+,
- [ ] Expected: Same as Test 7.1
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 7.3: Settings via Toolbar (if button exists)**
- [ ] If toolbar has Settings button, click it
- [ ] Expected: Same as Test 7.1
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked / ⬜ N/A

**Test 7.4: Settings - Command Registry Integration**
- [ ] Check console logs when opening Settings
- [ ] Expected: "File -> Settings executed via CommandRegistry" message
- [ ] Expected: NO "old event handler" message
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

---

### 8. Error Handling Testing

**Test 8.1: Invalid Command ID**
- [ ] Manually call (via debug console or test): `executeCommand("invalid.command")`
- [ ] Expected: CommandExecutionResult::CommandNotFound
- [ ] Expected: Error logged or error handler called
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 8.2: Disabled Command Execution**
- [ ] Close all documents
- [ ] Attempt to execute disabled command (e.g., file.save)
- [ ] Expected: CommandExecutionResult::CommandDisabled
- [ ] Expected: No execution, appropriate message
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 8.3: Exception Handling**
- [ ] Execute command that throws exception (if any)
- [ ] Expected: CommandExecutionResult::ExecutionFailed
- [ ] Expected: Error handler called, exception caught
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

---

### 9. Cross-UI Consistency Testing

**Test 9.1: Save Command - 3 Paths**
- [ ] Execute File → Save (menu)
- [ ] Note behavior
- [ ] Execute toolbar Save button
- [ ] Expected: Identical behavior to menu
- [ ] Execute Ctrl+S
- [ ] Expected: Identical behavior to menu/toolbar
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 9.2: Bold Command - 3 Paths**
- [ ] Select text
- [ ] Execute Format → Bold (menu)
- [ ] Undo
- [ ] Execute toolbar Bold button
- [ ] Expected: Identical behavior
- [ ] Undo
- [ ] Execute Ctrl+B
- [ ] Expected: Identical behavior
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

---

### 10. Logging Verification

**Test 10.1: Command Execution Logging**
- [ ] Execute any command (e.g., File → Save)
- [ ] Check console output:
  - [ ] "CommandRegistry: Executing command 'file.save'" (or similar)
  - [ ] Execution result logged
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 10.2: MenuBuilder Logging**
- [ ] Start application
- [ ] Check console output:
  - [ ] "MenuBuilder: Building menubar from CommandRegistry"
  - [ ] "MenuBuilder: Added menu 'file' with X items"
  - [ ] Repeat for edit, format, view, help
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

**Test 10.3: ToolbarBuilder Logging**
- [ ] Start application
- [ ] Check console output:
  - [ ] "ToolbarBuilder: Building toolbar from CommandRegistry"
  - [ ] "ToolbarBuilder: Toolbar created with X tools"
- [ ] Status: ⬜ Pass / ⬜ Fail / ⬜ Blocked

---

## Test Execution Guidelines

### Before Testing
1. Build latest version: `cmake --build build-linux-wsl -j4`
2. Verify build success: `ls bin/kalahari`
3. Start application with logging: `./bin/kalahari 2>&1 | tee test-session.log`
4. Have this task file open for checking off tests

### During Testing
- Check off each test as you complete it (⬜ → ✅ Pass or ❌ Fail)
- Note any unexpected behavior in "Issues Found" section below
- Take screenshots for critical issues
- Log all failures with reproduction steps

### After Testing
- Summarize results (X/Y tests passed)
- Create GitHub issues for any failures
- Update task file status based on results
- Commit test results to task file

---

## Issues Found

### Critical Issues (Block Release)
*None expected - log here if found*

### Major Issues (Should Fix Before Next Zagadnienie)
*None expected - log here if found*

### Minor Issues (Can Fix Later)
*None expected - log here if found*

### Enhancement Ideas (Future)
*Log any improvement ideas that come up during testing*

---

## Success Criteria

**Minimum passing threshold: 90% of tests pass**

For this task to be considered COMPLETE:
- [ ] All menu tests (File, Edit, Format) pass (15+ tests)
- [ ] All toolbar tests pass (8 tests)
- [ ] All keyboard shortcut tests pass (9 tests)
- [ ] State management tests pass (5 tests)
- [ ] Settings integration tests pass (4 tests)
- [ ] Cross-UI consistency tests pass (2 tests)
- [ ] Logging verification tests pass (3 tests)
- [ ] Critical issues: 0
- [ ] Major issues: ≤ 2
- [ ] Test results documented in this file

**If <90% pass:**
- Create separate bug fix tasks for failures
- Block next zagadnienie until fixes complete

---

## Acceptance Criteria

- [ ] Task file created (this file)
- [ ] Testing session conducted (60-90 minutes)
- [ ] All test cases executed (checkboxes filled)
- [ ] Pass/Fail status recorded for each test
- [ ] Issues documented in "Issues Found" section
- [ ] Test summary generated (X/Y passed)
- [ ] GitHub issues created for any failures
- [ ] Task file committed with results

---

## Notes

### Why Manual Testing?
- Command Registry is core infrastructure used by all UI
- MenuBuilder/ToolbarBuilder are new code (not yet battle-tested)
- State management (isEnabled/isChecked) needs verification
- Better to catch issues now than after building more on top

### Test Environment Choice
- Primary: Linux (WSL Ubuntu) - main development platform
- Secondary: Windows (VS 2026) - if time permits after Linux tests
- macOS: Not tested (no macOS access)

### Automation Future
Many of these tests can be automated in Phase 2+:
- UI automation tests (CppTest, or Python + pyautogui)
- Unit tests for CommandRegistry (already exist - 16 tests, 100% pass)
- Integration tests for MenuBuilder/ToolbarBuilder

---

**Created:** 2025-11-13
**Estimated Start:** After Task #00034
**Estimated Duration:** 60-90 minutes
**Type:** Manual Testing (one-time comprehensive verification)
