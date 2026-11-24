// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plan_chronology.cpp
/// @brief The module of the chronology of the Book Plan the program has

#include "kalahari/gui/plan_chronology.h"

#include "kalahari/core/extension_points.h"

#include <vector>

namespace kalahari::gui {

std::shared_ptr<IPlanChronologyProvider> planChronologyProvider() {
    const std::vector<std::shared_ptr<IPlanChronologyProvider>> providers =
        core::ExtensionPointRegistry::getInstance().getPluginsOfType<IPlanChronologyProvider>();
    return providers.empty() ? nullptr : providers.front();
}

}  // namespace kalahari::gui
