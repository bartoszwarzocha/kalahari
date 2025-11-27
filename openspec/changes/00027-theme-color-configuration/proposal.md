# Proposal: Centralized Theme Color Configuration

**ID:** 00027-theme-color-configuration
**Status:** PENDING
**Created:** 2025-11-27
**Author:** Claude

## Summary

Create a centralized UI for configuring all theme colors in one place. Currently, only icon colors (primary/secondary) are user-configurable, while other theme colors (log levels, UI elements) are fixed per theme. This proposal unifies all color configuration under Appearance -> Theme.

## Problem Statement

1. **Inconsistent configuration**: Icon colors are stored per-theme in settings.json, but log colors are fixed in Theme JSON files
2. **No user customization**: Users cannot customize log panel colors, UI accent colors, etc.
3. **Scattered UI**: Icon colors are in Theme tab, but other colors have no configuration UI
4. **Future-proofing**: As more theme colors are added, each would need separate UI

## Proposed Solution

### 1. Unified Color Storage (per-theme in settings.json)

```json
{
  "themes": {
    "Light": {
      "colors": {
        "primary": "#333333",
        "secondary": "#666666",
        "accent": "#0078D4"
      },
      "log": {
        "trace": "#CC00CC",
        "debug": "#CC00CC",
        "info": "#000000",
        "warning": "#FF8C00",
        "error": "#CC0000",
        "critical": "#CC0000",
        "background": "#F5F5F5"
      }
    },
    "Dark": {
      "colors": { ... },
      "log": { ... }
    }
  }
}
```

### 2. Dynamic Color Editor UI

Replace current Theme tab content with dynamic color editor:

```
+------------------------------------------+
| Theme: [Light v]  [Reset to Defaults]    |
+------------------------------------------+
| Icon Colors                              |
|   Primary:     [####] #333333            |
|   Secondary:   [####] #666666            |
+------------------------------------------+
| Log Panel Colors                         |
|   Trace:       [####] #CC00CC            |
|   Debug:       [####] #CC00CC            |
|   Info:        [####] #000000            |
|   Warning:     [####] #FF8C00            |
|   Error:       [####] #CC0000            |
|   Critical:    [####] #CC0000            |
|   Background:  [####] #F5F5F5            |
+------------------------------------------+
| UI Colors (future)                       |
|   Accent:      [####] #0078D4            |
+------------------------------------------+
```

### 3. Architecture

- **ColorConfigWidget**: Reusable widget for single color (button + hex label)
- **ColorGroupWidget**: QGroupBox containing multiple ColorConfigWidgets
- **ThemeColorEditor**: Main widget that dynamically builds groups from Theme struct
- **SettingsManager**: Extended with per-theme color getters/setters for all color categories

### 4. "Reset to Theme Defaults" Button

- Reads original colors from Theme JSON file (Light.json, Dark.json)
- Clears user overrides in settings.json for current theme
- Refreshes UI with default colors

## Benefits

1. **Consistency**: All colors configured in one place
2. **Extensibility**: New color categories auto-appear in UI
3. **User control**: Full theme customization
4. **Clean architecture**: Single source of truth for color overrides

## Files to Modify

| File | Changes |
|------|---------|
| `settings_dialog.cpp` | Replace Theme tab with dynamic color editor |
| `settings_manager.h/cpp` | Add per-theme color getters/setters for log colors |
| `theme_manager.cpp` | Load user color overrides from settings |
| `settings_data.h` | Add log color fields |

## Files to Create

| File | Purpose |
|------|---------|
| `color_config_widget.h/cpp` | Reusable color picker widget |
| `theme_color_editor.h/cpp` | Dynamic theme color editor |

## Dependencies

- OpenSpec #00022 (Theme System Foundation) - DEPLOYED
- OpenSpec #00025 (Theme-Icon Integration) - DEPLOYED

## Estimated Effort

- Phase 1: Color storage architecture (2h)
- Phase 2: ColorConfigWidget (1h)
- Phase 3: ThemeColorEditor UI (3h)
- Phase 4: Reset to Defaults (1h)
- Phase 5: Testing & polish (1h)

**Total:** ~8 hours

## Open Questions

1. Should we support custom themes (user-created JSON files)?
2. Should color changes apply immediately (live preview) or on Apply?
3. Should we add color presets (e.g., "High Contrast", "Solarized")?
