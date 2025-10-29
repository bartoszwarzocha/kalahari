/// @file python_interpreter.h
/// @brief Python 3.11 embedded interpreter for plugin system
///
/// This class manages the embedded Python interpreter lifecycle.
/// Singleton pattern ensures only one interpreter exists per application.
///
/// Key features:
/// - Python 3.11 embedded (bundled with application)
/// - Thread-safe initialization/finalization
/// - Platform-specific PYTHONHOME configuration
/// - Integration with pybind11 for C++/Python interop

#pragma once

#include <string>
#include <filesystem>
#include <mutex>

namespace kalahari {
namespace core {

/// @brief Singleton manager for embedded Python interpreter
///
/// Usage:
/// @code
/// // In main():
/// auto& python = PythonInterpreter::getInstance();
/// python.initialize();
///
/// // ... application lifetime ...
///
/// python.finalize();
/// @endcode
class PythonInterpreter {
public:
    /// @brief Get singleton instance
    /// @return Reference to the singleton PythonInterpreter
    static PythonInterpreter& getInstance();

    // Prevent copying and moving
    PythonInterpreter(const PythonInterpreter&) = delete;
    PythonInterpreter& operator=(const PythonInterpreter&) = delete;
    PythonInterpreter(PythonInterpreter&&) = delete;
    PythonInterpreter& operator=(PythonInterpreter&&) = delete;

    /// @brief Initialize Python interpreter
    ///
    /// This must be called BEFORE wxWidgets initialization.
    /// Sets PYTHONHOME to bundled Python location and starts the interpreter.
    ///
    /// @throws std::runtime_error if Python initialization fails
    void initialize();

    /// @brief Finalize Python interpreter
    ///
    /// This must be called AFTER wxWidgets cleanup.
    /// Safely shuts down the Python interpreter.
    void finalize();

    /// @brief Check if Python is initialized
    /// @return true if interpreter is running, false otherwise
    bool isInitialized() const;

    /// @brief Get Python version string
    /// @return Python version (e.g., "3.11.9")
    std::string getPythonVersion() const;

    /// @brief Get PYTHONHOME path
    /// @return Path to bundled Python runtime
    std::filesystem::path getPythonHome() const;

    /// @brief Execute test Python code to verify integration
    /// Tests: basic execution, imports, variable access
    /// @return Test results as string (for display/logging)
    std::string executeTest();

private:
    /// @brief Private constructor (singleton)
    PythonInterpreter();

    /// @brief Destructor (calls finalize if needed)
    ~PythonInterpreter();

    /// @brief Get platform-specific Python home directory
    /// @return Path to Python installation
    std::filesystem::path getPlatformPythonHome() const;

    /// @brief Auto-detect Python home based on executable location
    /// Works in both development (vcpkg) and production (bundled) modes
    /// @return Path to Python home directory
    std::filesystem::path detectPythonHome() const;

    /// @brief Detect Python standard library path (platform-specific)
    /// @param pythonHome Python home directory
    /// @return Path to Python stdlib (lib/python3.X on Unix, Lib on Windows)
    std::filesystem::path detectPythonStdlib(const std::filesystem::path& pythonHome) const;

    /// Initialization state
    bool m_initialized = false;

    /// Mutex for thread-safe initialization/finalization
    mutable std::mutex m_mutex;
};

} // namespace core
} // namespace kalahari
