/// @file extension_points.h
/// @brief Plugin extension point interfaces for Kalahari
///
/// This file defines the abstract interfaces that plugins can implement to extend
/// Kalahari's functionality. Extension points follow the Strategy and Observer patterns.
///
/// **Architecture:**
/// - IPlugin: Base interface all plugins must implement
/// - IExporter: Export documents to various formats (DOCX, PDF, Markdown)
/// - IPanelProvider: Add custom dockable UI panels (Qt6 QWidget-based)
/// - ICommandProvider: Register custom commands for menus/toolbars (Qt6 Command Registry)
/// - IAssistant: Provide graphical assistant personalities
/// - ExtensionPointRegistry: Central registry for plugin registration
///
/// Example usage:
/// @code
/// class MyExporter : public IExporter {
///     std::string getPluginId() const override { return "my-exporter"; }
///     std::string getVersion() const override { return "1.0.0"; }
///     void onInit() override { /* initialization */ }
///     void onActivate() override { /* activation */ }
///     bool exportDocument(const std::string& format,
///                        const std::string& filepath) override {
///         return true;
///     }
/// };
///
/// auto plugin = std::make_shared<MyExporter>();
/// ExtensionPointRegistry::getInstance().registerPlugin(plugin);
/// @endcode

#pragma once

#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <vector>

// Forward declarations
class QWidget;

namespace kalahari {

// Forward declaration for Command structure (avoid circular dependency)
namespace gui {
    struct Command;
}

namespace core {

/// @brief Base interface for all plugins
///
/// All plugins must implement this interface. It defines the basic lifecycle
/// and metadata methods that every plugin needs to provide.
class IPlugin {
public:
    /// @brief Virtual destructor for proper cleanup
    virtual ~IPlugin() = default;

    /// @brief Get unique plugin identifier
    ///
    /// This ID must be unique across all installed plugins and is used
    /// to identify the plugin in the registry and during loading.
    ///
    /// @return Unique plugin identifier (e.g., "kalahari-lion-assistant")
    virtual std::string getPluginId() const = 0;

    /// @brief Get plugin version
    ///
    /// Should follow semantic versioning (e.g., "1.2.3")
    ///
    /// @return Plugin version string
    virtual std::string getVersion() const = 0;

    /// @brief Plugin initialization hook
    ///
    /// Called once when the plugin is first loaded. Use this to initialize
    /// resources, connect to events, or set up the plugin state.
    ///
    /// @throws std::runtime_error if initialization fails
    virtual void onInit() = 0;

    /// @brief Plugin activation hook
    ///
    /// Called when the user enables/activates the plugin. Use this to
    /// register UI components, start processing, or enable features.
    ///
    /// @throws std::runtime_error if activation fails
    virtual void onActivate() = 0;

    /// @brief Plugin deactivation hook
    ///
    /// Called when the user disables/deactivates the plugin. Use this to
    /// unregister UI components, stop processing, or disable features.
    /// Default implementation does nothing.
    virtual void onDeactivate() {}
};

/// @brief Export plugin interface
///
/// Plugins implementing this interface can export documents to various formats.
/// Used for creating export functionality (DOCX, PDF, Markdown, etc.).
class IExporter : public IPlugin {
public:
    /// @brief Virtual destructor
    ~IExporter() override = default;

    /// @brief Export document to specific format
    ///
    /// @param format Export format identifier (e.g., "pdf", "docx", "markdown")
    /// @param filepath Destination file path for the exported document
    /// @return true if export succeeded, false otherwise
    /// @throws std::runtime_error if export fails with critical error
    virtual bool exportDocument(const std::string& format,
                               const std::string& filepath) = 0;
};

/// @brief UI panel provider interface (Qt6)
///
/// Plugins implementing this interface can provide custom dockable panels
/// for the main window. Used for adding custom UI panels (research, outline, etc.).
///
/// **Qt6 Integration:**
/// - Returned QWidget* is wrapped in QDockWidget by MainWindow
/// - Ownership transfers to MainWindow (parent manages lifetime)
/// - Use Qt layouts (QVBoxLayout, QHBoxLayout) for panel content
class IPanelProvider : public IPlugin {
public:
    /// @brief Virtual destructor
    ~IPanelProvider() override = default;

    /// @brief Create a dockable panel widget
    ///
    /// The returned widget will be integrated into MainWindow's QDockWidget system.
    /// Implement this to create custom UI panels using Qt6 widgets.
    ///
    /// @param parentWindow Parent QWidget pointer (typically MainWindow)
    /// @return QWidget* pointer to created panel widget
    /// @note Ownership transfers to caller. MainWindow manages widget lifetime.
    /// @note pybind11 automatically handles QWidget* for Python plugins.
    virtual QWidget* createPanel(QWidget* parentWindow) = 0;
};

/// @brief Command provider interface (Qt6 Command Registry Integration)
///
/// Plugins implementing this interface can register custom commands that
/// automatically appear in menus, toolbars, and Command Palette.
///
/// **Qt6 Command Registry:**
/// - Commands are registered with CommandRegistry singleton during plugin activation
/// - Menu/toolbar updates happen automatically via MenuBuilder/ToolbarBuilder
/// - Keyboard shortcuts, icons, and enable/disable callbacks supported
///
/// **Example (C++):**
/// @code
/// class MyPlugin : public ICommandProvider {
///     std::vector<gui::Command> getCommands() override {
///         return {
///             gui::Command{
///                 "myplugin.action",    // id
///                 "My Action",          // label
///                 "Do something cool",  // tooltip
///                 "Plugins",            // category
///                 gui::IconSet(),       // icons
///                 gui::KeyboardShortcut(), // shortcut
///                 [this]() { doAction(); }, // execute callback
///                 nullptr,              // isEnabled
///                 nullptr,              // isChecked
///                 true,                 // showInMenu
///                 true                  // showInToolbar
///             }
///         };
///     }
/// };
/// @endcode
///
/// **Example (Python):**
/// @code
/// class MyPlugin(kalahari_api.ICommandProvider):
///     def get_commands(self):
///         return [
///             kalahari_api.Command(
///                 id="myplugin.action",
///                 label="My Action",
///                 category="Plugins",
///                 execute=self.do_action,
///                 showInMenu=True
///             )
///         ]
/// @endcode
class ICommandProvider : public IPlugin {
public:
    /// @brief Virtual destructor
    ~ICommandProvider() override = default;

    /// @brief Get list of commands to register
    ///
    /// Called once during plugin activation (onActivate()). All returned commands
    /// are automatically registered with CommandRegistry and appear in menus/toolbars.
    ///
    /// **Command Lifecycle:**
    /// 1. Plugin activated → getCommands() called
    /// 2. Commands registered with CommandRegistry
    /// 3. MenuBuilder/ToolbarBuilder query registry → menus/toolbars update
    /// 4. User clicks menu item → Command::execute callback invoked
    ///
    /// @return Vector of Command structures to register
    /// @note Use gui::Command forward-declared type (full definition in command.h)
    virtual std::vector<gui::Command> getCommands() = 0;
};

/// @brief Graphical assistant interface
///
/// Plugins implementing this interface can provide graphical assistant personalities.
/// Assistants interact with users through messages, achievements, and encouragement.
class IAssistant : public IPlugin {
public:
    /// @brief Virtual destructor
    ~IAssistant() override = default;

    /// @brief Show assistant message to user
    ///
    /// @param message Message text to display
    /// @param messageType Type of message (info, warning, congratulation, etc.)
    virtual void showMessage(const std::string& message,
                            const std::string& messageType = "info") = 0;

    /// @brief Called when user reaches a writing goal
    ///
    /// Use this to trigger celebration animations, achievements, or encouragement.
    virtual void onGoalReached() = 0;

    /// @brief Called when user starts a new writing session
    virtual void onSessionStart() {}

    /// @brief Called when user ends a writing session
    virtual void onSessionEnd() {}
};

/// @brief Extension point registry (singleton)
///
/// Central registry for plugin registration and lookup.
/// Thread-safe singleton that manages all registered plugins.
class ExtensionPointRegistry {
public:
    /// @brief Get the singleton instance
    /// @return Reference to ExtensionPointRegistry
    static ExtensionPointRegistry& getInstance();

    // Prevent copying/moving
    ExtensionPointRegistry(const ExtensionPointRegistry&) = delete;
    ExtensionPointRegistry& operator=(const ExtensionPointRegistry&) = delete;

    /// @brief Register a plugin
    ///
    /// Registers a plugin in the registry. If a plugin with the same ID already exists,
    /// it will be replaced. This operation is thread-safe.
    ///
    /// @param plugin Shared pointer to plugin implementing IPlugin interface
    /// @throws std::invalid_argument if plugin is null or getPluginId() returns empty string
    /// @throws std::runtime_error if plugin initialization fails
    void registerPlugin(std::shared_ptr<IPlugin> plugin);

    /// @brief Unregister a plugin by ID
    ///
    /// Removes a plugin from the registry. Thread-safe operation.
    ///
    /// @param pluginId ID of plugin to unregister
    /// @return true if plugin was found and removed, false if not found
    bool unregisterPlugin(const std::string& pluginId);

    /// @brief Get plugin by ID
    ///
    /// Thread-safe lookup of registered plugin.
    ///
    /// @param pluginId ID of plugin to retrieve
    /// @return Shared pointer to plugin, or nullptr if not found
    std::shared_ptr<IPlugin> getPlugin(const std::string& pluginId) const;

    /// @brief Get plugin as specific interface type
    ///
    /// Template helper for type-safe plugin retrieval with interface casting.
    ///
    /// @tparam T Interface type to retrieve (e.g., IExporter, IAssistant)
    /// @param pluginId ID of plugin
    /// @return Pointer to plugin cast to requested interface type, or nullptr
    template<typename T>
    std::shared_ptr<T> getPluginAs(const std::string& pluginId) const {
        auto plugin = getPlugin(pluginId);
        return plugin ? std::dynamic_pointer_cast<T>(plugin) : nullptr;
    }

    /// @brief Get all plugins implementing a specific interface
    ///
    /// Template helper to find all plugins that implement a given interface.
    ///
    /// @tparam T Interface type to search for (e.g., IExporter)
    /// @return Vector of plugins implementing interface T
    template<typename T>
    std::vector<std::shared_ptr<T>> getPluginsOfType() const {
        std::vector<std::shared_ptr<T>> result;
        std::lock_guard<std::mutex> lock(m_mutex);

        for (const auto& [id, plugin] : m_plugins) {
            if (auto typed = std::dynamic_pointer_cast<T>(plugin)) {
                result.push_back(typed);
            }
        }

        return result;
    }

    /// @brief Get all registered plugins
    ///
    /// Returns a snapshot of all registered plugins.
    ///
    /// @return Vector of all plugins
    std::vector<std::shared_ptr<IPlugin>> getAllPlugins() const;

    /// @brief Check if plugin is registered
    ///
    /// @param pluginId ID to check
    /// @return true if plugin with given ID is registered
    bool hasPlugin(const std::string& pluginId) const;

    /// @brief Clear all registered plugins
    ///
    /// Removes all plugins from registry. Use with caution.
    void clearAll();

private:
    /// @brief Private constructor (singleton pattern)
    ExtensionPointRegistry() = default;

public:
    /// @brief Destructor (public for pybind11 compatibility)
    /// @note Should never be called directly - only for RAII cleanup
    ~ExtensionPointRegistry() = default;

private:

    /// Map of plugin ID to plugin instance
    std::map<std::string, std::shared_ptr<IPlugin>> m_plugins;

    /// Mutex for thread-safe access
    mutable std::mutex m_mutex;
};

} // namespace core
} // namespace kalahari
