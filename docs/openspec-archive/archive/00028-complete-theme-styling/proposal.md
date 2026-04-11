# 00028: Complete Theme Styling System

## Status
DEPLOYED

## Summary

Implement a comprehensive theme styling system that provides full coverage of all Qt widgets using a hybrid QPalette + QSS (Qt Style Sheets) approach. This establishes the permanent foundation for the entire project's theming capabilities, with real-time global refresh when any color changes.

## Problem Statement

### Current Limitations

1. **Incomplete QPalette Coverage**
   - Missing palette roles: `ToolTipBase`, `ToolTipText`, `PlaceholderText`, `BrightText`
   - Tooltips appear with system colors, breaking theme consistency
   - Placeholder text in input fields uses default colors

2. **No Qt Style Sheet (QSS) Support**
   - QPalette alone cannot style all widget elements
   - Custom widget parts (scrollbar handles, combobox arrows, progress bar chunks) unstyled
   - No border control for complex widgets

3. **Hardcoded setStyleSheet() Calls in Codebase**
   ```cpp
   // Found in codebase - these break theme consistency:
   restartNote->setStyleSheet("color: #666; font-style: italic;");
   warningLabel->setStyleSheet("QLabel { color: #ff6600; ... }");
   diagNote->setStyleSheet("color: #666; margin-left: 20px;");
   m_previewWidget->setStyleSheet("background-color: #f0f0f0; ...");
   ```
   These hardcoded colors don't respond to theme changes.

4. **No Global Real-Time Refresh**
   - When theme colors change, not all widgets update immediately
   - Some require application restart

## Solution Overview

### SIMPLIFIED Architecture (Final Implementation)

After extensive testing, we discovered that **QPalette + Fusion style handles ALL widget colors automatically** - including scrollbars, comboboxes, spinboxes, and all other native widgets.

**Key insight:** Qt's Fusion style is designed to fully use QPalette colors for ALL sub-controls. The only exception is tooltips, which require explicit QSS.

```
+-------------------+     +-------------------+     +-------------------+
|   Theme JSON      |---->|   ThemeManager    |---->|   QPalette        |
|   (20 color roles)|     |   (singleton)     |     |   (covers 99%)    |
+-------------------+     +--------+----------+     +-------------------+
                                   |
                                   v
                          +-------------------+
                          |   StyleSheet      |
                          | (tooltip only ~68 |
                          |  lines of code)   |
                          +-------------------+
```

### What Was Implemented

1. **Extended QPalette** - Added 4 new color roles:
   - `toolTipBase` - Tooltip background color
   - `toolTipText` - Tooltip text color
   - `placeholderText` - Input field placeholder color
   - `brightText` - High contrast text (on dark backgrounds)

2. **Minimal StyleSheet Generator** - Only generates QSS for tooltips (~68 LOC)

3. **Real-Time Refresh Signal** - `themeStyleChanged()` signal for instant updates

4. **Settings UI Expansion** - Added UI Colors and Palette Colors groups to Settings

5. **Hardcoded Styles Cleanup** - Replaced hardcoded colors with theme-aware alternatives

## Goal

- Full visual consistency across all Qt widgets in all themes
- Real-time global refresh when any color changes (no restart needed)
- Foundation established for entire project lifetime
- Clean separation: QPalette for native widgets, QSS for custom styling
- Remove all hardcoded color references from codebase

## Scope

### Included

1. **QPalette Completion**
   - Add `ToolTipBase` (tooltip background)
   - Add `ToolTipText` (tooltip text color)
   - Add `PlaceholderText` (input field placeholder)
   - Add `BrightText` (high contrast text)

2. **StyleSheet Class Implementation**
   - New header: `include/kalahari/core/stylesheet.h`
   - New source: `src/core/stylesheet.cpp`
   - Minimal QSS generation (tooltips only)

3. **Real-Time Refresh Mechanism**
   - New signal: `ThemeManager::themeStyleChanged()`
   - Regenerate QSS on any color change
   - Apply via `QApplication::setStyleSheet()`

4. **Settings UI Expansion**
   - "UI Colors" group (4 colors)
   - "Palette Colors" group (16 colors)
   - QScrollArea for Theme page
   - Per-theme color storage

5. **Hardcoded Styles Cleanup**
   - Replaced hardcoded colors in 4 files with theme-aware colors

### Excluded

- **Editor text styling** (QSyntaxHighlighter colors - future task)
- **Custom theme creation UI** (file-based themes sufficient)
- **Animation effects** (focus on colors only)
- **Platform-specific native styling** (Fusion style only)

## Acceptance Criteria

- [x] Tooltips display with theme colors (background + text)
- [x] Scrollbars (vertical + horizontal) styled consistently (via QPalette)
- [x] ComboBox dropdowns match theme (via QPalette)
- [x] SpinBox buttons and arrows themed (via QPalette)
- [x] All color changes reflect immediately (real-time refresh works)
- [x] Settings dialog exposes all new color options
- [x] Both Light and Dark themes fully styled
- [x] No visual regressions from current state

## Technical Notes

### Final Architecture Decision

| Component | Styling Method | Reason |
|-----------|---------------|--------|
| All native widgets | QPalette + Fusion | Native support via Qt's Fusion style |
| QToolTip | QSS | Only exception - QPalette doesn't fully apply |

### Performance

- Simplified stylesheet.cpp from ~445 to ~68 lines
- No performance overhead from complex QSS

## Implementation Details

### Additional Fixes Applied

1. **ComboBox/SpinBox arrows** - Removed custom QSS that was interfering with Fusion style
2. **Dropdown selection colors** - Fixed by using correct QPalette roles
3. **Dock panel title bars** - Added icons via custom title bar widget
4. **Panel icons refresh on theme change** - Connected to themeChanged signal
5. **ColorConfigWidget alignment** - Fixed to 40/60 split
6. **Theme switching** - Now loads saved user colors per theme

### Files Modified

- `include/kalahari/core/theme.h` - Added 4 new palette roles
- `src/core/theme.cpp` - Parse new roles, extend toQPalette()
- `include/kalahari/core/theme_manager.h` - Add themeStyleChanged signal
- `src/core/theme_manager.cpp` - Apply stylesheet, emit signal
- `include/kalahari/core/stylesheet.h` - New file
- `src/core/stylesheet.cpp` - New file (~68 LOC)
- `src/gui/settings_dialog.cpp` - Extended color configuration UI
- `resources/themes/Light.json` - Added new palette entries
- `resources/themes/Dark.json` - Added new palette entries
- `src/gui/main_window.cpp` - Dock panel title bars with icons
- `src/gui/widgets/color_config_widget.cpp` - Fixed alignment
- `CMakeLists.txt` - Added new source files

## Notes

1. This task establishes the **permanent theming foundation** - once complete, adding new themed widgets only requires adding their QSS rules to StyleSheet class (though most widgets work via QPalette).

2. The hybrid QPalette + QSS approach ensures:
   - Native widgets get proper palette colors (99% of UI)
   - Tooltips get custom QSS styling (only exception)
   - Both update simultaneously on theme change

3. All QSS is **generated, not static** - ensures consistency with palette colors and enables dynamic theming.
