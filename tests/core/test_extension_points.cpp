/// @file test_extension_points.cpp
/// @brief Unit tests for Extension Points Registry (Task #00010)
///
/// Tests cover:
/// - Singleton pattern
/// - Plugin registration and unregistration
/// - Type-safe plugin retrieval
/// - Thread-safety
/// - Extension point interface validation

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/extension_points.h>
#include <thread>
#include <vector>
#include <memory>

using namespace kalahari::core;

// =============================================================================
// Mock Plugin Classes for Testing
// =============================================================================

class TestPlugin : public IPlugin {
public:
    TestPlugin(const std::string& id = "test-plugin", const std::string& version = "1.0.0")
        : m_id(id), m_version(version), m_init_called(false), m_activate_called(false) {}

    std::string getPluginId() const override { return m_id; }
    std::string getVersion() const override { return m_version; }
    void onInit() override { m_init_called = true; }
    void onActivate() override { m_activate_called = true; }

    bool wasInitCalled() const { return m_init_called; }
    bool wasActivateCalled() const { return m_activate_called; }

private:
    std::string m_id;
    std::string m_version;
    bool m_init_called;
    bool m_activate_called;
};

class TestExporter : public IExporter {
public:
    TestExporter(const std::string& id = "test-exporter")
        : m_id(id), m_init_called(false) {}

    std::string getPluginId() const override { return m_id; }
    std::string getVersion() const override { return "1.0.0"; }
    void onInit() override { m_init_called = true; }
    void onActivate() override {}
    bool exportDocument(const std::string& format, const std::string& /* filepath */) override {
        return format == "pdf" || format == "docx";
    }

    bool wasInitCalled() const { return m_init_called; }

private:
    std::string m_id;
    bool m_init_called;
};

class TestAssistant : public IAssistant {
public:
    TestAssistant(const std::string& id = "test-assistant")
        : m_id(id), m_message_count(0) {}

    std::string getPluginId() const override { return m_id; }
    std::string getVersion() const override { return "1.0.0"; }
    void onInit() override {}
    void onActivate() override {}
    void showMessage(const std::string& /* message */, const std::string& /* type */ = "info") override {
        m_message_count++;
    }
    void onGoalReached() override {}

    int getMessageCount() const { return m_message_count; }

private:
    std::string m_id;
    int m_message_count;
};

// =============================================================================
// Test Cases
// =============================================================================

TEST_CASE("ExtensionPointRegistry is a singleton", "[extension-points][singleton]") {
    ExtensionPointRegistry::getInstance().clearAll(); // Reset state

    SECTION("getInstance() returns the same instance") {
        auto& registry1 = ExtensionPointRegistry::getInstance();
        auto& registry2 = ExtensionPointRegistry::getInstance();
        REQUIRE(&registry1 == &registry2);
    }
}

TEST_CASE("Plugin registration basic operations", "[extension-points][registration]") {
    auto& registry = ExtensionPointRegistry::getInstance();
    registry.clearAll();

    SECTION("Register a plugin successfully") {
        auto plugin = std::make_shared<TestPlugin>("my-plugin");
        registry.registerPlugin(plugin);

        REQUIRE(registry.hasPlugin("my-plugin"));
        REQUIRE(std::dynamic_pointer_cast<TestPlugin>(registry.getPlugin("my-plugin")) != nullptr);
    }

    SECTION("Plugin initialization is called on registration") {
        auto plugin = std::make_shared<TestPlugin>("init-test");
        registry.registerPlugin(plugin);

        REQUIRE(plugin->wasInitCalled());
    }

    SECTION("Cannot register null plugin") {
        REQUIRE_THROWS_AS(registry.registerPlugin(nullptr), std::invalid_argument);
    }

    SECTION("Cannot register plugin with empty ID") {
        auto plugin = std::make_shared<TestPlugin>("");
        REQUIRE_THROWS_AS(registry.registerPlugin(plugin), std::invalid_argument);
    }

    SECTION("Unregister existing plugin") {
        auto plugin = std::make_shared<TestPlugin>("remove-me");
        registry.registerPlugin(plugin);
        REQUIRE(registry.hasPlugin("remove-me"));

        REQUIRE(registry.unregisterPlugin("remove-me"));
        REQUIRE_FALSE(registry.hasPlugin("remove-me"));
    }

    SECTION("Unregister non-existent plugin returns false") {
        REQUIRE_FALSE(registry.unregisterPlugin("non-existent"));
    }
}

TEST_CASE("Type-safe plugin retrieval", "[extension-points][type-casting]") {
    auto& registry = ExtensionPointRegistry::getInstance();
    registry.clearAll();

    SECTION("Retrieve plugin as specific interface type") {
        auto exporter = std::make_shared<TestExporter>("my-exporter");
        registry.registerPlugin(exporter);

        // Retrieve as IExporter
        auto retrieved = registry.getPluginAs<IExporter>("my-exporter");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->exportDocument("pdf", "test.pdf"));

        // Retrieved as wrong type returns nullptr
        auto wrong_type = registry.getPluginAs<IAssistant>("my-exporter");
        REQUIRE(wrong_type == nullptr);
    }

    SECTION("Get plugins of specific type") {
        auto exporter1 = std::make_shared<TestExporter>("exporter-1");
        auto exporter2 = std::make_shared<TestExporter>("exporter-2");
        auto assistant = std::make_shared<TestAssistant>("assistant-1");

        registry.registerPlugin(exporter1);
        registry.registerPlugin(exporter2);
        registry.registerPlugin(assistant);

        auto exporters = registry.getPluginsOfType<IExporter>();
        REQUIRE(exporters.size() == 2);

        auto assistants = registry.getPluginsOfType<IAssistant>();
        REQUIRE(assistants.size() == 1);

        auto panels = registry.getPluginsOfType<IPanelProvider>();
        REQUIRE(panels.size() == 0);
    }
}

TEST_CASE("Registry queries", "[extension-points][queries]") {
    auto& registry = ExtensionPointRegistry::getInstance();
    registry.clearAll();

    SECTION("Get all plugins") {
        auto p1 = std::make_shared<TestPlugin>("p1");
        auto p2 = std::make_shared<TestPlugin>("p2");
        registry.registerPlugin(p1);
        registry.registerPlugin(p2);

        auto all = registry.getAllPlugins();
        REQUIRE(all.size() == 2);
    }

    SECTION("Check plugin existence") {
        auto plugin = std::make_shared<TestPlugin>("exists");
        registry.registerPlugin(plugin);

        REQUIRE(registry.hasPlugin("exists"));
        REQUIRE_FALSE(registry.hasPlugin("not-exists"));
    }

    SECTION("Clear all plugins") {
        registry.registerPlugin(std::make_shared<TestPlugin>("p1"));
        registry.registerPlugin(std::make_shared<TestPlugin>("p2"));
        REQUIRE(registry.getAllPlugins().size() == 2);

        registry.clearAll();
        REQUIRE(registry.getAllPlugins().size() == 0);
    }
}

TEST_CASE("Thread-safety of plugin registry", "[extension-points][thread-safety]") {
    auto& registry = ExtensionPointRegistry::getInstance();
    registry.clearAll();

    SECTION("Concurrent registration is safe") {
        const int NUM_THREADS = 10;
        const int PLUGINS_PER_THREAD = 10;
        std::vector<std::thread> threads;

        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([t, &registry]() {
                for (int p = 0; p < PLUGINS_PER_THREAD; ++p) {
                    std::string id = "plugin-" + std::to_string(t) + "-" + std::to_string(p);
                    auto plugin = std::make_shared<TestPlugin>(id);
                    registry.registerPlugin(plugin);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        REQUIRE(registry.getAllPlugins().size() == NUM_THREADS * PLUGINS_PER_THREAD);
    }

    SECTION("Concurrent queries during registration") {
        registry.clearAll();

        std::vector<std::thread> threads;
        volatile bool keep_running = true;

        // Registration thread
        threads.emplace_back([&registry, &keep_running]() {
            for (int i = 0; i < 50 && keep_running; ++i) {
                auto plugin = std::make_shared<TestPlugin>("plugin-" + std::to_string(i));
                registry.registerPlugin(plugin);
            }
        });

        // Query threads
        for (int t = 0; t < 3; ++t) {
            threads.emplace_back([&registry, &keep_running]() {
                while (keep_running) {
                    registry.getAllPlugins();
                    registry.hasPlugin("some-plugin");
                }
            });
        }

        threads[0].join(); // Wait for registration to complete
        keep_running = false;
        for (size_t i = 1; i < threads.size(); ++i) {
            threads[i].join();
        }

        REQUIRE(registry.getAllPlugins().size() == 50);
    }
}

TEST_CASE("Plugin replacement", "[extension-points][replacement]") {
    auto& registry = ExtensionPointRegistry::getInstance();
    registry.clearAll();

    SECTION("Registering plugin with same ID replaces previous") {
        auto plugin1 = std::make_shared<TestPlugin>("same-id", "1.0.0");
        registry.registerPlugin(plugin1);

        auto plugin2 = std::make_shared<TestPlugin>("same-id", "2.0.0");
        registry.registerPlugin(plugin2);

        auto all = registry.getAllPlugins();
        REQUIRE(all.size() == 1);

        auto retrieved = registry.getPlugin("same-id");
        REQUIRE(retrieved->getVersion() == "2.0.0");
    }
}
