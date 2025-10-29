# Kalahari Plugin API Reference

**Version:** 0.1 (Phase 0 Week 3-4)
**Status:** Foundation - Stub Implementation
**Last Updated:** 2025-10-29

---

## Overview

The Kalahari Plugin API provides Python plugins with access to core C++ functionality through the `kalahari_api` module (pybind11 bindings).

**Current Phase (Week 3-4):** Only Logger bindings are exposed. EventBus and Extension Points will be available in Week 5-6.

---

## Module: kalahari_api

### Import

```python
import kalahari_api

# Use Logger
kalahari_api.Logger.info("Your message here")
```

### Logger Class

The `Logger` class provides logging functionality for plugins.

**Methods (all static):**

#### `Logger.info(message: str) -> None`

Log an info-level message.

**Example:**
```python
kalahari_api.Logger.info("Plugin initialized successfully")
```

#### `Logger.debug(message: str) -> None`

Log a debug-level message.

**Example:**
```python
kalahari_api.Logger.debug("Debug variable: " + str(some_value))
```

#### `Logger.warn(message: str) -> None`

Log a warning-level message.

**Example:**
```python
kalahari_api.Logger.warn("Plugin performance degraded")
```

#### `Logger.error(message: str) -> None`

Log an error-level message.

**Example:**
```python
kalahari_api.Logger.error("Failed to process file: invalid format")
```

---

## Complete Example Plugin

```python
#!/usr/bin/env python3
"""
Example Kalahari plugin using the API.
"""

import kalahari_api

class ExamplePlugin:
    def __init__(self):
        kalahari_api.Logger.info("ExamplePlugin initializing")

    def do_something(self):
        kalahari_api.Logger.debug("Processing data...")

        try:
            # Your plugin logic here
            result = 42
            kalahari_api.Logger.info(f"Plugin result: {result}")
            return result

        except Exception as e:
            kalahari_api.Logger.error(f"Plugin error: {e}")
            raise

# Create and use plugin
plugin = ExamplePlugin()
plugin.do_something()
```

---

## Testing the API

### Manual Test (Python Script)

```bash
cd build-linux
python3 ../tests/test_python_bindings.py
```

Expected output:
```
✅ Successfully imported kalahari_api
✅ Logger.info() works
✅ Logger.debug() works
✅ Logger.warn() works
✅ Logger.error() works
```

### Automated Tests (C++)

```bash
cd build-linux
ctest --output-on-failure -R "plugin-manager|python-interop"
```

Expected results:
```
Test #8: PluginManager: Singleton pattern ... PASSED
Test #9: PluginManager: Thread safety ... PASSED
Test #10: Python interop: Initialize Python ... PASSED
Test #11: Python interop: Execute simple Python ... PASSED
Test #12: Python interop: Logger accessible ... PASSED
```

### GUI Diagnostics Menu

Run the application with diagnostic mode:

```bash
./build-linux/bin/kalahari --diag
```

Then:
1. Go to **Diagnostics** menu
2. Click **Test Python Bindings (pybind11)**
3. Verify success message

---

## EventBus API (Phase 0 Week 5-6) ✅

The EventBus provides a thread-safe publish/subscribe system for event-driven communication between core and plugins.

### Event Class

Represents an event with a type identifier.

**Constructor:**
```python
Event(type: str)
```

**Attributes:**
- `type` (str): Event type identifier (e.g., "document:opened", "goal:reached")
- `data`: Reserved for future use (currently returns None)

**Example:**
```python
import kalahari_api

# Create event
evt = kalahari_api.Event("document:opened")
print(evt.type)  # "document:opened"
```

### EventBus Class (Singleton)

Thread-safe event bus for pub/sub communication.

#### `EventBus.get_instance() -> EventBus`

Get the EventBus singleton instance.

**Example:**
```python
bus = kalahari_api.EventBus.get_instance()
```

#### `subscribe(event_type: str, callback: callable) -> None`

Subscribe to events of a specific type. The callback will be invoked whenever an event of that type is emitted.

**Parameters:**
- `event_type` (str): Event type to listen for
- `callback` (callable): Function to call when event is emitted. Signature: `callback(event: Event) -> None`

**Example:**
```python
def on_document_opened(event):
    kalahari_api.Logger.info(f"Document opened: {event.type}")

bus = kalahari_api.EventBus.get_instance()
bus.subscribe("document:opened", on_document_opened)
```

#### `unsubscribe(event_type: str) -> None`

Unsubscribe from all listeners for a specific event type.

**Parameters:**
- `event_type` (str): Event type to stop listening for

**Example:**
```python
bus.unsubscribe("document:opened")
```

#### `emit(event: Event) -> None`

Emit an event synchronously. All registered callbacks for the event type will be invoked immediately in the calling thread.

**Parameters:**
- `event` (Event): Event to emit

**Example:**
```python
evt = kalahari_api.Event("document:saved")
bus.emit(evt)
```

#### `emit_async(event: Event) -> None`

Emit an event asynchronously. The event will be queued and callbacks will be invoked on the GUI thread (via wxTheApp->CallAfter). Safe to call from worker threads.

**Parameters:**
- `event` (Event): Event to emit

**Example:**
```python
# Safe to call from background thread
evt = kalahari_api.Event("export:complete")
bus.emit_async(evt)
```

#### `has_subscribers(event_type: str) -> bool`

Check if an event type has any subscribers.

**Parameters:**
- `event_type` (str): Event type to check

**Returns:**
- `bool`: True if at least one subscriber exists

**Example:**
```python
if bus.has_subscribers("document:opened"):
    kalahari_api.Logger.info("Document open listeners registered")
```

#### `get_subscriber_count(event_type: str) -> int`

Get the number of subscribers for an event type.

**Parameters:**
- `event_type` (str): Event type to query

**Returns:**
- `int`: Number of registered callbacks

**Example:**
```python
count = bus.get_subscriber_count("document:opened")
kalahari_api.Logger.info(f"Subscribers: {count}")
```

#### `clear_all() -> None`

Clear all event subscriptions. Use with caution.

**Example:**
```python
bus.clear_all()  # Remove all listeners
```

### Standard Event Types

Kalahari core emits the following standard events:

| Event Type | Description | When Emitted |
|------------|-------------|--------------|
| `document:opened` | Document opened | After successful document load |
| `document:saved` | Document saved | After successful save operation |
| `document:closed` | Document closed | Before document cleanup |
| `editor:selection_changed` | Text selection changed | When user changes selection |
| `editor:content_changed` | Document content modified | After text edit |
| `plugin:loaded` | Plugin loaded successfully | After plugin initialization |
| `plugin:unloaded` | Plugin unloaded | Before plugin cleanup |
| `goal:reached` | User reached writing goal | When word count goal achieved |

### Complete EventBus Example

```python
#!/usr/bin/env python3
import kalahari_api

# Get EventBus instance
bus = kalahari_api.EventBus.get_instance()

# Define event handlers
def on_document_event(event):
    kalahari_api.Logger.info(f"Document event: {event.type}")

def on_goal_reached(event):
    kalahari_api.Logger.info("Congratulations! Goal reached!")

# Subscribe to events
bus.subscribe("document:opened", on_document_event)
bus.subscribe("document:saved", on_document_event)
bus.subscribe("goal:reached", on_goal_reached)

# Emit events (typically done by core, shown here for demo)
evt1 = kalahari_api.Event("document:opened")
bus.emit(evt1)

evt2 = kalahari_api.Event("goal:reached")
bus.emit(evt2)

# Check subscriptions
print(f"Document listeners: {bus.get_subscriber_count('document:opened')}")

# Cleanup
bus.unsubscribe("document:opened")
```

### Extension Points (C++ Only - Week 5-6)

**Note:** Extension point interfaces are currently available only in C++. Python plugin support for extension points will be added in future releases.

**C++ Interfaces:**

- `IExporter` - Export documents to various formats (DOCX, PDF, Markdown)
- `IPanelProvider` - Add custom dockable UI panels
- `IAssistant` - Provide graphical assistant personalities
- `IPlugin` - Base interface all plugins must implement

See C++ API documentation for details.

---

## Error Handling

All Logger methods are safe and won't raise exceptions even if logging fails:

```python
try:
    kalahari_api.Logger.info("This is always safe")
    # Logging failures are silent (logged internally)
except:
    # You won't get here
    pass
```

---

## Plugin System Architecture

### Phase 0 Week 5-6 (Current) ✅

```
Kalahari Core (C++)
    ├── ExtensionPointRegistry (C++ only)
    │   ├── IPlugin (base interface)
    │   ├── IExporter
    │   ├── IPanelProvider
    │   └── IAssistant
    ├── EventBus (thread-safe pub/sub)
    │   ├── Event struct
    │   └── Subscriber management
    └── Logger (multi-level logging)

    ↓ pybind11 bindings

kalahari_api Python Module
    ├── Logger (operational)
    │   ├── .info()
    │   ├── .debug()
    │   ├── .warn()
    │   └── .error()
    └── EventBus (operational)
        ├── Event class
        ├── .get_instance()
        ├── .subscribe()
        ├── .unsubscribe()
        ├── .emit()
        ├── .emit_async()
        ├── .has_subscribers()
        ├── .get_subscriber_count()
        └── .clear_all()
```

### Phase 0 Week 7-8 (Coming)

```
Additional features:
    ├── .kplugin format handler (ZIP-based packages)
    ├── Plugin manifest parsing (JSON metadata)
    ├── Document model (Book → Parts → Chapters)
    └── Extension Points Python bindings
```

---

## Development Workflow

### For Plugin Developers

1. Create a `.py` file in the `plugins/` directory
2. Import `kalahari_api` at the top
3. Use `Logger` for debugging
4. Build and test: `./scripts/build_linux.sh`
5. Run tests: `ctest`
6. Launch app and test via Diagnostics menu

### For Core Developers

1. Add new C++ APIs to `include/kalahari/core/`
2. Expose via pybind11 in `src/bindings/python_bindings.cpp`
3. Add Python tests in `tests/test_python_bindings.py`
4. Add C++ tests in `tests/core/test_python_interop.cpp`
5. Update this document

---

## Troubleshooting

### ImportError: No module named 'kalahari_api'

**Cause:** Module not built or Python path incorrect.

**Solution:**
```bash
# Rebuild
./scripts/build_linux.sh

# Check file exists
ls build-linux/lib/python/kalahari_api.so
ls build-linux/bin/kalahari_api.so

# Test with explicit path
PYTHONPATH=build-linux/lib/python python3 -c "import kalahari_api"
```

### ModuleNotFoundError during plugin load

**Cause:** Plugin directory or manifest structure incorrect.

**Solution:**
- Ensure `plugins/` directory exists at root
- Verify `.kplugin` manifest is valid JSON
- Check application log: Help → Open Log Folder

### Logger methods not working

**Cause:** Python interpreter not initialized.

**Solution:**
- Check application was launched correctly
- Run with: `kalahari --diag` to enable diagnostics
- Check Python 3.11 is available: `python3 --version`

---

## Resources

- **Full architecture:** `project_docs/04_plugin_system.md`
- **Implementation guide:** `project_docs/07_mvp_tasks.md` (Week 3-4)
- **pybind11 docs:** https://pybind11.readthedocs.io/
- **wxWidgets docs:** https://docs.wxwidgets.org/

---

**Document Version:** 0.1
**Status:** Foundation (Phase 0 Week 3-4)
**Next Update:** Phase 0 Week 5-6 (EventBus + Extension Points)
