/// @file plugin_manifest.cpp
/// @brief Plugin manifest implementation

#include <kalahari/core/plugin_manifest.h>
#include <algorithm>
#include <regex>

namespace kalahari {
namespace core {

PluginManifest PluginManifest::fromJson(const json& j) {
    PluginManifest manifest;

    // Required fields - will throw if missing
    manifest.id = j.at("id").get<std::string>();
    manifest.name = j.at("name").get<std::string>();
    manifest.version = j.at("version").get<std::string>();
    manifest.api_version = j.at("api_version").get<std::string>();
    manifest.entry_point = j.at("entry_point").get<std::string>();

    // Optional fields - use value_or for defaults
    manifest.description = j.value("description", "");
    manifest.author = j.value("author", "");
    manifest.license = j.value("license", "");

    // Optional arrays
    if (j.contains("extension_points") && j["extension_points"].is_array()) {
        manifest.extension_points = j["extension_points"].get<std::vector<std::string>>();
    }

    if (j.contains("dependencies") && j["dependencies"].is_array()) {
        manifest.dependencies = j["dependencies"].get<std::vector<std::string>>();
    }

    if (j.contains("permissions") && j["permissions"].is_array()) {
        manifest.permissions = j["permissions"].get<std::vector<std::string>>();
    }

    return manifest;
}

bool PluginManifest::validate() const {
    // Check required fields are non-empty
    if (id.empty() || name.empty() || version.empty() ||
        api_version.empty() || entry_point.empty()) {
        return false;
    }

    // Validate version format (simple check: X.Y.Z)
    std::regex version_regex(R"(\d+\.\d+\.\d+.*)");
    if (!std::regex_match(version, version_regex)) {
        return false;
    }

    if (!std::regex_match(api_version, version_regex)) {
        return false;
    }

    // Validate entry_point format (module:class)
    std::regex entry_point_regex(R"([a-zA-Z_][a-zA-Z0-9_]*:[a-zA-Z_][a-zA-Z0-9_]*)");
    if (!std::regex_match(entry_point, entry_point_regex)) {
        return false;
    }

    return true;
}

bool PluginManifest::implementsExtensionPoint(const std::string& point) const {
    return std::find(extension_points.begin(), extension_points.end(), point)
           != extension_points.end();
}

bool PluginManifest::requiresPermission(const std::string& permission) const {
    return std::find(permissions.begin(), permissions.end(), permission)
           != permissions.end();
}

} // namespace core
} // namespace kalahari
