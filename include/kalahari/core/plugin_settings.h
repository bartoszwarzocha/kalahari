// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file plugin_settings.h
/// @brief The settings plugins declare and the values the writer gives them
///
/// A plugin declares its settings (kalahari_api.Setting from get_settings()); the page of the
/// plugins in the Settings window shows them, and their values are kept in plugins.settings
/// as {plugin id: {setting id: value}}. A value of another kind than the default, outside the
/// limits or not one of the choices is not taken; a stored one like that reads as the default.

#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace kalahari::core {

/// @brief A setting a plugin declares
struct PluginSettingInfo {
    std::string id;
    std::string label;
    std::string description;
    nlohmann::json defaultValue;  ///< A bool, an integer, a number or a string: the kind
    std::vector<std::pair<nlohmann::json, std::string>> choices;  ///< (value, label); none: any
    std::optional<double> minimum;  ///< The smallest number (numbers only)
    std::optional<double> maximum;  ///< The largest number (numbers only)

    /// @brief Whether @p value may be the value of the setting
    [[nodiscard]] bool accepts(const nlohmann::json& value) const;

    /// @brief Whether the declaration itself is right: an id, a label, a default it accepts
    [[nodiscard]] bool isValid() const;
};

/// @brief The value of the setting the writer gave, or its default
[[nodiscard]] nlohmann::json pluginSettingValue(const std::string& pluginId,
                                                const PluginSettingInfo& setting);

/// @brief Remember a value of the setting
/// @return false when the setting does not accept it (nothing is changed)
bool setPluginSettingValue(const std::string& pluginId, const PluginSettingInfo& setting,
                           const nlohmann::json& value);

/// @brief Forget the values of every setting of the plugin (it is removed)
void forgetPluginSettings(const std::string& pluginId);

}  // namespace kalahari::core
