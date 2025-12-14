# Change: Icon Downloader + Converter Tool

## Why

Kalahari needs a comprehensive icon system with 100+ Material Design SVG icons in 3 themes (TwoTone, Rounded, Outlined). Manually downloading and converting hundreds of icons is:
- **Time-consuming** - 300+ SVG files (100 icons × 3 themes)
- **Error-prone** - Material Design SVG format differs from Kalahari template format
- **Token-intensive** - AI would waste resources on repetitive manual work

An automated Icon Downloader + Converter tool will:
- **Accelerate development** - Download and convert icons in seconds
- **Enable self-service** - User can add custom icons without AI assistance
- **Ensure consistency** - Automated conversion guarantees uniform template format
- **Support extensibility** - Foundation for future icon theme plugins

This tool is **CRITICAL** for Task #00023 (Built-in Icon Set) - we need it NOW to avoid manual work.

## What Changes

### Core Functionality
1. **CLI Icon Downloader**
   - `kalahari --dev --get-icon <name>` - Download icon in all 3 themes
   - `--themes twotone,rounded,outlined` - Select specific themes
   - `--source <url>` - Custom Material Design source URL
   - `--batch save,open,close` - Download multiple icons at once
   - Default source: `https://raw.githubusercontent.com/google/material-design-icons/master/src/`

2. **Automatic SVG Conversion**
   - Material Design SVG → Kalahari template format
   - `fill="currentColor"` + `opacity="0.3"` → `fill="{COLOR_SECONDARY}"`
   - `fill="currentColor"` → `fill="{COLOR_PRIMARY}"`
   - Minification (remove comments, unnecessary attributes)
   - Validation (ensure valid SVG structure)

3. **GUI Icon Downloader (Dev Tools)**
   - Dev Tools → Icon Downloader dialog
   - Input: Icon name
   - Output: 3 SVG variants (preview + auto-save)
   - Source URL configuration
   - Live preview of downloaded icons

4. **Appearance Settings Integration**
   - Settings → Appearance → Icon Colors section
   - Primary Color picker (default: `#424242` for light theme)
   - Secondary Color picker (default: `#757575` for light theme)
   - Live preview of color changes
   - QSettings persistence

5. **Resource Management**
   - Auto-save to `resources/icons/{theme}/{name}.svg`
   - Create directories if missing
   - Generate/update `.qrc` manifest (future: Task #00021)
   - Prevent overwrite without confirmation

### Architecture Changes
- New class: `IconDownloader` (core/utils)
- New class: `SvgConverter` (core/utils)
- New dialog: `IconDownloaderDialog` (gui/dialogs)
- Updated: `cmd_line_parser.h` (add `--get-icon` flags)
- Updated: `SettingsDialog` (add Icon Colors section)

## Impact

### Affected Specs
- **NEW SPEC:** `icon-management` (ADDED Requirements)

### Affected Code
- `include/kalahari/core/cmd_line_parser.h` - New CLI flags
- `include/kalahari/core/utils/icon_downloader.h` - New file
- `include/kalahari/core/utils/svg_converter.h` - New file
- `include/kalahari/gui/dialogs/icon_downloader_dialog.h` - New file
- `include/kalahari/gui/settings_dialog.h` - Icon Colors UI
- `CMakeLists.txt` - Add new source files
- `vcpkg.json` - Possibly add libcurl (if not using Qt Network)

### Dependencies
- **Qt6 Network** - For HTTP downloads (QNetworkAccessManager)
- **Qt6 Xml** - For SVG parsing/manipulation (QDomDocument)
- No new external dependencies (Qt6 provides everything)

### Timeline
- **Duration:** 4-6 hours (atomic task)
- **Priority:** CRITICAL (blocks Task #00023)
- **Phase:** 0 (Qt Foundation)

### Breaking Changes
- None (new functionality only)

### Migration Plan
- N/A (no existing icon system yet)
