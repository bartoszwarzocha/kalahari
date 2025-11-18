# Task #00045: BWX Reactive Controls - wxWidgets Wrappers

**Created:** 2025-11-17
**Phase:** Phase 1 (Zagadnienie 1.3 - Theme Management)
**Status:** 🚧 IN PROGRESS
**Priority:** 🔴 HIGH
**Estimated Time:** 4-6 hours
**Type:** Implementation (BWX SDK Core Feature)

---

## 📋 Context

**Previous Work:**
- ✅ Task #00043: bwxReactive base class implemented (static registry + broadcast)
- ✅ Task #00044: DPI Support attempted (failed - unpredictable auto-DPI behavior)
- ✅ Architectural Decision: Replace auto-DPI with user-controlled ThemeManager

**Problem:**
Top-level reactive pattern (MainWindow : public bwxReactive) requires:
- ❌ Manual font propagation (50+ lines boilerplate per window)
- ❌ Easy to forget controls (runtime bugs)
- ❌ Hardcoded logic (m_titleLabel uses GetTitleFont - how to remember?)
- ❌ Duplicate code across windows/dialogs/panels

**Solution:**
Granular reactive controls - each wxWidgets control gets a bwx wrapper:
- ✅ Auto-registers in bwxReactive static registry
- ✅ Auto-refreshes font/theme on broadcast
- ✅ Smart modes (title, editor, small, etc.)
- ✅ Zero boilerplate in MainWindow/dialogs

---

## 🎯 Objectives

1. **Create bwx_controls.h/.cpp** in BWX SDK
2. **Implement Tier 1 controls** (10 most common wxWidgets controls)
3. **Use scale-based approach** (no dependency on ThemeManager for BWX SDK independence)
4. **Add smart modes** (title mode, editor mode, small mode)
5. **Verify compilation** (explicit template instantiation, multiple inheritance)

---

## 📦 Deliverables

### **1. bwx_controls.h** (Header)

Location: `external/bwx_sdk/include/bwx_sdk/bwx_gui/bwx_controls.h`

**Tier 1 Controls (Must-have):**
- ✅ bwxButton (wxButton wrapper)
- ✅ bwxStaticText (wxStaticText wrapper + title mode)
- ✅ bwxTextCtrl (wxTextCtrl wrapper + editor mode)
- ✅ bwxChoice (wxChoice wrapper)
- ✅ bwxCheckBox (wxCheckBox wrapper)
- ✅ bwxRadioButton (wxRadioButton wrapper)
- ✅ bwxNotebook (wxNotebook wrapper)
- ✅ bwxAuiNotebook (wxAuiNotebook wrapper - complex tabs handling)
- ✅ bwxTreeCtrl (wxTreeCtrl wrapper)
- ✅ bwxListCtrl (wxListCtrl wrapper)

**Features:**
- Multiple inheritance: `class bwxButton : public wxButton, public bwxReactive`
- Constructor forwarding: `using wxButton::wxButton;` (C++11)
- Smart modes: `SetTitleMode(bool)`, `SetEditorMode(bool)`, `SetSmallMode(bool)`
- Scale-based font update (no ThemeManager dependency)

---

### **2. bwx_controls.cpp** (Implementation)

Location: `external/bwx_sdk/src/bwx_gui/bwx_controls.cpp`

**Implementation for each control:**

```cpp
void bwxButton::onFontScaleChanged(double scale) {
    // Scale-based approach (BWX SDK independent)
    wxFont font = GetFont();
    int baseSize = 10;  // System default
    int newSize = static_cast<int>(baseSize * scale);
    font.SetPointSize(newSize);
    SetFont(font);
    Refresh();
}

void bwxButton::onThemeChanged(const std::string& themeName) {
    // Future: Update colors, icons
    // For now: no-op (Phase 2)
}
```

**Special case: bwxStaticText (title mode)**

```cpp
class bwxStaticText : public wxStaticText, public bwxReactive {
public:
    using wxStaticText::wxStaticText;

    void SetTitleMode(bool isTitle) { m_isTitleMode = isTitle; }

protected:
    void onFontScaleChanged(double scale) override {
        wxFont font = GetFont();
        int baseSize = m_isTitleMode ? 15 : 10;  // Title = 1.5x
        int newSize = static_cast<int>(baseSize * scale);
        font.SetPointSize(newSize);
        if (m_isTitleMode) {
            font.MakeBold();
        }
        SetFont(font);
        Refresh();
    }

private:
    bool m_isTitleMode = false;
};
```

**Special case: bwxTextCtrl (editor mode - monospace)**

```cpp
class bwxTextCtrl : public wxTextCtrl, public bwxReactive {
public:
    using wxTextCtrl::wxTextCtrl;

    void SetEditorMode(bool isEditor) { m_isEditorMode = isEditor; }

protected:
    void onFontScaleChanged(double scale) override {
        int baseSize = 10;
        int newSize = static_cast<int>(baseSize * scale);

        if (m_isEditorMode) {
            // Monospace font for editor
            wxFont font(newSize, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
            SetFont(font);
        } else {
            wxFont font = GetFont();
            font.SetPointSize(newSize);
            SetFont(font);
        }

        Refresh();
    }

private:
    bool m_isEditorMode = false;
};
```

**Special case: bwxAuiNotebook (complex tabs handling)**

```cpp
class bwxAuiNotebook : public wxAuiNotebook, public bwxReactive {
public:
    using wxAuiNotebook::wxAuiNotebook;

protected:
    void onFontScaleChanged(double scale) override {
        int baseSize = 10;
        int newSize = static_cast<int>(baseSize * scale);

        wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
        font.SetPointSize(newSize);

        // Set fonts on notebook
        SetFont(font);
        SetNormalFont(font);
        SetSelectedFont(font);
        SetMeasuringFont(font);

        // Set fonts on wxAuiTabArt (critical for tabs!)
        if (wxAuiTabArt* tabArt = GetArtProvider()) {
            tabArt->SetNormalFont(font);
            tabArt->SetSelectedFont(font);
            tabArt->SetMeasuringFont(font);
        }

        Refresh();
        Update();  // Force immediate redraw
    }
};
```

---

### **3. CMakeLists.txt Update**

Add bwx_controls.cpp to BWX SDK sources:

```cmake
# external/bwx_sdk/CMakeLists.txt

set(BWX_SDK_SOURCES
    src/bwx_gui/bwx_reactive.cpp
    src/bwx_gui/bwx_controls.cpp  # ← NEW
)
```

---

## 🛠️ Implementation Steps

### **Step 1: Create bwx_controls.h (60 min)**

1. Create header file: `external/bwx_sdk/include/bwx_sdk/bwx_gui/bwx_controls.h`
2. Add copyright header + documentation
3. Implement 10 Tier 1 controls:
   - bwxButton
   - bwxStaticText (+ title mode)
   - bwxTextCtrl (+ editor mode)
   - bwxChoice
   - bwxCheckBox
   - bwxRadioButton
   - bwxNotebook
   - bwxAuiNotebook (complex)
   - bwxTreeCtrl
   - bwxListCtrl
4. Add type aliases (convenience):
   ```cpp
   namespace bwx {
   namespace gui {
       using Button = bwxButton;
       using StaticText = bwxStaticText;
       // ... etc.
   } // namespace gui
   } // namespace bwx
   ```

### **Step 2: Create bwx_controls.cpp (90 min)**

1. Create implementation file: `external/bwx_sdk/src/bwx_gui/bwx_controls.cpp`
2. Implement `onFontScaleChanged()` for each control (scale-based)
3. Implement `onThemeChanged()` stubs (future Phase 2)
4. Special handling:
   - bwxStaticText: title mode (1.5x size, bold)
   - bwxTextCtrl: editor mode (monospace)
   - bwxAuiNotebook: wxAuiTabArt fonts

### **Step 3: Update CMakeLists.txt (5 min)**

1. Add `src/bwx_gui/bwx_controls.cpp` to `BWX_SDK_SOURCES`
2. Verify no missing includes

### **Step 4: Build and Verify (30 min)**

1. Update submodule: `cd external/bwx_sdk && git add . && git commit -m "feat(bwx-sdk): Add reactive controls wrappers (Task #00045)"`
2. Update parent repo: `cd ../.. && git add external/bwx_sdk && git commit -m "feat(bwx-sdk): Update submodule (Task #00045)"`
3. Build: `cmake --build build-linux-wsl --config Debug -j4`
4. Verify:
   - ✅ No compilation errors
   - ✅ All 10 controls compile
   - ✅ Multiple inheritance works (wxButton + bwxReactive)
   - ✅ Constructor forwarding works (`using wxButton::wxButton;`)

### **Step 5: Basic Test (Manual - 15 min)**

1. Create simple test window:
   ```cpp
   bwx::gui::bwxButton* btn = new bwx::gui::bwxButton(this, wxID_OK, "OK");
   ```
2. Verify auto-registration:
   ```cpp
   size_t count = bwx::gui::bwxReactive::getRegisteredControlsCount();
   // Should be > 0
   ```
3. Test broadcast:
   ```cpp
   bwx::gui::bwxReactive::broadcastFontScaleChange(1.5);
   // Button font should scale to 15pt
   ```

---

## ✅ Acceptance Criteria

1. **Compilation:**
   - ✅ bwx_controls.h compiles without errors
   - ✅ bwx_controls.cpp compiles without errors
   - ✅ All 10 Tier 1 controls are implemented
   - ✅ Multiple inheritance (wxButton + bwxReactive) works
   - ✅ Constructor forwarding works

2. **Functionality:**
   - ✅ Controls auto-register in bwxReactive::s_controls[]
   - ✅ Controls auto-unregister on destruction
   - ✅ `broadcastFontScaleChange(1.5)` scales all controls to 15pt
   - ✅ Title mode (bwxStaticText) uses 1.5x size + bold
   - ✅ Editor mode (bwxTextCtrl) uses monospace font
   - ✅ bwxAuiNotebook tabs scale correctly

3. **Code Quality:**
   - ✅ Proper documentation (Doxygen comments)
   - ✅ Consistent naming (bwx prefix)
   - ✅ No hardcoded values (use baseSize = 10, scale dynamically)
   - ✅ Thread-safe (single-threaded GUI thread for MVP)

---

## 🚀 Next Steps

**Task #00046: ThemeManager + SettingsManager Integration**
- Create ThemeManager singleton (Kalahari)
- Add `appearance.font_size_preset` to SettingsManager
- ThemeManager::NotifyThemeChanged() calls bwxReactive::broadcastFontScaleChange()
- Settings Dialog: Add "Font Size" dropdown (ExtraSmall...ExtraLarge)

**Task #00047: MainWindow Integration**
- Replace `wxButton* m_okButton` → `bwx::gui::bwxButton* m_okButton`
- Remove manual font propagation code (zero boilerplate!)
- Test: Change font preset in Settings → all controls refresh

---

## 📝 Notes

**Why scale-based approach (not ThemeManager)?**
- BWX SDK is reusable library (independent of Kalahari)
- ThemeManager is Kalahari-specific
- Scale-based: `onFontScaleChanged(1.5)` → 15pt font (universal)
- ThemeManager can call: `bwxReactive::broadcastFontScaleChange(baseFontSize / 10.0)`

**Why not Policy template?**
- Too complex for MVP
- Scale-based is simpler and works well
- Policy can be added in Phase 2 if needed

**Why 10 controls (Tier 1)?**
- Covers 90% of Kalahari UI
- ~3-4 hours work
- Tier 2 (15 more controls) can be added later incrementally

---

## 🤖 Generated with [Claude Code](https://claude.com/claude-code)

**Co-Authored-By:** Claude <noreply@anthropic.com>
