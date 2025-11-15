# Task #00040: Dynamic Text Wrapping Verification

⚠️ **OBSOLETE** - This task is merged into BWX SDK verification tasks.
See ROADMAP.md § 1.3 and Task #00049 for new approach.

**Status:** ~~📋 Planned~~ → **OBSOLETE** (2025-11-15)
**Reason:** Text wrapping verification merged into comprehensive Settings System Verification
**Replaced by:** Task #00049 (Settings System Verification) - includes text wrapping tests
**Priority:** ~~P2 (MEDIUM)~~ (archived)
**Estimated:** ~~30-60 minutes~~ (covered by #00049)
**Dependencies:** None

---

## Problem

Verify that long help text in Settings panels wraps correctly when:
1. Window is resized smaller
2. Font scaling is increased (text becomes longer)
3. Settings Dialog is opened on small screens

Example: "Text scaling affects all UI elements..." should wrap, not overflow.

---

## Root Cause Analysis

- `wxStaticText` has `wxST_NO_AUTORESIZE` flag
- `Wrap(width)` method called with fixed pixel width
- Should use `-1` (auto-wrap) or percentage-based width

---

## Solution

**Verification task - check if wrapping already works.**

If NOT working:
1. Find all `wxStaticText` in settings panels
2. Remove fixed `Wrap(400)` calls
3. Use `Wrap(-1)` or dynamic calculation:
   ```cpp
   text->Wrap(GetClientSize().GetWidth() - 40);
   ```
4. Bind `EVT_SIZE` to recalculate wrap on resize

---

## Testing Plan

### Test Case 1: Normal Window Size
1. Open Settings → Appearance
2. Read help text under Font Scaling
3. Verify: Text wraps to multiple lines, no horizontal scroll

### Test Case 2: Small Window
1. Resize Settings Dialog to 400px width
2. Verify: Text wraps to more lines, still readable

### Test Case 3: Large Font + Small Window
1. Set font scaling to 2.0x
2. Resize window to 500px width
3. Verify: Text wraps correctly, no overflow

### Test Case 4: Window Resize
1. Open Settings Dialog
2. Drag corner to resize smaller/larger
3. Verify: Text re-wraps dynamically

---

## Acceptance Criteria

- [ ] Help text wraps on small windows (< 600px)
- [ ] Text re-wraps when window resized
- [ ] Text re-wraps when font scaling changes
- [ ] No horizontal scrollbars appear
- [ ] Text remains readable at all sizes

---

## Expected Issues

If wrapping fails:
1. Fixed `Wrap(400)` hardcoded → change to dynamic
2. No `EVT_SIZE` binding → add resize handler
3. `wxST_NO_AUTORESIZE` prevents wrap → remove flag

---

## Rollback Plan

This is verification only.
If bugs found, create follow-up task: "Fix dynamic text wrapping"

---

**Created:** 2025-11-09
