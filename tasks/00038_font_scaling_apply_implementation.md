# Task #00038: Font Scaling Apply Implementation

**Status:** 📋 Planned
**Priority:** P1 (HIGH)
**Estimated:** 2-3 hours
**Dependencies:** #00022 (Apply button must work), #00037 (preview should work first)

---

## Problem

After changing font scaling and clicking Apply, the application UI fonts don't update. All panels, menus, and controls keep their original font size.

---

## Root Cause Analysis

1. Font scaling value is saved to SettingsManager
2. MainWindow receives `EVT_SETTINGS_APPLIED`
3. But there's no mechanism to update ALL UI fonts at runtime
4. wxWidgets doesn't have built-in "global font scaling" API

---

## Solution Approaches

### Option A: Update All Panels Individually (RECOMMENDED)
- Iterate through all wxAuiPaneInfo
- For each panel: call custom `ApplyFontScaling(scale)` method
- Each panel updates its own controls

### Option B: Restart Required Dialog
- Show message: "Font scaling requires restart"
- Apply on next launch (easier but worse UX)

### Option C: CSS-like System (FUTURE)
- Implement global font manager
- All panels register with manager
- Manager broadcasts font changes

**Decision:** Use Option A for MVP, plan Option C for Phase 1.

---

## Implementation Plan

### Files to Modify
- `src/gui/main_window.h` (add UpdateAllFonts method)
- `src/gui/main_window.cpp` (implement font update)
- Panel classes (add ApplyFontScaling if needed)

### Code Changes

**main_window.cpp:**
```cpp
void MainWindow::OnSettingsApplied(wxCommandEvent& event)
{
    auto& settings = SettingsManager::getInstance();

    // Icon size update (from #00023)
    // ... existing code ...

    // Font scaling update
    double fontScale = settings.getDouble("ui.font_scale", 1.0);
    UpdateAllFonts(fontScale);
}

void MainWindow::UpdateAllFonts(double scale)
{
    int baseSize = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT).GetPointSize();
    int newSize = static_cast<int>(baseSize * scale);

    wxFont newFont = GetFont();
    newFont.SetPointSize(newSize);

    // Update main window
    SetFont(newFont);

    // Update all child windows recursively
    UpdateChildFonts(this, newFont);

    // Update menu bar
    if (GetMenuBar()) {
        GetMenuBar()->SetFont(newFont);
    }

    // Update status bar
    if (GetStatusBar()) {
        GetStatusBar()->SetFont(newFont);
    }

    // Force layout refresh
    Layout();
    Refresh();

    wxLogMessage("Applied font scaling: %.1fx (size %d)", scale, newSize);
}

void MainWindow::UpdateChildFonts(wxWindow* parent, const wxFont& font)
{
    for (wxWindow* child : parent->GetChildren()) {
        child->SetFont(font);
        UpdateChildFonts(child, font);
    }
}
```

---

## Acceptance Criteria

- [ ] Change font scaling to 1.5x
- [ ] Click Apply
- [ ] All UI elements immediately update:
  - Menu bar text
  - Toolbar (if has text)
  - Panel labels
  - Status bar
  - Dialogs (next open)
- [ ] Layout remains correct (no overflow)
- [ ] No visual glitches

---

## Testing Steps

### Test Case 1: Increase Font (1.5x)
1. Settings → Appearance → Font Scaling = 1.5x
2. Click Apply
3. Verify: All UI text is larger

### Test Case 2: Decrease Font (0.8x)
1. Font Scaling = 0.8x
2. Click Apply
3. Verify: All UI text is smaller

### Test Case 3: Panel Layouts
1. Apply 2.0x scaling
2. Open Navigator Panel
3. Verify: Text fits, no overflow

### Test Case 4: New Windows
1. Apply 1.5x scaling
2. Open new dialog (e.g., About)
3. Verify: Dialog uses scaled font

---

## Edge Cases

- **Very small (0.8x):** Ensure readability
- **Very large (2.0x):** Check for layout breaks
- **Nested panels:** All children must update
- **Custom controls:** May need special handling

---

## Known Limitations

- Some native controls (wxListCtrl, wxTreeCtrl) may not scale perfectly
- Fixed-size controls may need manual adjustment
- Third-party plugins won't auto-scale (need API)

---

## Rollback Plan

If runtime font update causes issues:
- Revert to Option B (restart required)
- Show dialog: "Please restart application"

---

**Created:** 2025-11-09
