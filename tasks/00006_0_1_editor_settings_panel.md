# Task #00006: Editor Settings Panel

**Status:** ✅ COMPLETE
**Phase:** Phase 0 (Foundation)
**Zagadnienie:** 0.1 (Qt6 Migration)
**Estimated Time:** 60-90 minutes
**Started:** 2025-11-20
**Completed:** 2025-11-20
**Actual Time:** ~60 minutes

---

## 🎯 Goal

Replace the placeholder Editor tab in Settings Dialog with functional controls for:
- Font family selection (QFontComboBox)
- Font size configuration (QSpinBox)
- Tab size configuration (QSpinBox)
- Line numbers toggle (QCheckBox)
- Word wrap toggle (QCheckBox)

Controls must load current values from `SettingsManager` and save changes back on Apply/OK.

---

## ✅ Acceptance Criteria

1. ✅ Editor tab contains 5 properly labeled controls
2. ✅ Font Family combo shows monospace fonts (Consolas, Courier New, Monaco, etc.)
3. ✅ Font Size spinner allows 8-32 range with step 1
4. ✅ Tab Size spinner allows 2-8 range with step 1
5. ✅ Line Numbers checkbox (default: enabled)
6. ✅ Word Wrap checkbox (default: disabled)
7. ✅ Controls load current values from SettingsManager on dialog open
8. ✅ Apply button saves values to SettingsManager (dialog stays open)
9. ✅ OK button saves values and closes dialog
10. ✅ Cancel button discards changes
11. ✅ Build succeeds (scripts\build_windows.bat)
12. ✅ All tests pass (no new tests required - existing 68 tests)

---

## 📝 Implementation Plan

### Step 1: Update Header (settings_dialog.h)

Add private member variables for controls:

```cpp
private:
    // ... existing members ...

    // Editor tab controls
    QFontComboBox* m_fontFamilyComboBox;
    QSpinBox* m_editorFontSizeSpinBox;
    QSpinBox* m_tabSizeSpinBox;
    QCheckBox* m_lineNumbersCheckBox;
    QCheckBox* m_wordWrapCheckBox;
```

### Step 2: Implement Editor Tab UI (settings_dialog.cpp)

Replace placeholder in `createUI()` method (lines 98-105):

```cpp
// Editor tab
m_editorTab = new QWidget();
QVBoxLayout* editorLayout = new QVBoxLayout(m_editorTab);

// Create group box for visual grouping
QGroupBox* editorGroup = new QGroupBox(tr("Editor Settings"));
QGridLayout* gridLayout = new QGridLayout();

// Font family (row 0)
QLabel* fontFamilyLabel = new QLabel(tr("Font Family:"));
m_fontFamilyComboBox = new QFontComboBox();
m_fontFamilyComboBox->setFontFilters(QFontComboBox::MonospacedFonts);  // Only monospace
m_fontFamilyComboBox->setCurrentFont(QFont("Consolas"));  // Default
gridLayout->addWidget(fontFamilyLabel, 0, 0);
gridLayout->addWidget(m_fontFamilyComboBox, 0, 1);

// Font size (row 1)
QLabel* fontSizeLabel = new QLabel(tr("Font Size:"));
m_editorFontSizeSpinBox = new QSpinBox();
m_editorFontSizeSpinBox->setMinimum(8);
m_editorFontSizeSpinBox->setMaximum(32);
m_editorFontSizeSpinBox->setSingleStep(1);
m_editorFontSizeSpinBox->setSuffix(" pt");
gridLayout->addWidget(fontSizeLabel, 1, 0);
gridLayout->addWidget(m_editorFontSizeSpinBox, 1, 1);

// Tab size (row 2)
QLabel* tabSizeLabel = new QLabel(tr("Tab Size:"));
m_tabSizeSpinBox = new QSpinBox();
m_tabSizeSpinBox->setMinimum(2);
m_tabSizeSpinBox->setMaximum(8);
m_tabSizeSpinBox->setSingleStep(1);
m_tabSizeSpinBox->setSuffix(" spaces");
gridLayout->addWidget(tabSizeLabel, 2, 0);
gridLayout->addWidget(m_tabSizeSpinBox, 2, 1);

// Line numbers (row 3)
m_lineNumbersCheckBox = new QCheckBox(tr("Show Line Numbers"));
gridLayout->addWidget(m_lineNumbersCheckBox, 3, 0, 1, 2);  // Span 2 columns

// Word wrap (row 4)
m_wordWrapCheckBox = new QCheckBox(tr("Enable Word Wrap"));
gridLayout->addWidget(m_wordWrapCheckBox, 4, 0, 1, 2);  // Span 2 columns

// Make controls stretch horizontally
gridLayout->setColumnStretch(1, 1);

editorGroup->setLayout(gridLayout);
editorLayout->addWidget(editorGroup);
editorLayout->addStretch();  // Push group to top

m_tabWidget->addTab(m_editorTab, tr("Editor"));
```

**Required headers:**
```cpp
#include <QFontComboBox>
// QSpinBox, QCheckBox already included from Task #00005
```

### Step 3: Update Constructor Initialization

Update constructor member initialization list (after line 28):

```cpp
SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    // ... existing members ...
    , m_fontSizeSpinBox(nullptr)
    , m_fontFamilyComboBox(nullptr)        // NEW
    , m_editorFontSizeSpinBox(nullptr)     // NEW
    , m_tabSizeSpinBox(nullptr)            // NEW
    , m_lineNumbersCheckBox(nullptr)       // NEW
    , m_wordWrapCheckBox(nullptr)          // NEW
{
    // ...
}
```

### Step 4: Implement loadSettings() Method

Update `loadSettings()` (add after Appearance loading):

```cpp
void SettingsDialog::loadSettings() {
    // ... existing Appearance loading ...

    // Load editor settings
    std::string fontFamily = settings.get<std::string>("editor.fontFamily", "Consolas");
    QFont font(QString::fromStdString(fontFamily));
    m_fontFamilyComboBox->setCurrentFont(font);
    logger.debug("Loaded editor font family: {}", fontFamily);

    int editorFontSize = settings.get<int>("editor.fontSize", 12);
    m_editorFontSizeSpinBox->setValue(editorFontSize);
    logger.debug("Loaded editor font size: {}", editorFontSize);

    int tabSize = settings.get<int>("editor.tabSize", 4);
    m_tabSizeSpinBox->setValue(tabSize);
    logger.debug("Loaded tab size: {}", tabSize);

    bool lineNumbers = settings.get<bool>("editor.lineNumbers", true);
    m_lineNumbersCheckBox->setChecked(lineNumbers);
    logger.debug("Loaded line numbers: {}", lineNumbers);

    bool wordWrap = settings.get<bool>("editor.wordWrap", false);
    m_wordWrapCheckBox->setChecked(wordWrap);
    logger.debug("Loaded word wrap: {}", wordWrap);

    logger.debug("SettingsDialog: Settings loaded successfully");
}
```

### Step 5: Implement saveSettings() Method

Update `saveSettings()` (add after Appearance saving):

```cpp
void SettingsDialog::saveSettings() {
    // ... existing Appearance saving ...

    // Save editor settings
    QString fontFamilyData = m_fontFamilyComboBox->currentFont().family();
    settings.set("editor.fontFamily", fontFamilyData.toStdString());
    logger.debug("Saved editor font family: {}", fontFamilyData.toStdString());

    int editorFontSize = m_editorFontSizeSpinBox->value();
    settings.set("editor.fontSize", editorFontSize);
    logger.debug("Saved editor font size: {}", editorFontSize);

    int tabSize = m_tabSizeSpinBox->value();
    settings.set("editor.tabSize", tabSize);
    logger.debug("Saved tab size: {}", tabSize);

    bool lineNumbers = m_lineNumbersCheckBox->isChecked();
    settings.set("editor.lineNumbers", lineNumbers);
    logger.debug("Saved line numbers: {}", lineNumbers);

    bool wordWrap = m_wordWrapCheckBox->isChecked();
    settings.set("editor.wordWrap", wordWrap);
    logger.debug("Saved word wrap: {}", wordWrap);

    // Save to disk
    if (settings.save()) {
        logger.info("SettingsDialog: Settings saved successfully");
    } else {
        logger.error("SettingsDialog: Failed to save settings");
    }
}
```

---

## 📂 Files to Modify

1. **include/kalahari/gui/settings_dialog.h**
   - Add 5 control member variables
   - Add QFontComboBox forward declaration

2. **src/gui/settings_dialog.cpp**
   - Add header: QFontComboBox
   - Replace Editor tab placeholder (lines 98-105)
   - Update constructor initialization
   - Implement loadSettings() logic
   - Implement saveSettings() logic

---

## 🔨 Build & Test Plan

### Build:
```cmd
scripts\build_windows.bat
```

### Manual Test:
1. Run `build-windows\bin\kalahari.exe`
2. Open Edit → Settings (Ctrl+,)
3. Click Editor tab
4. Verify 5 controls present:
   - Font Family (combo with monospace fonts)
   - Font Size (8-32)
   - Tab Size (2-8)
   - Line Numbers (checkbox)
   - Word Wrap (checkbox)
5. Change all values
6. Click Apply → check logs
7. Close dialog, reopen → verify values persisted
8. Change values, click Cancel → verify changes discarded

---

## 📊 Estimated Timeline

- Step 1 (Header update): 5 minutes
- Step 2 (UI implementation): 20 minutes
- Step 3 (Constructor): 5 minutes
- Step 4 (loadSettings): 10 minutes
- Step 5 (saveSettings): 10 minutes
- Build & test: 15 minutes
- CHANGELOG & commit: 5 minutes

**Total:** 70 minutes

---

## 🚀 Next Task

After Task #00006 completion:
- **Task #00007:** EditorWidget Basic Implementation (Qt rich text control)

---

## 📝 Notes

- QFontComboBox filters: Use `MonospacedFonts` for editor (Consolas, Courier, Monaco)
- Font size range 8-32 pt (same as Appearance tab - consistency)
- Tab size 2-8 spaces (covers most coding styles: 2, 4, 8)
- Line Numbers: Default ON (most developers expect this)
- Word Wrap: Default OFF (code editors usually don't wrap)
- All settings use generic `SettingsManager::get<T>()` / `set<T>()` API
- No need to modify SettingsManager core (fully generic)
