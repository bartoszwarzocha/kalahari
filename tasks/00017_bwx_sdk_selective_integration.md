# Task #00017: bwx_sdk Selective Integration (Cherry-Picking Approach)

**Status:** 📋 Planned
**Priority:** Medium
**Estimated Time:** 4-6 hours
**Assignee:** TBD
**Created:** 2025-11-02
**Phase:** Phase 0 (Foundation) - Week 4

---

## 📝 Summary

Integrate **selected utilities** from bwx_sdk library into Kalahari using **cherry-picking approach** instead of full git submodule integration. After critical quality analysis, we determined that only **2 components** from bwx_sdk provide real value without introducing unnecessary complexity.

**Key Decision:** Do NOT add entire bwx_sdk as dependency (includes unused OpenGL code, redundant utilities, outdated patterns). Extract only genuinely useful components.

---

## 🎯 Objectives

### Primary Goals
1. ✅ Extract `bwxToISO8601()` function for timestamp formatting in BookElement
2. ✅ Extract `_L()` macro for internationalization (wxGetTranslation wrapper)
3. ✅ Create clean utility headers in Kalahari's codebase

### Non-Goals (Explicitly Rejected)
- ❌ Full bwx_sdk integration via git submodule
- ❌ Using bwx_globals macro system (_member, _vector, _FI, etc.)
- ❌ Replacing CmdLineParser (current implementation is sufficient)
- ❌ Replacing SettingsManager (JSON is better than INI)
- ❌ Using bwx_string utilities (trivial wrappers around wxString)
- ❌ Using bwx_json (conflicts with nlohmann_json)
- ❌ Using bwxBoxSizer (30 lines savings not worth new API learning curve)

---

## 🔍 Background

### Analysis Performed

We analyzed bwx_sdk library (user's existing wxWidgets extension library) to identify refactoring opportunities and potential code reuse between bwx_sdk and Kalahari.

**Key Findings:**
1. **bwx_sdk contains 13 modules** (bwx_core, bwx_gui, bwx_gl, bwx_utils)
2. **Most components are redundant** - functionality already exists in wxWidgets or Kalahari
3. **Only 2 components provide real value:**
   - `bwxToISO8601()` - wxDateTime doesn't have built-in ISO 8601 formatting
   - `_L()` macro - cleaner than `wxGetTranslation(s)` for i18n

**Quality Assessment:**

| Component | Quality | Utility | Decision |
|-----------|---------|---------|----------|
| bwx_globals macros | ❌ Anti-pattern | ❌ Outdated | Reject |
| bwxCmdLineParser | ⚠️ Over-engineered | ⚠️ Unnecessary | Reject |
| bwxConfigUtils | ❌ INI format | ❌ Inferior to JSON | Reject |
| bwx_string | ⚠️ Trivial wrappers | ❌ wxString has all | Reject |
| bwx_datetime::ToISO8601 | ✅ Good | ✅ Needed | **Accept** |
| _L() macro | ✅ Good | ✅ i18n standard | **Accept** |
| bwx_json | ❌ Custom impl | ❌ Conflicts nlohmann | Reject |
| bwxBoxSizer | ✅ Good | ⚠️ 30 lines saved | Defer |
| bwxInternat | ✅ Good | ⏸️ Phase 2 feature | Defer |

---

## 📋 Implementation Plan

### Phase 1: Extract datetime utilities (1 hour)

**Step 1.1: Create datetime_utils.h**

Create `include/kalahari/core/utils/datetime_utils.h`:

```cpp
/// @file datetime_utils.h
/// @brief DateTime utility functions for Kalahari
///
/// Selected utilities extracted from bwx_sdk library.
/// Original source: https://github.com/bartoszwarzocha/bwx_sdk
/// License: wxWidgets License

#pragma once

#include <wx/datetime.h>
#include <string>

namespace kalahari {
namespace core {
namespace utils {

/// @brief Convert wxDateTime to ISO 8601 string format
/// @param date wxDateTime to convert
/// @return ISO 8601 formatted string (e.g., "2025-11-02T14:30:00Z")
///
/// Extracted from bwx_sdk::dt::bwxToISO8601()
/// Used for timestamps in BookElement metadata
std::string toISO8601(const wxDateTime& date);

/// @brief Convert ISO 8601 string to wxDateTime
/// @param iso8601 ISO 8601 formatted string
/// @return wxDateTime object, or invalid wxDateTime if parsing fails
wxDateTime fromISO8601(const std::string& iso8601);

} // namespace utils
} // namespace core
} // namespace kalahari
```

**Step 1.2: Create datetime_utils.cpp**

Create `src/core/utils/datetime_utils.cpp`:

```cpp
/// @file datetime_utils.cpp
/// @brief Implementation of datetime utilities

#include <kalahari/core/utils/datetime_utils.h>
#include <sstream>
#include <iomanip>

namespace kalahari {
namespace core {
namespace utils {

std::string toISO8601(const wxDateTime& date) {
    if (!date.IsValid()) {
        return "";
    }

    // Format: YYYY-MM-DDTHH:MM:SSZ (UTC)
    // Example: 2025-11-02T14:30:00Z

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << date.GetYear()
        << "-"
        << std::setw(2) << (date.GetMonth() + 1)  // wxDateTime::Month is 0-based
        << "-"
        << std::setw(2) << date.GetDay()
        << "T"
        << std::setw(2) << date.GetHour()
        << ":"
        << std::setw(2) << date.GetMinute()
        << ":"
        << std::setw(2) << date.GetSecond()
        << "Z";

    return oss.str();
}

wxDateTime fromISO8601(const std::string& iso8601) {
    wxDateTime date;

    // Parse ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ
    // wxDateTime::ParseISOCombined() handles "YYYY-MM-DDTHH:MM:SS"
    if (date.ParseISOCombined(iso8601.c_str())) {
        return date;
    }

    // Return invalid date if parsing fails
    return wxDateTime();
}

} // namespace utils
} // namespace core
} // namespace kalahari
```

**Step 1.3: Update CMakeLists.txt**

Add to `src/CMakeLists.txt`:

```cmake
# Core library source files
set(KALAHARI_CORE_SOURCES
    core/logger.cpp
    core/settings_manager.cpp
    # ... existing files ...
    core/utils/datetime_utils.cpp  # <-- ADD THIS
)
```

Create `src/core/utils/` directory:

```bash
mkdir -p src/core/utils
mkdir -p include/kalahari/core/utils
```

---

### Phase 2: Extract i18n macros (30 minutes)

**Step 2.1: Create i18n_macros.h**

Create `include/kalahari/core/utils/i18n_macros.h`:

```cpp
/// @file i18n_macros.h
/// @brief Internationalization macros for Kalahari
///
/// Macros for wxWidgets i18n/l10n support (wxLocale + gettext).
/// Extracted from bwx_sdk library.

#pragma once

#include <wx/intl.h>

/// @brief Translation macro (wrapper for wxGetTranslation)
/// @param s String to translate
/// @return Translated wxString
///
/// Usage:
/// @code
/// wxString title = _L("File");  // Returns translated string based on current locale
/// @endcode
///
/// Extracted from bwx_sdk::_L() macro
#define _L(s) wxGetTranslation(s)
```

**Step 2.2: Update code to use _L() macro**

No immediate changes needed - this is for future i18n implementation (Phase 2).
Document for later use in:
- Menu labels
- Dialog titles
- Status messages

---

### Phase 3: Update BookElement to use toISO8601() (1 hour)

**Step 3.1: Find current timestamp usage**

Search for timestamp formatting in BookElement:

```bash
grep -r "FormatISO\|Format(" include/kalahari/core/book_element.h
grep -r "FormatISO\|Format(" src/core/book_element.cpp
```

**Step 3.2: Replace with toISO8601()**

In `book_element.cpp`, replace manual formatting with:

```cpp
#include <kalahari/core/utils/datetime_utils.h>

// Before:
std::string timestamp = m_modifiedDate.Format("%Y-%m-%dT%H:%M:%SZ").ToStdString();

// After:
std::string timestamp = kalahari::core::utils::toISO8601(m_modifiedDate);
```

---

### Phase 4: Add unit tests (1.5 hours)

**Step 4.1: Create test file**

Create `tests/core/utils/test_datetime_utils.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/utils/datetime_utils.h>

using namespace kalahari::core::utils;

TEST_CASE("toISO8601 formats wxDateTime correctly", "[datetime_utils]") {
    wxDateTime date(2, wxDateTime::Nov, 2025, 14, 30, 45);

    std::string iso = toISO8601(date);

    REQUIRE(iso == "2025-11-02T14:30:45Z");
}

TEST_CASE("toISO8601 handles invalid date", "[datetime_utils]") {
    wxDateTime invalid;

    std::string iso = toISO8601(invalid);

    REQUIRE(iso.empty());
}

TEST_CASE("fromISO8601 parses valid ISO string", "[datetime_utils]") {
    std::string iso = "2025-11-02T14:30:45Z";

    wxDateTime date = fromISO8601(iso);

    REQUIRE(date.IsValid());
    REQUIRE(date.GetYear() == 2025);
    REQUIRE(date.GetMonth() == wxDateTime::Nov);
    REQUIRE(date.GetDay() == 2);
    REQUIRE(date.GetHour() == 14);
    REQUIRE(date.GetMinute() == 30);
    REQUIRE(date.GetSecond() == 45);
}

TEST_CASE("fromISO8601 handles invalid string", "[datetime_utils]") {
    std::string invalid = "not-a-date";

    wxDateTime date = fromISO8601(invalid);

    REQUIRE_FALSE(date.IsValid());
}

TEST_CASE("Round-trip conversion (to/from ISO8601)", "[datetime_utils]") {
    wxDateTime original(15, wxDateTime::Mar, 2024, 9, 15, 30);

    std::string iso = toISO8601(original);
    wxDateTime restored = fromISO8601(iso);

    REQUIRE(restored.IsEqualTo(original));
}
```

**Step 4.2: Update tests CMakeLists.txt**

Add to `tests/CMakeLists.txt`:

```cmake
set(TEST_SOURCES
    # ... existing tests ...
    core/utils/test_datetime_utils.cpp  # <-- ADD THIS
)
```

**Step 4.3: Run tests**

```bash
./scripts/build_linux.sh
./build-linux/bin/kalahari-tests "[datetime_utils]"
```

---

### Phase 5: Documentation and cleanup (1 hour)

**Step 5.1: Update CHANGELOG.md**

```markdown
## [Unreleased]

### Added
- datetime utilities (toISO8601, fromISO8601) extracted from bwx_sdk
- i18n macros (_L) for future internationalization support

### Changed
- BookElement now uses toISO8601() for timestamp formatting

### Technical
- Created core/utils module for shared utility functions
```

**Step 5.2: Document decision in Serena memory**

Create memory file documenting architectural decisions (see separate memory document).

**Step 5.3: Verify build on all platforms**

```bash
# Linux (WSL)
./scripts/build_linux.sh

# macOS (GitHub Actions)
git push origin main  # Trigger CI/CD

# Windows (GitHub Actions)
# Check CI/CD results
```

---

## 🧪 Testing Checklist

### Unit Tests
- [ ] `toISO8601()` formats date correctly
- [ ] `toISO8601()` handles invalid date
- [ ] `fromISO8601()` parses valid string
- [ ] `fromISO8601()` handles invalid string
- [ ] Round-trip conversion works (to/from)

### Integration Tests
- [ ] BookElement timestamps use new format
- [ ] Document save/load preserves timestamps
- [ ] All existing tests still pass

### CI/CD
- [ ] Linux build passes
- [ ] macOS build passes
- [ ] Windows build passes

---

## ✅ Acceptance Criteria

1. ✅ `datetime_utils.h/cpp` created with toISO8601() and fromISO8601()
2. ✅ `i18n_macros.h` created with _L() macro
3. ✅ BookElement uses toISO8601() for timestamps
4. ✅ All unit tests pass (5 tests for datetime_utils)
5. ✅ CI/CD passes on all platforms (Linux, macOS, Windows)
6. ✅ CHANGELOG.md updated
7. ✅ Architectural decision documented in Serena memory

---

## 📊 Estimated Impact

**Lines of Code:**
- **Added:** ~120 lines (datetime_utils.h/cpp + tests)
- **Removed:** ~20 lines (manual timestamp formatting in BookElement)
- **Net:** +100 lines

**Quality Improvements:**
- ✅ Consistent ISO 8601 formatting across codebase
- ✅ Reusable utility functions (DRY principle)
- ✅ Better test coverage for datetime operations
- ✅ Prepared for future i18n implementation (_L macro)

**Avoided Complexity:**
- ❌ Did NOT add entire bwx_sdk library (~10,000+ lines, including OpenGL)
- ❌ Did NOT introduce outdated macro patterns (_member, _vector, etc.)
- ❌ Did NOT replace working implementations (CmdLineParser, SettingsManager)

---

## 🚫 What We Explicitly REJECTED

### Components Analyzed but NOT Used

1. **bwxCmdLineParser** - Current CmdLineParser is sufficient (only `--diag` switch needed)
2. **bwxConfigUtils** - SettingsManager (JSON) is superior to INI format
3. **bwx_string utilities** - All functions are trivial wrappers around wxString API
4. **bwx_datetime (other functions)** - wxDateTime already has equivalent methods
5. **bwx_json** - Would conflict with nlohmann_json (standard in Kalahari)
6. **bwxBoxSizer** - Saves ~30 lines but requires learning new API (not worth it)
7. **bwx_globals macros** - Anti-patterns in modern C++20 code
8. **bwx_utils (colors)** - Not relevant to book editor domain

### Rationale

**Quality over quantity** - Adding entire bwx_sdk would introduce:
- Unused code (OpenGL modules, color utilities, etc.)
- Maintenance burden (syncing updates, compatibility)
- Outdated patterns (macro-heavy code, INI configs)
- Learning curve (bwxBoxSizer API, macro system)

**Verdict:** Extract only genuinely useful components via cherry-picking.

---

## 🔮 Future Considerations

### Phase 2: Internationalization
- Revisit `bwxInternat` when implementing i18n system
- Decision deferred to Phase 2 i18n task

### bwxBoxSizer Reconsideration
- If GUI layout code becomes unwieldy (Phase 1 panels)
- If team prefers semantic API (Add1Expand vs Add with flags)
- Decision: Evaluate after implementing 6+ panels in Phase 1

---

## 📚 References

- [bwx_sdk repository](https://github.com/bartoszwarzocha/bwx_sdk)
- [project_docs/09_i18n.md](../project_docs/09_i18n.md) - Internationalization plan
- [ISO 8601 standard](https://en.wikipedia.org/wiki/ISO_8601)
- Serena memory: `bwx_sdk_architectural_decisions` (created in this task)

---

## 📝 Notes

- This task uses **cherry-picking approach** instead of git submodule
- Only 2 components extracted (toISO8601, _L macro)
- Full quality analysis performed - see Serena memory for details
- Decision approved after critical evaluation of 13 bwx_sdk modules

---

**Task created:** 2025-11-02
**Last updated:** 2025-11-02
**Next review:** After Phase 1 completion (re-evaluate bwxBoxSizer)
