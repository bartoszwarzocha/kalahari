# Task #00008: Settings Dialog with Diagnostic Mode Toggle

## Context

- **Phase:** Phase 0 Week 3
- **Roadmap Reference:** ROADMAP.md - Phase 0 Foundation
- **Related Docs:**
  - **[project_docs/08_gui_design.md](../project_docs/08_gui_design.md)** - **Settings Dialog section** (full architecture & design rationale)
  - tasks/00006_command_line_parser_and_diagnostic_mode.md (current diagnostic implementation)
  - project_docs/03_architecture.md (MVP pattern, GUI structure)
- **Dependencies:**
  - Task #00006 (Diagnostic mode infrastructure exists)
  - Settings system already implemented (SettingsManager)

## Objective

Replace the problematic "Restart in Diagnostic Mode" mechanism (which causes terminal hanging issues on Linux/VirtualBox) with an elegant in-session toggle accessible via Settings Dialog.

**Key Goals:**

1. Remove wxExecute-based restart mechanism (source of terminal bugs)
2. Create Settings Dialog with **tree-based navigation** (wxTreeCtrl + wxScrolledWindow)
3. Add runtime-only "Enable Diagnostic Options" toggle in Advanced → Diagnostics
4. Implement confirmation dialog for safety
5. Dynamic show/hide of Diagnostics menu without restart
6. Keep `--diag` CLI flag for command-line startup
7. Clean up terminal reset code from onExit()

## Architecture Overview

**IMPORTANT:** Full architectural design, tree structure, and implementation details are documented in:
**[project_docs/08_gui_design.md - Settings Dialog Section](../project_docs/08_gui_design.md#settings-dialog)**

**Quick Summary:**

- **Layout:** wxSplitterWindow (tree 280px | scrollable panel)
- **Navigation:** wxTreeCtrl with hierarchical categories
- **Phase 0:** Only Advanced → Diagnostics implemented
- **Future:** Expandable to 50+ settings categories across all phases
- **Why tree?** Scalable, hierarchical, industry standard (VS Code, FreeCAD, Eclipse)

## Implementation Checklist

### Phase 1: Remove Old Restart Mechanism

- [ ] Remove `onHelpRestartDiagnostic()` from MainWindow
- [ ] Remove "Help → Restart in Diagnostic Mode" menu item
- [ ] Remove terminal reset code from `kalahari_app.cpp::OnExit()`
  - Remove `resetTerminalState()` function
  - Remove `#include <termios.h>` and related code
  - Keep simple `std::_Exit(0)` on Linux (VirtualBox workaround)
- [ ] Test: Single session exits cleanly, terminal prompt returns immediately

### Phase 2: Create Settings Dialog Skeleton

- [ ] Create `src/gui/settings_dialog.h` (see 08_gui_design.md for class structure)
- [ ] Create `src/gui/settings_dialog.cpp`
- [ ] Implement wxSplitterWindow layout (tree 280px | scrollable panel)
- [ ] Add "File → Settings..." menu item in MainWindow (Ctrl+,)
- [ ] Update CMakeLists.txt with new files

### Phase 3: Build Tree Navigation (Phase 0 Minimal)

- [ ] Create wxTreeCtrl with icon support
- [ ] Build tree structure: Advanced → Diagnostics (only 2 nodes)
- [ ] Implement panel switching on tree selection
- [ ] Create DiagnosticsPanel with:
  - Warning header ("⚠️ Advanced Diagnostic Options")
  - Checkbox ("Enable Diagnostic Options")
  - Description text (runtime-only, not saved)
- [ ] Gray out checkbox if launched with `--diag` flag

### Phase 4: Confirmation Dialog

- [ ] Implement confirmation in `SettingsDialog::onOK()`
- [ ] Show warning dialog when enabling diagnostics
- [ ] Prevent dialog close if user clicks "No"
- [ ] Return new diagnostic mode state to MainWindow

### Phase 5: Dynamic Menu Toggle in MainWindow

- [ ] Add `void MainWindow::setDiagnosticMode(bool enabled)` method
- [ ] Rebuild menu bar to show/hide Diagnostics menu
- [ ] Call from `MainWindow::onFileSettings()` when dialog closes with OK
- [ ] Add logging for diagnostic mode changes

### Phase 6: CLI Flag Preservation

- [ ] Keep `--diag` flag in `kalahari_app.cpp::OnInit()`
- [ ] Pass `launchedWithDiagFlag` to SettingsDialog
- [ ] Disable checkbox in Settings if CLI flag was used
- [ ] Add tooltip explaining CLI override

### Phase 7: Testing & Documentation

- [ ] **Manual Testing:**
  - [ ] Settings Dialog opens (Ctrl+,)
  - [ ] Enable diagnostics → confirmation → Diagnostics menu appears
  - [ ] Disable diagnostics → Diagnostics menu disappears
  - [ ] Launch with `--diag` → checkbox grayed out
  - [ ] Close application → terminal returns immediately (no Enter)
  - [ ] Test on all platforms (Windows, Linux VirtualBox, macOS if available)
- [ ] **Code Quality:**
  - [ ] No compilation warnings
  - [ ] Memory leaks checked (valgrind/ASAN if available)
  - [ ] Doxygen comments added
  - [ ] Error handling for dialog creation failures
- [ ] **Documentation:**
  - [ ] Update CHANGELOG.md (Added/Changed/Removed sections)
  - [ ] Update task #00006 (mark restart mechanism as removed)
  - [ ] Verify 08_gui_design.md is up to date

## Risks & Open Questions

**Risks:**

- **R1:** Menu bar rebuild might flicker or lose focus
  - Mitigation: Test on all platforms
- **R2:** Settings Dialog adds complexity early (Phase 0)
  - Mitigation: Minimal implementation (only Advanced → Diagnostics), extensible for Phase 1
- **R3:** Users might enable diagnostics by accident
  - Mitigation: Confirmation dialog + clear warning text

**Open Questions:**

- **Q1:** Should we add "Diagnostics" toggle to View menu as well?
  - Answer: No - Settings only. Keep it "hidden" for non-technical users.
- **Q2:** Should diagnostic mode survive app minimize/restore?
  - Answer: Yes - it's session state, not window state.

## Status
- **Created:** 2025-10-28
- **Approved:** 2025-10-29 (User)
- **Started:** 2025-10-29
- **Completed:** 2025-10-29

## Implementation Notes

**Terminal Bug Fix:**

- **Old approach:** wxExecute() restart → terminal left in raw mode → bash hangs ❌
- **New approach:** In-process toggle → no restart → terminal unaffected ✅

**Design Decisions:**

See **[project_docs/08_gui_design.md - Settings Dialog](../project_docs/08_gui_design.md#settings-dialog)** for full design rationale including:

- Why runtime-only (not persisted)
- Why confirmation dialog
- Why tree navigation (not tabs)
- Why 280px tree width
- Why wxSplitterWindow

**Key Implementation Details (2025-10-29):**

1. **Settings Dialog Architecture:**
   - Layout: wxSplitterWindow with 280px tree + scrollable content panel
   - Tree navigation: wxTreeCtrl with 2 nodes (Advanced → Diagnostics)
   - Panel system: DiagnosticsPanel with wxStaticBoxSizer layout
   - State management: SettingsState struct passed between MainWindow ↔ Dialog

2. **Native Icon Implementation:**
   - Replaced emoji (⚠️) with wxArtProvider::GetBitmap(wxART_WARNING, wxART_MESSAGE_BOX)
   - Uses wxStaticBitmap in horizontal sizer with text
   - Cross-platform native appearance (Windows/macOS/Linux)
   - **CARDINAL RULE:** Never use emoji - always native system resources

3. **Confirmation Flow:**
   - onOK() checks if diagnostics enabled (and wasn't before)
   - wxMessageBox with wxYES_NO | wxICON_WARNING
   - Prevents dialog close if user clicks "No"
   - Only shown when enabling (not when disabling)

4. **Dynamic Menu Toggle:**
   - setDiagnosticMode(bool) rebuilds menu bar
   - Safely deletes old menubar after creating new one
   - Logs all state changes for debugging
   - No flicker observed on Linux

5. **CLI Flag Handling:**
   - m_launchedWithDiagFlag tracked in MainWindow
   - Passed to SettingsDialog via SettingsState
   - Checkbox disabled with tooltip when CLI flag used
   - Prevents runtime changes when launched with --diag

6. **Old Code Removal:**
   - Deleted onHelpRestartDiagnostic() (restart mechanism)
   - Deleted resetTerminalState() (terminal workaround)
   - Removed <termios.h> includes
   - Removed "Help → Restart in Diagnostic Mode" menu item

**Verification:**
- ✅ Build succeeds on Linux (no warnings)
- ✅ Settings Dialog opens with Ctrl+,
- ✅ Native warning icon visible
- ✅ Checkbox toggles diagnostic mode
- ✅ Confirmation dialog shown when enabling
- ✅ Diagnostics menu appears/disappears dynamically
- ✅ No terminal hang issues (VirtualBox workaround working)

## Related Files

**New files:**
- `src/gui/settings_dialog.h`
- `src/gui/settings_dialog.cpp`

**Modified files:**
- `src/gui/main_window.h` (add setDiagnosticMode(), onFileSettings())
- `src/gui/main_window.cpp` (implement new methods, remove old restart code)
- `src/gui/kalahari_app.cpp` (clean up onExit(), remove terminal reset)
- `src/CMakeLists.txt` (add new source files)
- `CHANGELOG.md`
- `tasks/00006_command_line_parser_and_diagnostic_mode.md` (update status)

**Files to remove code from:**
- `src/gui/main_window.cpp::onHelpRestartDiagnostic()` - DELETE
- `src/gui/kalahari_app.cpp::resetTerminalState()` - DELETE
- `src/gui/kalahari_app.cpp` - Remove `<termios.h>` includes

---

**Task Priority:** 🔴 HIGH (Fixes critical terminal bug on Linux/VirtualBox)

**Estimated Effort:** 4-6 hours
- Settings Dialog skeleton: 2h
- Dynamic menu toggle: 1h
- Confirmation flow: 1h
- Testing & cleanup: 1-2h
