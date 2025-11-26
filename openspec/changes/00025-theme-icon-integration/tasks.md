# Tasks: Theme-Icon Integration

**Status:** ✅ COMPLETE (2025-11-26)

## Implementation Plan

### Phase 1: Fix SVG Icons (30 min)

- [x] **1.1** Delete all non-templated icons from `resources/icons/twotone/`
- [x] **1.2** Create list of icons to re-download (from main_window.cpp registrations)
- [x] **1.3** Re-download icons using CLI mode (SvgConverter pipeline)
- [x] **1.4** Verify all icons have placeholders

### Phase 2: Connect IconRegistry to ThemeManager (30 min)

- [x] **2.1** Make IconRegistry a QObject (add Q_OBJECT macro)
- [x] **2.2** Add onThemeChanged slot
- [x] **2.3** Connect to ThemeManager in initialize()
- [x] **2.4** Implement onThemeChanged slot

### Phase 3: Remove Hardcoded Colors (15 min)

- [x] **3.1** Remove DEFAULT_LIGHT/DEFAULT_DARK static constants
- [x] **3.2** Update resetTheme() to use ThemeManager

### Phase 4: GUI Icon Refresh on Theme Change (added 2025-11-26)

- [x] **4.1** ToolbarManager::refreshIcons() - stores cmdId in QAction::data(), refreshes icons
- [x] **4.2** MenuBuilder::refreshIcons() - same pattern, recursive for submenus
- [x] **4.3** MainWindow::onThemeChanged() - calls both refreshIcons() methods
- [x] **4.4** Per-theme icon color storage in SettingsManager

### Phase 5: Testing (15 min)

- [x] **5.1** Build and run application
- [x] **5.2** Verify all toolbar icons display correctly
- [x] **5.3** Open Settings → Appearance → Icons
- [x] **5.4** Change icon primary/secondary colors
- [x] **5.5** Click Apply - verify toolbar AND menu icons update immediately
- [x] **5.6** Switch theme (Light/Dark) - verify colors switch per-theme
- [x] **5.7** Restart application, verify per-theme colors persist

## Verification Checklist

- [x] All icons have `{COLOR_PRIMARY}` and `{COLOR_SECONDARY}` placeholders
- [x] IconRegistry connected to ThemeManager::themeChanged
- [x] Theme switch in Settings updates icon colors (toolbar + menu)
- [x] Per-theme icon colors saved/restored correctly
- [x] Build succeeds on Windows

## Files Modified

1. `include/kalahari/core/icon_registry.h` - ThemeManager connection
2. `src/core/icon_registry.cpp` - onThemeChanged implementation
3. `include/kalahari/gui/menu_builder.h` - Added refreshIcons() declaration
4. `src/gui/menu_builder.cpp` - Added setData(cmdId), refreshIcons() implementation
5. `include/kalahari/gui/main_window.h` - Added MenuBuilder* member
6. `src/gui/main_window.cpp` - Store MenuBuilder, call refreshIcons() in onThemeChanged()
7. `src/core/settings_manager.cpp` - Per-theme icon color storage

## Known Limitation (Task #00026)

Current implementation requires each GUI component (ToolbarManager, MenuBuilder) to:
1. Store cmdId in QAction::data()
2. Implement own refreshIcons() method
3. Connect to ThemeManager::themeChanged

**Task #00026** will centralize this - single signal from IconRegistry that all GUI components can listen to.
