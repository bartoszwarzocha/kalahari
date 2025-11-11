# Task #00021: Fix Windows Settings Dialog Crash

**Status:** 🧪 AWAITING VERIFICATION (Fix implemented, user testing pending)
**Priority:** P0 (CRITICAL)
**Estimated:** 1-2 hours | **Actual:** 1 hour (implementation)
**Implemented:** 2025-11-09
**Commit:** 258210b "fix(gui): Implement exception handling system and fix Settings Dialog crash"
**Verification Status:** CI/CD passing, manual testing required
**Dependencies:** None

---

## Problem

Settings Dialog crashes immediately when opened on Windows. Linux and macOS work correctly.

**Error Location:** `SettingsDialog` constructor during icon initialization for tree control.

---

## Root Cause Analysis

1. `IconRegistry::getSizeForClient("settings_tree")` returns invalid size on Windows
2. `wxImageList` creation with invalid size causes crash
3. Defensive null checks missing

**Suspected Code:**
```cpp
// settings_dialog.cpp:148-156
int iconSize = IconRegistry::getInstance().getSizeForClient("settings_tree");
wxImageList* imageList = new wxImageList(iconSize, iconSize);
// If iconSize is 0 or invalid → CRASH
```

---

## Solution

Replace dynamic icon size lookup with fixed safe value:

1. Use hardcoded 16x16 for tree icons (standard size)
2. Add defensive checks: validate bitmap before adding to ImageList
3. Add fallback: if bitmap invalid, use empty/placeholder icon
4. Log warnings instead of crashing

---

## Implementation Plan

### Files to Modify
- `src/gui/settings_dialog.cpp` (lines ~148-156 in constructor)

### Code Changes

```cpp
// BEFORE (CRASHES):
int iconSize = IconRegistry::getInstance().getSizeForClient("settings_tree");
wxImageList* imageList = new wxImageList(iconSize, iconSize);

// AFTER (SAFE):
const int TREE_ICON_SIZE = 16;  // Fixed safe size
wxImageList* imageList = new wxImageList(TREE_ICON_SIZE, TREE_ICON_SIZE);

// For each icon added:
wxBitmap icon = loadIcon("settings");
if (icon.IsOk() && icon.GetWidth() == TREE_ICON_SIZE) {
    imageList->Add(icon);
} else {
    // Fallback: empty bitmap
    imageList->Add(wxBitmap(TREE_ICON_SIZE, TREE_ICON_SIZE));
    wxLogWarning("Failed to load settings tree icon, using placeholder");
}
```

---

## Acceptance Criteria

- [ ] Settings Dialog opens on Windows without crash
- [ ] Tree icons display correctly (or show placeholders if unavailable)
- [ ] No errors in diagnostic logs
- [ ] Linux and macOS behavior unchanged (regression test)

---

## Testing Steps

### Test on Windows:
1. Build application on Windows (CI/CD or local)
2. Run application
3. File → Settings (or Ctrl+,)
4. Expected: Dialog opens successfully
5. Verify: Tree icons visible (or empty placeholders)
6. Check logs: No crash-related errors

### Regression Test on Linux:
1. Build on Linux
2. Open Settings Dialog
3. Verify: Still works as before

---

## Edge Cases

- Icon files missing: Should show placeholder, not crash
- ImageList fails to create: Handle gracefully
- IconRegistry returns 0: Use fixed size fallback

---

## Rollback Plan

If fix introduces new issues:
- Revert to previous `settings_dialog.cpp`
- Add temporary platform check: `#ifdef __WXMSW__` disable icons

---

**Created:** 2025-11-09
