// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file window_editor_provider.cpp
/// @brief The editors of the window kinds that modules of the program bring

#include <kalahari/core/window_editor_provider.h>

#include <algorithm>

namespace kalahari::core {

std::shared_ptr<IWindowEditorProvider> windowEditorProvider(const QString& editorId) {
    if (editorId.isEmpty()) {
        return nullptr;
    }
    const std::vector<std::shared_ptr<IWindowEditorProvider>> providers =
        ExtensionPointRegistry::getInstance().getPluginsOfType<IWindowEditorProvider>();
    const auto found = std::find_if(
        providers.cbegin(), providers.cend(),
        [&editorId](const std::shared_ptr<IWindowEditorProvider>& provider) {
            return provider->editorId() == editorId;
        });
    return found != providers.cend() ? *found : nullptr;
}

std::vector<std::shared_ptr<IWindowEditorProvider>> windowEditorProviders() {
    std::vector<std::shared_ptr<IWindowEditorProvider>> providers =
        ExtensionPointRegistry::getInstance().getPluginsOfType<IWindowEditorProvider>();
    std::sort(providers.begin(), providers.end(),
              [](const std::shared_ptr<IWindowEditorProvider>& first,
                 const std::shared_ptr<IWindowEditorProvider>& second) {
                  return first->editorId() < second->editorId();
              });
    return providers;
}

}  // namespace kalahari::core
