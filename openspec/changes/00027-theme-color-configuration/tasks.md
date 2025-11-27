# Tasks: Centralized Theme Color Configuration

**Status:** PENDING

**Created:** 2025-11-27

## Summary

Create a centralized UI for configuring all theme colors in one place (Appearance -> Theme), with per-theme storage in settings.json and "Reset to Theme Defaults" functionality.

## Phase 1: Color Storage Architecture

- [ ] Extend SettingsManager with per-theme log color methods
  - `getLogColorForTheme(themeName, colorKey, defaultColor)`
  - `setLogColorForTheme(themeName, colorKey, color)`
  - Keys: trace, debug, info, warning, error, critical, background

- [ ] Update ThemeManager to load user color overrides
  - Check settings.json for theme-specific overrides
  - Fall back to Theme JSON defaults

- [ ] Add log colors to SettingsData struct
  - QColor fields for each log level
  - Include in operator!= comparison

## Phase 2: ColorConfigWidget

- [ ] Create reusable color picker widget
  - QPushButton showing current color
  - Click opens QColorDialog
  - Displays hex code next to button
  - Emits colorChanged(QColor) signal

- [ ] Add to `include/kalahari/gui/widgets/color_config_widget.h`
- [ ] Add to `src/gui/widgets/color_config_widget.cpp`

## Phase 3: Theme Color Editor UI

- [ ] Redesign Appearance/Theme page
  - Theme selector at top (existing)
  - Dynamic color groups below

- [ ] Create ColorGroupWidget
  - QGroupBox with title
  - Grid of ColorConfigWidgets
  - Supports arbitrary number of colors

- [ ] Build groups from Theme struct
  - "Icon Colors" group (primary, secondary)
  - "Log Panel Colors" group (6 levels + background)
  - "UI Colors" group (future: accent, etc.)

## Phase 4: Reset to Defaults

- [ ] Add "Reset to Theme Defaults" button
  - Positioned next to theme selector
  - Confirms action with user

- [ ] Implement reset logic
  - Read original colors from Theme JSON
  - Clear user overrides in settings.json
  - Refresh all ColorConfigWidgets

## Phase 5: Integration & Testing

- [ ] Wire up Apply/OK to save all colors
  - Use existing batch mode for performance

- [ ] Test theme switching
  - Colors should update to new theme defaults (or user overrides)

- [ ] Test persistence
  - Close and reopen app
  - User colors should persist

## Files to Create

| File | Purpose |
|------|---------|
| `include/kalahari/gui/widgets/color_config_widget.h` | Reusable color picker |
| `src/gui/widgets/color_config_widget.cpp` | Implementation |

## Files to Modify

| File | Changes |
|------|---------|
| `include/kalahari/core/settings_manager.h` | Add log color methods |
| `src/core/settings_manager.cpp` | Implement log color methods |
| `include/kalahari/core/theme_manager.h` | Add override loading |
| `src/core/theme_manager.cpp` | Load overrides from settings |
| `include/kalahari/gui/settings_data.h` | Add log color fields |
| `include/kalahari/gui/settings_dialog.h` | Add color editor members |
| `src/gui/settings_dialog.cpp` | Redesign Theme page |
| `src/CMakeLists.txt` | Add new source files |

## Dependencies

- OpenSpec #00022 (Theme System Foundation) - DEPLOYED
- OpenSpec #00025 (Theme-Icon Integration) - DEPLOYED
- OpenSpec #00024 (Log Panel Enhanced) - DEPLOYED

## Notes

- Consider live preview vs apply-on-click
- Consider adding color presets in future
- Custom theme creation is out of scope for this task
