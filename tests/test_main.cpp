/// @file test_main.cpp
/// @brief Kalahari test suite - Main test runner
///
/// Phase 0 Week 1 Day 3 - Catch2 integration enabled

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <kalahari/version.h>

TEST_CASE("Version information is valid", "[version]") {
    SECTION("Version string is not empty") {
        REQUIRE_FALSE(std::string(kalahari::VERSION).empty());
    }

    SECTION("Version components are correct") {
        REQUIRE(kalahari::VERSION_MAJOR == 0);
        REQUIRE(kalahari::VERSION_MINOR == 0);
        REQUIRE(kalahari::VERSION_PATCH == 1);
    }

    SECTION("Platform is recognized") {
        REQUIRE(std::string(kalahari::PLATFORM) != "Unknown");
    }
}

TEST_CASE("Build configuration is valid", "[build]") {
    SECTION("Build type is set") {
        REQUIRE_FALSE(std::string(kalahari::BUILD_TYPE).empty());
    }

    SECTION("Compiler information is available") {
        REQUIRE_FALSE(std::string(kalahari::COMPILER).empty());
    }
}
