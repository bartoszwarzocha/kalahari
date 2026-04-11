# Implementation Tasks

## Status: PROPOSED

Awaiting approval before implementation.

**Estimated Duration:** 4 weeks
**Estimated LOC:** ~2500

---

## Phase 1: Emergency Fallback System

### 1.1 Fallback Theme Implementation
- [ ] Create `include/kalahari/core/fallback_theme.h`
- [ ] Create `src/core/fallback_theme.cpp`
- [ ] Implement `FallbackTheme::getLightTheme()` with minimal QPalette
- [ ] Implement `FallbackTheme::getDarkTheme()` with minimal QPalette
- [ ] Implement `FallbackTheme::isUsingFallback()` flag
- [ ] Add to CMakeLists.txt

**Files:** `include/kalahari/core/fallback_theme.h`, `src/core/fallback_theme.cpp`
**LOC:** ~100

### 1.2 Fallback Icons Implementation
- [ ] Create `include/kalahari/core/fallback_icons.h`
- [ ] Create `src/core/fallback_icons.cpp`
- [ ] Embed 16 essential icons as SVG string literals:
  - file.new, file.open, file.save, file.saveAs
  - edit.undo, edit.redo, edit.cut, edit.copy, edit.paste
  - edit.delete, edit.selectAll, edit.find
  - help.about, help.help
  - edit.settings, file.exit
- [ ] Implement `FallbackIcons::getIcon(iconId)` method
- [ ] Implement `FallbackIcons::getAllIconIds()` method
- [ ] Add to CMakeLists.txt

**Files:** `include/kalahari/core/fallback_icons.h`, `src/core/fallback_icons.cpp`
**LOC:** ~250

### 1.3 Startup Integration
- [ ] Update `main.cpp` to apply fallback theme BEFORE plugin init
- [ ] Update `main.cpp` to register fallback icons BEFORE plugin init
- [ ] Test application starts without any plugins
- [ ] Test application starts with empty plugins/ directory

**Files:** `src/main.cpp`
**LOC:** ~30

---

## Phase 2: Extended Theme Data Structure

### 2.1 Theme Struct Extensions
- [ ] Add `ThemeMeta` struct (name, version, author, description, base, preview)
- [ ] Add `ThemePalette` struct with all 20+ QPalette color roles
- [ ] Add `ThemeDisabledColors` for disabled state colors
- [ ] Add `ThemeEditor` struct (background, foreground, lineNumbers, currentLine, selection, cursor, matchingBracket)
- [ ] Add `ThemeLog` struct (background, info, debug, warning, error)
- [ ] Add `ThemeToolbar` struct (background, separator, buttonHover)
- [ ] Add `ThemeScrollbar` struct (track, thumb, thumbHover, width)
- [ ] Add `ThemeStatusbar` struct (background, border)
- [ ] Add `ThemePanel` struct (background, border, headerBackground)
- [ ] Add `ThemeMenu` struct (background, itemHover, separator)
- [ ] Add `ThemeComponents` aggregate struct
- [ ] Add optional `qssPath` field to Theme

**Files:** `include/kalahari/core/theme.h`
**LOC:** ~150

### 2.2 Theme JSON Parser Extensions
- [ ] Implement `ThemeMeta::fromJson()` / `toJson()`
- [ ] Implement `ThemePalette::fromJson()` / `toJson()` with all color roles
- [ ] Implement `ThemeEditor::fromJson()` / `toJson()`
- [ ] Implement `ThemeLog::fromJson()` / `toJson()`
- [ ] Implement `ThemeComponents::fromJson()` / `toJson()`
- [ ] Update `Theme::fromJson()` to use new structures
- [ ] Add backward compatibility for old theme format (Task #00022)
- [ ] Add validation with meaningful error messages
- [ ] Add default values for missing optional fields

**Files:** `src/core/theme.cpp`
**LOC:** ~200

---

## Phase 3: Four-Layer Theme Application

### 3.1 Layer 1: Fusion Style
- [ ] Add `ThemeManager::setBaseStyle()` method
- [ ] Call `QApplication::setStyle("Fusion")` in applyTheme()
- [ ] Verify Fusion respects QPalette on all platforms

**Files:** `src/core/theme_manager.cpp`
**LOC:** ~20

### 3.2 Layer 2: QPalette Application
- [ ] Create `ThemeManager::buildPalette(const ThemePalette&)` method
- [ ] Map all ThemePalette colors to QPalette::ColorRole:
  - Window, WindowText, Base, AlternateBase, Text
  - Button, ButtonText, BrightText
  - Highlight, HighlightedText
  - Link, LinkVisited
  - ToolTipBase, ToolTipText
  - PlaceholderText
  - Light, Midlight, Mid, Dark, Shadow
- [ ] Handle disabled state colors (QPalette::Disabled group)
- [ ] Call `QApplication::setPalette()`
- [ ] Add fallback for missing colors (inherit from base)

**Files:** `src/core/theme_manager.cpp`
**LOC:** ~100

### 3.3 Layer 3: Component QSS Generation
- [ ] Create `ThemeManager::generateComponentQSS(const Theme&)` method
- [ ] Generate QSS for QScrollBar (vertical + horizontal):
  - track, thumb, thumb:hover, width, border-radius
  - arrow buttons (hide or style)
- [ ] Generate QSS for QToolBar:
  - background, separator, spacing
- [ ] Generate QSS for QStatusBar:
  - background, border-top
- [ ] Generate QSS for QDockWidget:
  - title bar background, border
- [ ] Generate QSS for QMenu / QMenuBar:
  - background, item:hover, separator
- [ ] Generate QSS for QTabBar / QTabWidget:
  - tab background, selected tab, hover
- [ ] Apply generated QSS via `qApp->setStyleSheet()`

**Files:** `src/core/theme_manager.cpp`
**LOC:** ~200

### 3.4 Layer 4: Full QSS Override
- [ ] Create `ThemeManager::loadQSSFile(const QString& path)` method
- [ ] Handle relative paths (relative to theme directory)
- [ ] Handle missing QSS file gracefully (log warning, continue)
- [ ] Append full QSS after component QSS
- [ ] Support QSS variables substitution (future enhancement)

**Files:** `src/core/theme_manager.cpp`
**LOC:** ~50

### 3.5 Unified applyTheme() Pipeline
- [ ] Refactor `applyTheme()` to call all four layers in order
- [ ] Update IconRegistry colors after theme change
- [ ] Emit `themeChanged(theme)` signal
- [ ] Add detailed logging for each layer
- [ ] Handle errors gracefully (fallback to previous theme)

**Files:** `src/core/theme_manager.cpp`
**LOC:** ~50

---

## Phase 4: Extension Points for Plugins

### 4.1 IThemeProvider Extension Point
- [ ] Create `include/kalahari/plugins/theme_provider.h`
- [ ] Define `IThemeProvider` abstract interface:
  - `get_theme_directory() -> Path`
  - `get_themes() -> List[ThemeMetadata]`
  - `get_qss_directory() -> Path` (optional, default = theme_directory)
- [ ] Add pybind11 bindings in `src/bindings/extensions_bindings.cpp`
- [ ] Register extension point in ExtensionPointRegistry
- [ ] Document extension point API

**Files:** `include/kalahari/plugins/theme_provider.h`, `src/bindings/extensions_bindings.cpp`
**LOC:** ~100

### 4.2 IIconSetProvider Extension Point
- [ ] Create `include/kalahari/plugins/icon_set_provider.h`
- [ ] Define `IIconSetProvider` abstract interface:
  - `get_icon_sets() -> List[IconSetMetadata]`
  - `get_icon_mapping() -> Dict[str, str]` (optional)
- [ ] Add pybind11 bindings in `src/bindings/extensions_bindings.cpp`
- [ ] Register extension point in ExtensionPointRegistry
- [ ] Document extension point API

**Files:** `include/kalahari/plugins/icon_set_provider.h`, `src/bindings/extensions_bindings.cpp`
**LOC:** ~80

### 4.3 Protected Plugin Support
- [ ] Add `core` field to PluginManifest struct
- [ ] Add `protected` field to PluginManifest struct
- [ ] Implement `PluginManager::canUninstall(pluginId)` method
- [ ] Update PluginManager to load core plugins first
- [ ] Define core plugins path: `app_dir/plugins/core/`
- [ ] Define user plugins path: `~/.kalahari/plugins/`

**Files:** `include/kalahari/core/plugin_manifest.h`, `src/core/plugin_manager.cpp`
**LOC:** ~50

---

## Phase 5: Plugin-Based Theme Discovery

### 5.1 ThemeManager Plugin Integration
- [ ] Add `ThemeInfo` struct with source tracking:
  - name, description, path, source (plugin ID), isCore, isProtected
- [ ] Implement `ThemeManager::discoverAllThemes()` method
- [ ] Implement `ThemeManager::addFallbackThemes()` (Level 1)
- [ ] Implement `ThemeManager::discoverPluginThemes(PluginType)` (Level 2-3)
- [ ] Update `getAvailableThemes()` to return `QList<ThemeInfo>`
- [ ] Add `getThemeByName(name)` method with source resolution
- [ ] Handle theme name conflicts (prefer user > core > fallback)

**Files:** `include/kalahari/core/theme_manager.h`, `src/core/theme_manager.cpp`
**LOC:** ~150

### 5.2 IconRegistry Plugin Integration
- [ ] Add `IconSetInfo` struct with source tracking:
  - id, name, description, directory, source, isCore
- [ ] Implement `IconRegistry::discoverAllIconSets()` method
- [ ] Implement `IconRegistry::addFallbackIcons()` (Level 1)
- [ ] Implement `IconRegistry::discoverPluginIconSets(PluginType)` (Level 2-3)
- [ ] Update `getAvailableIconSets()` to return `QList<IconSetInfo>`
- [ ] Implement icon name mapping support (mapping.json)
- [ ] Add `switchIconSet(iconSetId)` method

**Files:** `include/kalahari/core/icon_registry.h`, `src/core/icon_registry.cpp`
**LOC:** ~150

---

## Phase 6: System Theme Integration

### 6.1 OS Theme Detection
- [ ] Add `ThemeManager::getSystemColorScheme()` method
- [ ] Use `QGuiApplication::styleHints()->colorScheme()`
- [ ] Handle `Qt::ColorScheme::Unknown` case (default to Light)

**Files:** `src/core/theme_manager.cpp`
**LOC:** ~20

### 6.2 Follow System Theme
- [ ] Add `m_followSystemTheme` member variable
- [ ] Add `setFollowSystemTheme(bool)` method
- [ ] Add `isFollowingSystemTheme()` method
- [ ] Connect to `QStyleHints::colorSchemeChanged` signal
- [ ] Implement `onSystemThemeChanged(Qt::ColorScheme)` slot
- [ ] Auto-switch to Light/Dark based on OS preference
- [ ] Add `systemThemeChanged(Qt::ColorScheme)` signal

**Files:** `include/kalahari/core/theme_manager.h`, `src/core/theme_manager.cpp`
**LOC:** ~60

### 6.3 Settings Persistence
- [ ] Add `followSystemTheme` to SettingsManager
- [ ] Add `iconSet` to SettingsManager
- [ ] Load/save follow system theme preference
- [ ] Load/save selected icon set

**Files:** `include/kalahari/core/settings_manager.h`, `src/core/settings_manager.cpp`
**LOC:** ~40

---

## Phase 7: Core Plugins

### 7.1 kalahari-default-themes Plugin
- [ ] Create `plugins/core/kalahari-default-themes/manifest.json`
- [ ] Create `plugins/core/kalahari-default-themes/main.py`
- [ ] Implement `DefaultThemesPlugin(IThemeProvider)`
- [ ] Create `plugins/core/kalahari-default-themes/themes/Light.json`
- [ ] Create `plugins/core/kalahari-default-themes/themes/Dark.json`
- [ ] Test plugin loads correctly
- [ ] Test themes apply correctly

**Files:** `plugins/core/kalahari-default-themes/*`
**LOC:** ~200 (Python + JSON)

### 7.2 kalahari-default-icons Plugin
- [ ] Create `plugins/core/kalahari-default-icons/manifest.json`
- [ ] Create `plugins/core/kalahari-default-icons/main.py`
- [ ] Implement `DefaultIconsPlugin(IIconSetProvider)`
- [ ] Create `plugins/core/kalahari-default-icons/mapping.json`
- [ ] Move icons from `resources/icons/` to plugin:
  - `icons/outlined/*.svg`
  - `icons/rounded/*.svg`
  - `icons/twotone/*.svg`
- [ ] Test plugin loads correctly
- [ ] Test all three icon sets work

**Files:** `plugins/core/kalahari-default-icons/*`
**LOC:** ~150 (Python + JSON)

---

## Phase 8: Settings Dialog Integration

### 8.1 Theme Selector UI
- [ ] Add theme QComboBox to Appearance settings tab
- [ ] Populate with themes from `ThemeManager::getAvailableThemes()`
- [ ] Show theme source in dropdown (e.g., "Serengeti (African Themes)")
- [ ] Apply theme immediately on selection
- [ ] Show theme description as tooltip

**Files:** `src/gui/dialogs/settings_dialog.cpp`
**LOC:** ~50

### 8.2 Icon Set Selector UI
- [ ] Add icon set QComboBox to Appearance settings tab
- [ ] Populate with icon sets from `IconRegistry::getAvailableIconSets()`
- [ ] Apply icon set immediately on selection
- [ ] Show icon set description as tooltip

**Files:** `src/gui/dialogs/settings_dialog.cpp`
**LOC:** ~40

### 8.3 Follow System Theme Option
- [ ] Add "Follow system theme" QCheckBox
- [ ] Disable theme dropdown when checked
- [ ] Connect to ThemeManager::setFollowSystemTheme()
- [ ] Update checkbox state from settings on load

**Files:** `src/gui/dialogs/settings_dialog.cpp`
**LOC:** ~30

---

## Phase 9: Migration & Cleanup

### 9.1 Remove Old Theme Files
- [ ] Remove `resources/themes/Light.json` (moved to plugin)
- [ ] Remove `resources/themes/Dark.json` (moved to plugin)
- [ ] Update CMakeLists.txt to not copy old themes
- [ ] Update any hardcoded theme paths

**Files:** Various
**LOC:** -100 (removal)

### 9.2 Remove Old Icon Files
- [ ] Move `resources/icons/outlined/` to plugin
- [ ] Move `resources/icons/rounded/` to plugin
- [ ] Move `resources/icons/twotone/` to plugin
- [ ] Update CMakeLists.txt to not copy old icons
- [ ] Update any hardcoded icon paths

**Files:** Various
**LOC:** -50 (removal)

---

## Phase 10: Testing

### 10.1 Unit Tests
- [ ] Test `FallbackTheme::getLightTheme()` returns valid theme
- [ ] Test `FallbackTheme::getDarkTheme()` returns valid theme
- [ ] Test `FallbackIcons::getIcon()` for all 16 essential icons
- [ ] Test `Theme::fromJson()` with new extended format
- [ ] Test `Theme::fromJson()` backward compatibility (old format)
- [ ] Test `ThemeManager::buildPalette()` maps all colors
- [ ] Test `ThemeManager::generateComponentQSS()` output
- [ ] Test `ThemeManager::discoverAllThemes()` finds all sources
- [ ] Test `IconRegistry::discoverAllIconSets()` finds all sources
- [ ] Test protected plugin cannot be uninstalled

**Files:** `tests/core/test_fallback_theme.cpp`, `tests/core/test_theme_extended.cpp`
**LOC:** ~200

### 10.2 Integration Tests
- [ ] Test app starts without any plugins (fallback only)
- [ ] Test app starts with corrupt plugin (graceful fallback)
- [ ] Test theme switching updates entire UI
- [ ] Test icon set switching updates all icons
- [ ] Test "Follow system theme" responds to OS changes
- [ ] Test theme persists between app restarts
- [ ] Test icon set persists between app restarts

### 10.3 Platform Testing
- [ ] Test on Windows 10/11
- [ ] Test on macOS (if available)
- [ ] Test on Linux (WSL or native)
- [ ] Verify Fusion style looks correct on all platforms
- [ ] Verify system theme detection works on all platforms

---

## Summary

| Phase | Description | Estimated LOC |
|-------|-------------|---------------|
| 1 | Emergency Fallback | ~380 |
| 2 | Extended Theme Structure | ~350 |
| 3 | Four-Layer Application | ~420 |
| 4 | Extension Points | ~230 |
| 5 | Plugin Discovery | ~300 |
| 6 | System Theme Integration | ~120 |
| 7 | Core Plugins | ~350 |
| 8 | Settings Dialog | ~120 |
| 9 | Migration & Cleanup | -150 |
| 10 | Testing | ~200 |
| **Total** | | **~2320 LOC** |

---

## Dependencies

**Requires completed:**
- Task #00022 (Theme System Foundation) - COMPLETE
- Task #00014 (Plugin Foundation) - COMPLETE
- Task #00021 (Icon Registry Runtime) - COMPLETE

**External dependencies:**
- Qt6::Widgets (QPalette, QStyle, QStyleHints)
- pybind11 (extension point bindings)
- nlohmann_json (theme parsing)

---

## Acceptance Criteria

1. [ ] Application starts even when all plugins are missing/corrupt
2. [ ] Emergency fallback provides usable Light/Dark themes
3. [ ] Emergency fallback provides 16 essential icons
4. [ ] Core plugins (default-themes, default-icons) load correctly
5. [ ] Core plugins cannot be uninstalled via UI
6. [ ] User can install additional theme/icon plugins
7. [ ] Theme switching applies all 4 layers correctly
8. [ ] Icon set switching updates all icons
9. [ ] "Follow system theme" works on Windows
10. [ ] Settings persist between app restarts
11. [ ] All 73+ existing tests pass
12. [ ] New unit tests for fallback system pass
13. [ ] Works on Windows, macOS, Linux
