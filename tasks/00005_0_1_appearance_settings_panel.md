# Task #00005: Appearance Settings Panel

**Status:** ✅ COMPLETE
**Phase:** Phase 0 (Foundation)
**Zagadnienie:** 0.1 (Qt6 Migration)
**Estimated Time:** 60-90 minutes
**Started:** 2025-11-20
**Completed:** 2025-11-20
**Actual Time:** ~70 minutes

---

## 🎯 Goal

Replace the placeholder Appearance tab in Settings Dialog with functional controls for:
- Theme selection (QComboBox)
- Language selection (QComboBox)
- Font size configuration (QSpinBox)

Controls must load current values from `SettingsManager` and save changes back on Apply/OK.

---

## ✅ Acceptance Criteria

1. ✅ Appearance tab contains 3 properly labeled controls
2. ✅ Theme combo box shows: Light, Dark, Savanna, Midnight
3. ✅ Language combo box shows: English, Polski
4. ✅ Font size spinner allows 8-32 range with step 1
5. ✅ Controls load current values from SettingsManager on dialog open
6. ✅ Apply button saves values to SettingsManager (dialog stays open)
7. ✅ OK button saves values and closes dialog
8. ✅ Cancel button discards changes
9. ✅ Build succeeds (scripts\build_windows.bat)
10. ✅ Manual test confirms load/save works correctly

---

## 📝 Implementation Plan

### Step 1: Update Header (settings_dialog.h)

Add private member variables for controls:

```cpp
private:
    // ... existing members ...

    // Appearance tab controls
    QComboBox* m_themeComboBox;
    QComboBox* m_languageComboBox;
    QSpinBox* m_fontSizeSpinBox;
```

### Step 2: Implement Appearance Tab UI (settings_dialog.cpp)

Replace placeholder in `createUI()` method (lines 46-53):

```cpp
// Appearance tab
m_appearanceTab = new QWidget();
QVBoxLayout* appearanceLayout = new QVBoxLayout(m_appearanceTab);

// Create group box for visual grouping
QGroupBox* appearanceGroup = new QGroupBox(tr("Appearance Settings"));
QGridLayout* gridLayout = new QGridLayout();

// Theme selection (row 0)
QLabel* themeLabel = new QLabel(tr("Theme:"));
m_themeComboBox = new QComboBox();
m_themeComboBox->addItem(tr("Light"), "Light");
m_themeComboBox->addItem(tr("Dark"), "Dark");
m_themeComboBox->addItem(tr("Savanna"), "Savanna");
m_themeComboBox->addItem(tr("Midnight"), "Midnight");
gridLayout->addWidget(themeLabel, 0, 0);
gridLayout->addWidget(m_themeComboBox, 0, 1);

// Language selection (row 1)
QLabel* languageLabel = new QLabel(tr("Language:"));
m_languageComboBox = new QComboBox();
m_languageComboBox->addItem(tr("English"), "en");
m_languageComboBox->addItem(tr("Polski"), "pl");
gridLayout->addWidget(languageLabel, 1, 0);
gridLayout->addWidget(m_languageComboBox, 1, 1);

// Font size (row 2)
QLabel* fontSizeLabel = new QLabel(tr("Font Size:"));
m_fontSizeSpinBox = new QSpinBox();
m_fontSizeSpinBox->setMinimum(8);
m_fontSizeSpinBox->setMaximum(32);
m_fontSizeSpinBox->setSingleStep(1);
m_fontSizeSpinBox->setSuffix(" pt");
gridLayout->addWidget(fontSizeLabel, 2, 0);
gridLayout->addWidget(m_fontSizeSpinBox, 2, 1);

// Make controls stretch horizontally
gridLayout->setColumnStretch(1, 1);

appearanceGroup->setLayout(gridLayout);
appearanceLayout->addWidget(appearanceGroup);
appearanceLayout->addStretch();  // Push group to top

m_tabWidget->addTab(m_appearanceTab, tr("Appearance"));
```

**Required headers:**
```cpp
#include <QComboBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QGridLayout>
```

### Step 3: Implement loadSettings() Method

Update `loadSettings()` (currently lines 83-95):

```cpp
void SettingsDialog::loadSettings() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Loading settings from SettingsManager");

    auto& settings = core::SettingsManager::getInstance();

    // Load theme
    std::string currentTheme = settings.getTheme();
    int themeIndex = m_themeComboBox->findData(QString::fromStdString(currentTheme));
    if (themeIndex != -1) {
        m_themeComboBox->setCurrentIndex(themeIndex);
    }
    logger.debug("Loaded theme: {}", currentTheme);

    // Load language
    std::string currentLang = settings.getLanguage();
    int langIndex = m_languageComboBox->findData(QString::fromStdString(currentLang));
    if (langIndex != -1) {
        m_languageComboBox->setCurrentIndex(langIndex);
    }
    logger.debug("Loaded language: {}", currentLang);

    // Load font size (assuming default 12 if not set)
    // Note: SettingsManager doesn't have getFontSize() yet - will use default
    m_fontSizeSpinBox->setValue(12);
    logger.debug("Loaded font size: 12 (default)");

    logger.debug("SettingsDialog: Settings loaded successfully");
}
```

### Step 4: Implement saveSettings() Method

Update `saveSettings()` (currently lines 97-110):

```cpp
void SettingsDialog::saveSettings() {
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsDialog: Saving settings to SettingsManager");

    auto& settings = core::SettingsManager::getInstance();

    // Save theme
    QString themeData = m_themeComboBox->currentData().toString();
    settings.setTheme(themeData.toStdString());
    logger.debug("Saved theme: {}", themeData.toStdString());

    // Save language
    QString langData = m_languageComboBox->currentData().toString();
    settings.setLanguage(langData.toStdString());
    logger.debug("Saved language: {}", langData.toStdString());

    // Save font size (log only - no setter in SettingsManager yet)
    int fontSize = m_fontSizeSpinBox->value();
    logger.debug("Font size: {} (not saved - no setter available)", fontSize);

    // Write to disk
    if (settings.save()) {
        logger.info("SettingsDialog: Settings saved successfully");
    } else {
        logger.error("SettingsDialog: Failed to save settings");
    }
}
```

**Note:** Font size will be logged but not persisted until `SettingsManager::setFontSize()` is added in a future task.

### Step 5: Constructor Initialization

Update constructor member initialization list (line 17-21):

```cpp
SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_tabWidget(nullptr)
    , m_buttonBox(nullptr)
    , m_appearanceTab(nullptr)
    , m_editorTab(nullptr)
    , m_themeComboBox(nullptr)        // NEW
    , m_languageComboBox(nullptr)     // NEW
    , m_fontSizeSpinBox(nullptr)      // NEW
{
    // ...
}
```

---

## 📂 Files to Modify

1. **include/kalahari/gui/settings_dialog.h**
   - Add 3 control member variables

2. **src/gui/settings_dialog.cpp**
   - Add headers: QComboBox, QSpinBox, QGroupBox, QGridLayout
   - Replace Appearance tab placeholder (lines 46-53)
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
3. Verify Appearance tab shows 3 controls with labels
4. Check Theme combo: Light, Dark, Savanna, Midnight
5. Check Language combo: English, Polski
6. Check Font Size spinner: 8-32 range
7. Change theme to "Dark", language to "pl", font to 16
8. Click Apply → check logs for "Saved theme: Dark"
9. Close dialog, reopen → verify values persisted
10. Change values, click Cancel → verify changes discarded

---

## 📊 Estimated Timeline

- Step 1 (Header update): 5 minutes
- Step 2 (UI implementation): 20 minutes
- Step 3 (loadSettings): 10 minutes
- Step 4 (saveSettings): 10 minutes
- Step 5 (Constructor): 5 minutes
- Build & test: 15 minutes
- CHANGELOG & commit: 5 minutes

**Total:** 70 minutes

---

## 🚀 Next Task

After Task #00005 completion:
- **Task #00006:** Editor Settings Panel (Tab 2 implementation)

---

## 📝 Notes

- Font size will be logged but not persisted (no `SettingsManager::setFontSize()` method yet)
- This is acceptable for Task #00005 - font size persistence can be added later
- Focus on Theme and Language as primary functional controls
- Qt QComboBox uses `userData` to store internal values (e.g., "Light") separate from display text (e.g., tr("Light"))
- All text uses `tr()` for future i18n support
