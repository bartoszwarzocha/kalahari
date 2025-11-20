# Task #00004b: Settings Dialog Structure (Qt6)

**Status:** PLANNED
**Created:** 2025-11-20
**Estimated Time:** 2-3 hours
**Atomic Task:** YES (one dialog, basic structure only)

---

## 📋 Goal

Create a Settings Dialog (QDialog) with tabbed structure for Kalahari application settings.

**Scope:**
- Dialog structure with QTabWidget
- 2 placeholder tabs (Appearance, Editor)
- OK/Cancel/Apply button box
- Integration with MainWindow (Edit menu → Settings action)
- Basic load/save integration with SettingsManager

**Out of Scope (Future Tasks):**
- Actual settings controls (Task #00005, #00006)
- Theme preview
- Font selection
- Advanced editor options

---

## 🎯 Acceptance Criteria

### 1. SettingsDialog Class
- [x] Header: `include/kalahari/gui/settings_dialog.h`
- [x] Implementation: `src/gui/settings_dialog.cpp`
- [x] Inherits from `QDialog`
- [x] Has `Q_OBJECT` macro (signals/slots support)
- [x] Constructor: `SettingsDialog(QWidget* parent = nullptr)`

### 2. Dialog Structure
- [x] QTabWidget with 2 tabs:
  - Tab 0: "Appearance" (placeholder QLabel)
  - Tab 1: "Editor" (placeholder QLabel)
- [x] QDialogButtonBox with:
  - OK button (accepts and saves)
  - Cancel button (rejects changes)
  - Apply button (saves without closing)
- [x] Modal dialog (blocks main window when open)

### 3. MainWindow Integration
- [x] New action: `m_settingsAction` in MainWindow
- [x] Edit menu: Add "Settings..." after separator (Ctrl+,)
- [x] Slot: `onSettings()` - opens SettingsDialog
- [x] Dialog opens centered on MainWindow

### 4. SettingsManager Integration
- [x] Dialog loads current settings on open
- [x] OK button: Saves and closes
- [x] Apply button: Saves without closing
- [x] Cancel button: Discards changes and closes

### 5. Build & Tests
- [x] Builds successfully (Windows native)
- [x] No new warnings
- [x] Dialog opens when "Settings..." clicked
- [x] Tabs switchable
- [x] Buttons respond correctly

---

## 📐 Implementation Plan

### Step 1: Create SettingsDialog Header (15 min)

**File:** `include/kalahari/gui/settings_dialog.h`

```cpp
/// @file settings_dialog.h
/// @brief Settings dialog for Kalahari application
///
/// Provides a tabbed dialog for configuring application settings.
/// Structure:
/// - Appearance tab: Theme, font, DPI settings
/// - Editor tab: Font, line numbers, word wrap, etc.

#pragma once

#include <QDialog>

class QTabWidget;
class QDialogButtonBox;

namespace kalahari {
namespace gui {

/// @brief Settings dialog with tabbed panels
///
/// Modal dialog for configuring Kalahari application settings.
/// Settings are loaded from SettingsManager on open and saved
/// when OK/Apply is clicked.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget (usually MainWindow)
    explicit SettingsDialog(QWidget* parent = nullptr);

    /// @brief Destructor
    ~SettingsDialog() override = default;

private slots:
    /// @brief Apply button clicked - save settings without closing
    void onApply();

    /// @brief OK button clicked - save settings and close
    void onAccept();

    /// @brief Cancel button clicked - discard changes and close
    void onReject();

private:
    /// @brief Create dialog UI
    void createUI();

    /// @brief Load settings from SettingsManager
    void loadSettings();

    /// @brief Save settings to SettingsManager
    void saveSettings();

    // Widgets
    QTabWidget* m_tabWidget;
    QDialogButtonBox* m_buttonBox;

    // Placeholder tabs (will be replaced with actual panels in Tasks #00005, #00006)
    QWidget* m_appearanceTab;
    QWidget* m_editorTab;
};

} // namespace gui
} // namespace kalahari
```

### Step 2: Implement SettingsDialog (45 min)

**File:** `src/gui/settings_dialog.cpp`

**Key Methods:**
- `createUI()`: Build QTabWidget + QDialogButtonBox
- `loadSettings()`: Read from SettingsManager (placeholder - no actual controls yet)
- `saveSettings()`: Write to SettingsManager (placeholder)
- Signal/slot connections: OK→accept(), Cancel→reject(), Apply→onApply()

### Step 3: Update MainWindow (30 min)

**Files:** `include/kalahari/gui/main_window.h`, `src/gui/main_window.cpp`

**Changes:**
1. Add member: `QAction* m_settingsAction;`
2. `createActions()`: Initialize Settings action (Ctrl+,)
3. `createMenus()`: Add to Edit menu after separator
4. Add slot: `void onSettings();`
5. Implement `onSettings()`: Create and exec() dialog

### Step 4: Update CMakeLists (5 min)

**File:** `src/CMakeLists.txt`

Add to `KALAHARI_SOURCES`:
```cmake
gui/settings_dialog.cpp
${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings_dialog.h
```

### Step 5: Build & Test (30 min)

1. Clean build
2. Run application
3. Click Edit → Settings...
4. Test tab switching
5. Test OK/Cancel/Apply buttons
6. Verify no crashes

---

## 📊 File Changes

### New Files (2)
- `include/kalahari/gui/settings_dialog.h` (~120 lines)
- `src/gui/settings_dialog.cpp` (~150 lines)

### Modified Files (3)
- `include/kalahari/gui/main_window.h` (+2 lines: action, slot)
- `src/gui/main_window.cpp` (+30 lines: action setup, slot impl)
- `src/CMakeLists.txt` (+2 lines: add new files)
- `CHANGELOG.md` (1 entry in [Unreleased])

---

## 🔗 Related Tasks

- **Depends on:** Task #00004a (SettingsManager Qt migration) ✅ DONE
- **Followed by:** Task #00005 (Appearance Settings Panel)
- **Followed by:** Task #00006 (Editor Settings Panel)

---

## ⚠️ Risks & Mitigation

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| AUTOMOC not finding new header | Low | Medium | Add header to KALAHARI_SOURCES |
| Dialog not modal | Low | Low | Use `exec()` instead of `show()` |
| Buttons not responding | Low | Medium | Connect signals properly (accept/reject) |

---

## 📝 Notes

- This is a **STRUCTURE ONLY** task - actual settings controls come in Tasks #00005-00006
- Focus: Dialog shell + button logic + MainWindow integration
- Placeholders: QLabel widgets in tabs (temporary)
- SettingsManager integration: Placeholder calls (no actual settings to save yet)

---

## ✅ Definition of Done

- [ ] SettingsDialog header created with Q_OBJECT
- [ ] SettingsDialog implemented with QTabWidget + buttons
- [ ] MainWindow has Settings action in Edit menu (Ctrl+,)
- [ ] Dialog opens modally when action triggered
- [ ] Tabs switchable
- [ ] OK saves and closes, Cancel discards, Apply saves
- [ ] Builds successfully (Windows native)
- [ ] CHANGELOG.md updated
- [ ] Git commit created
