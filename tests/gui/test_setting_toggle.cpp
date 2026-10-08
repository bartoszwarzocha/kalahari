/// @file test_setting_toggle.cpp
/// @brief Toggles that show an on/off setting

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/utils/setting_toggle.h"

#include <QAction>

#include <memory>

using namespace kalahari;

TEST_CASE("Setting toggles: checked while the setting is on, whatever changes it", "[gui][settings]") {
    auto& settings = core::SettingsManager::getInstance();
    const bool darkBefore = settings.get<bool>("editor.darkMode", true);
    settings.set<bool>("editor.darkMode", true);

    QAction toggle(QStringLiteral("Dark Paper"));
    gui::utils::followSetting(&toggle, "editor.darkMode", true);
    CHECK(toggle.isCheckable());
    CHECK(toggle.isChecked());

    // Changed elsewhere: an editor's context menu or the Settings dialog
    settings.set<bool>("editor.darkMode", false);
    CHECK_FALSE(toggle.isChecked());
    settings.set<bool>("editor.darkMode", true);
    CHECK(toggle.isChecked());

    // Another setting leaves the toggle alone
    const bool focusBefore = settings.get<bool>("editor.focus.enabled", false);
    settings.set<bool>("editor.focus.enabled", !focusBefore);
    CHECK(toggle.isChecked());
    settings.set<bool>("editor.focus.enabled", focusBefore);

    // The toggle itself turns the setting over, as the command behind it does
    QObject::connect(&toggle, &QAction::triggered, [&settings]() {
        settings.set<bool>("editor.darkMode", !settings.get<bool>("editor.darkMode", true));
    });
    toggle.trigger();
    CHECK_FALSE(settings.get<bool>("editor.darkMode", true));
    CHECK_FALSE(toggle.isChecked());
    toggle.trigger();
    CHECK(settings.get<bool>("editor.darkMode", false));
    CHECK(toggle.isChecked());

    settings.set<bool>("editor.darkMode", darkBefore);
}

TEST_CASE("Setting toggles: a deleted toggle no longer follows its setting", "[gui][settings]") {
    auto& settings = core::SettingsManager::getInstance();
    const bool darkBefore = settings.get<bool>("editor.darkMode", true);

    auto toggle = std::make_unique<QAction>(QStringLiteral("Dark Paper"));
    gui::utils::followSetting(toggle.get(), "editor.darkMode", true);
    toggle.reset();

    // Would reach the deleted action if it still followed the setting
    settings.set<bool>("editor.darkMode", !darkBefore);
    settings.set<bool>("editor.darkMode", darkBefore);
    CHECK(settings.get<bool>("editor.darkMode", !darkBefore) == darkBefore);
}
