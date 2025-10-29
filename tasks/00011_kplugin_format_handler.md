# Task #00011: .kplugin Format Handler + Actual Plugin Loading

## Context

- **Phase:** Phase 0 Week 6 (Plugin System Completion)
- **Roadmap Reference:** ROADMAP.md - Phase 0 Foundation
- **Related Docs:**
  - **[project_docs/04_plugin_system.md](../project_docs/04_plugin_system.md)** - Plugin architecture spec
  - **[project_docs/07_mvp_tasks.md](../project_docs/07_mvp_tasks.md)** - Week 6 plan
  - tasks/00009_plugin_manager_pybind11_bindings.md (PluginManager foundation)
  - tasks/00010_extension_points_eventbus.md (Extension Points + EventBus)
- **Dependencies:**
  - Task #00009 (PluginManager + pybind11)
  - Task #00010 (Extension Points + EventBus)
  - libzip already in vcpkg dependencies

## Objective

Complete plugin system with actual .kplugin file handling:

1. **PluginManager Enhancement** - Implement actual plugin discovery and loading
2. **.kplugin Format** - ZIP-based plugin package format
3. **Manifest Parsing** - JSON manifest.json handling
4. **Plugin Loading** - Extract and execute Python plugin code
5. **Error Handling** - Isolated plugin failures

**Key Goals:**

- Discover .kplugin files in plugins/ directory
- Parse manifest.json (plugin metadata + requirements)
- Extract ZIP to temporary directory
- Load Python plugin module
- Call plugin lifecycle methods (on_init, on_activate)
- Graceful error handling (plugin crashes don't crash app)
- Prepare for Phase 2 MVP plugins

## Architecture Overview

### .kplugin Package Format

```
my_plugin.kplugin (ZIP archive)
├── manifest.json           # Plugin metadata + requirements
├── plugin.py               # Main plugin code
├── requirements.txt        # Python dependencies (optional)
└── assets/                 # Plugin resources
    ├── icons/
    ├── images/
    └── docs/
```

### manifest.json Structure

```json
{
  "id": "org.kalahari.exporter.docx",
  "name": "DOCX Exporter",
  "version": "0.1.0",
  "description": "Export documents to DOCX format",
  "author": "Kalahari Team",
  "license": "MIT",
  "api_version": "0.1",
  "entry_point": "plugin.py:ExporterPlugin",
  "extension_points": ["IExporter"],
  "dependencies": [
    "python-docx>=0.8.10"
  ],
  "permissions": [
    "file_access",
    "event_subscription"
  ]
}
```

### Plugin Loading Sequence

```
1. PluginManager::discoverPlugins()
   ├── Scan plugins/ directory
   ├── Find .kplugin files
   └── Parse manifest.json for each

2. PluginManager::loadPlugin(id)
   ├── Extract .kplugin to temp dir
   ├── Add temp dir to sys.path
   ├── Import plugin module
   ├── Call plugin.on_init()
   ├── Register with ExtensionPointRegistry
   └── Subscribe to EventBus events

3. Plugin Active State
   ├── Plugin callable from C++
   ├── Can emit/receive events
   └── Can register UI panels

4. PluginManager::unloadPlugin(id)
   ├── Call plugin.on_deactivate()
   ├── Unregister from registry
   ├── Remove from sys.path
   └── Cleanup temp directory
```

## Implementation Checklist

### Phase 1: Enhanced Plugin Discovery

- [ ] Update `include/kalahari/core/plugin_manager.h`
  - Add PluginMetadata struct (id, name, version, manifest_path)
  - Add member: `std::map<std::string, PluginMetadata> m_plugins`
  - Add method: `std::vector<PluginMetadata> getDiscoveredPlugins() const`
- [ ] Update `src/core/plugin_manager.cpp`
  - Implement `discoverPlugins()` - scan plugins/ directory
  - Use libzip to detect .kplugin files
  - Verify manifest.json exists
  - Store metadata (don't load yet)
- [ ] Create `include/kalahari/core/plugin_manifest.h`
  - PluginManifest struct (parse JSON)
  - Methods: fromJson(), validate()
- [ ] Create `src/core/plugin_manifest.cpp`
  - JSON deserialization (nlohmann_json)
  - Validation: required fields, version checks

### Phase 2: ZIP Extraction

- [ ] Create `include/kalahari/core/plugin_archive.h`
  - PluginArchive class (RAII wrapper for libzip)
  - Methods: extract(), verify()
  - Temporary directory management
- [ ] Create `src/core/plugin_archive.cpp`
  - Use libzip to extract .kplugin files
  - Extract to ~/.local/share/Kalahari/plugins/temp/
  - Verify manifest.json present
  - Cleanup on destruction
- [ ] Update `src/CMakeLists.txt`
  - Link against libzip::zip

### Phase 3: Plugin Loading Implementation

- [ ] Update `PluginManager::loadPlugin(id)`
  - Extract .kplugin using PluginArchive
  - Add extracted dir to Python sys.path
  - Import plugin module dynamically
  - Create plugin instance
  - Call plugin.on_init()
  - Call plugin.on_activate()
  - Log success/failure
- [ ] Implement error handling
  - Try-catch blocks around Python calls
  - Graceful failure (log error, mark plugin as broken)
  - Don't crash app if plugin fails

### Phase 4: Plugin Instance Management

- [ ] Store loaded plugins
  - Map: plugin_id → PluginInstance
  - PluginInstance: manifest + module + lifecycle_state
- [ ] Lifecycle management
  - States: Discovered, Loaded, Activated, Deactivated, Error
  - Transitions validated (can't deactivate if not activated)
- [ ] Extension point registration
  - Query plugin for interface implementations
  - Register with ExtensionPointRegistry

### Phase 5: Unloading & Cleanup

- [ ] Implement `PluginManager::unloadPlugin(id)`
  - Call plugin.on_deactivate()
  - Unregister from ExtensionPointRegistry
  - Remove from sys.path
  - Delete PluginInstance
  - Clean temporary files
- [ ] Implement `PluginManager::~PluginManager()`
  - Gracefully unload all active plugins
  - Log any errors

### Phase 6: Testing

- [ ] Create sample test plugin
  - `tests/plugins/hello_plugin.kplugin`
  - Simple manifest.json
  - Basic plugin.py with on_init/on_activate
- [ ] Create C++ unit tests `tests/core/test_plugin_loading.cpp`
  - Test discovery (finds .kplugin files)
  - Test manifest parsing
  - Test loading/unloading
  - Test error handling (bad plugin)
- [ ] Create Python integration test
  - Verify plugin module loaded
  - Verify on_init called
  - Verify event subscription works

### Phase 7: Integration & Documentation

- [ ] Build verification
  - `./scripts/build_linux.sh` succeeds
  - All tests pass
- [ ] Create example plugin documentation
  - `docs/plugin_development_guide.md`
  - How to create .kplugin files
  - Plugin template
  - Manifest reference
- [ ] Update CHANGELOG.md
  - All components of plugin system complete
  - Ready for Phase 1

## Proposed Code Examples

### Plugin Manifest Structure

```cpp
/// @file plugin_manifest.h
/// @brief Plugin manifest parsing

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace kalahari {
namespace core {

using json = nlohmann::json;

/// @brief Plugin metadata from manifest.json
struct PluginManifest {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string license;
    std::string api_version;
    std::string entry_point;
    std::vector<std::string> extension_points;
    std::vector<std::string> dependencies;
    std::vector<std::string> permissions;

    /// @brief Parse from JSON
    static PluginManifest fromJson(const json& j);

    /// @brief Validate manifest (required fields, versions)
    bool validate() const;

    /// @brief Check if requires specific extension point
    bool requiresExtensionPoint(const std::string& point) const;
};

} // namespace core
} // namespace kalahari
```

### Plugin Archive Handler

```cpp
/// @file plugin_archive.h
/// @brief ZIP plugin archive extraction

#pragma once

#include <string>
#include <filesystem>

namespace kalahari {
namespace core {

/// @brief RAII wrapper for ZIP extraction
class PluginArchive {
public:
    /// @brief Extract .kplugin file
    /// @param plugin_path Path to .kplugin file
    /// @return Extracted directory path
    PluginArchive(const std::string& plugin_path);

    /// @brief Cleanup on destruction
    ~PluginArchive();

    // Prevent copying
    PluginArchive(const PluginArchive&) = delete;
    PluginArchive& operator=(const PluginArchive&) = delete;

    /// @brief Get extracted directory path
    const std::filesystem::path& getExtractedPath() const;

    /// @brief Verify manifest.json exists
    bool hasManifest() const;

private:
    std::filesystem::path m_extracted_dir;
};

} // namespace core
} // namespace kalahari
```

### Enhanced Plugin Manager

```cpp
// Updated in plugin_manager.cpp:

size_t PluginManager::discoverPlugins() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::filesystem::path plugins_dir = "plugins";

    if (!std::filesystem::exists(plugins_dir)) {
        Logger::getInstance().warn("plugins/ directory not found");
        return 0;
    }

    size_t discovered = 0;

    // Find all .kplugin files
    for (const auto& entry : std::filesystem::directory_iterator(plugins_dir)) {
        if (entry.path().extension() == ".kplugin") {
            try {
                PluginArchive archive(entry.path().string());

                if (!archive.hasManifest()) {
                    Logger::getInstance().warn("No manifest in: {}", entry.path().filename().string());
                    continue;
                }

                // Parse manifest
                auto manifest_path = archive.getExtractedPath() / "manifest.json";
                std::ifstream manifest_file(manifest_path);
                auto manifest_json = json::parse(manifest_file);
                auto manifest = PluginManifest::fromJson(manifest_json);

                if (manifest.validate()) {
                    PluginMetadata metadata{
                        .id = manifest.id,
                        .name = manifest.name,
                        .version = manifest.version,
                        .path = entry.path()
                    };
                    m_plugins[manifest.id] = metadata;
                    discovered++;

                    Logger::getInstance().info("Plugin discovered: {} v{}",
                                             manifest.name, manifest.version);
                }
            } catch (const std::exception& e) {
                Logger::getInstance().error("Failed to discover plugin: {}", e.what());
            }
        }
    }

    Logger::getInstance().info("Plugin discovery complete: {} plugins found", discovered);
    return discovered;
}
```

## Risks & Open Questions

**Risks:**

- **R1:** ZIP extraction may fail silently if libzip errors not handled
  - Mitigation: Wrap libzip in try-catch, verify extraction
- **R2:** Python sys.path pollution if multiple plugins
  - Mitigation: Use isolated directories, clean up on unload
- **R3:** Plugin dependencies may conflict (same library, different versions)
  - Mitigation: Phase 2+ concern, for now assume compatible

**Open Questions:**

- **Q1:** Should plugins be auto-loaded on startup or lazy-loaded?
  - Answer: Auto-discover on startup, lazy-load on demand
- **Q2:** Where to store temporary extracted plugins?
  - Answer: `~/.local/share/Kalahari/plugins/temp/` (configurable)
- **Q3:** How to handle plugin with missing dependencies?
  - Answer: Phase 2 - for now, fail with error message

## Acceptance Criteria

✅ **Discovery:**
- Scans plugins/ directory
- Finds .kplugin files
- Parses manifest.json
- Validates metadata

✅ **Loading:**
- Extracts .kplugin to temp dir
- Imports Python module
- Calls on_init() and on_activate()
- Registers with ExtensionPointRegistry

✅ **Unloading:**
- Calls on_deactivate()
- Cleans up temp files
- Removes from sys.path

✅ **Error Handling:**
- Plugin failures don't crash app
- Errors logged appropriately
- Plugin marked as broken

✅ **Testing:**
- Unit tests for discovery, loading, unloading
- Integration test with sample plugin
- Error cases tested

## Status
- **Created:** 2025-10-29
- **Approved:** [Awaiting user testing + approval]
- **Started:**
- **Completed:**

## Implementation Notes

(To be filled during implementation)

## Related Files

**New files:**
- `include/kalahari/core/plugin_manifest.h`
- `src/core/plugin_manifest.cpp`
- `include/kalahari/core/plugin_archive.h`
- `src/core/plugin_archive.cpp`
- `tests/plugins/hello_plugin.kplugin`
- `tests/core/test_plugin_loading.cpp`
- `docs/plugin_development_guide.md`

**Modified files:**
- `include/kalahari/core/plugin_manager.h` (enhanced)
- `src/core/plugin_manager.cpp` (full implementation)
- `src/CMakeLists.txt` (link libzip)
- `CHANGELOG.md`

---

**Task Priority:** 🟠 HIGH (Required for Phase 1 MVP plugins)
**Estimated Effort:** 12-15 hours (Week 6)

---

**References:**
- libzip documentation: https://libzip.org/
- project_docs/04_plugin_system.md - Complete architecture
