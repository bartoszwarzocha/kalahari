// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plugin_api.cpp
/// @brief The kalahari_api module of the Python plugins

#include "kalahari/core/plugin_api.h"

#include "kalahari/core/event_bus.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/plugin_host.h"
#include "kalahari/version.h"

#include <pybind11/functional.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <memory>
#include <utility>

namespace py = pybind11;

namespace kalahari::core {

namespace {

/// @brief Owns a Python callable stored inside a C++ EventBus listener
///
/// The EventBus singleton is destroyed by C++ static destruction, which runs
/// after the Python interpreter has been finalized. Dropping the reference then
/// aborts the process ("PyThreadState_Get: the GIL is released"), so the
/// reference is released under the GIL while Python is alive and leaked otherwise.
class PythonCallback {
public:
    explicit PythonCallback(py::object callable) : m_callable(std::move(callable)) {}

    PythonCallback(const PythonCallback&) = delete;
    PythonCallback& operator=(const PythonCallback&) = delete;

    ~PythonCallback() {
        if (Py_IsInitialized() == 0) {
            m_callable.release();
            return;
        }
        try {
            py::gil_scoped_acquire acquire;
            m_callable = py::object();
        } catch (...) {
            m_callable.release();  // Leaking the reference is the only safe option left
        }
    }

    void operator()(const Event& evt) const { m_callable(evt); }

private:
    py::object m_callable;
};

void defineLogger(py::module_& m) {
    py::class_<Logger>(m, "Logger", "Logging interface for Kalahari plugins")
        .def_static(
            "info", [](const std::string& msg) { Logger::getInstance().info("{}", msg); },
            py::arg("message"), "Log an info-level message")
        .def_static(
            "error", [](const std::string& msg) { Logger::getInstance().error("{}", msg); },
            py::arg("message"), "Log an error-level message")
        .def_static(
            "debug", [](const std::string& msg) { Logger::getInstance().debug("{}", msg); },
            py::arg("message"), "Log a debug-level message")
        .def_static(
            "warn", [](const std::string& msg) { Logger::getInstance().warn("{}", msg); },
            py::arg("message"), "Log a warning-level message");
}

void defineEvents(py::module_& m) {
    py::class_<Event>(m, "Event", "Event data structure for pub/sub communication")
        .def(py::init<const std::string&>(), py::arg("type"), "Create event with type")
        .def(py::init<const std::string&, const py::object&>(), py::arg("type"), py::arg("data"),
             "Create event with type and data")
        .def_readwrite("type", &Event::type, "Event type identifier (e.g., 'document:opened')")
        .def_property(
            "data",
            [](const Event& /* self */) -> py::object {
                // For Python, data is treated as an opaque object: no access to C++ std::any
                return py::none();
            },
            [](Event& self, const py::object& obj) {
                // Store reference in std::any (as void* for simplicity)
                self.data = std::any(reinterpret_cast<uintptr_t>(obj.ptr()));
            },
            "Event data payload (stored as reference)");

    py::class_<EventBus>(m, "EventBus", "Thread-safe event publish/subscribe system")
        .def_static("get_instance", &EventBus::getInstance, py::return_value_policy::reference,
                    "Get EventBus singleton instance")
        .def(
            "subscribe",
            [](EventBus& self, const std::string& eventType, py::object callback) {
                auto holder = std::make_shared<PythonCallback>(std::move(callback));
                EventListener listener = [holder](const Event& evt) {
                    try {
                        py::gil_scoped_acquire acquire;
                        (*holder)(evt);
                    } catch (const py::error_already_set& e) {
                        Logger::getInstance().error(
                            "EventBus: Python callback raised exception: {}", e.what());
                    }
                };
                return self.subscribe(eventType, listener);
            },
            py::arg("event_type"), py::arg("callback"),
            "Subscribe to event type with Python callback; returns a subscription id")
        .def("unsubscribe", &EventBus::unsubscribe, py::arg("subscription_id"),
             "Remove the listener with the given subscription id; returns True if it was found")
        .def("emit", &EventBus::emit, py::arg("event"),
             "Emit event synchronously to all subscribers")
        .def("emit_async", &EventBus::emitAsync, py::arg("event"),
             "Emit event asynchronously (marshalled to GUI thread)")
        .def("has_subscribers", &EventBus::hasSubscribers, py::arg("event_type"),
             "Check if event type has subscribers")
        .def("get_subscriber_count", &EventBus::getSubscriberCount, py::arg("event_type"),
             "Get number of subscribers for event type")
        .def("clear_all", &EventBus::clearAll, "Clear all event subscriptions");
}

void defineExtensions(py::module_& m) {
    py::class_<ApiCommand>(m, "Command",
                           "A command of the plugin: get_commands() gives them to the menus")
        .def(py::init([](std::string id, std::string label, py::object execute,
                         std::string tooltip, std::string menu, std::string shortcut,
                         std::string icon, bool needsBook) {
                 return ApiCommand{std::move(id),       std::move(label), std::move(execute),
                                   std::move(tooltip),  std::move(menu),  std::move(shortcut),
                                   std::move(icon),     needsBook};
             }),
             py::arg("id"), py::arg("label"), py::arg("execute"), py::kw_only(),
             py::arg("tooltip") = "", py::arg("menu") = "", py::arg("shortcut") = "",
             py::arg("icon") = "", py::arg("needs_book") = false)
        .def_readwrite("id", &ApiCommand::id)
        .def_readwrite("label", &ApiCommand::label)
        .def_readwrite("execute", &ApiCommand::execute)
        .def_readwrite("tooltip", &ApiCommand::tooltip)
        .def_readwrite("menu", &ApiCommand::menu)
        .def_readwrite("shortcut", &ApiCommand::shortcut)
        .def_readwrite("icon", &ApiCommand::icon)
        .def_readwrite("needs_book", &ApiCommand::needsBook);

    py::class_<ApiPanel>(m, "Panel",
                         "A panel of the plugin: get_panels() gives them to View > Panels")
        .def(py::init([](std::string id, std::string title, py::object content, py::object onLink,
                         std::string icon) {
                 return ApiPanel{std::move(id),     std::move(title), std::move(content),
                                 std::move(onLink), std::move(icon),  std::string()};
             }),
             py::arg("id"), py::arg("title"), py::arg("content"), py::kw_only(),
             py::arg("on_link") = py::none(), py::arg("icon") = "")
        .def_readwrite("id", &ApiPanel::id)
        .def_readwrite("title", &ApiPanel::title)
        .def_readwrite("content", &ApiPanel::content)
        .def_readwrite("on_link", &ApiPanel::onLink)
        .def_readwrite("icon", &ApiPanel::icon)
        .def_readonly("plugin_id", &ApiPanel::owner)
        .def("refresh", &ApiPanel::refresh, "Read the page of the panel again, in every window");

    py::class_<ApiExportFormat>(m, "ExportFormat",
                                "A format of File > Export: get_export_formats() gives them")
        .def(py::init([](std::string id, std::string name, std::string extension) {
                 return ApiExportFormat{std::move(id), std::move(name), std::move(extension)};
             }),
             py::arg("id"), py::arg("name"), py::arg("extension"))
        .def_readwrite("id", &ApiExportFormat::id)
        .def_readwrite("name", &ApiExportFormat::name)
        .def_readwrite("extension", &ApiExportFormat::extension);

    py::class_<ApiSetting>(m, "Setting",
                           "A setting of the plugin: get_settings() gives them to the page of "
                           "the plugins in the Settings window")
        .def(py::init([](std::string id, std::string label, const py::object& defaultValue,
                         std::string description, const py::object& choices,
                         const py::object& minimum, const py::object& maximum) {
                 ApiSetting setting;
                 setting.info.id = std::move(id);
                 setting.info.label = std::move(label);
                 setting.info.description = std::move(description);
                 setting.info.defaultValue = jsonOf(defaultValue);
                 if (!choices.is_none()) {
                     for (py::handle choice : choices) {
                         const auto pair = py::reinterpret_borrow<py::tuple>(choice);
                         if (pair.size() != 2) {
                             throw py::value_error("a choice is a (value, label) pair");
                         }
                         setting.info.choices.emplace_back(jsonOf(pair[0]),
                                                           py::str(pair[1]).cast<std::string>());
                     }
                 }
                 if (!minimum.is_none()) {
                     setting.info.minimum = minimum.cast<double>();
                 }
                 if (!maximum.is_none()) {
                     setting.info.maximum = maximum.cast<double>();
                 }
                 if (!setting.info.isValid()) {
                     throw py::value_error("Setting '" + setting.info.id +
                                           "': an id, a label and a default of a bool, an int, "
                                           "a float or a str it accepts are needed");
                 }
                 return setting;
             }),
             py::arg("id"), py::arg("label"), py::arg("default"), py::kw_only(),
             py::arg("description") = "", py::arg("choices") = py::none(),
             py::arg("minimum") = py::none(), py::arg("maximum") = py::none())
        .def_property_readonly("id", [](const ApiSetting& setting) { return setting.info.id; })
        .def_property_readonly("label",
                               [](const ApiSetting& setting) { return setting.info.label; })
        .def_property_readonly(
            "default", [](const ApiSetting& setting) { return pythonOf(setting.info.defaultValue); })
        .def_readonly("plugin_id", &ApiSetting::owner)
        .def_property("value", &ApiSetting::value, &ApiSetting::setValue,
                      "The value the writer gave, or the default");
}

void defineBook(py::module_& m) {
    py::class_<PluginText>(m, "Text", "A text of the book, the title of a part, or the contents")
        .def_readonly("kind", &PluginText::kind)
        .def_readonly("id", &PluginText::id)
        .def_readonly("title", &PluginText::title)
        .def_readonly("text", &PluginText::text)
        .def_readonly("kml", &PluginText::kml)
        .def("__repr__", [](const PluginText& text) {
            return "<kalahari_api.Text " + text.kind + " '" + text.title + "'>";
        });

    py::class_<PluginBook>(m, "Book", "A book: its texts in reading order, as they are now")
        .def_readonly("title", &PluginBook::title)
        .def_readonly("author", &PluginBook::author)
        .def_readonly("language", &PluginBook::language)
        .def_readonly("items", &PluginBook::items)
        .def_property_readonly(
            "texts",
            [](const PluginBook& book) {
                std::vector<PluginText> texts;
                for (const PluginText& item : book.items) {
                    if (item.kind == "text") {
                        texts.push_back(item);
                    }
                }
                return texts;
            },
            "The texts alone, without the titles of the parts and the contents")
        .def("__repr__",
             [](const PluginBook& book) { return "<kalahari_api.Book '" + book.title + "'>"; });

    m.def(
        "book", []() { return PluginHost::getInstance().book(); },
        "The book of the window in front, with the unsaved changes; None without a book");
    m.def(
        "front_text", []() { return PluginHost::getInstance().frontText(); },
        "The text in front (of the book or a file outside it); None without one");
}

void defineProgram(py::module_& m) {
    m.def(
        "show_message",
        [](const std::string& title, const std::string& text) {
            PluginHost::getInstance().showMessage(title, text);
        },
        py::arg("title"), py::arg("text"), "Show a message to the writer in a window");
    m.def(
        "program_language", []() { return PluginHost::getInstance().programLanguage(); },
        "The language of the program's windows (\"pl\", \"en\")");
    m.def(
        "program_version", []() { return std::string(VERSION_STRING); },
        "The version of the program (\"0.4.0-alpha\")");
    m.attr("API_VERSION") = PLUGIN_API_VERSION;
}

}  // namespace

void ApiPanel::refresh() const {
    PluginHost::getInstance().refreshPanel(owner, id);
}

py::object ApiSetting::value() const {
    if (owner.empty()) {
        return pythonOf(info.defaultValue);  // Not read by the program yet
    }
    return pythonOf(pluginSettingValue(owner, info));
}

void ApiSetting::setValue(const py::object& value) const {
    if (owner.empty()) {
        throw py::value_error("Setting '" + info.id +
                              "': the program has not read it from get_settings() yet");
    }
    if (!setPluginSettingValue(owner, info, jsonOf(value))) {
        throw py::value_error("Setting '" + info.id + "' does not accept " +
                              std::string(py::repr(value)));
    }
}

nlohmann::json jsonOf(const py::handle& value) {
    if (value.is_none()) {
        return nullptr;
    }
    // bool before int: True is an int in Python
    if (py::isinstance<py::bool_>(value)) {
        return value.cast<bool>();
    }
    if (py::isinstance<py::int_>(value)) {
        return value.cast<std::int64_t>();
    }
    if (py::isinstance<py::float_>(value)) {
        return value.cast<double>();
    }
    if (py::isinstance<py::str>(value)) {
        return value.cast<std::string>();
    }
    if (py::isinstance<py::list>(value) || py::isinstance<py::tuple>(value)) {
        nlohmann::json array = nlohmann::json::array();
        for (py::handle item : value) {
            array.push_back(jsonOf(item));
        }
        return array;
    }
    if (py::isinstance<py::dict>(value)) {
        nlohmann::json object = nlohmann::json::object();
        for (auto [key, item] : py::reinterpret_borrow<py::dict>(value)) {
            if (!py::isinstance<py::str>(key)) {
                throw py::type_error("the keys of a dict are str");
            }
            object[key.cast<std::string>()] = jsonOf(item);
        }
        return object;
    }
    throw py::type_error("not a None, bool, int, float, str, list or dict: " +
                         std::string(py::repr(value)));
}

py::object pythonOf(const nlohmann::json& value) {
    switch (value.type()) {
        case nlohmann::json::value_t::boolean:
            return py::bool_(value.get<bool>());
        case nlohmann::json::value_t::number_integer:
            return py::int_(value.get<std::int64_t>());
        case nlohmann::json::value_t::number_unsigned:
            return py::int_(value.get<std::uint64_t>());
        case nlohmann::json::value_t::number_float:
            return py::float_(value.get<double>());
        case nlohmann::json::value_t::string:
            return py::str(value.get<std::string>());
        case nlohmann::json::value_t::array: {
            py::list list;
            for (const nlohmann::json& item : value) {
                list.append(pythonOf(item));
            }
            return list;
        }
        case nlohmann::json::value_t::object: {
            py::dict dict;
            for (const auto& [key, item] : value.items()) {
                dict[py::str(key)] = pythonOf(item);
            }
            return dict;
        }
        default:
            return py::none();
    }
}

void definePluginApi(PyObject* object) {
    auto module = py::reinterpret_borrow<py::module_>(object);
    module.doc() = "Kalahari API for Python plugins";
    defineLogger(module);
    defineEvents(module);
    defineExtensions(module);
    defineBook(module);
    defineProgram(module);
}

}  // namespace kalahari::core
