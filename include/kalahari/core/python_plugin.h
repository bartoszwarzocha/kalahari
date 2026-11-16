// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file python_plugin.h
/// @brief A plugin written in Python as the extension points see it
///
/// PluginManager loads the class of a plugin's entry point and wraps its object in a
/// PythonPlugin, which it registers in ExtensionPointRegistry. The object needs no base class:
/// each extension point is a method it may have.
///
/// | Method of the plugin's class            | Extension point                          |
/// |-----------------------------------------|------------------------------------------|
/// | get_commands() -> [kalahari_api.Command] | ICommandProvider: commands in the menus  |
/// | get_panels() -> [kalahari_api.Panel]     | IPanelProvider: panels in View > Panels  |
/// | get_export_formats() -> [ExportFormat]   | IExporter: formats of File > Export      |
/// | export_document(format, path, book)      | IExporter: writes the file               |
/// | on_goal_reached()                        | IAssistant: the daily goal reached       |
/// | show_message(message, kind)              | IAssistant: a message for the assistant  |
/// | get_settings() -> [kalahari_api.Setting] | its page in the Settings window          |
/// | on_settings_changed()                    | the writer changed its settings          |
///
/// Every call takes the GIL; an exception of the plugin becomes a std::runtime_error with the
/// plugin's words, which the program shows to the writer.

#pragma once

#include "kalahari/core/extension_points.h"
#include "kalahari/core/plugin_settings.h"

// Qt's "slots" keyword would break Python's headers when Qt's come first
#pragma push_macro("slots")
#undef slots
#include <pybind11/pybind11.h>
#pragma pop_macro("slots")

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace kalahari::core {

/// @brief What a plugin added to the program, as the window of the plugins lists it
struct PluginContributions {
    std::vector<PluginCommandInfo> commands;
    std::vector<PluginPanelInfo> panels;
    std::vector<ExportFormatInfo> exportFormats;
    std::vector<PluginSettingInfo> settings;  ///< Shown on its page in the Settings window
    bool assistant = false;  ///< It hears of the goals reached (on_goal_reached)
};

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"  // pybind11's types are hidden, the class is not
#endif

/// @brief A plugin written in Python, behind every extension point
class PythonPlugin final : public IExporter,
                           public ICommandProvider,
                           public IPanelProvider,
                           public IAssistant {
public:
    /// @param id The id of the plugin (its manifest)
    /// @param version Its version (its manifest)
    /// @param folder The folder its archive is extracted to: the files its commands and panels
    ///        name (icons) are there
    /// @param instance The object of the class of its entry point
    PythonPlugin(std::string id, std::string version, std::filesystem::path folder,
                 std::shared_ptr<pybind11::object> instance);

    /// @brief Releases the plugin's Python objects under the GIL (or leaks them when the
    ///        interpreter is gone at the exit of the program)
    ~PythonPlugin() override;

    PythonPlugin(const PythonPlugin&) = delete;
    PythonPlugin& operator=(const PythonPlugin&) = delete;

    // IPlugin: the plugin was initialised and activated by PluginManager already
    [[nodiscard]] std::string getPluginId() const override { return m_id; }
    [[nodiscard]] std::string getVersion() const override { return m_version; }
    void onInit() override {}
    void onActivate() override {}

    // ICommandProvider
    std::vector<PluginCommandInfo> getCommands() override { return m_commandInfos; }
    void runCommand(const std::string& commandId) override;

    // IPanelProvider
    std::vector<PluginPanelInfo> getPanels() override { return m_panelInfos; }
    std::string panelContent(const std::string& panelId) override;
    void onPanelLink(const std::string& panelId, const std::string& link) override;

    // IExporter
    [[nodiscard]] std::vector<ExportFormatInfo> getFormats() const override {
        return m_formats;
    }
    bool exportDocument(const std::string& format, const std::string& filepath) override;

    // IAssistant
    void showMessage(const std::string& message, const std::string& messageType) override;
    void onGoalReached() override;

    /// @brief The writer changed the settings of the plugin (its page in the Settings window)
    void settingsChanged();

    /// @brief What the plugin added to the program
    [[nodiscard]] PluginContributions contributions() const;

private:
    /// The commands, panels, formats and settings of the plugin, read once
    void readExtensions();
    void readCommands();
    void readPanels();
    void readFormats();
    void readSettings();
    /// An icon a command or panel names: a file of the plugin becomes its absolute path
    [[nodiscard]] std::string iconPath(const std::string& icon) const;

    std::string m_id;
    std::string m_version;
    std::filesystem::path m_folder;
    std::shared_ptr<pybind11::object> m_instance;
    std::vector<pybind11::object> m_commands;  ///< kalahari_api.Command objects
    std::vector<pybind11::object> m_panels;    ///< kalahari_api.Panel objects
    std::vector<PluginCommandInfo> m_commandInfos;
    std::vector<PluginPanelInfo> m_panelInfos;
    std::vector<ExportFormatInfo> m_formats;
    std::vector<PluginSettingInfo> m_settings;
    bool m_hears = false;  ///< It has on_goal_reached()
};

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

}  // namespace kalahari::core
