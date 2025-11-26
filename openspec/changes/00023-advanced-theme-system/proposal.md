# Change: Advanced Theme System with Plugin Architecture

## Why

Kalahari needs a professional, flexible theming system that:
- **Supports system Light/Dark mode** - Auto-detect and follow OS preference
- **Provides custom branded themes** - African-inspired themes (Serengeti, Savannah, Kalahari, etc.)
- **Enables full visual customization** - Beyond colors: border-radius, shadows, spacing
- **Maintains cross-platform consistency** - Same look on Windows, macOS, Linux
- **Allows plugin-based themes** - Themes and icon sets distributed as Python plugins
- **Guarantees application stability** - Emergency fallback when plugins fail

**Context:** This builds on Task #00022 (Theme System Foundation) which provides basic ThemeManager. This task extends it to a professional plugin-based theming architecture.

## Strategic Decision: Hybrid Architecture (Strategy C)

All themes (including defaults) are delivered as **Python plugins**, but with safety layers:

```
┌─────────────────────────────────────────────────────────────────┐
│                    THEME LOADING HIERARCHY                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Level 3: User/Downloaded Plugins                               │
│           └── ~/.kalahari/plugins/african-themes/               │
│           └── Installable/removable by user                     │
│                                                                  │
│  Level 2: Core Plugins (pre-installed, PROTECTED)               │
│           └── plugins/core/kalahari-default-themes/             │
│           └── plugins/core/kalahari-default-icons/              │
│           └── Cannot be uninstalled from UI                     │
│           └── Can be updated independently from app             │
│                                                                  │
│  Level 1: Emergency Fallback (HARDCODED in C++)                 │
│           └── Minimal Light + Dark palette (~100 LOC)           │
│           └── Minimal icon set (essential icons only)           │
│           └── Used ONLY when all plugins fail                   │
│           └── Guarantees app always starts                      │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Rationale:**
- **Safety**: Writer's IDE must ALWAYS start, even if plugins corrupted
- **Consistency**: Single plugin mechanism for all themes
- **Updatability**: Core themes updated via plugin system, not app releases
- **User protection**: Core plugins cannot be accidentally deleted

## What Changes

### 1. Multi-Layer Theme Architecture

```
Layer 4: Full QSS Override (optional)     ← Complete visual control
Layer 3: Component QSS (targeted)         ← Toolbar, panels, scrollbars
Layer 2: QPalette (colors)                ← 20+ color roles
Layer 1: Base Style (Fusion)              ← Cross-platform foundation
```

### 2. Plugin-Based Theme System

**Extension Points:**
- `IThemeProvider` - Plugin provides theme JSON files
- `IIconSetProvider` - Plugin provides icon sets (SVG directories)

**Plugin Types:**
- **Core Plugins** (`core: true`, `protected: true`) - Pre-installed, cannot be removed
- **User Plugins** - Downloadable, removable

### 3. Emergency Fallback System

Hardcoded C++ fallback used when:
- First application start (before plugins loaded)
- All theme plugins fail to load
- Corrupted plugin system

Contains:
- Minimal Light palette (QPalette colors only)
- Minimal Dark palette (QPalette colors only)
- Essential icons (16 basic icons embedded as C++ strings)

### 4. Extended Theme JSON Format

```json
{
  "meta": {
    "name": "Serengeti",
    "version": "1.0",
    "author": "Kalahari Team",
    "description": "Warm savanna sunset tones",
    "base": "dark",
    "preview": "serengeti-preview.png"
  },
  "palette": { /* 20+ QPalette colors */ },
  "icons": { "primary": "#E8DCC8", "secondary": "#8B7355" },
  "editor": { /* editor-specific colors */ },
  "log": { /* log panel colors */ },
  "components": { /* per-component styling */ },
  "qss": "serengeti.qss"
}
```

### 5. System Theme Integration

- Detect OS light/dark via `QStyleHints::colorScheme()`
- Option to follow system theme automatically
- Signal when system theme changes

### 6. Default Theme Distribution

**Core Plugin: `kalahari-default-themes`**
- Light theme (system default light)
- Dark theme (system default dark)

**Core Plugin: `kalahari-default-icons`**
- Outlined icon set
- Rounded icon set
- TwoTone icon set

**User Plugins (downloadable):**
| Plugin | Contents |
|--------|----------|
| `african-themes` | Serengeti, Savannah, Kalahari, Desert, Victoria, Midnight |
| `vintage-icons` | Retro-style icon set |
| `minimal-icons` | Ultra-clean minimal icons |

### 7. Protected Plugin Concept

```json
{
  "id": "org.kalahari.themes.default",
  "name": "Default Themes",
  "version": "1.0.0",
  "core": true,
  "protected": true,
  "provides": ["theme_provider"]
}
```

In Plugin Manager UI:
- Core plugins visible but "Uninstall" button disabled
- Tooltip: "Core plugin - cannot be removed"
- Update button available if newer version exists

## Impact

### Affected Specs
- **plugin-system** - New extension points (IThemeProvider, IIconSetProvider)
- **icon-system** - Plugin-based icon loading
- **settings-dialog** - Theme/icon selector UI
- **main-window** - Plugin-aware theme initialization

### Affected Code

**New Files:**
- `src/core/fallback_theme.cpp` - Emergency hardcoded themes (~150 LOC)
- `src/core/fallback_icons.cpp` - Emergency hardcoded icons (~200 LOC)
- `include/kalahari/plugins/theme_provider.h` - IThemeProvider interface
- `include/kalahari/plugins/icon_set_provider.h` - IIconSetProvider interface
- `plugins/core/kalahari-default-themes/` - Core theme plugin
- `plugins/core/kalahari-default-icons/` - Core icon plugin

**Modified Files:**
- `include/kalahari/core/theme.h` - Extended structure
- `include/kalahari/core/theme_manager.h` - Plugin discovery
- `src/core/theme_manager.cpp` - Layered application + plugin loading
- `include/kalahari/core/icon_registry.h` - Plugin-based icon loading
- `src/core/icon_registry.cpp` - Multi-source icon discovery
- `src/gui/dialogs/settings_dialog.cpp` - Theme/icon selector

### Dependencies
- Qt6::Core (QPalette, QStyle, QStyleHints)
- Qt6::Widgets (QApplication::setStyle, setPalette)
- nlohmann_json (theme file parsing)
- pybind11 (plugin extension points)
- Existing Plugin System (ExtensionPointRegistry)

### Breaking Changes
- Theme files move from `resources/themes/` to plugin directories
- Icon files move from `resources/icons/` to plugin directories
- Old hardcoded paths deprecated (fallback only)

## Current Status

**Status:** PROPOSED

**Builds on:**
- Task #00022 (Theme System Foundation) - COMPLETE
- Task #00014 (Plugin Foundation) - COMPLETE

**Estimated effort:** ~2000-2500 LOC
- Fallback themes/icons: ~350 LOC
- Extension points: ~200 LOC
- ThemeManager extensions: ~400 LOC
- IconRegistry extensions: ~300 LOC
- Core theme plugin: ~300 LOC (Python + JSON)
- Core icon plugin: ~200 LOC (Python + manifest)
- Settings dialog changes: ~150 LOC
- Tests: ~300 LOC
