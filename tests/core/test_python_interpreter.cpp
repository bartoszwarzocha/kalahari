// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_python_interpreter.cpp
/// @brief Unit tests for PythonInterpreter

#include <catch2/catch_test_macros.hpp>
#include <pybind11/embed.h>
#include <kalahari/core/python_interpreter.h>
#include <kalahari/core/unicode_paths.h>

using namespace kalahari::core;
namespace py = pybind11;

TEST_CASE("PythonInterpreter initialization", "[python]") {
    auto& python = PythonInterpreter::getInstance();
    python.initialize();  // No-op when already initialized

    SECTION("Python is initialized") {
        REQUIRE(python.isInitialized());
    }

    SECTION("Python version is available") {
        std::string version = python.getPythonVersion();
        REQUIRE(!version.empty());
        REQUIRE(version.find("3.") != std::string::npos);
    }

    SECTION("Python home path exists") {
        std::filesystem::path home = python.getPythonHome();
        REQUIRE(!home.empty());
        REQUIRE(std::filesystem::exists(home));
    }
}

TEST_CASE("PythonInterpreter executeTest", "[python]") {
    auto& python = PythonInterpreter::getInstance();
    python.initialize();  // No-op when already initialized

    REQUIRE(python.isInitialized());

    SECTION("Execute test passes all checks") {
        std::string result = python.executeTest();
        INFO(result);

        // Check that all tests passed
        REQUIRE(result.find("Test 1") != std::string::npos);
        REQUIRE(result.find("Test 2") != std::string::npos);
        REQUIRE(result.find("Test 3") != std::string::npos);
        REQUIRE(result.find("Test 4") != std::string::npos);
        REQUIRE(result.find("Test 5") != std::string::npos);
        REQUIRE(result.find("[PASS]") != std::string::npos);
        REQUIRE(result.find("[FAIL]") == std::string::npos);
        REQUIRE(result.find("ERROR") == std::string::npos);
    }
}

TEST_CASE("Python keeps its compiled modules in the cache of the user", "[python]") {
    auto& python = PythonInterpreter::getInstance();
    python.initialize();  // No-op when already initialized

    // Not next to the modules: an installed program does not write into its folder
    const std::filesystem::path folder = PythonInterpreter::compiledModulesFolder();
    REQUIRE_FALSE(folder.empty());
    CHECK(folder.filename() == "python");
    CHECK(folder.parent_path().filename() == "kalahari");

    py::gil_scoped_acquire gil;
    const py::object prefix = py::module_::import("sys").attr("pycache_prefix");
    REQUIRE_FALSE(prefix.is_none());
    CHECK(pathOfText(prefix.cast<std::string>()) == folder);
}
