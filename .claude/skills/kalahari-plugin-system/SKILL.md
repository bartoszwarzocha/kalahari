---
name: kalahari-plugin-system
description: Python plugin architecture expert (pybind11, .kplugin format, Extension Points, Event Bus)
---

# Kalahari Plugin System Expert Skill

## Purpose

This skill provides specialized knowledge for **Kalahari's Python plugin architecture** built on pybind11. It ensures correct implementation of Extension Points, Event Bus, plugin lifecycle, and .kplugin format.

## When to Activate

Claude should use this skill when:
- ✅ Creating new plugins (Python or C++ side)
- ✅ Implementing Extension Points in C++
- ✅ Working with pybind11 bindings
- ✅ Designing Event Bus architecture
- ✅ Creating/loading .kplugin files
- ✅ Plugin API versioning and compatibility
- ✅ Plugin development tools (hot reload, debugging)

## Core Competencies

### 1. Kalahari Plugin Stack

**Components:**
- **Python:** 3.11 Embedded (bundled with application, zero user friction)
- **Binding:** pybind11 (C++/Python interop)
- **Format:** .kplugin (ZIP container with manifest.json + Python code)
- **API:** Semantic Versioning (MAJOR.MINOR.PATCH)
- **Event System:** Async + thread marshalling (thread-safe, GUI-aware)
- **Distribution:** GitHub (MVP) → Own marketplace (Post-1.0)

**Architecture Layers:**
```
┌─────────────────────────────────────┐
│      Python Plugins (.kplugin)      │ ← User-facing features
├─────────────────────────────────────┤
│      Extension Points (C++)         │ ← Plugin API interfaces
├─────────────────────────────────────┤
│      Event Bus (C++)                │ ← Async messaging
├─────────────────────────────────────┤
│      pybind11 Bindings              │ ← C++ ↔ Python bridge
├─────────────────────────────────────┤
│      Core Application (C++)         │ ← Business logic
└─────────────────────────────────────┘
```

### 2. Extension Points Pattern

**Definition (C++):**
```cpp
/// @file extension_points/i_exporter.h
#pragma once

#include <string>
#include <vector>
#include <memory>

namespace kalahari {
namespace plugins {

/// Extension Point: Document exporter
class IExporter {
public:
    virtual ~IExporter() = default;

    /// Get supported file extension (e.g., "docx", "pdf")
    virtual std::string getFileExtension() const = 0;

    /// Get human-readable format name
    virtual std::string getFormatName() const = 0;

    /// Export document to file
    /// @param document Document to export
    /// @param filePath Target file path
    /// @return true if successful
    virtual bool exportDocument(
        const Document& document,
        const std::string& filePath
    ) = 0;
};

using ExporterPtr = std::shared_ptr<IExporter>;

} // namespace plugins
} // namespace kalahari
```

**Registration (C++):**
```cpp
/// @file plugin_manager.h
class PluginManager {
public:
    /// Register exporter extension
    void registerExporter(ExporterPtr exporter);

    /// Get all registered exporters
    std::vector<ExporterPtr> getExporters() const;

    /// Get exporter by extension
    ExporterPtr getExporter(const std::string& extension) const;

private:
    std::map<std::string, ExporterPtr> m_exporters;
};
```

**Python Implementation:**
```python
# plugin_docx_exporter/exporter.py
from kalahari import IExporter

class DocxExporter(IExporter):
    def get_file_extension(self) -> str:
        return "docx"

    def get_format_name(self) -> str:
        return "Microsoft Word Document"

    def export_document(self, document, file_path: str) -> bool:
        from docx import Document as DocxDocument
        doc = DocxDocument()
        # ... conversion logic ...
        doc.save(file_path)
        return True

# Register with Kalahari
def plugin_init(kalahari_api):
    exporter = DocxExporter()
    kalahari_api.register_exporter(exporter)
```

### 3. pybind11 Bindings

**Exposing Extension Points:**
```cpp
/// @file python_bindings/exporters.cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "extension_points/i_exporter.h"

namespace py = pybind11;

// Trampoline class for Python inheritance
class PyExporter : public IExporter {
public:
    using IExporter::IExporter;

    std::string getFileExtension() const override {
        PYBIND11_OVERRIDE_PURE(std::string, IExporter, get_file_extension);
    }

    std::string getFormatName() const override {
        PYBIND11_OVERRIDE_PURE(std::string, IExporter, get_format_name);
    }

    bool exportDocument(const Document& doc, const std::string& path) override {
        PYBIND11_OVERRIDE_PURE(bool, IExporter, export_document, doc, path);
    }
};

PYBIND11_MODULE(kalahari, m) {
    m.doc() = "Kalahari Plugin API";

    py::class_<IExporter, PyExporter, std::shared_ptr<IExporter>>(m, "IExporter")
        .def(py::init<>())
        .def("get_file_extension", &IExporter::getFileExtension)
        .def("get_format_name", &IExporter::getFormatName)
        .def("export_document", &IExporter::exportDocument);

    // Expose Document class
    py::class_<Document>(m, "Document")
        .def("get_title", &Document::getTitle)
        .def("get_content", &Document::getContent)
        .def("get_chapters", &Document::getChapters);
}
```

**Python GIL Handling:**
```cpp
// When calling Python from C++ worker thread
{
    py::gil_scoped_acquire acquire;  // Acquire GIL
    m_pythonCallback();              // Call Python
}  // GIL released automatically

// When calling C++ from Python (long operation)
{
    py::gil_scoped_release release;  // Release GIL
    longRunningCppOperation();       // Other threads can run
}  // GIL reacquired automatically
```

### 4. Event Bus Architecture

**C++ Event Bus:**
```cpp
/// @file event_bus.h
#pragma once

#include <functional>
#include <string>
#include <map>
#include <vector>
#include <mutex>

namespace kalahari {
namespace events {

using EventCallback = std::function<void(const std::string& eventData)>;
using CallbackId = int;

class EventBus {
public:
    static EventBus& getInstance();

    /// Subscribe to event
    CallbackId subscribe(const std::string& eventName, EventCallback callback);

    /// Unsubscribe from event
    void unsubscribe(const std::string& eventName, CallbackId id);

    /// Publish event (async)
    void publish(const std::string& eventName, const std::string& eventData);

    /// Publish event on GUI thread (for wxWidgets)
    void publishOnMainThread(const std::string& eventName, const std::string& eventData);

private:
    EventBus() = default;
    std::map<std::string, std::map<CallbackId, EventCallback>> m_subscribers;
    std::mutex m_mutex;
    CallbackId m_nextId = 1;
};

} // namespace events
} // namespace kalahari
```

**Python API:**
```python
# In plugin code
from kalahari import event_bus

def on_document_saved(data):
    import json
    info = json.loads(data)
    print(f"Document saved: {info['title']}")

# Subscribe
callback_id = event_bus.subscribe("document.saved", on_document_saved)

# Publish
event_bus.publish("plugin.ready", '{"name": "My Plugin"}')

# Unsubscribe
event_bus.unsubscribe("document.saved", callback_id)
```

### 5. .kplugin Format

**Structure:**
```
my-plugin.kplugin  (ZIP file)
├── manifest.json       # Metadata, dependencies, entry point
├── plugin.py           # Main plugin file
├── __init__.py         # Python package marker
├── requirements.txt    # Python dependencies (optional)
├── assets/             # Images, icons, etc.
│   └── icon.png
├── locales/            # Translations (optional)
│   ├── en/kalahari.mo
│   └── pl/kalahari.mo
└── tests/              # Unit tests (optional)
    └── test_plugin.py
```

**manifest.json:**
```json
{
  "plugin": {
    "id": "com.example.docx-exporter",
    "name": "DOCX Exporter",
    "version": "1.0.0",
    "description": "Export documents to Microsoft Word format",
    "author": "Example Author",
    "license": "MIT",
    "homepage": "https://github.com/example/docx-exporter"
  },
  "compatibility": {
    "kalahari_api": "^1.0.0",
    "python": ">=3.11,<4.0"
  },
  "entry_point": "plugin.py:plugin_init",
  "dependencies": {
    "python": ["python-docx>=0.8.11"]
  },
  "extension_points": ["IExporter"],
  "events": {
    "subscribes": ["document.saved", "document.loaded"],
    "publishes": ["export.started", "export.completed", "export.failed"]
  }
}
```

**plugin.py (Entry Point):**
```python
"""
DOCX Exporter Plugin for Kalahari
"""
from kalahari import IExporter, event_bus
import logging

logger = logging.getLogger(__name__)

class DocxExporter(IExporter):
    # ... implementation ...

def plugin_init(kalahari_api):
    """
    Plugin entry point called by Kalahari
    Args:
        kalahari_api: Kalahari Plugin API object
    """
    logger.info("Initializing DOCX Exporter plugin")

    # Register exporter
    exporter = DocxExporter()
    kalahari_api.register_exporter(exporter)

    # Subscribe to events
    event_bus.subscribe("document.saved", on_document_saved)

    logger.info("DOCX Exporter plugin initialized")

def plugin_shutdown(kalahari_api):
    """
    Plugin shutdown (optional, called on unload)
    """
    logger.info("Shutting down DOCX Exporter plugin")
```

### 6. Plugin Lifecycle

**States:**
```
DISCOVERED → VALIDATED → LOADED → ACTIVE → UNLOADED
     ↓           ↓          ↓         ↓        ↓
   ERROR       ERROR      ERROR     ERROR   ERROR
```

**C++ Plugin Manager:**
```cpp
class PluginManager {
public:
    /// Discover plugins in directory
    std::vector<PluginInfo> discoverPlugins(const std::string& pluginsDir);

    /// Load plugin from .kplugin file
    bool loadPlugin(const std::string& pluginPath);

    /// Unload plugin
    bool unloadPlugin(const std::string& pluginId);

    /// Get plugin info
    PluginInfo getPluginInfo(const std::string& pluginId) const;

    /// Check API compatibility
    bool isCompatible(const std::string& apiVersion) const;

private:
    std::map<std::string, LoadedPlugin> m_plugins;
    py::scoped_interpreter m_pythonInterpreter;
};
```

**Loading Process:**
1. Extract .kplugin (ZIP) to temp directory
2. Parse manifest.json
3. Validate API version compatibility
4. Install Python dependencies (`pip install -r requirements.txt`)
5. Import Python module
6. Call `plugin_init(kalahari_api)`
7. Register as ACTIVE

### 7. API Versioning

**Semantic Versioning:**
- **MAJOR:** Breaking changes (incompatible API)
- **MINOR:** New features (backward compatible)
- **PATCH:** Bug fixes (backward compatible)

**Compatibility Check:**
```cpp
bool PluginManager::isCompatible(const std::string& requiredVersion) const {
    // Parse "^1.2.3" (caret) or "~1.2.3" (tilde)
    // ^ = compatible with 1.x.x (< 2.0.0)
    // ~ = compatible with 1.2.x (< 1.3.0)

    auto [major, minor, patch] = parseVersion(requiredVersion);
    auto [currentMajor, currentMinor, currentPatch] = getCurrentAPIVersion();

    if (requiredVersion[0] == '^') {
        return major == currentMajor && currentMinor >= minor;
    } else if (requiredVersion[0] == '~') {
        return major == currentMajor && minor == currentMinor;
    }
    return false;
}
```

### 8. Extension Points Available (MVP)

| Extension Point | Purpose | Example Plugin |
|-----------------|---------|----------------|
| **IExporter** | Document export formats | DOCX, PDF, EPUB, Markdown |
| **IImporter** | Document import formats | Markdown, TXT, RTF |
| **IAssistant** | Graphical assistants | Lion, Meerkat, Elephant |
| **IStatistics** | Statistics calculators | Word count, reading time, pacing |
| **ITheme** | UI themes | Dark, Light, Savanna |

**Phase 1+ Extension Points:**
- **ISpellChecker:** Spell checking engines
- **IGrammarChecker:** Grammar validation
- **ICloudSync:** Cloud storage integrations
- **IVersionControl:** Git, SVN integrations

### 9. Testing Patterns

**C++ Unit Tests (Catch2):**
```cpp
TEST_CASE("PluginManager loads valid plugin") {
    PluginManager manager;

    REQUIRE(manager.loadPlugin("test_plugins/valid.kplugin"));
    REQUIRE(manager.getPluginInfo("com.test.valid").state == PluginState::ACTIVE);
}

TEST_CASE("PluginManager rejects incompatible API") {
    PluginManager manager;

    REQUIRE_FALSE(manager.loadPlugin("test_plugins/incompatible.kplugin"));
}
```

**Python Plugin Tests:**
```python
# tests/test_plugin.py
import pytest
from plugin import DocxExporter

def test_exporter_extension():
    exporter = DocxExporter()
    assert exporter.get_file_extension() == "docx"

def test_export_document(tmp_path):
    exporter = DocxExporter()
    output = tmp_path / "test.docx"

    success = exporter.export_document(mock_document, str(output))

    assert success
    assert output.exists()
```

## Resources

- **Official Docs:** project_docs/04_plugin_system.md
- **Architecture:** project_docs/03_architecture.md
- **pybind11 Docs:** https://pybind11.readthedocs.io/
- **Python Embedding:** https://docs.python.org/3/extending/embedding.html

## Quick Reference

**Most Used pybind11 Features:**
- `py::class_<T>` - Expose C++ class
- `py::def()` - Expose function
- `py::init<>()` - Constructor
- `py::gil_scoped_acquire` - Acquire GIL
- `py::gil_scoped_release` - Release GIL
- `PYBIND11_OVERRIDE_PURE` - Virtual method override
- `py::module_` - Python module

**Event Bus Events (MVP):**
- `document.created`, `document.loaded`, `document.saved`
- `chapter.added`, `chapter.deleted`, `chapter.moved`
- `plugin.loaded`, `plugin.unloaded`, `plugin.error`
- `export.started`, `export.completed`, `export.failed`
- `app.startup`, `app.shutdown`

---

**Skill Version:** 1.0
**Last Updated:** 2025-10-26
**Framework Compatibility:** Kalahari Phase 0+
