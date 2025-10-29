# Task #00007: Fix Python Standard Library Detection (Cross-Platform)

## Context
- **Phase:** Phase 0 Week 2 (Threading & Python)
- **Roadmap Reference:** ROADMAP.md Phase 0 - Python embedding foundation
- **Related Docs:** project_docs/02_tech_stack.md (Python 3.11 Embedded)
- **Dependencies:** Task #00002 (Python embedding - completed but broken)
- **Severity:** CRITICAL - Plugins cannot load without working Python

## Problem
Python interpreter fails to initialize on both Windows and Linux:

**Windows:**
```
[error] Failed to initialize Python: Python standard library not found at: C:\Python312\Lib
```

**Linux (VirtualBox/WSL):**
```
[error] Failed to initialize Python: Python standard library not found at: /usr/Lib
```

**Root Cause:**
- Current code uses hardcoded path guessing with wrong case sensitivity
- Windows: searches `C:\Python312\Lib` (correct uppercase)
- Linux: searches `/usr/Lib` (WRONG - should be `/usr/lib/python3.12`)

**Impact:**
- ✅ C++ core works perfectly (GUI, tests, threading)
- ❌ Python plugins cannot load
- ❌ Plugin system non-functional

## Objective
Fix `PythonManager::initialize()` to correctly detect Python standard library on all platforms:
1. Windows: `C:\PythonXY\Lib` or bundled Python
2. Linux: `/usr/lib/pythonX.Y` (system Python)
3. macOS: `/usr/local/lib/pythonX.Y` or `/opt/homebrew/lib/pythonX.Y`

## Proposed Approach

### 1. Use Python's Own Detection
Instead of guessing paths, use Python's compiled-in paths:

```cpp
// Get stdlib path from Python itself (before Py_Initialize)
PyConfig config;
PyConfig_InitPythonConfig(&config);

// Set program name to help Python find its home
config.program_name = Py_DecodeLocale(argv[0], nullptr);

// Initialize with config
Py_InitializeFromConfig(&config);

// Now query actual stdlib path
py::module_ sys = py::module_::import("sys");
std::string stdlib_path = sys.attr("prefix").cast<std::string>();
```

### 2. Platform-Specific Fallbacks
If config fails, use platform-specific detection:

**Windows:**
```cpp
// 1. Check registry: HKEY_LOCAL_MACHINE\SOFTWARE\Python\PythonCore\3.X\InstallPath
// 2. Check environment: %PYTHONHOME%
// 3. Check bundled: executable_dir/python3X/
// 4. Check PATH: where python3.exe
```

**Linux:**
```cpp
// 1. Check environment: $PYTHONHOME
// 2. Query python3-config: python3-config --prefix
// 3. Check standard locations:
//    - /usr/lib/python3.12
//    - /usr/lib/python3.11
//    - /usr/local/lib/python3.X
// 4. Use pkg-config: pkg-config --variable=prefix python3
```

**macOS:**
```cpp
// 1. Check Homebrew: /opt/homebrew/lib/python3.X
// 2. Check system: /Library/Frameworks/Python.framework/Versions/3.X
// 3. Check environment: $PYTHONHOME
```

### 3. Detailed Logging
Add comprehensive logging to help diagnose issues:
```cpp
spdlog::debug("Python detection attempt 1: PyConfig");
spdlog::debug("Python detection attempt 2: Platform-specific");
spdlog::debug("Checking path: {}", candidate_path);
spdlog::info("Python stdlib found at: {}", final_path);
```

## Implementation Plan (Checklist)

### Phase 1: Research & Design
- [ ] Study pybind11 initialization best practices
- [ ] Research Python's `PyConfig` API (Python 3.8+)
- [ ] Review how other embedded Python projects solve this
- [ ] Document platform-specific Python stdlib locations

### Phase 2: Refactor PythonManager
- [ ] Extract stdlib detection to separate method: `detectPythonStdlib()`
- [ ] Implement PyConfig-based detection (primary method)
- [ ] Implement platform-specific fallbacks (Windows/Linux/macOS)
- [ ] Add comprehensive logging at each detection step
- [ ] Handle errors gracefully (don't crash if Python missing)

### Phase 3: Platform Testing
- [ ] Test on Windows 10/11 (system Python + bundled)
- [ ] Test on Linux/Ubuntu (system Python via apt)
- [ ] Test on WSL (system Python)
- [ ] Test on VirtualBox Linux VM (system Python)
- [ ] Document working configurations in BUILDING.md

### Phase 4: Error Handling
- [ ] Display user-friendly error if Python not found
- [ ] Suggest installation commands (apt/brew/winget)
- [ ] Allow application to continue without Python (plugins disabled)
- [ ] Add "Python Status" indicator in Help → About dialog

### Phase 5: Documentation
- [ ] Update BUILDING.md with Python requirements
- [ ] Update project_docs/02_tech_stack.md with Python stdlib detection
- [ ] Add troubleshooting section for Python initialization errors
- [ ] Document supported Python versions (3.11, 3.12)

## Risks & Open Questions

### Questions:
- Q: Should we bundle Python with the application (Phase 1) or require system Python?
  - **Decision needed:** Bundled = zero friction, System = smaller binary

- Q: What Python versions to support? 3.11 only? 3.11-3.13?
  - **Suggestion:** 3.11+ (pybind11 compatible, modern enough)

- Q: Should app start without Python? (plugins disabled but core works)
  - **Suggestion:** YES - graceful degradation, show warning in UI

### Risks:
- Risk: PyConfig API might be complex/fragile
  - **Mitigation:** Keep platform-specific fallbacks

- Risk: Different Linux distros install Python differently
  - **Mitigation:** Try multiple standard paths, log all attempts

- Risk: User might have multiple Python versions installed
  - **Mitigation:** Prefer system default (`python3`), allow override via env var

## Platform-Specific Implementation Details

### Windows Detection Strategy:
```cpp
std::string PythonManager::detectPythonStdlibWindows() {
    // 1. Try PyConfig (most reliable)
    std::string path = tryPyConfig();
    if (!path.empty()) return path;

    // 2. Check registry
    path = checkWindowsRegistry();
    if (!path.empty()) return path;

    // 3. Check environment
    if (const char* home = std::getenv("PYTHONHOME")) {
        return std::string(home) + "\\Lib";
    }

    // 4. Check bundled Python (future Phase 1 feature)
    path = getExecutableDir() + "\\python312\\Lib";
    if (fs::exists(path)) return path;

    // 5. Search PATH
    path = findPythonInPath();
    return path;
}
```

### Linux Detection Strategy:
```cpp
std::string PythonManager::detectPythonStdlibLinux() {
    // 1. Try PyConfig (most reliable)
    std::string path = tryPyConfig();
    if (!path.empty()) return path;

    // 2. Use python3-config
    path = runCommand("python3-config --prefix");
    if (!path.empty()) return path + "/lib/python" + PYTHON_VERSION;

    // 3. Check standard locations
    std::vector<std::string> candidates = {
        "/usr/lib/python3.12",
        "/usr/lib/python3.11",
        "/usr/local/lib/python3.12",
        "/usr/local/lib/python3.11"
    };
    for (const auto& candidate : candidates) {
        if (fs::exists(candidate)) return candidate;
    }

    // 4. Check PYTHONHOME
    if (const char* home = std::getenv("PYTHONHOME")) {
        return std::string(home) + "/lib/python" + PYTHON_VERSION;
    }

    return "";
}
```

## Verification Checklist

### Automated Tests:
- [ ] Unit test: `detectPythonStdlib()` returns valid path
- [ ] Unit test: Graceful degradation if Python missing
- [ ] Integration test: Initialize Python successfully
- [ ] Integration test: Load simple plugin (print "Hello")

### Manual Tests:
- [ ] Windows: Fresh install, system Python 3.12
- [ ] Windows: No Python installed (should show error, not crash)
- [ ] Linux: Ubuntu 22.04 with apt python3
- [ ] Linux: WSL Ubuntu with system Python
- [ ] VirtualBox: Linux VM with system Python

### Success Criteria:
- ✅ Python initializes successfully on all platforms
- ✅ Logs show correct stdlib path detection
- ✅ Simple plugin loads and executes
- ✅ Application doesn't crash if Python missing (shows warning instead)

## Status
- **Created:** 2025-10-27
- **Approved:** 2025-10-27 (by User)
- **Started:** 2025-10-27
- **Completed:** (pending)

## Implementation Notes

### Changes Made:

1. **Added `detectPythonStdlib()` private method** (python_interpreter.h:93-96)
   - Takes Python home path as parameter
   - Returns platform-specific stdlib path
   - Comprehensive documentation

2. **Implemented platform-specific stdlib detection** (python_interpreter.cpp:288-362)
   - **Windows**: `pythonHome / "Lib"` (uppercase, direct)
   - **Linux**: Tries `lib/python3.13`, `lib/python3.12`, `lib/python3.11`
     - Fallback: Iterates `/usr/lib/` for `python3.*` directories
   - **macOS**: Same as Linux, plus Framework fallback
   - Detailed logging at each attempt

3. **Refactored `initialize()` method** (python_interpreter.cpp:65-71)
   - Removed hardcoded `pythonHome / "Lib"` assumption
   - Now calls `detectPythonStdlib(pythonHome)`
   - Simplified logic - stdlib detection separated from home detection

### Build & Test Results:

**WSL Build:**
- ✅ Compiled in 6 seconds (incremental)
- ✅ All tests pass (2070 assertions in 12 test cases)
- ✅ Binaries in `build-linux-wsl/bin/`

**VirtualBox Build:**
- ⏳ Awaiting user verification with GUI test

### Key Decisions:

1. **Multi-version support**: Tries Python 3.13 → 3.11 for forward compatibility
2. **Fallback strategy**: Comprehensive - tries standard paths, then iterates lib directory
3. **Logging**: Debug logs for each attempt, info log for success
4. **Error messages**: Clear, actionable error if stdlib not found

### Verification Needed:

- [ ] VirtualBox GUI test (user will verify)
- [ ] Windows build test (user will verify)
- [ ] Confirm Python plugins can load

## Related Files
- `src/python/PythonManager.cpp` - Main file to modify
- `include/python/PythonManager.h` - May need new private methods
- `CMakeLists.txt` - Verify Python finding logic
- `BUILDING.md` - Document Python requirements
