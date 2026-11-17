// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plugin_settings.cpp
/// @brief The settings plugins declare and the values the writer gives them

#include "kalahari/core/plugin_settings.h"

#include "kalahari/core/settings_manager.h"

#include <algorithm>

namespace kalahari::core {

namespace {

constexpr const char* SETTINGS_KEY = "plugins.settings";

using json = nlohmann::json;

/// Whether @p value is of the kind of @p model (an integer is a number too)
bool sameKind(const json& model, const json& value) {
    if (model.is_boolean()) {
        return value.is_boolean();
    }
    if (model.is_number_integer()) {
        return value.is_number_integer();
    }
    if (model.is_number()) {
        return value.is_number();
    }
    if (model.is_string()) {
        return value.is_string();
    }
    return false;
}

}  // namespace

bool PluginSettingInfo::accepts(const json& value) const {
    if (!sameKind(defaultValue, value)) {
        return false;
    }
    if (value.is_number()) {
        const double number = value.get<double>();
        if ((minimum && number < *minimum) || (maximum && number > *maximum)) {
            return false;
        }
    }
    if (!choices.empty()) {
        return std::any_of(choices.begin(), choices.end(),
                           [&value](const auto& choice) { return choice.first == value; });
    }
    return true;
}

bool PluginSettingInfo::isValid() const {
    return !id.empty() && !label.empty() && accepts(defaultValue);
}

json pluginSettingValue(const std::string& pluginId, const PluginSettingInfo& setting) {
    const json all = SettingsManager::getInstance().get<json>(SETTINGS_KEY, json::object());
    if (all.is_object() && all.contains(pluginId) && all[pluginId].is_object() &&
        all[pluginId].contains(setting.id)) {
        const json& stored = all[pluginId][setting.id];
        if (setting.accepts(stored)) {
            // A whole number of a setting of numbers stays a number
            return setting.defaultValue.is_number_float() ? json(stored.get<double>()) : stored;
        }
    }
    return setting.defaultValue;
}

bool setPluginSettingValue(const std::string& pluginId, const PluginSettingInfo& setting,
                           const json& value) {
    if (!setting.accepts(value)) {
        return false;
    }
    auto& settings = SettingsManager::getInstance();
    json all = settings.get<json>(SETTINGS_KEY, json::object());
    if (!all.is_object()) {
        all = json::object();
    }
    if (!all.contains(pluginId) || !all[pluginId].is_object()) {
        all[pluginId] = json::object();
    }
    all[pluginId][setting.id] = value;
    settings.set<json>(SETTINGS_KEY, all);
    return true;
}

void forgetPluginSettings(const std::string& pluginId) {
    auto& settings = SettingsManager::getInstance();
    json all = settings.get<json>(SETTINGS_KEY, json::object());
    if (all.is_object() && all.erase(pluginId) > 0) {
        settings.set<json>(SETTINGS_KEY, all);
    }
}

}  // namespace kalahari::core
