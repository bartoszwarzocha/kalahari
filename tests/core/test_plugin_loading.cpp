// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_plugin_loading.cpp
/// @brief Tests for plugin discovery and loading

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/plugin_manager.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/python_interpreter.h>
#include <kalahari/core/unicode_paths.h>

#include "test_support/plugin_signing.h"

#include <QTemporaryDir>

#include <filesystem>

using namespace kalahari::core;

namespace {

/// The test plugin of the repository (copied next to the tests' build by CMake)
std::filesystem::path helloPluginFile() {
    for (const std::filesystem::path& folder : PluginManager::getInstance().pluginFolders()) {
        const std::filesystem::path file = folder / "hello_plugin.kplugin";
        if (std::filesystem::is_regular_file(file)) {
            return file;
        }
    }
    return {};
}

}  // namespace

// The hello plugin, signed with the tests' key, in a folder of its own: the program loads
// only signed plugins
class PluginLoadingTestFixture {
public:
    PluginLoadingTestFixture() {
        // Initialize logger in the test process' own temporary folder
        Logger::getInstance().init(
            (std::filesystem::temp_directory_path() / "kalahari_test_plugin_loading.log").string());

        REQUIRE(m_folder.isValid());
        const std::filesystem::path source = helloPluginFile();
        REQUIRE_FALSE(source.empty());
        const std::filesystem::path copy =
            pathOfText(m_folder.path().toStdString()) / "hello_plugin.kplugin";
        REQUIRE(std::filesystem::copy_file(source, copy));
        kalahari::test::signPlugin(copy);
        PluginManager::getInstance().setPluginFolders({copy.parent_path()});

        // Initialize Python interpreter
        PythonInterpreter::getInstance().initialize();
    }

    ~PluginLoadingTestFixture() {
        auto& manager = PluginManager::getInstance();
        manager.unloadAll();
        manager.setPluginFolders({});
        manager.discoverPlugins();
    }

    PluginLoadingTestFixture(const PluginLoadingTestFixture&) = delete;
    PluginLoadingTestFixture& operator=(const PluginLoadingTestFixture&) = delete;

private:
    QTemporaryDir m_folder;
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

    SECTION("Tests unpack the plugin in their temporary folder, not in the user's profile") {
        REQUIRE(manager.loadPlugin("org.kalahari.test.hello"));
        const auto* instance = manager.getPluginInstance("org.kalahari.test.hello");
        REQUIRE(instance != nullptr);
        REQUIRE(instance->archive != nullptr);

        const std::filesystem::path relative = instance->archive->getExtractedPath()
                                                   .lexically_relative(
                                                       std::filesystem::temp_directory_path());
        REQUIRE_FALSE(relative.empty());
        REQUIRE(*relative.begin() != "..");
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
