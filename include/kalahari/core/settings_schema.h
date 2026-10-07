/// @file settings_schema.h
/// @brief The one list of application settings and their default values
///
/// Every setting stored in settings.json has its default value here and nowhere
/// else. SettingsManager::get() falls back to this list when a key is missing, so
/// callers do not repeat defaults. Per-theme colors (themes.<name>.*,
/// icons.themes.<name>.*) are not listed: their defaults come from the theme files.

#pragma once

#include <map>
#include <string>
#include <nlohmann/json.hpp>

namespace kalahari {
namespace core {
namespace settings_schema {

/// @brief Default value of a setting
/// @param key Setting key; '.' and '/' both separate levels ("icons.sizes.menu")
/// @return Pointer to the default value, or nullptr if the key is not listed
const nlohmann::json* defaultValue(const std::string& key);

/// @brief All listed settings (key with '.' separators -> default value)
const std::map<std::string, nlohmann::json>& defaults();

} // namespace settings_schema
} // namespace core
} // namespace kalahari
