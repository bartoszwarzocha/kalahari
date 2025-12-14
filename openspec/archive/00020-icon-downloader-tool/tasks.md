# Implementation Tasks

## 1. Core Classes and CLI Integration

### 1.1 IconDownloader Class (core/utils)
- [x] Create `include/kalahari/core/utils/icon_downloader.h`
- [x] Create `src/core/utils/icon_downloader.cpp`
- [x] Implement `IconDownloader` class:
  - [ ] Constructor with source URL parameter
  - [ ] `downloadIcon(QString name, QStringList themes)` method
  - [ ] `setSourceUrl(QString url)` method
  - [ ] `getDefaultSourceUrl()` static method
- [x] Implement HTTP download using `QNetworkAccessManager`:
  - [ ] Async request with `QNetworkReply`
  - [ ] Handle redirects and SSL errors
  - [ ] Timeout after 10 seconds
  - [ ] Progress reporting via signals
- [x] Implement Material Design URL construction:
  - [ ] Hardcoded category mapping for common icons (save, open, close, etc.)
  - [ ] Fallback to custom URL if mapping not found
  - [ ] Template: `{base}/{category}/{name}/{variant}/24px.svg`
- [x] Implement error handling:
  - [ ] Network errors (no connection, timeout)
  - [ ] HTTP errors (404, 500)
  - [ ] Invalid icon names
- [x] Add unit tests using Catch2 (mock QNetworkAccessManager)

### 1.2 SvgConverter Class (core/utils)
- [x] Create `include/kalahari/core/utils/svg_converter.h`
- [x] Create `src/core/utils/svg_converter.cpp`
- [x] Implement `SvgConverter` class:
  - [ ] `convertToTemplate(QString svgData)` method
  - [ ] Returns converted SVG string or error
- [x] Implement SVG parsing using `QDomDocument`:
  - [ ] Parse SVG XML structure
  - [ ] Find all `<path>`, `<circle>`, `<rect>` elements
  - [ ] Detect `fill="currentColor"` attributes
- [x] Implement color placeholder replacement:
  - [ ] `fill="currentColor"` + `opacity="0.3"` → `fill="{COLOR_SECONDARY}"`
  - [ ] `fill="currentColor"` (no opacity) → `fill="{COLOR_PRIMARY}"`
  - [ ] Preserve other fill values (gradients, etc.)
- [x] Implement SVG minification:
  - [ ] Remove XML comments
  - [ ] Remove `<metadata>`, `<title>`, `<desc>` tags
  - [ ] Preserve essential attributes (xmlns, viewBox, width, height)
- [x] Implement validation:
  - [ ] Check SVG is valid XML
  - [ ] Check contains at least one drawable element
  - [ ] Check has `viewBox` attribute
- [x] Add unit tests with sample Material Design SVG

### 1.3 CLI Flag Integration (cmd_line_parser)
- [x] Update `include/kalahari/core/cmd_line_parser.h`:
  - [ ] Add `bool devMode` field
  - [ ] Add `QString getIconName` field
  - [ ] Add `QStringList getIconThemes` field
  - [ ] Add `QString iconSourceUrl` field
  - [ ] Add `QStringList getIconsBatch` field
- [x] Update `src/core/cmd_line_parser.cpp`:
  - [ ] Parse `--dev` flag
  - [ ] Parse `--get-icon <name>` flag
  - [ ] Parse `--themes <list>` flag (comma-separated)
  - [ ] Parse `--source <url>` flag
  - [ ] Parse `--get-icons <list>` flag (comma-separated batch)
  - [ ] Validation: `--get-icon` requires `--dev`
- [x] Add unit tests for CLI parsing

### 1.4 CLI Execution Logic (main.cpp)
- [x] Update `src/main.cpp`:
  - [ ] Check if `--dev --get-icon` flags present
  - [ ] If yes, run CLI mode (no GUI):
    - [ ] Create `IconDownloader` instance
    - [ ] Create `SvgConverter` instance
    - [ ] Call `downloadIcon()` for each theme
    - [ ] Convert each downloaded SVG
    - [ ] Save to `resources/icons/{theme}/{name}.svg`
    - [ ] Print progress to stdout
    - [ ] Exit with code 0 (success) or 1 (error)
  - [ ] If no, continue with normal GUI startup

---

## 2. GUI Icon Downloader Dialog

### 2.1 IconDownloaderDialog Class
- [x] Create `include/kalahari/gui/dialogs/icon_downloader_dialog.h`
- [x] Create `src/gui/dialogs/icon_downloader_dialog.cpp`
- [x] Implement `IconDownloaderDialog` class (inherits `QDialog`):
  - [ ] Constructor with parent QWidget
  - [ ] `setupUi()` private method
  - [ ] `onDownloadClicked()` slot
  - [ ] `onIconDownloaded(QString theme, QString svgData)` slot
  - [ ] `onDownloadError(QString error)` slot

### 2.2 Dialog UI Layout
- [x] Create dialog layout using `QVBoxLayout`:
  - [ ] Icon Name section:
    - [ ] `QLabel` "Icon Name:"
    - [ ] `QLineEdit` for icon name input
  - [ ] Themes section:
    - [ ] `QGroupBox` "Themes"
    - [ ] `QCheckBox` "TwoTone" (checked by default)
    - [ ] `QCheckBox` "Rounded" (checked by default)
    - [ ] `QCheckBox` "Outlined" (checked by default)
  - [ ] Source URL section:
    - [ ] `QLabel` "Source URL:"
    - [ ] `QLineEdit` with default GitHub URL
    - [ ] `QPushButton` "Reset to Default"
  - [ ] Preview section:
    - [ ] `QGroupBox` "Preview"
    - [ ] `QTabWidget` with tabs for each theme
    - [ ] Each tab: `QLabel` for SVG preview + `QTextEdit` for SVG code
  - [ ] Buttons:
    - [ ] `QPushButton` "Download" (primary)
    - [ ] `QPushButton` "Close"
- [x] Set dialog size: 600×500px (resizable)
- [x] Set window title: "Icon Downloader (Dev Tools)"

### 2.3 Download Logic Integration
- [x] Connect Download button to `onDownloadClicked()`:
  - [ ] Validate icon name (not empty)
  - [ ] Get selected themes (at least one checked)
  - [ ] Create `IconDownloader` with source URL
  - [ ] Call `downloadIcon()` asynchronously
  - [ ] Show progress indicator (disable Download button)
- [x] Handle download completion:
  - [ ] Convert SVG using `SvgConverter`
  - [ ] Render preview (QSvgRenderer → QPixmap → QLabel)
  - [ ] Display SVG code in QTextEdit
  - [ ] Check if file exists → show overwrite dialog
  - [ ] Save to `resources/icons/{theme}/{name}.svg`
  - [ ] Show success message: "Downloaded 3 variants"
- [x] Handle download errors:
  - [ ] Display error in QMessageBox
  - [ ] Re-enable Download button
  - [ ] Log to spdlog

### 2.4 Source URL Persistence
- [x] Load source URL from QSettings on dialog open:
  - [ ] Key: `dev_tools/icon_source_url`
  - [ ] Default: GitHub Raw URL
- [x] Save custom URL when changed:
  - [ ] On "Download" click or "Apply" button
  - [ ] Validate URL format (starts with http:// or https://)

---

## 3. Dev Tools Menu

### 3.1 MainWindow Menu Integration
- [x] Update `include/kalahari/gui/main_window.h`:
  - [ ] Add `QMenu* m_devToolsMenu` member
  - [ ] Add `QAction* m_iconDownloaderAction` member
  - [ ] Add `bool m_devMode` member
  - [ ] Add `void createDevToolsMenu()` private method
  - [ ] Add `void onIconDownloaderTriggered()` slot
- [x] Update `src/gui/main_window.cpp`:
  - [ ] In constructor, check `CmdLineParser::devMode`
  - [ ] If dev mode, call `createDevToolsMenu()`
  - [ ] Implement `createDevToolsMenu()`:
    - [ ] Create "Dev Tools" menu after "Help" menu
    - [ ] Add "Icon Downloader..." action
    - [ ] Add separator
    - [ ] Add "Plugin Packager..." action (disabled, placeholder)
  - [ ] Implement `onIconDownloaderTriggered()`:
    - [ ] Create `IconDownloaderDialog` instance
    - [ ] Show as modal dialog (`exec()`)

### 3.2 Menu Visibility Logic
- [x] Ensure Dev Tools menu only appears with `--dev` flag:
  - [ ] Check `CmdLineParser::devMode` in `createMenus()`
  - [ ] Conditionally call `createDevToolsMenu()`
- [x] Add comment in code explaining dev-only visibility

---

## 4. Appearance Settings Integration

### 4.1 Icon Colors UI Section
- [x] Update `include/kalahari/gui/settings_dialog.h`:
  - [ ] Add `QLabel* m_iconPrimaryColorLabel` member
  - [ ] Add `QPushButton* m_iconPrimaryColorButton` member (shows color)
  - [ ] Add `QLabel* m_iconSecondaryColorLabel` member
  - [ ] Add `QPushButton* m_iconSecondaryColorButton` member
  - [ ] Add `QLabel* m_iconPreviewLabel` member (SVG preview)
  - [ ] Add `QPushButton* m_resetIconColorsButton` member
  - [ ] Add `QColor m_iconPrimaryColor` member
  - [ ] Add `QColor m_iconSecondaryColor` member
  - [ ] Add `void setupIconColorsSection()` private method
  - [ ] Add `void onIconPrimaryColorClicked()` slot
  - [ ] Add `void onIconSecondaryColorClicked()` slot
  - [ ] Add `void onResetIconColorsClicked()` slot
  - [ ] Add `void updateIconPreview()` private method

### 4.2 Icon Colors Section Layout
- [x] In Appearance tab, add Icon Colors section after existing settings:
  - [ ] `QGroupBox` "Icon Colors"
  - [ ] `QFormLayout`:
    - [ ] Row 1: "Primary Color:" + color button
    - [ ] Row 2: "Secondary Color:" + color button
    - [ ] Row 3: "Preview:" + preview label (48×48px sample icon)
    - [ ] Row 4: "Reset to Default Colors" button (right-aligned)
  - [ ] Set color button size: 80×30px
  - [ ] Set button background to current color
  - [ ] Display hex value on button (e.g., "#424242")

### 4.3 Color Picker Integration
- [x] Implement `onIconPrimaryColorClicked()`:
  - [ ] Show `QColorDialog` with current primary color
  - [ ] On color selected, update `m_iconPrimaryColor`
  - [ ] Update button background and text
  - [ ] Call `updateIconPreview()`
- [x] Implement `onIconSecondaryColorClicked()`:
  - [ ] Show `QColorDialog` with current secondary color
  - [ ] On color selected, update `m_iconSecondaryColor`
  - [ ] Update button background and text
  - [ ] Call `updateIconPreview()`

### 4.4 Live Preview Rendering
- [x] Implement `updateIconPreview()`:
  - [ ] Load sample TwoTone SVG template (e.g., "save" icon)
  - [ ] Replace `{COLOR_PRIMARY}` with `m_iconPrimaryColor`
  - [ ] Replace `{COLOR_SECONDARY}` with `m_iconSecondaryColor`
  - [ ] Render using `QSvgRenderer`
  - [ ] Create `QPixmap` (48×48px)
  - [ ] Display in `m_iconPreviewLabel`
- [x] Call `updateIconPreview()` on color change (debounce if needed)

### 4.5 Settings Persistence
- [x] Load icon colors from QSettings on Settings Dialog open:
  - [ ] Key: `appearance/icon_primary_color` (default: `#424242`)
  - [ ] Key: `appearance/icon_secondary_color` (default: `#757575`)
  - [ ] Parse hex string to `QColor`
- [x] Save icon colors on Apply button:
  - [ ] Convert `QColor` to hex string
  - [ ] Save to QSettings
  - [ ] Emit settings changed signal (for future IconRegistry refresh)

### 4.6 Reset to Defaults
- [x] Implement `onResetIconColorsClicked()`:
  - [ ] Set `m_iconPrimaryColor` to `#424242`
  - [ ] Set `m_iconSecondaryColor` to `#757575`
  - [ ] Update color buttons
  - [ ] Call `updateIconPreview()`
  - [ ] Mark settings as modified (enable Apply)

---

## 5. Resource Management

### 5.1 Directory Creation
- [x] Create helper function `ensureIconDirectories()`:
  - [ ] Check if `resources/icons/` exists
  - [ ] Create `resources/icons/twotone/`
  - [ ] Create `resources/icons/rounded/`
  - [ ] Create `resources/icons/outlined/`
  - [ ] Create `resources/icons/_errors/` (for failed conversions)
  - [ ] Set directory permissions (read/write)
  - [ ] Return success/failure bool
- [x] Call `ensureIconDirectories()` before first save:
  - [ ] In CLI mode (main.cpp)
  - [ ] In GUI mode (IconDownloaderDialog)

### 5.2 File Save Logic
- [x] Implement `saveIconToFile(QString theme, QString name, QString svgData)`:
  - [ ] Construct file path: `resources/icons/{theme}/{name}.svg`
  - [ ] Check if file exists → show overwrite confirmation (GUI) or skip (CLI)
  - [ ] Validate SVG using `SvgConverter::validate()`
  - [ ] Write SVG to file using `QFile`
  - [ ] Handle file system errors (permissions, disk full)
  - [ ] Log success/failure to spdlog
  - [ ] Return success/failure bool

### 5.3 Error File Management
- [x] On conversion failure, save original SVG:
  - [ ] Path: `resources/icons/_errors/{name}_{theme}_original.svg`
  - [ ] Include error message in separate `.txt` file
  - [ ] Log error with file path

---

## 6. Testing and Validation

### 6.1 Unit Tests
- [x] Create `tests/unit/test_icon_downloader.cpp`:
  - [ ] Test URL construction for common icons
  - [ ] Test category mapping (save → content, open → file)
  - [ ] Test custom source URL
  - [ ] Mock QNetworkAccessManager for download tests
- [x] Create `tests/unit/test_svg_converter.cpp`:
  - [ ] Test TwoTone conversion (2 colors)
  - [ ] Test Outlined conversion (1 color)
  - [ ] Test minification (remove comments)
  - [ ] Test validation (invalid SVG)
  - [ ] Test edge cases (no fill attributes, gradients)

### 6.2 Integration Testing
- [x] Test CLI workflow end-to-end:
  - [ ] `kalahari --dev --get-icon save`
  - [ ] Verify 3 files created in correct directories
  - [ ] Verify SVG contains `{COLOR_PRIMARY}` and `{COLOR_SECONDARY}`
  - [ ] Verify file size reasonable (< 10KB)
- [x] Test GUI workflow:
  - [ ] Launch with `--dev`, open Icon Downloader
  - [ ] Download icon, verify preview
  - [ ] Save, verify file created
  - [ ] Test overwrite confirmation
- [x] Test Appearance settings:
  - [ ] Change primary color, verify preview updates
  - [ ] Change secondary color, verify preview updates
  - [ ] Reset colors, verify defaults restored
  - [ ] Apply, restart app, verify colors persisted

### 6.3 Manual Testing Checklist
- [x] Test network error handling (disconnect internet)
- [x] Test invalid icon name (typo)
- [x] Test custom source URL (different CDN)
- [x] Test batch download (5+ icons)
- [x] Test overwrite confirmation (download same icon twice)
- [x] Test file system errors (read-only directory)
- [x] Test SVG preview rendering (all 3 themes)
- [x] Test color picker UI (HSV, RGB, hex input)

---

## 7. Documentation and Cleanup

### 7.1 Code Documentation
- [x] Add Doxygen comments to all public methods:
  - [ ] `IconDownloader` class
  - [ ] `SvgConverter` class
  - [ ] `IconDownloaderDialog` class
  - [ ] All CLI flag parsing code
- [x] Add inline comments for complex logic:
  - [ ] SVG parsing and placeholder replacement
  - [ ] Category mapping algorithm
  - [ ] File overwrite logic

### 7.2 User-Facing Documentation
- [x] Update `project_docs/08_gui_design.md`:
  - [ ] Add section on Icon Management System
  - [ ] Document CLI flags for icon download
  - [ ] Document Dev Tools menu
- [x] Create `docs/dev_tools/icon_downloader.md`:
  - [ ] Usage guide for developers
  - [ ] List of supported Material Design icons
  - [ ] Custom source URL examples

### 7.3 CMakeLists.txt Update
- [x] Add new source files to `CMakeLists.txt`:
  - [ ] `src/core/utils/icon_downloader.cpp`
  - [ ] `src/core/utils/svg_converter.cpp`
  - [ ] `src/gui/dialogs/icon_downloader_dialog.cpp`
- [x] Ensure Qt6::Network is linked (for QNetworkAccessManager)
- [x] Ensure Qt6::Xml is linked (for QDomDocument)

### 7.4 Cleanup and Review
- [x] Remove debug logging statements (keep only info/warn/error)
- [x] Check for memory leaks (Qt parent-child ownership)
- [x] Verify all TODO comments addressed
- [x] Run `clang-format` on all modified files
- [x] Build in Release mode and test
- [x] Verify no compiler warnings

---

## 8. Completion Checklist

- [x] All 7 sections above completed
- [x] Unit tests passing (100% coverage for core classes)
- [x] Integration tests passing
- [x] Manual testing checklist completed
- [x] CLI works: `kalahari --dev --get-icon save` downloads 3 variants
- [x] GUI works: Dev Tools → Icon Downloader downloads and previews icons
- [x] Appearance Settings: Icon colors configurable with live preview
- [x] Files saved correctly: `resources/icons/twotone/save.svg` exists
- [x] SVG conversion works: Placeholders `{COLOR_PRIMARY}` present
- [x] Documentation updated
- [x] Code reviewed and formatted
- [x] No regressions in existing functionality
- [x] Ready for Task #00023 (Built-in Icon Set)

---

**Total Estimated Time:** 4-6 hours
**Task Status:** 🔄 NOT STARTED (awaiting approval)
