/// @file test_python_interpreter.cpp
/// @brief Unit tests for PythonInterpreter

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/python_interpreter.h>

using namespace kalahari::core;

TEST_CASE("PythonInterpreter initialization", "[python]") {
    auto& python = PythonInterpreter::getInstance();

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

    REQUIRE(python.isInitialized());

    SECTION("Execute test passes all checks") {
        std::string result = python.executeTest();

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
