# Tasks for #00028: Complete Theme Styling System

## Phase 1: QPalette Completion

- [x] Add `toolTipBase` field to Theme::Palette struct
- [x] Add `toolTipText` field to Theme::Palette struct
- [x] Add `placeholderText` field to Theme::Palette struct
- [x] Add `brightText` field to Theme::Palette struct
- [x] Update `Theme::fromJson()` to parse new palette roles
- [x] Update `toQPalette()` to set all 20 color roles
- [x] Update `Light.json` with new palette entries
- [x] Update `Dark.json` with new palette entries

## Phase 2: StyleSheet Class Implementation

- [x] Create `include/kalahari/core/stylesheet.h`
- [x] Create `src/core/stylesheet.cpp`
- [x] Implement `generate(const Theme&)` - main entry point
- [x] Implement tooltip-only QSS generation (simplified from original 12 widget styles)
- [x] Add color utility functions (darken, lighten, withAlpha)
- [x] Add to CMakeLists.txt

## Phase 3: Widget QSS Coverage (SIMPLIFIED)

**Key Discovery:** QPalette + Fusion style handles ALL widget colors automatically.
The original plan for 12 widget-specific QSS generators was unnecessary.

- [x] Implement `generateTooltipStyle()` (the only essential QSS)
- [x] Remove unnecessary QSS for scrollbars (QPalette handles it)
- [x] Remove unnecessary QSS for comboboxes (QPalette handles it)
- [x] Remove unnecessary QSS for spinboxes (QPalette handles it)
- [x] Remove unnecessary QSS for other widgets (QPalette handles it)
- [x] Simplified stylesheet.cpp from ~445 to ~68 lines

## Phase 4: ThemeManager Integration

- [x] Add `#include "stylesheet.h"` to theme_manager.cpp
- [x] Add `themeStyleChanged()` signal to ThemeManager header
- [x] Call `StyleSheet::generate()` in `applyTheme()`
- [x] Apply via `QApplication::setStyleSheet()`
- [x] Emit `themeStyleChanged()` after stylesheet application
- [x] Regenerate and reapply QSS in `applyColorOverrides()`

## Phase 5: Settings UI Expansion

- [x] Add "UI Colors" group (4 colors: toolTipBase, toolTipText, placeholderText, brightText)
- [x] Add "Palette Colors" group (16 colors)
- [x] Wrap Theme page in QScrollArea
- [x] Per-theme color storage in SettingsManager
- [x] Live preview on color change
- [x] "Restore Defaults" resets all colors

## Phase 6: Hardcoded Styles Cleanup

- [x] Refactor hardcoded colors in settings_dialog.cpp
- [x] Refactor hardcoded colors in color_config_widget.cpp
- [x] Refactor hardcoded colors in icon_downloader_dialog.cpp
- [x] Refactor hardcoded colors in main_window.cpp (dock panel titles)

## Additional Fixes Applied

- [x] Fixed ComboBox/SpinBox arrows (removed interfering custom QSS)
- [x] Fixed dropdown selection colors (correct QPalette roles)
- [x] Added icons to dock panel title bars
- [x] Panel icons refresh on theme change
- [x] Fixed ColorConfigWidget alignment (40/60 split)
- [x] Theme switching now loads saved user colors per theme

## Testing

- [x] Light theme - all widgets styled correctly
- [x] Dark theme - all widgets styled correctly
- [x] Theme switching - all widgets update immediately
- [x] Color override - individual colors update in real-time
- [x] "Restore Defaults" - all colors reset properly
- [x] Tooltips display with theme colors
- [x] Build succeeds on Windows (Debug)

## Documentation

- [x] Update proposal.md status to DEPLOYED
- [x] Update tasks.md to reflect simplified implementation
- [x] Update CHANGELOG.md with [Unreleased] entry
- [x] Verify ROADMAP.md section 1.7 is complete

---

## Progress Summary

**Original Plan:** 87 tasks across 8 phases (complex QSS for 12 widget types)

**Final Implementation:** 42 tasks (simplified architecture)
- QPalette (20 roles) + Fusion style = handles ALL widget colors automatically
- QSS = only for tooltips (the only exception)
- Reduced stylesheet.cpp from ~445 to ~68 lines

**All tasks completed.**
