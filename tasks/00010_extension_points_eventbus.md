# Task #00010: Extension Points + Event Bus Foundation

## Context

- **Phase:** Phase 0 Week 5-6 (Plugin Architecture)
- **Roadmap Reference:** ROADMAP.md - Phase 0 Foundation
- **Related Docs:**
  - **[project_docs/04_plugin_system.md](../project_docs/04_plugin_system.md)** - Complete plugin architecture spec
  - **[project_docs/07_mvp_tasks.md](../project_docs/07_mvp_tasks.md)** - Week 5-6 plan
  - tasks/00009_plugin_manager_pybind11_bindings.md (Plugin Manager operational)
- **Dependencies:**
  - Task #00009 (PluginManager + pybind11 complete)
  - pybind11 working (kalahari_api module)

## Objective

Extend plugin system with:

1. **Extension Points** - Define C++ interfaces for plugin capabilities
2. **Event Bus** - Thread-safe pub/sub system for core↔plugin communication

**Key Goals:**

- Establish plugin extension architecture patterns (Strategy, Observer)
- Create `EventBus` singleton for asynchronous plugin communication
- Expose EventBus to Python via pybind11
- Prove extension point pattern works (test with stub plugin)
- Lay groundwork for Phase 1 plugins (Exporters, Panels, Assistants)

## Architecture Overview

### Extension Points

```cpp
// Four main extension point interfaces

class IExporter {
  // Plugins export documents to formats (DOCX, PDF, Markdown)
  virtual std::string exportDocument(const Document& doc) = 0;
};

class IPanelProvider {
  // Plugins add dockable UI panels to main window
  virtual wxPanel* createPanel(wxWindow* parent) = 0;
};

class IAssistant {
  // Plugins provide graphical assistants (Lion, Meerkat, Elephant, Cheetah)
  virtual void onSessionStart() = 0;
  virtual void onGoalReached() = 0;
};

class IPlugin {
  // Base interface all plugins must implement
  virtual std::string getPluginId() = 0;
  virtual std::string getVersion() = 0;
  virtual void onInit() = 0;
  virtual void onActivate() = 0;
};
```

### Event Bus Architecture

```
┌────────────────────────────────────────┐
│         Kalahari Core (C++)            │
│  ┌──────────────────────────────────┐  │
│  │  EventBus (Singleton)            │  │
│  │  - subscribe(event, callback)    │  │
│  │  - unsubscribe(event, callback)  │  │
│  │  - emit(event)                   │  │
│  │  - emitAsync(event)              │  │
│  │  - Thread-safe (std::mutex)      │  │
│  └──────────────────────────────────┘  │
│           ↕ pybind11                    │
│  ┌──────────────────────────────────┐  │
│  │  kalahari_api (Python module)    │  │
│  │  - EventBus.subscribe()          │  │
│  │  - EventBus.emit()               │  │
│  │  - EventBus.emitAsync()          │  │
│  └──────────────────────────────────┘  │
│           ↕ import                      │
│  ┌──────────────────────────────────┐  │
│  │  Python Plugins                  │  │
│  │  - Subscribe to events           │  │
│  │  - React to document changes     │  │
│  │  - Emit custom events            │  │
│  └──────────────────────────────────┘  │
└────────────────────────────────────────┘
```

## Implementation Checklist

### Phase 1: Extension Points Interfaces

- [ ] Create `include/kalahari/core/extension_points.h`
  - `IExporter` interface (abstract base)
  - `IPanelProvider` interface
  - `IAssistant` interface
  - `IPlugin` base interface
  - `ExtensionPointRegistry` singleton
  - Doxygen comments
- [ ] Create `src/core/extension_points.cpp`
  - Registry implementation (std::map)
  - Thread-safe registration (std::mutex)
  - Plugin lookup methods
- [ ] Update `src/CMakeLists.txt`
  - Add extension_points.cpp to kalahari target
- [ ] Create unit tests `tests/core/test_extension_points.cpp`
  - Test interface implementation
  - Test registry registration
  - Test thread-safe lookup

### Phase 2: Event Bus Implementation

- [ ] Create `include/kalahari/core/event_bus.h`
  - `Event` struct (type + data)
  - `EventListener` typedef (callback)
  - `EventBus` singleton class
  - Methods: subscribe, unsubscribe, emit, emitAsync
  - Thread-safe with std::mutex
  - Event queue (std::queue)
  - Doxygen comments
- [ ] Create `src/core/event_bus.cpp`
  - Singleton getInstance() implementation
  - Subscribe/unsubscribe logic
  - Synchronous emit (direct callback)
  - Asynchronous emit (wxTheApp->CallAfter for GUI marshalling)
  - Thread-safe listener map
- [ ] Update `src/CMakeLists.txt`
  - Add event_bus.cpp to kalahari target
- [ ] Create unit tests `tests/core/test_event_bus.cpp`
  - Test subscription/unsubscription
  - Test emit (sync + async)
  - Test thread safety
  - Test GUI marshalling

### Phase 3: pybind11 EventBus Bindings

- [ ] Update `src/bindings/python_bindings.cpp`
  - Add EventBus class bindings
  - Expose: subscribe(), unsubscribe(), emit(), emitAsync()
  - Python callable for event callbacks
- [ ] Build verification
  - Verify `kalahari_api.EventBus` loads
  - Test: `python3 -c "import kalahari_api; print(dir(kalahari_api.EventBus))"`
- [ ] Create Python test `tests/test_event_bus.py`
  - Import kalahari_api.EventBus
  - Subscribe to event
  - Emit from C++, verify callback called in Python

### Phase 4: Extension Point Registry Integration

- [ ] Create `include/kalahari/core/plugin_manifest.h`
  - `PluginManifest` struct (id, version, name, extension_points)
  - JSON deserialization (nlohmann_json)
- [ ] Update `PluginManager::discoverPlugins()`
  - Parse manifest.json from .kplugin files
  - Register plugin extension points
  - Store manifest metadata
- [ ] Create unit tests `tests/core/test_plugin_manifest.cpp`
  - Test manifest parsing
  - Test extension point registration

### Phase 5: Integration & Testing

- [ ] Build all tests
  - `./scripts/build_linux.sh` succeeds
  - No compilation warnings
- [ ] Run tests
  - `ctest` passes all extension point tests
  - `ctest` passes all event bus tests
  - Event bus thread safety verified (10+ threads)
- [ ] Manual test
  - Create sample plugin that subscribes to events
  - Emit events from C++
  - Verify Python callbacks executed
- [ ] GUI test
  - Add "Diagnostics → Test Event Bus" menu item
  - Test EventBus functionality from GUI

### Phase 6: Documentation

- [ ] Update `docs/plugin_api_reference.md`
  - Add EventBus API reference
  - Add extension point interface documentation
  - Code examples for each interface
- [ ] Update CHANGELOG.md
  - New features summary
  - Impact on plugin system
- [ ] Update Task #00010 status

## Proposed Code Examples

### Extension Points Header

```cpp
/// @file extension_points.h
/// @brief Plugin extension point interfaces

#pragma once

#include <string>
#include <memory>
#include <map>
#include <mutex>

namespace kalahari {
namespace core {

/// @brief Base interface for all plugins
class IPlugin {
public:
    virtual ~IPlugin() = default;

    /// @brief Get unique plugin identifier
    virtual std::string getPluginId() const = 0;

    /// @brief Get plugin version
    virtual std::string getVersion() const = 0;

    /// @brief Plugin initialization
    virtual void onInit() = 0;

    /// @brief Plugin activation
    virtual void onActivate() = 0;
};

/// @brief Export plugin interface
class IExporter : public IPlugin {
public:
    /// @brief Export document to specific format
    virtual bool exportDocument(const std::string& format,
                               const std::string& filepath) = 0;
};

/// @brief UI panel provider interface
class IPanelProvider : public IPlugin {
public:
    /// @brief Create a dockable panel (Week 5+)
    virtual void* createPanel(void* parentWindow) = 0;
};

/// @brief Graphical assistant interface
class IAssistant : public IPlugin {
public:
    /// @brief Show assistant message
    virtual void showMessage(const std::string& message) = 0;

    /// @brief Called when user reaches a writing goal
    virtual void onGoalReached() = 0;
};

/// @brief Extension point registry (singleton)
class ExtensionPointRegistry {
public:
    static ExtensionPointRegistry& getInstance();

    /// @brief Register a plugin
    void registerPlugin(std::shared_ptr<IPlugin> plugin);

    /// @brief Get plugin by ID
    std::shared_ptr<IPlugin> getPlugin(const std::string& pluginId) const;

private:
    ExtensionPointRegistry() = default;
    ~ExtensionPointRegistry() = default;

    std::map<std::string, std::shared_ptr<IPlugin>> m_plugins;
    mutable std::mutex m_mutex;
};

} // namespace core
} // namespace kalahari
```

### Event Bus Header

```cpp
/// @file event_bus.h
/// @brief Thread-safe event publish/subscribe system

#pragma once

#include <string>
#include <map>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <any>

namespace kalahari {
namespace core {

/// @brief Event data structure
struct Event {
    std::string type;        ///< Event type identifier
    std::any data;           ///< Event data (any type)
};

/// @brief Event listener callback type
using EventListener = std::function<void(const Event&)>;

/// @brief Thread-safe pub/sub event bus (singleton)
class EventBus {
public:
    /// @brief Get singleton instance
    static EventBus& getInstance();

    // Prevent copying/moving
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    /// @brief Subscribe to event type
    /// @param eventType Event type to listen for
    /// @param listener Callback to invoke on event
    void subscribe(const std::string& eventType, EventListener listener);

    /// @brief Unsubscribe from event type
    void unsubscribe(const std::string& eventType);

    /// @brief Emit event synchronously (direct callback)
    void emit(const Event& event);

    /// @brief Emit event asynchronously (marshalled to GUI thread)
    void emitAsync(const Event& event);

private:
    EventBus() = default;
    ~EventBus() = default;

    std::map<std::string, std::vector<EventListener>> m_listeners;
    std::queue<Event> m_eventQueue;
    mutable std::mutex m_mutex;
};

} // namespace core
} // namespace kalahari
```

## Risks & Open Questions

**Risks:**

- **R1:** Python callbacks may not handle exceptions gracefully
  - Mitigation: Wrap Python calls in try-catch, log errors
- **R2:** Event marshalling to GUI thread may cause deadlock
  - Mitigation: Use wxTheApp->CallAfter (safe from any thread)
- **R3:** Extension point registry may conflict with PluginManager
  - Mitigation: Clear separation of concerns - Registry for interfaces, Manager for lifecycle

**Open Questions:**

- **Q1:** Should EventBus filter events by type or all listeners get all events?
  - Answer: Type-based filtering (more efficient, cleaner)
- **Q2:** How to handle plugin load order dependencies?
  - Answer: Phase 1+ concern - for now all plugins independent
- **Q3:** Should extension points be versioned?
  - Answer: Yes - plugin API versioning (Task #00011)

## Acceptance Criteria

✅ **Extension Points:**
- Interface hierarchy correct (IPlugin base, others inherit)
- Registry singleton works
- Thread-safe registration

✅ **Event Bus:**
- Subscribe/unsubscribe work
- Sync emit (direct callback)
- Async emit (GUI marshalled)
- Thread-safe (tested with 10+ threads)

✅ **pybind11 Bindings:**
- EventBus accessible from Python
- Python can subscribe to events
- C++ can emit, Python callbacks executed

✅ **Tests:**
- Unit tests pass (extension points + event bus)
- Python integration tests pass
- Thread safety verified (no race conditions)

✅ **Documentation:**
- API reference complete
- Code examples for each interface
- Event types documented

## Status
- **Created:** 2025-10-29
- **Approved:** [Awaiting user testing + approval]
- **Started:**
- **Completed:**

## Implementation Notes

(To be filled during implementation)

## Related Files

**New files:**
- `include/kalahari/core/extension_points.h`
- `src/core/extension_points.cpp`
- `include/kalahari/core/event_bus.h`
- `src/core/event_bus.cpp`
- `include/kalahari/core/plugin_manifest.h`
- `src/core/plugin_manifest.cpp`
- `tests/core/test_extension_points.cpp`
- `tests/core/test_event_bus.cpp`
- `tests/test_event_bus.py`

**Modified files:**
- `src/CMakeLists.txt` (add new sources)
- `src/bindings/python_bindings.cpp` (add EventBus bindings)
- `docs/plugin_api_reference.md` (add EventBus + interfaces)
- `CHANGELOG.md`

---

**Task Priority:** 🟠 HIGH (Required for Phase 1 plugins)
**Estimated Effort:** 14-18 hours (Week 5-6)

---

**References:**
- pybind11 docs: https://pybind11.readthedocs.io/
- wxWidgets threading: https://docs.wxwidgets.org/
- project_docs/04_plugin_system.md - Complete architecture
