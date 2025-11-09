# Task #00024: Icon Size Persistence Verification

**Status:** 📋 Planned
**Priority:** P2 (MEDIUM)
**Estimated:** 30-60 minutes
**Dependencies:** #00023 (Apply must work first)

---

## Problem

After applying icon size change and clicking OK, verify that:
1. Setting is saved to `settings.json`
2. After application restart, icon size is loaded correctly
3. UI displays correct size immediately on startup

---

## Root Cause Analysis

- `SettingsManager::save()` must be called when dialog closes
- `SettingsManager::load()` must read value on startup
- `MainWindow` must apply loaded size during initialization

---

## Solution

**Verification only - no code changes expected.**

Verify existing flow:
1. `SettingsDialog::OnOK()` → calls `OnApply()` → saves to SettingsManager
2. `SettingsManager` auto-saves to `settings.json` on change
3. On next startup: `MainWindow::ctor` reads from SettingsManager
4. Icons are created with correct size

---

## Testing Plan

### Test Case 1: Save to JSON
1. Set icon size to 32px
2. Click OK
3. Open `settings.json`
4. Verify: `"ui": { "icon_size": 32 }`

### Test Case 2: Load from JSON
1. Close application
2. Edit `settings.json`: `"icon_size": 16`
3. Start application
4. Verify: Toolbar icons are 16px

### Test Case 3: Default Value
1. Delete `settings.json`
2. Start application
3. Verify: Toolbar icons are 24px (default)

### Test Case 4: Invalid Value
1. Edit `settings.json`: `"icon_size": 9999`
2. Start application
3. Verify: Falls back to 24px, logs warning

---

## Acceptance Criteria

- [ ] Icon size persists to `settings.json` after Apply + OK
- [ ] Application loads correct icon size on startup
- [ ] Invalid values trigger fallback to default (24px)
- [ ] Logs show clear messages about loaded icon size

---

## Expected Issues

If persistence fails, possible causes:
1. `SettingsManager::save()` not called
2. `settings.json` write permission denied
3. JSON serialization error

---

## Rollback Plan

This is verification only - no rollback needed.
If bugs found, create new task to fix persistence.

---

**Created:** 2025-11-09
