# Task #00021: Fix Windows Settings Dialog Crash

**Status:** ✅ COMPLETE (2025-11-11)
**Priority:** P0 (CRITICAL)
**Estimated:** 1-2 hours | **Actual:** 6 hours (systematic debugging)
**Implemented:** 2025-11-11
**Final Commit:** a372a50 "fix(gui): Remove EVT_SIZE handlers from all settings panels"
**Verified:** Windows + Linux (both platforms working)
**Dependencies:** None

---

## Problem

Settings Dialog crashed immediately when opened on **BOTH Windows AND Linux**.

**Initial Report (incorrect):** "Only Windows crashes"
**Actual Issue:** Both platforms crashed, crash happened after adding serialization (loading config data).

---

## Root Cause Analysis (FINAL - CORRECT)

**ROOT CAUSE:** `EVT_SIZE` event handlers in settings panels triggered premature `Layout()` + `FitInside()` calls during panel construction, before dialog had proper size.

### Timeline of Problem:

1. **Before serialization:** Panels worked fine
   - Panel constructors created controls with default values
   - No events triggered during construction
   - No premature Layout() calls

2. **After serialization (Task #00020):** Crash introduced
   - Panel constructors call `SetValue()` with data from `SettingsManager`
   - `SetValue()` triggers wxWidgets events (wxEVT_SIZE, wxEVT_SPINCTRLDOUBLE)
   - Event handlers execute **during construction**
   - `onSize()` handlers call:
     - `Layout()` on panel
     - `GetParent()->Layout()` on content panel
     - `FitInside()` on scrolled window
   - BUT: Dialog not shown yet, panels have 0x0 size
   - wxWidgets calculates layout with zero-size panels → CRASH

### Affected Panels:

All 3 panels with `EVT_SIZE` handlers:
- `AppearanceSettingsPanel` - `onSize()` for dynamic text wrapping
- `LogSettingsPanel` - `onSize()` for dynamic text wrapping
- `EditorSettingsPanel` - `onSize()` for dynamic text wrapping

**Design Flaw:** Dynamic text wrapping via `onSize()` handlers was unnecessary - wxWidgets sizers with `wxEXPAND` flag handle this automatically.

---

## Solution

**Remove all `EVT_SIZE` handlers from settings panels.**

wxWidgets sizers with proper flags (`wxEXPAND`, `wxALL`) handle layout automatically. Manual `onSize()` handlers that call `Layout()` + `FitInside()` are:
- Unnecessary (sizers do this automatically)
- Dangerous (cause recursive layout during construction)
- Bad practice (violates wxWidgets design patterns)

### Files Modified:

1. `src/gui/appearance_settings_panel.cpp` - Removed `EVT_SIZE(AppearanceSettingsPanel::onSize)`
2. `src/gui/log_settings_panel.cpp` - Removed `EVT_SIZE(LogSettingsPanel::onSize)`
3. `src/gui/editor_settings_panel.cpp` - Removed `EVT_SIZE(EditorSettingsPanel::onSize)`

**Code Change:**
```cpp
// BEFORE (CRASHED):
wxBEGIN_EVENT_TABLE(EditorSettingsPanel, wxPanel)
    EVT_SIZE(EditorSettingsPanel::onSize)  // ← CAUSED CRASH
wxEND_EVENT_TABLE()

// AFTER (WORKS):
wxBEGIN_EVENT_TABLE(EditorSettingsPanel, wxPanel)
    // EVT_SIZE removed - unnecessary, sizers handle layout
wxEND_EVENT_TABLE()
```

---

## Debug Process (Systematic Approach)

**Phase-by-phase testing with wxMessageBox checkpoints:**

1. **Phase 1:** Empty dialog (no panels) → ✅ PASSED
2. **Phase 2:** DiagnosticsPanel only → ✅ PASSED
3. **Phase 3:** + EditorSettingsPanel → ❌ FAILED (controls stacked incorrectly)
4. **Root Cause Found:** `EVT_SIZE` in EditorSettingsPanel
5. **Verification:** Removed `EVT_SIZE` from all panels → ✅ ALL WORKING

**Key Insight:** User observation "kontrolki na sobie" (controls stacked) was the breakthrough - indicated layout problem during construction, not after.

---

## Acceptance Criteria

- [x] Settings Dialog opens on Windows without crash
- [x] Settings Dialog opens on Linux without crash
- [x] All panels display correctly (Appearance, Editor, Diagnostics, Log)
- [x] Panel switching works correctly
- [x] No layout issues (controls properly sized and positioned)
- [x] Diagnostic mode toggle works
- [x] Config data loads correctly into all panels

---

## Testing Results

### Linux (VMware WSL):
- ✅ Settings Dialog opens successfully
- ✅ All 4 panels display correctly
- ✅ Panel switching smooth
- ✅ No crashes, no layout issues

### Windows:
- ✅ Settings Dialog opens successfully (reported by user)
- ✅ All panels functional

---

## Commits Related to This Task

**Incorrect attempts (wrong diagnosis):**
- `258210b` - Added defensive FitInside() checks (symptom, not root cause)
- `f449dee` - Added Layout() calls in buildTree() (wrong diagnosis)
- `b1781ff` - Tried Freeze()/Thaw() pattern (amateur fix, wrong approach)

**Debug commits (systematic approach):**
- `c2d6fb7` - PHASE 1: Empty dialog test
- `81a1ef0` - PHASE 2: DiagnosticsPanel only
- `b8e544e` - PHASE 3: EditorSettingsPanel added
- `442b31f` - Disabled EVT_SIZE in EditorSettingsPanel

**Final fix (ROOT CAUSE):**
- `a372a50` - Removed EVT_SIZE from all panels → **PROBLEM SOLVED**

---

## Lessons Learned

1. **Don't guess root cause** - Use systematic debugging (phase-by-phase testing)
2. **Listen to user observations** - "kontrolki na sobie" was key clue
3. **Question design patterns** - `onSize()` handlers were unnecessary from the start
4. **Trust wxWidgets** - Sizers with proper flags handle layout automatically
5. **Avoid manual Layout() calls** - Let wxWidgets do its job
6. **Test incrementally** - Adding one component at a time reveals exact problem

---

## Related Files

- `src/gui/settings_dialog.cpp` - Dialog structure
- `src/gui/appearance_settings_panel.cpp` - Appearance settings
- `src/gui/log_settings_panel.cpp` - Log settings
- `src/gui/editor_settings_panel.cpp` - Editor settings
- `src/gui/settings_dialog.h` - SettingsState structure

---

**Created:** 2025-11-09
**Completed:** 2025-11-11
**Total Time:** 6 hours (including incorrect attempts + systematic debugging)
