# Task #00014 - Plugin Integration Foundation (Qt Migration)

**Phase:** 0 (Qt Foundation) - Week 5
**Priority:** HIGH (Architecture - Blocks Phase 2)
**Estimated Time:** 3-4 hours
**Started:** 2025-11-21
**Status:** 🔄 IN PROGRESS

---

## Context

**Problem:** Plugin system has wxWidgets dependencies preventing integration with Command Registry and Qt.

**Critical Issues Found:**
1. ❌ **ICommandProvider missing** - Plugins cannot register commands
2. ⚠️ **EventBus uses wxTheApp->CallAfter()** - No async events in Qt
3. ⚠️ **IPanelProvider uses void\*** - wx-specific API

**Impact:**
- AI Assistant plugin cannot add commands to menu/toolbar
- Plugins cannot safely emit events from threads
- Panel plugins require manual casting

---

## Objectives

1. **Add ICommandProvider interface** to extension_points.h
2. **Adapt EventBus::emitAsync()** from wxWidgets to Qt
3. **Change IPanelProvider** from void* to QWidget*
4. **Update Python bindings** (pybind11) for Qt types
5. **Test integration** with example plugin

---

## Implementation Plan

### Part 1: ICommandProvider Interface (1h)

**File:** `include/kalahari/core/extension_points.h`

```cpp
/// @brief Command provider interface
///
/// Plugins implementing this interface can register custom commands
/// that automatically appear in menus, toolbars, and Command Palette.
///
/// Example (Python):
/// @code
/// class MyPlugin(ICommandProvider):
///     def get_commands(self):
///         return [
///             Command(
///                 id="myplugin.action",
///                 label="My Action",
///                 category="Plugins",
///                 execute=lambda: print("Executed!")
///             )
///         ]
/// @endcode
class ICommandProvider : public IPlugin {
public:
    ~ICommandProvider() override = default;

    /// @brief Get list of commands to register
    ///
    /// Called once during plugin activation. Commands are automatically
    /// registered with CommandRegistry and appear in menus/toolbars.
    ///
    /// @return Vector of Command structures
    virtual std::vector<gui::Command> getCommands() = 0;
};
```

**Integration with PluginManager:**
```cpp
// plugin_manager.cpp - in loadPlugin():

// After plugin activation, check for command provider
auto commandProvider = dynamic_cast<ICommandProvider*>(instance.get());
if (commandProvider) {
    auto commands = commandProvider->getCommands();
    auto& registry = gui::CommandRegistry::getInstance();
    for (const auto& cmd : commands) {
        registry.registerCommand(cmd);
    }
    logger.info("Registered {} commands from plugin '{}'",
                commands.size(), pluginId);
}
```

---

### Part 2: EventBus Qt Adaptation (1h)

**File:** `src/core/event_bus.cpp`

**Current (wxWidgets):**
```cpp
#ifdef wxUSE_WXWIDGETS
    #include <wx/app.h>
#endif

void EventBus::emitAsync(const Event& event) {
#ifdef wxUSE_WXWIDGETS
    wxTheApp->CallAfter([this, event]() {
        emit(event);
    });
#else
    emit(event);  // Fallback: synchronous
#endif
}
```

**New (Qt6):**
```cpp
#include <QApplication>
#include <QMetaObject>

void EventBus::emitAsync(const Event& event) {
    // Qt GUI thread marshalling via QMetaObject
    QMetaObject::invokeMethod(
        QApplication::instance(),
        [this, event]() {
            emit(event);  // Execute on GUI thread
        },
        Qt::QueuedConnection
    );
}
```

**Header Update:** `include/kalahari/core/event_bus.h`
- Remove wxWidgets comments referencing wxTheApp->CallAfter
- Update docs: "Qt6 QMetaObject::invokeMethod for GUI thread marshalling"

---

### Part 3: IPanelProvider Type Safety (30min)

**File:** `include/kalahari/core/extension_points.h`

**Current (wxWidgets void*):**
```cpp
class IPanelProvider : public IPlugin {
    /// @param parentWindow Parent wxWindow pointer (cast from void*)
    /// @return void* pointer to created wxPanel
    virtual void* createPanel(void* parentWindow) = 0;
};
```

**New (Qt6 QWidget*):**
```cpp
// Forward declaration
class QWidget;

class IPanelProvider : public IPlugin {
public:
    ~IPanelProvider() override = default;

    /// @brief Create a dockable panel widget
    ///
    /// The returned widget will be integrated into MainWindow's QDockWidget system.
    ///
    /// @param parentWindow Parent QWidget pointer (typically MainWindow)
    /// @return QWidget* pointer to created panel
    /// @note Ownership transfers to caller (MainWindow manages lifetime)
    virtual QWidget* createPanel(QWidget* parentWindow) = 0;
};
```

**Python Bindings:** pybind11 automatically handles QWidget* conversion.

---

### Part 4: Python Bindings Update (1h)

**File:** `bindings/python/kalahari_bindings.cpp`

**Add ICommandProvider binding:**
```cpp
// Command struct binding (if not already bound)
py::class_<gui::Command>(m, "Command")
    .def(py::init<>())
    .def_readwrite("id", &gui::Command::id)
    .def_readwrite("label", &gui::Command::label)
    .def_readwrite("category", &gui::Command::category)
    .def_readwrite("tooltip", &gui::Command::tooltip)
    .def_readwrite("execute", &gui::Command::execute)
    .def_readwrite("showInMenu", &gui::Command::showInMenu)
    .def_readwrite("showInToolbar", &gui::Command::showInToolbar);

// ICommandProvider interface
py::class_<ICommandProvider, IPlugin, PyICommandProvider>(m, "ICommandProvider")
    .def(py::init<>())
    .def("get_commands", &ICommandProvider::getCommands);

// Trampoline class for Python inheritance
class PyICommandProvider : public ICommandProvider {
public:
    using ICommandProvider::ICommandProvider;

    std::vector<gui::Command> getCommands() override {
        PYBIND11_OVERRIDE_PURE(
            std::vector<gui::Command>,
            ICommandProvider,
            get_commands
        );
    }
};
```

**Update IPanelProvider binding:**
```cpp
// Change void* to QWidget*
py::class_<IPanelProvider, IPlugin, PyIPanelProvider>(m, "IPanelProvider")
    .def(py::init<>())
    .def("create_panel", &IPanelProvider::createPanel);
```

---

### Part 5: Example Plugin (30min)

**Create:** `plugins/examples/hello_command.py`

```python
import kalahari_api

class HelloCommandPlugin(kalahari_api.ICommandProvider):
    def get_plugin_id(self):
        return "hello-command-plugin"

    def get_version(self):
        return "1.0.0"

    def on_init(self):
        print("HelloCommandPlugin initialized!")

    def on_activate(self):
        print("HelloCommandPlugin activated!")

    def get_commands(self):
        return [
            kalahari_api.Command(
                id="hello.greet",
                label="Say Hello",
                tooltip="Display a greeting message",
                category="Plugins",
                execute=self.say_hello,
                showInMenu=True,
                showInToolbar=False
            )
        ]

    def say_hello(self):
        print("Hello from Plugin!")
        # Emit event
        event = kalahari_api.Event("plugin:greeting", "Hello!")
        kalahari_api.EventBus.emit(event)
```

---

## Files to Modify

1. `include/kalahari/core/extension_points.h` (+30 lines: ICommandProvider)
2. `src/core/event_bus.cpp` (~10 lines changed: wx → Qt)
3. `include/kalahari/core/event_bus.h` (doc update)
4. `bindings/python/kalahari_bindings.cpp` (+50 lines: bindings)
5. `src/core/plugin_manager.cpp` (+15 lines: ICommandProvider integration)

**New files:**
6. `plugins/examples/hello_command.py` (example plugin)

---

## Testing Plan

### Manual Testing:
1. Build project (Qt6 + Python bindings)
2. Place `hello_command.py` in `plugins/` directory
3. Launch Kalahari
4. Check: Plugins menu has "Say Hello" action
5. Click "Say Hello" → check console for "Hello from Plugin!"
6. Check EventBus logs for "plugin:greeting" event

### Integration Points:
- ✅ ICommandProvider: Commands appear in menu
- ✅ EventBus: Async emit works from Qt threads
- ✅ IPanelProvider: Type-safe QWidget* API

---

## Acceptance Criteria

- [x] ICommandProvider interface added to extension_points.h
- [x] PluginManager integrates ICommandProvider with CommandRegistry
- [x] EventBus::emitAsync() uses Qt6 QMetaObject (no wx dependency)
- [x] IPanelProvider uses QWidget* (type-safe)
- [x] Python bindings updated for all interfaces
- [x] Example plugin (hello_command.py) demonstrates command registration
- [x] Build succeeds (no wx references in event_bus.cpp)
- [x] Manual test: Plugin command appears in menu

---

## Dependencies

- Task #00013 (Command Registry) - ✅ COMPLETE
- Qt6 6.5.0+ installed via vcpkg
- Python 3.11 + pybind11

---

## Notes

**Why Now?**
- Phase 2 (Plugin System MVP) will depend on these interfaces
- Command Registry is ready - plugins need to use it
- Better to fix architecture now than refactor later

**Future Work (Phase 2):**
- Plugin Manager UI dialog
- Hot-reload plugins
- Plugin marketplace integration

---

**Created:** 2025-11-21
**Estimated Completion:** 2025-11-21 (same day)
