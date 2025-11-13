# Task #00037: Font Scaling Live Preview

**Status:** 📋 Planned
**Priority:** P1 (HIGH)
**Estimated:** 1-2 hours
**Dependencies:** None

---

## Problem

In Appearance Settings Panel, the "Text scaling" spinner (0.8x - 2.0x) doesn't update the example text in real-time. User spins the control but sees no immediate feedback.

---

## Root Cause Analysis

1. `wxSpinCtrlDouble` has `EVT_SPINCTRLDOUBLE` event
2. Currently NOT bound in `AppearanceSettingsPanel`
3. Example text (`m_fontScalingExample`) needs manual font size update

---

## Solution

**Add live preview event handler:**

1. Bind `EVT_SPINCTRLDOUBLE` to handler method
2. In handler: read spinner value
3. Calculate new font size: `baseFontSize * spinnerValue`
4. Update `m_fontScalingExample->SetFont(newFont)`
5. Call `m_fontScalingExample->Refresh()`

---

## Implementation Plan

### Files to Modify
- `src/gui/appearance_settings_panel.h` (add handler declaration)
- `src/gui/appearance_settings_panel.cpp` (implement handler)

### Code Changes

**appearance_settings_panel.h:**
```cpp
private:
    void OnFontScalingChange(wxSpinDoubleEvent& event);
```

**appearance_settings_panel.cpp:**
```cpp
// In constructor, after creating m_fontScaling:
m_fontScaling->Bind(wxEVT_SPINCTRLDOUBLE,
                    &AppearanceSettingsPanel::OnFontScalingChange,
                    this);

void AppearanceSettingsPanel::OnFontScalingChange(wxSpinDoubleEvent& event)
{
    double scale = event.GetValue();

    // Base font size (from system or default)
    int baseSize = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT).GetPointSize();
    int newSize = static_cast<int>(baseSize * scale);

    // Update example text
    wxFont newFont = m_fontScalingExample->GetFont();
    newFont.SetPointSize(newSize);
    m_fontScalingExample->SetFont(newFont);
    m_fontScalingExample->Refresh();

    wxLogDebug("Font scaling preview: %.1fx (size %d)", scale, newSize);
}
```

---

## Acceptance Criteria

- [ ] Spin font scaling control up/down
- [ ] Example text immediately changes size
- [ ] No need to click Apply for preview
- [ ] Scale values 0.8x - 2.0x work correctly
- [ ] Text remains readable at all scales

---

## Testing Steps

1. **Open Settings:** Appearance panel
2. **Spin Up:** 1.0x → 1.5x
   - Expected: Example text becomes larger
3. **Spin Down:** 1.5x → 0.8x
   - Expected: Example text becomes smaller
4. **Edge Cases:**
   - Min: 0.8x (text should still be readable)
   - Max: 2.0x (text should not overflow panel)

---

## Edge Cases

- **Very small fonts (0.8x):** Ensure text doesn't become unreadable
- **Very large fonts (2.0x):** Panel should expand or scroll if needed
- **Rapid spinning:** No lag or visual glitches

---

## Rollback Plan

If implementation causes issues:
- Remove event binding
- Keep static example text (no preview)

---

**Created:** 2025-11-09
