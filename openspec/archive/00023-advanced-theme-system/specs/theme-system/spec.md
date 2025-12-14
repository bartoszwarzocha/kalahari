# Theme System Specification (Delta)

## ADDED Requirements

### Requirement: Three-Level Theme Hierarchy
The system SHALL implement a three-level theme hierarchy:
1. **Level 1 (Emergency Fallback)**: Hardcoded C++ minimal Light/Dark themes, always available
2. **Level 2 (Core Plugins)**: Pre-installed protected plugins that cannot be removed via UI
3. **Level 3 (User Plugins)**: User-installable/removable theme plugins

The system SHALL use themes from higher levels when lower levels are unavailable or corrupted.

#### Scenario: Application starts without plugins
- **WHEN** the application starts with no plugins installed
- **THEN** the emergency fallback Light theme is applied
- **AND** the application is fully usable
- **AND** essential icons are displayed from fallback

#### Scenario: Core plugin provides themes
- **WHEN** core plugins are loaded successfully
- **THEN** themes from core plugins are available for selection
- **AND** core plugin themes take precedence over fallback themes

#### Scenario: User plugin provides additional themes
- **WHEN** user has installed additional theme plugins
- **THEN** those themes appear in the theme selector
- **AND** user plugin themes can override core plugin themes with same name

---

### Requirement: Emergency Fallback System
The system SHALL provide hardcoded emergency fallback themes in C++:
- Minimal Light theme with basic QPalette colors
- Minimal Dark theme with basic QPalette colors
- 16 essential icons embedded as SVG string literals

The fallback system SHALL be loaded BEFORE any plugins are initialized.

#### Scenario: Fallback theme provides basic functionality
- **WHEN** using the fallback Light theme
- **THEN** all QPalette color roles are defined
- **AND** basic UI elements are readable and usable
- **AND** the application does not crash

#### Scenario: Fallback icons cover essential functions
- **WHEN** using fallback icons
- **THEN** icons for file operations (new, open, save) are available
- **AND** icons for edit operations (undo, redo, cut, copy, paste) are available
- **AND** icons for help and settings are available

#### Scenario: Fallback is used when plugins fail
- **WHEN** all theme plugins fail to load
- **THEN** the fallback theme is automatically applied
- **AND** a warning is logged
- **AND** the user can continue working

---

### Requirement: IThemeProvider Extension Point
The system SHALL define an `IThemeProvider` extension point for Python plugins:
- `get_theme_directory() -> Path`: Return path to theme JSON files
- `get_themes() -> List[Dict]`: Return list of theme metadata (name, file, description)
- `get_qss_directory() -> Path`: Optional, return path to QSS files

#### Scenario: Plugin provides theme metadata
- **WHEN** a plugin implements IThemeProvider
- **AND** the plugin is loaded
- **THEN** ThemeManager discovers themes via `get_themes()`
- **AND** themes are added to available themes list

#### Scenario: Plugin theme files are loaded
- **WHEN** a theme from a plugin is selected
- **THEN** the theme JSON is loaded from `get_theme_directory()`
- **AND** optional QSS is loaded from `get_qss_directory()`

---

### Requirement: IIconSetProvider Extension Point
The system SHALL define an `IIconSetProvider` extension point for Python plugins:
- `get_icon_sets() -> List[Dict]`: Return list of icon set metadata (id, name, directory)
- `get_icon_mapping() -> Dict[str, str]`: Optional mapping of logical names to filenames

#### Scenario: Plugin provides icon set
- **WHEN** a plugin implements IIconSetProvider
- **AND** the plugin is loaded
- **THEN** IconRegistry discovers icon sets via `get_icon_sets()`
- **AND** icon sets are added to available icon sets list

#### Scenario: Icon name mapping is applied
- **WHEN** an icon set provides a mapping via `get_icon_mapping()`
- **THEN** IconRegistry uses mapped filename for each logical icon name
- **AND** unmapped icon names fall back to direct filename lookup

---

### Requirement: Protected Plugin Mechanism
The system SHALL support protected plugins that cannot be uninstalled via UI:
- Plugins with `core: true` and `protected: true` in manifest
- Located in `app_dir/plugins/core/` directory
- Update is allowed, uninstall is blocked

#### Scenario: Protected plugin cannot be uninstalled
- **WHEN** user attempts to uninstall a protected plugin via Plugin Manager
- **THEN** the uninstall action is blocked
- **AND** a message explains the plugin is protected

#### Scenario: Protected plugin can be updated
- **WHEN** a newer version of a protected plugin is available
- **THEN** the update action is enabled
- **AND** the plugin can be updated normally

---

### Requirement: Four-Layer Theme Application
The system SHALL apply themes in four layers:
1. **Layer 1 (Fusion)**: Set Qt Fusion style as cross-platform base
2. **Layer 2 (QPalette)**: Apply palette colors for all color roles
3. **Layer 3 (Component QSS)**: Generate QSS for specific components
4. **Layer 4 (Full QSS)**: Apply optional complete QSS override

Each layer SHALL build upon the previous, with later layers overriding earlier ones.

#### Scenario: All four layers applied in order
- **WHEN** a theme is applied via ThemeManager::applyTheme()
- **THEN** Fusion style is set first
- **AND** QPalette is applied second
- **AND** Component QSS is generated and applied third
- **AND** Full QSS file is loaded and applied fourth (if specified)

#### Scenario: Layer 4 is optional
- **WHEN** a theme does not specify a qss file path
- **THEN** only layers 1-3 are applied
- **AND** no error is raised

---

### Requirement: Extended Theme JSON Format
The system SHALL support an extended theme JSON format containing:
- `meta`: name, version, author, description, base (light/dark), preview image
- `palette`: Full QPalette color roles (~20 colors) with disabled states
- `icons`: Primary and secondary icon colors
- `editor`: Editor-specific colors (background, foreground, selection, cursor, lineNumbers)
- `log`: Log panel colors (info, debug, warning, error, background)
- `components`: Per-component styling (toolbar, scrollbar, statusbar, panel, menu)
- `qss`: Optional path to full QSS override file

#### Scenario: Parse complete extended theme
- **WHEN** a theme JSON with all sections is loaded
- **THEN** all sections are parsed into Theme struct
- **AND** the theme can be applied successfully

#### Scenario: Backward compatibility with basic format
- **WHEN** a theme JSON in basic format (Task #00022) is loaded
- **THEN** the theme loads successfully with default values for new fields
- **AND** no error is raised

---

### Requirement: System Theme Integration
The system SHALL integrate with OS light/dark mode:
- Detect current system color scheme via Qt6 `QStyleHints::colorScheme()`
- Optionally follow system theme automatically
- React to system theme changes in real-time
- Allow override (force specific theme regardless of system)

#### Scenario: Detect system dark mode
- **WHEN** the operating system is set to dark mode
- **THEN** ThemeManager::getSystemColorScheme() returns Qt::ColorScheme::Dark

#### Scenario: Follow system theme enabled
- **WHEN** followSystemTheme is enabled
- **AND** the user changes OS from light to dark mode
- **THEN** the application automatically switches to Dark theme
- **AND** themeChanged signal is emitted

#### Scenario: Override system theme
- **WHEN** followSystemTheme is disabled
- **AND** user selects a specific theme
- **THEN** that theme is used regardless of OS setting

---

### Requirement: Core Theme Plugin
The system SHALL include a pre-installed core theme plugin:
- Plugin ID: `org.kalahari.themes.default`
- Provides: Light theme, Dark theme
- Location: `plugins/core/kalahari-default-themes/`
- Properties: `core: true`, `protected: true`

#### Scenario: Core theme plugin loads on startup
- **WHEN** the application starts
- **THEN** kalahari-default-themes plugin is loaded
- **AND** Light and Dark themes are available

#### Scenario: Core theme plugin is protected
- **WHEN** user views kalahari-default-themes in Plugin Manager
- **THEN** the uninstall button is disabled
- **AND** tooltip indicates it is a core plugin

---

### Requirement: Core Icon Plugin
The system SHALL include a pre-installed core icon plugin:
- Plugin ID: `org.kalahari.icons.default`
- Provides: Outlined, Rounded, TwoTone icon sets
- Location: `plugins/core/kalahari-default-icons/`
- Properties: `core: true`, `protected: true`

#### Scenario: Core icon plugin loads on startup
- **WHEN** the application starts
- **THEN** kalahari-default-icons plugin is loaded
- **AND** three icon sets (Outlined, Rounded, TwoTone) are available

#### Scenario: Icon mapping applied
- **WHEN** an icon is requested by logical name (e.g., "file.new")
- **THEN** the mapping.json is consulted
- **AND** the mapped filename (e.g., "note_add.svg") is loaded

---

### Requirement: Theme Selector in Settings
The system SHALL provide UI for theme selection in Settings > Appearance:
- Dropdown list of available themes
- Theme source indication (e.g., "Serengeti (African Themes)")
- Theme applies immediately on selection (live preview)
- "Follow system theme" checkbox option

#### Scenario: Select theme from dropdown
- **WHEN** user opens Settings > Appearance
- **AND** selects a theme from dropdown
- **THEN** the theme is applied immediately
- **AND** the setting is saved to SettingsManager

#### Scenario: Follow system theme checkbox
- **WHEN** user checks "Follow system theme"
- **THEN** the theme dropdown is disabled
- **AND** the system theme is applied

---

### Requirement: Icon Set Selector in Settings
The system SHALL provide UI for icon set selection in Settings > Appearance:
- Dropdown list of available icon sets
- Icon set applies immediately on selection
- Selected icon set persists between sessions

#### Scenario: Select icon set from dropdown
- **WHEN** user opens Settings > Appearance
- **AND** selects an icon set from dropdown
- **THEN** all icons update to use the new icon set
- **AND** the setting is saved to SettingsManager

---

## MODIFIED Requirements

### Requirement: Theme Data Structure
The Theme struct SHALL contain the following fields:
- `meta`: ThemeMeta (name, version, author, description, base, preview)
- `palette`: ThemePalette (all QPalette color roles including disabled states)
- `icons`: ThemeIcons (primary, secondary colors)
- `editor`: ThemeEditor (editor-specific colors)
- `log`: ThemeLog (log panel colors)
- `components`: ThemeComponents (per-component styling)
- `qssPath`: Optional path to full QSS file

The Theme struct SHALL support JSON serialization via fromJson() and toJson().
The Theme struct SHALL be backward compatible with Task #00022 format.

#### Scenario: Create extended theme from JSON
- **WHEN** Theme::fromJson() is called with extended JSON
- **THEN** all fields including new sections are populated
- **AND** the returned Theme is valid

#### Scenario: Load basic theme format
- **WHEN** Theme::fromJson() is called with basic JSON (Task #00022)
- **THEN** the theme loads with default values for new fields
- **AND** no error is raised

---

### Requirement: ThemeManager Singleton
The ThemeManager SHALL provide the following extended interface:
- `getInstance()`: Access singleton instance
- `loadTheme(name)`: Load theme from any source (plugin or fallback)
- `applyTheme(theme)`: Apply theme using four-layer architecture
- `switchTheme(name)`: Load and apply theme by name
- `getCurrentTheme()`: Return currently active theme
- `getAvailableThemes()`: Return list of ThemeInfo from all sources
- `discoverAllThemes()`: Discover themes from fallback, core, and user plugins
- `getSystemColorScheme()`: Return OS light/dark preference
- `setFollowSystemTheme(bool)`: Enable/disable system theme following
- `isFollowingSystemTheme()`: Check if following system theme
- `themeChanged(Theme)`: Signal emitted when theme changes
- `systemThemeChanged(Qt::ColorScheme)`: Signal emitted when OS theme changes

#### Scenario: Discover themes from all sources
- **WHEN** ThemeManager::discoverAllThemes() is called
- **THEN** fallback themes are added (Level 1)
- **AND** core plugin themes are added (Level 2)
- **AND** user plugin themes are added (Level 3)

#### Scenario: Theme info includes source
- **WHEN** ThemeManager::getAvailableThemes() is called
- **THEN** each ThemeInfo includes source (plugin ID or "fallback")
- **AND** each ThemeInfo includes isCore and isProtected flags

---

### Requirement: IconRegistry Singleton
The IconRegistry SHALL provide the following extended interface:
- `getInstance()`: Access singleton instance
- `discoverAllIconSets()`: Discover icon sets from fallback, core, and user plugins
- `getAvailableIconSets()`: Return list of IconSetInfo from all sources
- `switchIconSet(iconSetId)`: Switch to a different icon set
- `getCurrentIconSet()`: Return current icon set ID
- `getIcon(iconId)`: Return icon from current set (with mapping applied)
- `setThemeColors(primary, secondary)`: Update icon colors for current theme

#### Scenario: Discover icon sets from plugins
- **WHEN** IconRegistry::discoverAllIconSets() is called
- **THEN** fallback icons are registered (Level 1)
- **AND** core plugin icon sets are discovered (Level 2)
- **AND** user plugin icon sets are discovered (Level 3)

#### Scenario: Switch icon set
- **WHEN** IconRegistry::switchIconSet("rounded") is called
- **THEN** all icons update to use the Rounded icon set
- **AND** icon colors from current theme are preserved
