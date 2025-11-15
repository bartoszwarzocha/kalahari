# Task #00041: Theme Restart Dialog Verification

⚠️ **OBSOLETE** - This task is merged into BWX SDK verification tasks.
See ROADMAP.md § 1.3 and Task #00049 for new approach.

**Status:** ~~📋 Planned~~ → **OBSOLETE** (2025-11-15)
**Reason:** Theme dialog verification merged into comprehensive Settings System Verification
**Replaced by:** Task #00049 (Settings System Verification) - includes theme dialog tests
**Priority:** ~~P2 (MEDIUM)~~ (archived)
**Estimated:** ~~30 minutes~~ (covered by #00049)
**Dependencies:** None

---

## Problem

Verify that theme change (Light/Dark) shows restart dialog with correct behavior:
1. User changes theme in Settings
2. Clicks Apply or OK
3. Dialog appears: "Theme change requires restart. Restart now?"
4. User can choose Yes/No
5. If Yes: application exits cleanly
6. On next startup: new theme is applied

---

## Root Cause Analysis

This feature was implemented in earlier tasks. Need to verify:
- Dialog appears when theme changes
- User can cancel (No button)
- Application exits gracefully (Yes button)
- Settings persist before exit
- New theme loads on next startup

---

## Testing Plan

### Test Case 1: Theme Change + Apply
1. Settings → Appearance → Theme = Dark
2. Click Apply (NOT OK)
3. Verify: Restart dialog appears
4. Click No
5. Verify: Dialog closes, Settings still open, old theme active

### Test Case 2: Theme Change + OK
1. Settings → Appearance → Theme = Dark
2. Click OK
3. Verify: Settings dialog closes, then restart dialog appears
4. Click No
5. Verify: Old theme still active

### Test Case 3: Restart Accepted
1. Change theme to Dark
2. Click Apply
3. Dialog: Click Yes (Restart Now)
4. Verify: Application exits cleanly (no crash)
5. Restart application manually
6. Verify: Dark theme is active

### Test Case 4: No Change = No Dialog
1. Open Settings
2. Theme already = Light
3. Select Light again (no change)
4. Click Apply
5. Verify: NO restart dialog appears

### Test Case 5: Multiple Changes
1. Light → Dark → Click Apply → Cancel dialog
2. Dark → Light → Click Apply → Cancel dialog
3. Light → Dark → Click Apply → Accept dialog
4. Verify: Final choice (Dark) persists

---

## Acceptance Criteria

- [ ] Restart dialog appears ONLY when theme changes
- [ ] Dialog has clear message: "Restart required"
- [ ] User can cancel (No button)
- [ ] User can accept (Yes button → app exits)
- [ ] Theme persists to settings.json before exit
- [ ] New theme loads correctly on next startup

---

## Expected Issues

If dialog doesn't appear:
- `OnApply()` doesn't check if theme changed
- Event not triggering restart logic

If app doesn't exit:
- `wxApp::ExitMainLoop()` not called
- Need `Close(true)` on MainWindow

If theme doesn't persist:
- `SettingsManager::save()` not called before exit

---

## Edge Cases

- User changes theme multiple times before clicking Apply
- User changes theme, clicks Apply, cancels dialog, changes again
- User closes Settings Dialog without clicking OK (should NOT show restart dialog)

---

## Rollback Plan

This is verification only.
If bugs found, create follow-up task.

---

**Created:** 2025-11-09
