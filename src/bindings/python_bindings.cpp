/// @file python_bindings.cpp
/// @brief pybind11 bindings for Kalahari core API

#include <pybind11/pybind11.h>
#include <kalahari/core/logger.h>

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
}
