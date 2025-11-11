# Task #00027: Font Scaling Persistence Verification

**Status:** 📋 Planned
**Priority:** P2 (MEDIUM)
**Estimated:** 30-60 minutes
**Dependencies:** #00026 (Apply must work first)

---

## Problem

Verify that font scaling setting:
1. Persists to `settings.json` after Apply + OK
2. Loads correctly on application startup
3. UI applies scaled fonts immediately on launch

---

## Root Cause Analysis

- `SettingsManager` should save `ui.font_scale` to JSON
- On startup, `MainWindow` should read and apply scale
- Default value: 1.0 (100% normal size)

---

## Solution

**Verification only - no code changes expected.**

Verify existing flow:
1. Save flow: SettingsDialog → OnApply → SettingsManager → settings.json
2. Load flow: MainWindow ctor → SettingsManager::load → ApplyFontScaling

If load flow missing:
- Add `UpdateAllFonts(scale)` call in MainWindow constructor

---

## Testing Plan

### Test Case 1: Save to JSON
1. Set font scaling to 1.5x
2. Click OK
3. Open `settings.json`
4. Verify: `"ui": { "font_scale": 1.5 }`

### Test Case 2: Load from JSON
1. Close application
2. Edit `settings.json`: `"font_scale": 0.8`
3. Start application
4. Verify: All UI text is smaller (80% size)

### Test Case 3: Default Value
1. Delete `settings.json`
2. Start application
3. Verify: Normal font size (1.0x scale)

### Test Case 4: Invalid Value
1. Edit `settings.json`: `"font_scale": 99.9`
2. Start application
3. Verify: Falls back to 1.0x, logs warning

### Test Case 5: Edge Values
1. Set to 0.8x (min), restart
2. Verify: Text is smallest allowed size
3. Set to 2.0x (max), restart
4. Verify: Text is largest allowed size

---

## Acceptance Criteria

- [ ] Font scale persists to `settings.json`
- [ ] Application loads correct scale on startup
- [ ] UI applies scale before showing main window
- [ ] Invalid values (< 0.5 or > 3.0) trigger warning + default
- [ ] Logs show: "Loaded font scale: 1.5x"

---

## Expected Issues

If persistence fails:
1. `UpdateAllFonts()` not called in MainWindow constructor
2. Settings loaded after UI creation (too late)
3. JSON write error

If load fails:
- Add explicit call in MainWindow::OnCreate or constructor

---

## Rollback Plan

Verification only - no rollback needed.
If bugs found, create follow-up task.

---

**Created:** 2025-11-09
