/// @file plugin_manifest.h
/// @brief Plugin manifest structure and JSON parsing
///
/// This file provides the PluginManifest struct representing the metadata
/// stored in manifest.json files within .kplugin archives. The manifest
/// contains essential information about plugin identity, requirements,
/// and capabilities.
///
/// Example manifest.json:
/// @code{.json}
/// {
///   "id": "org.kalahari.exporter.docx",
///   "name": "DOCX Exporter",
///   "version": "0.1.0",
///   "description": "Export documents to DOCX format",
///   "author": "Kalahari Team",
///   "license": "MIT",
///   "api_version": "0.1",
///   "entry_point": "plugin.py:ExporterPlugin",
///   "extension_points": ["IExporter"],
///   "dependencies": ["python-docx>=0.8.10"],
///   "permissions": ["file_access", "event_subscription"]
/// }
/// @endcode

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace kalahari {
namespace core {

using json = nlohmann::json;

/// @brief Plugin metadata from manifest.json
///
/// Contains all required and optional fields from a plugin's manifest.
/// Used during plugin discovery to determine compatibility and requirements.
struct PluginManifest {
    // Required fields
    std::string id;               ///< Unique plugin identifier (e.g., org.kalahari.exporter.docx)
    std::string name;             ///< Human-readable plugin name
    std::string version;          ///< Plugin version (semantic versioning)
    std::string api_version;      ///< Required Kalahari API version
    std::string entry_point;      ///< Python module:class entry point

    // Optional fields
    std::string description;                  ///< Plugin description
    std::string author;                       ///< Plugin author/organization
    std::string license;                      ///< License identifier (e.g., MIT, GPL-3.0)
    std::vector<std::string> extension_points; ///< Extension points implemented
    std::vector<std::string> dependencies;     ///< Python package dependencies
    std::vector<std::string> permissions;      ///< Required permissions

    /// @brief Parse manifest from JSON
    /// @param j JSON object parsed from manifest.json
    /// @return PluginManifest with populated fields
    /// @throws json::exception if required fields are missing
    static PluginManifest fromJson(const json& j);

    /// @brief Validate manifest (check required fields and format)
    /// @return true if manifest is valid, false otherwise
    ///
    /// Checks:
    /// - All required fields are non-empty
    /// - Version strings are in valid format
    /// - Entry point has correct format (module:class)
    bool validate() const;

    /// @brief Check if plugin implements a specific extension point
    /// @param point Extension point name to check
    /// @return true if plugin implements this extension point
    bool implementsExtensionPoint(const std::string& point) const;

    /// @brief Check if plugin requires a specific permission
    /// @param permission Permission name to check
    /// @return true if plugin requires this permission
    bool requiresPermission(const std::string& permission) const;
};

} // namespace core
} // namespace kalahari
