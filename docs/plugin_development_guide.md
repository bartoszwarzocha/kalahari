# Kalahari Plugin Development Guide

> **Status:** 📝 Draft - Phase 0 Week 6
> **Last Updated:** 2025-10-30
> **Target Audience:** Plugin developers (Phase 2+)

---

## Overview

This guide will be the comprehensive resource for creating Kalahari plugins. Currently in draft form during Phase 0, it will be fully populated during Phase 1-2 as the plugin API stabilizes.

**Current Status:** Plugin system foundation complete (Task #00011)
- ✅ .kplugin format defined (ZIP with manifest.json + plugin.py)
- ✅ Plugin lifecycle implemented (discovery → load → activate → deactivate → unload)
- ✅ Extension Points + Event Bus ready
- ⏳ Documentation in progress (Phase 1-2)

---

## Quick Start (Phase 2+)

### 1. Plugin Structure

```
my_plugin.kplugin (ZIP archive)
├── manifest.json           # Plugin metadata (required)
├── plugin.py               # Main plugin code (required)
├── requirements.txt        # Python dependencies (optional)
└── assets/                 # Plugin resources (optional)
    ├── icons/
    ├── images/
    └── docs/
```

### 2. Minimal Example: Hello Plugin

**manifest.json:**
```json
{
  "id": "org.example.hello",
  "name": "Hello Plugin",
  "version": "0.1.0",
  "description": "Simple example plugin",
  "author": "Your Name",
  "license": "MIT",
  "api_version": "0.1.0",
  "entry_point": "plugin:HelloPlugin",
  "extension_points": [],
  "dependencies": [],
  "permissions": []
}
```

**plugin.py:**
```python
class HelloPlugin:
    """Minimal plugin demonstrating lifecycle hooks"""

    def __init__(self):
        self.name = "Hello Plugin"

    def on_init(self):
        """Called when plugin is first loaded"""
        print(f"[{self.name}] Initialized")

    def on_activate(self):
        """Called when plugin becomes active"""
        print(f"[{self.name}] Activated")

    def on_deactivate(self):
        """Called when plugin is being unloaded"""
        print(f"[{self.name}] Deactivated")
```

**Creating .kplugin:**
```bash
zip my_plugin.kplugin manifest.json plugin.py
```

---

## Developer Tools Roadmap

### Phase 0-1 (Current - Q1 2026)
- ✅ **Foundation** - Plugin system working
- ✅ **Documentation** - This guide (basic structure)
- ✅ **Example Plugin** - hello_plugin.kplugin (working reference)

### Phase 2-3 (Q2 2026)
- 📝 **Comprehensive Docs** - Full API reference
- 📝 **Advanced Examples** - All Extension Points demonstrated
- 📝 **Best Practices** - Patterns, anti-patterns, performance tips

### Phase 4-5 (Q3 2026)
- 🔨 **Developer Mode** - GUI tools in Kalahari application
  - Plugin Creator Wizard
  - Plugin Validator
  - Plugin Packager
  - Live reload for development
- 🔨 **CLI Tools** - Automation scripts
- 🔨 **JSON Schema** - VSCode autocomplete for manifest.json

---

## Coming Soon

This guide will cover:

1. **Plugin Architecture**
   - Extension Points (IPlugin, IExporter, IPanelProvider, IAssistant)
   - Event Bus (pub/sub patterns)
   - Lifecycle hooks (init, activate, deactivate)

2. **Manifest Reference**
   - Required fields (id, name, version, api_version, entry_point)
   - Optional fields (description, author, license, permissions)
   - Extension points declaration
   - Dependency management

3. **Python API**
   - kalahari_api module reference
   - Logger integration
   - Event subscription
   - Extension Point implementation

4. **Testing**
   - Unit testing plugins
   - Integration testing
   - Debugging techniques

5. **Distribution**
   - .kplugin packaging
   - Version management
   - Plugin marketplace (Phase 5)

---

## Current Examples

**Working Example:**
- `tests/plugins/hello_plugin.kplugin` - Minimal working plugin
- Shows lifecycle hooks (on_init, on_activate, on_deactivate)
- Demonstrates manifest.json structure

**Test Suite:**
- `tests/core/test_plugin_loading.cpp` - Integration tests
- Covers discovery, loading, unloading, full lifecycle

---

## Questions or Feedback?

This is a living document. As the plugin system evolves, this guide will expand.

**For Phase 0-1 developers:**
- Study hello_plugin.kplugin example
- Review test_plugin_loading.cpp for usage patterns
- Check project_docs/04_plugin_system.md for architecture

**Developer Tools Coming:** Phase 4-5 (Q3 2026)
- Why later? API needs to stabilize first (Phase 1-3)
- Community feedback will shape the tools
- Plugin marketplace preparation requires quality tooling

---

**Next Update:** Phase 1 (when MVP plugins are developed)
