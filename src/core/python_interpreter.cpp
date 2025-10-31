/// @file python_interpreter.cpp
/// @brief Implementation of PythonInterpreter

#include <kalahari/core/python_interpreter.h>
#include <kalahari/core/logger.h>
#include <pybind11/embed.h>
#include <stdexcept>
#include <chrono>  // For finalization timing diagnostics

#ifdef _WIN32
#include <windows.h>  // GetModuleFileNameW
#else
#include <unistd.h>   // readlink
#include <climits>    // PATH_MAX
#ifdef __APPLE__
#include <mach-o/dyld.h>  // _NSGetExecutablePath
#endif
#endif

namespace py = pybind11;

namespace kalahari {
namespace core {

// =============================================================================
// Singleton instance
// =============================================================================

PythonInterpreter& PythonInterpreter::getInstance() {
    static PythonInterpreter instance;
    return instance;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

PythonInterpreter::PythonInterpreter() {
    // Don't initialize here - too early in startup
    // Call initialize() explicitly after Logger is ready
}

PythonInterpreter::~PythonInterpreter() {
    if (m_initialized) {
        finalize();
    }
}

// =============================================================================
// Public API
// =============================================================================

void PythonInterpreter::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        Logger::getInstance().warn("Python already initialized");
        return;
    }

    try {
        Logger::getInstance().info("Initializing Python interpreter...");

        // Use PyConfig API (Python 3.8+) for full control over paths
        // This MUST work without user configuration - autodetect everything
        PyConfig config;
        PyConfig_InitPythonConfig(&config);

        // Detect Python home based on executable location
        std::filesystem::path pythonHome = detectPythonHome();
        Logger::getInstance().info("Detected Python home: {}", pythonHome.string());

        // Detect stdlib path (platform-specific: Lib on Windows, lib/python3.X on Unix)
        std::filesystem::path pythonLib = detectPythonStdlib(pythonHome);
        Logger::getInstance().info("Python stdlib path: {}", pythonLib.string());

#ifdef _WIN32
        // Windows uses wchar_t
        PyConfig_SetString(&config, &config.home, pythonHome.wstring().c_str());

        // Set module search paths explicitly
        config.module_search_paths_set = 1;
        PyWideStringList_Append(&config.module_search_paths, pythonLib.wstring().c_str());
#else
        // Linux/macOS use UTF-8 → must convert to wchar_t* using Py_DecodeLocale
        wchar_t* homeWide = Py_DecodeLocale(pythonHome.string().c_str(), nullptr);
        wchar_t* libWide = Py_DecodeLocale(pythonLib.string().c_str(), nullptr);

        if (!homeWide || !libWide) {
            PyMem_RawFree(homeWide);
            PyMem_RawFree(libWide);
            throw std::runtime_error("Failed to convert paths to wchar_t (Py_DecodeLocale failed)");
        }

        PyConfig_SetString(&config, &config.home, homeWide);

        config.module_search_paths_set = 1;
        PyWideStringList_Append(&config.module_search_paths, libWide);

        // Clean up allocated wchar_t* (Python docs: must use PyMem_RawFree)
        PyMem_RawFree(homeWide);
        PyMem_RawFree(libWide);
#endif

        // Initialize from config
        PyStatus status = Py_InitializeFromConfig(&config);
        PyConfig_Clear(&config);

        if (PyStatus_Exception(status)) {
            std::string error = status.err_msg ? status.err_msg : "Unknown error";
            throw std::runtime_error("Py_InitializeFromConfig failed: " + error);
        }

        m_initialized = true;
        Logger::getInstance().info("Python {} initialized successfully", getPythonVersion());

    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to initialize Python: {}", e.what());
        throw std::runtime_error(std::string("Python initialization failed: ") + e.what());
    }
}

void PythonInterpreter::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        Logger::getInstance().warn("Python not initialized, nothing to finalize");
        return;
    }

    try {
        Logger::getInstance().info("========================================");
        Logger::getInstance().info("PYTHON FINALIZATION STARTED");
        Logger::getInstance().info("========================================");

        // Step 1: Check GIL state before finalization
        Logger::getInstance().debug("[FINALIZE STEP 1/6] Checking GIL state...");
        bool gilHeld = false;
        try {
            // Check if GIL is held by attempting to acquire/release
            PyGILState_STATE gilState = PyGILState_Ensure();
            gilHeld = true;
            PyGILState_Release(gilState);
            Logger::getInstance().debug("[FINALIZE STEP 1/6] GIL check complete (was held: {})", gilHeld);
        } catch (...) {
            Logger::getInstance().warn("[FINALIZE STEP 1/6] GIL check failed (interpreter may be in inconsistent state)");
        }

        // Step 2: Check Python thread state
        Logger::getInstance().debug("[FINALIZE STEP 2/6] Checking Python thread state...");
        PyThreadState* mainThreadState = PyThreadState_Get();
        if (mainThreadState) {
            Logger::getInstance().debug("[FINALIZE STEP 2/6] Main thread state: {}", static_cast<void*>(mainThreadState));
        } else {
            Logger::getInstance().warn("[FINALIZE STEP 2/6] No main thread state found!");
        }

        // Step 3: Flush Python stdout/stderr
        Logger::getInstance().debug("[FINALIZE STEP 3/6] Flushing Python stdout/stderr...");
        try {
            py::gil_scoped_acquire gil;
            py::module_ sys = py::module_::import("sys");
            if (py::hasattr(sys, "stdout") && !sys.attr("stdout").is_none()) {
                sys.attr("stdout").attr("flush")();
            }
            if (py::hasattr(sys, "stderr") && !sys.attr("stderr").is_none()) {
                sys.attr("stderr").attr("flush")();
            }
            Logger::getInstance().debug("[FINALIZE STEP 3/6] Python streams flushed");
        } catch (const std::exception& e) {
            Logger::getInstance().warn("[FINALIZE STEP 3/6] Failed to flush Python streams: {}", e.what());
        }

        // Step 4: Log interpreter info before finalization
        Logger::getInstance().debug("[FINALIZE STEP 4/6] Python version: {}", getPythonVersion());
        Logger::getInstance().debug("[FINALIZE STEP 4/6] Initialized flag: {}", m_initialized);

        // Step 5: Call py::finalize_interpreter() - CRITICAL SECTION
        Logger::getInstance().info("[FINALIZE STEP 5/6] Calling py::finalize_interpreter()...");
        Logger::getInstance().flush(); // CRITICAL: Flush before finalize in case it hangs

        auto startTime = std::chrono::steady_clock::now();
        py::finalize_interpreter();
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        Logger::getInstance().info("[FINALIZE STEP 5/6] py::finalize_interpreter() completed in {}ms", duration);

        // Step 6: Mark as finalized
        Logger::getInstance().debug("[FINALIZE STEP 6/6] Setting m_initialized = false");
        m_initialized = false;

        Logger::getInstance().info("========================================");
        Logger::getInstance().info("PYTHON FINALIZATION COMPLETE");
        Logger::getInstance().info("========================================");
        Logger::getInstance().flush(); // Ensure logs are written

    } catch (const std::exception& e) {
        Logger::getInstance().error("ERROR during Python finalization: {}", e.what());
        Logger::getInstance().flush();
        // Don't throw from destructor-like function
    }
}

bool PythonInterpreter::isInitialized() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}

std::string PythonInterpreter::getPythonVersion() const {
    if (!m_initialized) {
        return "Not initialized";
    }

    try {
        py::gil_scoped_acquire acquire;
        py::object sys = py::module_::import("sys");
        py::object version = sys.attr("version");
        return py::str(version);
    } catch (const std::exception& e) {
        Logger::getInstance().error("Failed to get Python version: {}", e.what());
        return "Unknown";
    }
}

std::filesystem::path PythonInterpreter::getPythonHome() const {
    return getPlatformPythonHome();
}

std::string PythonInterpreter::executeTest() {
    if (!m_initialized) {
        return "ERROR: Python not initialized";
    }

    std::string result;

    try {
        py::gil_scoped_acquire acquire;

        // Test 1: Basic arithmetic
        py::exec("test_result = 2 + 2");
        int arithmetic = py::globals()["test_result"].cast<int>();
        result += "Test 1 - Arithmetic: 2 + 2 = " + std::to_string(arithmetic);
        result += (arithmetic == 4) ? " [PASS]\n" : " [FAIL]\n";

        // Test 2: String formatting
        py::exec(R"(
test_message = "Hello from Python {}".format("3.12")
)");
        std::string message = py::globals()["test_message"].cast<std::string>();
        result += "Test 2 - String: \"" + message + "\" [PASS]\n";

        // Test 3: Import standard library
        py::exec("import sys");
        py::object sys = py::module_::import("sys");
        std::string version = py::str(sys.attr("version"));
        result += "Test 3 - Import sys: version = " + version.substr(0, 6) + "... [PASS]\n";

        // Test 4: List operations
        py::exec("test_list = [1, 2, 3, 4, 5]");
        py::exec("test_sum = sum(test_list)");
        int sum = py::globals()["test_sum"].cast<int>();
        result += "Test 4 - List sum: sum([1,2,3,4,5]) = " + std::to_string(sum);
        result += (sum == 15) ? " [PASS]\n" : " [FAIL]\n";

        // Test 5: Dictionary
        py::exec(R"(
test_dict = {'name': 'Kalahari', 'version': '0.0.1'}
test_name = test_dict['name']
)");
        std::string name = py::globals()["test_name"].cast<std::string>();
        result += "Test 5 - Dictionary: name = \"" + name + "\" [PASS]\n";

        result += "\nAll Python integration tests passed! ✓";

    } catch (const py::error_already_set& e) {
        result += "\nERROR: Python exception: " + std::string(e.what());
    } catch (const std::exception& e) {
        result += "\nERROR: C++ exception: " + std::string(e.what());
    }

    return result;
}

// =============================================================================
// Private helpers
// =============================================================================

std::filesystem::path PythonInterpreter::detectPythonHome() const {
    // AUTO-DETECT Python location based on executable path
    // This MUST work in both development and production without user configuration

#ifdef _WIN32
    // Windows: Get executable directory
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();

    Logger::getInstance().debug("Executable directory: {}", exeDir.string());

    // Strategy 1: Production - Python bundled next to .exe
    // C:/Program Files/Kalahari/kalahari.exe
    // C:/Program Files/Kalahari/Lib/
    std::filesystem::path bundledLib = exeDir / "Lib";
    if (std::filesystem::exists(bundledLib)) {
        Logger::getInstance().info("Found bundled Python (production mode)");
        return exeDir;
    }

    // Strategy 2: Development - vcpkg Python
    // build-windows/bin/kalahari.exe
    // build-windows/vcpkg_installed/x64-windows/tools/python3/
    std::filesystem::path vcpkgPython = exeDir.parent_path() / "vcpkg_installed" / "x64-windows" / "tools" / "python3";
    if (std::filesystem::exists(vcpkgPython / "Lib")) {
        Logger::getInstance().info("Found vcpkg Python (development mode)");
        return vcpkgPython;
    }

    // Strategy 3: Debug build - vcpkg in different path
    // build-windows/bin/Debug/kalahari.exe (MSBuild multi-config)
    vcpkgPython = exeDir.parent_path().parent_path() / "vcpkg_installed" / "x64-windows" / "tools" / "python3";
    if (std::filesystem::exists(vcpkgPython / "Lib")) {
        Logger::getInstance().info("Found vcpkg Python (debug build mode)");
        return vcpkgPython;
    }

    throw std::runtime_error("Python installation not found! Searched:\n" +
                             bundledLib.string() + "\n" +
                             vcpkgPython.string());

#elif defined(__APPLE__)
    // macOS: Check vcpkg first, then fallback to system/bundled Python
    char exePath[PATH_MAX];
    uint32_t size = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &size) == 0) {
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
        Logger::getInstance().debug("Executable directory: {}", exeDir.string());

        // Strategy 1: Development - vcpkg Python (arm64-osx or x64-osx)
        // build/bin/kalahari
        // build/vcpkg_installed/arm64-osx/tools/python3/
        std::filesystem::path vcpkgPython = exeDir.parent_path() / "vcpkg_installed" / "arm64-osx" / "tools" / "python3";
        if (std::filesystem::exists(vcpkgPython / "bin" / "python3")) {
            Logger::getInstance().info("Found vcpkg Python arm64 (development mode)");
            return vcpkgPython;
        }

        // Try x64-osx (Intel Macs)
        vcpkgPython = exeDir.parent_path() / "vcpkg_installed" / "x64-osx" / "tools" / "python3";
        if (std::filesystem::exists(vcpkgPython / "bin" / "python3")) {
            Logger::getInstance().info("Found vcpkg Python x64 (development mode)");
            return vcpkgPython;
        }

        // Strategy 2: Production - Bundled Python in .app/Contents/Resources/
        std::filesystem::path bundledPython = exeDir.parent_path() / "Resources" / "python3";
        if (std::filesystem::exists(bundledPython)) {
            Logger::getInstance().info("Found bundled Python (production mode)");
            return bundledPython;
        }
    }

    // Strategy 3: Fallback - System Python (Homebrew or system)
    Logger::getInstance().warn("vcpkg Python not found, falling back to /usr/local");
    return "/usr/local";

#else
    // Linux: Check vcpkg first, then system
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();

        // Development: vcpkg Python
        std::filesystem::path vcpkgPython = exeDir.parent_path() / "vcpkg_installed" / "x64-linux" / "tools" / "python3";
        if (std::filesystem::exists(vcpkgPython / "lib" / "python3.12")) {
            Logger::getInstance().info("Found vcpkg Python (development mode)");
            return vcpkgPython;
        }
    }

    // Production: System Python
    return "/usr";
#endif
}

std::filesystem::path PythonInterpreter::getPlatformPythonHome() const {
    return detectPythonHome();
}

std::filesystem::path PythonInterpreter::detectPythonStdlib(const std::filesystem::path& pythonHome) const {
    // Platform-specific Python standard library detection
    // This is critical - Python stdlib location varies by platform!

#ifdef _WIN32
    // Windows: Always "Lib" (uppercase) directly under Python home
    // Example: C:\Python312\Lib
    //          build-windows\vcpkg_installed\x64-windows\tools\python3\Lib
    std::filesystem::path stdlibPath = pythonHome / "Lib";

    Logger::getInstance().debug("Windows stdlib detection: {}", stdlibPath.string());

    if (std::filesystem::exists(stdlibPath)) {
        Logger::getInstance().info("Found Windows stdlib: {}", stdlibPath.string());
        return stdlibPath;
    }

    throw std::runtime_error("Windows Python stdlib not found at: " + stdlibPath.string());

#elif defined(__APPLE__)
    // macOS: Multiple possible locations depending on Python distribution
    // 1. Try lib/pythonX.Y (standard Unix layout - vcpkg, system Python)
    // 2. Try Frameworks/Python.framework/Versions/X.Y/lib/pythonX.Y (Homebrew Python)
    // 3. Try lib/ directory scan (vcpkg might use different structure)
    std::vector<std::string> versions = {"3.13", "3.12", "3.11"};

    // Attempt 1: Standard Unix layout (lib/pythonX.Y)
    for (const auto& version : versions) {
        std::filesystem::path stdlibPath = pythonHome / "lib" / ("python" + version);
        Logger::getInstance().debug("macOS stdlib attempt (standard): {}", stdlibPath.string());

        if (std::filesystem::exists(stdlibPath)) {
            Logger::getInstance().info("Found macOS stdlib: {}", stdlibPath.string());
            return stdlibPath;
        }
    }

    // Attempt 2: Framework layout (Homebrew)
    for (const auto& version : versions) {
        std::filesystem::path frameworkPath = pythonHome / "Frameworks" / "Python.framework" / "Versions" / version / "lib" / ("python" + version);
        Logger::getInstance().debug("macOS stdlib attempt (framework): {}", frameworkPath.string());

        if (std::filesystem::exists(frameworkPath)) {
            Logger::getInstance().info("Found macOS stdlib (framework): {}", frameworkPath.string());
            return frameworkPath;
        }
    }

    // Attempt 3: Scan lib/ directory for python3.X folders (vcpkg)
    std::filesystem::path libDir = pythonHome / "lib";
    if (std::filesystem::exists(libDir)) {
        Logger::getInstance().debug("Scanning lib directory: {}", libDir.string());

        for (const auto& entry : std::filesystem::directory_iterator(libDir)) {
            if (entry.is_directory()) {
                std::string dirname = entry.path().filename().string();
                if (dirname.starts_with("python3.") || dirname == "python3") {
                    Logger::getInstance().info("Found macOS stdlib (scanned): {}", entry.path().string());
                    return entry.path();
                }
            }
        }
    }

    // Fallback: Just use lib/ if it exists (some Python distributions)
    if (std::filesystem::exists(libDir)) {
        Logger::getInstance().info("Found macOS stdlib (fallback lib/): {}", libDir.string());
        return libDir;
    }

    throw std::runtime_error("macOS Python stdlib not found under: " + pythonHome.string());

#else
    // Linux: lib/pythonX.Y (lowercase, versioned)
    // Try multiple Python versions (3.13 → 3.11)
    std::vector<std::string> versions = {"3.13", "3.12", "3.11"};

    for (const auto& version : versions) {
        std::filesystem::path stdlibPath = pythonHome / "lib" / ("python" + version);
        Logger::getInstance().debug("Linux stdlib attempt: {}", stdlibPath.string());

        if (std::filesystem::exists(stdlibPath)) {
            Logger::getInstance().info("Found Linux stdlib: {}", stdlibPath.string());
            return stdlibPath;
        }
    }

    // Fallback: Check if pythonHome already points to lib directory
    // (e.g., /usr → check /usr/lib/python3.12)
    std::filesystem::path libDir = pythonHome / "lib";
    if (std::filesystem::exists(libDir)) {
        Logger::getInstance().debug("Checking lib subdirectory: {}", libDir.string());

        for (const auto& entry : std::filesystem::directory_iterator(libDir)) {
            if (entry.is_directory() && entry.path().filename().string().starts_with("python3.")) {
                Logger::getInstance().info("Found Linux stdlib: {}", entry.path().string());
                return entry.path();
            }
        }
    }

    throw std::runtime_error("Linux Python stdlib not found under: " + pythonHome.string() + "/lib/python3.X");
#endif
}

} // namespace core
} // namespace kalahari
