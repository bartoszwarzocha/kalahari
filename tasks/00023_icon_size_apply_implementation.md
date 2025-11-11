# Task #00023: Icon Size Apply Implementation

**Status:** ✅ DONE
**Priority:** P1 (HIGH)
**Estimated:** 1-2 hours
**Dependencies:** #00022 (Apply button must be connected)

---

## Problem

In Appearance Settings Panel, changing icon size from 24px to 32px and clicking Apply has no visible effect. The toolbar icons don't update until application restart.

---

## Root Cause Analysis

1. `AppearanceSettingsPanel::OnApply()` saves to `SettingsManager`
2. `MainWindow` receives `EVT_SETTINGS_APPLIED` but doesn't reload icons
3. `IconRegistry` needs to regenerate icon cache for new size
4. Toolbar needs to rebuild with new icon size

---

## Solution

**In MainWindow::OnSettingsApplied():**
1. Read new icon size from SettingsManager
2. Call `IconRegistry::setSizes(newSize, newSize)`
3. Clear existing toolbar
4. Recreate toolbar with new icons
5. Call `Layout()` to refresh

---

## Implementation Plan

### Files to Modify
- `src/gui/main_window.cpp` (MainWindow::OnSettingsApplied)

### Code Changes

```cpp
void MainWindow::OnSettingsApplied(wxCommandEvent& event)
{
    auto& settings = kalahari::core::SettingsManager::getInstance();

    // Get new icon size
    int iconSize = settings.getInt("ui.icon_size", 24);

    // Update IconRegistry
    kalahari::core::IconRegistry::getInstance().setSizes(iconSize, iconSize);

    // Recreate toolbar
    if (GetToolBar()) {
        GetToolBar()->Destroy();
    }
    CreateToolBar();

    // Refresh layout
    Layout();

    wxLogMessage("Applied icon size: %d", iconSize);
}
```

---

## Acceptance Criteria

- [x] Change icon size to 32px in Settings → Appearance
- [x] Click Apply button
- [x] Toolbar icons immediately update to 32px (no restart)
- [x] Icons are clear and properly scaled
- [x] No visual glitches

---

## Testing Steps

1. **Startup:** Open application with default 24px icons
2. **Change:** Settings → Appearance → Icon Size = 32px
3. **Apply:** Click Apply button (NOT OK)
4. **Verify:** Toolbar icons immediately become larger
5. **Edge case:** Change 32px → 16px → Apply (should work both ways)
6. **Persistence:** Close Settings, reopen, check value persisted

---

## Rollback Plan

If implementation fails:
- Revert changes to `main_window.cpp`
- Keep old behavior (restart required)

---

**Created:** 2025-11-09
