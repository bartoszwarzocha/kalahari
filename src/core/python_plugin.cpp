// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file python_plugin.cpp
/// @brief A plugin written in Python as the extension points see it

#include "kalahari/core/python_plugin.h"

#include "kalahari/core/logger.h"
#include "kalahari/core/plugin_api.h"
#include "kalahari/core/plugin_host.h"

#include <pybind11/stl.h>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

namespace py = pybind11;

namespace kalahari::core {

namespace {

/// The plugin's words of an exception it raised
std::runtime_error pluginError(const std::string& pluginId, const py::error_already_set& error) {
    return std::runtime_error(pluginId + ": " + error.what());
}

/// The objects a method of the plugin gives, when it has the method
py::list listOf(const py::object& instance, const char* method) {
    if (!py::hasattr(instance, method)) {
        return py::list();
    }
    py::object result = instance.attr(method)();
    if (result.is_none()) {
        return py::list();
    }
    return py::list(result);
}

}  // namespace

PythonPlugin::PythonPlugin(std::string id, std::string version, std::filesystem::path folder,
                           std::shared_ptr<py::object> instance)
    : m_id(std::move(id)),
      m_version(std::move(version)),
      m_folder(std::move(folder)),
      m_instance(std::move(instance)) {
    readExtensions();
}

PythonPlugin::~PythonPlugin() {
    if (Py_IsInitialized() == 0) {
        // The interpreter is gone at the exit of the program: dropping a reference would crash
        for (py::object& each : m_commands) {
            each.release();
        }
        for (py::object& each : m_panels) {
            each.release();
        }
        if (m_instance) {
            m_instance->release();
        }
        return;
    }
    try {
        py::gil_scoped_acquire gil;
        m_commands.clear();
        m_panels.clear();
        m_instance.reset();
    } catch (...) {
        Logger::getInstance().error("PythonPlugin: '{}' not released", m_id);
    }
}

void PythonPlugin::readExtensions() {
    py::gil_scoped_acquire gil;
    // The classes of the API are known once the module is imported
    py::module_::import("kalahari_api");
    const py::object& instance = *m_instance;
    auto& logger = Logger::getInstance();

    try {
        std::set<std::string> ids;
        for (py::handle item : listOf(instance, "get_commands")) {
            if (!py::isinstance<ApiCommand>(item)) {
                logger.warn("Plugin '{}': get_commands() gave something else than a Command",
                            m_id);
                continue;
            }
            const auto& command = item.cast<const ApiCommand&>();
            if (command.id.empty() || command.label.empty() || !ids.insert(command.id).second) {
                logger.warn("Plugin '{}': a command without an id or a label, or twice: '{}'",
                            m_id, command.id);
                continue;
            }
            m_commands.push_back(py::reinterpret_borrow<py::object>(item));
            m_commandInfos.push_back({command.id, command.label, command.tooltip, command.menu,
                                      command.shortcut, iconPath(command.icon),
                                      command.needsBook});
        }
    } catch (const py::error_already_set& error) {
        logger.error("Plugin '{}': get_commands() failed: {}", m_id, error.what());
    }

    try {
        std::set<std::string> ids;
        for (py::handle item : listOf(instance, "get_panels")) {
            if (!py::isinstance<ApiPanel>(item)) {
                logger.warn("Plugin '{}': get_panels() gave something else than a Panel", m_id);
                continue;
            }
            auto& panel = item.cast<ApiPanel&>();
            if (panel.id.empty() || panel.title.empty() || !ids.insert(panel.id).second) {
                logger.warn("Plugin '{}': a panel without an id or a title, or twice: '{}'", m_id,
                            panel.id);
                continue;
            }
            panel.owner = m_id;
            m_panels.push_back(py::reinterpret_borrow<py::object>(item));
            m_panelInfos.push_back({panel.id, panel.title, iconPath(panel.icon)});
        }
    } catch (const py::error_already_set& error) {
        logger.error("Plugin '{}': get_panels() failed: {}", m_id, error.what());
    }

    try {
        if (py::hasattr(instance, "export_document")) {
            std::set<std::string> ids;
            for (py::handle item : listOf(instance, "get_export_formats")) {
                if (!py::isinstance<ApiExportFormat>(item)) {
                    logger.warn("Plugin '{}': get_export_formats() gave something else than an "
                                "ExportFormat",
                                m_id);
                    continue;
                }
                const auto& format = item.cast<const ApiExportFormat&>();
                if (format.id.empty() || format.name.empty() || !ids.insert(format.id).second) {
                    logger.warn("Plugin '{}': a format without an id or a name, or twice: '{}'",
                                m_id, format.id);
                    continue;
                }
                m_formats.push_back({format.id, format.name, format.extension});
            }
        }
    } catch (const py::error_already_set& error) {
        logger.error("Plugin '{}': get_export_formats() failed: {}", m_id, error.what());
    }

    m_hears = py::hasattr(instance, "on_goal_reached");
}

std::string PythonPlugin::iconPath(const std::string& icon) const {
    if (std::filesystem::path(icon).extension() != ".svg") {
        return icon;  // An icon of the program, or none
    }
    // A file of the plugin; one outside its folder is none
    const std::filesystem::path file = (m_folder / std::filesystem::path(icon)).lexically_normal();
    const std::filesystem::path folder = m_folder.lexically_normal();
    const auto [end, rest] = std::mismatch(folder.begin(), folder.end(), file.begin(), file.end());
    if (end != folder.end()) {
        Logger::getInstance().warn("Plugin '{}': the icon '{}' is outside the plugin", m_id, icon);
        return {};
    }
    return file.string();
}

void PythonPlugin::runCommand(const std::string& commandId) {
    py::gil_scoped_acquire gil;
    for (const py::object& each : m_commands) {
        const auto& command = each.cast<const ApiCommand&>();
        if (command.id != commandId) {
            continue;
        }
        try {
            if (!command.execute.is_none()) {
                command.execute();
            }
        } catch (const py::error_already_set& error) {
            throw pluginError(m_id, error);
        }
        return;
    }
    throw std::runtime_error(m_id + ": no command '" + commandId + "'");
}

std::string PythonPlugin::panelContent(const std::string& panelId) {
    py::gil_scoped_acquire gil;
    for (const py::object& each : m_panels) {
        const auto& panel = each.cast<const ApiPanel&>();
        if (panel.id != panelId) {
            continue;
        }
        try {
            if (panel.content.is_none()) {
                return {};
            }
            py::object page = panel.content();
            return page.is_none() ? std::string() : std::string(py::str(page));
        } catch (const py::error_already_set& error) {
            throw pluginError(m_id, error);
        }
    }
    throw std::runtime_error(m_id + ": no panel '" + panelId + "'");
}

void PythonPlugin::onPanelLink(const std::string& panelId, const std::string& link) {
    py::gil_scoped_acquire gil;
    for (const py::object& each : m_panels) {
        const auto& panel = each.cast<const ApiPanel&>();
        if (panel.id != panelId) {
            continue;
        }
        try {
            if (!panel.onLink.is_none()) {
                panel.onLink(link);
            }
        } catch (const py::error_already_set& error) {
            throw pluginError(m_id, error);
        }
        return;
    }
}

bool PythonPlugin::exportDocument(const std::string& format, const std::string& filepath) {
    py::gil_scoped_acquire gil;
    if (!py::hasattr(*m_instance, "export_document")) {
        return false;
    }
    try {
        const PluginBook* book = PluginHost::getInstance().exportBook();
        py::object written = m_instance->attr("export_document")(
            format, filepath, book != nullptr ? py::cast(*book) : py::none());
        // Nothing said is done; False is a failure
        return written.is_none() || written.cast<bool>();
    } catch (const py::error_already_set& error) {
        throw pluginError(m_id, error);
    } catch (const py::cast_error&) {
        throw std::runtime_error(m_id + ": export_document() gave something else than True "
                                        "or False");
    }
}

void PythonPlugin::showMessage(const std::string& message, const std::string& messageType) {
    py::gil_scoped_acquire gil;
    if (!py::hasattr(*m_instance, "show_message")) {
        return;
    }
    try {
        m_instance->attr("show_message")(message, messageType);
    } catch (const py::error_already_set& error) {
        throw pluginError(m_id, error);
    }
}

void PythonPlugin::onGoalReached() {
    if (!m_hears) {
        return;
    }
    py::gil_scoped_acquire gil;
    try {
        m_instance->attr("on_goal_reached")();
    } catch (const py::error_already_set& error) {
        throw pluginError(m_id, error);
    }
}

PluginContributions PythonPlugin::contributions() const {
    return {m_commandInfos, m_panelInfos, m_formats, m_hears};
}

}  // namespace kalahari::core
