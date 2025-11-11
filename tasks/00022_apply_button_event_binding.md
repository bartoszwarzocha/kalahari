# Task #00022: Apply Button Event Binding

**Status:** ✅ COMPLETE (2025-11-11)
**Priority:** P1 (HIGH)
**Estimated:** 1-2 hours | **Actual:** 30 minutes (fix only)
**Implemented:** 2025-11-11
**Commit:** 3088f94 "fix(gui): Use wxPostEvent to send settings to parent"
**Verified:** Event flow working correctly
**Dependencies:** #00021 (COMPLETE)

---

## Problem

Settings Dialog has "Apply" button, but changes weren't being applied to MainWindow.

**Root Cause:** Event was sent using `ProcessWindowEvent()` which delivered event to dialog itself, not to parent (MainWindow).

---

## Solution

Changed event sending from `ProcessWindowEvent(applyEvent)` to `wxPostEvent(GetParent(), applyEvent)`.

**File Modified:** `src/gui/settings_dialog.cpp:517`

**Code Change:**
```cpp
// BEFORE (BROKEN):
ProcessWindowEvent(applyEvent);  // Sent to dialog, not parent

// AFTER (FIXED):
wxPostEvent(GetParent(), applyEvent);  // Correctly sends to MainWindow
```

---

## Implementation Details

Event flow now works correctly:

1. ✅ User clicks **Apply** button in SettingsDialog
2. ✅ `onApply()` handler calls `applyChanges()` to save panel states
3. ✅ `validateSettings()` checks if all values are valid
4. ✅ Custom event `EVT_SETTINGS_APPLIED` created with new state
5. ✅ Event sent to parent using `wxPostEvent(GetParent(), event)`
6. ✅ MainWindow receives event in `onSettingsApplied()` handler
7. ✅ Settings saved to `SettingsManager`
8. ✅ Dialog remains open for further changes

---

## Acceptance Criteria

- [x] Click Apply button in Settings Dialog
- [x] Log shows: "Settings applied (dialog remains open) - event sent to MainWindow"
- [x] MainWindow receives event: "Settings -> Apply clicked - applying settings immediately"
- [x] No crashes or errors
- [x] Dialog remains open after Apply (doesn't close)
- [x] Settings persisted to SettingsManager

---

## Testing Results

**Verified (2025-11-11):**
- ✅ Apply button triggers event correctly
- ✅ MainWindow receives `EVT_SETTINGS_APPLIED`
- ✅ Settings saved to SettingsManager
- ✅ Dialog stays open
- ✅ Can click Apply multiple times

**Logs confirm:**
```
[info] Settings applied (dialog remains open) - event sent to MainWindow
[info] Settings -> Apply clicked - applying settings immediately
```

---

## Known Limitations (Separate Tasks)

**Task #00022 ONLY handles event binding.** The following are **out of scope** and require separate tasks:

1. **GUI doesn't refresh after Apply** → Task #00023 (Icon size update)
   - Icons don't change size immediately
   - Requires IconRegistry refresh + toolbar reload

2. **Font scaling preview incorrect** → Separate bug fix
   - Example text uses hardcoded base size (10pt)
   - Should use actual font size from control

3. **Theme changes require restart** → By design
   - Documented in Appearance panel
   - Requires full application restart

---

## Related Tasks

- **Task #00021:** Settings Dialog crash fix (prerequisite)
- **Task #00023:** Icon size real-time update (follow-up)
- **Task #00026:** Font scaling real-time update (follow-up)

---

## Related Files

- `src/gui/settings_dialog.cpp` - Apply button handler
- `src/gui/settings_dialog.h` - EVT_SETTINGS_APPLIED event definition
- `src/gui/main_window.cpp` - Event receiver handler

---

**Created:** 2025-11-09
**Completed:** 2025-11-11
**Total Time:** 30 minutes (event routing fix only)
