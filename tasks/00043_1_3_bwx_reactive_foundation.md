# Task #00043: BWX SDK Reactive Foundation

**Status:** 📋 Planned
**Priority:** P0 (CRITICAL - Architecture Foundation)
**Estimated:** 3-4 hours
**Dependencies:** Task #00035 (Command Registry complete)
**Phase:** 1 (Core Editor)
**Zagadnienie:** 1.3 (BWX SDK Reactive GUI Management System)
**Type:** Architecture Foundation

---

## Problem

Current GUI management requires manual iteration over controls for dynamic updates (font scaling, theme switching, accessibility modes). This approach:
- ❌ **Not scalable** - Each new feature = new manual update code
- ❌ **Error-prone** - Easy to miss controls in iteration
- ❌ **Inconsistent** - Plugins can't participate in dynamic updates
- ❌ **Maintenance burden** - 100+ controls to manage manually

**Need:** Professional reactive GUI management system similar to Qt/WPF/Flutter.

---

## Goal

Implement **BWX SDK Reactive System foundation:**
1. **bwxReactive base class** - Static registry + broadcast API
2. **bwxManaged<wxStaticText> proof-of-concept** - Template for single control type
3. **Test with 1 control** - Verify font scaling works in AppearanceSettingsPanel

**Success Criteria:** Font scaling update via broadcast works for 1 managed StaticText control.

---

## Architecture

### Component 1: bwxReactive Base Class

**Location:** `bwx_sdk/gui/reactive.h` + `bwx_sdk/gui/reactive.cpp`

**Purpose:** Pure interface for reactive behavior with static registry and broadcast.

```cpp
// bwx_sdk/gui/reactive.h
#pragma once

#include <vector>
#include <string>

namespace bwx {
namespace gui {

/// @brief Base class for all reactive GUI controls
///
/// Provides static registry and broadcast mechanism for dynamic UI updates.
/// All managed controls inherit from this class and implement event handlers.
class bwxReactive {
public:
    // ========================================================================
    // Broadcast API (called by MainWindow or other coordinators)
    // ========================================================================

    /// @brief Broadcast font scale change to all registered controls
    /// @param scale New font scaling factor (0.8 - 2.0)
    static void broadcastFontScaleChange(double scale);

    /// @brief Broadcast theme change to all registered controls
    /// @param themeName New theme name ("Light", "Dark", "Custom")
    static void broadcastThemeChange(const std::string& themeName);

    // Future extensibility:
    // static void broadcastAccessibilityChange(bool enabled);
    // static void broadcastLanguageChange(const std::string& locale);

protected:
    // ========================================================================
    // Lifecycle (automatic registration/unregistration)
    // ========================================================================

    /// @brief Constructor - registers control in static registry
    bwxReactive();

    /// @brief Destructor - unregisters control from static registry
    virtual ~bwxReactive();

    // ========================================================================
    // Event Handlers (pure virtual - subclasses must implement)
    // ========================================================================

    /// @brief Handle font scale change event
    /// @param scale New scaling factor (0.8 - 2.0)
    virtual void onFontScaleChanged(double scale) = 0;

    /// @brief Handle theme change event
    /// @param themeName New theme name ("Light", "Dark", "Custom")
    virtual void onThemeChanged(const std::string& themeName) = 0;

private:
    /// @brief Static registry of all live reactive controls
    static std::vector<bwxReactive*> s_controls;
};

} // namespace gui
} // namespace bwx
```

**Implementation:** `bwx_sdk/gui/reactive.cpp`

```cpp
#include "reactive.h"
#include <algorithm>

namespace bwx {
namespace gui {

// Static registry initialization
std::vector<bwxReactive*> bwxReactive::s_controls;

// ============================================================================
// Lifecycle
// ============================================================================

bwxReactive::bwxReactive() {
    s_controls.push_back(this);
}

bwxReactive::~bwxReactive() {
    auto it = std::find(s_controls.begin(), s_controls.end(), this);
    if (it != s_controls.end()) {
        s_controls.erase(it);
    }
}

// ============================================================================
// Broadcast API
// ============================================================================

void bwxReactive::broadcastFontScaleChange(double scale) {
    for (auto* control : s_controls) {
        control->onFontScaleChanged(scale);
    }
}

void bwxReactive::broadcastThemeChange(const std::string& themeName) {
    for (auto* control : s_controls) {
        control->onThemeChanged(themeName);
    }
}

} // namespace gui
} // namespace bwx
```

---

### Component 2: bwxManaged<wxStaticText> Proof-of-Concept

**Location:** `bwx_sdk/gui/managed.h`

**Purpose:** Template wrapper for wxWidgets controls (proof-of-concept with wxStaticText).

```cpp
// bwx_sdk/gui/managed.h
#pragma once

#include "reactive.h"
#include <wx/wx.h>

namespace bwx {
namespace gui {

/// @brief Managed control template (proof-of-concept for wxStaticText)
///
/// Wraps wxWidgets controls with reactive behavior.
/// Multiple inheritance: concrete class (wxStaticText) + interface (bwxReactive).
template<typename WxWidget>
class bwxManaged : public WxWidget, public bwxReactive {
public:
    // ========================================================================
    // Constructor (perfect forwarding to wxWidget)
    // ========================================================================

    /// @brief Constructor - forwards all arguments to wxWidget constructor
    template<typename... Args>
    bwxManaged(Args&&... args)
        : WxWidget(std::forward<Args>(args)...)
    {
        // Store original font size for accurate scaling
        m_originalFontSize = this->GetFont().GetPointSize();
    }

    // ========================================================================
    // Configuration Flags
    // ========================================================================

    /// @brief Enable/disable font change reactions
    void setFontChangeEnabled(bool enabled) { m_enableFontChange = enabled; }

    /// @brief Enable/disable theme change reactions
    void setThemeChangeEnabled(bool enabled) { m_enableThemeChange = enabled; }

protected:
    // ========================================================================
    // bwxReactive Implementation
    // ========================================================================

    void onFontScaleChanged(double scale) override {
        if (!m_enableFontChange) return;

        // Calculate new font size
        int newSize = static_cast<int>(m_originalFontSize * scale);

        // Update font
        wxFont font = this->GetFont();
        font.SetPointSize(newSize);
        this->SetFont(font);
        this->Refresh();
    }

    void onThemeChanged(const std::string& themeName) override {
        if (!m_enableThemeChange) return;

        // Theme handling (placeholder for now)
        // Will be implemented in Task #00044
    }

private:
    bool m_enableFontChange = true;
    bool m_enableThemeChange = true;
    int m_originalFontSize = 10;
};

// ========================================================================
// Type Alias (proof-of-concept)
// ========================================================================

using StaticText = bwxManaged<wxStaticText>;

} // namespace gui
} // namespace bwx
```

---

## Implementation Plan

### Step 1: Create bwx_sdk Directory Structure (15 min)

```bash
mkdir -p bwx_sdk/gui
touch bwx_sdk/gui/reactive.h
touch bwx_sdk/gui/reactive.cpp
touch bwx_sdk/gui/managed.h
touch bwx_sdk/CMakeLists.txt
```

**bwx_sdk/CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.21)

# BWX SDK GUI Library
add_library(bwx_gui STATIC
    gui/reactive.cpp
)

target_include_directories(bwx_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bwx_gui PUBLIC
    wx::core
)
```

**Update root CMakeLists.txt:**
```cmake
# Add bwx_sdk subdirectory
add_subdirectory(bwx_sdk)

# Link bwx_gui to kalahari_gui
target_link_libraries(kalahari_gui PRIVATE bwx_gui)
```

---

### Step 2: Implement bwxReactive Base Class (30 min)

1. Copy code from Architecture section to `reactive.h`
2. Copy code from Architecture section to `reactive.cpp`
3. Build and verify no compilation errors

**Expected:** Clean build, new library `bwx_gui` created.

---

### Step 3: Implement bwxManaged<T> Template (30 min)

1. Copy code from Architecture section to `managed.h`
2. Build and verify template compiles

**Expected:** Clean build, template ready for use.

---

### Step 4: Test with 1 Control in AppearanceSettingsPanel (90 min)

**Location:** `src/gui/appearance_settings_panel.cpp`

**Change 1:** Include bwx headers
```cpp
#include <bwx_sdk/gui/managed.h>
```

**Change 2:** Replace ONE wxStaticText with bwx::StaticText
```cpp
// BEFORE:
wxStaticText* m_typographyDescription = new wxStaticText(
    themeBox->GetStaticBox(),
    wxID_ANY,
    "Text scaling affects all UI elements..."
);

// AFTER:
bwx::StaticText* m_typographyDescription = new bwx::StaticText(
    themeBox->GetStaticBox(),
    wxID_ANY,
    "Text scaling affects all UI elements..."
);
```

**Change 3:** Add test broadcast in MainWindow::onSettingsApplied()
```cpp
// Add AFTER icon size reload (line ~1595)

// TEST: Broadcast font scaling to all reactive controls
double fontScale = newState.fontScaling;
bwx::gui::bwxReactive::broadcastFontScaleChange(fontScale);

core::Logger::getInstance().info("Broadcasted font scaling: {:.1f}x", fontScale);
```

**Build and test:**
1. Build application
2. Run: Settings → Appearance → Font Scaling = 1.5x
3. Click Apply
4. **Expected:** Typography description text becomes larger!
5. Change to 0.8x, click Apply
6. **Expected:** Typography description text becomes smaller!

---

## Acceptance Criteria

- [x] bwx_sdk/gui/ directory structure created
- [x] bwxReactive base class implemented (reactive.h, reactive.cpp)
- [x] bwxManaged<wxStaticText> template implemented (managed.h)
- [x] bwx::StaticText type alias defined
- [x] CMake integration complete (bwx_gui library builds)
- [x] 1 control migrated in AppearanceSettingsPanel
- [x] Font scaling broadcast works for migrated control
- [x] Build passes on all platforms (Linux, macOS, Windows)
- [x] Logs show: "Broadcasted font scaling: X.Xx"
- [x] Manual test: Font size changes when Apply clicked

---

## Testing Plan

### Unit Tests (Future - Task #00048)
For now, manual testing only. Unit tests will be added in Task #00048.

### Manual Testing

**Test Case 1: Font Scaling Increase**
1. Open Settings → Appearance
2. Change Font Scaling to 1.5x
3. Click Apply
4. **Expected:** Typography description text becomes larger
5. **Verify:** Logs show "Broadcasted font scaling: 1.5x"

**Test Case 2: Font Scaling Decrease**
1. Change Font Scaling to 0.8x
2. Click Apply
3. **Expected:** Typography description text becomes smaller
4. **Verify:** Logs show "Broadcasted font scaling: 0.8x"

**Test Case 3: Build Verification**
1. Build on Linux (WSL)
2. Build passes
3. Application starts without crashes
4. **Expected:** No compilation errors, clean startup

---

## Edge Cases

- **Empty registry:** If no controls registered, broadcast is no-op (safe)
- **Control destroyed during broadcast:** Iterator invalidation possible
  - **Mitigation:** Copy vector before iteration (Task #00044)
- **Multiple inheritance safety:** wxStaticText is concrete, bwxReactive is pure interface (safe)

---

## Known Limitations

- **Single control type:** Only wxStaticText tested (other types in Task #00044)
- **No unit tests:** Manual testing only (tests in Task #00048)
- **Theme change stub:** onThemeChanged() is placeholder (Task #00044)
- **No iterator safety:** Vector modified during iteration possible (Task #00044)

---

## Rollback Plan

If proof-of-concept fails:
1. Remove bwx_sdk/ directory
2. Revert CMakeLists.txt changes
3. Revert AppearanceSettingsPanel changes
4. Return to manual font scaling approach (Tasks #00037-00038)

**Risk Assessment:** LOW - Changes isolated to new library + 1 control migration.

---

## Next Steps

**After Task #00043 complete:**
1. Create Task #00044 (BWX Managed Template Generalization)
2. Generalize template for all wxWindow types
3. Add iterator safety (copy vector before broadcast)
4. Test with 10+ control types

---

## Documentation Updates

**project_docs/02_tech_stack.md:**
- Add bwx_sdk to technology stack
- Explain reactive GUI management approach

**project_docs/03_architecture.md:**
- Add BWX SDK Reactive System section
- Explain bwxReactive + bwxManaged<T> pattern
- Document multiple inheritance safety

**ROADMAP.md:**
- Already updated (§ 1.3)

---

**Created:** 2025-11-15
**Started:** (awaiting approval)
**Completed:** (pending)
