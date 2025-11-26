# Tasks: Centralized Icon Management System (ArtProvider)

**Status:** IN PROGRESS (Architecture redesign approved)

## Implementation Plan

### Phase 1: ArtProvider Core (~60 min)

- [ ] **1.1** Create `include/kalahari/core/art_provider.h`
  - ArtProvider class declaration
  - IconContext enum
  - Public API declarations
  - `resourcesChanged()` signal

- [ ] **1.2** Create `src/core/art_provider.cpp`
  - Singleton implementation
  - Delegate to IconRegistry for icon loading
  - Delegate to ThemeManager for colors
  - Delegate to SettingsManager for persistence
  - `createAction()` - factory for self-updating QAction
  - Signal emission on any visual change

- [ ] **1.3** Extend IconSizeConfig in `icon_registry.h`
  ```cpp
  struct IconSizeConfig {
      int toolbar = 24;
      int menu = 16;
      int treeView = 16;   // NEW
      int tabBar = 16;     // NEW
      int statusBar = 16;  // NEW
      int button = 20;     // NEW
      int panel = 20;
      int dialog = 32;
      int comboBox = 16;   // NEW
  };
  ```

- [ ] **1.4** Update `icon_registry.cpp`
  - Load/save extended sizes
  - Add `getPreviewPixmap(actionId, size, devicePixelRatio)` for HiDPI preview

- [ ] **1.5** Add to `src/CMakeLists.txt`
  - Add `core/art_provider.cpp` to sources

### Phase 2: KalahariStyle Integration (~20 min)

- [ ] **2.1** Update `include/kalahari/gui/kalahari_style.h`
  - Use ArtProvider instead of IconRegistry directly

- [ ] **2.2** Update `src/gui/kalahari_style.cpp`
  - Get sizes from ArtProvider
  - Support all extended contexts

- [ ] **2.3** Apply KalahariStyle in `src/main.cpp`
  ```cpp
  QApplication app(argc, argv);
  app.setStyle(new kalahari::gui::KalahariStyle());
  ```

### Phase 3: Component Refactoring (~30 min)

- [ ] **3.1** Refactor ToolbarManager
  - Use `ArtProvider::createAction()` instead of manual icon setup
  - Remove `refreshIcons()` method entirely
  - Actions auto-update via signal

- [ ] **3.2** Refactor MenuBuilder
  - Use `ArtProvider::createAction()` instead of manual icon setup
  - Remove `refreshIcons()` method entirely
  - Actions auto-update via signal

- [ ] **3.3** Simplify MainWindow
  - Remove manual icon refresh calls from `onThemeChanged()`
  - ArtProvider handles all icon updates automatically

### Phase 4: Settings Dialog (~45 min)

- [ ] **4.1** Add Icon Style selector
  - ComboBox with TwoTone/Filled/Outlined/Rounded options
  - Store selection in `appearance.iconTheme`

- [ ] **4.2** Add Icon Preview widget
  - Show 6-8 sample icons in selected style
  - Use `ArtProvider::getPreviewPixmap()` for HiDPI
  - Update preview when style changes

- [ ] **4.3** Add extended size controls
  - Spinboxes for: Toolbar, Menu, TreeView, TabBar, Button
  - Range: 12-48px depending on context

- [ ] **4.4** Connect to ArtProvider
  - On Apply: call `ArtProvider::setIconTheme()`, `setIconSize()`
  - ArtProvider emits `resourcesChanged()` automatically

### Phase 5: Testing & Verification (~30 min)

- [ ] **5.1** Build and run application
- [ ] **5.2** Test icon theme switching (all 4 styles)
- [ ] **5.3** Test size changes (toolbar, menu)
- [ ] **5.4** Test theme switching (Light ↔ Dark)
- [ ] **5.5** Test HiDPI preview rendering
- [ ] **5.6** Verify persistence (restart app, check settings)

### Phase 6: COMMIT (~5 min)

- [ ] **6.1** `git add` all changed files
- [ ] **6.2** `git commit` with descriptive message
- [ ] **6.3** Verify commit successful

## Files Checklist

### Create
- [ ] `include/kalahari/core/art_provider.h`
- [ ] `src/core/art_provider.cpp`

### Modify
- [ ] `include/kalahari/core/icon_registry.h` - Extended IconSizeConfig
- [ ] `src/core/icon_registry.cpp` - Extended sizes, getPreviewPixmap
- [ ] `include/kalahari/gui/kalahari_style.h` - Use ArtProvider
- [ ] `src/gui/kalahari_style.cpp` - Use ArtProvider
- [ ] `include/kalahari/gui/toolbar_manager.h` - Remove refreshIcons
- [ ] `src/gui/toolbar_manager.cpp` - Use ArtProvider::createAction
- [ ] `include/kalahari/gui/menu_builder.h` - Remove refreshIcons
- [ ] `src/gui/menu_builder.cpp` - Use ArtProvider::createAction
- [ ] `include/kalahari/gui/settings_dialog.h` - Icon preview members
- [ ] `src/gui/settings_dialog.cpp` - Icon style selector, preview, sizes
- [ ] `src/gui/main_window.cpp` - Remove manual refresh
- [ ] `src/main.cpp` - Apply KalahariStyle
- [ ] `src/CMakeLists.txt` - Add art_provider.cpp

## Verification Checklist

### ArtProvider Core
- [ ] Singleton accessible via `ArtProvider::getInstance()`
- [ ] `createAction()` returns functional QAction with icon
- [ ] `resourcesChanged()` signal emitted on theme/size change
- [ ] Managed actions auto-update when signal emitted

### Icon Theme Switching
- [ ] TwoTone style works
- [ ] Filled style works
- [ ] Outlined style works
- [ ] Rounded style works
- [ ] Preview updates on selection change
- [ ] All app icons update on Apply

### Size Changes
- [ ] Toolbar size change works
- [ ] Menu size change works
- [ ] Sizes persist across restart

### Theme Switching
- [ ] Light → Dark updates all icons
- [ ] Dark → Light updates all icons
- [ ] No icon flicker during switch
- [ ] Colors correct in both themes

### HiDPI
- [ ] Preview icons are crisp (not pixelated)
- [ ] Toolbar icons are crisp
- [ ] Menu icons are crisp

## CRITICAL REMINDER

**COMMIT IMMEDIATELY after Phase 5 testing passes!**

Do NOT make additional changes before committing.
Do NOT start new features before committing.
COMMIT the working state FIRST, then continue.
