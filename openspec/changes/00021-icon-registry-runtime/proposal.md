# Change: IconRegistry + Runtime Icon System

## Why

Kalahari currently has **no centralized icon management** - icons are created ad-hoc with hardcoded colors and scattered throughout the codebase. We need a professional icon system that:
- **Centralizes** all icon definitions (single source of truth)
- **Supports runtime theming** - users can change PRIMARY/SECONDARY colors without recompiling
- **Enables user customization** - replace icons, override colors per-icon
- **Integrates with Command Registry** - seamless icon retrieval for menus/toolbars
- **Persists settings** - icon customizations saved via SettingsManager
- **Supports TwoTone icons** - {COLOR_PRIMARY} + {COLOR_SECONDARY} placeholders

**Context:** wxWidgets had a full IconRegistry system (~500 LOC) with:
- Icon registration API (`registerIcon(id, svg, label)`)
- Theme support (Light/Dark/Custom)
- Per-icon color overrides
- Size configuration (toolbar/menu/panel/dialog)
- Settings persistence (JSON)
- User customization (replace SVG, change colors)

**Migration Goal:** Port wxWidgets IconRegistry to Qt6 + extend for TwoTone support

## What Changes

### Core Functionality

1. **IconRegistry Singleton (Qt6)**
   - Central icon storage: `std::map<QString, IconDescriptor>`
   - Registration API: `registerIcon(actionId, svgPath, theme, label)`
   - Retrieval API: `getIcon(actionId, theme, size)` → QIcon with colors applied
   - Color replacement: `{COLOR_PRIMARY}` / `{COLOR_SECONDARY}` → actual QColor
   - QPixmap caching for performance (avoid re-rendering same icon)

2. **IconDescriptor Structure**
   ```cpp
   struct IconDescriptor {
       QString defaultSVGPath;               // "resources/icons/twotone/save.svg"
       std::optional<QString> userSVGPath;   // User override
       std::optional<QColor> primaryOverride; // Per-icon PRIMARY color
       std::optional<QColor> secondaryOverride; // Per-icon SECONDARY color
       QString label;                        // "Save File"
   };
   ```

3. **ThemeConfig Structure**
   ```cpp
   struct ThemeConfig {
       QColor primaryColor;   // Default: #424242 (dark gray)
       QColor secondaryColor; // Default: #757575 (medium gray)
       QString name;          // "Light", "Dark", "Custom"
   };
   ```

4. **IconSizeConfig Structure (from wxWidgets)**
   ```cpp
   struct IconSizeConfig {
       int toolbar = 24;   // Toolbar icon size (px)
       int menu = 16;      // Menu icon size (px)
       int panel = 20;     // Panel caption icon size (px)
       int dialog = 32;    // Dialog icon size (px)
   };
   ```

5. **Command Registry Integration**
   - New factory method: `IconSet::fromRegistry(actionId, theme)`
   - Uses IconRegistry to load SVG from disk
   - Applies color replacement at load time
   - Creates QPixmap in 3 sizes (16×16, 24×24, 32×32)
   - Returns IconSet ready for Command usage

6. **Settings Persistence**
   - Save/load via SettingsManager (JSON)
   - Keys:
     - `icons/theme/primary_color` (QString hex: "#424242")
     - `icons/theme/secondary_color` (QString hex: "#757575")
     - `icons/sizes/toolbar` (int: 24)
     - `icons/sizes/menu` (int: 16)
     - `icons/custom/{actionId}/svg_path` (QString)
     - `icons/custom/{actionId}/primary_color` (QString hex)

7. **Icon Loading Strategy (Phase 0)**
   - Load from loose files: `resources/icons/{theme}/{name}.svg`
   - Read file content, replace `{COLOR_PRIMARY}` / `{COLOR_SECONDARY}` with actual colors
   - Render to QPixmap using QSvgRenderer
   - Cache in memory (std::map<QString, QPixmap>)
   - Optional .qrc embedding: Phase 1+ (performance optimization)

### Architecture Changes

- **New class:** `IconRegistry` (core/icon_registry.h/cpp) - ~600 LOC
- **Updated:** `Command` struct - add `IconSet::fromRegistry()` usage
- **Updated:** `ToolbarManager` - use IconRegistry for dynamic icons
- **Updated:** `SettingsDialog` - Icon Colors tab already exists (Task #00020), just wire to IconRegistry

## Impact

### Affected Specs
- **NEW SPEC:** `icon-system` (ADDED Requirements)

### Affected Code
- `include/kalahari/core/icon_registry.h` - New file (~250 LOC)
- `src/core/icon_registry.cpp` - New file (~350 LOC)
- `include/kalahari/gui/command.h` - Update IconSet with fromRegistry()
- `src/gui/command.cpp` - Implement fromRegistry()
- `src/gui/toolbar_manager.cpp` - Use IconRegistry
- `src/gui/settings_dialog.cpp` - Connect Icon Colors to IconRegistry
- `CMakeLists.txt` - Add new source files

### Dependencies
- **Qt6::Core** - QString, QColor, QFile
- **Qt6::Gui** - QPixmap, QIcon
- **Qt6::Svg** - QSvgRenderer (for SVG → QPixmap rendering)
- **Qt6::Xml** - QDomDocument (for SVG manipulation, already in Task #00020)
- No new external dependencies

### Timeline
- **Duration:** 4-6 hours (atomic task)
- **Priority:** HIGH (unblocks Phase 1 icon system)
- **Phase:** 0 (Qt Foundation)

### Breaking Changes
- None (new functionality only)
- Existing IconSet factory methods (fromStandardIcon, createPlaceholder) remain unchanged

### Migration Plan
1. Create IconRegistry Qt6 (inspired by wxWidgets version)
2. Extend for TwoTone support (PRIMARY + SECONDARY colors)
3. Integrate with Command Registry (IconSet::fromRegistry)
4. Wire Settings Dialog Icon Colors to IconRegistry
5. Test with existing icons from Task #00020
