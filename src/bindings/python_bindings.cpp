// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file python_bindings.cpp
/// @brief The kalahari_api extension module, for Python scripts outside the program
///
/// The program's own interpreter has the same module built in (plugin_api_embedded.cpp); both
/// are filled by core::definePluginApi().

#include <pybind11/pybind11.h>

#include <kalahari/core/plugin_api.h>

PYBIND11_MODULE(kalahari_api, m) {
    kalahari::core::definePluginApi(m.ptr());
}
