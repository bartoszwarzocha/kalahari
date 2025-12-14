# Change: Centralized Icon Management System (ArtProvider)

## Why

### Problem 1: Distributed Icon Refresh Architecture

**Current State (OpenSpec #00025):**
```
ThemeManager ──themeChanged()──► MainWindow::onThemeChanged()
                                       │
                                       ├──► IconRegistry.setThemeColors
                                       ├──► m_toolbarManager->refreshIcons()
                                       └──► m_menuBuilder->refreshIcons()
```

**Problems:**
- **Open/Closed Violation:** Adding new icon-using component requires MainWindow modification
- **DRY Violation:** Identical refresh logic duplicated in ToolbarManager and MenuBuilder
- **Tight Coupling:** MainWindow orchestrates ALL icon refreshes
- **No Single Source of Truth:** Components manage their own icon state

### Problem 2: No Central Visual Resource Manager

Applications like wxWidgets have `wxArtProvider` - a central point for all visual resources.
Kalahari lacks this, leading to:
- Components directly accessing IconRegistry
- No unified API for icons, colors, styles
- Difficult to extend with new visual resources

### Problem 3: Limited Icon Configuration

**Current:** Only 4 icon size contexts (toolbar, menu, panel, dialog)
**Missing:** TreeView, TabBar, StatusBar, Button, ComboBox

### Problem 4: No Icon Theme Switching UI

Icon theme (twotone/filled/outlined/rounded) stored in settings but:
- No UI to change it
- No preview of different styles
- No immediate refresh when changed

## What Changes

### NEW ARCHITECTURE: ArtProvider (Central Visual Resource Manager)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         ArtProvider (Singleton)                              │
│                    Central Point for ALL Visual Resources                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ PUBLIC API (used by all components)                                  │    │
│  ├─────────────────────────────────────────────────────────────────────┤    │
│  │                                                                      │    │
│  │  // Factory - creates self-updating QAction                         │    │
│  │  QAction* createAction(cmdId, text, parent)                         │    │
│  │                                                                      │    │
│  │  // Direct icon access (for special cases)                          │    │
│  │  QIcon getIcon(cmdId, context)                                      │    │
│  │  QPixmap getPixmap(cmdId, size)                                     │    │
│  │  QPixmap getPreviewPixmap(cmdId, size, dpr)  // HiDPI preview       │    │
│  │                                                                      │    │
│  │  // Theme info                                                       │    │
│  │  QString getIconTheme()           // twotone/filled/outlined/rounded│    │
│  │  QColor getPrimaryColor()                                           │    │
│  │  QColor getSecondaryColor()                                         │    │
│  │  QString getThemeName()           // Light/Dark/Custom              │    │
│  │                                                                      │    │
│  │  // Size configuration                                               │    │
│  │  int getIconSize(context)                                           │    │
│  │  void setIconSize(context, size)                                    │    │
│  │                                                                      │    │
│  │  // Theme configuration                                              │    │
│  │  void setIconTheme(theme)         // triggers resourcesChanged      │    │
│  │  void setPrimaryColor(color)      // triggers resourcesChanged      │    │
│  │  void setSecondaryColor(color)    // triggers resourcesChanged      │    │
│  │                                                                      │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
│  signals:                                                                    │
│    resourcesChanged()  ← Emitted on ANY visual change                       │
│                          All managed QActions auto-update                   │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ INTERNAL (delegates to existing systems)                             │    │
│  ├─────────────────────────────────────────────────────────────────────┤    │
│  │  IconRegistry*   m_iconRegistry   // SVG loading, caching, rendering│    │
│  │  ThemeManager*   m_themeManager   // Theme colors, palette          │    │
│  │  SettingsManager* (static)        // Persistence                    │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                    resourcesChanged() signal
                                    │
         ┌──────────────────────────┼──────────────────────────┐
         ▼                          ▼                          ▼
   ┌───────────┐            ┌───────────┐              ┌───────────┐
   │ QAction 1 │            │ QAction 2 │              │ QAction N │
   │ (managed) │            │ (managed) │              │ (managed) │
   │ auto-     │            │ auto-     │              │ auto-     │
   │ updates   │            │ updates   │              │ updates   │
   └───────────┘            └───────────┘              └───────────┘
         ▲                          ▲                          ▲
         │                          │                          │
   ToolbarManager             MenuBuilder               Future Components
   (just creates              (just creates             (just create
    actions, ZERO              actions, ZERO             actions, ZERO
    refresh logic)             refresh logic)            refresh logic)
```

### IconContext Enum

```cpp
enum class IconContext {
    Toolbar,    // 24px default - toolbar buttons
    Menu,       // 16px default - menu items
    TreeView,   // 16px default - Navigator, file trees, outlines
    TabBar,     // 16px default - notebook tabs, central tab bar
    StatusBar,  // 16px default - status bar indicators
    Button,     // 20px default - QPushButton with icon
    Panel,      // 20px default - panel captions (Log, Search, etc.)
    Dialog,     // 32px default - dialog icons (About, Error, etc.)
    ComboBox    // 16px default - ComboBox item icons
};
```

### KalahariStyle (QProxyStyle)

Custom style that reads icon sizes from ArtProvider for Qt's pixel metrics:
- `PM_SmallIconSize` → Menu size
- `PM_ToolBarIconSize` → Toolbar size
- `PM_ListViewIconSize` → TreeView size
- `PM_TabBarIconSize` → TabBar size
- `PM_ButtonIconSize` → Button size

### Settings Dialog Updates

**Appearance/Icons Page:**
```
┌─────────────────────────────────────────────────────────────┐
│ Icon Style                                                   │
├─────────────────────────────────────────────────────────────┤
│ Style: [▼ TwoTone (filled accent)    ]                      │
│                                                              │
│ Preview:                                                     │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ [save] [open] [undo] [redo] [cut] [copy] [paste] [find] │ │
│ └─────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│ Icon Sizes                                                   │
├─────────────────────────────────────────────────────────────┤
│ Toolbar Icons:    [▼ 24 ] px                                │
│ Menu Icons:       [▼ 16 ] px                                │
│ Tree View Icons:  [▼ 16 ] px                                │
│ Tab Bar Icons:    [▼ 16 ] px                                │
│ Button Icons:     [▼ 20 ] px                                │
└─────────────────────────────────────────────────────────────┘
```

## Usage Examples

### ToolbarManager (ZERO refresh logic)

```cpp
void ToolbarManager::buildToolbar() {
    auto& art = ArtProvider::getInstance();

    // Create self-updating actions - they auto-refresh on theme/size change
    m_toolbar->addAction(art.createAction("file.new", tr("New"), this));
    m_toolbar->addAction(art.createAction("file.open", tr("Open"), this));
    m_toolbar->addAction(art.createAction("file.save", tr("Save"), this));
    // DONE - no refresh method needed!
}
```

### MenuBuilder (ZERO refresh logic)

```cpp
void MenuBuilder::buildFileMenu(QMenu* menu) {
    auto& art = ArtProvider::getInstance();

    menu->addAction(art.createAction("file.new", tr("&New"), menu));
    menu->addAction(art.createAction("file.open", tr("&Open"), menu));
    // DONE - no refresh method needed!
}
```

### Settings Dialog (icon preview)

```cpp
void SettingsDialog::updateIconPreview() {
    auto& art = ArtProvider::getInstance();
    QString theme = m_iconStyleComboBox->currentData().toString();

    // Get HiDPI-aware preview pixmaps
    qreal dpr = devicePixelRatioF();
    m_previewLabel1->setPixmap(art.getPreviewPixmap("file.save", 32, dpr));
    m_previewLabel2->setPixmap(art.getPreviewPixmap("file.open", 32, dpr));
    // ...
}
```

## Impact

### Files to Create

| File | Purpose |
|------|---------|
| `include/kalahari/core/art_provider.h` | ArtProvider class declaration |
| `src/core/art_provider.cpp` | ArtProvider implementation |

### Files to Modify

| File | Changes |
|------|---------|
| `include/kalahari/core/icon_registry.h` | Extended IconSizeConfig, add treeView/tabBar/statusBar/button/comboBox |
| `src/core/icon_registry.cpp` | Load/save extended sizes, getPreviewPixmap for HiDPI |
| `include/kalahari/gui/toolbar_manager.h` | Remove refreshIcons(), simplify to use ArtProvider |
| `src/gui/toolbar_manager.cpp` | Use ArtProvider::createAction() |
| `include/kalahari/gui/menu_builder.h` | Remove refreshIcons(), simplify to use ArtProvider |
| `src/gui/menu_builder.cpp` | Use ArtProvider::createAction() |
| `src/gui/settings_dialog.cpp` | Icon theme selector, preview, extended size controls |
| `src/gui/main_window.cpp` | Apply KalahariStyle, remove manual icon refresh |
| `src/main.cpp` | Apply KalahariStyle to QApplication |
| `src/CMakeLists.txt` | Add new source files |

### Existing Files (already present, need integration)

| File | Status |
|------|--------|
| `include/kalahari/gui/kalahari_style.h` | EXISTS (untracked) - integrate with ArtProvider |
| `src/gui/kalahari_style.cpp` | EXISTS (untracked) - integrate with ArtProvider |

## Acceptance Criteria

### ArtProvider Core
- [ ] ArtProvider singleton created and initialized
- [ ] `createAction()` returns self-updating QAction
- [ ] `resourcesChanged()` signal emitted on theme/size/style change
- [ ] All managed actions auto-update when signal emitted

### Icon Theme Switching
- [ ] Settings Dialog has icon style dropdown (TwoTone/Filled/Outlined/Rounded)
- [ ] Preview shows sample icons in selected style
- [ ] Changing style immediately updates all icons in application
- [ ] Selection persists across sessions

### Extended Icon Sizes
- [ ] IconSizeConfig has all 9 contexts (toolbar, menu, treeView, tabBar, statusBar, button, panel, dialog, comboBox)
- [ ] Settings Dialog has controls for configurable sizes
- [ ] Sizes persist and restore correctly
- [ ] KalahariStyle returns correct sizes for Qt pixel metrics

### Component Integration
- [ ] ToolbarManager uses ArtProvider::createAction()
- [ ] MenuBuilder uses ArtProvider::createAction()
- [ ] Both have ZERO refresh logic
- [ ] Theme switch updates all icons automatically

### HiDPI Support
- [ ] Preview icons render crisp at any DPI
- [ ] `getPreviewPixmap()` respects devicePixelRatio
- [ ] Normal icons render correctly (no DPI scaling in getIcon)

## Estimated Effort

- ArtProvider core: ~60 min
- KalahariStyle integration: ~20 min
- ToolbarManager/MenuBuilder refactor: ~30 min
- Settings Dialog (preview + sizes): ~45 min
- Testing: ~30 min

**Total: ~3 hours**

## Current Status

**Status:** IN PROGRESS (Architecture redesign approved)

**Builds on:**
- OpenSpec #00021 (IconRegistry Runtime System) - COMPLETE
- OpenSpec #00025 (Theme-Icon Integration) - COMPLETE

**Key Decision:** User requested central ArtProvider architecture instead of distributed refresh approach. This provides:
- Single source of truth for all visual resources
- Self-updating actions (ZERO refresh logic in components)
- Clean separation of concerns
- Easy extensibility for future visual resources
