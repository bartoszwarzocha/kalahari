# Change: Theme System Foundation

## Why

Kalahari needs a centralized theme management system to support:
- **Light/Dark themes** - User-selectable UI themes
- **Runtime theme switching** - Change theme without restart
- **Color customization** - User can override individual colors
- **IconRegistry integration** - Icons update when theme changes
- **Settings persistence** - Theme choice saved between sessions

**Context:** This is ROADMAP 1.7 "Advanced Theme & Icon System" - Phase 0 foundation.

## What Changes

### Core Functionality

1. **Theme Data Structure** (`theme.h`)
   - Theme metadata: name, version, author, description
   - Main colors: primary, secondary, accent, background, text
   - Log panel colors: info, debug, background
   - JSON serialization (fromJson/toJson)

2. **ThemeManager Singleton** (`theme_manager.h/cpp`)
   - Load themes from `resources/themes/*.json`
   - Get/switch current theme
   - Apply user color overrides
   - Emit `themeChanged(Theme)` signal for UI updates

3. **Default Theme Files**
   - `resources/themes/Light.json`
   - `resources/themes/Dark.json`

4. **MainWindow Integration**
   - Connect `ThemeManager::themeChanged` to `IconRegistry::setThemeColors()`
   - Icons automatically update when theme changes

## Current Status

**IMPLEMENTED (100%):**
- [x] Theme struct with JSON serialization
- [x] ThemeManager singleton (load/switch/apply)
- [x] Color override system (applyColorOverrides, resetColorOverrides)
- [x] Default Light.json + Dark.json themes
- [x] MainWindow::onThemeChanged() → IconRegistry integration
- [x] Files in CMakeLists.txt
- [x] Settings persistence (save/load current theme name from SettingsManager)
- [x] ThemeManager path resolution (uses applicationDirPath() like IconRegistry)
- [x] All tests passing (73/73)

## Impact

### Affected Specs
- **icon-system** (IconRegistry integration done)

### Affected Code
- `include/kalahari/core/theme.h` - NEW (50 LOC)
- `src/core/theme.cpp` - NEW (90 LOC)
- `include/kalahari/core/theme_manager.h` - NEW (90 LOC)
- `src/core/theme_manager.cpp` - NEW (195 LOC)
- `resources/themes/Light.json` - NEW
- `resources/themes/Dark.json` - NEW
- `src/gui/main_window.cpp` - MODIFIED (onThemeChanged)

### Dependencies
- Qt6::Core (QString, QColor, QFile)
- nlohmann_json (JSON parsing)
- No new external dependencies

### Breaking Changes
- None (new functionality only)
