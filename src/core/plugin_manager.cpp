/// @file plugin_manager.cpp
/// @brief Plugin manager implementation

#include <kalahari/core/plugin_manager.h>
#include <kalahari/core/logger.h>

namespace kalahari {
namespace core {

PluginManager& PluginManager::getInstance() {
    static PluginManager instance;
    return instance;
}

size_t PluginManager::discoverPlugins() {
    std::lock_guard<std::mutex> lock(m_mutex);
    Logger::getInstance().info("PluginManager: Discovering plugins...");
    // Phase 0 Week 3-4: Stub implementation
    // Phase 0 Week 5-6: Actual plugin scanning
    m_plugins.clear();
    Logger::getInstance().info("PluginManager: Plugin discovery complete. Found: {} plugins",
                               m_plugins.size());
    return m_plugins.size();
}

bool PluginManager::loadPlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Logger::getInstance().info("PluginManager: Loading plugin '{}'", pluginId);
    // Phase 0 Week 3-4: Stub implementation
    // Phase 0 Week 5-6: Actual plugin loading from .kplugin files
    Logger::getInstance().info("PluginManager: Plugin '{}' loaded (stub)", pluginId);
    return true;
}

void PluginManager::unloadPlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Logger::getInstance().info("PluginManager: Unloading plugin '{}'", pluginId);
    // Phase 0 Week 3-4: Stub implementation
    // Phase 0 Week 5-6: Actual plugin unloading
    Logger::getInstance().info("PluginManager: Plugin '{}' unloaded (stub)", pluginId);
}

std::vector<PluginMetadata> PluginManager::getDiscoveredPlugins() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_plugins;
}

} // namespace core
} // namespace kalahari
