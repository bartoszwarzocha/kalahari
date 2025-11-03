---
name: kalahari-plugin-system
description: USE ME when creating plugins, implementing Extension Points (IExporter/IAssistant/IPanelProvider), pybind11 bindings, Event Bus events, or working with .kplugin format. Contains complete plugin architecture patterns.
---

# Kalahari Plugin System Expert Skill

## Quick Activation Triggers

USE this skill when you see:
- "plugin", ".kplugin", "Extension Point", "plugin architecture"
- "pybind11", "PYBIND11_MODULE", "PyExporter", "trampoline"
- "Event Bus", "subscribe", "publish", "event callback"
- "manifest.json", "plugin_init()", "PluginManager"
- "IExporter", "IAssistant", "IPanelProvider", "IPlugin"

## Critical Patterns (Kalahari-Specific)

### 1. Extension Point Definition (C++)

```cpp
/// @file extension_points/i_exporter.h
namespace kalahari::plugins {

class IExporter {
public:
    virtual ~IExporter() = default;

    virtual std::string getFileExtension() const = 0;
    virtual std::string getFormatName() const = 0;
    virtual bool exportDocument(const Document& doc, const std::string& path) = 0;
};

using ExporterPtr = std::shared_ptr<IExporter>;

} // namespace kalahari::plugins
```

### 2. pybind11 Trampoline (C++)

```cpp
/// @file python_bindings/exporters.cpp
#include <pybind11/pybind11.h>
namespace py = pybind11;

// Trampoline for Python inheritance
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

PYBIND11_MODULE(kalahari_api, m) {
    py::class_<IExporter, PyExporter, std::shared_ptr<IExporter>>(m, "IExporter")
        .def(py::init<>())
        .def("get_file_extension", &IExporter::getFileExtension)
        .def("get_format_name", &IExporter::getFormatName)
        .def("export_document", &IExporter::exportDocument);
}
```

### 3. Python Plugin Implementation

```python
# plugin.py
from kalahari_api import IExporter

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

def plugin_init(kalahari_api):
    """Entry point called by Kalahari"""
    exporter = DocxExporter()
    kalahari_api.register_exporter(exporter)
```

### 4. .kplugin Format (ZIP)

**Structure:**
```
my-plugin.kplugin
├── manifest.json       # Metadata, dependencies
├── plugin.py           # Entry point
├── __init__.py
└── requirements.txt    # Python deps (optional)
```

**manifest.json:**
```json
{
  "plugin": {
    "id": "com.example.docx-exporter",
    "name": "DOCX Exporter",
    "version": "1.0.0",
    "author": "Author Name"
  },
  "compatibility": {
    "kalahari_api": "^1.0.0",
    "python": ">=3.11,<4.0"
  },
  "entry_point": "plugin.py:plugin_init",
  "extension_points": ["IExporter"]
}
```

### 5. Event Bus Pattern

**C++ Event Bus:**
```cpp
class EventBus {
public:
    static EventBus& getInstance();

    CallbackId subscribe(const std::string& event, EventCallback callback);
    void unsubscribe(const std::string& event, CallbackId id);
    void publish(const std::string& event, const std::string& data);
    void publishOnMainThread(const std::string& event, const std::string& data);
};
```

**Python Usage:**
```python
from kalahari_api import event_bus

def on_document_saved(data):
    import json
    info = json.loads(data)
    print(f"Saved: {info['title']}")

# Subscribe
callback_id = event_bus.subscribe("document.saved", on_document_saved)

# Publish
event_bus.publish("plugin.ready", '{"name": "My Plugin"}')
```

### 6. GIL Handling (CRITICAL)

```cpp
// Calling Python from C++ worker thread
{
    py::gil_scoped_acquire acquire;  // Acquire GIL
    m_pythonCallback();
}  // GIL released

// Calling C++ from Python (long operation)
{
    py::gil_scoped_release release;  // Release GIL
    longRunningCppOperation();
}  // GIL reacquired
```

## Extension Points (MVP)

| Extension Point | Purpose | Example |
|----------------|---------|---------|
| IExporter | Document export | DOCX, PDF, EPUB |
| IAssistant | Graphical assistants | Lion, Meerkat, Elephant |
| IPanelProvider | Custom panels | Statistics, Research |

## Plugin Lifecycle

```
DISCOVERED → VALIDATED → LOADED → ACTIVE → UNLOADED
```

**Loading Process:**
1. Extract .kplugin (ZIP) to temp
2. Parse manifest.json
3. Validate API compatibility
4. Import Python module
5. Call plugin_init(kalahari_api)

## Resources

- **Kalahari Docs**: project_docs/04_plugin_system.md
- **pybind11**: https://pybind11.readthedocs.io/
- **Architecture**: project_docs/03_architecture.md

## Common Events (MVP)

- `document.created`, `document.loaded`, `document.saved`
- `chapter.added`, `chapter.deleted`
- `plugin.loaded`, `plugin.unloaded`
- `export.started`, `export.completed`
