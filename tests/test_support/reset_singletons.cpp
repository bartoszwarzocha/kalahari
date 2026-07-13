/// @file reset_singletons.cpp
/// @brief Implementation of the test-only singleton reset helper (WS4.1).

#include "reset_singletons.h"

#include <kalahari/core/art_provider.h>
#include <kalahari/core/icon_registry.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/theme_manager.h>
#include <kalahari/core/trusted_keys.h>
#include <kalahari/gui/command_registry.h>

namespace kalahari {
namespace test {

void resetSingletons() {
    // Order: configuration first, then derived visual state.
    core::SettingsManager::getInstance().resetToDefaults();
    core::TrustedKeys::getInstance().clear();
    gui::CommandRegistry::getInstance().clear();

    // IconRegistry: compose the public reset paths (there is no single reset()).
    // Each of resetTheme(), resetAllCustomizations() and resetSizes() clears the
    // icon cache on its own, so calling all three returns the registry to a clean
    // baseline (theme, per-icon customizations, sizes, and cache).
    core::IconRegistry::getInstance().resetTheme();
    core::IconRegistry::getInstance().resetAllCustomizations();
    core::IconRegistry::getInstance().resetSizes();

    // ThemeManager: clear user color overrides. The active theme is intentionally
    // NOT switched to a fixed base here: no current test mutates the active theme,
    // and forcing a switch would change the baseline theme for the whole suite. If
    // a future test calls switchTheme(), it must restore a known theme itself (or
    // this hook should be extended to switch to a deterministic base).
    core::ThemeManager::getInstance().resetColorOverrides();

    // ArtProvider: new test-only reset (the only production change WS4.1 needs).
    core::ArtProvider::getInstance().reset();

    // Logger: intentionally left as-is (append-only, harmless between tests).
}

} // namespace test
} // namespace kalahari
