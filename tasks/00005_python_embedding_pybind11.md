# Task #00005: Python Embedding & pybind11 Integration

## Context
- **Phase:** Phase 0 Week 3-4
- **Roadmap Reference:** ROADMAP.md Phase 0 "Plugin Architecture"
- **Related Docs:**
  - [02_tech_stack.md](../project_docs/02_tech_stack.md) - Python 3.11 Embedded, pybind11
  - [04_plugin_system.md](../project_docs/04_plugin_system.md) - Plugin architecture overview
  - [03_architecture.md](../project_docs/03_architecture.md) - Threading, dependency management
- **Dependencies:**
  - Task #00001 (GUI window) - COMPLETED ✅
  - Task #00002 (Threading) - COMPLETED ✅
  - Task #00003 (Settings System) - COMPLETED ✅
  - Python 3.11 (will be bundled with application)
  - pybind11 library (already in project)

## Objective

Integrate **Python 3.11 as an embedded interpreter** into Kalahari, enabling the plugin system to execute Python code from C++.

This enables:
- Plugin system foundation (plugins written in Python)
- Python-based importers/exporters (python-docx, reportlab, etc.)
- AI/NLP integrations (OpenAI API, spaCy, etc.)
- Custom user scripts
- Future plugin marketplace

**Key Requirements:**
1. Python 3.11 embedded (bundled with application, no user installation needed)
2. pybind11 integration (bidirectional C++/Python interop)
3. Python GIL handling (thread-safe execution)
4. Error handling (Python exceptions → C++ exceptions)
5. Resource management (proper Python interpreter lifecycle)

## Proposed Approach

### 1. Python Embedding Architecture

**Embedded Python 3.11:**
- Bundle Python 3.11 runtime with application
- No dependency on system Python
- Cross-platform packaging (Windows .exe, macOS .app, Linux AppImage/Snap)
- Python environment isolated per-application

**Initialization Pattern:**
```cpp
namespace kalahari::core {
    class PythonInterpreter {
    public:
        static PythonInterpreter& getInstance();  // Singleton

        void initialize();  // Start Python interpreter
        void finalize();    // Stop Python interpreter

        bool isInitialized() const;
        std::string getPythonVersion() const;
        std::filesystem::path getPythonHome() const;

    private:
        PythonInterpreter();
        ~PythonInterpreter();

        bool m_initialized = false;
    };
}
```

**Lifecycle:**
- Initialize in `main()` (before wxWidgets)
- Finalize in `main()` cleanup (after wxWidgets)
- Available throughout application lifetime

### 2. pybind11 Integration

**What is pybind11:**
- Header-only C++11/14/17 library
- Seamless C++/Python interop
- Automatic type conversions (std::string ↔ str, std::vector ↔ list)
- Exception translation (Python exceptions → C++ exceptions)
- Python GIL management (RAII wrappers)

**Basic Usage Example:**
```cpp
#include <pybind11/embed.h>  // Everything needed for embedding
namespace py = pybind11;

// Execute Python code
try {
    py::gil_scoped_acquire acquire;  // Acquire Python GIL

    py::exec(R"(
        import sys
        print(f"Python version: {sys.version}")

        def greet(name):
            return f"Hello, {name}!"
    )");

    // Call Python function from C++
    py::object result = py::eval("greet('Kalahari')");
    std::string greeting = result.cast<std::string>();

    Logger::info("Python says: {}", greeting);

} catch (const py::error_already_set& e) {
    Logger::error("Python error: {}", e.what());
}
```

**Exposing C++ to Python:**
```cpp
// Expose Kalahari API to Python plugins
PYBIND11_EMBEDDED_MODULE(kalahari_api, m) {
    m.doc() = "Kalahari Plugin API";

    // Expose Logger to Python
    py::class_<Logger>(m, "Logger")
        .def_static("info", &Logger::info)
        .def_static("warn", &Logger::warn)
        .def_static("error", &Logger::error);

    // Expose Document to Python (future Phase 1)
    py::class_<Document>(m, "Document")
        .def("get_title", &Document::getTitle)
        .def("set_title", &Document::setTitle);
}
```

### 3. Threading & GIL Management

**Problem:** Python Global Interpreter Lock (GIL) prevents true parallelism

**Solution:** GIL management with pybind11 RAII wrappers

**Patterns:**

**Pattern 1: Acquire GIL (before calling Python):**
```cpp
void executePluginCode(const std::string& code) {
    py::gil_scoped_acquire acquire;  // Acquire GIL (RAII)
    py::exec(code);
    // GIL released automatically when `acquire` goes out of scope
}
```

**Pattern 2: Release GIL (before long C++ operations):**
```cpp
py::gil_scoped_release release;  // Release GIL while doing C++ work
// Long-running C++ operation (file I/O, network, etc.)
std::this_thread::sleep_for(std::chrono::seconds(5));
// GIL reacquired automatically when `release` goes out of scope
```

**Pattern 3: Background thread with Python:**
```cpp
void MainWindow::executePluginInBackground(const std::string& pythonCode) {
    submitBackgroundTask([pythonCode]() {
        // This runs in background thread (NOT GUI thread)

        py::gil_scoped_acquire acquire;  // Must acquire GIL!

        try {
            py::exec(pythonCode);
        } catch (const py::error_already_set& e) {
            Logger::error("Plugin error: {}", e.what());
        }

        // GIL released automatically
    });
}
```

### 4. Python Environment Setup

**Bundled Python Location:**
- **Windows:** `Kalahari.exe` directory → `python311/` subfolder
- **Linux:** `/opt/kalahari/python311/` or `~/.local/share/kalahari/python311/`
- **macOS:** `Kalahari.app/Contents/Resources/python311/`

**Python Path Configuration:**
```cpp
void PythonInterpreter::initialize() {
    // Set PYTHONHOME to bundled Python
    std::filesystem::path pythonHome = getPythonHome();
    Py_SetPythonHome(pythonHome.wstring().c_str());  // Windows: wstring

    // Initialize Python interpreter
    Py_Initialize();

    // Add plugin directories to sys.path
    py::module_ sys = py::module_::import("sys");
    sys.attr("path").attr("append")(getPluginDirectory().string());

    m_initialized = true;
    Logger::info("Python {} initialized", getPythonVersion());
}
```

### 5. Error Handling

**Python Exceptions → C++ Exceptions:**
```cpp
try {
    py::gil_scoped_acquire acquire;
    py::exec("raise ValueError('Something went wrong')");

} catch (const py::error_already_set& e) {
    // pybind11 exception with Python traceback
    Logger::error("Python error: {}", e.what());

    // Show to user
    wxMessageBox(
        wxString::FromUTF8(e.what()),
        "Plugin Error",
        wxOK | wxICON_ERROR
    );
}
```

**C++ Exceptions → Python Exceptions:**
```cpp
PYBIND11_EMBEDDED_MODULE(kalahari_api, m) {
    // Automatically translates std::exception to Python RuntimeError
    m.def("risky_operation", []() {
        throw std::runtime_error("C++ error");
    });
}
```

### 6. Resource Management

**Singleton Lifecycle:**
```cpp
PythonInterpreter::PythonInterpreter() {
    // Don't initialize here (too early in startup)
}

PythonInterpreter::~PythonInterpreter() {
    if (m_initialized) {
        Py_Finalize();
        m_initialized = false;
        Logger::info("Python interpreter finalized");
    }
}
```

**Explicit Control in main():**
```cpp
int main(int argc, char** argv) {
    // 1. Initialize logging first
    Logger::getInstance().info("Kalahari starting...");

    // 2. Initialize Python
    auto& python = PythonInterpreter::getInstance();
    python.initialize();

    // 3. Initialize wxWidgets
    wxApp::SetInstance(new KalahariApp());
    int result = wxEntry(argc, argv);

    // 4. Cleanup Python (after wxWidgets)
    python.finalize();

    Logger::getInstance().info("Kalahari exited");
    return result;
}
```

## Implementation Plan (Checklist)

### Phase 1: Basic Python Embedding (4-6 hours)
- [ ] Add Python 3.11 to vcpkg.json manifest
- [ ] Create `include/kalahari/core/python_interpreter.h` header
- [ ] Create `src/core/python_interpreter.cpp` implementation
- [ ] Implement `initialize()` and `finalize()` methods
- [ ] Set PYTHONHOME to bundled Python location (platform-specific)
- [ ] Update CMakeLists.txt (link Python libraries, pybind11)
- [ ] Initialize Python in main() (before wxWidgets)
- [ ] Finalize Python in main() (after wxWidgets cleanup)

### Phase 2: pybind11 Integration (3-4 hours)
- [ ] Add pybind11 submodule or vcpkg dependency
- [ ] Create sample Python script (`scripts/hello.py`)
- [ ] Execute Python code from C++ (py::exec test)
- [ ] Expose Logger to Python (PYBIND11_EMBEDDED_MODULE)
- [ ] Test bidirectional communication (C++ → Python → C++)
- [ ] Verify Python exceptions translate to C++

### Phase 3: GIL Management (2-3 hours)
- [ ] Test py::gil_scoped_acquire in single-threaded context
- [ ] Test py::gil_scoped_release during long operations
- [ ] Integrate with MainWindow::submitBackgroundTask()
- [ ] Execute Python in background thread with GIL acquisition
- [ ] Stress test: multiple threads calling Python concurrently

### Phase 4: Error Handling & Logging (2 hours)
- [ ] Catch py::error_already_set exceptions
- [ ] Log Python errors with full traceback
- [ ] Test Python exceptions (syntax errors, runtime errors)
- [ ] Test C++ exceptions from Python (pybind11 translation)
- [ ] Show Python errors to user (wxMessageBox)

### Phase 5: Testing (3-4 hours)
- [ ] Unit tests for PythonInterpreter (Catch2)
- [ ] Test initialize/finalize lifecycle
- [ ] Test Python version detection
- [ ] Test PYTHONHOME configuration
- [ ] Test GIL acquisition/release
- [ ] Test exception handling
- [ ] Manual testing: run Python script from GUI

### Phase 6: Documentation (1 hour)
- [ ] Update task file with implementation notes
- [ ] Document Python API in code (Doxygen)
- [ ] Add Python examples to project_docs/

## Risks & Open Questions

**Q: Which Python 3.11 version to bundle?**
- **Proposed:** Python 3.11.9 (latest stable 3.11.x as of 2024)
- **Why 3.11:** Faster than 3.10, stable, good library support

**Q: How to bundle Python with application?**
- **Windows:** Copy `python311.dll` + `python311.zip` (stdlib) to exe directory
- **Linux:** Bundle Python in `/opt/kalahari/python311/` or AppImage
- **macOS:** Bundle in `Kalahari.app/Contents/Resources/python311/`
- **Details:** See [02_tech_stack.md](../project_docs/02_tech_stack.md) Python Embedding section

**Q: How to handle pip dependencies for plugins?**
- **Phase 0 (MVP):** Plugins must bundle dependencies in .kplugin ZIP
- **Phase 2+:** Plugin Manager can install dependencies via pip
- **Rationale:** Avoid pip dependency in Phase 0, keep simple

**Q: Performance impact of GIL?**
- **Mitigation:** Release GIL during long C++ operations
- **Measurement:** Profile with py::gil_scoped_release benchmarks
- **Acceptable:** Plugins are I/O-bound (file reading, API calls), not CPU-bound

**Risk: Python initialization failure (missing dependencies)**
- **Mitigation:** Graceful fallback, disable plugin system if Python fails
- **User message:** "Python runtime not found. Plugins disabled."

**Risk: Thread-safety issues with GIL**
- **Mitigation:** Strict GIL acquisition pattern, unit tests with multiple threads
- **Documentation:** Clear guidelines for plugin developers

## Status
- **Created:** 2025-10-27
- **Approved:** ✅ 2025-10-27 (by User)
- **Started:** ✅ 2025-10-27
- **Completed:** ⏳ In progress

## Implementation Notes
(To be added during implementation)

## Verification
- [ ] Python interpreter initializes without errors
- [ ] Python version is 3.11.x
- [ ] Can execute Python code from C++ (py::exec)
- [ ] Can call Python functions from C++ (py::eval)
- [ ] Logger is accessible from Python (kalahari_api.Logger)
- [ ] Python exceptions are caught and logged
- [ ] GIL acquisition works in background threads
- [ ] Multiple threads can call Python concurrently (with GIL)
- [ ] Application exits cleanly (Py_Finalize no crashes)
- [ ] Unit tests pass (PythonInterpreter, GIL, exceptions)

---

**Estimated Time:** 14-20 hours (2-3 days)

**Priority:** High (blocks plugin system - Phase 0 critical path)

**Can be done in parallel with:** None (foundational component)
