# Plugin System

> **Extensibility Architecture** - Plugin API specification and extension points

**Status:** ✅ Complete
**Version:** 1.0
**Last Updated:** 2025-10-25

---

## Overview

### Philosophy

Kalahari's plugin system is built on these principles:

1. **Extensibility from Day Zero** - Not retrofitted, core design element
2. **Python for Flexibility** - Embedded Python 3.11 via pybind11
3. **Type-Safe Interfaces** - C++ Extension Points with clear contracts
4. **Event-Driven Communication** - Async, thread-safe EventBus
5. **Isolation & Safety** - Plugin errors don't crash app

### Key Components

```
┌────────────────────────────────────────┐
│         Kalahari Core (C++)            │
│  ┌──────────────────────────────────┐  │
│  │   Extension Points (Interfaces)  │  │
│  │  IExporter, IPanel, IAssistant  │  │
│  └──────────────────────────────────┘  │
│              ↕ pybind11                │
│  ┌──────────────────────────────────┐  │
│  │    Python Plugins (.kplugin)     │  │
│  │  DOCX Export, AI Assistant, ...  │  │
│  └──────────────────────────────────┘  │
│              ↕ Events                  │
│  ┌──────────────────────────────────┐  │
│  │        EventBus (C++)            │  │
│  │  Thread-safe, async, GUI-aware   │  │
│  └──────────────────────────────────┘  │
└────────────────────────────────────────┘
```

---

## Extension Points

### IExporter - Export Document to Format

**C++ Interface:**
```cpp
/// @file plugins/interfaces/i_exporter.h

class IExporter {
public:
    virtual ~IExporter() = default;
    
    /// Display name (e.g., "Microsoft Word")
    virtual std::string getName() const = 0;
    
    /// File extensions (e.g., [".docx"])
    virtual std::vector<std::string> getExtensions() const = 0;
    
    /// Export document to file
    /// @return true on success, false on failure
    virtual bool exportDocument(const Document& doc, const std::string& path) = 0;
};
```

**Python Implementation (example):**
```python
from kalahari.plugin_api import IExporter

class DocxExporter(IExporter):
    def get_name(self) -> str:
        return "Microsoft Word"
    
    def get_extensions(self) -> list[str]:
        return [".docx"]
    
    def export_document(self, doc, path: str) -> bool:
        from docx import Document as DocxDoc
        
        docx_doc = DocxDoc()
        docx_doc.add_heading(doc.get_title(), 0)
        
        for chapter in doc.get_chapters():
            docx_doc.add_heading(chapter.get_title(), 1)
            docx_doc.add_paragraph(chapter.get_content())
        
        docx_doc.save(path)
        return True
```

### IPanelProvider - Custom UI Panel

**C++ Interface:**
```cpp
class IPanelProvider {
public:
    virtual ~IPanelProvider() = default;
    
    /// Panel unique ID
    virtual std::string getPanelId() const = 0;
    
    /// Panel display title
    virtual std::string getPanelTitle() const = 0;
    
    /// Create panel widget
    virtual wxWindow* createPanel(wxWindow* parent) = 0;
    
    /// Destroy panel (cleanup)
    virtual void destroyPanel(wxWindow* panel) = 0;
};
```

### ICommandProvider - Register Commands

**C++ Interface:**
```cpp
class ICommandProvider {
public:
    virtual ~ICommandProvider() = default;
    
    /// Get all commands this plugin provides
    virtual std::vector<Command> getCommands() const = 0;
};

struct Command {
    std::string id;              // "plugin.ai.suggest"
    std::string label;           // "AI Suggestion"
    std::function<void()> execute;
    std::function<bool()> isEnabled;
};
```

---

## Plugin API

### Document Class (exposed to Python)

```cpp
PYBIND11_MODULE(kalahari_core, m) {
    py::class_<Document>(m, "Document")
        .def("get_title", &Document::getTitle)
        .def("set_title", &Document::setTitle)
        .def("get_chapters", &Document::getChapters)
        .def("add_chapter", &Document::addChapter)
        .def("get_word_count", &Document::getWordCount);
    
    py::class_<Chapter>(m, "Chapter")
        .def("get_id", &Chapter::getId)
        .def("get_title", &Chapter::getTitle)
        .def("set_title", &Chapter::setTitle)
        .def("get_content", &Chapter::getContent)
        .def("set_content", &Chapter::setContent)
        .def("get_word_count", &Chapter::getWordCount);
}
```

**Python usage:**
```python
# In plugin code
doc = kalahari.get_current_document()
print(f"Title: {doc.get_title()}")
print(f"Chapters: {len(doc.get_chapters())}")

for chapter in doc.get_chapters():
    print(f"  {chapter.get_title()}: {chapter.get_word_count()} words")
```

---

## Event System

### Event Types

```python
class EventType:
    # Document events
    DOCUMENT_CREATED = "document.created"
    DOCUMENT_OPENED = "document.opened"
    DOCUMENT_CLOSED = "document.closed"
    DOCUMENT_SAVED = "document.saved"
    
    # Text editing
    TEXT_CHANGED = "text.changed"
    SELECTION_CHANGED = "selection.changed"
    
    # Chapter management
    CHAPTER_ADDED = "chapter.added"
    CHAPTER_DELETED = "chapter.deleted"
    CHAPTER_RENAMED = "chapter.renamed"
    
    # User activity
    USER_IDLE = "user.idle"
    USER_ACTIVE = "user.active"
    
    # Goals
    GOAL_REACHED = "goal.reached"
    STREAK_ACHIEVED = "streak.achieved"
```

### Subscribing to Events

```python
class MyPlugin:
    def on_activate(self):
        # Subscribe to event
        self.event_bus.subscribe(
            EventType.TEXT_CHANGED,
            self.on_text_changed
        )
    
    def on_text_changed(self, event):
        # event.data contains {"chapter_id": "ch1", "text": "..."}
        chapter_id = event.data["chapter_id"]
        text = event.data["text"]
        # Update statistics, etc.
```

---

## Plugin Manifest

### manifest.json Specification

**Required fields:**
```json
{
  "api_version": "1.0",
  "id": "com.kalahari.docx_exporter",
  "name": "DOCX Exporter",
  "version": "1.0.0",
  "author": "Kalahari Team",
  "entry_point": "plugin.DocxExporter"
}
```

**Optional fields:**
```json
{
  "description": "Export documents to Microsoft Word format",
  "website": "https://kalahari.app/plugins/docx",
  "license": "MIT",
  
  "requires": {
    "kalahari": ">=1.0.0",
    "python": ">=3.11"
  },
  
  "dependencies": {
    "python": ["python-docx>=0.8.11"]
  },
  
  "extension_points": ["exporter"],
  
  "permissions": [
    "filesystem.write"
  ]
}
```

---

## Plugin Lifecycle

### State Machine

```
Discovered → Loaded → Initialized → Activated
     ↓         ↓          ↓            ↓
Unloaded  ← Shutdown ← Deactivated ← [can reactivate]
```

**States:**

1. **Discovered** - Found in plugins/ directory
2. **Loaded** - Python module imported
3. **Initialized** - `on_init()` called
4. **Activated** - `on_activate()` called, UI registered
5. **Deactivated** - `on_deactivate()` called, UI hidden
6. **Shutdown** - `on_shutdown()` called
7. **Unloaded** - Python module unloaded

### Plugin Interface Methods

```python
class IPlugin:
    def on_init(self):
        """Called once after plugin is loaded"""
        pass
    
    def on_activate(self):
        """Called when plugin is activated (app start or user enables)"""
        pass
    
    def on_deactivate(self):
        """Called when plugin is deactivated (app shutdown or user disables)"""
        pass
    
    def on_shutdown(self):
        """Called before plugin is unloaded"""
        pass
```

---

## API Versioning

### Semantic Versioning

**Format:** `MAJOR.MINOR.PATCH`

- **MAJOR:** Breaking changes (plugins must update)
- **MINOR:** New features (backward compatible)
- **PATCH:** Bug fixes

**Compatibility check:**
```cpp
bool isCompatible(const std::string& pluginApiVersion) {
    auto plugin_ver = Version::parse(pluginApiVersion);
    auto current_ver = Version::parse(KALAHARI_API_VERSION);
    
    // MAJOR must match
    if (plugin_ver.major != current_ver.major) return false;
    
    // Plugin can use older MINOR
    if (plugin_ver.minor > current_ver.minor) return false;
    
    return true;
}
```

---

## Thread Safety

### Guidelines

1. **Plugin operations on worker threads:**
```python
def export_document(self, doc, path):
    # This runs on worker thread
    # Release GIL during long operations
    result = do_heavy_work()
    
    # Marshal to GUI thread for UI updates
    kalahari.call_after(lambda: self.show_success())
```

2. **Event handlers marshalled to GUI thread automatically**

3. **Python GIL handled by pybind11**

---

## Error Handling

### Plugin Crash Isolation

**C++ wrapper:**
```cpp
bool PluginManager::executePlugin(const std::string& id, 
                                  std::function<void()> fn) {
    try {
        py::gil_scoped_acquire acquire;
        fn();
        return true;
    } catch (const py::error_already_set& e) {
        spdlog::error("Plugin {} failed: {}", id, e.what());
        wxLogError("Plugin error: %s", e.what());
        
        // Plugin crash doesn't crash app
        return false;
    }
}
```

---

## Complete Example: DOCX Exporter Plugin

### Directory Structure

```
plugins/docx_exporter/
├── manifest.json
├── plugin.py
├── requirements.txt
└── assets/
    └── icon.png
```

### manifest.json

```json
{
  "api_version": "1.0",
  "id": "com.kalahari.docx_exporter",
  "name": "DOCX Exporter",
  "version": "1.0.0",
  "author": "Kalahari Team",
  "description": "Export documents to Microsoft Word format",
  "entry_point": "plugin.DocxExporter",
  "extension_points": ["exporter"],
  "dependencies": {
    "python": ["python-docx>=0.8.11"]
  }
}
```

### plugin.py

```python
from kalahari.plugin_api import IExporter, IPlugin

class DocxExporter(IPlugin, IExporter):
    def on_init(self):
        print("DOCX Exporter initialized")
    
    def on_activate(self):
        print("DOCX Exporter activated")
    
    def get_name(self) -> str:
        return "Microsoft Word"
    
    def get_extensions(self) -> list[str]:
        return [".docx"]
    
    def export_document(self, doc, path: str) -> bool:
        try:
            from docx import Document as DocxDoc
            from docx.shared import Pt, Inches
            
            docx_doc = DocxDoc()
            
            # Title
            title = docx_doc.add_heading(doc.get_title(), 0)
            
            # Chapters
            for chapter in doc.get_chapters():
                docx_doc.add_heading(chapter.get_title(), 1)
                
                # Content (strip RTF, convert to plain text)
                content = self._strip_rtf(chapter.get_content())
                docx_doc.add_paragraph(content)
                
                docx_doc.add_page_break()
            
            docx_doc.save(path)
            return True
            
        except Exception as e:
            print(f"Export failed: {e}")
            return False
    
    def _strip_rtf(self, rtf_text: str) -> str:
        # Simple RTF → plain text conversion
        # (Real implementation would use RTF parser)
        import re
        text = re.sub(r'\[a-z]+\d* ', '', rtf_text)
        text = re.sub(r'[{}]', '', text)
        return text
```

---

## Summary

This plugin system provides:

✅ **Extension Points** - IExporter, IPanelProvider, ICommandProvider
✅ **Python API** - Document, Chapter, EventBus exposed
✅ **Event System** - 15+ event types, async subscription
✅ **Manifest Format** - Complete specification
✅ **Lifecycle Management** - 7-state machine
✅ **API Versioning** - Semantic versioning with compatibility checks
✅ **Thread Safety** - GIL handling, GUI marshalling
✅ **Error Isolation** - Plugin crashes don't crash app
✅ **Complete Example** - DOCX Exporter plugin

**Next Steps:**
1. Implement PluginManager (C++)
2. Create pybind11 bindings
3. Build 4 MVP plugins (DOCX, Markdown, Statistics, Assistant Lion)
4. Test plugin lifecycle

---

**Document Version:** 1.0
**Last Updated:** 2025-10-25
**Next Review:** Start of Phase 2 (Plugin System MVP)

---

## Developer Tools & Plugin Creation Support

### Overview

To support external plugin developers and reduce barriers to entry, Kalahari will provide comprehensive tooling for plugin creation. This is planned for **Phase 4-5** (Q3 2026) after the plugin API stabilizes.

### Why Not Earlier?

**Timing Rationale:**
1. **API Stability** - Plugin API will evolve significantly during Phase 1-3
2. **Community Feedback** - Need real developer pain points before building tools
3. **Business Context** - Plugin marketplace (Phase 5) requires quality tooling
4. **Resource Optimization** - Documentation + CLI tools provide 80% value with 20% effort

### Phase 0-1: Foundation (Q1 2026) ✅

**Current Status:**
- ✅ Plugin system working (Task #00011)
- ✅ hello_plugin.kplugin example
- ✅ Integration tests demonstrating usage
- 📝 Basic documentation (docs/plugin_development_guide.md)

**Deliverables:**
```
docs/
├── plugin_development_guide.md   # Step-by-step tutorial
└── plugin_api_reference.md       # Extension Points + Event Bus

examples/
└── hello_plugin/
    ├── manifest.json
    └── plugin.py

tests/
└── plugins/
    └── hello_plugin.kplugin      # Working reference
```

### Phase 2-3: Documentation Expansion (Q2 2026)

**Goal:** Comprehensive documentation as API stabilizes

**Deliverables:**
- ✅ Complete API reference (all Extension Points)
- ✅ Event Bus patterns and best practices
- ✅ Advanced plugin examples (all features)
- ✅ Common pitfalls and troubleshooting
- ✅ Performance optimization guide

**Documentation Structure:**
```markdown
docs/plugin_development_guide.md
├── 1. Quick Start (Hello Plugin)
├── 2. Plugin Architecture
│   ├── Extension Points (IExporter, IPanelProvider, IAssistant)
│   ├── Event Bus (pub/sub patterns)
│   └── Lifecycle Hooks (init, activate, deactivate)
├── 3. Manifest Reference
│   ├── Required fields
│   ├── Optional fields
│   └── Dependency management
├── 4. Python API
│   ├── kalahari_api module
│   ├── Document model
│   └── Event handling
├── 5. Testing
│   ├── Unit testing
│   ├── Integration testing
│   └── Debugging
├── 6. Best Practices
│   ├── Performance
│   ├── Thread safety
│   └── Error handling
└── 7. Distribution & Marketplace
```

### Phase 4: Developer Tools in Application (Q3 2026)

**Goal:** Full GUI-based plugin creation workflow

#### Developer Mode (Optional Feature)

**Activation:** Settings → Advanced → Enable Developer Mode

**Menu Location:** Tools → Developer Tools

**Features:**

1. **Plugin Creator Wizard 🧙**
   ```
   Step 1: Basic Info (name, id, author, description)
   Step 2: Extension Points (which interfaces to implement)
   Step 3: Events (which events to subscribe to)
   Step 4: Dependencies (Python packages)
   Step 5: Generate Template (creates manifest.json + plugin.py skeleton)
   ```

2. **Plugin Validator 🔍**
   ```
   - Manifest validation (required fields, version format)
   - Python syntax check (AST parsing)
   - Entry point verification (class exists, correct signature)
   - Dependency check (packages available)
   - Extension Point implementation check (methods present)
   - Asset validation (icons, images exist)
   ```

3. **Plugin Packager 📦**
   ```
   - Select plugin directory
   - Validate structure
   - Create .kplugin ZIP
   - Digital signature (Phase 5)
   - Test installation
   ```

4. **Live Plugin Reload 🔄**
   ```
   - Watch plugin directory for changes
   - Auto-reload on save
   - Error display in real-time
   - Debug console for print() output
   ```

**UI Mockup:**
```
┌─────────────────────────────────────────────┐
│ Developer Tools                        [X]  │
├─────────────────────────────────────────────┤
│ Tabs: [Creator] [Validator] [Packager]     │
├─────────────────────────────────────────────┤
│ Plugin Creator Wizard                       │
│                                             │
│ Plugin Name: ________________               │
│ Plugin ID:   org.example._____              │
│ Author:      ________________               │
│                                             │
│ Extension Points:                           │
│ ☐ IExporter                                 │
│ ☑ IPanelProvider                            │
│ ☐ IAssistant                                │
│                                             │
│           [< Back]  [Next >]  [Generate]    │
└─────────────────────────────────────────────┘
```

### Phase 4: CLI Tools (Q3 2026)

**Goal:** Automation for power users and CI/CD

**Tools Location:** `tools/plugin_dev/`

#### 1. `create_plugin.py`
```bash
$ python tools/plugin_dev/create_plugin.py \
    --name "My Plugin" \
    --id "org.example.myplugin" \
    --author "Your Name" \
    --extension-points IExporter \
    --output ./my_plugin/

Created:
  my_plugin/
  ├── manifest.json
  └── plugin.py
```

#### 2. `validate_plugin.py`
```bash
$ python tools/plugin_dev/validate_plugin.py ./my_plugin/

✅ Manifest valid
✅ Python syntax OK
✅ Entry point found: plugin:MyPlugin
✅ Extension Points implemented: IExporter
⚠️  Warning: No icon.png found
```

#### 3. `package_plugin.py`
```bash
$ python tools/plugin_dev/package_plugin.py \
    --input ./my_plugin/ \
    --output ./my_plugin.kplugin

Created: my_plugin.kplugin (2.3 KB)
Files: manifest.json, plugin.py, icon.png
```

### Phase 5: Advanced Features (Q3 2026)

#### Plugin Manifest Schema (JSON Schema)

**File:** `schemas/plugin_manifest_schema.json`

**Purpose:** VSCode autocomplete and validation

**Usage:**
```json
{
  "$schema": "https://kalahari.app/schemas/plugin_manifest.json",
  "id": "org.example.plugin",
  ...
}
```

**Benefits:**
- Real-time validation in VSCode
- Autocomplete for all fields
- Inline documentation tooltips
- Reduces manifest errors

#### Plugin Template Repository

**Location:** `examples/plugin_templates/`

**Templates:**
```
examples/plugin_templates/
├── minimal/              # Hello World
├── exporter/            # IExporter implementation
├── panel/               # IPanelProvider with UI
├── assistant/           # IAssistant with personality
├── advanced/            # All features (events, permissions, dependencies)
└── premium/             # Marketplace-ready structure
```

### Implementation Priority

**Phase 0-1 (NOW):**
- ✅ Working plugin system
- ✅ hello_plugin.kplugin example
- ✅ Basic docs

**Phase 2-3 (Q2 2026):**
- 📝 Complete documentation
- 📝 Advanced examples
- 📝 Best practices guide

**Phase 4 (Q3 2026):**
- 🔨 Developer Mode (GUI tools)
- 🔨 CLI tools
- 🔨 JSON Schema

**Phase 5 (Q3 2026):**
- 🔨 Plugin marketplace integration
- 🔨 Digital signatures
- 🔨 Automated testing

### Analogies from Other Platforms

| Platform | Developer Tools | Timing |
|----------|----------------|--------|
| **VSCode** | Extension Generator (Yeoman) | After API stable |
| **WordPress** | Plugin Creator in dashboard | Years after launch |
| **Sublime Text** | Documentation only | Never added GUI |
| **Obsidian** | Documentation + Templates | After community growth |

**Kalahari Strategy:** Follow VSCode model - CLI tools early, GUI tools when API stable and community exists.

### Success Metrics

**Phase 4 Success:**
- 🎯 10+ external plugin developers
- 🎯 50+ community plugins
- 🎯 90% of new plugins pass validation first try
- 🎯 Average plugin creation time < 30 minutes (using wizard)

**Phase 5 Success:**
- 🎯 Plugin marketplace with 100+ plugins
- 🎯 Community-contributed plugins in top downloads
- 🎯 External developers contributing to core Extension Points

---

## Plugin Security (OpenSpec #00038)

### Signature Verification

Plugins are verified using **Ed25519 digital signatures** before loading.

**Components:**
- `PluginSignature` - Verifies .kplugin.sig files
- `TrustedKeys` - Manages trusted publisher keys
- `plugins.allowUnsigned` - Development mode setting

**Signature File Format (.kplugin.sig):**
```json
{
  "version": 1,
  "algorithm": "ed25519",
  "archive_hash": "sha256:<hex-hash>",
  "signature": "<base64-signature>",
  "signed_by": "kalahari-official",
  "signed_at": "2025-01-15T10:30:00Z"
}
```

**Verification Flow:**
1. `discoverPlugins()` finds .kplugin file
2. Check for corresponding .kplugin.sig file
3. If no .sig: check `allowUnsigned` setting
4. If .sig exists: verify against trusted keys
5. Only load plugins with valid signatures (or unsigned in dev mode)

**Trusted Publishers:**
- Built-in keys in `resources/keys/trusted_publishers.json`
- User keys configurable via settings

**Development Mode:**
```cpp
// In settings.json
"plugins.allowUnsigned": true  // WARNING: Disables signature checks
```

### Thread Safety

**mutex+GIL Deadlock Prevention (OpenSpec #00038):**

Plugin operations use a 3-phase pattern to prevent deadlock:

```cpp
// Phase 1: Mutex only - copy data
{
    std::lock_guard<std::mutex> lock(m_mutex);
    localCopy = m_data;
}

// Phase 2: GIL only - Python operations (no mutex!)
{
    py::gil_scoped_acquire gil;
    pythonResult = callPython(localCopy);
}

// Phase 3: Mutex only - save results with double-check
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (stillValid()) {
        m_result = pythonResult;
    }
}
```

---

## Summary (Updated)

This plugin system provides:

✅ **Extension Points** - IExporter, IPanelProvider, ICommandProvider
✅ **Python API** - Document, Chapter, EventBus exposed
✅ **Event System** - 15+ event types, async subscription
✅ **Manifest Format** - Complete specification
✅ **Lifecycle Management** - 7-state machine
✅ **API Versioning** - Semantic versioning with compatibility checks
✅ **Thread Safety** - GIL handling, GUI marshalling
✅ **Error Isolation** - Plugin crashes don't crash app
✅ **Complete Example** - hello_plugin.kplugin (Task #00011)
🔜 **Developer Tools** - Planned Phase 4-5 (Q3 2026)
🔜 **Plugin Marketplace** - Planned Phase 5 (Q3 2026)

**Next Steps:**
1. ✅ Implement PluginManager (C++) - DONE Task #00011
2. ✅ Create pybind11 bindings - DONE Task #00009-00010
3. Build 4 MVP plugins (DOCX, Markdown, Statistics, Assistant Lion) - Phase 1
4. Expand documentation - Phase 2-3
5. Develop Developer Tools - Phase 4-5

---

**Document Version:** 1.2
**Last Updated:** 2025-12-17 (Added Security section - OpenSpec #00038)
**Next Review:** Start of Phase 2 (Plugin System MVP)
