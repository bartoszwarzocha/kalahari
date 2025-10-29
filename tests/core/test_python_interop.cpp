/// @file test_python_interop.cpp
/// @brief C++ ↔ Python interoperability tests

#include <catch2/catch_test_macros.hpp>
#include <Python.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/plugin_manager.h>

using namespace kalahari::core;

/// @brief Python interpreter RAII wrapper for tests
class PythonInterpreter {
public:
    PythonInterpreter() {
        Py_Initialize();
    }

    ~PythonInterpreter() {
        if (Py_IsInitialized()) {
            Py_Finalize();
        }
    }

private:
    // Prevent copying
    PythonInterpreter(const PythonInterpreter&) = delete;
    PythonInterpreter& operator=(const PythonInterpreter&) = delete;
};

TEST_CASE("Python interop: Initialize Python interpreter", "[python-interop]") {
    PythonInterpreter py;
    REQUIRE(Py_IsInitialized());
}

TEST_CASE("Python interop: Execute simple Python code", "[python-interop]") {
    PythonInterpreter py;

    const char* code = R"(
print("Hello from Python")
x = 42
)";

    int result = PyRun_SimpleString(code);
    REQUIRE(result == 0);  // Success
}

TEST_CASE("Python interop: Execute Python with sys.path setup", "[python-interop]") {
    PythonInterpreter py;

    // In real scenario, would add build directory to sys.path here
    // For now, just verify Python code execution works
    const char* code = R"(
import sys
sys.path.insert(0, '.')
)";

    int result = PyRun_SimpleString(code);
    REQUIRE(result == 0);
}

TEST_CASE("Python interop: PluginManager accessible from C++", "[python-interop]") {
    PluginManager& manager = PluginManager::getInstance();

    // Verify singleton is working
    REQUIRE(manager.getDiscoveredPlugins().empty());
    REQUIRE_NOTHROW(manager.discoverPlugins());
}

TEST_CASE("Python interop: Logger accessible from C++", "[python-interop]") {
    Logger& logger = Logger::getInstance();

    // Verify Logger works in C++
    REQUIRE_NOTHROW(logger.info("Test from C++"));
    REQUIRE_NOTHROW(logger.debug("Debug from C++"));
}
