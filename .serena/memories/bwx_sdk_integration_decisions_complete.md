# bwx_sdk Integration - Complete Decisions (2025-11-03)

## Integration Strategy: Selective Integration

**Decision:** Use ONLY selected bwx_sdk modules in Kalahari (not entire library)

### Modules Selected:
1. ✅ **bwx_core** - Core utilities
   - Mathematical functions (bwx_math.h/cpp)
   - Error handling utilities
   - Multithreading helpers (wxThread error descriptions)
   - Platform-independent utilities

2. ✅ **bwx_gl** - OpenGL helpers (conditional)
   - Only if 3D rendering needed in future
   - Currently not required for MVP

3. ❌ **bwx_oop** - EXCLUDED
   - Reason: Name collision with Kalahari's bwxOOP class
   - Would create duplicate definitions and linking errors

## Architecture: Clean Slate (Task #00018 - COMPLETED)

### Directory Structure:
```
external/bwx_sdk/
├── include/bwx_sdk/          # Headers (single source of truth)
│   ├── bwx_core/
│   │   ├── bwx_core.h
│   │   ├── bwx_math.h
│   │   └── bwx_oop.h        # (excluded from Kalahari)
│   └── bwx_gl/
│       └── bwx_gl.h
└── src/                      # Implementation files
    ├── bwx_core/
    │   ├── bwx_core.cpp
    │   └── bwx_math.cpp
    └── bwx_gl/
        └── bwx_gl.cpp
```

### Include Pattern:
- **OLD (local):** `#include "bwx_core.h"`
- **NEW (global):** `#include <bwx_sdk/bwx_core/bwx_core.h>`

### Build Integration:
- Git submodule at `external/bwx_sdk/`
- CMakeLists.txt includes bwx_sdk as subdirectory
- vcpkg manages wxWidgets dependency
- No manual header copying (removed `scripts/copy_headers.py`)

## Key Fixes Implemented:

### 1. Type Punning (bwx_math.cpp)
**Problem:** Unsafe C-style casts
```cpp
// OLD (unsafe):
long i;
i = *(long*)&y;
y = *(float*)&i;
```

**FIXED:**
```cpp
// NEW (type-safe, C++20):
int32_t i;
i = std::bit_cast<int32_t>(y);
y = std::bit_cast<float>(i);
```

### 2. Platform Compatibility
**Problem:** `sizeof(long)` differs:
- Windows: 4 bytes
- Linux/macOS: 8 bytes

**Solution:** Use `int32_t` (always 4 bytes on all platforms)

### 3. Duplicate Definitions
**Problem:** Inline functions in both .h and .cpp
**Solution:** Inline functions only in headers

### 4. Code Formatting
**Changed:** Tabs → 4 spaces (entire codebase)

## CI/CD Status:
- ✅ All platforms: Zero errors, zero warnings
- ✅ Linux: 3m 16s (with vcpkg binary cache)
- ✅ macOS: 3m 36s
- ✅ Windows: 9m 19s
- ✅ All 50 tests passing

## Related Tasks:
- **Task #00017:** bwx_sdk Selective Integration Plan (planning document)
- **Task #00018:** Clean Slate Architecture Refactoring (implementation - COMPLETED)

## Documentation:
- Serena memory: `bwx_sdk_architectural_decisions_2025-11-02.md`
- CHANGELOG.md: Complete entry with all commits and changes
- Git commits:
  - bwx_sdk: `d637490`, `8caf951`
  - Kalahari: `507e10b`, `4de1593`, `346969f`

## Future Considerations:
- **If collision occurs:** Rename bwx_sdk's bwxOOP to avoid conflict
- **If bwx_gl needed:** Add selective compilation flag
- **Performance:** Monitor build times, optimize if needed
