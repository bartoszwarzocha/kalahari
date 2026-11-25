// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

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
