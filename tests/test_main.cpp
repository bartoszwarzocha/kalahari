/// @file test_main.cpp
/// @brief Kalahari test suite - Main test runner
///
/// Phase 0 Week 1 Day 3 - Catch2 integration enabled

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <kalahari/version.h>
#include <kalahari/core/settings_manager.h>

// =============================================================================
// Test Environment Setup
// =============================================================================

/// @brief Sets up test environment before any tests run
/// This struct's constructor runs before main() due to global initialization
struct TestEnvironmentSetup {
    TestEnvironmentSetup() {
        // Set test mode - SettingsManager will use temp directory
#ifdef _WIN32
        _putenv("KALAHARI_TEST_MODE=1");
#else
        setenv("KALAHARI_TEST_MODE", "1", 1);
#endif
    }

    ~TestEnvironmentSetup() {
        // Cleanup: delete test settings directory
        try {
            std::filesystem::path testDir = std::filesystem::temp_directory_path() / "kalahari_test";
            if (std::filesystem::exists(testDir)) {
                std::filesystem::remove_all(testDir);
            }
        } catch (...) {
            // Ignore cleanup errors
        }
    }
};

// Global instance - constructor runs before main()
static TestEnvironmentSetup g_testSetup;

/// @brief Test event listener to reset SettingsManager before each test
class SettingsResetListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testCaseStarting(Catch::TestCaseInfo const& /* testInfo */) override {
        // Reset singleton before EACH test case
        kalahari::core::SettingsManager::getInstance().resetToDefaults();
    }
};

CATCH_REGISTER_LISTENER(SettingsResetListener);

/// @brief Custom main for test initialization
int main(int argc, char* argv[]) {
    // Initialize Catch2
    Catch::Session session;

    // Parse command line
    int returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0) {
        return returnCode;
    }

    // Run tests (listener will reset before each test)
    return session.run();
}

// =============================================================================
// Test Cases
// =============================================================================

TEST_CASE("Version information is valid", "[version]") {
    SECTION("Version string is not empty") {
        REQUIRE_FALSE(std::string(kalahari::VERSION).empty());
    }

    SECTION("Version components are correct") {
        REQUIRE(kalahari::VERSION_MAJOR == 0);
        REQUIRE(kalahari::VERSION_MINOR == 3);
        REQUIRE(kalahari::VERSION_PATCH == 0);
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
