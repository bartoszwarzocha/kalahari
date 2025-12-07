# Tasks: Centralized Theme Color Configuration

**Status:** DEPLOYED

**Created:** 2025-11-27
**Last Updated:** 2025-12-07
**Deployed:** 2025-12-07

---

## Summary

Create a centralized UI for configuring all theme colors in one place (Appearance -> Theme), with per-theme storage in settings.json and "Reset to Theme Defaults" functionality.

## Phase 1: Color Storage Architecture

- [x] Extend SettingsManager with per-theme log color methods
  - `getLogColorForTheme(themeName, colorKey, defaultColor)`
  - `setLogColorForTheme(themeName, colorKey, color)`
  - Keys: trace, debug, info, warning, error, critical, background

- [x] Update ThemeManager to load user color overrides
  - Check settings.json for theme-specific overrides
  - Fall back to Theme JSON defaults

- [x] Add log colors to SettingsData struct
  - QColor fields for each log level
  - Include in operator!= comparison

## Phase 2: ColorConfigWidget

- [x] Create reusable color picker widget
  - QPushButton showing current color
  - Click opens QColorDialog
  - Displays hex code next to button
  - Emits colorChanged(QColor) signal

- [x] Add to `include/kalahari/gui/widgets/color_config_widget.h`
- [x] Add to `src/gui/widgets/color_config_widget.cpp`

## Phase 3: Theme Color Editor UI

- [x] Redesign Appearance/Theme page
  - Theme selector at top (existing)
  - Dynamic color groups below

- [x] Build groups from Theme struct
  - "Icon Colors" group (primary, secondary)
  - "Log Panel Colors" group (6 levels + background)

## Phase 4: Reset to Defaults

- [x] Add "Reset to Theme Defaults" button
- [x] Implement reset logic

## Phase 5: Integration & Testing

- [x] Build Debug: OK
- [x] Tests: PASS
- [x] Manual testing: Theme page functional with Icon Colors and Log Panel Colors groups

## Files Created

| File | Status |
|------|--------|
| `include/kalahari/gui/widgets/color_config_widget.h` | CREATED |
| `src/gui/widgets/color_config_widget.cpp` | CREATED |

## Files Modified

| File | Status |
|------|--------|
| `include/kalahari/core/settings_manager.h` | MODIFIED - 4 log color methods |
| `src/core/settings_manager.cpp` | MODIFIED - log color implementation |
| `include/kalahari/gui/settings_data.h` | MODIFIED - 7 log color fields |
| `include/kalahari/gui/settings_dialog.h` | MODIFIED - ColorConfigWidget pointers |
| `src/gui/settings_dialog.cpp` | MODIFIED - Theme page with color groups |
| `src/CMakeLists.txt` | MODIFIED - added color_config_widget |

## Dependencies

- OpenSpec #00022 (Theme System Foundation) - DEPLOYED
- OpenSpec #00025 (Theme-Icon Integration) - DEPLOYED
- OpenSpec #00024 (Log Panel Enhanced) - DEPLOYED

## Implementation Notes

Feature was originally implemented but lost when code on branch `feature/claude-workflow-redesign` was never merged to main. Restored 2025-12-07.
