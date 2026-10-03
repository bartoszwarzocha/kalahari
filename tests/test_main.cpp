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

#ifdef _WIN32
#include <process.h>  // _getpid
#else
#include <unistd.h>   // getpid
#endif

#include "test_support/reset_singletons.h"

#include <QApplication>

// =============================================================================
// Test Environment Setup
// =============================================================================

/// @brief Sets up test environment before any tests run
/// This struct's constructor runs before main() due to global initialization
///
/// Each test process gets its own temporary directory: TMPDIR (POSIX) and
/// TMP/TEMP (Windows) are redirected to it, so every temp_directory_path()
/// based file (test settings, archives, databases) is private to the process.
/// This keeps tests independent when ctest runs them in parallel.
struct TestEnvironmentSetup {
    std::filesystem::path m_testTempDir;

    TestEnvironmentSetup() {
        m_testTempDir = std::filesystem::temp_directory_path() /
                        ("kalahari_tests_" + std::to_string(currentProcessId()));
        std::filesystem::create_directories(m_testTempDir);
        const std::string dir = m_testTempDir.string();

        // Set test mode - SettingsManager will use temp directory
#ifdef _WIN32
        _putenv_s("TMP", dir.c_str());
        _putenv_s("TEMP", dir.c_str());
        _putenv("KALAHARI_TEST_MODE=1");
#else
        setenv("TMPDIR", dir.c_str(), 1);
        setenv("XDG_DATA_HOME", dir.c_str(), 1);  // Linux: keep plugin extraction out of $HOME
        setenv("KALAHARI_TEST_MODE", "1", 1);
#endif
    }

    ~TestEnvironmentSetup() {
        // Cleanup: delete this process' temporary directory
        try {
            std::filesystem::remove_all(m_testTempDir);
        } catch (...) {
            // Ignore cleanup errors
        }
    }

    static long currentProcessId() {
#ifdef _WIN32
        return static_cast<long>(_getpid());
#else
        return static_cast<long>(getpid());
#endif
    }
};

// Global instance - constructor runs before main()
static TestEnvironmentSetup g_testSetup;

/// @brief Test event listener to reset ALL process-global singletons before each test
///
/// Sub-Project C WS4.1: previously this only reset SettingsManager. It now calls
/// kalahari::test::resetSingletons() so no singleton state leaks across test
/// ordering (SettingsManager, TrustedKeys, CommandRegistry, IconRegistry,
/// ThemeManager, ArtProvider).
class GlobalResetListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testCaseStarting(Catch::TestCaseInfo const& /* testInfo */) override {
        // Reset all singletons to a deterministic baseline before EACH test case
        kalahari::test::resetSingletons();
    }
};

CATCH_REGISTER_LISTENER(GlobalResetListener);

/// @brief Custom main for test initialization
int main(int argc, char* argv[]) {
    // Initialize Qt (required for QSqlDatabase, QTextLayout, QWidget, and other Qt components)
    // Note: QApplication is needed for QWidget-based tests (BookEditor, etc.)
#ifdef __linux__
    // Tests never need a real display; default to the offscreen platform so the
    // binary also runs headless (CI, ctest discovery) unless the caller overrides it.
    // Windows and macOS always have their native platform plugin (vcpkg's Qt
    // ships no offscreen plugin there).
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
#endif
    QApplication app(argc, argv);
    app.setApplicationName("kalahari-tests");

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
