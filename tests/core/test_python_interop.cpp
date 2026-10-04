/// @file test_python_interop.cpp
/// @brief C++ ↔ Python interoperability tests

#include <catch2/catch_test_macros.hpp>
#include <pybind11/embed.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/plugin_manager.h>
#include <kalahari/core/python_interpreter.h>

namespace py = pybind11;
using namespace kalahari::core;

namespace {

/// Runs Python code in the application's interpreter. The interpreter is a
/// process-wide singleton shared with the other Python tests, so the tests
/// never finalize it themselves.
bool runPython(const char* code) {
    PythonInterpreter::getInstance().initialize();  // No-op when already initialized
    py::gil_scoped_acquire gil;
    try {
        py::exec(code);
        return true;
    } catch (const py::error_already_set& e) {
        UNSCOPED_INFO(e.what());
        return false;
    }
}

} // namespace

TEST_CASE("Python interop: Initialize Python interpreter", "[python-interop]") {
    PythonInterpreter::getInstance().initialize();
    REQUIRE(PythonInterpreter::getInstance().isInitialized());
    REQUIRE(Py_IsInitialized());
}

TEST_CASE("Python interop: Execute simple Python code", "[python-interop]") {
    REQUIRE(runPython(R"(
print("Hello from Python")
x = 42
)"));
}

TEST_CASE("Python interop: Execute Python with sys.path setup", "[python-interop]") {
    REQUIRE(runPython(R"(
import sys
sys.path.insert(0, '.')
)"));
}

TEST_CASE("Python interop: PluginManager accessible from C++", "[python-interop]") {
    PluginManager& manager = PluginManager::getInstance();

    // Verify singleton is working (rescan first: an earlier test may have discovered plugins)
    REQUIRE_NOTHROW(manager.discoverPlugins());
    REQUIRE(manager.getDiscoveredPlugins().empty());
}

TEST_CASE("Python interop: Logger accessible from C++", "[python-interop]") {
    Logger& logger = Logger::getInstance();

    // Verify Logger works in C++
    REQUIRE_NOTHROW(logger.info("Test from C++"));
    REQUIRE_NOTHROW(logger.debug("Debug from C++"));
}
