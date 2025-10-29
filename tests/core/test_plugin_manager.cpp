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
    std::vector<std::thread> threads;
    std::vector<PluginManager*> instances;

    // Create 10 threads, each accessing getInstance()
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&instances]() {
            instances.push_back(&PluginManager::getInstance());
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    // All instances should point to same singleton
    for (size_t i = 1; i < instances.size(); ++i) {
        REQUIRE(instances[i] == instances[0]);
    }
}

TEST_CASE("PluginManager: discoverPlugins returns 0", "[plugin-manager]") {
    PluginManager& manager = PluginManager::getInstance();
    size_t count = manager.discoverPlugins();

    // Phase 0 Week 3-4: Stub returns 0
    REQUIRE(count == 0);
}

TEST_CASE("PluginManager: loadPlugin succeeds", "[plugin-manager]") {
    PluginManager& manager = PluginManager::getInstance();

    // Phase 0 Week 3-4: Stub always returns true
    bool result = manager.loadPlugin("test-plugin");
    REQUIRE(result == true);
}

TEST_CASE("PluginManager: getDiscoveredPlugins empty", "[plugin-manager]") {
    PluginManager& manager = PluginManager::getInstance();
    auto plugins = manager.getDiscoveredPlugins();

    REQUIRE(plugins.empty());
}

TEST_CASE("PluginManager: unloadPlugin works", "[plugin-manager]") {
    PluginManager& manager = PluginManager::getInstance();

    // Phase 0 Week 3-4: Stub should not throw
    REQUIRE_NOTHROW(manager.unloadPlugin("test-plugin"));
}
