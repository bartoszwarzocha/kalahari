# Task #00001: Plugin Manager Skeleton

## Context
- **Phase:** Phase 0 Week 2
- **Roadmap Reference:** ROADMAP.md Phase 0, Week 2: Plugin Manager Skeleton
- **Related Docs:**
  - [project_docs/04_plugin_system.md](../project_docs/04_plugin_system.md) - Plugin API specification
  - [project_docs/03_architecture.md](../project_docs/03_architecture.md) - Singleton pattern for PluginManager
- **Dependencies:**
  - Task #00000 (Week 1): CMake, vcpkg, basic project structure ✅ COMPLETE
  - vcpkg packages: nlohmann_json, spdlog already configured

## Objective
Create the foundational PluginManager class that will serve as the core plugin infrastructure for Kalahari. This is the first step in implementing the plugin system from day zero.

**Success Criteria:**
- PluginManager class exists as singleton
- Can discover plugin folders in `plugins/` directory
- Can read and validate plugin metadata (manifest.json)
- Basic plugin lifecycle methods (load/unload) are defined (stub implementation)
- Logging integrated with spdlog
- Unit tests verify basic functionality
- Code compiles on all platforms via CI

## Proposed Approach

### 1. Architecture
- **Pattern:** Singleton (as per 03_architecture.md - infrastructure services)
- **Location:** `src/core/PluginManager.{h,cpp}`
- **Plugin Discovery:** Scan `plugins/` directory for folders containing `manifest.json`
- **Metadata Format:** JSON (nlohmann_json library)
- **Logging:** spdlog for all plugin operations

### 2. Plugin Metadata Structure
```json
{
  "id": "kalahari.plugin.docx_exporter",
  "name": "DOCX Exporter",
  "version": "1.0.0",
  "api_version": "1.0.0",
  "type": "python",
  "entry_point": "plugin.py",
  "dependencies": [],
  "author": "Kalahari Team",
  "description": "Export documents to DOCX format"
}
```

### 3. PluginManager Interface (Skeleton)
```cpp
class PluginManager {
public:
    static PluginManager& getInstance();

    // Discovery
    void discoverPlugins(const std::string& pluginDir);
    std::vector<PluginMetadata> getAvailablePlugins() const;

    // Lifecycle (stub for now)
    bool loadPlugin(const std::string& pluginId);
    void unloadPlugin(const std::string& pluginId);
    bool isPluginLoaded(const std::string& pluginId) const;

    // Metadata
    std::optional<PluginMetadata> getPluginMetadata(const std::string& pluginId) const;

private:
    PluginManager() = default;
    ~PluginManager() = default;
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    std::map<std::string, PluginMetadata> m_availablePlugins;
    std::set<std::string> m_loadedPlugins;
};
```

### 4. Key Technical Decisions
- **nlohmann_json** for JSON parsing (already in vcpkg.json)
- **std::filesystem** for directory scanning (C++17, available in C++20)
- **std::optional** for returning metadata (C++17)
- **spdlog** for structured logging
- **Singleton pattern** for global access (as per architecture doc)

### 5. Implementation Steps
1. Create `PluginMetadata` struct
2. Implement singleton getInstance()
3. Implement discoverPlugins() with filesystem traversal
4. Parse manifest.json with validation
5. Add stub methods for load/unload
6. Add comprehensive logging
7. Write unit tests (Catch2)

### Why This Approach
- **Singleton:** Matches architectural decision for infrastructure services
- **JSON metadata:** Human-readable, extensible, industry standard
- **Stub lifecycle:** Allows testing discovery without Python embedding complexity
- **Filesystem:** Modern C++ approach, cross-platform via std::filesystem

### Alternatives Considered
- **Alternative A: Load Python immediately**
  - ❌ Too complex for Week 2, Python embedding is Week 3-4 task
- **Alternative B: XML metadata**
  - ❌ More verbose, JSON is simpler and better for Python plugins
- **Alternative C: Static registration**
  - ❌ We want dynamic plugin discovery at runtime

## Implementation Plan (Checklist)

### Core Implementation
- [ ] Create `src/core/plugin/` directory structure
- [ ] Create `PluginMetadata` struct in `plugin_metadata.h`
- [ ] Implement `PluginManager` class skeleton in `plugin_manager.h`
- [ ] Implement singleton getInstance() method
- [ ] Implement discoverPlugins() with std::filesystem
- [ ] Implement JSON parsing with nlohmann_json
- [ ] Add metadata validation (required fields)
- [ ] Implement getAvailablePlugins() and getPluginMetadata()
- [ ] Add stub methods for loadPlugin()/unloadPlugin()
- [ ] Integrate spdlog logging for all operations

### Testing
- [ ] Create `tests/core/plugin/test_plugin_manager.cpp`
- [ ] Write test: Singleton returns same instance
- [ ] Write test: Discover plugins in test directory
- [ ] Write test: Parse valid manifest.json
- [ ] Write test: Reject invalid manifest.json (missing fields)
- [ ] Write test: Handle non-existent plugin directory gracefully
- [ ] All tests pass locally

### Build & Documentation
- [ ] Update `src/CMakeLists.txt` to include new files
- [ ] Update `tests/CMakeLists.txt` to include new tests
- [ ] Add Doxygen comments to all public methods
- [ ] Create example `plugins/example_plugin/manifest.json` for testing

### Verification
- [ ] Code compiles on Windows (local)
- [ ] Code compiles on macOS (CI)
- [ ] Code compiles on Linux (CI)
- [ ] All tests pass
- [ ] No compiler warnings (-Wall -Wextra)
- [ ] clang-format applied (if configured)
- [ ] Code reviewed by user

## Risks & Open Questions

### Open Questions
- **Q:** Should plugin discovery be automatic on startup, or manual call required?
  - **Proposal:** Manual call in main(), gives control over when plugins are scanned

- **Q:** Should we validate `api_version` compatibility now or later?
  - **Proposal:** Add validation logic now (semantic versioning check), but allow all versions initially

- **Q:** Error handling strategy for invalid plugins?
  - **Proposal:** Log warning and skip invalid plugins, don't crash the app

### Risks
- **Risk:** std::filesystem not available on older compilers
  - **Mitigation:** We require C++20, std::filesystem is mandatory (C++17 feature)

- **Risk:** Plugin directory doesn't exist
  - **Mitigation:** Create `plugins/` directory if missing, log info message

- **Risk:** Circular dependencies between plugins
  - **Mitigation:** Not handling dependencies yet, Week 3 task

## Status
- **Created:** 2025-10-26
- **Approved:** YYYY-MM-DD (awaiting user approval)
- **Started:** YYYY-MM-DD
- **Completed:** YYYY-MM-DD

## Implementation Notes
(Will be filled during implementation)

## Verification
- [ ] Code compiles on Windows
- [ ] Code compiles on macOS (CI)
- [ ] Code compiles on Linux (CI)
- [ ] Tests pass (Catch2)
- [ ] No memory leaks (valgrind/ASAN if available)
- [ ] Code reviewed
- [ ] Documentation updated
- [ ] CHANGELOG.md updated
- [ ] CI/CD pipeline passes

## Related Tasks
- **Depends on:** Task #00000 (Week 1 foundation) ✅ COMPLETE
- **Blocks:** Task #00002 (Week 3: Python Embedding)
- **Blocks:** Task #00003 (Week 3: Plugin Loader with Python)
- **Related:** ROADMAP.md Phase 0 Week 2

## Post-Completion Review
(Will be filled after completion)
