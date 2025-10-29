# Task #00009: Plugin Manager + pybind11 Basic Bindings

## Context

- **Phase:** Phase 0 Week 3-4 (Python Embedding)
- **Roadmap Reference:** ROADMAP.md - Phase 0 Foundation (Plugin Architecture)
- **Related Docs:**
  - **[project_docs/04_plugin_system.md](../project_docs/04_plugin_system.md)** - Complete plugin architecture spec
  - **[project_docs/07_mvp_tasks.md](../project_docs/07_mvp_tasks.md)** - Week 3-4: Python Embedding
  - tasks/00005_python_embedding_pybind11.md (Python interpreter working)
- **Dependencies:**
  - Task #00005 (PythonInterpreter operational)
  - pybind11 already in vcpkg dependencies
  - Python 3.11 embedded and functional

## Objective

Create the foundation of Kalahari's plugin system by implementing:

1. **PluginManager** - Singleton managing plugin lifecycle (discover, load, unload)
2. **pybind11 Bindings** - Expose core C++ APIs to Python (Logger, EventBus stubs)
3. **C++ ↔ Python Communication** - Verify bidirectional function calls work
4. **Plugin Discovery** - Scan `plugins/` directory for `.kplugin` files (future)

**Key Goals:**

- Establish plugin architecture patterns (Singleton, Extension Points)
- Create `kalahari_api` Python module for plugins to import
- Prove pybind11 integration works (call C++ from Python, Python from C++)
- Lay groundwork for Phase 2 (4 MVP plugins)

## Architecture Overview

**IMPORTANT:** Full plugin system design is documented in:
**[project_docs/04_plugin_system.md](../project_docs/04_plugin_system.md)**

**Quick Summary:**

```
┌────────────────────────────────────────┐
│         Kalahari Core (C++)            │
│  ┌──────────────────────────────────┐  │
│  │      PluginManager (C++)         │  │
│  │  - discoverPlugins()             │  │
│  │  - loadPlugin()                  │  │
│  │  - unloadPlugin()                │  │
│  └──────────────────────────────────┘  │
│              ↕ pybind11                │
│  ┌──────────────────────────────────┐  │
│  │    kalahari_api (Python module)  │  │
│  │  - Logger.info/error/debug       │  │
│  │  - EventBus (stub for Week 7)    │  │
│  └──────────────────────────────────┘  │
│              ↕ import                  │
│  ┌──────────────────────────────────┐  │
│  │    Python Plugins (future)       │  │
│  │  import kalahari_api             │  │
│  └──────────────────────────────────┘  │
└────────────────────────────────────────┘
```

**Phase 0 Week 3-4 Scope:**
- PluginManager skeleton (no actual plugin loading yet)
- pybind11 module: `kalahari_api` with Logger bindings
- Test script: `tests/test_python_bindings.py`
- CMake integration: `src/bindings/CMakeLists.txt`

**Phase 0 Week 5-6 (Future):**
- Actual plugin loading from `.kplugin` files
- Extension Points (IExporter, IPanelProvider, IAssistant)
- Plugin manifest parsing (JSON)

## Implementation Checklist

### Phase 1: PluginManager Skeleton

- [ ] Create `include/kalahari/core/plugin_manager.h`
  - Singleton pattern (getInstance())
  - Empty methods: discoverPlugins(), loadPlugin(), unloadPlugin()
  - Vector of plugin metadata (for future use)
  - Mutex for thread safety
  - Doxygen comments
- [ ] Create `src/core/plugin_manager.cpp`
  - Implement singleton getInstance()
  - Stub methods with logging (spdlog)
  - Constructor/destructor
- [ ] Update `src/CMakeLists.txt`
  - Add plugin_manager.cpp to kalahari target
- [ ] Create `tests/core/test_plugin_manager.cpp`
  - Test singleton pattern
  - Test getInstance() returns same instance
  - Test thread safety (multiple threads calling getInstance())

### Phase 2: pybind11 Bindings Module

- [ ] Create `src/bindings/python_bindings.cpp`
  - `PYBIND11_MODULE(kalahari_api, m)` definition
  - Expose Logger::getInstance() to Python
  - Bindings for: info(), error(), debug(), warning()
  - Module docstring
- [ ] Create `src/bindings/CMakeLists.txt`
  - Find pybind11 (already in vcpkg)
  - Create Python module target: `kalahari_api`
  - Link against kalahari core
  - Set LIBRARY_OUTPUT_DIRECTORY for testing
- [ ] Update root `CMakeLists.txt`
  - Add `add_subdirectory(src/bindings)` if not present
- [ ] Build verification
  - Verify `kalahari_api.so` (Linux) / `kalahari_api.pyd` (Windows) is generated
  - Check module loads: `python3 -c "import kalahari_api; print(dir(kalahari_api))"`

### Phase 3: Python ↔ C++ Communication Tests

- [ ] Create `tests/test_python_bindings.py`
  - Import kalahari_api module
  - Call Logger.info("Hello from Python")
  - Verify log output appears
  - Test all log levels (info, error, debug, warning)
- [ ] Create C++ test: `tests/core/test_python_interop.cpp`
  - Initialize Python interpreter
  - Import kalahari_api
  - Call Python function from C++
  - Verify results
  - Clean up (Py_Finalize)
- [ ] Integration test in MainWindow
  - Add "Diagnostics → Test Python Bindings" menu item
  - Execute Python script that uses kalahari_api
  - Display results in wxMessageBox
  - Log everything for debugging

### Phase 4: Documentation & Examples

- [ ] Create `docs/plugin_api_reference.md`
  - Document kalahari_api module
  - Logger API reference
  - Code examples
  - Future Extension Points (stubs)
- [ ] Update CHANGELOG.md
  - Added: PluginManager skeleton
  - Added: pybind11 bindings (kalahari_api module)
  - Added: Python ↔ C++ communication tests
  - Impact: Foundation for plugin system
- [ ] Update Task #00009
  - Mark all checklist items complete
  - Add Implementation Notes
  - Set Completed date

### Phase 5: Verification & Testing

- [ ] **Build Tests:**
  - [ ] Linux: `./scripts/build_linux.sh` succeeds
  - [ ] Windows: `scripts\build_windows.bat` succeeds (if available)
  - [ ] No compilation warnings
- [ ] **Unit Tests:**
  - [ ] `ctest` passes all tests
  - [ ] PluginManager tests pass
  - [ ] Python interop tests pass
- [ ] **Manual Tests:**
  - [ ] Python script imports kalahari_api successfully
  - [ ] Logger.info("test") appears in logs
  - [ ] "Diagnostics → Test Python Bindings" menu works
  - [ ] No crashes or memory leaks (valgrind if available)
- [ ] **Code Quality:**
  - [ ] Doxygen comments complete
  - [ ] No TODO/FIXME left behind
  - [ ] Code follows C++20 conventions
  - [ ] Thread-safe (mutex usage correct)

## Proposed Code Examples

### PluginManager Header

```cpp
/// @file plugin_manager.h
/// @brief Singleton managing plugin lifecycle

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <filesystem>

namespace kalahari {
namespace core {

/// @brief Metadata for a discovered plugin (future use)
struct PluginMetadata {
    std::string id;
    std::string name;
    std::string version;
    std::filesystem::path path;
};

/// @brief Singleton manager for plugins
class PluginManager {
public:
    static PluginManager& getInstance();

    // Prevent copying/moving
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /// @brief Discover plugins in plugins/ directory
    /// @return Number of plugins discovered
    size_t discoverPlugins();

    /// @brief Load a plugin by ID
    /// @param pluginId Unique plugin identifier
    /// @return true if loaded successfully
    bool loadPlugin(const std::string& pluginId);

    /// @brief Unload a plugin
    /// @param pluginId Plugin to unload
    void unloadPlugin(const std::string& pluginId);

    /// @brief Get list of discovered plugins
    std::vector<PluginMetadata> getDiscoveredPlugins() const;

private:
    PluginManager() = default;
    ~PluginManager() = default;

    std::vector<PluginMetadata> m_plugins;
    mutable std::mutex m_mutex;
};

} // namespace core
} // namespace kalahari
```

### pybind11 Bindings

```cpp
/// @file python_bindings.cpp
/// @brief pybind11 bindings for Kalahari core API

#include <pybind11/pybind11.h>
#include <kalahari/core/logger.h>

namespace py = pybind11;

PYBIND11_MODULE(kalahari_api, m) {
    m.doc() = "Kalahari Core API for Python plugins";

    // Logger class (static methods)
    py::class_<kalahari::core::Logger>(m, "Logger")
        .def_static("info", [](const std::string& msg) {
            kalahari::core::Logger::getInstance().info(msg);
        }, "Log info message")
        .def_static("error", [](const std::string& msg) {
            kalahari::core::Logger::getInstance().error(msg);
        }, "Log error message")
        .def_static("debug", [](const std::string& msg) {
            kalahari::core::Logger::getInstance().debug(msg);
        }, "Log debug message")
        .def_static("warning", [](const std::string& msg) {
            kalahari::core::Logger::getInstance().warning(msg);
        }, "Log warning message");
}
```

### Python Test Script

```python
#!/usr/bin/env python3
"""
Test script for kalahari_api Python bindings
Run from build directory: python3 ../tests/test_python_bindings.py
"""

import sys
import os

# Add build directory to Python path for kalahari_api module
sys.path.insert(0, os.path.dirname(__file__))

try:
    import kalahari_api
    print("✅ kalahari_api module imported successfully")

    # Test Logger bindings
    kalahari_api.Logger.info("Hello from Python! 🐍")
    kalahari_api.Logger.debug("This is a debug message")
    kalahari_api.Logger.warning("This is a warning")
    kalahari_api.Logger.error("This is an error (test)")

    print("✅ All Logger methods work correctly")
    print(f"✅ Module dir: {dir(kalahari_api)}")

except ImportError as e:
    print(f"❌ Failed to import kalahari_api: {e}")
    sys.exit(1)
except Exception as e:
    print(f"❌ Runtime error: {e}")
    sys.exit(1)

print("\n✅ All tests passed!")
```

## Risks & Open Questions

**Risks:**

- **R1:** pybind11 module may not load if PYTHONPATH incorrect
  - Mitigation: CMake sets LIBRARY_OUTPUT_DIRECTORY correctly
- **R2:** GIL (Global Interpreter Lock) handling needed for threading
  - Mitigation: Document GIL patterns, defer to Week 7 (EventBus)
- **R3:** Windows DLL export/import may cause linker errors
  - Mitigation: Test on Windows early, use proper CMake target properties

**Open Questions:**

- **Q1:** Should PluginManager be initialized at app start or lazy?
  - Answer: Lazy (first getInstance() call), matches Logger pattern
- **Q2:** Where to place kalahari_api.so for runtime loading?
  - Answer: Copy to build/bin/ directory via CMake (POST_BUILD)
- **Q3:** Do we need Python virtual environment for development?
  - Answer: No - using embedded Python 3.11 from vcpkg

## Acceptance Criteria

✅ **PluginManager:**
- Singleton pattern works
- Thread-safe (tested with multiple threads)
- Methods log activity correctly
- Unit tests pass

✅ **pybind11 Bindings:**
- `kalahari_api` module builds successfully
- Module imports in Python without errors
- Logger.info/error/debug/warning all work
- Python script can call C++ Logger

✅ **C++ ↔ Python:**
- C++ can execute Python code
- Python can call C++ functions
- No memory leaks (verified with valgrind if available)
- No crashes

✅ **Build System:**
- Linux build succeeds
- Windows build succeeds (if tested)
- CMake configuration correct
- Module output path correct

✅ **Tests:**
- `ctest` passes all tests
- `test_python_bindings.py` passes
- Manual test via Diagnostics menu works

## Status
- **Created:** 2025-10-29
- **Approved:** [Awaiting user approval]
- **Started:**
- **Completed:**

## Implementation Notes

(To be filled during implementation)

## Related Files

**New files:**
- `include/kalahari/core/plugin_manager.h`
- `src/core/plugin_manager.cpp`
- `src/bindings/python_bindings.cpp`
- `src/bindings/CMakeLists.txt`
- `tests/core/test_plugin_manager.cpp`
- `tests/core/test_python_interop.cpp`
- `tests/test_python_bindings.py`
- `docs/plugin_api_reference.md`

**Modified files:**
- `src/CMakeLists.txt` (add plugin_manager.cpp)
- `CMakeLists.txt` (add src/bindings if needed)
- `src/gui/main_window.cpp` (add "Test Python Bindings" menu item)
- `CHANGELOG.md`

---

**Task Priority:** 🟡 MEDIUM (Foundation for plugin system, but not blocking)

**Estimated Effort:** 12-16 hours (Week 3-4)
- PluginManager skeleton: 3h
- pybind11 bindings: 4h
- Tests (C++ + Python): 3h
- Integration + debugging: 2-3h
- Documentation: 2-3h

---

**References:**

- pybind11 docs: https://pybind11.readthedocs.io/
- project_docs/04_plugin_system.md - Complete architecture
- project_docs/07_mvp_tasks.md - Week 3-4 plan
