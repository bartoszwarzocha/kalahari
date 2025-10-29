/// @file python_bindings.cpp
/// @brief pybind11 bindings for Kalahari core API

#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/event_bus.h>
#include <kalahari/core/extension_points.h>
#include <cstdint>

namespace py = pybind11;

/// @brief Kalahari API module for Python plugins
/// @details Exposes core C++ functionality to Python via pybind11
PYBIND11_MODULE(kalahari_api, m) {
    m.doc() = "Kalahari Core API for Python plugins - version 5.0";

    // Logger class (singleton with static methods)
    py::class_<kalahari::core::Logger>(m, "Logger",
        "Logging interface for Kalahari plugins")
        .def_static("info",
            [](const std::string& msg) {
                kalahari::core::Logger::getInstance().info("{}", msg);
            },
            py::arg("message"),
            "Log an info-level message")
        .def_static("error",
            [](const std::string& msg) {
                kalahari::core::Logger::getInstance().error("{}", msg);
            },
            py::arg("message"),
            "Log an error-level message")
        .def_static("debug",
            [](const std::string& msg) {
                kalahari::core::Logger::getInstance().debug("{}", msg);
            },
            py::arg("message"),
            "Log a debug-level message")
        .def_static("warn",
            [](const std::string& msg) {
                kalahari::core::Logger::getInstance().warn("{}", msg);
            },
            py::arg("message"),
            "Log a warning-level message");

    // Event struct for pub/sub communication
    py::class_<kalahari::core::Event>(m, "Event",
        "Event data structure for pub/sub communication")
        .def(py::init<const std::string&>(),
            py::arg("type"),
            "Create event with type")
        .def(py::init<const std::string&, const py::object&>(),
            py::arg("type"),
            py::arg("data"),
            "Create event with type and data")
        .def_readwrite("type", &kalahari::core::Event::type,
            "Event type identifier (e.g., 'document:opened')")
        .def_property("data",
            [](const kalahari::core::Event& /* self */) -> py::object {
                // For Python, data is treated as an opaque object
                // In this release, we don't provide access to C++ std::any
                return py::none();
            },
            [](kalahari::core::Event& self, const py::object& obj) {
                // Store reference in std::any (as void* for simplicity)
                // This is a simplified implementation
                self.data = std::any(reinterpret_cast<uintptr_t>(obj.ptr()));
            },
            "Event data payload (stored as reference)");

    // EventBus singleton for event pub/sub
    py::class_<kalahari::core::EventBus>(m, "EventBus",
        "Thread-safe event publish/subscribe system")
        .def_static("get_instance", &kalahari::core::EventBus::getInstance,
            py::return_value_policy::reference,
            "Get EventBus singleton instance")
        .def("subscribe",
            [](kalahari::core::EventBus& self, const std::string& eventType,
               py::object callback) {
                kalahari::core::EventListener listener =
                    [callback](const kalahari::core::Event& evt) {
                        try {
                            py::gil_scoped_acquire acquire;
                            callback(evt);
                        } catch (const py::error_already_set& e) {
                            kalahari::core::Logger::getInstance().error(
                                "EventBus: Python callback raised exception: {}",
                                e.what());
                        }
                    };
                self.subscribe(eventType, listener);
            },
            py::arg("event_type"),
            py::arg("callback"),
            "Subscribe to event type with Python callback")
        .def("unsubscribe",
            &kalahari::core::EventBus::unsubscribe,
            py::arg("event_type"),
            "Unsubscribe from event type")
        .def("emit",
            &kalahari::core::EventBus::emit,
            py::arg("event"),
            "Emit event synchronously to all subscribers")
        .def("emit_async",
            &kalahari::core::EventBus::emitAsync,
            py::arg("event"),
            "Emit event asynchronously (marshalled to GUI thread)")
        .def("has_subscribers",
            &kalahari::core::EventBus::hasSubscribers,
            py::arg("event_type"),
            "Check if event type has subscribers")
        .def("get_subscriber_count",
            &kalahari::core::EventBus::getSubscriberCount,
            py::arg("event_type"),
            "Get number of subscribers for event type")
        .def("clear_all",
            &kalahari::core::EventBus::clearAll,
            "Clear all event subscriptions");
}
