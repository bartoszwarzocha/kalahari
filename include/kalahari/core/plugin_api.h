// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plugin_api.h
/// @brief The kalahari_api module of the Python plugins: its classes and its contents
///
/// The module is defined once (definePluginApi()) and made twice: as a module built into the
/// program (PYBIND11_EMBEDDED_MODULE in plugin_api_embedded.cpp), which the plugins import in
/// the program's own interpreter, and as the extension module of src/bindings, which Python
/// scripts import.

#pragma once

#include <pybind11/pybind11.h>

#include <string>

namespace kalahari::core {

/// @brief kalahari_api.Command: a command of a plugin (core::PluginCommandInfo and its action)
struct ApiCommand {
    std::string id;
    std::string label;
    pybind11::object execute;  ///< Called without arguments when the writer chooses it
    std::string tooltip;
    std::string menu;
    std::string shortcut;
    std::string icon;  ///< An icon of the program or an SVG file of the plugin (relative path)
    bool needsBook = false;
};

/// @brief kalahari_api.Panel: a panel of a plugin (core::PluginPanelInfo and its page)
struct ApiPanel {
    std::string id;
    std::string title;
    pybind11::object content;  ///< Called without arguments; gives the page as rich text
    pybind11::object onLink;   ///< Called with the address of a link clicked; may be None
    std::string icon;
    std::string owner;  ///< The id of the plugin, set by the program when it reads the panel

    /// @brief Read the page of the panel again, in every window
    void refresh() const;
};

/// @brief kalahari_api.ExportFormat: a format a plugin writes (core::ExportFormatInfo)
struct ApiExportFormat {
    std::string id;
    std::string name;
    std::string extension;
};

/// @brief Fill the module @p module with the API of the plugins
void definePluginApi(pybind11::module_& module);

/// @brief The version of the API of the plugins: the "api_version" a manifest asks for
inline constexpr const char* PLUGIN_API_VERSION = "1.0";

}  // namespace kalahari::core
