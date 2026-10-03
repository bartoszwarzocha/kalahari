/// @file test_plugin_manager.cpp
/// @brief Unit tests for PluginManager singleton

#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>
#include <kalahari/core/plugin_manager.h>

using namespace kalahari::core;

TEST_CASE("PluginManager: Singleton pattern", "[plugin-manager]") {
    PluginManager& manager1 = PluginManager::getInstance();
    PluginManager& manager2 = PluginManager::getInstance();

    REQUIRE(&manager1 == &manager2);
}

TEST_CASE("PluginManager: Thread safety", "[plugin-manager]") {
    constexpr size_t THREAD_COUNT = 10;
    std::vector<std::thread> threads;
    std::vector<PluginManager*> instances(THREAD_COUNT, nullptr);

    // Create 10 threads, each accessing getInstance() and writing its own slot
    for (size_t i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&instances, i]() {
            instances[i] = &PluginManager::getInstance();
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    // All instances should point to same singleton
    REQUIRE(instances[0] != nullptr);
    for (size_t i = 1; i < instances.size(); ++i) {
        REQUIRE(instances[i] == instances[0]);
    }
}

TEST_CASE("PluginManager: discoverPlugins returns 0", "[plugin-manager]") {
    PluginManager& manager = PluginManager::getInstance();
    size_t count = manager.discoverPlugins();

    // The test user plugin directory is empty
    REQUIRE(count == 0);
}

TEST_CASE("PluginManager: loadPlugin fails for an undiscovered plugin", "[plugin-manager]") {
    PluginManager& manager = PluginManager::getInstance();

    REQUIRE_FALSE(manager.loadPlugin("test-plugin"));
}

TEST_CASE("PluginManager: getDiscoveredPlugins empty", "[plugin-manager]") {
    PluginManager& manager = PluginManager::getInstance();
    auto plugins = manager.getDiscoveredPlugins();

    REQUIRE(plugins.empty());
}

TEST_CASE("PluginManager: unloadPlugin works", "[plugin-manager]") {
    PluginManager& manager = PluginManager::getInstance();

    // Unloading a plugin that is not loaded is a no-op
    REQUIRE_NOTHROW(manager.unloadPlugin("test-plugin"));
}
