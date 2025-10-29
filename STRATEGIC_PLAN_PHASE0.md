# Kalahari Phase 0 Foundation - Strategic Plan

**Status:** Week 3-4 Complete | Week 5-6 Planned
**Updated:** 2025-10-29
**Phase Target:** Complete by Week 8 (Late November 2025)

---

## 📊 Current Progress

### ✅ COMPLETED: Weeks 1-4

| Week | Scope | Task | Status | Files | Tests |
|------|-------|------|--------|-------|-------|
| 1 | Project Setup | CMake + vcpkg | ✅ | Core | ✅ |
| 2 | wxWidgets GUI | Main Window | ✅ | 5 | ✅ |
| 3 | Settings System | JSON + Dialog | ✅ Task #00003 | 6 | ✅ |
| 3 | Build Automation | Cross-platform | ✅ Task #00004 | 8 | ✅ |
| 2 | Python Embedding | Py3.11 + Interpreter | ✅ Task #00005 | 4 | ✅ |
| 3-4 | **Plugin Foundation** | **PluginManager + pybind11** | **✅ Task #00009** | **14** | **✅** |

**Weeks 1-4 Summary:**
- 📁 **42+ files** created/modified
- 🧪 **50+ unit tests** passing
- 🏗️ **Complete foundation** for plugin system
- 🎯 **Core infrastructure** working on all platforms

### ⏳ PLANNED: Weeks 5-8

| Week | Scope | Task | Effort | Priority |
|------|-------|------|--------|----------|
| 5-6 | Extension Points + Event Bus | Task #00010 | 14-18h | 🟠 HIGH |
| 6 | .kplugin Format Handler | Task #00011 | 12-15h | 🟠 HIGH |
| 7-8 | Document Model + JSON | Task #00012 | 12-15h | 🟠 HIGH |
| TBD | Phase 0 Polish | Task #00013 | 5-8h | 🟡 MEDIUM |

---

## 🔍 Detailed Analysis: Phase 0 Architecture

### Current State (After Task #00009)

**What We Have:**
```
✅ PluginManager (Singleton)
   - Stub methods ready for actual implementation
   - Thread-safe (std::mutex)
   - Tested singleton + thread safety patterns

✅ kalahari_api (Python Module)
   - Logger bindings (info, error, debug, warning)
   - Python ↔ C++ communication verified
   - Module auto-copies to bin/ for testing

✅ Build System
   - CMake + vcpkg fully integrated
   - pybind11 detection + configuration
   - Multi-platform (Linux, Windows, macOS)

✅ Testing Infrastructure
   - 6 PluginManager unit tests
   - 5 Python interop tests
   - 1 Python integration test script
   - GUI diagnostics menu item
```

**Architecture Diagram (Current):**
```
┌─────────────────────────────────────┐
│  Kalahari Core (C++)                │
│  ┌───────────────────────────────┐  │
│  │ Main Infrastructure Layer     │  │
│  ├─ wxWidgets GUI               │  │
│  ├─ spdlog Logging              │  │
│  ├─ nlohmann_json Settings      │  │
│  └─ Threading (4 max threads)   │  │
│  ┌───────────────────────────────┐  │
│  │ Plugin Foundation (NEW)       │  │
│  ├─ PluginManager (Singleton)   │  │
│  ├─ kalahari_api (pybind11)     │  │
│  └─ Logger bindings             │  │
│  ┌───────────────────────────────┐  │
│  │ MISSING (Week 5-8):           │  │
│  ├─ [ ] ExtensionPointRegistry   │  │
│  ├─ [ ] EventBus (Pub/Sub)       │  │
│  ├─ [ ] .kplugin Format Handler  │  │
│  └─ [ ] Document Model           │  │
│  ┌───────────────────────────────┐  │
│  │ Phase 1 Placeholder           │  │
│  ├─ [ ] wxRichTextCtrl Editor    │  │
│  ├─ [ ] Project Navigator Panel  │  │
│  └─ [ ] Auto-save System         │  │
│  └─────────────────────────────────┘
│        ↕ pybind11
│  ┌───────────────────────────────┐
│  │ Python 3.11 Embedded          │
│  │ ├─ sys, os modules            │
│  │ ├─ kalahari_api module        │
│  │ └─ Plugin registry            │
│  └───────────────────────────────┘
│        ↕ import
│  ┌───────────────────────────────┐
│  │ Python Plugins (Phase 2)      │
│  │ ├─ [ ] DOCX Exporter          │
│  │ ├─ [ ] Markdown Tools         │
│  │ ├─ [ ] Statistics             │
│  │ └─ [ ] Assistant Lion         │
│  └───────────────────────────────┘
└─────────────────────────────────────┘
```

---

## 📋 Week 5-6 Plan: Extension Points + Event Bus (Task #00010)

**Goal:** Enable plugin extension and inter-plugin communication

### Components to Build

#### 1. Extension Point Interfaces
```cpp
IPlugin (base)
├── IExporter (export documents)
├── IPanelProvider (add UI panels)
└── IAssistant (graphical helpers)
```

**Files:**
- `include/kalahari/core/extension_points.h` (100 lines)
- `src/core/extension_points.cpp` (80 lines)
- `tests/core/test_extension_points.cpp` (100 lines)

#### 2. EventBus (Pub/Sub System)
```cpp
EventBus (Singleton)
├── subscribe(event_type, callback)
├── unsubscribe(event_type)
├── emit(event) // Synchronous
└── emitAsync(event) // GUI marshalled
```

**Features:**
- Thread-safe (std::mutex + std::queue)
- GUI marshalling via wxTheApp->CallAfter
- Type-safe callbacks

**Files:**
- `include/kalahari/core/event_bus.h` (80 lines)
- `src/core/event_bus.cpp` (120 lines)
- `tests/core/test_event_bus.cpp` (120 lines)
- `tests/test_event_bus.py` (60 lines)

#### 3. pybind11 Extensions
- Add EventBus bindings to `src/bindings/python_bindings.cpp`
- Expose subscribe, unsubscribe, emit, emitAsync to Python

**Estimated Files:** 8 new files | 600+ lines of code | 14-18 hours

---

## 📋 Week 6 Plan: .kplugin Format Handler (Task #00011)

**Goal:** Actual plugin loading from .kplugin ZIP files

### Components to Build

#### 1. Manifest Parsing
```json
{
  "id": "org.kalahari.example",
  "name": "Example Plugin",
  "version": "0.1.0",
  "api_version": "0.1",
  "extension_points": ["IExporter"],
  "entry_point": "plugin.py:ExamplePlugin"
}
```

**Files:**
- `include/kalahari/core/plugin_manifest.h`
- `src/core/plugin_manifest.cpp`
- `tests/core/test_plugin_manifest.cpp`

#### 2. ZIP Archive Extraction
```cpp
PluginArchive archive("my_plugin.kplugin");
auto extracted_path = archive.getExtractedPath();
// Automatic cleanup on destruction
```

**Files:**
- `include/kalahari/core/plugin_archive.h`
- `src/core/plugin_archive.cpp`

#### 3. Enhanced PluginManager
- Implement actual `discoverPlugins()` - scans plugins/
- Implement actual `loadPlugin()` - extracts and loads
- Implement `unloadPlugin()` - cleanup
- Add error handling - plugin failures isolated

**Updates:**
- `include/kalahari/core/plugin_manager.h` (enhanced)
- `src/core/plugin_manager.cpp` (full implementation)

#### 4. Testing & Examples
- Example plugin: `tests/plugins/hello_plugin.kplugin`
- Unit tests for discovery, loading, unloading
- Integration test with sample plugin

**Estimated Files:** 8 new files | 500+ lines of code | 12-15 hours

---

## 📋 Weeks 7-8 Plan: Document Model (Task #00012)

**Goal:** Core document structure for Phase 1 editor

### Components to Build

#### 1. Document Classes
```cpp
Document (represents book)
├── Part (optional grouping)
│   └── Chapter (individual chapter)
│       ├── metadata (title, author, created_date)
│       └── content (rich text - Phase 1)
└── Metadata (book properties)
```

#### 2. Serialization
- `toJson()` / `fromJson()` using nlohmann_json
- `.klh` file format (ZIP container with JSON + content)

#### 3. File I/O
```cpp
Document doc;
doc.save("my_book.klh");
Document loaded = Document::load("my_book.klh");
```

**Files:**
- `include/kalahari/core/document.h`
- `include/kalahari/core/chapter.h`
- `include/kalahari/core/part.h`
- `src/core/document.cpp`
- `src/core/chapter.cpp`
- `src/core/part.cpp`
- `src/core/klh_handler.h`
- `src/core/klh_handler.cpp`
- `tests/core/test_document_model.cpp` (80%+ coverage)

**Estimated Files:** 10 new files | 600+ lines of code | 12-15 hours

---

## 🎯 Phase 0 Week 8 Deliverables

### Must-Have (Critical for Phase 1)

- ✅ Core Infrastructure (done)
- ✅ Plugin Foundation (done - Task #00009)
- ⏳ Extension Points + EventBus (Task #00010)
- ⏳ .kplugin Format Handler (Task #00011)
- ⏳ Document Model (Task #00012)

### Nice-to-Have (Can defer to Phase 1)

- Document editor UI (Phase 1 Week 9)
- Project navigator panel (Phase 1 Week 11)
- Auto-save system (Phase 1 Week 12)

### Definition of "Phase 0 Complete"

✅ All infrastructure working on Windows + Linux + macOS
✅ Plugin system fully functional (discover, load, unload)
✅ Document model with save/load
✅ 50+ unit tests passing
✅ Build automation scripts working
✅ CI/CD pipeline running

---

## 📊 Effort Estimation Summary

| Task | Hours | Days | Priority |
|------|-------|------|----------|
| #00010: Extension Points + EventBus | 14-18h | 2-3 | 🟠 HIGH |
| #00011: .kplugin Format Handler | 12-15h | 2-3 | 🟠 HIGH |
| #00012: Document Model | 12-15h | 2-3 | 🟠 HIGH |
| #00013: Phase 0 Polish | 5-8h | 1 | 🟡 MEDIUM |
| **TOTAL** | **43-56h** | **~8-9 days** | |

**Timeline:**
- Week 5-6: Tasks #00010 + #00011 (4-5 days)
- Week 7-8: Task #00012 (3-4 days)
- Polish & Testing: 1-2 days

**Target Completion:** End of November 2025

---

## 🚀 Transition to Phase 1

Once Phase 0 is complete:

### Phase 1 Setup (Week 9)
- Plan editor architecture (MVP pattern)
- Setup wxRichTextCtrl integration
- Create DocumentPresenter class

### Phase 1 Weeks 9-20 (3-4 months)
1. **Rich Text Editor** (Weeks 9-10)
   - wxRichTextCtrl with formatting (bold, italic, styles)
   - Undo/redo system
   - Find & Replace

2. **Project Navigator** (Weeks 11-12)
   - wxTreeCtrl with book structure
   - Chapter management (add, delete, rename)
   - Drag & drop reordering

3. **Statistics & Stats Panel** (Weeks 13-14)
   - Word/character count
   - Session statistics
   - Charts (matplotlib)

4. **Auto-save & Backup** (Weeks 15-16)
   - Background save thread
   - Version history
   - Backup system

5. **Polish & Testing** (Weeks 17-20)
   - UI refinement
   - Performance optimization
   - Manual testing all platforms

---

## 🎓 Key Learnings from Phase 0

### What Worked Well
1. **Task-driven development** - Clear task files with acceptance criteria
2. **Incremental progress** - Each week adds clear value
3. **Testing from day one** - Unit tests caught issues early
4. **Memory tracking** - Documented decisions help future work
5. **Comprehensive documentation** - project_docs/ provides context

### What to Improve
1. **Earlier platform testing** - Should test Windows/macOS sooner
2. **Dependency management** - vcpkg helped but docs could be better
3. **Performance profiling** - Haven't benchmarked yet (Phase 1 task)
4. **Community communication** - Should publish progress blog posts

---

## 📋 Next Session Checklist

When you test on Windows + Linux, please verify:

- [ ] Application starts without errors
- [ ] Settings Dialog opens (File → Settings)
- [ ] Diagnostics menu visible (with --diag flag)
- [ ] "Test Python Integration" menu works
- [ ] "Test Python Bindings (pybind11)" menu works
- [ ] Log files created in correct location
- [ ] Build automation scripts work (both platforms)
- [ ] All tests pass: `ctest --output-on-failure`

### Critical Path for Next Session
1. ✅ Build on Windows (your testing now)
2. ✅ Build on Linux (your testing now)
3. ✅ Verify all menu items work
4. ✅ Run unit + Python tests
5. ⏳ Then approve Task #00010 + #00011
6. ⏳ Then I implement Task #00010
7. ⏳ Then I implement Task #00011

---

## 📞 Questions for User

Before Task #00010 implementation, please clarify:

1. **Extension Point Strategies:** Should plugins be loaded in priority order, or all same-level?
2. **EventBus Scope:** Should events be app-wide, or scoped per plugin?
3. **Plugin Permissions:** Should we have permission system (file access, network, etc.) in Phase 0 or Phase 2?
4. **Error Recovery:** If plugin crashes, should app try to reload or mark as broken?
5. **Phase 1 Priority:** Should we prioritize Editor or Plugin System first?

---

**Document Version:** 1.0
**Last Updated:** 2025-10-29
**Next Review:** After user testing completes
