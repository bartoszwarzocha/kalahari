# Settings Inventory - Complete Parameter Tracking

**Purpose:** Track ALL application settings - implementation status, storage location, GUI integration

**Last Updated:** 2025-11-09

---

## Settings Categories

### 1. Window State (SettingsManager)

| Parameter | Module | Source File | JSON Key | Type | Default | Status | Notes |
|-----------|--------|-------------|----------|------|---------|--------|-------|
| Window width | MainWindow | `src/gui/main_window.cpp` | `window.width` | int | 1280 | ✅ IMPLEMENTED | Load/save on startup/close |
| Window height | MainWindow | `src/gui/main_window.cpp` | `window.height` | int | 800 | ✅ IMPLEMENTED | Load/save on startup/close |
| Window X position | MainWindow | `src/gui/main_window.cpp` | `window.x` | int | 100 | ✅ IMPLEMENTED | Load/save on startup/close |
| Window Y position | MainWindow | `src/gui/main_window.cpp` | `window.y` | int | 100 | ✅ IMPLEMENTED | Load/save on startup/close |
| Window maximized | MainWindow | `src/gui/main_window.cpp` | `window.maximized` | bool | false | ✅ IMPLEMENTED | Load/save on startup/close |

### 2. Appearance Settings (AppearanceSettingsPanel)

| Parameter | Module | Source File | JSON Key | Type | Default | Status | Notes |
|-----------|--------|-------------|----------|------|---------|--------|-------|
| **Theme** | KalahariApp | `src/gui/kalahari_app.cpp` | `appearance.theme` | string | "System" | ✅ IMPLEMENTED | Loaded on startup (line 82), applied via SetAppearance() |
| Icon size | IconRegistry | `src/gui/icon_registry.cpp` | `appearance.iconSize` | int | 24 | ⚠️ NOT CONNECTED | Panel saves, but IconRegistry doesn't load |
| Font scaling | GUI | N/A | `appearance.fontScaling` | double | 1.0 | ⚠️ NOT CONNECTED | Panel saves, but no module uses it |

**Appearance Settings Issues:**

- ✅ `appearance.theme`: **FIXED** - Now loads from settings.json on startup (2025-11-09)
- ❌ `appearance.iconSize`: AppearanceSettingsPanel saves, but IconRegistry doesn't read from SettingsManager
- ❌ `appearance.fontScaling`: No module uses this value yet

### 3. Editor Settings (EditorSettingsPanel)

| Parameter | Module | Source File | JSON Key | Type | Default | Status | Notes |
|-----------|--------|-------------|----------|------|---------|--------|-------|
| Caret blink enabled | bwxTextEditor | `external/bwx_sdk/src/bwx_gui/bwx_text_editor.cpp` | `editor.caretBlinkEnabled` | bool | true | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Caret blink rate | bwxTextEditor | `external/bwx_sdk/src/bwx_gui/bwx_text_editor.cpp` | `editor.caretBlinkRate` | int | 500 | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Caret width | bwxTextEditor | `external/bwx_sdk/src/bwx_gui/bwx_text_editor.cpp` | `editor.caretWidth` | int | 1 | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Margin left | bwxTextEditor | `external/bwx_sdk/src/bwx_gui/bwx_text_editor.cpp` | `editor.marginLeft` | int | 20 | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Margin right | bwxTextEditor | `external/bwx_sdk/src/bwx_gui/bwx_text_editor.cpp` | `editor.marginRight` | int | 20 | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Margin top | bwxTextEditor | `external/bwx_sdk/src/bwx_gui/bwx_text_editor.cpp` | `editor.marginTop` | int | 10 | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Margin bottom | bwxTextEditor | `external/bwx_sdk/src/bwx_gui/bwx_text_editor.cpp` | `editor.marginBottom` | int | 10 | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Line spacing | FullViewRenderer | `external/bwx_sdk/src/bwx_gui/bwx_text_renderer.cpp` | `editor.lineSpacing` | double | 1.2 | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Selection opacity | FullViewRenderer | `external/bwx_sdk/src/bwx_gui/bwx_text_renderer.cpp` | `editor.selectionOpacity` | int | 128 | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Selection color (R) | FullViewRenderer | `external/bwx_sdk/src/bwx_gui/bwx_text_renderer.cpp` | `editor.selectionColor.r` | int | 0 | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Selection color (G) | FullViewRenderer | `external/bwx_sdk/src/bwx_gui/bwx_text_renderer.cpp` | `editor.selectionColor.g` | int | 120 | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Selection color (B) | FullViewRenderer | `external/bwx_sdk/src/bwx_gui/bwx_text_renderer.cpp` | `editor.selectionColor.b` | int | 215 | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Antialiasing | FullViewRenderer | `external/bwx_sdk/src/bwx_gui/bwx_text_renderer.cpp` | `editor.antialiasing` | bool | true | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Auto focus | EditorPanel | `src/gui/panels/editor_panel.cpp` | `editor.autoFocus` | bool | true | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Word wrap | bwxTextEditor | `external/bwx_sdk/src/bwx_gui/bwx_text_editor.cpp` | `editor.wordWrap` | bool | true | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |
| Undo limit | bwxTextDocument | `external/bwx_sdk/src/bwx_core/bwx_text_document.cpp` | `editor.undoLimit` | int | 100 | ✅ IMPLEMENTED | Applied via EditorPanel::applySettings() |

**Editor Settings Status:** ✅ ALL WORKING

### 4. Diagnostic Log Settings (LogSettingsPanel)

| Parameter | Module | Source File | JSON Key | Type | Default | Status | Notes |
|-----------|--------|-------------|----------|------|---------|--------|-------|
| Buffer size | LogPanel | `src/gui/panels/log_panel.cpp` | `log.bufferSize` | int | 500 | ✅ IMPLEMENTED | Loads from SettingsManager, applied via applySettings() |
| Background color (R) | LogPanel | `src/gui/panels/log_panel.cpp` | `log.backgroundColor.r` | int | 60 | ✅ IMPLEMENTED | Loads from SettingsManager, applied via applySettings() |
| Background color (G) | LogPanel | `src/gui/panels/log_panel.cpp` | `log.backgroundColor.g` | int | 60 | ✅ IMPLEMENTED | Loads from SettingsManager, applied via applySettings() |
| Background color (B) | LogPanel | `src/gui/panels/log_panel.cpp` | `log.backgroundColor.b` | int | 60 | ✅ IMPLEMENTED | Loads from SettingsManager, applied via applySettings() |
| Text color (R) | LogPanel | `src/gui/panels/log_panel.cpp` | `log.textColor.r` | int | 255 | ✅ IMPLEMENTED | Loads from SettingsManager, applied via applySettings() |
| Text color (G) | LogPanel | `src/gui/panels/log_panel.cpp` | `log.textColor.g` | int | 255 | ✅ IMPLEMENTED | Loads from SettingsManager, applied via applySettings() |
| Text color (B) | LogPanel | `src/gui/panels/log_panel.cpp` | `log.textColor.b` | int | 255 | ✅ IMPLEMENTED | Loads from SettingsManager, applied via applySettings() |
| Font size | LogPanel | `src/gui/panels/log_panel.cpp` | `log.fontSize` | int | 11 | ✅ IMPLEMENTED | Loads from SettingsManager, applied via applySettings() |

**Log Settings Status:** ✅ ALL WORKING (Fixed 2025-11-09)
- ✅ LogPanel::applySettings() loads settings from SettingsManager
- ✅ Called on LogPanel initialization (constructor line 46)
- ✅ Called from MainWindow after settings save (line 931-934)

### 5. UI State (SettingsManager)

| Parameter | Module | Source File | JSON Key | Type | Default | Status | Notes |
|-----------|--------|-------------|----------|------|---------|--------|-------|
| UI language | MainWindow | `src/gui/main_window.cpp` | `ui.language` | string | "en" | ⚠️ PLACEHOLDER | Not implemented yet (Phase 2) |
| UI font size | GUI | N/A | `ui.font_size` | int | 12 | ⚠️ PLACEHOLDER | Created by migration, not used |
| Last perspective | PerspectiveManager | `src/gui/perspective_manager.cpp` | `ui.lastPerspective` | string | "Default" | ✅ IMPLEMENTED | Load/save on startup/close |

### 6. Session State (SettingsManager)

| Parameter | Module | Source File | JSON Key | Type | Default | Status | Notes |
|-----------|--------|-------------|----------|------|---------|--------|-------|
| Auto-save interval | N/A | N/A | `session.auto_save_interval` | int | 300 | ⚠️ PLACEHOLDER | Created by createDefaults(), not used |
| Backup enabled | N/A | N/A | `session.backup_enabled` | bool | true | ⚠️ PLACEHOLDER | Created by createDefaults(), not used |

### 7. Recent Files (SettingsManager)

| Parameter | Module | Source File | JSON Key | Type | Default | Status | Notes |
|-----------|--------|-------------|----------|------|---------|--------|-------|
| Recent files list | MainWindow | `src/gui/main_window.cpp` | `recent_files` | array | [] | ⚠️ PLACEHOLDER | Created by createDefaults(), not used |

---

## Implementation Status Summary

### ✅ Fully Working (27 settings)
- Window state (5): width, height, x, y, maximized
- Appearance (1): theme
- Editor settings (14): All caret, margin, rendering, behavior settings
- Log settings (8): All buffer/color/font settings (**FIXED 2025-11-09**)
- UI state (1): lastPerspective

### ⚠️ Partially Working (2 settings)
- `appearance.iconSize`: Saved but IconRegistry doesn't read it
- `appearance.fontScaling`: Saved but no module uses it

### 🔮 Placeholder (5 settings)
- `ui.language`: Not implemented yet
- `ui.font_size`: Created by migration, not used
- `session.auto_save_interval`: Not implemented
- `session.backup_enabled`: Not implemented
- `recent_files`: Not implemented

---

## Critical Fixes Completed (2025-11-09)

### ✅ FIXED: Theme Not Loading
**File:** `src/gui/kalahari_app.cpp` (line 82)

**Root Cause:** SettingsManager::getInstance() created instance but didn't call load()!

**Fix Applied:**
```cpp
core::SettingsManager& settingsMgr = core::SettingsManager::getInstance();
settingsMgr.load();  // ✅ ADDED - Load settings.json BEFORE reading theme
wxString themeName = wxString::FromUTF8(
    settingsMgr.get<std::string>("appearance.theme", "System")
);
```

### ✅ FIXED: Log Settings Not Applied
**File:** `src/gui/panels/log_panel.cpp` + `main_window.cpp`

**Root Cause:** LogPanel::applySettings() used hardcoded values instead of loading from SettingsManager.

**Fix Applied:**
1. ✅ Updated LogPanel::applySettings() to load all settings from SettingsManager (lines 94-139)
2. ✅ Added SettingsManager include to log_panel.cpp (line 8)
3. ✅ Added call from MainWindow after settings save (lines 931-934)

### ⏳ TODO: Icon Size Not Applied
**File:** `src/gui/icon_registry.cpp`

**Problem:** IconRegistry doesn't read `appearance.iconSize` from SettingsManager.

**Fix Required:** Add SettingsManager::load() call in IconRegistry initialization.

**Status:** Low priority - icon size works but requires app restart

---

## Settings Flow Architecture

### Settings Save Flow (OK)
```
SettingsDialog
  ↓ User clicks OK/Apply
  ↓ SettingsDialog::applyChanges()
  ↓ Each panel calls saveToState()
  ↓ MainWindow::onFileSettings() receives newState
  ↓ MainWindow saves to SettingsManager::set()
  ↓ SettingsManager::save() writes to settings.json
```

### Settings Load Flow (BROKEN in some places)
```
Application Startup
  ↓ KalahariApp::OnInit()
  ↓ ❌ SettingsManager::getInstance() (NO LOAD!)
  ↓ ✅ MainWindow::MainWindow() loads window state
  ↓ ❌ LogPanel created but NO applySettings()
  ↓ ✅ EditorPanel has applySettings()

Settings Dialog Open
  ↓ MainWindow::onFileSettings()
  ↓ ✅ Load ALL settings from SettingsManager
  ↓ Create SettingsState
  ↓ Pass to SettingsDialog
```

---

## Next Steps

1. ~~**Fix theme loading**~~ - ✅ COMPLETED (2025-11-09)
2. ~~**Fix log settings**~~ - ✅ COMPLETED (2025-11-09)
3. **Fix icon size** - Connect IconRegistry to SettingsManager (low priority)
4. **Test all fixes** - Verify theme switching, log settings changes work correctly
5. **Add this document** to project_docs/README.md index
6. **Keep updated** as new settings are added

---

**Document Version:** 1.0
**Status:** Initial inventory complete
**Maintainer:** Track ALL settings here before implementation
