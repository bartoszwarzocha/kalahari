/// @file test_settings_manager.cpp
/// @brief Unit tests for SettingsManager (Task #00003)
///
/// Tests cover:
/// - Singleton pattern
/// - Default settings creation
/// - Get/Set operations (type-safe API)
/// - JSON persistence (load/save)
/// - Error handling (corrupted JSON)
/// - Thread-safety (basic check)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/settings_manager.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace kalahari::core;

// =============================================================================
// Test Helper: Create temporary settings file
// =============================================================================

class TempSettingsFile {
public:
    TempSettingsFile() {
        m_path = std::filesystem::temp_directory_path() / "kalahari_test_settings.json";
    }

    ~TempSettingsFile() {
        if (std::filesystem::exists(m_path)) {
            std::filesystem::remove(m_path);
        }
    }

    std::filesystem::path path() const { return m_path; }

    void write(const std::string& content) {
        std::ofstream file(m_path);
        file << content;
    }

private:
    std::filesystem::path m_path;
};

// =============================================================================
// Test Cases
// =============================================================================

TEST_CASE("SettingsManager is a singleton", "[settings][singleton]") {
    SECTION("getInstance() returns the same instance") {
        auto& instance1 = SettingsManager::getInstance();
        auto& instance2 = SettingsManager::getInstance();

        REQUIRE(&instance1 == &instance2);
    }
}

TEST_CASE("SettingsManager creates default settings", "[settings][defaults]") {
    auto& settings = SettingsManager::getInstance();

    SECTION("Default window size is 1280x800") {
        QSize size = settings.getWindowSize();
        REQUIRE(size.width() == 1280);
        REQUIRE(size.height() == 800);
    }

    SECTION("Default window position is (100, 100)") {
        QPoint pos = settings.getWindowPosition();
        REQUIRE(pos.x() == 100);
        REQUIRE(pos.y() == 100);
    }

    SECTION("Default window is not maximized") {
        REQUIRE_FALSE(settings.isWindowMaximized());
    }

    SECTION("Default language is English") {
        REQUIRE(settings.getLanguage() == "en");
    }

    SECTION("Default theme is Light") {
        REQUIRE(settings.getTheme() == "Light");
    }
}

TEST_CASE("SettingsManager get/set operations", "[settings][api]") {
    auto& settings = SettingsManager::getInstance();

    SECTION("Set and get window size") {
        QSize newSize(1920, 1080);
        settings.setWindowSize(newSize);

        QSize retrievedSize = settings.getWindowSize();
        REQUIRE(retrievedSize.width() == 1920);
        REQUIRE(retrievedSize.height() == 1080);
    }

    SECTION("Set and get window position") {
        QPoint newPos(200, 150);
        settings.setWindowPosition(newPos);

        QPoint retrievedPos = settings.getWindowPosition();
        REQUIRE(retrievedPos.x() == 200);
        REQUIRE(retrievedPos.y() == 150);
    }

    SECTION("Set and get maximized state") {
        settings.setWindowMaximized(true);
        REQUIRE(settings.isWindowMaximized() == true);

        settings.setWindowMaximized(false);
        REQUIRE(settings.isWindowMaximized() == false);
    }

    SECTION("Set and get language") {
        settings.setLanguage("pl");
        REQUIRE(settings.getLanguage() == "pl");

        settings.setLanguage("en");
        REQUIRE(settings.getLanguage() == "en");
    }

    SECTION("Set and get theme") {
        settings.setTheme("Dark");
        REQUIRE(settings.getTheme() == "Dark");

        settings.setTheme("Savanna");
        REQUIRE(settings.getTheme() == "Savanna");
    }
}

TEST_CASE("SettingsManager type-safe get with default", "[settings][api]") {
    auto& settings = SettingsManager::getInstance();

    SECTION("Get existing int value") {
        settings.set("window.width", 1600);
        int width = settings.get<int>("window.width", 9999);
        REQUIRE(width == 1600);
    }

    SECTION("Get non-existing int value returns default") {
        int value = settings.get<int>("nonexistent.key", 42);
        REQUIRE(value == 42);
    }

    SECTION("Get existing string value") {
        settings.set("ui.language", std::string("de"));
        std::string lang = settings.get<std::string>("ui.language", "unknown");
        REQUIRE(lang == "de");
    }

    SECTION("Get non-existing string value returns default") {
        std::string value = settings.get<std::string>("nonexistent.key", "default_value");
        REQUIRE(value == "default_value");
    }

    SECTION("Get existing bool value") {
        settings.set("window.maximized", true);
        bool maximized = settings.get<bool>("window.maximized", false);
        REQUIRE(maximized == true);
    }

    SECTION("Get non-existing bool value returns default") {
        bool value = settings.get<bool>("nonexistent.key", true);
        REQUIRE(value == true);
    }
}

TEST_CASE("SettingsManager save and load", "[settings][persistence]") {
    auto& settings = SettingsManager::getInstance();

    SECTION("Save creates settings file") {
        settings.setWindowSize(QSize(1600, 900));
        settings.setWindowPosition(QPoint(50, 75));
        settings.setLanguage("pl");

        REQUIRE(settings.save() == true);

        // Verify file exists
        std::filesystem::path filePath = settings.getSettingsFilePath();
        REQUIRE(std::filesystem::exists(filePath));
    }

    SECTION("Load reads settings from file") {
        // Save settings
        settings.setWindowSize(QSize(800, 600));
        settings.setWindowPosition(QPoint(10, 20));
        settings.setWindowMaximized(true);
        settings.save();

        // Modify in-memory settings
        settings.setWindowSize(QSize(1024, 768));
        settings.setWindowPosition(QPoint(100, 100));
        settings.setWindowMaximized(false);

        // Load from file (should restore saved values)
        REQUIRE(settings.load() == true);

        // Verify values were restored
        QSize size = settings.getWindowSize();
        QPoint pos = settings.getWindowPosition();
        bool maximized = settings.isWindowMaximized();

        REQUIRE(size.width() == 800);
        REQUIRE(size.height() == 600);
        REQUIRE(pos.x() == 10);
        REQUIRE(pos.y() == 20);
        REQUIRE(maximized == true);
    }
}

TEST_CASE("SettingsManager error handling", "[settings][errors]") {
    auto& settings = SettingsManager::getInstance();

    SECTION("Load from non-existent file returns true (uses defaults)") {
        // Delete settings file if exists
        std::filesystem::path filePath = settings.getSettingsFilePath();
        if (std::filesystem::exists(filePath)) {
            std::filesystem::remove(filePath);
        }

        REQUIRE(settings.load() == true);

        // Should use defaults
        REQUIRE(settings.getLanguage() == "en");
    }

    SECTION("Load from corrupted JSON returns false (uses defaults)") {
        // Create corrupted JSON file
        std::filesystem::path filePath = settings.getSettingsFilePath();

        // Ensure directory exists
        std::filesystem::create_directories(filePath.parent_path());

        std::ofstream file(filePath);
        file << "{\"window\": {\"width\": 1280, \"hei";  // Incomplete JSON
        file.close();

        REQUIRE(settings.load() == false);

        // Should use defaults (not crash!)
        QSize size = settings.getWindowSize();
        REQUIRE(size.width() == 1280);
        REQUIRE(size.height() == 800);

        // Backup file should be created
        std::filesystem::path backupPath = filePath;
        backupPath += ".bak";
        REQUIRE(std::filesystem::exists(backupPath));

        // Cleanup
        std::filesystem::remove(backupPath);
    }
}

TEST_CASE("SettingsManager thread-safety", "[settings][threading]") {
    auto& settings = SettingsManager::getInstance();

    SECTION("Concurrent get/set operations don't crash") {
        constexpr int NUM_THREADS = 10;
        constexpr int ITERATIONS = 100;

        // Atomic counter for successful validations (Catch2 is NOT thread-safe!)
        std::atomic<int> valid_reads(0);
        std::vector<std::thread> threads;

        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([&settings, t, &valid_reads]() {
                for (int i = 0; i < ITERATIONS; ++i) {
                    // Set window size
                    settings.set("window.width", 1000 + t * 10 + i);
                    settings.set("window.height", 800 + t * 5 + i);

                    // Get window size
                    int width = settings.get<int>("window.width", 1280);
                    int height = settings.get<int>("window.height", 800);

                    // Verify values are reasonable (cannot use REQUIRE in threads!)
                    if (width > 0 && height > 0) {
                        valid_reads.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }

        // Verify all reads were valid (Catch2-safe assertion in main thread)
        REQUIRE(valid_reads == NUM_THREADS * ITERATIONS);
    }
}

TEST_CASE("SettingsManager settings file path", "[settings][paths]") {
    auto& settings = SettingsManager::getInstance();

    SECTION("Settings file path is valid") {
        std::filesystem::path filePath = settings.getSettingsFilePath();

        REQUIRE_FALSE(filePath.empty());
        REQUIRE(filePath.filename() == "settings.json");

        // Verify parent directory is platform-specific
        std::string parentPath = filePath.parent_path().string();
        bool validPath = false;

        // Check if running in test mode (KALAHARI_TEST_MODE is set)
        bool isTestMode = false;
#ifdef _WIN32
        char* testMode = nullptr;
        size_t testLen = 0;
        if (_dupenv_s(&testMode, &testLen, "KALAHARI_TEST_MODE") == 0 && testMode != nullptr) {
            isTestMode = true;
            free(testMode);
        }
#else
        isTestMode = (std::getenv("KALAHARI_TEST_MODE") != nullptr);
#endif

        if (isTestMode) {
            // Test mode: should contain "kalahari_test"
            validPath = parentPath.find("kalahari_test") != std::string::npos;
        } else {
            // Production mode: platform-specific paths
#ifdef _WIN32
            validPath = parentPath.find("Kalahari") != std::string::npos;
#elif defined(__APPLE__)
            validPath = parentPath.find("Library") != std::string::npos &&
                       parentPath.find("Application Support") != std::string::npos &&
                       parentPath.find("Kalahari") != std::string::npos;
#else // Linux
            validPath = parentPath.find(".config") != std::string::npos &&
                       parentPath.find("kalahari") != std::string::npos;
#endif
        }

        REQUIRE(validPath);
    }
}

TEST_CASE("SettingsManager icon colors (Task #00020)", "[settings][icons]") {
    auto& settings = SettingsManager::getInstance();

    SECTION("Default primary icon color is #333333") {
        std::string primary = settings.getIconColorPrimary();
        REQUIRE(primary == "#333333");
    }

    SECTION("Default secondary icon color is #999999") {
        std::string secondary = settings.getIconColorSecondary();
        REQUIRE(secondary == "#999999");
    }

    SECTION("Can set and get primary icon color") {
        settings.setIconColorPrimary("#ff0000");
        std::string primary = settings.getIconColorPrimary();
        REQUIRE(primary == "#ff0000");

        // Restore default
        settings.setIconColorPrimary("#333333");
    }

    SECTION("Can set and get secondary icon color") {
        settings.setIconColorSecondary("#00ff00");
        std::string secondary = settings.getIconColorSecondary();
        REQUIRE(secondary == "#00ff00");

        // Restore default
        settings.setIconColorSecondary("#999999");
    }

    SECTION("Icon colors persist to disk") {
        // Set custom colors
        settings.setIconColorPrimary("#abcdef");
        settings.setIconColorSecondary("#123456");
        settings.save();

        // Create new instance (reload from disk)
        settings.load();

        // Verify colors persisted
        REQUIRE(settings.getIconColorPrimary() == "#abcdef");
        REQUIRE(settings.getIconColorSecondary() == "#123456");

        // Restore defaults
        settings.setIconColorPrimary("#333333");
        settings.setIconColorSecondary("#999999");
        settings.save();
    }
}
