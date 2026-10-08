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
#include <kalahari/core/settings_schema.h>
#include <kalahari/editor/editor_appearance.h>
#include <nlohmann/json.hpp>
#include <QByteArray>
#include <QCoreApplication>
#include <QEventLoop>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

using namespace kalahari::core;

namespace {
/// Run the event loop, so a scheduled save can happen
void runEventLoop(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}
}  // namespace

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

TEST_CASE("SettingsManager takes defaults from the settings schema", "[settings][schema]") {
    auto& settings = SettingsManager::getInstance();
    settings.resetToDefaults();

    SECTION("A missing key returns the schema default") {
        REQUIRE_FALSE(settings.hasKey("dashboard.maxItems"));
        REQUIRE(settings.get<int>("dashboard.maxItems") == 5);
        REQUIRE(settings.get<int>("icons/sizes/toolbar") == 24);
        REQUIRE(settings.get<std::string>("appearance.iconTheme") == "twotone");
    }

    SECTION("The schema default wins over the caller's fallback") {
        REQUIRE(settings.get<int>("dashboard.maxItems", 99) == 5);
    }

    SECTION("A stored value wins over the schema default") {
        settings.set("dashboard.maxItems", 8);
        REQUIRE(settings.get<int>("dashboard.maxItems") == 8);
    }

    SECTION("Editor font defaults match the editor's constants") {
        REQUIRE(*settings_schema::defaultValue("editor.fontFamily") ==
                kalahari::editor::DEFAULT_TEXT_FONT_FAMILY);
        REQUIRE(*settings_schema::defaultValue("editor.fontSize") ==
                kalahari::editor::DEFAULT_TEXT_FONT_SIZE);
    }

    settings.resetToDefaults();
}

TEST_CASE("SettingsManager notifies about changed settings", "[settings][notify]") {
    auto& settings = SettingsManager::getInstance();
    settings.resetToDefaults();

    std::vector<std::string> changed;
    int id = settings.subscribe([&changed](const std::string& key) { changed.push_back(key); });

    settings.set("dashboard.maxItems", 7);
    settings.set("dashboard.maxItems", 7);  // same value: no notification
    settings.set("icons/sizes/menu", 20);
    settings.removeKey("dashboard.maxItems");
    settings.removeKey("dashboard.maxItems");  // already gone: no notification

    settings.unsubscribe(id);
    settings.set("dashboard.maxItems", 9);  // after unsubscribe: no notification

    REQUIRE(changed == std::vector<std::string>{"dashboard.maxItems", "icons.sizes.menu",
                                                "dashboard.maxItems"});
    settings.resetToDefaults();
}

TEST_CASE("SettingsManager migrates old settings files", "[settings][migration]") {
    auto& settings = SettingsManager::getInstance();
    std::filesystem::path filePath = settings.getSettingsFilePath();
    std::filesystem::create_directories(filePath.parent_path());

    {
        std::ofstream file(filePath);
        file << R"({
            "version": "1.1",
            "ui": {"language": "pl", "font_size": 12, "theme": "Dark"},
            "appearance": {"iconTheme": "filled", "toolbarIconSize": 24, "iconSize": 24},
            "log": {"bufferSize": 800, "fontSize": 11,
                    "backgroundColor": {"r": 60, "g": 60, "b": 60}},
            "session": {"auto_save_interval": 300},
            "editor": {"margins": {"viewHorizontal": 50.0, "viewVertical": 30.0, "pageTop": 25.4}},
            "dashboard": {"autoLoadLastProject": true, "maxItems": 4},
            "icons": {"colorPrimary": "#7a7a7a", "colorSecondary": "#dadada",
                      "theme": {"name": "Light"},
                      "themes": {"Dark": {"colorPrimary": "#ffaa00"}}},
            "themes": {"Dark": {"colors": {"primary": "#ffaa00", "secondary": "#644300",
                                           "infoHeader": "#123456"}}}
        })";
    }

    REQUIRE(settings.load());

    REQUIRE(settings.get<std::string>("version", "") == "1.4");
    REQUIRE(settings.getLanguage() == "pl");
    REQUIRE(settings.getTheme() == "Dark");
    REQUIRE(settings.get<std::string>("appearance.iconTheme") == "filled");
    REQUIRE(settings.get<int>("log.bufferSize") == 800);
    REQUIRE_FALSE(settings.hasKey("ui.theme"));
    REQUIRE_FALSE(settings.hasKey("ui.font_size"));
    REQUIRE_FALSE(settings.hasKey("appearance.toolbarIconSize"));
    REQUIRE_FALSE(settings.hasKey("appearance.iconSize"));
    REQUIRE_FALSE(settings.hasKey("log.fontSize"));
    REQUIRE_FALSE(settings.hasKey("log.backgroundColor"));
    REQUIRE_FALSE(settings.hasKey("session"));
    REQUIRE_FALSE(settings.hasKey("editor.margins.viewHorizontal"));
    REQUIRE_FALSE(settings.hasKey("editor.margins.viewVertical"));
    REQUIRE(settings.get<double>("editor.margins.pageTop") == 25.4);
    REQUIRE_FALSE(settings.hasKey("dashboard.autoLoadLastProject"));
    REQUIRE(settings.get<int>("dashboard.maxItems") == 4);
    REQUIRE_FALSE(settings.hasKey("icons.colorPrimary"));
    REQUIRE_FALSE(settings.hasKey("icons.theme"));
    REQUIRE(settings.getIconColorPrimaryForTheme("Dark", "") == "#ffaa00");
    REQUIRE_FALSE(settings.hasKey("themes.Dark.colors.primary"));
    REQUIRE_FALSE(settings.hasKey("themes.Dark.colors.secondary"));
    REQUIRE(settings.get<std::string>("themes.Dark.colors.infoHeader", "") == "#123456");

    // The migrated settings reach the file
    std::filesystem::path tempPath = filePath;
    tempPath += ".tmp";
    REQUIRE_FALSE(std::filesystem::exists(tempPath));
    {
        std::ifstream file(filePath);
        REQUIRE(nlohmann::json::parse(file).value("version", "") == "1.4");
    }

    settings.resetToDefaults();
}

TEST_CASE("SettingsManager save replaces the file in one step", "[settings][persistence]") {
    auto& settings = SettingsManager::getInstance();
    settings.resetToDefaults();
    settings.set("dashboard.maxItems", 6);

    REQUIRE(settings.save());

    // Only settings.json is left: no temporary file next to it
    std::filesystem::path filePath = settings.getSettingsFilePath();
    REQUIRE(std::filesystem::exists(filePath));
    for (const auto& entry : std::filesystem::directory_iterator(filePath.parent_path())) {
        const std::string name = entry.path().filename().string();
        if (name.rfind("settings.json", 0) == 0) {
            REQUIRE(name == "settings.json");
        }
    }

    settings.set("dashboard.maxItems", 3);
    REQUIRE(settings.load());
    REQUIRE(settings.get<int>("dashboard.maxItems") == 6);

    settings.resetToDefaults();
}

TEST_CASE("SettingsManager saves changes shortly after they are made", "[settings][persistence]") {
    REQUIRE(QCoreApplication::instance() != nullptr);
    auto& settings = SettingsManager::getInstance();
    settings.resetToDefaults();
    const std::filesystem::path filePath = settings.getSettingsFilePath();

    SECTION("Several changes are written together") {
        settings.set("dashboard.maxItems", 7);
        settings.set("dashboard.iconSize", 32);
        REQUIRE_FALSE(std::filesystem::exists(filePath));  // not at once

        runEventLoop(1500);
        REQUIRE(std::filesystem::exists(filePath));
        std::ifstream file(filePath);
        nlohmann::json saved = nlohmann::json::parse(file);
        REQUIRE(saved["dashboard"]["maxItems"] == 7);
        REQUIRE(saved["dashboard"]["iconSize"] == 32);
    }

    SECTION("A change from another thread is saved too") {
        std::thread worker([&settings] { settings.set("dashboard.maxItems", 4); });
        worker.join();

        runEventLoop(1500);
        REQUIRE(std::filesystem::exists(filePath));
        settings.set("dashboard.maxItems", 1);  // in memory only, until the next save
        REQUIRE(settings.load());
        REQUIRE(settings.get<int>("dashboard.maxItems") == 4);
    }

    SECTION("A reset cancels the scheduled save") {
        settings.set("dashboard.maxItems", 7);
        settings.resetToDefaults();

        runEventLoop(1500);
        REQUIRE_FALSE(std::filesystem::exists(filePath));
    }

    settings.resetToDefaults();
}

TEST_CASE("SettingsManager stores binary values as base64", "[settings][api]") {
    auto& settings = SettingsManager::getInstance();
    settings.resetToDefaults();

    const QByteArray bytes("\x01\x00\xff layout", 10);
    settings.setBinary("window.state", bytes);
    REQUIRE(settings.getBinary("window.state") == bytes);
    REQUIRE(settings.get<std::string>("window.state") == bytes.toBase64().toStdString());
    REQUIRE(settings.getBinary("window.geometry").isEmpty());

    settings.resetToDefaults();
}

TEST_CASE("SettingsManager moves the old QSettings values into settings.json", "[settings][migration]") {
    auto& settings = SettingsManager::getInstance();
    settings.resetToDefaults();

    const char* const POLISH_PATH = "/books/za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87.klh";  // UTF-8
    const std::filesystem::path iniPath =
        std::filesystem::temp_directory_path() / "kalahari_legacy_settings.ini";
    std::filesystem::remove(iniPath);
    {
        QSettings legacy(QString::fromStdString(iniPath.string()), QSettings::IniFormat);
        legacy.setValue("geometry", QByteArray("geometry-bytes"));
        legacy.setValue("windowState", QByteArray("state-bytes"));
        legacy.setValue("Toolbars/configVersion", 5);
        legacy.setValue("Toolbars/file/visible", true);
        legacy.setValue("Toolbars/format/visible", false);
        legacy.setValue("recentFiles", QStringList{"/books/a.klh", QString::fromUtf8(POLISH_PATH)});
        legacy.setValue("other", 1);  // not Kalahari's layout: stays

        settings.migrateLegacyQSettings(legacy);

        REQUIRE(legacy.allKeys() == QStringList{"other"});
    }

    REQUIRE(settings.getBinary("window.geometry") == QByteArray("geometry-bytes"));
    REQUIRE(settings.getBinary("window.state") == QByteArray("state-bytes"));
    REQUIRE(settings.get<int>("toolbars.configVersion") == 5);
    REQUIRE(settings.get<bool>("toolbars.visible.file", false));
    REQUIRE_FALSE(settings.get<bool>("toolbars.visible.format", true));
    REQUIRE(settings.get<std::vector<std::string>>("recent_files")
            == std::vector<std::string>{"/books/a.klh", POLISH_PATH});

    std::filesystem::remove(iniPath);
    settings.resetToDefaults();
}
