# Task #00046: ThemeManager + SettingsManager Integration

**Created:** 2025-11-18
**Phase:** Phase 1 (Zagadnienie 1.3 - Theme Management)
**Status:** 🚧 IN PROGRESS
**Priority:** 🔴 HIGH
**Estimated Time:** 3-4 hours
**Type:** Integration (Kalahari Core + BWX SDK)

---

## 📋 Context

**Previous Work:**
- ✅ Task #00043: Manual font scaling implemented (SetFont() everywhere)
- ✅ Task #00044: Auto-DPI abandoned (wxWidgets 3.3.1 unreliable)
- ✅ Task #00045: BWX Reactive Controls (14 wrappers, granular reactive pattern)

**Problem:**
Task #00045 created reactive controls (bwxButton, bwxStaticText, etc.) that auto-register and respond to broadcasts. However:
- ❌ No central coordinator to trigger broadcasts
- ❌ No integration with SettingsManager
- ❌ No user-facing UI to change font size
- ❌ Controls exist but nothing calls `bwxReactive::broadcastFontScaleChange()`

**Solution:**
Create ThemeManager singleton that:
- ✅ Reads `appearance.font_size_preset` from SettingsManager
- ✅ Calculates scale factor (0.7-2.0) from preset
- ✅ Broadcasts scale change to all bwx controls
- ✅ Provides UI in AppearanceSettingsPanel (font size dropdown)

---

## 🎯 Objectives

1. **Create ThemeManager singleton** (`src/gui/theme_manager.h/.cpp`)
2. **Add font size preset enum** (ExtraSmall...ExtraLarge)
3. **Integrate with SettingsManager** (`appearance.font_size_preset`)
4. **Add UI to AppearanceSettingsPanel** (font size dropdown)
5. **Initialize on startup** (MainWindow constructor)
6. **Test full workflow** (change preset → all controls refresh)

---

## 📦 Deliverables

### **1. FontSizePreset Enum**

Location: `src/gui/theme_manager.h`

```cpp
namespace kalahari {
namespace gui {

/// @brief Font size preset for application-wide font scaling
enum class FontSizePreset {
    ExtraSmall = 0,  // 70% (scale 0.7)
    Small = 1,       // 85% (scale 0.85)
    Normal = 2,      // 100% (scale 1.0) - DEFAULT
    Medium = 3,      // 115% (scale 1.15)
    Large = 4,       // 130% (scale 1.3)
    ExtraLarge = 5   // 150% (scale 1.5)
};

} // namespace gui
} // namespace kalahari
```

**Rationale:**
- 6 presets cover typical user needs (small monitors → 4K displays)
- Scale factors: 0.7, 0.85, 1.0, 1.15, 1.3, 1.5
- Normal (1.0) is default - no scaling for fresh installs
- ExtraLarge (1.5) useful for accessibility

---

### **2. ThemeManager Singleton**

Location: `src/gui/theme_manager.h`

```cpp
/// @brief Central theme management singleton
///
/// Coordinates appearance settings (fonts, colors, icons) across the application.
/// Integrates with SettingsManager and broadcasts changes to bwx::gui::bwxReactive controls.
///
/// **Responsibilities:**
/// - Read `appearance.font_size_preset` from SettingsManager
/// - Calculate scale factor from preset
/// - Broadcast font scale changes to all bwx controls
/// - (Future Phase 2) Manage color themes, icon sets
///
/// **Usage:**
/// ```cpp
/// // Initialize on startup (MainWindow constructor)
/// ThemeManager::getInstance().initialize(settingsManager);
///
/// // Apply changes when user saves settings
/// ThemeManager::getInstance().applyFontSizePreset(preset);
/// ```
class ThemeManager {
public:
    /// Get singleton instance
    static ThemeManager& getInstance();

    /// Initialize with SettingsManager reference
    /// @param settingsManager Reference to SettingsManager singleton
    void initialize(core::SettingsManager& settingsManager);

    /// Apply font size preset (updates all bwx controls)
    /// @param preset Font size preset (ExtraSmall...ExtraLarge)
    void applyFontSizePreset(FontSizePreset preset);

    /// Get current font size preset
    /// @return Current preset
    FontSizePreset getCurrentFontSizePreset() const;

    /// Convert preset to scale factor
    /// @param preset Font size preset
    /// @return Scale factor (0.7-1.5)
    static double presetToScale(FontSizePreset preset);

    /// Convert scale to preset (reverse mapping)
    /// @param scale Scale factor
    /// @return Closest matching preset
    static FontSizePreset scaleToPreset(double scale);

    /// Convert preset to display string
    /// @param preset Font size preset
    /// @return Display string ("Extra Small", "Normal", etc.)
    static std::string presetToString(FontSizePreset preset);

    /// Convert display string to preset
    /// @param str Display string
    /// @return Preset (Normal if invalid string)
    static FontSizePreset stringToPreset(const std::string& str);

private:
    ThemeManager() = default;
    ~ThemeManager() = default;

    // Disable copy/move
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    core::SettingsManager* m_settingsManager = nullptr;  ///< Settings reference
    FontSizePreset m_currentPreset = FontSizePreset::Normal;  ///< Current preset
};

} // namespace gui
} // namespace kalahari
```

---

### **3. ThemeManager Implementation**

Location: `src/gui/theme_manager.cpp`

**Key methods:**

```cpp
void ThemeManager::initialize(core::SettingsManager& settingsManager) {
    m_settingsManager = &settingsManager;

    // Read current preset from settings (default: Normal)
    int presetValue = m_settingsManager->getInt("appearance.font_size_preset", 2);
    m_currentPreset = static_cast<FontSizePreset>(presetValue);

    // Apply current preset on startup
    applyFontSizePreset(m_currentPreset);
}

void ThemeManager::applyFontSizePreset(FontSizePreset preset) {
    m_currentPreset = preset;

    // Save to SettingsManager
    if (m_settingsManager) {
        m_settingsManager->setInt("appearance.font_size_preset", static_cast<int>(preset));
    }

    // Calculate scale factor
    double scale = presetToScale(preset);

    // Broadcast to all bwx controls
    bwx::gui::bwxReactive::broadcastFontScaleChange(scale);

    core::Logger::getInstance().info("ThemeManager: Applied font size preset '{}' (scale {:.2f})",
        presetToString(preset), scale);
}

double ThemeManager::presetToScale(FontSizePreset preset) {
    switch (preset) {
        case FontSizePreset::ExtraSmall: return 0.7;
        case FontSizePreset::Small:      return 0.85;
        case FontSizePreset::Normal:     return 1.0;
        case FontSizePreset::Medium:     return 1.15;
        case FontSizePreset::Large:      return 1.3;
        case FontSizePreset::ExtraLarge: return 1.5;
        default:                         return 1.0;
    }
}

std::string ThemeManager::presetToString(FontSizePreset preset) {
    switch (preset) {
        case FontSizePreset::ExtraSmall: return "Extra Small";
        case FontSizePreset::Small:      return "Small";
        case FontSizePreset::Normal:     return "Normal";
        case FontSizePreset::Medium:     return "Medium";
        case FontSizePreset::Large:      return "Large";
        case FontSizePreset::ExtraLarge: return "Extra Large";
        default:                         return "Normal";
    }
}
```

---

### **4. AppearanceSettingsPanel UI Update**

Location: `src/gui/appearance_settings_panel.h`

**Add new section:**

```cpp
private:
    // NEW: Font Size Section
    void createFontSizeSection(wxSizer* parent);

    // NEW: Controls
    bwx::gui::Choice* m_fontSizeChoice = nullptr;
    bwx::gui::StaticText* m_fontSizeDescription = nullptr;
```

Location: `src/gui/appearance_settings_panel.cpp`

**In constructor (after createIconSection):**

```cpp
AppearanceSettingsPanel::AppearanceSettingsPanel(wxWindow* parent, SettingsState& state)
    : wxPanel(parent), m_state(state)
{
    core::Logger::getInstance().debug("AppearanceSettingsPanel: Creating panel");

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Create 3 sections (Task #00046: Font size added)
    createThemeSection(mainSizer);
    createFontSizeSection(mainSizer);  // NEW
    createIconSection(mainSizer);

    SetSizer(mainSizer);
    core::Logger::getInstance().info("AppearanceSettingsPanel: Panel created with 3 sections");
}
```

**New method implementation:**

```cpp
void AppearanceSettingsPanel::createFontSizeSection(wxSizer* parent) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Font Size");

    // Description
    m_fontSizeDescription = new bwx::gui::StaticText(box->GetStaticBox(), wxID_ANY,
        "Adjust text size throughout the application");
    m_fontSizeDescription->SetFont(m_fontSizeDescription->GetFont().MakeItalic());
    box->Add(m_fontSizeDescription, 0, wxALL | wxEXPAND, 5);

    // Font size choice
    wxBoxSizer* fontSizeSizer = new wxBoxSizer(wxHORIZONTAL);
    bwx::gui::StaticText* fontSizeLabel = new bwx::gui::StaticText(box->GetStaticBox(), wxID_ANY,
        "Application font size:");
    fontSizeSizer->Add(fontSizeLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_fontSizeChoice = new bwx::gui::Choice(box->GetStaticBox(), wxID_ANY);
    m_fontSizeChoice->Append("Extra Small");  // Index 0
    m_fontSizeChoice->Append("Small");        // Index 1
    m_fontSizeChoice->Append("Normal");       // Index 2
    m_fontSizeChoice->Append("Medium");       // Index 3
    m_fontSizeChoice->Append("Large");        // Index 4
    m_fontSizeChoice->Append("Extra Large");  // Index 5

    // Select current preset from state
    m_fontSizeChoice->SetSelection(m_state.fontSizePreset);
    m_fontSizeChoice->SetToolTip("Choose font size for all text in the application");
    fontSizeSizer->Add(m_fontSizeChoice, 1, wxEXPAND, 0);

    box->Add(fontSizeSizer, 0, wxALL | wxEXPAND, 5);

    // Note
    bwx::gui::StaticText* note = new bwx::gui::StaticText(box->GetStaticBox(), wxID_ANY,
        "Note: Font size changes apply immediately when you click Apply.");
    wxFont noteFont = note->GetFont();
    noteFont.MakeItalic();
    noteFont.SetPointSize(noteFont.GetPointSize() - 1);
    note->SetFont(noteFont);
    note->SetForegroundColour(wxColour(100, 100, 100));
    box->Add(note, 0, wxALL | wxEXPAND, 5);

    parent->Add(box, 0, wxALL | wxEXPAND, 10);
}
```

---

### **5. SettingsState Update**

Location: `src/gui/settings_dialog.h`

**Add to SettingsState struct:**

```cpp
struct SettingsState {
    // Existing fields...
    std::string themeName;
    int iconSize;

    // NEW: Font size preset (Task #00046)
    int fontSizePreset = 2;  // Default: Normal (FontSizePreset::Normal)
};
```

Location: `src/gui/settings_dialog.cpp`

**In loadSettings():**

```cpp
void SettingsDialog::loadSettings() {
    auto& settings = core::SettingsManager::getInstance();

    // Load font size preset (Task #00046)
    m_state.fontSizePreset = settings.getInt("appearance.font_size_preset", 2);

    // Existing loads...
}
```

**In applySettings():**

```cpp
void SettingsDialog::applySettings() {
    auto& settings = core::SettingsManager::getInstance();

    // Apply font size preset (Task #00046)
    settings.setInt("appearance.font_size_preset", m_state.fontSizePreset);
    ThemeManager::getInstance().applyFontSizePreset(
        static_cast<FontSizePreset>(m_state.fontSizePreset)
    );

    // Existing applies...

    settings.save();
}
```

**In onApply():**

```cpp
void SettingsDialog::onApply(wxCommandEvent& event) {
    // Read font size from AppearanceSettingsPanel (Task #00046)
    if (m_appearancePanel) {
        m_state.fontSizePreset = m_appearancePanel->getFontSizePreset();
    }

    // Existing reads...

    applySettings();

    core::Logger::getInstance().info("SettingsDialog: Applied all settings");
}
```

---

### **6. AppearanceSettingsPanel Getter**

Location: `src/gui/appearance_settings_panel.h`

```cpp
public:
    /// Get currently selected font size preset index
    /// @return Index (0-5) corresponding to FontSizePreset
    int getFontSizePreset() const {
        return m_fontSizeChoice ? m_fontSizeChoice->GetSelection() : 2;
    }
```

---

### **7. MainWindow Initialization**

Location: `src/gui/main_window.cpp`

**In constructor (after Settings initialization):**

```cpp
MainWindow::MainWindow()
    : wxFrame(nullptr, wxID_ANY, "Kalahari Writer's IDE", wxDefaultPosition, wxSize(1280, 720))
{
    // Existing initialization...

    // Initialize ThemeManager (Task #00046)
    gui::ThemeManager::getInstance().initialize(core::SettingsManager::getInstance());

    core::Logger::getInstance().info("MainWindow: Theme manager initialized");

    // Continue with rest of initialization...
}
```

---

### **8. CMakeLists.txt Update**

Add new source file:

```cmake
# src/CMakeLists.txt

set(KALAHARI_SOURCES
    # Existing sources...
    gui/theme_manager.cpp  # NEW (Task #00046)
)
```

---

## 🛠️ Implementation Steps

### **Step 1: Create ThemeManager Files (45 min)**

1. Create `src/gui/theme_manager.h` (enum + singleton class)
2. Create `src/gui/theme_manager.cpp` (implementation)
3. Implement:
   - FontSizePreset enum (6 values)
   - ThemeManager singleton pattern
   - initialize() method
   - applyFontSizePreset() method
   - presetToScale() / scaleToPreset() conversions
   - presetToString() / stringToPreset() conversions

### **Step 2: Update SettingsState (15 min)**

1. Add `fontSizePreset` field to SettingsState struct
2. Update loadSettings() to read `appearance.font_size_preset`
3. Update applySettings() to call ThemeManager::applyFontSizePreset()
4. Update onApply() to read from AppearanceSettingsPanel

### **Step 3: Update AppearanceSettingsPanel (45 min)**

1. Add createFontSizeSection() method
2. Add m_fontSizeChoice control
3. Call createFontSizeSection() in constructor
4. Add getFontSizePreset() getter
5. Update layout (3 sections: Theme, Font Size, Icons)

### **Step 4: Initialize in MainWindow (10 min)**

1. Call ThemeManager::getInstance().initialize() in MainWindow constructor
2. Verify initialization order (after SettingsManager, before GUI creation)

### **Step 5: Update CMakeLists.txt (5 min)**

1. Add `gui/theme_manager.cpp` to KALAHARI_SOURCES

### **Step 6: Build and Test (60 min)**

1. Build: `cmake --build build-linux-wsl --config Debug -j4`
2. Verify compilation (no errors)
3. Manual testing:
   - Launch Kalahari
   - Verify default font size (Normal, 100%)
   - Open Settings → Appearance
   - Change font size to "Large"
   - Click Apply
   - Verify all controls scale to 130%
   - Change to "Extra Small"
   - Click Apply
   - Verify all controls scale to 70%
   - Restart application
   - Verify setting persists (still Extra Small)

---

## ✅ Acceptance Criteria

1. **Compilation:**
   - ✅ ThemeManager compiles without errors
   - ✅ AppearanceSettingsPanel compiles without errors
   - ✅ MainWindow compiles without errors

2. **Functionality:**
   - ✅ ThemeManager initializes on startup
   - ✅ Default font size is Normal (100%, scale 1.0)
   - ✅ Font size dropdown appears in Settings → Appearance
   - ✅ Changing preset + Apply scales all bwx controls immediately
   - ✅ ExtraSmall (70%) works correctly
   - ✅ ExtraLarge (150%) works correctly
   - ✅ Setting persists after restart

3. **Code Quality:**
   - ✅ Singleton pattern implemented correctly
   - ✅ No memory leaks (SettingsManager reference, not pointer ownership)
   - ✅ Proper logging (info level for preset changes)
   - ✅ Doxygen comments for all public methods

---

## 🚀 Next Steps

**Task #00047: MainWindow Control Migration**
- Replace all `wxButton*` → `bwx::gui::bwxButton*`
- Replace all `wxStaticText*` → `bwx::gui::bwxStaticText*`
- Remove manual font propagation code (zero boilerplate!)
- Test: Change font preset → all MainWindow controls refresh

**Task #00048: Full Application Control Migration**
- Migrate remaining panels (Navigator, Log, etc.)
- Replace all wx* controls with bwx::gui::* equivalents
- Verify all controls respond to theme changes
- Stress test with all 6 presets

**Phase 2+ Features:**
- Color theme system (Light/Dark mode colors)
- Icon scaling (integrate with font size preset)
- Custom user fonts (FontDialog integration)

---

## 🔧 Post-Implementation Fixes

### **Fix #1: Initialization Order Bug**

**Problem Discovered (2025-11-18):**
- User reported: "mimo, że zapisana została wartośc Medium, to po ponownym uruchomieniu okna/programu, nie widać żadnego efektu"
- Settings were saved correctly (Medium preset persisted in settings.json)
- But controls showed default size on application restart

**Root Cause:**
- ThemeManager::initialize() was called at line 237 (MainWindow constructor)
- BUT bwx controls were created later in initializeAUI() at line 274
- Therefore: `broadcastFontScaleChange()` broadcasted to EMPTY registry (0 controls)
- Controls created later with default fonts, never receiving the saved preset

**Fix Applied:**
- Moved ThemeManager::initialize() to AFTER initializeAUI() (line 274)
- This ensures controls exist BEFORE broadcast happens
- Changed code in `src/gui/main_window.cpp`:

```cpp
// Initialize wxAUI docking system with panels (Task #00013)
initializeAUI();

// Initialize ThemeManager AFTER controls are created (Task #00046 fix)
// This ensures broadcastFontScaleChange() reaches already-registered bwx controls
core::Logger::getInstance().info("Initializing ThemeManager...");
gui::ThemeManager::getInstance().initialize(settings);
core::Logger::getInstance().info("ThemeManager initialized");
```

**Lesson Learned:**
- Observer pattern requires observers to exist BEFORE broadcast
- Initialization order matters for reactive systems
- Always initialize coordinator AFTER registering subscribers

### **Fix #2: Dynamic Dialog Sizing**

**Problem Discovered (2025-11-18):**
- User reported: "Okno dialogowe nie zmienia rozmiaru" (Dialog doesn't resize)
- Controls changed font but dialog stayed same size

**Root Cause:**
- Hardcoded `wxSize(800, 600)` in SettingsDialog constructor
- Used `FitInside()` instead of `Fit()` (violates CLAUDE.md rules)

**User Directive:**
> "nie mozemy używać żadnych wxSize. Chyba że wyłącznie w kontekście minimal size. Cała kalkulacja wilkości każdego elementu interfejsu powinna być dynamiczna przez Fit() - nie uzywamy FitInside()!"

**Fix Applied:**
1. Changed constructor: `wxSize(800, 600)` → `wxDefaultSize`
2. Added `Fit()` after SetSizer() to calculate initial size dynamically
3. Added dynamic minimum size: `SetMinSize(wxSize(700, 500))` based on best size
4. Rewrote `onFontScaleChanged()` to use only `Fit()` (no `FitInside()`, no `SetSize()`)
5. Added `SetMinSize(wxSize(400, 400))` to m_contentPanel (wxScrolledWindow) to prevent collapse

**Code Pattern (SettingsDialog::onFontScaleChanged):**
```cpp
void SettingsDialog::onFontScaleChanged(double scale) {
    // Invalidate cached sizes
    InvalidateBestSize();
    if (m_contentPanel) m_contentPanel->InvalidateBestSize();
    if (m_currentPanel) m_currentPanel->InvalidateBestSize();

    // Recalculate layout bottom-to-top
    if (m_currentPanel && m_currentPanel->GetSizer()) {
        m_currentPanel->Layout();
    }
    if (m_contentPanel) {
        m_contentPanel->Layout();
    }
    Layout();

    // Dynamically resize dialog to fit new content (Fit() instead of SetSize!)
    Fit();

    // Ensure minimum size is respected
    wxSize currentSize = GetSize();
    wxSize minSize = GetMinSize();
    wxSize finalSize(std::max(currentSize.GetWidth(), minSize.GetWidth()),
                     std::max(currentSize.GetHeight(), minSize.GetHeight()));
    if (finalSize != currentSize) {
        SetSize(finalSize);
    }
    Refresh();
}
```

**Lesson Learned:**
- Never use hardcoded wxSize except for minimums
- Use Fit() for dynamic sizing, not FitInside() or SetSize()
- wxScrolledWindow needs explicit minimum size to prevent collapse
- Dynamic sizing requires proper InvalidateBestSize() + Layout() sequence

---

## 📝 Notes

**Why ThemeManager in gui/ namespace?**
- Manages GUI appearance (fonts, colors, icons)
- Interacts heavily with bwxReactive (GUI-specific)
- Depends on wxWidgets (GUI framework)
- Not a generic core service

**Why scale-based instead of absolute font sizes?**
- BWX SDK controls use scale-based approach (Task #00045)
- Scale factor (0.7-1.5) is more flexible than fixed sizes
- Works across different platforms (Windows/macOS/Linux)
- Easier to extend (can add more presets without changing control code)

**Why 6 presets (not 5 or 7)?**
- Covers typical use cases (small laptops to 4K displays)
- Not overwhelming for users (too many choices = confusion)
- ExtraSmall (0.7) useful for 4K displays with limited space
- ExtraLarge (1.5) useful for accessibility / large monitors

**Why immediate Apply (not restart required)?**
- BWX Reactive pattern supports instant updates
- Better UX (user sees changes immediately)
- No need to restart application
- Note in Theme section still mentions restart (for color theme)

---

## 🤖 Generated with [Claude Code](https://claude.com/claude-code)

**Co-Authored-By:** Claude <noreply@anthropic.com>
