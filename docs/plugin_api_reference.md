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

## Future API (Phase 0 Week 5-6)

### EventBus (Stub)

```python
# Not yet implemented - Week 5
# kalahari_api.EventBus.subscribe("event_name", callback)
# kalahari_api.EventBus.publish("event_name", data)
```

### Extension Points

**Future interfaces (Week 5-6):**

- `IExporter` - Export documents to various formats
- `IPanelProvider` - Add custom UI panels
- `IAssistant` - Add graphical assistant personalities
- `IPlugin` - Base interface for all plugins

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

### Phase 0 Week 3-4 (Current)

```
Kalahari Core (C++)
    ↓ pybind11
kalahari_api Module
    ├── Logger (operational)
    └── (stub placeholders)
```

### Phase 0 Week 5-6 (Coming)

```
Kalahari Core (C++)
    ↓ pybind11
kalahari_api Module
    ├── Logger (operational)
    ├── EventBus (new)
    └── Extension Points
        ├── IExporter
        ├── IPanelProvider
        ├── IAssistant
        └── IPlugin (base)
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
