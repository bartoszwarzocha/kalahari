# Changelog

All notable changes to the Kalahari project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Changed

- EditorPanel upgraded from QPlainTextEdit to QTextEdit (Phase E)
- Added per-chapter dirty tracking with asterisk indicator in tab title
- Added Save All functionality for dirty chapters
- NavigatorPanel: Added "Other Files" section for standalone files (Phase F)

### Added

- StandaloneInfoBar widget for standalone file mode notification (Phase F)
- AddToProjectDialog for adding standalone files to project (Phase F)
- Menu item "File > Open > File..." (Ctrl+Shift+O) for opening standalone files (Phase F)

- **OpenSpec #00033:** Project File System - Solution-like Architecture - 2025-12-12
  - ProjectManager class for solution-like project architecture
  - WorkMode enum (NoDocument, ProjectMode, StandaloneMode)
  - Project folder structure with .klh manifest files
  - Core API: createProject(), openProject(), closeProject(), saveManifest()
  - Path helpers: getContentPath(), getMetadataPath(), getMindmapsPath(), etc.
  - Automatic folder structure creation (content/, metadata/, mindmaps/, timelines/, resources/, .kalahari/)
  - Files added: `project_manager.h`, `project_manager.cpp`

- **OpenSpec #00032:** Theme & Icons Optimization - 2025-12-11
  - **Problem solved:** Triple theme update path causing 3x IconRegistry updates on startup
  - **Problem solved:** Panel header icons not updating on color change
  - **Problem solved:** Fragmented icon color management across components
  - **Architecture improvements:**
    - ArtProvider is now single source of truth for theme propagation
    - Removed redundant ThemeManager->IconRegistry direct connection
    - Removed duplicate setThemeColors() call in MainWindow::onThemeChanged()
    - Removed redundant startup initialization in MainWindow constructor
    - Connected ArtProvider::resourcesChanged to MainWindow::refreshDockIcons()
  - **Bug fixes:**
    - CRITICAL-1: Triple theme update path (3x IconRegistry updates) - Fixed
    - CRITICAL-2: Redundant startup initialization - Fixed
    - MAJOR-1: Hardcoded QColor in BusyIndicator - Fixed (uses theme.palette.shadow)
    - MAJOR-2: Non-themed dock button icons - Fixed (registered dock.float, dock.close)
    - MAJOR-3: ToolbarManagerDialog missing auto-refresh - Fixed (added refreshButtonIcons)
  - Files modified: `main_window.cpp`, `main_window.h`, `busy_indicator.cpp`, `toolbar_manager_dialog.cpp`, `toolbar_manager_dialog.h`
  - All 73 tests pass (571 assertions)
  - Commit: `ebd7716`

- **OpenSpec #00031:** Toolbar System - 2025-12-10
  - **ToolbarManagerDialog:** Visual Studio-style 3-column toolbar customization
    - Left panel: Toolbar list with Built-in, User, Plugin sections
    - Center panel: Command browser with category filter and search
    - Right panel: Current toolbar commands with reordering controls
  - **Toolbar Customization Features:**
    - Drag & drop and double-click to add commands
    - Move up/down, remove, add separator buttons
    - Create/delete/rename user-defined toolbars
    - Reset to defaults functionality
    - Overflow menu (chevron) for narrow toolbars
    - Right-click context menu on toolbar area
  - **ToolbarManager API Extensions:**
    - `getToolbarCommands()`, `setToolbarCommands()` - customization
    - `createUserToolbar()`, `deleteUserToolbar()`, `renameUserToolbar()` - user toolbars
    - `rebuildToolbar()`, `resetToDefaults()` - toolbar management
    - `loadConfigurations()`, `saveConfigurations()` - JSON persistence via SettingsManager
  - Files added: `toolbar_manager_dialog.h`, `toolbar_manager_dialog.cpp`
  - Files modified: `toolbar_manager.h`, `toolbar_manager.cpp`, `CMakeLists.txt`

- **OpenSpec #00030:** Menu System Review & Cleanup - 2025-12-10
  - **Keyboard Shortcuts:**
    - Panel toggles: F2 (Navigator), F3 (Properties), F4 (Log), F5 (Search), F6 (Assistant)
    - F11 for Full Screen (standard)
    - F1 for Help (standard)
    - Ctrl+F/H for Find/Find & Replace
    - Ctrl+B/I/U for Bold/Italic/Underline
    - Ctrl+W for Close Book
  - **Dynamic Toolbars Submenu:**
    - VIEW/Toolbars now dynamically built by ToolbarManager
    - Removed static CommandRegistry entries (no duplicates)
    - Added "Toolbar Manager..." action
  - **Recent Books Submenu:**
    - New RecentBooksManager singleton class
    - Stores up to 10 recent files in QSettings
    - Dynamic FILE/Recent Books submenu with numbered items (1-9, 0)
    - Auto-removes non-existent files
    - "Clear Recent Files" action
  - Files added: `recent_books_manager.h`, `recent_books_manager.cpp`
  - Files modified: `main_window.cpp`, `main_window.h`, `toolbar_manager.cpp`,
    `register_commands.hpp`, `CMakeLists.txt`

- **OpenSpec #00029:** Fix Theme and Icon Loading on Linux/macOS - 2025-12-10
  - Created ResourcePaths singleton for cross-platform resource discovery
  - Multi-path search: executable dir, macOS bundle, Linux FHS, development fallbacks
  - Updated IconRegistry.loadSVGFromFile() to use ResourcePaths
  - Updated ThemeManager.loadThemeFile() and getAvailableThemes() to use ResourcePaths
  - Changed CMake from configure-time file(COPY) to POST_BUILD commands
  - Added macOS bundle resource copying to Contents/Resources
  - Files added: `resource_paths.h`, `resource_paths.cpp`
  - Files modified: `icon_registry.cpp`, `theme_manager.cpp`, `CMakeLists.txt`, `src/CMakeLists.txt`

- **Task #00013:** Command Registry Qt Migration - 2025-11-21
  - Migrated Command Registry from wxWidgets to Qt6 (6 files, ~670 LOC)
  - Core structures adapted: IconSet (wxBitmap -> QPixmap), KeyboardShortcut (Qt::KeyboardModifiers), Command (added toQAction())
  - CommandRegistry singleton: Framework-agnostic, zero changes needed
  - MenuBuilder created for Qt (150 LOC): buildMenuBar(), buildMenu(), dynamic generation from registry
  - ToolbarBuilder adapted for Qt (82 LOC): buildToolBar(), QAction-based
  - MainWindow refactored: registerCommands() (15 commands), removed 14 QAction* members, replaced createActions/createMenus/createToolbars
  - Registered commands: File (6), Edit (6), Help (2) - all with keyboard shortcuts
  - Architecture benefits: Single source of truth, plugin-friendly, customizable UI foundation
  - Commits: `d3196b2` (Command Registry files), `b518a5d` (MainWindow refactor)
  - Files added: `command.h/cpp`, `command_registry.h/cpp`, `toolbar_builder.h/cpp`, `menu_builder.h/cpp`
  - Files modified: `main_window.h/cpp`, `CMakeLists.txt`

- **Task #00014:** Plugin Integration Foundation (Qt Migration) - 2025-11-21
  - Fixed 3 critical wxWidgets dependencies in plugin system preventing Qt integration
  - **ICommandProvider Interface:** Added new extension point for plugin command registration
    - Plugins can now call getCommands() to return std::vector<gui::Command>
    - Automatic integration with CommandRegistry during plugin activation
    - Commands appear in menus/toolbars via MenuBuilder/ToolbarBuilder
  - **EventBus Qt Migration:** Replaced wxWidgets async event system with Qt6
    - Old: wxTheApp->CallAfter() -> New: QMetaObject::invokeMethod(QApplication::instance(), Qt::QueuedConnection)
    - Thread-safe GUI thread marshalling for async events
    - Fallback: Direct emit if QApplication not available
  - **IPanelProvider Type Safety:** Replaced void* with QWidget* for panel creation
    - Old: void* createPanel(void*) -> New: QWidget* createPanel(QWidget*)
    - pybind11 automatically handles QWidget* for Python plugins
    - Type-safe Qt6 API, no casting required
  - Architecture benefits: Plugin system ready for Phase 2, no wxWidgets dependencies
  - Commit: `3156830` - feat(plugins): Add ICommandProvider, migrate EventBus to Qt6, QWidget* panels
  - Files modified: `extension_points.h` (+68 lines), `event_bus.h/cpp` (~40 lines)
  - Python bindings: Deferred to Phase 2 (Plugin System MVP)

- **Task #00017:** Custom About Dialog (Qt Migration) - 2025-11-22
  - Replaced simple QMessageBox::about() with professional custom AboutDialog
  - **AboutDialog class:** QDialog subclass with tabbed interface (250 LOC)
    - Fixed size: 600x720px, modal behavior
    - Custom banner: 580x100px black background with white "KALAHARI" text (QPainter)
    - 3 tabs: "About", "Third-Party Components", "License"
  - **Tab 1 - About:** Application info (version 0.3.1-alpha), platform support, Qt version, credits, copyright
  - **Tab 2 - Third-Party Components:** Attribution list for 8 libraries (Qt6, nlohmann_json, spdlog, libzip, Catch2, pybind11, Python 3.11, vcpkg)
  - **Tab 3 - License:** Full MIT License text + trademark notice
  - MainWindow integration: Updated onAbout() to use dialogs::AboutDialog
  - OpenSpec validation: First task using OpenSpec framework (change ID: 00017-enhance-about-dialog)
  - Files added: `about_dialog.h` (55 LOC), `about_dialog.cpp` (250 LOC)
  - Files modified: `main_window.cpp`, `CMakeLists.txt`
  - Feature parity with wxWidgets version achieved

- **Task #00018:** Diagnostic Mode with --diag Parameter (Qt Migration) - 2025-11-22
  - Added comprehensive diagnostic mode with 18 debugging tools in 6 categories
  - **Two activation methods:**
    - Command line: `kalahari --diag` (or `-d`) - persistent for session
    - Advanced Settings: Checkbox in new "Advanced" tab (runtime only, not saved, confirmation dialog)
  - **CmdLineParser enhancement:** Added `isDiagnosticMode()` convenience method
  - **SettingsDialog:** New "Advanced" tab with warning label and diagnostic mode toggle
    - Warning: Orange text alert for developer tools
    - Confirmation dialog when enabling (QMessageBox::warning)
    - Signal `diagnosticModeChanged(bool)` connects to MainWindow
  - **MainWindow diagnostic API:**
    - Public: `enableDiagnosticMode()`, `disableDiagnosticMode()`
    - Slot: `onDiagModeChanged(bool)` - responds to Settings checkbox
    - Private: `createDiagnosticMenu()`, `removeDiagnosticMenu()`
  - **Diagnostics menu:** 18 tools in 6 categories (all output to Log Panel via Logger)
    1. **System Information:** System Info, Qt Environment, File System Check
    2. **Application State:** Settings Dump, Memory Statistics, Open Documents Stats
    3. **Core Systems:** Logger Test (5 levels: debug/info/warn/error/critical), Event Bus Test, Plugin Check, Command Registry Dump
    4. **Python Environment:** Python Environment, Import Test, Memory Test, Interpreter Status
    5. **Performance:** Performance Benchmark, Render Statistics
    6. **Quick Actions:** Clear Log, Force Crash (#ifdef _DEBUG only), Memory Leak Test (#ifdef _DEBUG only)
  - **LogPanel enhancement:** Added `clear()` method for diagnostic tool support
  - **Architecture:** Menu created OUTSIDE Command Registry (direct QAction), no keyboard shortcuts
  - OpenSpec validation: Change ID `00018-diagnostic-mode`
  - Files modified: `cmd_line_parser.h/cpp`, `settings_dialog.h/cpp`, `main_window.h/cpp`, `main.cpp`, `log_panel.h/cpp`
  - Feature parity with wxWidgets diagnostic system achieved + Python tools added

- **Task #00019:** Toolbar Manager System with Icons - 2025-11-22
  - Replaced single hardcoded toolbar with flexible multi-toolbar architecture (5 toolbars)
  - **ToolbarManager class:** Centralized management of multiple toolbars (~300 LOC)
    - Configuration-driven design with ToolbarConfig struct (id, label, area, visibility, commands)
    - 5 toolbars: File, Edit, Book, View, Tools (all visible by default in Qt::TopToolBarArea)
    - State persistence: toolbar visibility saved/restored via QSettings
    - View menu integration: 5 checkable actions for toolbar visibility toggles
  - **IconSet enhancements:** Added 2 factory methods for Phase 0 (no icon files yet)
    - `IconSet::fromStandardIcon(QStyle::StandardPixmap)` - Qt standard icons (New, Open, Save, Undo, Redo, etc.)
    - `IconSet::createPlaceholder(letter, color)` - Colored squares with white letters (QPainter-generated)
    - Pre-rendered in 3 sizes: 16x16 (menu), 24x24 (toolbar default), 32x32 (large toolbar)
  - **REG_CMD_TOOL_ICON macro:** New 9-parameter registration macro for commands with icons
  - **Command assignments:** 25 toolbar commands assigned icons (6 File + 7 Edit + 4 Book + 5 View + 3 Tools)
    - Qt standard icons: SP_FileIcon, SP_DirOpenIcon, SP_DialogSaveButton, SP_ArrowBack, SP_ArrowForward, etc.
    - Placeholder icons: Colored squares for Cut (red), Copy (blue), Paste (green), Select All (purple), etc.
  - **MainWindow integration:**
    - Constructor: Initialize m_toolbarManager (replaces m_fileToolbar)
    - createToolbars(): Call m_toolbarManager->createToolbars(registry)
    - createDocks(): Call m_toolbarManager->createViewMenuActions(m_viewMenu)
    - closeEvent(): Save toolbar state with m_toolbarManager->saveState()
    - showEvent(): Restore toolbar state with m_toolbarManager->restoreState()
  - **View menu structure:**
    - "Panels" submenu (Navigator, Properties, Log, Search, Assistant)
    - "Reset Layout" action
    - Separator
    - 5 toolbar toggle actions (File Toolbar, Edit Toolbar, Book Toolbar, View Toolbar, Tools Toolbar)
  - **Architecture benefits:** Scalable toolbar system, user-customizable layout, toolbar/icon separation ready for Phase 2
  - OpenSpec validation: Change ID `00019-toolbar-manager`
  - Files added: `toolbar_manager.h` (160 LOC), `toolbar_manager.cpp` (300 LOC), `register_commands.hpp` (88 LOC)
  - Files modified: `command.h/cpp` (IconSet factory methods), `main_window.h/cpp` (ToolbarManager integration)
  - Toolbar count: 1 -> 5, Commands with icons: 8 -> 25

- **Task #00020:** Icon Downloader Tool with Dev Tools Menu - 2025-11-23
  - Comprehensive icon management system for Material Design Icons (Google's repository, Apache 2.0 license)
  - **Two operation modes:**
    - GUI: IconDownloaderDialog with live preview
    - CLI: Batch download tool for AI/automation (--cli --dev --get-icon/--get-icons)
  - **IconDownloader class:** QNetworkAccessManager-based HTTP downloader (~200 LOC)
    - Supports 3 icon themes: TwoTone (default), Rounded, Outlined
    - Category mapping for 36 common icons (action, file, content, etc.)
    - Material Design URL construction: `{base}/{category}/{icon_name}/{variant}/24px.svg`
    - Signals: downloadComplete(), downloadError(), progress()
    - 10-second timeout per icon, HTTPS support via OpenSSL
  - **SvgConverter class:** Opacity-based color placeholder conversion (~150 LOC)
    - Converts Material Design SVGs to template format with {COLOR_PRIMARY}/{COLOR_SECONDARY}
    - Logic: opacity > 0.5 -> {COLOR_PRIMARY}, opacity <= 0.5 -> {COLOR_SECONDARY}
    - QDomDocument API for safe XML manipulation (avoids regex corruption)
    - Validation: Syntax checking, malformed XML detection
    - Supports elements: path, circle, rect, polygon, polyline, ellipse, line
  - **IconDownloaderDialog:** Qt6 modal dialog with live preview (~400 LOC)
    - 700x600px window with icon name input, theme checkboxes (3), progress bar, QSvgWidget preview
    - Auto-creates resources/icons/{theme}/ directories
    - Downloads sequentially, displays progress (1/3, 2/3, 3/3)
    - Old-style SIGNAL/SLOT macros for Windows DLL compatibility (avoids staticMetaObject export)
    - Preview shows TwoTone variant with color replacement visualization
  - **CLI mode:** Command-line icon download tool for AI/developer use
    - Flags: `--cli` (required), `--dev` (required), `--get-icon <name>`, `--get-icons <list>`, `--themes <list>`, `--source <url>`
    - Example: `kalahari --cli --dev --get-icons "save,search,folder_open" --themes "twotone,rounded"`
    - DownloadHelper class in main.cpp for Qt signal/slot cross-DLL support (includes main.moc)
    - Synchronous download with QEventLoop (15-second timeout)
    - Success summary: "X downloaded, Y failed"
  - **Dev Tools menu:** New top-level menu (visible only with --dev flag)
    - MainWindow API: enableDevMode(), disableDevMode(), isDevMode(), createDevToolsMenu()
    - Currently 1 tool: "Icon Downloader" (opens IconDownloaderDialog)
    - Architecture: Menu created OUTSIDE Command Registry (direct QAction)
  - **Appearance Settings:** Icon color configuration in Settings dialog
    - SettingsManager methods: getIconColorPrimary(), setIconColorPrimary(), getIconColorSecondary(), setIconColorSecondary()
    - Defaults: Primary #333333 (dark gray), Secondary #999999 (light gray)
    - SettingsDialog: 2 new controls in Appearance tab (QPushButton color pickers)
    - QColorDialog integration for color selection
    - Persistent storage in settings.json under "icons.colorPrimary" and "icons.colorSecondary"
  - **Command-line parser enhancement:** Extended with addOption() for options with values
    - New method: getOptionValue(name) -> QString
    - Qt's showHelp() integration (works on Windows GUI apps automatically)
  - **Unit tests:** 50+ test cases for SvgConverter and Settings integration
    - test_svg_converter.cpp: Opacity threshold logic, multi-path conversion, element handling, validation, edge cases
    - test_settings_manager.cpp: Icon color defaults, persistence, get/set operations
  - **Dependencies:** Qt6Network, Qt6Xml, Qt6SvgWidgets, OpenSSL (libssl-3-x64.dll, libcrypto-3-x64.dll, qopensslbackendd.dll)
  - OpenSpec validation: Change ID `00020-icon-downloader`
  - Files added: `icon_downloader.h/cpp` (~200 LOC), `svg_converter.h/cpp` (~150 LOC), `icon_downloader_dialog.h/cpp` (~400 LOC), `test_svg_converter.cpp` (175 LOC)
  - Files modified: `settings_manager.h/cpp` (+20 lines), `settings_dialog.h/cpp` (+120 lines), `cmd_line_parser.h/cpp` (+40 lines), `main_window.h/cpp` (+50 lines), `main.cpp` (+140 lines), `vcpkg.json` (qtbase network/openssl features), `CMakeLists.txt`, `tests/CMakeLists.txt`
  - Material Design integration: Ready for runtime theming system (Phase 2)

- **Task #00021:** IconRegistry Runtime System with Theme Colors - 2025-11-24
  - Complete runtime icon management system with theme color support and customization
  - **IconRegistry class:** Singleton managing SVG icon loading, caching, and theming (~550 LOC)
    - Supports runtime theme colors: PRIMARY (default #424242) and SECONDARY (default #757575)
    - TwoTone icon support: {COLOR_PRIMARY} and {COLOR_SECONDARY} placeholders replaced at runtime
    - Per-icon customization: Override default SVG paths and color overrides for specific icons
    - QPixmap caching: Cache key format `{actionId}_{theme}_{size}_{primaryColor}_{secondaryColor}`
    - Cache invalidation on theme/size changes for efficient memory usage
    - Settings persistence: Stores theme colors, icon sizes, and per-icon customizations in JSON
  - **Data structures:**
    - IconDescriptor: Stores default SVG path, user override path, color overrides, label
    - ThemeConfig: Stores primary/secondary colors and theme name (DEFAULT_LIGHT/DEFAULT_DARK presets)
    - IconSizeConfig: Stores sizes for toolbar (24), menu (16), panel (20), dialog (32)
  - **IconRegistry API:**
    - registerIcon(actionId, svgPath, label) - Register icon with default SVG path
    - getIcon(actionId, theme, size) -> QIcon - Load, color-replace, render, and cache SVG
    - setThemeColors(primary, secondary, name) - Change runtime theme colors (invalidates cache)
    - setSizes(config) - Configure icon sizes for different UI contexts
    - loadFromSettings() / saveToSettings() - Persist icon customizations
  - **SVG processing pipeline:**
    1. loadSVGFromFile() - Read SVG content from disk
    2. replaceColorPlaceholders() - Replace {COLOR_PRIMARY}/{COLOR_SECONDARY} with QColor::name()
    3. renderSVGToPixmap() - QSvgRenderer -> QPixmap at specified size
    4. Cache QPixmap for reuse
  - **Command Registry integration:**
    - IconSet::fromRegistry(actionId, theme) factory method - Load icons in 3 sizes (16/24/32) from IconRegistry
    - Replaces IconSet::fromStandardIcon() and IconSet::createPlaceholder() (Phase 0 temporary icons)
  - **MainWindow integration:**
    - registerCommands(): 16 icons registered (file.new, file.open, file.save, file.saveAs, file.exit, edit.undo, edit.redo, edit.cut, edit.copy, edit.paste, edit.delete, edit.selectAll, edit.find, edit.settings, help.about, help.help)
    - All icons loaded from resources/icons/twotone/*.svg using IconRegistry
  - **main.cpp initialization:**
    - IconRegistry::getInstance().initialize() - Load theme/sizes/customizations from settings on startup
  - **Settings persistence:**
    - JSON keys: icons/theme/primary_color, icons/theme/secondary_color, icons/theme/name
    - Icon sizes: icons/sizes/toolbar, icons/sizes/menu, icons/sizes/panel, icons/sizes/dialog
    - Per-icon customizations: icons/custom/{actionId}/svg_path, icons/custom/{actionId}/primary_color, icons/custom/{actionId}/secondary_color
  - **Build adjustments:**
    - Temporarily disabled IconDownloader in kalahari_core library (moc linkage issue #if 0 wrappers in main.cpp and icon_downloader_dialog.cpp)
    - SKIP_AUTOMOC property on main.cpp to avoid Q_OBJECT inside #if 0 blocks
    - Qt6::Svg and Qt6::Gui dependencies added to kalahari_core library
  - **Architecture benefits:** Runtime theme switching foundation, per-icon customization ready for Settings UI (Phase 2), complete separation from Qt Resource System (.qrc)
  - OpenSpec validation: Change ID `00021-icon-registry-runtime`
  - Files added: `icon_registry.h` (~250 LOC), `icon_registry.cpp` (~550 LOC)
  - Files modified: `command.h/cpp` (IconSet::fromRegistry factory), `main_window.cpp` (icon registration), `main.cpp` (IconRegistry initialization), `src/CMakeLists.txt` (Qt6::Svg, Qt6::Gui, SKIP_AUTOMOC), `tests/CMakeLists.txt` (disable icon_downloader)
  - Total LOC added: ~800 lines
  - Manual testing: Application starts successfully, 16 icons registered, settings persisted

- **Task #00023:** Theme System Foundation with QPalette Integration - 2025-11-25
  - Complete theme architecture redesign using Qt QPalette for native widget styling
  - **Problem solved:** QSpinBox/QComboBox arrows invisible in dark theme (Qt Fusion style requires QPalette colors)
  - **Theme JSON architecture extended:**
    - Added "palette" section with 16 QPalette color roles (window, windowText, base, alternateBase, text, button, buttonText, highlight, highlightedText, light, midlight, mid, dark, shadow, link, linkVisited)
    - Light.json and Dark.json updated to version 1.1 with full palette definitions
    - Backward compatibility: Auto-generates palette from "colors" section if "palette" missing
  - **Theme struct (theme.h) extended:**
    - Added Palette sub-struct with 16 QColor members mapping to QPalette::ColorRole
    - Added toQPalette() method converting Theme::Palette to QPalette object
    - Sets colors for all three QPalette::ColorGroup states (Active, Inactive, Disabled)
  - **ThemeManager (theme_manager.cpp) enhanced:**
    - applyTheme() now calls QApplication::setPalette() with theme-derived QPalette
    - Removed broken dark.qss file approach (CSS border-triangles don't work in Qt)
    - Theme colors in ONE place (JSON), applied system-wide via QPalette
  - **Architecture benefits:**
    - Qt Fusion style uses QPalette for ALL widget colors (spinbox arrows, combobox dropdowns, scrollbars)
    - No hardcoded colors in QSS needed - QPalette handles everything
    - Runtime theme switching works correctly with all native widgets
    - Log panel colors integrated (theme.log.info, theme.log.debug, theme.log.background)
  - OpenSpec validation: Change ID `00023-theme-system-foundation`
  - Files modified: `theme.h` (+50 LOC Palette struct), `theme.cpp` (+80 LOC toQPalette + parsing), `theme_manager.cpp` (+5 LOC setPalette), `Dark.json` (+17 lines palette), `Light.json` (+17 lines palette)
  - Total architecture LOC: ~150 lines
  - Manual testing: Dark theme spinbox/combobox arrows visible, all controls styled correctly

- **Task #00024:** Settings Dialog Refactor with Hierarchical Navigation - 2025-11-25
  - Complete Settings Dialog restructure from flat tabs to hierarchical tree + stacked panels
  - **Layout change:** QTabWidget (2 tabs) -> QTreeWidget (left sidebar) + QStackedWidget (right panels)
  - **14 settings pages implemented:**
    - **Appearance:** General, Theme, Icons (3 pages)
    - **Editor:** General, Spelling, Auto-correct, Completion (4 pages)
    - **Files:** Backup, Auto-save, Import/Export (3 pages)
    - **Network:** Cloud Sync, Updates (2 pages)
    - **Advanced:** General, Performance (2 pages)
  - **PageIndex enum:** Extended from 7 to 14 entries for direct page access
  - **Tree navigation:** Categories expand to show sub-items, single-click switches page
  - **Assistant icon:** Changed from "smart_toy" (robot) to "pets" (animal mascot) per brand guidelines
  - **Icon downloads:** Added "pets" icon for all 4 Material Design styles (twotone, outlined, rounded, filled)
  - OpenSpec validation: Change ID `00024-settings-dialog-refactor`
  - Files modified: `settings_dialog.h` (+30 LOC PageIndex enum), `settings_dialog.cpp` (+200 LOC tree structure + 11 new pages), `main_window.cpp` (assistant icon registration), `download_all_icons.sh` (pets icon)
  - Manual testing: All 14 pages accessible, tree navigation works correctly

- **OpenSpec #00025:** Theme-Icon Integration - 2025-11-26
  - Complete integration between ThemeManager and IconRegistry for unified icon theming
  - **Per-theme icon color storage:**
    - SettingsManager: Added `iconColors.{theme}.primary` and `iconColors.{theme}.secondary` JSON keys
    - Each theme (Light, Dark) stores its own icon color preferences
    - New API: `hasCustomIconColorsForTheme()`, `getIconColorPrimaryForTheme()`, `setIconColorPrimaryForTheme()`
    - User overrides persist independently per theme
  - **ThemeManager per-theme color loading:**
    - On startup: Loads saved per-theme icon colors from SettingsManager
    - On theme switch: Loads per-theme colors for the new theme
    - `switchTheme()` applies custom colors after loading base theme
    - Color overrides applied via `applyColorOverrides()` method
  - **Settings Dialog icon color controls:**
    - Appearance/Icons page: Primary and secondary color pickers (QPushButton + QColorDialog)
    - Colors synchronized with ThemeManager and IconRegistry
    - "Apply" updates icons in real-time
    - Theme switch auto-loads saved per-theme colors
  - **GUI icon refresh mechanism:**
    - `ToolbarManager::refreshIcons()` - stores cmdId in QAction::data(), refreshes from IconRegistry
    - `MenuBuilder::refreshIcons()` - recursive submenu traversal, same pattern as toolbar
    - `MainWindow::onThemeChanged()` - calls both refresh methods after theme/color change
    - All toolbar AND menu icons update immediately on color change
  - **Architecture established:**
    - ThemeManager emits `themeChanged(const Theme&)` signal
    - GUI components (ToolbarManager, MenuBuilder) listen and refresh their icons
    - IconRegistry provides fresh icons with current theme colors
    - **Limitation identified:** Each GUI component needs own refreshIcons() - centralization planned for next task
  - OpenSpec validation: Change ID `00025-theme-icon-integration`
  - Files modified: `settings_manager.h/cpp` (+60 LOC per-theme storage), `settings_dialog.h/cpp` (+80 LOC color controls), `theme_manager.cpp` (+40 LOC per-theme loading), `menu_builder.h/cpp` (+70 LOC refreshIcons), `main_window.h/cpp` (+15 LOC MenuBuilder integration)
  - Manual testing: Per-theme colors persist, toolbar + menu icons refresh on color change

- **OpenSpec #00027:** Centralized Theme Color Configuration - 2025-12-07
  - Restored feature originally lost when `feature/claude-workflow-redesign` branch was never merged
  - **Centralized UI for all theme colors** in Settings > Appearance > Theme
  - **ColorConfigWidget:** Reusable color picker widget (~100 LOC)
    - QPushButton showing current color with QColorDialog
    - Hex code label next to button
    - Emits colorChanged(QColor) signal
  - **Theme page redesigned:**
    - Theme selector dropdown at top
    - "Icon Colors" group: Primary and Secondary color pickers
    - "Log Panel Colors" group: 7 color pickers (Trace, Debug, Info, Warning, Error, Critical, Background)
    - "Reset to Theme Defaults" button
  - **Per-theme log color storage:**
    - SettingsManager: `getLogColorForTheme()`, `setLogColorForTheme()` methods
    - Keys: trace, debug, info, warning, error, critical, background
    - JSON structure: `logColors.{theme}.{colorKey}`
  - **SettingsData extended:**
    - 7 QColor fields for log levels
    - Included in operator!= comparison for Apply detection
  - **Bug fixes applied same day:**
    - Fix: Dark theme showed #000000 for all log colors (`getCurrentSettingsAsData()` missing log color loading)
    - Fix: Auto-detect and clear corrupted log color settings (all 7 identical detection)
    - Fix: LogPanel not updating colors after settings change (`applyLogColors()` method added)
    - Fix: UI freeze when changing log colors (color caching in LogPanel)
    - Perf: Batch UI updates with 100ms timer (faster --diag mode startup)
  - **Files created:** `color_config_widget.h/cpp` (~100 LOC)
  - **Files modified:** `settings_manager.h/cpp`, `settings_data.h`, `settings_dialog.h/cpp`, `log_panel.h/cpp`, `main_window.cpp`, `CMakeLists.txt`
  - **Commits:** `b2fb36c` (feat), `b160dbf` (fix), `cd5f0bc` (fix), `627d92f` (fix), `cf17d1b` (fix), `6d11d28` (perf)
  - Manual testing: All color pickers functional, LogPanel colors update immediately

- **OpenSpec #00028:** Complete Theme Styling System - 2025-12-08
  - Permanent theming foundation using hybrid QPalette + QSS (Qt Style Sheets) architecture
  - **Key discovery:** QPalette + Fusion style handles ALL widget colors automatically (scrollbars, comboboxes, spinboxes, etc.)
  - **QPalette extended to 20 color roles:**
    - Added `toolTipBase` - Tooltip background color
    - Added `toolTipText` - Tooltip text color
    - Added `placeholderText` - Input field placeholder color
    - Added `brightText` - High contrast text (on dark backgrounds)
  - **StyleSheet class:** Minimal QSS generator (~68 LOC)
    - Only generates QSS for tooltips (the only exception requiring explicit QSS)
    - Color utility functions: darken(), lighten(), withAlpha()
    - Simplified from original ~445 LOC plan (most widget styles removed as unnecessary)
  - **ThemeManager integration:**
    - Added `themeStyleChanged()` signal for real-time refresh
    - StyleSheet::generate() called in applyTheme() and applyColorOverrides()
    - QSS applied via QApplication::setStyleSheet()
  - **Settings UI expansion:**
    - "UI Colors" group with 4 new color pickers (toolTipBase, toolTipText, placeholderText, brightText)
    - "Palette Colors" group with 16 standard palette colors
    - QScrollArea wrapper for Theme page (handles many color options)
    - Per-theme color storage in SettingsManager
  - **Hardcoded styles cleanup:**
    - Replaced hardcoded colors in settings_dialog.cpp
    - Replaced hardcoded colors in color_config_widget.cpp
    - Replaced hardcoded colors in icon_downloader_dialog.cpp
    - Replaced hardcoded colors in main_window.cpp (dock panel titles)
  - **Additional fixes:**
    - Fixed ComboBox/SpinBox arrows (removed interfering custom QSS)
    - Fixed dropdown selection colors (correct QPalette roles)
    - Added icons to dock panel title bars via custom title bar widget
    - Panel icons refresh on theme change
    - Fixed ColorConfigWidget alignment (40/60 split)
    - Theme switching now loads saved user colors per theme
  - **Files created:** `stylesheet.h` (~52 LOC), `stylesheet.cpp` (~68 LOC)
  - **Files modified:** `theme.h`, `theme.cpp`, `theme_manager.h`, `theme_manager.cpp`, `settings_dialog.cpp`, `Light.json`, `Dark.json`, `main_window.cpp`, `color_config_widget.cpp`, `CMakeLists.txt`
  - **Architecture:** QPalette (20 roles) + Fusion style = 99% of widgets; QSS = tooltips only
  - Manual testing: All widgets styled correctly in Light and Dark themes, real-time color updates work

- **OpenSpec #00026:** ArtProvider Central Visual Resource Manager - 2025-11-27
  - Central facade pattern for all visual resources (icons, cursors, animations)
  - **ArtProvider class:** Singleton managing all visual resource access (~300 LOC)
    - Central API: `getIcon()`, `getCursor()`, `getAnimation()` with unified interface
    - Delegates to IconRegistry, future CursorManager, AnimationManager
    - resourcesChanged signal for GUI component synchronization
    - **Batch mode optimization:** `beginBatchUpdate()` / `endBatchUpdate()` to coalesce signal emissions
    - Prevents freeze during settings apply (~11 signals → 1 signal, ~1340 re-renders → ~134)
  - **SettingsDialog integration:**
    - Uses ArtProvider batch mode during Apply to prevent theme change freeze
    - Icon/cursor/toolbar updates coalesced into single resourcesChanged emission
  - **BusyIndicator pattern:** Framework for async operations with progress indication
    - QProgressDialog integration for long-running operations
    - Cancelable operations with progress callback
  - **SettingsData centralization:**
    - All settings in single struct with operator!=() for dirty detection
    - Collected once from UI controls, compared with current state
    - Only changed settings applied (performance optimization)
  - OpenSpec validation: Change ID `00026-art-provider-central`
  - Files added: `art_provider.h/cpp` (~300 LOC), `busy_indicator.h/cpp`
  - Files modified: `settings_dialog.h/cpp` (batch mode), `main_window.h/cpp` (onApplySettings refactor)

- **OpenSpec #00024:** Enhanced Log Panel with Settings UI - 2025-11-27
  - Complete LogPanel enhancement from placeholder to full-featured diagnostic log viewer
  - **LogPanelSink class:** Custom spdlog sink for Qt integration (~100 LOC)
    - Thread-safe Qt signal emission from any thread (spdlog callbacks)
    - Emits `logMessage(int level, QString message)` signal
    - Registered in main.cpp before MainWindow creation
  - **LogPanel enhancements:**
    - QPlainTextEdit → QTextEdit for rich text (colored output)
    - Ring buffer: `std::deque<LogEntry>` with configurable size (default 500, range 1-1000)
    - Theme-aware colors: 6 log levels + background from Theme struct
    - Vertical toolbar: Options, Open Folder, Copy, Clear buttons
    - **Visibility optimization:** Skip UI updates when panel hidden, rebuild on showEvent()
  - **Mode-based visibility:**
    - Normal mode: LogPanel hidden, TRACE/DEBUG filtered out
    - --diag/--dev mode: LogPanel visible, all log levels shown
  - **Settings UI:**
    - New "Advanced / Log" page in Settings Dialog
    - Buffer size spinner (1-1000 lines)
    - Settings persist via SettingsManager (log.bufferSize)
  - **Theme integration:**
    - Log colors from Theme JSON (theme.log.trace, debug, info, warning, error, critical, background)
    - rebuildDisplay() on theme change with single setHtml() for performance
  - OpenSpec validation: Change ID `00024-log-panel-enhanced`
  - Files added: `log_panel_sink.h/cpp` (~100 LOC)
  - Files modified: `log_panel.h/cpp` (+400 LOC), `settings_dialog.h/cpp` (Advanced/Log page), `main_window.h/cpp` (mode flags), `main.cpp` (sink registration)

### Changed

- **ROADMAP.md restructured (2025-11-26):**
  - Removed ALL task numbers from ROADMAP (OpenSpec is now source of truth for tasks)
  - Updated section 1.7 "Theme & Icon System" with current architecture documentation
  - Added "Key Milestones" entry for Theme & Icon System Foundation
  - Changed Notes section: "OpenSpec Tasks" replaces "Task Numbering"
  - Document version bumped to 2.0

---

## [0.3.0-alpha] - 2025-11-20

