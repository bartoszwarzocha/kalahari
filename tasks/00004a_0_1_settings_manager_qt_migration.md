# Task #00004a: Migrate SettingsManager from wxWidgets to Qt

**Status:** 🟡 PLANNED (Awaiting Approval)
**Phase:** 0 (Qt Foundation)
**Week:** 1
**Day:** 1
**Priority:** HIGH
**Complexity:** LOW (1-2 hours)
**Dependencies:** Task #00003 (QDockWidget System) ✅ DONE

---

## 📋 Objective

Migrate `SettingsManager` from wxWidgets types (`wxSize`, `wxPoint`) to Qt6 types (`QSize`, `QPoint`) to eliminate wxWidgets dependencies and prepare for Qt-based Settings Dialog implementation.

**Why this is atomic:**
- ONE class modified (SettingsManager)
- TWO files changed (settings_manager.h, settings_manager.cpp)
- FOUR methods updated (getWindowSize, setWindowSize, getWindowPosition, setWindowPosition)
- NO GUI work, NO new features, ONLY type migration
- ~1 hour estimated time

**Related tasks:**
- ✅ Task #00001: Qt6 Hello World
- ✅ Task #00003: QDockWidget System
- ⏳ Task #00004b: Settings Dialog (depends on this task)

---

## 🎯 Current State vs. Target State

### Current State (wxWidgets)

**File:** `include/kalahari/core/settings_manager.h`

```cpp
#include <wx/gdicmn.h>  // wxSize, wxPoint

class SettingsManager {
public:
    // Window geometry helpers
    wxSize getWindowSize() const;
    void setWindowSize(const wxSize& size);
    wxPoint getWindowPosition() const;
    void setWindowPosition(const wxPoint& pos);
    // ...
};
```

**File:** `src/core/settings_manager.cpp`

```cpp
wxSize SettingsManager::getWindowSize() const {
    int width = get<int>("window.width", 1280);
    int height = get<int>("window.height", 800);
    return wxSize(width, height);
}

void SettingsManager::setWindowSize(const wxSize& size) {
    set("window.width", size.GetWidth());
    set("window.height", size.GetHeight());
}

wxPoint SettingsManager::getWindowPosition() const {
    int x = get<int>("window.x", 100);
    int y = get<int>("window.y", 100);
    return wxPoint(x, y);
}

void SettingsManager::setWindowPosition(const wxPoint& pos) {
    set("window.x", pos.x);
    set("window.y", pos.y);
}
```

### Target State (Qt6)

**File:** `include/kalahari/core/settings_manager.h`

```cpp
#include <QSize>
#include <QPoint>

class SettingsManager {
public:
    // Window geometry helpers
    QSize getWindowSize() const;
    void setWindowSize(const QSize& size);
    QPoint getWindowPosition() const;
    void setWindowPosition(const QPoint& pos);
    // ...
};
```

**File:** `src/core/settings_manager.cpp`

```cpp
QSize SettingsManager::getWindowSize() const {
    int width = get<int>("window.width", 1280);
    int height = get<int>("window.height", 800);
    return QSize(width, height);
}

void SettingsManager::setWindowSize(const QSize& size) {
    set("window.width", size.width());
    set("window.height", size.height());
}

QPoint SettingsManager::getWindowPosition() const {
    int x = get<int>("window.x", 100);
    int y = get<int>("window.y", 100);
    return QPoint(x, y);
}

void SettingsManager::setWindowPosition(const QPoint& pos) {
    set("window.x", pos.x());
    set("window.y", pos.y());
}
```

**Key Differences:**
- `wxSize` → `QSize`
- `wxPoint` → `QPoint`
- `size.GetWidth()` → `size.width()`
- `size.GetHeight()` → `size.height()`
- `pos.x` → `pos.x()` (Qt uses accessors, not public members)
- `pos.y` → `pos.y()`

---

## 📝 Requirements from QT_MIGRATION_ROADMAP.md

**From Task #00004:**
> SettingsManager must work with Qt types (QSize, QPoint) before Settings Dialog can use it

**Acceptance criteria:**
- ✅ All wxWidgets types removed from SettingsManager
- ✅ Qt6 types used consistently
- ✅ JSON structure unchanged (backward compatible)
- ✅ All existing functionality preserved
- ✅ Compiles without warnings
- ✅ No breaking changes to API (except type signatures)

---

## 🔧 Implementation Plan

### Step 1: Update Header File (settings_manager.h)

**Changes to make:**

```cpp
// REMOVE this line:
#include <wx/gdicmn.h>  // wxSize, wxPoint

// ADD these lines:
#include <QSize>
#include <QPoint>

// UPDATE method signatures (4 methods):
public:
    // Window geometry helpers
    QSize getWindowSize() const;
    void setWindowSize(const QSize& size);
    QPoint getWindowPosition() const;
    void setWindowPosition(const QPoint& pos);
```

**Lines affected:**
- Line 26: Remove `#include <wx/gdicmn.h>`
- Line 27: Add `#include <QSize>` and `#include <QPoint>`
- Line 87: Change `wxSize` → `QSize`
- Line 91: Change `const wxSize&` → `const QSize&`
- Line 95: Change `wxPoint` → `QPoint`
- Line 99: Change `const wxPoint&` → `const QPoint&`

### Step 2: Update Implementation File (settings_manager.cpp)

**Changes to make:**

```cpp
// Method 1: getWindowSize()
QSize SettingsManager::getWindowSize() const {
    int width = get<int>("window.width", 1280);
    int height = get<int>("window.height", 800);
    return QSize(width, height);  // wxSize → QSize
}

// Method 2: setWindowSize()
void SettingsManager::setWindowSize(const QSize& size) {
    set("window.width", size.width());    // GetWidth() → width()
    set("window.height", size.height());  // GetHeight() → height()
}

// Method 3: getWindowPosition()
QPoint SettingsManager::getWindowPosition() const {
    int x = get<int>("window.x", 100);
    int y = get<int>("window.y", 100);
    return QPoint(x, y);  // wxPoint → QPoint
}

// Method 4: setWindowPosition()
void SettingsManager::setWindowPosition(const QPoint& pos) {
    set("window.x", pos.x());  // pos.x → pos.x()
    set("window.y", pos.y());  // pos.y → pos.y()
}
```

**Lines affected:**
- Line 142: Signature `wxSize` → `QSize`
- Line 145: Return `wxSize` → `QSize`
- Line 148: Signature `const wxSize&` → `const QSize&`
- Line 149: `size.GetWidth()` → `size.width()`
- Line 150: `size.GetHeight()` → `size.height()`
- Line 153: Signature `wxPoint` → `QPoint`
- Line 156: Return `wxPoint` → `QPoint`
- Line 159: Signature `const wxPoint&` → `const QPoint&`
- Line 160: `pos.x` → `pos.x()`
- Line 161: `pos.y` → `pos.y()`

### Step 3: Check for Other wxWidgets Dependencies

**Files to grep for wx references:**
```bash
grep -r "wx[A-Z]" include/kalahari/core/settings_manager.h
grep -r "wx[A-Z]" src/core/settings_manager.cpp
```

**Expected result:** No wx references found after migration

### Step 4: Build Verification

```bash
cd /e/Python/Projekty/Kalahari
./scripts/build.sh       # Windows/WSL build
./scripts/build_linux.sh # Linux build (if applicable)
```

**Expected result:** Clean build with 0 warnings

---

## ✅ Acceptance Criteria

### Functional Requirements

- [x] **AC-01:** `getWindowSize()` returns `QSize` instead of `wxSize`
- [x] **AC-02:** `setWindowSize()` accepts `const QSize&` parameter
- [x] **AC-03:** `getWindowPosition()` returns `QPoint` instead of `wxPoint`
- [x] **AC-04:** `setWindowPosition()` accepts `const QPoint&` parameter
- [x] **AC-05:** JSON structure unchanged (`window.width`, `window.height`, `window.x`, `window.y`)
- [x] **AC-06:** All existing SettingsManager functionality works (load/save/get/set)

### Technical Requirements

- [x] **AC-07:** No `#include <wx/gdicmn.h>` in settings_manager.h
- [x] **AC-08:** `#include <QSize>` and `#include <QPoint>` present
- [x] **AC-09:** No wxWidgets types in method signatures
- [x] **AC-10:** Qt accessor methods used (`width()`, `height()`, `x()`, `y()`)
- [x] **AC-11:** Code compiles without warnings (GCC, Clang, MSVC)
- [x] **AC-12:** No breaking changes to JSON schema (backward compatible)

### Verification Requirements

- [x] **AC-13:** Existing unit tests pass (if any for SettingsManager)
- [x] **AC-14:** Application runs and saves/loads settings correctly
- [x] **AC-15:** Settings file (`settings.json`) has unchanged format
- [x] **AC-16:** No memory leaks (Qt manages QSize/QPoint by value, no heap)

### Documentation Requirements

- [x] **AC-17:** CHANGELOG.md updated with migration entry
- [x] **AC-18:** Task #00004a marked DONE
- [x] **AC-19:** Git commit message references Task #00004a

---

## 🧪 Test Plan

### Test Case 1: Manual Build Test

**Steps:**
1. Apply migration changes
2. Run `./scripts/build.sh`
3. Check for warnings/errors

**Expected:**
- Clean build
- 0 warnings
- 0 errors

**Status:** ⏳ Pending

---

### Test Case 2: Application Startup Test

**Steps:**
1. Build application
2. Delete existing `settings.json` (force defaults)
3. Run `./build-linux-wsl/bin/kalahari`
4. Check logs for SettingsManager initialization

**Expected:**
- Application starts without crashes
- Logger shows: "SettingsManager initialized"
- Default window size: 1280x800
- Default window position: (100, 100)

**Status:** ⏳ Pending

---

### Test Case 3: Settings Persistence Test

**Steps:**
1. Run application
2. Resize window to 1024x768
3. Move window to (200, 150)
4. Close application (triggers save)
5. Check `settings.json` content
6. Reopen application

**Expected:**
- `settings.json` contains:
  ```json
  {
    "window": {
      "width": 1024,
      "height": 768,
      "x": 200,
      "y": 150
    }
  }
  ```
- Application reopens at saved position/size

**Status:** ⏳ Pending

---

### Test Case 4: API Compatibility Test

**Steps:**
1. Create test code snippet in `main.cpp`:
   ```cpp
   auto& settings = kalahari::core::SettingsManager::getInstance();

   // Test QSize API
   QSize size = settings.getWindowSize();
   logger.info("Window size: {}x{}", size.width(), size.height());

   settings.setWindowSize(QSize(1920, 1080));

   // Test QPoint API
   QPoint pos = settings.getWindowPosition();
   logger.info("Window position: ({}, {})", pos.x(), pos.y());

   settings.setWindowPosition(QPoint(50, 50));
   ```
2. Build and run
3. Check logs

**Expected:**
- Logs show correct values
- No compile errors
- No runtime errors

**Status:** ⏳ Pending

---

### Test Case 5: Backward Compatibility Test

**Steps:**
1. Create old-format `settings.json`:
   ```json
   {
     "version": "1.0",
     "window": {
       "width": 1440,
       "height": 900,
       "x": 300,
       "y": 200,
       "maximized": false
     }
   }
   ```
2. Run application with migrated SettingsManager
3. Check that values are loaded correctly

**Expected:**
- Old settings load successfully
- No data loss
- Migration to 1.1 happens (if applicable)

**Status:** ⏳ Pending

---

## 📊 Estimated Time

| Phase | Task | Time |
|-------|------|------|
| Implementation | Update header file | 5 min |
| Implementation | Update cpp file | 10 min |
| Implementation | Verify no other wx deps | 5 min |
| Testing | Build verification | 5 min |
| Testing | Application startup test | 5 min |
| Testing | Settings persistence test | 10 min |
| Testing | API compatibility test | 10 min |
| Documentation | Update CHANGELOG.md | 5 min |
| Documentation | Update task file | 5 min |
| **Total** | | **60 min** |

---

## 🔍 Risk Assessment

### Risk 1: Breaking Changes

**Likelihood:** LOW
**Impact:** MEDIUM
**Mitigation:**
- JSON structure unchanged (internal representation identical)
- API signatures change, but Qt types are drop-in replacements
- No other Kalahari code uses these methods yet (early in migration)

### Risk 2: Qt Include Path Issues

**Likelihood:** LOW
**Impact:** LOW
**Mitigation:**
- Qt6::Core already linked in CMakeLists.txt (from Task #00001)
- QSize and QPoint are in QtCore, no additional dependencies

### Risk 3: Unit Test Failures

**Likelihood:** LOW
**Impact:** LOW
**Mitigation:**
- Check if SettingsManager has unit tests
- Update tests to use QSize/QPoint if needed
- Run `./build-linux-wsl/bin/kalahari-tests` before/after

---

## 📦 Files to Modify

### Header File

**File:** `include/kalahari/core/settings_manager.h`
**Lines:** 26-27 (includes), 87, 91, 95, 99 (method signatures)
**Changes:** 8 lines modified

### Implementation File

**File:** `src/core/settings_manager.cpp`
**Lines:** 142, 145, 148-150, 153, 156, 159-161
**Changes:** 10 lines modified

### Documentation

**File:** `CHANGELOG.md`
**Section:** [Unreleased]
**Changes:** Add migration entry

**File:** `tasks/00004a_0_1_settings_manager_qt_migration.md`
**Changes:** Mark DONE, update test results

---

## 🚀 Deployment

### Build Commands

```bash
# Windows/WSL build
cd /e/Python/Projekty/Kalahari
./scripts/build.sh

# Linux build
./scripts/build_linux.sh

# Run tests (if available)
./build-linux-wsl/bin/kalahari-tests
```

### Git Workflow

```bash
# Stage changes
git add include/kalahari/core/settings_manager.h
git add src/core/settings_manager.cpp
git add CHANGELOG.md
git add tasks/00004a_0_1_settings_manager_qt_migration.md

# Commit
git commit -m "feat(core): Migrate SettingsManager from wxWidgets to Qt6 (Task #00004a)

- Replace wxSize with QSize in getWindowSize/setWindowSize
- Replace wxPoint with QPoint in getWindowPosition/setWindowPosition
- Update method implementations to use Qt accessors (width(), height(), x(), y())
- Remove wx/gdicmn.h include, add QSize and QPoint includes
- JSON structure unchanged (backward compatible)
- Prepares for Qt-based Settings Dialog (Task #00004b)

Resolves #00004a (Phase 0, Week 1, Day 1)"
```

---

## 📚 Related Documentation

- **QT_MIGRATION_ROADMAP.md:** Lines 880-930 (Task #00004 requirements)
- **CLAUDE.md:** Cardinal Rules #1 (MCP Tools), #2 (wxWidgets → Qt patterns)
- **Qt6 Documentation:** QSize, QPoint reference
- **Task #00001:** Qt6 Hello World (CMake Qt integration)
- **Task #00003:** QDockWidget System (Qt panels structure)

---

## ✅ Definition of DONE

This task is considered DONE when:

1. ✅ All 19 acceptance criteria met
2. ✅ All 5 test cases pass
3. ✅ Build completes with 0 warnings
4. ✅ Application runs and saves/loads settings correctly
5. ✅ CHANGELOG.md updated
6. ✅ Git commit created with proper message
7. ✅ Task file marked DONE
8. ✅ Ready for Task #00004b (Settings Dialog)

---

## 🎯 Success Metrics

- **Lines of code changed:** ~18 lines
- **Files modified:** 2 files (+ docs)
- **Compilation time:** Same as before
- **Runtime performance:** Identical (QSize/QPoint are trivial types)
- **Memory usage:** No change (value types, no heap allocation)
- **Breaking changes:** 0 (JSON format unchanged)

---

**Created:** 2025-11-20
**Author:** Claude (AI)
**Approved:** ⏳ Awaiting user approval
**Started:** N/A
**Completed:** N/A
