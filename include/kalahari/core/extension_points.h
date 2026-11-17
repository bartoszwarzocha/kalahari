// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file extension_points.h
/// @brief Plugin extension point interfaces for Kalahari
///
/// This file defines the abstract interfaces that plugins can implement to extend
/// Kalahari's functionality. Extension points follow the Strategy and Observer patterns.
///
/// **Architecture:**
/// - IPlugin: Base interface all plugins must implement
/// - IExporter: Export documents to various formats (DOCX, PDF, Markdown)
/// - IImporter: Read the files of other formats into the texts of a book (File > Import)
/// - IPanelProvider: Add panels of their own (a widget, or a page of rich text)
/// - ICommandProvider: Add commands to the menus
/// - IAssistant: Hear of the moments of the writer's work (a goal reached)
/// - ExtensionPointRegistry: Central registry for plugin registration
///
/// The interfaces derive from IPlugin virtually, so one plugin can implement several of them
/// (a plugin written in Python is one core::PythonPlugin implementing them all).
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

/// @brief A format an exporter writes, as File > Export lists it
struct ExportFormatInfo {
    std::string id;         ///< Its identifier, which exportDocument() gets (e.g. "pdf")
    std::string name;       ///< Its name in the list of the formats (e.g. "PDF document")
    std::string extension;  ///< The extension of its files, without the dot (e.g. "pdf")
};

/// @brief Export plugin interface
///
/// Plugins implementing this interface can export documents to various formats.
/// Used for creating export functionality (DOCX, PDF, Markdown, etc.).
///
/// The window of File > Export lists the formats of getFormats() after the program's own
/// (DOCX and Markdown); a format with options of its own gets a tab of the window from
/// createOptionsPage(), shown while the format is chosen.
class IExporter : public virtual IPlugin {
public:
    /// @brief Virtual destructor
    ~IExporter() override = default;

    /// @brief The formats the exporter writes, for the list of the formats of File > Export
    ///
    /// @return The formats; the default, none, keeps the exporter out of the list
    virtual std::vector<ExportFormatInfo> getFormats() const { return {}; }

    /// @brief The options of @p format: a page that the window of the export shows as a tab of
    /// its own while the format is chosen
    ///
    /// The window owns the page and deletes it when it closes; the exporter reads the options
    /// from it in exportDocument(), which the window calls while it is open.
    ///
    /// @param format The id of one of the formats of getFormats()
    /// @param parent The parent of the page
    /// @return The page; the default, nullptr: the format has no options of its own
    virtual QWidget* createOptionsPage(const std::string& /*format*/, QWidget* /*parent*/) {
        return nullptr;
    }

    /// @brief Export document to specific format
    ///
    /// @param format Export format identifier (e.g., "pdf", "docx", "markdown")
    /// @param filepath Destination file path for the exported document
    /// @return true if export succeeded, false otherwise
    /// @throws std::runtime_error if export fails with critical error
    virtual bool exportDocument(const std::string& format,
                               const std::string& filepath) = 0;
};

/// @brief A format an importer reads, as File > Import lists it
struct ImportFormatInfo {
    std::string id;    ///< Its identifier, which importDocument() gets (e.g. "fountain")
    std::string name;  ///< Its name in the list of the formats (e.g. "Fountain screenplay")
    std::vector<std::string> extensions;  ///< The extensions of its files, without the dots
                                          ///< (e.g. "fountain", "spmd")
};

/// @brief A text of a file, as an importer reads it
struct ImportTextInfo {
    /// Its title in the book. Empty for the first text: the text before the titles, which
    /// becomes a title page when it is short; empty for another: the name of the file
    std::string title;
    /// What is in it, in Markdown (UTF-8, GitHub Flavored, with footnotes [^1]): paragraphs,
    /// emphasis, its own headings (they stay in the text), lists, tables, quotes and images
    /// whose paths are relative to the folder of the file
    std::string markdown;
};

/// @brief Import plugin interface
///
/// Plugins implementing this interface read the files of formats of their own into the texts
/// of a book. File > Import > Other Formats chooses the files of the formats of getFormats(),
/// and the window of the import lists them after the program's own (DOCX, Markdown and plain
/// text). The plugin divides a file into its texts and gives their titles; the writer chooses
/// their kinds and their place in the book. A format with options of its own gets a tab of the
/// window from createOptionsPage(), shown while the format is chosen.
class IImporter : public virtual IPlugin {
public:
    /// @brief Virtual destructor
    ~IImporter() override = default;

    /// @brief The formats the importer reads, for File > Import
    ///
    /// @return The formats; a format needs its id, its name and an extension. The default,
    ///         none, keeps the importer out of File > Import
    virtual std::vector<ImportFormatInfo> getFormats() const { return {}; }

    /// @brief The options of @p format: a page that the window of the import shows as a tab of
    /// its own while the format is chosen
    ///
    /// The window owns the page and deletes it when it closes; the importer reads the options
    /// from it in importDocument(), which the window calls while it is open (again whenever
    /// the files are read again).
    ///
    /// @param format The id of one of the formats of getFormats()
    /// @param parent The parent of the page
    /// @return The page; the default, nullptr: the format has no options of its own
    virtual QWidget* createOptionsPage(const std::string& /*format*/, QWidget* /*parent*/) {
        return nullptr;
    }

    /// @brief Read the file @p filepath of the format @p format
    ///
    /// @param format The id of one of the formats of getFormats()
    /// @param filepath The file
    /// @return Its texts, in their order in the book
    /// @throws std::runtime_error with why the file cannot be read; the window says it
    virtual std::vector<ImportTextInfo> importDocument(const std::string& format,
                                                       const std::string& filepath) = 0;
};

/// @brief A panel of a plugin, as View > Panels lists it
struct PluginPanelInfo {
    std::string id;     ///< Its identifier, unique among the panels of the plugin (e.g. "lengths")
    std::string title;  ///< Its name in View > Panels and on its tab, in the plugin's words
    std::string icon;   ///< An icon of the program (e.g. "view.statistics") or the absolute
                        ///< path of an SVG file of the plugin; empty: the icon of the plugins
};

/// @brief Panel provider interface
///
/// Plugins implementing this interface add panels of their own to every book window: each
/// panel of getPanels() gets a toggle in View > Panels and docks with the panels on the right,
/// closed until the writer opens it.
///
/// A panel is either a widget of the plugin (createPanel(), for plugins written in C++) or,
/// when createPanel() gives none, a page the program shows in the colors of its theme: the
/// rich text of panelContent() (Qt's subset of HTML), read again whenever the panel is shown
/// or refreshed. A link of the page whose address is not a web address (http, https) goes to
/// onPanelLink(), so a page can have buttons of its own.
class IPanelProvider : public virtual IPlugin {
public:
    /// @brief Virtual destructor
    ~IPanelProvider() override = default;

    /// @brief The panels of the plugin, read once when it is loaded
    virtual std::vector<PluginPanelInfo> getPanels() = 0;

    /// @brief A widget of the plugin for the panel @p panelId
    ///
    /// @param panelId The id of one of the panels of getPanels()
    /// @param parent The parent of the widget; the window owns the widget
    /// @return The widget; the default, nullptr: the program shows panelContent()
    virtual QWidget* createPanel(const std::string& /*panelId*/, QWidget* /*parent*/) {
        return nullptr;
    }

    /// @brief The page of the panel @p panelId, as rich text
    /// @throws std::runtime_error when the plugin fails; the panel shows why
    virtual std::string panelContent(const std::string& /*panelId*/) { return {}; }

    /// @brief The writer clicked the link @p link of the page of the panel @p panelId
    /// @throws std::runtime_error when the plugin fails
    virtual void onPanelLink(const std::string& /*panelId*/, const std::string& /*link*/) {}
};

/// @brief A command of a plugin, as the menus and Settings > Keyboard Shortcuts show it
struct PluginCommandInfo {
    std::string id;        ///< Its identifier, unique among the commands of the plugin
    std::string label;     ///< Its name in the menu, in the plugin's words; "&" marks the
                           ///< access key, "&&" is an ampersand
    std::string tooltip;   ///< What it does, for the tooltip and the status bar
    std::string menu;      ///< Where it goes: a menu of the program by its technical path
                           ///< ("TOOLS", "TOOLS/Statistics"); empty, or a menu the program
                           ///< does not have: Tools > Plugins > the name of the plugin
    std::string shortcut;  ///< Its keys ("Ctrl+Shift+L"); empty: none. Keys the program or
                           ///< the system has, or another command, are not given to it
    std::string icon;      ///< An icon of the program or the absolute path of an SVG file of
                           ///< the plugin; empty: the icon of the plugins
    bool needsBook = false;  ///< Greyed out while no book is open in the window
};

/// @brief Command provider interface
///
/// Plugins implementing this interface add commands to the menus of every book window. The
/// commands are read once when the plugin is loaded (getCommands()); choosing one in a menu,
/// or pressing its keys, calls runCommand() with its id. The writer can change their keys in
/// Settings > Keyboard Shortcuts like those of the program's own commands.
class ICommandProvider : public virtual IPlugin {
public:
    /// @brief Virtual destructor
    ~ICommandProvider() override = default;

    /// @brief The commands of the plugin, read once when it is loaded
    virtual std::vector<PluginCommandInfo> getCommands() = 0;

    /// @brief The writer chose the command @p commandId
    /// @throws std::runtime_error when the plugin fails; the program shows why
    virtual void runCommand(const std::string& commandId) = 0;
};

/// @brief Graphical assistant interface
///
/// Plugins implementing this interface can provide graphical assistant personalities.
/// Assistants interact with users through messages, achievements, and encouragement.
class IAssistant : public virtual IPlugin {
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
    /// The program calls it when the writer reaches the daily goal of Settings > Statistics
    /// and Goals, once a day. Use this to trigger celebration animations, achievements, or
    /// encouragement.
    virtual void onGoalReached() = 0;

    /// @brief Called when user starts a new writing session
    /// @note Not called yet: the program has no writing sessions
    virtual void onSessionStart() {}

    /// @brief Called when user ends a writing session
    /// @note Not called yet: the program has no writing sessions
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
