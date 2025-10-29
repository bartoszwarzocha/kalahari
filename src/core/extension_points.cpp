/// @file extension_points.cpp
/// @brief Implementation of plugin extension point registry

#include "kalahari/core/extension_points.h"
#include "kalahari/core/logger.h"
#include <stdexcept>

namespace kalahari {
namespace core {

ExtensionPointRegistry& ExtensionPointRegistry::getInstance() {
    static ExtensionPointRegistry instance;
    return instance;
}

void ExtensionPointRegistry::registerPlugin(std::shared_ptr<IPlugin> plugin) {
    if (!plugin) {
        throw std::invalid_argument("Cannot register null plugin");
    }

    std::string pluginId = plugin->getPluginId();
    if (pluginId.empty()) {
        throw std::invalid_argument("Plugin ID cannot be empty");
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Log registration
    Logger::getInstance().info("ExtensionPointRegistry: Registering plugin '{}' (v{})",
                              pluginId, plugin->getVersion());

    // Store plugin
    m_plugins[pluginId] = plugin;

    // Call plugin initialization hook
    try {
        plugin->onInit();
        Logger::getInstance().debug("ExtensionPointRegistry: Plugin '{}' initialized successfully",
                                   pluginId);
    } catch (const std::exception& e) {
        // Remove on init failure
        m_plugins.erase(pluginId);
        Logger::getInstance().error("ExtensionPointRegistry: Plugin '{}' initialization failed: {}",
                                   pluginId, e.what());
        throw std::runtime_error(std::string("Plugin initialization failed: ") + e.what());
    }
}

bool ExtensionPointRegistry::unregisterPlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end()) {
        Logger::getInstance().warn("ExtensionPointRegistry: Attempted to unregister unknown plugin '{}'",
                                  pluginId);
        return false;
    }

    // Call deactivation hook before removal
    try {
        it->second->onDeactivate();
    } catch (const std::exception& e) {
        Logger::getInstance().warn("ExtensionPointRegistry: Plugin '{}' deactivation threw exception: {}",
                                  pluginId, e.what());
    }

    m_plugins.erase(it);
    Logger::getInstance().info("ExtensionPointRegistry: Plugin '{}' unregistered", pluginId);
    return true;
}

std::shared_ptr<IPlugin> ExtensionPointRegistry::getPlugin(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_plugins.find(pluginId);
    if (it != m_plugins.end()) {
        return it->second;
    }

    return nullptr;
}

std::vector<std::shared_ptr<IPlugin>> ExtensionPointRegistry::getAllPlugins() const {
    std::vector<std::shared_ptr<IPlugin>> result;
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& [id, plugin] : m_plugins) {
        result.push_back(plugin);
    }

    return result;
}

bool ExtensionPointRegistry::hasPlugin(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_plugins.find(pluginId) != m_plugins.end();
}

void ExtensionPointRegistry::clearAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Call deactivation hooks for all plugins
    for (auto& [id, plugin] : m_plugins) {
        try {
            plugin->onDeactivate();
        } catch (const std::exception& e) {
            Logger::getInstance().warn("ExtensionPointRegistry: Plugin '{}' deactivation threw: {}",
                                      id, e.what());
        }
    }

    m_plugins.clear();
    Logger::getInstance().info("ExtensionPointRegistry: All plugins cleared");
}

} // namespace core
} // namespace kalahari
