## ADDED Requirements

### Requirement: CLI Icon Download
The application SHALL provide command-line interface for downloading Material Design icons with automatic conversion to Kalahari template format.

#### Scenario: Basic icon download
- **WHEN** user executes `kalahari --dev --get-icon save`
- **THEN** system SHALL download `save` icon in all 3 default themes (twotone, rounded, outlined)
- **AND** convert each SVG to Kalahari template format with `{COLOR_PRIMARY}` and `{COLOR_SECONDARY}` placeholders
- **AND** save files to `resources/icons/twotone/save.svg`, `resources/icons/rounded/save.svg`, `resources/icons/outlined/save.svg`
- **AND** display success message: "Downloaded 3 variants of 'save' icon"

#### Scenario: Theme-specific download
- **WHEN** user executes `kalahari --dev --get-icon open --themes twotone,outlined`
- **THEN** system SHALL download only TwoTone and Outlined variants
- **AND** skip Rounded variant
- **AND** display success message: "Downloaded 2 variants of 'open' icon"

#### Scenario: Custom source URL
- **WHEN** user executes `kalahari --dev --get-icon close --source https://custom-cdn.com/icons/`
- **THEN** system SHALL use custom URL as Material Design icon source
- **AND** download icons from custom source
- **AND** save to standard `resources/icons/{theme}/` directories

#### Scenario: Batch download
- **WHEN** user executes `kalahari --dev --get-icons save,open,close`
- **THEN** system SHALL download all 3 icons in all 3 themes (9 files total)
- **AND** display progress: "Downloading 1/3... 2/3... 3/3..."
- **AND** display summary: "Downloaded 3 icons (9 files total)"

#### Scenario: Network failure
- **WHEN** network connection fails during download
- **THEN** system SHALL display error message: "Network error: Could not connect to icon source"
- **AND** suggest checking internet connection
- **AND** exit with non-zero code

#### Scenario: Invalid icon name
- **WHEN** user requests non-existent icon `kalahari --dev --get-icon nonexistent`
- **THEN** system SHALL display error: "Icon 'nonexistent' not found"
- **AND** suggest similar icon names if available (fuzzy search)
- **AND** exit with non-zero code

---

### Requirement: SVG Conversion to Kalahari Template
Downloaded Material Design SVG SHALL be automatically converted to Kalahari template format with color placeholders.

#### Scenario: TwoTone icon conversion
- **WHEN** TwoTone SVG is downloaded with structure:
  ```xml
  <path fill="currentColor" opacity="0.3" d="M..."/>
  <path fill="currentColor" d="M..."/>
  ```
- **THEN** converter SHALL transform to:
  ```xml
  <path fill="{COLOR_SECONDARY}" opacity="0.3" d="M..."/>
  <path fill="{COLOR_PRIMARY}" d="M..."/>
  ```

#### Scenario: Outlined/Rounded icon conversion
- **WHEN** Outlined or Rounded SVG is downloaded (single color):
  ```xml
  <path fill="currentColor" d="M..."/>
  ```
- **THEN** converter SHALL transform to:
  ```xml
  <path fill="{COLOR_PRIMARY}" d="M..."/>
  ```

#### Scenario: SVG minification
- **WHEN** SVG contains comments, unnecessary namespaces, or metadata
- **THEN** converter SHALL remove:
  - XML comments (`<!-- ... -->`)
  - Unnecessary attributes (preserve `xmlns`, `viewBox`, `width`, `height`)
  - Metadata tags (`<metadata>`, `<title>`, `<desc>`)
- **AND** preserve essential structure for rendering

#### Scenario: Invalid SVG structure
- **WHEN** downloaded SVG is malformed or invalid
- **THEN** converter SHALL detect parsing errors
- **AND** display error: "Conversion failed: Invalid SVG structure"
- **AND** save original SVG to `resources/icons/_errors/{name}.svg` for inspection

---

### Requirement: GUI Icon Downloader Dialog
Dev Tools menu SHALL provide graphical Icon Downloader dialog for interactive icon management.

#### Scenario: Open Icon Downloader
- **WHEN** user selects Dev Tools → Icon Downloader (requires `--dev` flag)
- **THEN** system SHALL display Icon Downloader dialog with:
  - Icon Name input field
  - Themes checkboxes (TwoTone ☑, Rounded ☑, Outlined ☑)
  - Source URL input (default: GitHub Raw Material Design)
  - Download button
  - Preview area (empty initially)

#### Scenario: Download via GUI
- **WHEN** user enters "save" in Icon Name field and clicks Download
- **THEN** system SHALL download 3 variants (if all themes checked)
- **AND** display SVG preview for each theme in preview area
- **AND** show status: "Downloaded 3 variants successfully"
- **AND** auto-save to `resources/icons/{theme}/save.svg`

#### Scenario: Custom source URL configuration
- **WHEN** user changes Source URL to custom value
- **THEN** system SHALL validate URL format
- **AND** save custom URL to QSettings (`dev_tools/icon_source_url`)
- **AND** use custom URL for subsequent downloads

#### Scenario: Preview before save
- **WHEN** icons are downloaded successfully
- **THEN** system SHALL render SVG preview (24x24px) for each theme
- **AND** display original SVG code in expandable text area
- **AND** allow user to confirm or cancel save

#### Scenario: Overwrite confirmation
- **WHEN** icon file already exists at target location
- **THEN** system SHALL display confirmation dialog: "Icon 'save.svg' already exists. Overwrite?"
- **AND** provide options: [Overwrite] [Skip] [Cancel All]

---

### Requirement: Icon Color Configuration
Settings → Appearance SHALL allow configuring Primary and Secondary icon colors for SVG rendering.

#### Scenario: Default icon colors
- **WHEN** Appearance settings are opened for first time
- **THEN** system SHALL display Icon Colors section with:
  - Primary Color picker (default: `#424242` - dark gray)
  - Secondary Color picker (default: `#757575` - medium gray)
  - Live preview icon (sample SVG with both colors)

#### Scenario: Change Primary Color
- **WHEN** user selects new Primary Color (e.g., `#2196F3` blue)
- **THEN** live preview SHALL update immediately
- **AND** color SHALL be saved to QSettings (`appearance/icon_primary_color`)
- **AND** all icons using `{COLOR_PRIMARY}` SHALL render with new color after Apply

#### Scenario: Change Secondary Color
- **WHEN** user selects new Secondary Color (e.g., `#90CAF9` light blue)
- **THEN** live preview SHALL update immediately (TwoTone secondary layer)
- **AND** color SHALL be saved to QSettings (`appearance/icon_secondary_color`)
- **AND** TwoTone icons SHALL render secondary layer with new color after Apply

#### Scenario: Reset to defaults
- **WHEN** user clicks "Reset to Default Colors" button
- **THEN** system SHALL restore:
  - Primary Color → `#424242`
  - Secondary Color → `#757575`
- **AND** update live preview
- **AND** save defaults to QSettings

#### Scenario: Live preview rendering
- **WHEN** color picker value changes
- **THEN** preview icon SHALL re-render within 100ms
- **AND** display both Primary (main shape) and Secondary (accent layer) colors
- **AND** use sample TwoTone icon (e.g., "save" or "settings")

---

### Requirement: Resource Directory Management
System SHALL automatically manage icon resource directories and prevent data loss.

#### Scenario: Create missing directories
- **WHEN** icon is downloaded but `resources/icons/{theme}/` does not exist
- **THEN** system SHALL create directory structure:
  - `resources/icons/twotone/`
  - `resources/icons/rounded/`
  - `resources/icons/outlined/`
- **AND** set appropriate permissions (read/write)

#### Scenario: Save to correct theme directory
- **WHEN** TwoTone variant is downloaded
- **THEN** file SHALL be saved to `resources/icons/twotone/{name}.svg`
- **WHEN** Rounded variant is downloaded
- **THEN** file SHALL be saved to `resources/icons/rounded/{name}.svg`
- **WHEN** Outlined variant is downloaded
- **THEN** file SHALL be saved to `resources/icons/outlined/{name}.svg`

#### Scenario: Handle file system errors
- **WHEN** file system is read-only or disk is full
- **THEN** system SHALL display error: "Cannot save icon: {error_details}"
- **AND** log error to spdlog
- **AND** not delete or corrupt existing files

#### Scenario: Verify SVG validity before save
- **WHEN** converted SVG is about to be saved
- **THEN** system SHALL validate:
  - SVG is valid XML (QDomDocument::setContent returns true)
  - Contains at least one `<path>` or `<circle>` or `<rect>` element
  - Has required `viewBox` attribute
- **AND** reject invalid SVG with error message

---

### Requirement: Dev Tools Menu Integration
Application launched with `--dev` flag SHALL display Dev Tools menu with Icon Downloader action.

#### Scenario: Dev Tools menu visibility (with --dev flag)
- **WHEN** application is launched with `kalahari --dev`
- **THEN** main menu bar SHALL contain "Dev Tools" menu after "Help"
- **AND** Dev Tools menu SHALL contain:
  - "Icon Downloader..." action (enabled)
  - Separator
  - "Plugin Packager..." action (disabled, placeholder for future)

#### Scenario: Dev Tools menu hidden (without --dev flag)
- **WHEN** application is launched normally (without `--dev`)
- **THEN** "Dev Tools" menu SHALL NOT appear in menu bar
- **AND** Icon Downloader SHALL NOT be accessible via GUI

#### Scenario: Open Icon Downloader from menu
- **WHEN** user clicks Dev Tools → Icon Downloader...
- **THEN** IconDownloaderDialog SHALL open as modal dialog
- **AND** parent window SHALL be disabled until dialog is closed

---

## Design Notes

### Architecture
- **IconDownloader class** (core/utils): HTTP download logic using QNetworkAccessManager
- **SvgConverter class** (core/utils): SVG parsing and template conversion using QDomDocument
- **IconDownloaderDialog** (gui/dialogs): Qt dialog with QLineEdit, QCheckBox, QPushButton, QTextEdit
- **SettingsDialog update**: Add Icon Colors section to Appearance tab

### Material Design Icon URL Pattern
```
Base: https://raw.githubusercontent.com/google/material-design-icons/master/src/
Pattern: {base}/{category}/{icon_name}/{variant}/24px.svg

Examples:
- save (content category):
  - TwoTone: .../src/content/save/materialiconstwotone/24px.svg
  - Rounded: .../src/content/save/materialiconsround/24px.svg
  - Outlined: .../src/content/save/materialiconsoutlined/24px.svg

- open (file category):
  - TwoTone: .../src/file/folder_open/materialiconstwotone/24px.svg
```

**Challenge:** Category mapping (icon name → category) requires either:
1. Hardcoded mapping (100+ icons, maintainable)
2. GitHub API search (slower, dynamic)
3. User provides full path (flexible but complex)

**Recommendation:** Start with hardcoded mapping for common icons (Task #00023), allow custom URL for edge cases.

### Color Placeholder System
- `{COLOR_PRIMARY}` - Main icon color (100% opacity)
- `{COLOR_SECONDARY}` - Accent color for TwoTone (usually 30% opacity)
- Runtime replacement in IconRegistry (Task #00021)

### Qt Resource System Integration
Icons will be embedded via `.qrc` in Task #00021. For now, store as loose files in `resources/icons/`.

---

**Specification Version:** 1.0
**Status:** 🔄 IN PROGRESS
**Change ID:** `00020-icon-downloader-tool`
