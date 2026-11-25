// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file plugin_api_embedded.cpp
/// @brief The kalahari_api module built into the program, for its plugins
///
/// Compiled into the program and its tests, not into kalahari_core: the core library is also
/// loaded by the extension module of src/bindings into an interpreter that runs already, where
/// a built-in module can no longer be added.

#include <pybind11/embed.h>

#include "kalahari/core/plugin_api.h"

PYBIND11_EMBEDDED_MODULE(kalahari_api, m) {
    kalahari::core::definePluginApi(m.ptr());
}
