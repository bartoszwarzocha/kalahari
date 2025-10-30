/// @file test_plugin_loading.cpp
/// @brief Tests for plugin discovery and loading

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/plugin_manager.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/python_interpreter.h>

using namespace kalahari::core;

// Test fixture for plugin loading tests
class PluginLoadingTestFixture {
public:
    PluginLoadingTestFixture() {
        // Initialize logger
        Logger::getInstance().init("/tmp/kalahari_test_plugin_loading.log");

        // Initialize Python interpreter
        PythonInterpreter::getInstance().initialize();
    }

    ~PluginLoadingTestFixture() {
        // Cleanup is automatic (singletons)
    }
};

TEST_CASE("Plugin Manager - Discovery", "[plugin]") {
    PluginLoadingTestFixture fixture;

    SECTION("Discover plugins in plugins/ directory") {
        auto& manager = PluginManager::getInstance();
        size_t count = manager.discoverPlugins();

        REQUIRE(count > 0);  // Should find at least hello_plugin

        auto plugins = manager.getDiscoveredPlugins();
        REQUIRE(plugins.size() == count);

        // Check if hello_plugin was discovered
        bool found_hello = false;
        for (const auto& plugin : plugins) {
            if (plugin.id == "org.kalahari.test.hello") {
                found_hello = true;
                REQUIRE(plugin.name == "Hello Plugin");
                REQUIRE(plugin.version == "0.1.0");
            }
        }

        REQUIRE(found_hello);
    }
}

TEST_CASE("Plugin Manager - Loading", "[plugin]") {
    PluginLoadingTestFixture fixture;
    auto& manager = PluginManager::getInstance();

    // Discover plugins first
    manager.discoverPlugins();

    SECTION("Load hello_plugin successfully") {
        bool loaded = manager.loadPlugin("org.kalahari.test.hello");
        REQUIRE(loaded);

        REQUIRE(manager.isPluginLoaded("org.kalahari.test.hello"));

        const auto* instance = manager.getPluginInstance("org.kalahari.test.hello");
        REQUIRE(instance != nullptr);
        REQUIRE(instance->state == PluginState::Activated);
    }

    SECTION("Loading non-existent plugin fails") {
        bool loaded = manager.loadPlugin("org.kalahari.nonexistent");
        REQUIRE_FALSE(loaded);
    }

    SECTION("Double loading returns true (already loaded)") {
        manager.loadPlugin("org.kalahari.test.hello");
        bool loaded_again = manager.loadPlugin("org.kalahari.test.hello");
        REQUIRE(loaded_again);  // Should return true (idempotent)
    }
}

TEST_CASE("Plugin Manager - Unloading", "[plugin]") {
    PluginLoadingTestFixture fixture;
    auto& manager = PluginManager::getInstance();

    manager.discoverPlugins();
    manager.loadPlugin("org.kalahari.test.hello");

    SECTION("Unload loaded plugin") {
        REQUIRE(manager.isPluginLoaded("org.kalahari.test.hello"));

        manager.unloadPlugin("org.kalahari.test.hello");

        REQUIRE_FALSE(manager.isPluginLoaded("org.kalahari.test.hello"));
    }

    SECTION("Unloading non-loaded plugin is safe") {
        manager.unloadPlugin("org.kalahari.nonexistent");
        // Should not crash
    }
}

TEST_CASE("Plugin Manager - Full Lifecycle", "[plugin]") {
    PluginLoadingTestFixture fixture;
    auto& manager = PluginManager::getInstance();

    SECTION("Complete discovery → load → unload cycle") {
        // 1. Discovery
        size_t discovered = manager.discoverPlugins();
        REQUIRE(discovered > 0);

        // 2. Load
        bool loaded = manager.loadPlugin("org.kalahari.test.hello");
        REQUIRE(loaded);
        REQUIRE(manager.isPluginLoaded("org.kalahari.test.hello"));

        const auto* instance = manager.getPluginInstance("org.kalahari.test.hello");
        REQUIRE(instance != nullptr);
        REQUIRE(instance->id == "org.kalahari.test.hello");
        REQUIRE(instance->manifest.name == "Hello Plugin");

        // 3. Unload
        manager.unloadPlugin("org.kalahari.test.hello");
        REQUIRE_FALSE(manager.isPluginLoaded("org.kalahari.test.hello"));

        // 4. Reload should work
        loaded = manager.loadPlugin("org.kalahari.test.hello");
        REQUIRE(loaded);
        REQUIRE(manager.isPluginLoaded("org.kalahari.test.hello"));
    }
}
