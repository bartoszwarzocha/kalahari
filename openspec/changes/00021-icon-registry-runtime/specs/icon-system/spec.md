## ADDED Requirements

### Requirement: IconRegistry Initialization and Registration
The application SHALL provide centralized icon registry for managing all application icons.

#### Scenario: Initialize IconRegistry at startup
- **WHEN** application starts and calls `IconRegistry::getInstance().initialize()`
- **THEN** system SHALL create singleton instance
- **AND** load theme configuration from QSettings
- **AND** load icon size configuration from QSettings
- **AND** load user customizations from QSettings
- **AND** log initialization status to spdlog

#### Scenario: Register icon with default path
- **WHEN** code calls `IconRegistry::getInstance().registerIcon("file.new", "resources/icons/twotone/file-new.svg", "New File")`
- **THEN** system SHALL create IconDescriptor entry
- **AND** store default SVG path: `resources/icons/twotone/file-new.svg`
- **AND** store label: "New File"
- **AND** apply no per-icon color overrides (use theme colors)

#### Scenario: Check if icon is registered
- **WHEN** code calls `IconRegistry::getInstance().hasIcon("file.new")`
- **THEN** system SHALL return `true` if icon registered
- **AND** return `false` if icon not registered

#### Scenario: Get all registered icon IDs
- **WHEN** code calls `IconRegistry::getInstance().getAllIconIds()`
- **THEN** system SHALL return `QStringList` with all registered action IDs
- **AND** list SHALL be sorted alphabetically

---

### Requirement: Icon Retrieval with Color Replacement
IconRegistry SHALL load SVG files from disk and apply runtime color replacement.

#### Scenario: Get icon with theme colors (no overrides)
- **WHEN** code calls `IconRegistry::getInstance().getIcon("file.save", "twotone", 24)`
- **THEN** system SHALL load `resources/icons/twotone/save.svg` from disk
- **AND** replace `{COLOR_PRIMARY}` with theme primary color (e.g., `#424242`)
- **AND** replace `{COLOR_SECONDARY}` with theme secondary color (e.g., `#757575`)
- **AND** render SVG to QPixmap at 24×24px using QSvgRenderer
- **AND** convert QPixmap to QIcon
- **AND** cache QPixmap in memory for future requests
- **AND** return QIcon

#### Scenario: Get icon with per-icon color override
- **GIVEN** icon "file.save" has per-icon PRIMARY color override: `#FF0000` (red)
- **WHEN** code calls `IconRegistry::getInstance().getIcon("file.save", "twotone", 24)`
- **THEN** system SHALL replace `{COLOR_PRIMARY}` with `#FF0000` (override)
- **AND** replace `{COLOR_SECONDARY}` with theme secondary color (no override)
- **AND** render with overridden colors

#### Scenario: Get icon from cache (performance)
- **GIVEN** icon "file.save" was previously loaded and cached
- **WHEN** code calls `IconRegistry::getInstance().getIcon("file.save", "twotone", 24)` again
- **THEN** system SHALL return cached QIcon immediately
- **AND** NOT reload SVG from disk
- **AND** NOT re-render QPixmap

#### Scenario: Icon file not found
- **WHEN** code calls `IconRegistry::getInstance().getIcon("nonexistent", "twotone", 24)`
- **THEN** system SHALL log warning: "Icon file not found: resources/icons/twotone/nonexistent.svg"
- **AND** return empty QIcon
- **AND** NOT crash application

#### Scenario: Invalid SVG file
- **GIVEN** SVG file at `resources/icons/twotone/broken.svg` contains invalid XML
- **WHEN** code calls `IconRegistry::getInstance().getIcon("broken", "twotone", 24)`
- **THEN** system SHALL log error: "Failed to parse SVG: resources/icons/twotone/broken.svg"
- **AND** return empty QIcon
- **AND** NOT crash application

---

### Requirement: Theme Configuration (Primary + Secondary Colors)
IconRegistry SHALL support global theme with PRIMARY and SECONDARY colors.

#### Scenario: Default theme (Light)
- **WHEN** IconRegistry initializes with no saved settings
- **THEN** system SHALL use default Light theme:
  - Primary Color: `#424242` (dark gray)
  - Secondary Color: `#757575` (medium gray)
  - Name: "Light"

#### Scenario: Set custom theme colors
- **WHEN** code calls `IconRegistry::getInstance().setThemeColors(QColor("#2196F3"), QColor("#90CAF9"), "Blue")`
- **THEN** system SHALL update theme:
  - Primary Color: `#2196F3` (blue)
  - Secondary Color: `#90CAF9` (light blue)
  - Name: "Blue"
- **AND** clear icon cache (force re-render with new colors)
- **AND** save theme to QSettings (`icons/theme/primary_color`, `icons/theme/secondary_color`, `icons/theme/name`)

#### Scenario: Get current theme configuration
- **WHEN** code calls `IconRegistry::getInstance().getThemeConfig()`
- **THEN** system SHALL return `ThemeConfig` struct with current colors and name

#### Scenario: Reset to default theme
- **WHEN** code calls `IconRegistry::getInstance().resetTheme()`
- **THEN** system SHALL restore Light theme defaults:
  - Primary: `#424242`
  - Secondary: `#757575`
- **AND** clear icon cache
- **AND** save to QSettings

---

### Requirement: Icon Size Configuration (Context-Aware)
IconRegistry SHALL provide different icon sizes for different UI contexts.

#### Scenario: Default icon sizes
- **WHEN** IconRegistry initializes with no saved settings
- **THEN** system SHALL use default sizes:
  - Toolbar: 24px
  - Menu: 16px
  - Panel: 20px
  - Dialog: 32px

#### Scenario: Set custom icon sizes
- **WHEN** code calls `IconRegistry::getInstance().setSizes(IconSizeConfig{32, 20, 24, 48})`
- **THEN** system SHALL update size configuration:
  - Toolbar: 32px
  - Menu: 20px
  - Panel: 24px
  - Dialog: 48px
- **AND** clear icon cache (force re-render with new sizes)
- **AND** save sizes to QSettings (`icons/sizes/toolbar`, etc.)

#### Scenario: Get size for toolbar context
- **WHEN** code calls `IconRegistry::getInstance().getSizes().toolbar`
- **THEN** system SHALL return current toolbar icon size (e.g., 24)

#### Scenario: Get icon with context-specific size
- **WHEN** ToolbarManager calls `IconRegistry::getInstance().getIcon("file.save", "twotone", getSizes().toolbar)`
- **THEN** system SHALL render icon at toolbar size (24px by default)
- **WHEN** Menu calls `getIcon("file.save", "twotone", getSizes().menu)`
- **THEN** system SHALL render icon at menu size (16px by default)

---

### Requirement: User Customization (Per-Icon Overrides)
IconRegistry SHALL allow users to customize individual icons (SVG path, colors).

#### Scenario: Set custom SVG path for icon
- **WHEN** user calls `IconRegistry::getInstance().setCustomIconPath("file.save", "/custom/icons/my-save.svg")`
- **THEN** system SHALL update IconDescriptor:
  - `userSVGPath = "/custom/icons/my-save.svg"`
- **AND** clear cache for "file.save" (force reload)
- **AND** save to QSettings (`icons/custom/file.save/svg_path`)

#### Scenario: Load icon with custom SVG path
- **GIVEN** icon "file.save" has custom SVG path: `/custom/icons/my-save.svg`
- **WHEN** code calls `getIcon("file.save", "twotone", 24)`
- **THEN** system SHALL load `/custom/icons/my-save.svg` (NOT default path)
- **AND** apply color replacement
- **AND** render to QPixmap

#### Scenario: Clear custom SVG path (revert to default)
- **WHEN** user calls `IconRegistry::getInstance().clearCustomIconPath("file.save")`
- **THEN** system SHALL remove `userSVGPath` override
- **AND** revert to default path: `resources/icons/twotone/save.svg`
- **AND** clear cache for "file.save"
- **AND** remove from QSettings (`icons/custom/file.save/svg_path`)

#### Scenario: Set per-icon PRIMARY color override
- **WHEN** user calls `IconRegistry::getInstance().setIconPrimaryColor("file.save", QColor("#FF0000"))`
- **THEN** system SHALL update IconDescriptor:
  - `primaryOverride = QColor("#FF0000")`
- **AND** clear cache for "file.save"
- **AND** save to QSettings (`icons/custom/file.save/primary_color = "#FF0000"`)

#### Scenario: Set per-icon SECONDARY color override
- **WHEN** user calls `IconRegistry::getInstance().setIconSecondaryColor("file.save", QColor("#FFCCCC"))`
- **THEN** system SHALL update IconDescriptor:
  - `secondaryOverride = QColor("#FFCCCC")`
- **AND** clear cache for "file.save"
- **AND** save to QSettings (`icons/custom/file.save/secondary_color = "#FFCCCC"`)

#### Scenario: Clear per-icon color overrides
- **WHEN** user calls `IconRegistry::getInstance().clearIconColors("file.save")`
- **THEN** system SHALL remove `primaryOverride` and `secondaryOverride`
- **AND** revert to theme colors
- **AND** clear cache for "file.save"
- **AND** remove from QSettings

#### Scenario: Reset ALL customizations (factory defaults)
- **WHEN** user calls `IconRegistry::getInstance().resetAllCustomizations()`
- **THEN** system SHALL:
  - Clear ALL `userSVGPath` overrides
  - Clear ALL `primaryOverride` and `secondaryOverride` values
  - Reset theme to Light defaults
  - Reset sizes to defaults
  - Clear entire icon cache
  - Remove ALL custom settings from QSettings

---

### Requirement: Settings Persistence (QSettings Integration)
IconRegistry SHALL persist all customizations via SettingsManager.

#### Scenario: Save theme configuration on change
- **WHEN** user changes theme colors
- **THEN** system SHALL save to QSettings:
  - Key: `icons/theme/primary_color`, Value: "#2196F3"
  - Key: `icons/theme/secondary_color`, Value: "#90CAF9"
  - Key: `icons/theme/name`, Value: "Blue"

#### Scenario: Save icon sizes on change
- **WHEN** user changes icon sizes
- **THEN** system SHALL save to QSettings:
  - Key: `icons/sizes/toolbar`, Value: 32
  - Key: `icons/sizes/menu`, Value: 20
  - Key: `icons/sizes/panel`, Value: 24
  - Key: `icons/sizes/dialog`, Value: 48

#### Scenario: Save per-icon customization
- **WHEN** user customizes icon "file.save" (custom path + colors)
- **THEN** system SHALL save to QSettings:
  - Key: `icons/custom/file.save/svg_path`, Value: "/custom/my-save.svg"
  - Key: `icons/custom/file.save/primary_color`, Value: "#FF0000"
  - Key: `icons/custom/file.save/secondary_color`, Value: "#FFCCCC"

#### Scenario: Load all settings on startup
- **WHEN** IconRegistry calls `loadFromSettings()` during `initialize()`
- **THEN** system SHALL:
  - Load theme colors from QSettings (or use defaults)
  - Load icon sizes from QSettings (or use defaults)
  - Load ALL per-icon customizations from QSettings
  - Apply to IconDescriptors

#### Scenario: Handle missing/corrupted settings
- **GIVEN** QSettings contains invalid color hex: `icons/theme/primary_color = "INVALID"`
- **WHEN** IconRegistry loads settings
- **THEN** system SHALL log warning: "Invalid primary color in settings, using default"
- **AND** use default color: `#424242`
- **AND** NOT crash application

---

### Requirement: Command Registry Integration (IconSet Factory)
IconSet SHALL provide factory method to load icons from IconRegistry.

#### Scenario: Create IconSet from IconRegistry
- **WHEN** code calls `IconSet::fromRegistry("file.save", "twotone")`
- **THEN** system SHALL:
  - Query `IconRegistry::getInstance().getIcon("file.save", "twotone", 16)` → QIcon for menu
  - Query `IconRegistry::getInstance().getIcon("file.save", "twotone", 24)` → QIcon for toolbar
  - Query `IconRegistry::getInstance().getIcon("file.save", "twotone", 32)` → QIcon for large
  - Create IconSet with 3 QPixmaps (16×16, 24×24, 32×32)
  - Return IconSet

#### Scenario: Use IconSet in Command Registration
- **WHEN** code registers command:
  ```cpp
  REG_CMD_TOOL_ICON("file.save", "Save", "File",
      IconSet::fromRegistry("file.save", "twotone"),
      QKeySequence::Save, saveCallback);
  ```
- **THEN** system SHALL:
  - Load icon from IconRegistry
  - Apply current theme colors
  - Create Command with IconSet
  - Command available in menus/toolbars with correct icon

#### Scenario: Dynamic icon update (theme change)
- **GIVEN** Command "file.save" uses IconSet::fromRegistry()
- **WHEN** user changes theme colors in Settings
- **THEN** system SHALL:
  - Clear IconRegistry cache
  - Command keeps old IconSet (not auto-updated in Phase 0)
  - **Future (Phase 1):** Emit IconRegistry::themeChanged() signal → rebuild all IconSets

---

### Requirement: Performance and Caching
IconRegistry SHALL cache rendered QPixmaps to avoid redundant disk I/O and rendering.

#### Scenario: Cache key structure
- **WHEN** system caches icon
- **THEN** cache key SHALL be: `"{actionId}_{theme}_{size}_{primaryColor}_{secondaryColor}"`
- **EXAMPLE:** `"file.save_twotone_24_#424242_#757575"`

#### Scenario: Cache hit (performance)
- **GIVEN** icon "file.save" (twotone, 24px, colors #424242/#757575) is cached
- **WHEN** code requests same icon again
- **THEN** system SHALL return cached QPixmap in <1ms
- **AND** NOT reload SVG from disk
- **AND** NOT re-render

#### Scenario: Cache invalidation on theme change
- **WHEN** user changes theme primary color from #424242 to #2196F3
- **THEN** system SHALL clear entire cache
- **AND** next getIcon() call will reload and re-render

#### Scenario: Cache invalidation on per-icon customization
- **WHEN** user sets custom color for "file.save"
- **THEN** system SHALL clear cache entries matching "file.save_*"
- **AND** preserve cache for other icons

#### Scenario: Cache memory management
- **WHEN** cache grows beyond 100 icons
- **THEN** system SHALL implement LRU eviction (optional, Phase 1+)
- **OR** unlimited cache (acceptable for Phase 0 with ~100 icons)

---

## Design Notes

### Architecture
- **IconRegistry class** (core/icon_registry.h/cpp): Singleton managing all icon state (~600 LOC)
- **IconDescriptor struct**: Per-icon metadata (SVG path, label, color overrides)
- **ThemeConfig struct**: Global theme (PRIMARY + SECONDARY colors, name)
- **IconSizeConfig struct**: Context-aware sizes (toolbar/menu/panel/dialog)
- **Integration:** IconSet::fromRegistry() factory method (command.h/cpp)

### Color Replacement Algorithm
1. Load SVG file content (QString)
2. Replace `{COLOR_PRIMARY}` → QColor::name() hex string (e.g., "#424242")
3. Replace `{COLOR_SECONDARY}` → QColor::name() hex string (e.g., "#757575")
4. Create QSvgRenderer from modified SVG string
5. Render to QPixmap at requested size
6. Convert QPixmap → QIcon
7. Cache QPixmap with composite key

### wxWidgets Parity
Qt6 version extends wxWidgets IconRegistry with:
- ✅ TwoTone support (PRIMARY + SECONDARY colors)
- ✅ Theme names ("Light", "Dark", "Custom")
- ✅ Per-icon SECONDARY color override (wxWidgets had only single color)
- ✅ Disk-based loading (loose files, not embedded yet)
- ✅ QPixmap caching (performance)

wxWidgets features deferred to Phase 1:
- ❌ EventBus notifications on icon changes (IconRegistry::themeChanged signal)
- ❌ Automatic UI refresh on theme change
- ❌ .qrc embedding (loose files in Phase 0, embedded in Phase 1+)

### Settings Keys Structure
```
icons/
  theme/
    primary_color = "#424242"
    secondary_color = "#757575"
    name = "Light"
  sizes/
    toolbar = 24
    menu = 16
    panel = 20
    dialog = 32
  custom/
    file.save/
      svg_path = "/custom/icons/my-save.svg"
      primary_color = "#FF0000"
      secondary_color = "#FFCCCC"
    edit.copy/
      primary_color = "#00FF00"
```

### Error Handling
- Missing SVG file → Log warning, return empty QIcon
- Invalid SVG XML → Log error, return empty QIcon
- Invalid color in settings → Log warning, use default
- Invalid size in settings → Log warning, use default
- File system errors → Log error, graceful degradation

---

**Specification Version:** 1.0
**Status:** 🔄 IN PROGRESS
**Change ID:** `00021-icon-registry-runtime`
