# Task #00022: Apply Button Event Binding

**Status:** 📋 Planned
**Priority:** P1 (HIGH)
**Estimated:** 1-2 hours
**Dependencies:** #00021 (Windows crash must be fixed first)

---

## Problem

Settings Dialog has "Apply" button, but clicking it does nothing. Changes are only applied when clicking OK (which closes the dialog).

**Expected Behavior:**
- User changes setting (e.g., icon size 24px → 32px)
- Clicks "Apply"
- Changes take effect immediately (without closing dialog)
- User can continue adjusting settings

---

## Root Cause Analysis

1. Apply button exists in `SettingsDialog` UI
2. But NO event handler bound to `wxID_APPLY` button
3. Each settings panel has `OnApply()` method but it's never called
4. MainWindow needs to receive notification when Apply is clicked

---

## Solution

**Implement Apply button event flow:**

1. Bind `EVT_BUTTON(wxID_APPLY)` in `SettingsDialog` constructor
2. Handler calls `OnApply()` on all settings panels:
   - `m_appearancePanel->OnApply()`
   - `m_editorPanel->OnApply()`
   - `m_logPanel->OnApply()`
3. Each panel saves its changes to `SettingsManager`
4. SettingsDialog fires custom event: `EVT_SETTINGS_APPLIED`
5. MainWindow listens for `EVT_SETTINGS_APPLIED`
6. MainWindow handler: reload settings and update UI

---

## Implementation Plan

### Files to Modify
- `src/gui/settings_dialog.h` (add event handler declaration)
- `src/gui/settings_dialog.cpp` (implement Apply handler)
- `src/gui/main_window.h` (add EVT_SETTINGS_APPLIED handler)
- `src/gui/main_window.cpp` (implement handler)

### Step 1: Define Custom Event (if not exists)

```cpp
// In some header (e.g., events.h or settings_dialog.h)
wxDECLARE_EVENT(EVT_SETTINGS_APPLIED, wxCommandEvent);

// In cpp:
wxDEFINE_EVENT(EVT_SETTINGS_APPLIED, wxCommandEvent);
```

### Step 2: SettingsDialog - Bind Apply Button

```cpp
// settings_dialog.cpp constructor:
Bind(wxEVT_BUTTON, &SettingsDialog::OnApplyButton, this, wxID_APPLY);

void SettingsDialog::OnApplyButton(wxCommandEvent& event)
{
    // Call OnApply on all panels
    m_appearancePanel->OnApply();
    m_editorPanel->OnApply();
    m_logPanel->OnApply();

    // Notify MainWindow that settings changed
    wxCommandEvent evt(EVT_SETTINGS_APPLIED);
    wxPostEvent(GetParent(), evt);

    wxLogMessage("Settings applied successfully");
}
```

### Step 3: MainWindow - Handle Settings Applied

```cpp
// main_window.cpp constructor:
Bind(EVT_SETTINGS_APPLIED, &MainWindow::OnSettingsApplied, this);

void MainWindow::OnSettingsApplied(wxCommandEvent& event)
{
    // Reload settings and update UI
    // (Specific updates in tasks #00023, #00026)
    wxLogMessage("MainWindow received settings applied event");

    // Placeholder for now:
    // - Task #00023 will add icon size update
    // - Task #00026 will add font scaling update
}
```

---

## Acceptance Criteria

- [ ] Click Apply button in Settings Dialog
- [ ] Log shows: "Settings applied successfully"
- [ ] MainWindow receives event: "MainWindow received settings applied event"
- [ ] No crashes or errors
- [ ] Dialog remains open after Apply (doesn't close)

---

## Testing Steps

1. **Open Settings:** File → Settings
2. **Make Change:** Appearance → Icon Size = 32px
3. **Click Apply** (NOT OK)
4. **Check Logs:** Should show:
   - "Settings applied successfully"
   - "MainWindow received settings applied event"
5. **Verify:** Dialog still open, can make more changes
6. **Click OK:** Dialog closes normally

---

## Edge Cases

- User clicks Apply multiple times: Should work each time
- User changes multiple panels, then clicks Apply: All panels saved
- User clicks Apply, then Cancel: Applied changes persist (correct behavior)

---

## Rollback Plan

If event binding causes issues:
- Remove event handler
- Keep old behavior (Apply button does nothing)

---

**Created:** 2025-11-09
