# MVP Task Breakdown

> **Detailed Implementation Tasks** - Week-by-week breakdown for 18-month development

**Status:** ✅ Complete
**Version:** 1.0
**Last Updated:** 2025-10-25

---

## Overview

This document provides week-by-week task breakdown for Kalahari 1.0 development (18 months / 68 weeks).

**Phases:**
- **Phase 0:** Foundation (8 weeks) - Build infrastructure
- **Phase 1:** Core Editor (12 weeks) - Rich text editor + project management  
- **Phase 2:** Plugin System MVP (10 weeks) - 4 working plugins
- **Phase 3:** Feature Plugins (14 weeks) - Advanced features + premium plugins
- **Phase 4:** Advanced Plugins (12 weeks) - Professional features
- **Phase 5:** Polish & Release (12 weeks) - Testing + documentation + launch

---

## Phase 0: Foundation (Weeks 1-8)

### Week 1: Project Setup & CMake

**Goal:** Working C++ project with vcpkg dependencies

**Tasks:**
- [x] Create project structure (src/, include/, tests/, docs/)
- [x] Setup CMakeLists.txt (C++20, vcpkg manifest)
- [x] Configure vcpkg.json (wxWidgets, spdlog, nlohmann_json, libzip, Catch2)
- [x] Build "Hello World" console app
- [x] Setup GitHub repository + CI/CD (GitHub Actions)

**Acceptance Criteria:**
- ✅ CMake builds on Windows/macOS/Linux
- ✅ vcpkg dependencies install automatically
- ✅ GitHub Actions runs on each commit

**Estimated Time:** 5 days

---

### Week 2: wxWidgets Basic Window

**Goal:** Display basic wxWidgets window with menu bar

**Tasks:**
- [x] Create KalahariApp class (wxApp)
- [x] Create MainWindow class (wxFrame)
- [x] Add menu bar (File, Edit, View, Help)
- [x] Add status bar
- [x] Add basic toolbar
- [x] Setup spdlog logging

**Acceptance Criteria:**
- ✅ Window displays with native look & feel
- ✅ Menu items clickable (stub handlers)
- ✅ Logging works (debug + release)

**Estimated Time:** 5 days

---

### Week 3-4: Python Embedding (pybind11)

**Goal:** Embedded Python 3.11 working, simple C++↔Python call

**Tasks:**
- [x] Embed Python 3.11 interpreter (Py_Initialize)
- [x] Setup pybind11 bindings
- [x] Create test C++ class exposed to Python
- [x] Call Python function from C++
- [x] Call C++ function from Python
- [x] Handle Python GIL properly

**Acceptance Criteria:**
- ✅ Python interpreter starts/stops without errors
- ✅ Can import Python modules (test with `import sys`)
- ✅ C++ class accessible from Python
- ✅ Python exceptions caught in C++

**Estimated Time:** 10 days (2 weeks)

---

### Week 5-6: Plugin Manager Core

**Goal:** PluginManager discovers and loads .kplugin files

**Tasks:**
- [x] Create PluginManager class (Singleton)
- [x] Implement plugin discovery (scan plugins/ directory)
- [x] Parse manifest.json (nlohmann_json)
- [x] Load Python plugin module
- [x] Call plugin lifecycle methods (on_init, on_activate)
- [x] Extension Point registry
- [x] Plugin error handling (try-catch, isolation)

**Acceptance Criteria:**
- ✅ Discovers .kplugin files in plugins/
- ✅ Parses manifest.json successfully
- ✅ Loads Python plugin and calls on_init()
- ✅ Plugin errors don't crash app

**Estimated Time:** 10 days (2 weeks)

---

### Week 7: Event Bus

**Goal:** Thread-safe EventBus for core↔plugin communication

**Tasks:**
- [x] Create EventBus class (Singleton)
- [x] Implement subscribe/unsubscribe (type-safe)
- [x] Implement emit (synchronous)
- [x] Implement emitAsync (marshalled to GUI thread)
- [x] Add mutex for thread safety
- [x] Define core event types (DocumentChanged, TextEdited, etc.)

**Acceptance Criteria:**
- ✅ Subscribe to event type
- ✅ Emit event from C++, received in Python
- ✅ Async emit marshals to GUI thread
- ✅ Thread-safe (tested with multiple threads)

**Estimated Time:** 5 days

---

### Week 8: Document Model + JSON Serialization

**Goal:** Document/Chapter/Book classes with save/load

**Tasks:**
- [x] Create Document, Chapter, Book classes (Composite pattern)
- [x] Implement toJson() / fromJson() (nlohmann_json)
- [x] Create .klh file handler (libzip)
- [x] Save document to .klh file (ZIP + JSON + RTF stubs)
- [x] Load document from .klh file
- [x] Write unit tests (Catch2)

**Acceptance Criteria:**
- ✅ Create document, add chapters
- ✅ Save to .klh file (valid ZIP)
- ✅ Load from .klh file (data intact)
- ✅ Unit tests pass (>= 80% coverage)

**Estimated Time:** 5 days

---

## Phase 1: Core Editor (Weeks 9-20)

### Week 9-10: wxRichTextCtrl Integration

**Goal:** Rich text editor with basic formatting

**Tasks:**
- [x] Integrate wxRichTextCtrl into main window
- [x] Implement bold, italic, underline
- [x] Implement heading styles (H1, H2, H3)
- [x] Implement undo/redo (wxRichTextCtrl built-in)
- [x] Connect to Document model (MVP pattern)
- [x] Create DocumentPresenter class

**Acceptance Criteria:**
- ✅ Type text, apply formatting (bold, italic)
- ✅ Undo/redo works (Ctrl+Z, Ctrl+Y)
- ✅ Text persists in Document model

**Estimated Time:** 10 days (2 weeks)

---

### Week 11-12: Project Navigator Panel

**Goal:** wxTreeCtrl showing book structure (Parts → Chapters)

**Tasks:**
- [x] Create ProjectNavigatorPanel (wxPanel with wxTreeCtrl)
- [x] Populate tree from Document model
- [x] Add chapter context menu (Add, Delete, Rename, Move)
- [x] Implement chapter selection (loads in editor)
- [x] Implement drag & drop (reorder chapters)
- [x] Wire up to EventBus (ChapterAdded, ChapterDeleted events)

**Acceptance Criteria:**
- ✅ Tree displays book structure
- ✅ Right-click chapter → Add/Delete/Rename
- ✅ Double-click chapter → loads in editor
- ✅ Drag chapter → reorders in tree

**Estimated Time:** 10 days (2 weeks)

---

### Week 13-14: Save/Load (.klh files)

**Goal:** Full save/load cycle with .klh files

**Tasks:**
- [x] Implement File → Save (Ctrl+S)
- [x] Implement File → Save As
- [x] Implement File → Open
- [x] Save all chapters to .klh (chapters/*.rtf)
- [x] Load all chapters from .klh
- [x] Implement dirty flag (unsaved changes indicator)
- [x] Prompt user on close if unsaved

**Acceptance Criteria:**
- ✅ Save document to .klh
- ✅ Close app, reopen → data intact
- ✅ Unsaved changes indicator (*)
- ✅ Prompt before losing unsaved changes

**Estimated Time:** 10 days (2 weeks)

---

### Week 15-16: Auto-Save & Backup System

**Goal:** Auto-save every 5 minutes, keep snapshots

**Tasks:**
- [x] Implement auto-save timer (wxTimer)
- [x] Save to .klh.tmp, then rename (atomic)
- [x] Implement backup snapshots (.klh.autosave.TIMESTAMP)
- [x] Keep last N autosaves (configurable, default 5)
- [x] Implement recovery on crash (load last autosave)
- [x] Add settings UI (auto-save interval)

**Acceptance Criteria:**
- ✅ Auto-saves every 5 minutes (configurable)
- ✅ Can recover from crash (load autosave)
- ✅ Old autosaves deleted (keep last 5)

**Estimated Time:** 10 days (2 weeks)

---

### Week 17-18: wxAUI Docking System

**Goal:** Dockable panels (Navigator, Editor, Preview, etc.)

**Tasks:**
- [x] Setup wxAuiManager
- [x] Create dockable panels (Navigator, Editor, Preview, Assistant, Stats, Output)
- [x] Implement default layout (3-column from 08_gui_design.md)
- [x] Implement panel show/hide (View menu)
- [x] Implement perspective save/load
- [x] Add 3 focus modes (Normal, Focused, Distraction-Free)

**Acceptance Criteria:**
- ✅ Panels are dockable (drag & drop)
- ✅ Close panel → reopen from View menu
- ✅ Save layout → restore on restart
- ✅ Focus mode toggles (F11)

**Estimated Time:** 10 days (2 weeks)

---

### Week 19-20: Focus Modes & Perspectives

**Goal:** 4 perspectives + 3 focus modes working

**Tasks:**
- [x] Implement 4 perspectives (Writer, Editor, Researcher, Planner)
- [x] Implement perspective switcher (toolbar dropdown)
- [x] Implement 3 focus modes (Normal, Focused, Distraction-Free)
- [x] Implement F11 (toggle Distraction-Free)
- [x] Implement focus mode indicator (status bar)
- [x] Polish UI (icons, tooltips)

**Acceptance Criteria:**
- ✅ Switch perspectives → layout changes
- ✅ F11 → full-screen editor only
- ✅ Perspectives persist across restarts

**Estimated Time:** 10 days (2 weeks)

---

## Phase 2-5: High-Level Overview

### Phase 2: Plugin System MVP (Weeks 21-30)

**Goal:** 4 working plugins proving the system

**Deliverables:**
- Plugin 1: DOCX Exporter (python-docx)
- Plugin 2: Markdown Tools (import/export)
- Plugin 3: Basic Statistics (word count, charts)
- Plugin 4: Assistant Lion (1 animal, basic messages)
- Plugin Manager UI (enable/disable/configure)

**Estimated Time:** 10 weeks

---

### Phase 3: Feature Plugins (Weeks 31-44)

**Goal:** Advanced features + premium plugins

**Deliverables:**
- Free plugins: PDF, TXT/RTF, Spell Checker, 4 themes
- Premium Plugin: AI Assistant Pro (8 animals, AI integration)
- Premium Plugin: Advanced Analytics (timeline, graphs, pacing)
- License verification system

**Estimated Time:** 14 weeks

---

### Phase 4: Advanced Plugins (Weeks 45-56)

**Goal:** Professional writer's toolkit

**Deliverables:**
- Premium Plugin: Export Suite (EPUB, advanced PDF, LaTeX)
- Premium Plugin: Research Pro (OCR, citations)
- Character/Location banks
- Notes system + Writer's Calendar

**Estimated Time:** 12 weeks

---

### Phase 5: Polish & Release (Weeks 57-68)

**Goal:** Production-ready 1.0 release

**Deliverables:**
- Testing marathon (unit + integration + E2E)
- User manual (100-120 pages, EN + PL)
- Plugin API documentation
- Installers (Windows NSIS, macOS DMG, Linux AppImage)
- Website (kalahari.app)
- Launch (GitHub release, social media, blog posts)

**Estimated Time:** 12 weeks

---

## Critical Path

**Must complete before Phase 0 ends:**
1. CMake + vcpkg working
2. Python embedding stable
3. Plugin Manager loading plugins
4. EventBus thread-safe
5. Document model save/load

**Dependencies:**
- Phase 1 depends on: Phase 0 complete
- Phase 2 depends on: Phase 1 Week 20 (docking system)
- Phase 3 depends on: Phase 2 Plugin Manager UI
- Phase 4 depends on: Phase 3 AI Assistant
- Phase 5 depends on: Phase 4 all features complete

---

## Risk Assessment

**High Risk:**
- Python embedding complexity (GIL, threading)
- Plugin crash isolation
- Cross-platform wxWidgets quirks

**Medium Risk:**
- Performance (large documents, 100+ chapters)
- wxRichTextCtrl limitations (may need custom editor)

**Low Risk:**
- JSON serialization
- File I/O

**Mitigation:**
- Allocate extra time for Phase 0 (foundation critical)
- Early prototypes for risky features
- Weekly code reviews

---

## Summary

**Phase 0:** 8 weeks - Foundation (CMake, Python, Plugin Manager, EventBus, Document model)
**Phase 1:** 12 weeks - Core Editor (Rich text, navigator, save/load, auto-save, docking, perspectives)
**Phase 2:** 10 weeks - Plugin MVP (4 plugins proving system)
**Phase 3:** 14 weeks - Feature Plugins (premium features + AI)
**Phase 4:** 12 weeks - Advanced Plugins (professional toolkit)
**Phase 5:** 12 weeks - Polish & Release (testing, docs, installers, launch)

**Total:** 68 weeks (~18 months)

---

**Document Version:** 1.0
**Last Updated:** 2025-10-25
**Next Review:** Start of Phase 0
