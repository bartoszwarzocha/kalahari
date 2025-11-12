# Kalahari Development Roadmap

> **Writer's IDE** - 18-Month Journey from Concept to Public Release

**Current Status:** 🚀 Phase 1 Week 13 - Command Registry Architecture
**Next Task:** #00031 (Dynamic Menu Builder)
**Version:** 0.1.0-alpha (Phase 0 Complete, Phase 1 in progress)
**Last Updated:** 2025-11-12 (Task #00030 complete - Format Menu Commands registered)

---

## Overview

This roadmap outlines the development journey of Kalahari from initial concept to public 1.0 release. The project follows a **6-phase development strategy** spanning 14-20 months (realistically 18 months), with an **Open Core + Premium Plugins** business model.

**Key Milestones:**
- ✅ **Documentation Complete** (2025-10-25) - All 11 core documents finalized (100%)
- ✅ **Architectural Decisions** (2025-10-25) - All 7 core decisions finalized
- ✅ **Phase 0: Foundation** (2025-10-26 to 2025-11-03) - **COMPLETE** 🎉
  - Core infrastructure + Plugin system + Document model + bwx_sdk integration + CI/CD optimization
- 🚀 **Phase 1: Core Editor** (Weeks 9-20 | 3-4 months) - **IN PROGRESS** (Started 2025-11-04)
  - Current: Task #00019 COMPLETE ✅ (Custom wxWidgets Text Editor Control - MVP)
  - Progress: All 15 days complete - Document Model, Renderer, Main Control, Integration, Testing, Polish - 100% ✅
- ⏳ **Phase 2: Plugin System MVP** (Weeks 21-30 | 2-3 months)
- ⏳ **Phase 3: Feature Plugins** (Weeks 31-44 | 3-4 months)
- ⏳ **Phase 4: Advanced Plugins** (Weeks 45-56 | 2-3 months)
- ⏳ **Phase 5: Polish & Release** (Weeks 57-68 | 2-3 months)
- 🎯 **Public Release: Kalahari 1.0** (Target: Q2-Q3 2026)

---

## Phase 0: Foundation (Weeks 1-8 | 2-3 months) ✅ COMPLETE

**Goal:** Build technical infrastructure and plugin architecture foundation

**Status:** ✅ **100% COMPLETE** (2025-10-26 to 2025-11-03)
**Target Version:** 0.1.0-alpha
**Timeline:** 2-3 months from project start
**Started:** 2025-10-26
**Completed:** 2025-11-03
**Duration:** 8 days (accelerated timeline!)

**Milestones Achieved:**
- **Week 1-2 (Oct 26):** GUI foundation, Threading, Logging
- **Week 3 (Oct 27):** Settings System, Build Scripts
- **Week 3-4 (Oct 29):** Plugin Manager + pybind11 + Compilation Fixes
- **Week 5-6 (Oct 29):** Extension Points + Event Bus + pybind11 Bindings
- **Week 6 (Oct 30):** .kplugin Format Handler + Document Model + JSON Serialization
- **Week 7 (Nov 1):** wxAUI Docking System (Task #00013) + 6 core panels
- **Week 8 (Nov 2-3):** bwx_sdk integration (Tasks #00017, #00018) + CI/CD optimization (92% improvement)

### Architectural Decisions ✅ FINALIZED (2025-10-25)
- ✅ **GUI Pattern:** MVP (Model-View-Presenter)
  - Clear separation: Model (data) ↔ Presenter (logic) ↔ View (GUI)
  - Testable business logic (Presenters can be unit tested)
- ✅ **Error Handling:** Hybrid approach
  - Exceptions for programmer errors (invalid_argument, logic_error)
  - Error codes (std::optional) for expected failures
  - wxLog* for user-facing messages
- ✅ **Dependency Management:** Hybrid approach
  - Singletons for infrastructure (EventBus, CommandRegistry, PluginManager)
  - Dependency Injection for business logic (Models, Presenters)
- ✅ **Threading:** Dynamic thread pool
  - 2-4 worker threads (CPU-aware, adjustable ±2)
  - Python GIL handling (py::gil_scoped_acquire/release)
  - GUI marshalling (wxTheApp->CallAfter)
- ✅ **Memory Management:** Lazy loading (from Phase 1)
  - Metadata eager (titles, structure, stats)
  - Content on-demand (chapter text loaded when opened)
  - Smart pointers everywhere
- ✅ **Undo/Redo:** Command pattern
  - 100 commands default (configurable)
  - Mergeable consecutive edits
- ✅ **Document Model:** Composite pattern
  - Book → Parts → Chapters (flexible nested structure)

### Core Infrastructure
- [x] **CMake build system** (all platforms: Windows, macOS, Linux) ✅ Week 1
- [x] **vcpkg integration** (manifest mode) ✅ Week 1
- [x] **wxWidgets 3.3.0+ basic application window** ✅ Week 2
- [x] **Main window with menu bar, toolbar, status bar** ✅ Week 2
- [x] **Settings system (JSON persistence with nlohmann_json)** ✅ Week 3 (Task #00003)
- [x] **Logging system (spdlog - structured, multi-level)** ✅ Week 2
- [x] **Build automation scripts** (cross-platform) ✅ Week 3 (Task #00004)
- [x] **CI/CD pipelines** (GitHub Actions: Linux, macOS, Windows) ✅ 2025-10-31 (Infrastructure fix)

### Plugin Architecture
- [x] **Python 3.11 embedding** (bundled with application) ✅ Week 2 (Task #00005)
- [x] **pybind11 integration** (C++/Python interop) ✅ Week 3-4 (Task #00009)
  - kalahari_api module with Logger bindings (info, error, debug, warning)
  - Python ↔ C++ communication working
  - Unit tests + Python integration tests passing
- [x] **Plugin Manager** (discovery, loading, unloading, lifecycle) ✅ Week 3-4 (Task #00009)
  - Singleton pattern with thread-safe std::mutex
  - Stub methods ready for Week 5-6 actual implementation
  - Unit tests verify singleton behavior + thread safety
- [x] **Extension Points system** (C++ interfaces for plugins) ✅ Week 5-6 (Task #00010)
  - IExporter, IPanelProvider, IAssistant, IPlugin base interfaces
  - ExtensionPointRegistry singleton with type-safe retrieval
  - Thread-safe registration with std::mutex
  - 12 test cases covering registration, type-casting, thread safety
- [x] **Event Bus** (async, thread-safe, GUI-aware marshalling) ✅ Week 5-6 (Task #00010)
  - Thread-safe event queue (std::queue + std::mutex)
  - Subscriber pattern with unsubscribe support
  - GUI marshalling via wxTheApp->CallAfter for async emission
  - pybind11 bindings for Python plugins
  - 11 C++ test cases + 7 Python integration tests
  - 8 standard event types defined (document, editor, plugin, goal events)
- [x] **.kplugin format handler** (ZIP reading/writing with libzip) ✅ Week 6 (Task #00011)
  - Package structure: manifest.json + plugin.py + assets/
  - ZIP extraction and validation (PluginArchive RAII wrapper)
  - Plugin path detection and loading (full lifecycle)
  - Platform-specific temp directories (~/.local/share/Kalahari/plugins/temp/)
  - Actual plugin loading: extract → sys.path → import → instantiate → on_init() → on_activate()
- [x] **Plugin API versioning** (semantic versioning checks) ✅ Week 6 (Task #00011)
  - Version compatibility checking (manifest validation)
  - Graceful degradation for incompatible plugins

### Document Model ✅ Week 8 (Task #00012)
- [x] **Core C++ classes** (BookElement, Part, Book, Document) - 3-section professional structure
  - BookElement with flexible string-based type system (not enum)
  - Part as chapter container with aggregation
  - Book with frontMatter, body (Parts), backMatter (industry standard)
  - Document wrapper with project metadata (title, author, language, UUID)
- [x] **JSON serialization** (nlohmann_json) - toJson/fromJson pattern
  - ISO 8601 timestamps (created, modified) with platform-specific conversion
  - Metadata map for extensibility (plugins can add custom fields)
  - Optional field handling (genre, custom metadata)
- [x] **.klh file format** (ZIP container with JSON metadata) - DocumentArchive implementation
  - Phase 0 MVP: manifest.json only (RTF files in Phase 2)
  - libzip integration (ZIP_CREATE | ZIP_TRUNCATE, ZIP_RDONLY)
  - Static save/load methods with detailed error logging
- [x] **Basic CRUD operations** - Via Document API
  - Create: Document(title, author, language) with auto UUID generation
  - Read: Document::load(path) from .klh ZIP archive
  - Update: Document setters with touch() auto-timestamp
  - Delete: std::filesystem operations (external to Document class)
- [x] **In-memory document management** - Complete object model
  - Smart pointers (std::shared_ptr) for RAII memory management
  - Lazy loading ready (RTF paths stored, content on-demand in Phase 1)
  - Word count aggregation (Part → Book → Document)

### CI/CD Setup
- [x] **GitHub Actions workflow** (platform-specific: ci-linux.yml, ci-windows.yml, ci-macos.yml) ✅ Week 1
- [x] **Automated builds** (Windows + macOS + Linux, Debug + Release) ✅ Week 1
- [x] **Basic smoke tests** (Catch2 unit tests via ctest) ✅ Week 1
- [x] **vcpkg binary caching** (platform-specific cache keys) ✅ Week 1

### Deliverables
✅ Technical foundation working on all platforms
✅ Plugin system functional (can load/unload sample plugin)
✅ Basic document model with .klh file save/load
✅ CI/CD pipeline producing builds

---

## Phase 1: Core Editor (Weeks 9-20 | 12 weeks) 🚀 IN PROGRESS

**Goal:** Functional rich text editor with project management

**Status:** 🚀 **IN PROGRESS** (Started 2025-11-04)
**Target Version:** 0.2.0-alpha
**Timeline:** 12 weeks (3 months)
**Strategic Decision:** Custom wxWidgets text editor control (not wxRichTextCtrl, not web-based)
**Current Task:** #00019 Day 11/15 - Document Model + Renderer complete, Main Control next

### Phase 1 Task List - UPDATED (2025-11-04)

**Week 9: Foundation (UI Infrastructure)** ✅
- [x] **Task #00013:** wxAUI Docking System + Panel Management **✅ COMPLETE**
  - 6 core panels: Navigator, Editor, Properties, Statistics, Search, Assistant
  - PerspectiveManager (4 default perspectives)
  - IconRegistry + ArtProvider (Material Design SVG icons)
  - AboutDialog + ManagePerspectivesDialog
  - **Status:** ✅ COMPLETE (2025-11-01) | **File:** [tasks/00013_wxaui_docking_system.md](tasks/00013_wxaui_docking_system.md)
  - **Note:** Optional UX polish (drag & drop perspective customization) deferred to Phase 1 end

**Week 10-12: Core Editor (Custom Control)** 🚀 IN PROGRESS
- [x] **Task #00019:** Custom wxWidgets Text Editor Control **✅ COMPLETE (100%)**
  - ✅ **Days 1-8:** Document Model + Renderer (53% complete)
    - bwxTextDocument (Gap Buffer storage, undo/redo, formatting) - 1,450 LOC
    - FullViewRenderer (layout, hit testing, viewport culling) - 850 LOC
    - Test suite (75+ test cases, 2,239 assertions) - 950 LOC
    - Build status: ✅ Linux VB | ✅ Windows | ⚠️ Linux WSL (output issue only)
  - ✅ **Days 9-10:** bwxTextEditor Main Control (~1,000 LOC) (67% complete)
    - Two-phase construction (default + full constructor)
    - Event handling: Keyboard (OnChar, OnKeyDown), Mouse (OnLeftDown, OnMotion, OnMouseWheel), Focus (OnSetFocus, OnKillFocus)
    - Editing operations: Copy/Cut/Paste/SelectAll/Undo/Redo with keyboard shortcuts (Ctrl+C/X/V/A/Z/Y)
    - Formatting shortcuts: Bold (Ctrl+B), Italic (Ctrl+I), Underline (Ctrl+U)
    - Navigation: Arrow keys, Home/End (with Shift for selection)
    - Caret management: Blinking animation (500ms timer), scroll-to-cursor
    - Scrolling: Mouse wheel, auto-scroll to keep cursor visible
    - View mode architecture: VIEW_FULL (MVP), VIEW_PAGE/TYPEWRITER/PUBLISHER (future)
    - MVC architecture: Model (bwxTextDocument) + View (ITextRenderer) + Controller (bwxTextEditor)
    - Observer Pattern: IDocumentObserver for document notifications
    - Buffered painting: wxBufferedDC for flicker-free rendering
    - Test suite: 15 test cases for control creation, view modes, editing ops, document integration
    - Files: bwx_text_editor.h (250 LOC), bwx_text_editor.cpp (750 LOC), test_bwx_text_editor.cpp (180 LOC)
    - Build status: ✅ bwx_gui library compiles successfully
  - ✅ **Days 11-12:** Integration & Observer Pattern (+131 LOC) **COMPLETE (2025-11-06)**
    - EditorPanel implements IDocumentObserver (4 callbacks: OnTextChanged, OnCursorMoved, OnSelectionChanged, OnFormatChanged)
    - Observer lifecycle management: AddObserver() in setupLayout/loadChapter, RemoveObserver() in destructor
    - **TRUE DEBOUNCING:** wxTIMER_ONE_SHOT restarts on each change, fires 500ms AFTER typing stops
    - UpdateWordCount() called ONLY in debounced timer (O(n) only after idle)
    - Bug fixes: m_isModified now set correctly, hasUnsavedChanges() works
    - MainWindow integration: Format menu (Bold/Italic/Underline/Font/Clear), Edit menu (Cut/Copy/Paste/SelectAll/Undo/Redo), View Mode menu
    - Edge cases: loadChapter re-registration, clearContent handling, destructor safety
    - Files: editor_panel.h (+35 LOC), editor_panel.cpp (+96 LOC)
    - Build: ✅ All platforms (Linux 4m, macOS 2m37s, Windows 12m) - CI/CD passing
    - Acceptance criteria: ✅ #33-35 (word count real-time, StatusBar, metadata)
  - ✅ **Days 13-15:** Settings Infrastructure + Testing Preparation (+583 LOC) **COMPLETE (2025-11-06)**
    - **EditorSettingsPanel (383 LOC):** Comprehensive configuration UI with 4 sections
      - Cursor & Caret: Blink enable/rate/width controls
      - Margins & Padding: Left/Right/Top/Bottom spinners (0-100px)
      - Rendering: Line spacing (1.0-2.0x), selection color/opacity picker, antialiasing toggle
      - Behavior: Auto-focus, word wrap, undo limit (10-1000 commands)
    - **Live Updates:** All 14 settings apply without restart via EditorPanel::applySettings()
    - **Persistence:** JSON storage (~/.kalahari/settings.json) via SettingsManager
    - **bwx_sdk API Extensions:** FullViewRenderer selection API (SetSelectionColor/Opacity, GetSelectionColor/Opacity)
    - **Integration:** Settings Dialog tree node (top-level, default selection), MainWindow save/apply logic
    - **Testing Benefit:** Runtime configuration enables "in-flight" parameter testing without recompilation
    - **Build Script Fix:** Distro-specific ninja package detection (ninja-build for Ubuntu/Debian, ninja for Arch)
    - Files: editor_settings_panel.h/cpp (383 LOC), settings_dialog.h/cpp (+72 LOC), editor_panel.h/cpp (+65 LOC), main_window.cpp (+48 LOC), bwx_text_renderer.h/cpp (+25 LOC), bwx_text_editor.h (+6 LOC), build_linux.sh (+10 LOC)
    - Build: ✅ CI/CD all platforms passing (Linux, macOS, Windows)
    - **Manual Testing:** Deferred to Phase 1 Week 13 (can test during Tasks #00020-#00022 with runtime config)
    - Acceptance criteria: ✅ Settings infrastructure complete, enables comprehensive testing "in-flight"
  - **Status:** ✅ COMPLETE (2025-11-04 to 2025-11-06, 3 days) | **File:** [tasks/00019_custom_text_editor_control.md](tasks/00019_custom_text_editor_control.md)
  - **Note:** Custom control FROM SCRATCH (not wxRichTextCtrl-based) with 4 view modes planned (MVP: Full View only)
- [x] **Task #00015:** wxRichTextCtrl Integration **❌ REJECTED**
  - **Reason:** Insufficient control over features, custom control chosen instead
  - **File:** [tasks/00015_wxrichtextctrl_integration_REJECTED.md](tasks/00015_wxrichtextctrl_integration_REJECTED.md)
- [x] **Task #00016:** TipTap + wxWebView Rich Text Editor **❌ REJECTED**
  - **Reason:** Browser engine overhead, web-based complexity, native approach chosen
  - **File:** [tasks/00016_tiptap_rich_text_editor.md](tasks/00016_tiptap_rich_text_editor.md)

**Week 13: Settings System Fixes (ATOMIC TASKS 00021-00030)** 🔧 IN PROGRESS

> **Context:** Task #00020 (Navigator Panel) was executed but mixed with Settings System work, resulting in 6 bugs. New atomic approach: one small task at a time, full verification before next.

- [x] **Task #00020:** Project Navigator Panel + wxTreeCtrl **⚠️ COMPLETE WITH BUGS**
  - Hierarchical tree view structure created (3 tabs: Outline, Statistics, Bookmarks)
  - BUT: Settings System changes introduced bugs (Windows crash, Apply button, font scaling, etc.)
  - **Status:** ✅ Structure complete, 🔴 Requires fixes (#00021-00030) | **File:** [tasks/00020_project_navigator_panel.md](tasks/00020_project_navigator_panel.md)

- [x] **Task #00021:** Fix Windows Settings Dialog Crash **P0 CRITICAL** ✅ COMPLETE
  - Fix implemented: Defensive FitInside() checks, exception handling
  - CI/CD: ✅ Passing (Windows, Linux, macOS)
  - Manual testing: ✅ Verified working (no crash)
  - **File:** [tasks/00021_fix_windows_settings_crash.md](tasks/00021_fix_windows_settings_crash.md)
  - **Commit:** 258210b

- [x] **Task #00022:** Apply Button Event Binding **P1 HIGH** ✅ COMPLETE
  - EVT_BUTTON(wxID_APPLY) bound in SettingsDialog
  - EVT_SETTINGS_APPLIED custom event firing to MainWindow
  - Event-driven architecture implemented
  - **File:** [tasks/00022_apply_button_event_binding.md](tasks/00022_apply_button_event_binding.md)
  - **Commit:** a7299de

- [x] **Task #00023:** Icon Size Live Reload **P1 HIGH** ✅ COMPLETE
  - MainWindow::OnSettingsApplied() → IconRegistry::setSizes() → rebuild toolbar
  - Live reload working (instant visual feedback without restart)
  - **File:** [tasks/00023_icon_size_apply_implementation.md](tasks/00023_icon_size_apply_implementation.md)
  - **Commit:** a7299de

**Week 13-14: Command Registry Architecture (EPIC - 12 atomic tasks)** 📋 PLANNED

> **Context:** Settings System fixes (Tasks #00021-23) revealed need for centralized menu/toolbar command system. Current approach uses hardcoded command IDs (ID_FORMAT_BOLD, ID_VIEW_NAVIGATOR) scattered across files. Menu System Integration requires architectural rework before implementing Settings commands.

**Architectural Decision:** Implement Command Registry pattern (centralized command management, separation of concerns, plugin-ready architecture). See tasks/.wip/EPIC-command-registry-breakdown.md for full analysis.

- [x] **Task #00024:** Command Structure Implementation **P1 ARCHITECTURE** (45 min) ✅ COMPLETE
  - Implement IconSet struct (16/24/32px bitmap storage)
  - Implement KeyboardShortcut struct (toString/fromString parsing)
  - Implement Command struct (complete descriptor with callbacks)
  - **Status:** ✅ COMPLETE (2025-11-12) | **File:** tasks/00024_command_structure_implementation.md
  - **Commit:** (pending)

- [x] **Task #00025:** Implement Core Command Registry **P0 ARCHITECTURE** (75 min) ✅ COMPLETE
  - Implement CommandRegistry singleton (Meyers pattern, thread-safe)
  - Registration/unregistration/lookup methods (8 methods total)
  - 8 test cases (17 sections, 40 assertions) - all passing
  - **Status:** ✅ COMPLETE (2025-11-12) | **File:** tasks/00025_implement_core_command_registry.md
  - **Tests:** 100% pass rate (572 total assertions, 76 test cases)
  - **Commit:** (pending)

- [x] **Task #00026:** CommandRegistry Execution + Context **P0 ARCHITECTURE** (70 min) ✅ COMPLETE
  - Implement executeCommand() with full error handling
  - Add canExecute() and isChecked() precondition checking
  - CommandExecutionResult enum (5 states: Success, NotFound, Disabled, NoCallback, Failed)
  - CommandErrorHandler custom callback support
  - **Status:** ✅ COMPLETE (2025-11-12) | **File:** tasks/00026_command_registry_execution_context.md
  - **Tests:** 8 test cases, 41 assertions - 100% pass rate (613 total assertions, 84 test cases)
  - **Commit:** (pending)

- [x] **Task #00027:** Keyboard Shortcut Management **P1 ARCHITECTURE** (90 min) ✅ COMPLETE
  - Implement ShortcutManager singleton (Meyers pattern, thread-safe)
  - Binding/unbinding/query API (9 methods total)
  - JSON persistence (saveToFile, loadFromFile)
  - Integration with CommandRegistry (executeShortcut delegates)
  - operator< for KeyboardShortcut (std::map key compatibility)
  - **Status:** ✅ COMPLETE (2025-11-12, 75 min actual) | **File:** tasks/00027_keyboard_shortcut_management.md
  - **Tests:** 8 test cases, 42 assertions - 100% pass rate (655 total assertions, 91 test cases)
  - **Commit:** (pending)

- [x] **Task #00028:** Core Command Registration - File Menu **P1 IMPLEMENTATION** (60 min) ✅ COMPLETE
  - Register 6 File menu commands in CommandRegistry
  - Commands: file.new, file.open, file.save, file.save_as, file.settings, file.exit
  - Bind keyboard shortcuts (Ctrl+N, Ctrl+O, Ctrl+S, Ctrl+Shift+S, Ctrl+,, Alt+F4)
  - Refactor event handlers to use CommandRegistry::executeCommand()
  - **Status:** ✅ COMPLETE (2025-11-12, 55 min actual) | **File:** tasks/00028_core_command_registration_file_menu.md
  - **Tests:** Manual verification - all commands work via menu and shortcuts
  - **Full Test Suite:** 655 assertions, 91 test cases - 100% pass rate
  - **Note:** Settings handler NOT refactored (complex state management, deferred to future task)

- [x] **Task #00029:** Core Command Registration - Edit Menu **P1 IMPLEMENTATION** (45 min) ✅ COMPLETE
  - Register 6 Edit menu commands in CommandRegistry
  - Commands: edit.undo, edit.redo (stubs), edit.cut, edit.copy, edit.paste, edit.select_all (EditorPanel delegates)
  - Bind keyboard shortcuts (Ctrl+Z, Ctrl+Y, Ctrl+X, Ctrl+C, Ctrl+V, Ctrl+A)
  - Refactor event handlers to use CommandRegistry::executeCommand()
  - **Status:** ✅ COMPLETE (2025-11-12, 42 min actual) | **File:** tasks/00029_core_command_registration_edit_menu.md
  - **Tests:** Manual verification - all commands work via menu and shortcuts
  - **Full Test Suite:** 655 assertions, 91 test cases - 100% pass rate
  - **Note:** EditorPanel delegation creates dummy wxCommandEvent; TODO for Phase 2 refactoring to direct methods

- [x] **Task #00030:** Core Command Registration - Format Menu **P1 IMPLEMENTATION** (40 min) ✅ COMPLETE
  - Register 5 Format menu commands in CommandRegistry
  - Commands: format.bold, format.italic, format.underline, format.font, format.clear_formatting (all delegate to EditorPanel)
  - Bind keyboard shortcuts (Ctrl+B, Ctrl+I, Ctrl+U)
  - Refactor event handlers to use CommandRegistry::executeCommand()
  - **Status:** ✅ COMPLETE (2025-11-12, 38 min actual) | **File:** tasks/00030_core_command_registration_format_menu.md
  - **Tests:** Manual verification - all 5 commands work via menu and shortcuts
  - **Full Test Suite:** 655 assertions, 91 test cases - 100% pass rate
  - **Note:** EditorPanel delegation creates dummy wxCommandEvent; TODO for Phase 2 refactoring to direct methods

- [ ] **Task #00031:** Implement Dynamic Menu Builder **P0 ARCHITECTURE** (60-90 min)
  - Create MenuBuilder class (buildFromRegistry, addSeparator, addSubmenu)
  - Replace hardcoded createMenuBar() with dynamic builder
  - Verify all menus build correctly from CommandRegistry
  - **Status:** 📋 PLANNED | **Dependencies:** #00026-30 | **File:** tasks/00031_implement_dynamic_menu_builder.md

- [ ] **Task #00032:** Implement Dynamic Toolbar Builder **P0 ARCHITECTURE** (60-90 min)
  - Create ToolbarBuilder class (buildFromCommands, addSeparator, addControl)
  - Replace hardcoded createToolBar() with dynamic builder
  - Verify toolbar builds correctly, icons load properly
  - **Status:** 📋 PLANNED | **Dependencies:** #00031 | **File:** tasks/00032_implement_dynamic_toolbar_builder.md

- [ ] **Task #00033:** Add Settings Menu Commands **P1 INTEGRATION** (30-45 min)
  - Create 3 Settings commands (OpenSettings, ApplySettings, ResetSettings)
  - Register in CommandRegistry, add to Tools menu
  - Verify Settings Dialog opens, Apply works, Reset shows confirmation
  - **Status:** 📋 PLANNED | **Dependencies:** #00031 | **File:** tasks/00033_add_settings_menu_commands.md

- [ ] **Task #00034:** Verify Command State Management **P1 TESTING** (45-60 min)
  - Test enabled/disabled states (no document → Cut/Paste disabled)
  - Test dynamic updates (selection changes → format menu states)
  - Verify state propagation (menu + toolbar sync)
  - **Status:** 📋 PLANNED | **Dependencies:** #00032-33 | **File:** tasks/00034_verify_command_state_management.md

- [ ] **Task #00035:** Document Command Registry Architecture **P2 DOCUMENTATION** (30-45 min)
  - Write architecture document (class diagram, flow, examples)
  - Update ARCHITECTURE.md with Command Registry section
  - Add plugin integration guide (how plugins register commands)
  - **Status:** 📋 PLANNED | **Dependencies:** #00034 | **File:** tasks/00035_document_command_registry_architecture.md

**EPIC Summary:**
- **Total tasks:** 12 atomic tasks (all 30-90 minutes)
- **Estimated time:** 10-14 hours total
- **Priority:** P0 (blocks Settings System integration)
- **Dependencies:** Sequential execution (#00024 → #00025 → #00026-30 → #00031 → #00032 → #00033-35)
- **Benefits:** Centralized command management, plugin-ready architecture, separation of concerns
- **Breakdown file:** tasks/.wip/EPIC-command-registry-breakdown.md

**Week 14-15: Chapter Management (FUTURE EPIC - Will be broken into atomic tasks 00036+)**
- [ ] **EPIC:** Chapter Management CRUD Operations
  - Add, delete, rename, move chapters and parts
  - Drag & drop reordering
  - **Status:** 📋 Planned (after 00021-00030 complete) | **Priority:** P0
  - **Note:** Will be decomposed into 8-10 atomic tasks when ready

**Week 15-16: Persistence & Formatting (FUTURE EPICs)**
- [ ] **EPIC:** Content Save/Load Integration
  - Serialization, .klh ZIP storage, lazy loading
  - **Status:** 📋 Planned | **Priority:** P0
- [ ] **EPIC:** Text Styles + Paragraph Formatting
  - H1-H6, spacing, indentation, lists
  - **Status:** 📋 Planned | **Priority:** P1

**Week 16-17: Editor Reliability (FUTURE EPICs)**
- [ ] **EPIC:** Undo/Redo Command Pattern
  - ICommand interface, history management
  - **Status:** 📋 Planned | **Priority:** P0
- [ ] **EPIC:** Find & Replace
  - Search options, scope, Replace All
  - **Status:** 📋 Planned | **Priority:** P1

**Week 17-18: Data Safety (FUTURE EPICs)**
- [ ] **EPIC:** Auto-Save System
  - Configurable interval, background thread, crash recovery
  - **Status:** 📋 Planned | **Priority:** P0
- [ ] **EPIC:** Backup System
  - Rolling snapshots, retention policy, restore UI
  - **Status:** 📋 Planned | **Priority:** P1

**Week 19-20: UX Polish (FUTURE EPICs)**
- [ ] **EPIC:** Focus Modes + Perspectives
  - Normal, Focused, Distraction-Free modes
  - **Status:** 📋 Planned | **Priority:** P2
- [ ] **EPIC:** Keyboard Shortcuts System
  - 80+ shortcuts, customization UI, conflict detection
  - **Status:** 📋 Planned | **Priority:** P1
- [ ] **EPIC:** Word Count Live + Statistics Panel
  - Live count, session stats, charts
  - **Status:** 📋 Planned | **Priority:** P1
- [ ] **EPIC:** Status Bar + Info Bar
  - 8 segments, 5 message types
  - **Status:** 📋 Planned | **Priority:** P2
- [ ] **EPIC:** Spell Checking Integration
  - Red underline, suggestions, custom dictionary
  - **Status:** 📋 Planned | **Priority:** P2

**Before Phase 2: Settings System Enhancement**
- [ ] **Task #TBD:** Settings Schema Validation + Migration Framework (POZIOM 3)
  - JSON schema validation system for settings.json
  - Settings schema definitions with required keys, types, ranges
  - Validation on load with detailed error reporting
  - "Reset to defaults" dialog on validation errors with user confirmation
  - More robust migration framework (versioned migrations registry)
  - Migration testing infrastructure (unit tests for each migration)
  - Comprehensive error handling (corrupted files, partial migrations)
  - Documentation for adding new settings and migrations
  - **Status:** 📋 Planned (before Phase 2) | **Priority:** P0
  - **Dependencies:** Task #00020 (Settings Migration System - POZIOM 2) COMPLETE
  - **Rationale:** Professional settings management - automatic validation prevents user errors, robust migration ensures smooth upgrades, eliminates manual file editing

### Deliverables
✅ Functional rich text editor with formatting
✅ Complete project management (CRUD + navigation)
✅ Professional desktop UI with docking panels
✅ Auto-save and backup working reliably
✅ 80+ unit tests, 20+ integration tests

---

## Phase 2: Plugin System MVP (Weeks 21-30 | 2-3 months)

**Goal:** Prove plugin architecture with 4 working plugins

**Status:** ⏳ Pending
**Target Version:** 0.3.0-beta
**Timeline:** 2-3 months

### Plugin 1: DOCX Exporter (Free)
- [ ] python-docx integration
- [ ] Export Document → .docx with formatting preservation
- [ ] Basic styles mapping (headings, bold, italic)
- [ ] Paragraph formatting export
- [ ] Configuration UI (export options)

### Plugin 2: Markdown Tools (Free)
- [ ] Import .md → Document (markdown parsing)
- [ ] Export Document → .md (text + basic formatting)
- [ ] Markdown preview panel (real-time rendering)
- [ ] Syntax highlighting for Markdown mode

### Plugin 3: Basic Statistics (Free)
- [ ] Word count, character count (with/without spaces)
- [ ] Reading time estimation (configurable WPM)
- [ ] Session statistics (words written today, this week)
- [ ] Writing streak tracking
- [ ] Charts (matplotlib - bar charts, line graphs)
- [ ] Statistics panel (dockable)

### Plugin 4: Assistant Lion (Free)
- [ ] Graphical assistant panel (dockable, bottom-right)
- [ ] Lion animal graphics (6 moods: happy, thinking, praising, warning, sleeping, excited)
- [ ] Speech bubble UI (text messages from assistant)
- [ ] Basic triggers (break reminder, goal reached, session milestone)
- [ ] Personality system (Lion voice/tone)
- [ ] Configuration UI (enable/disable, frequency)

### Plugin Management
- [ ] Plugin list panel (installed plugins, status)
- [ ] Enable/disable functionality
- [ ] Plugin configuration UI (per-plugin settings)
- [ ] .kplugin installation (drag & drop, file picker)
- [ ] Plugin uninstallation
- [ ] Plugin update checking (version comparison)

### Plugin API
- [ ] Command registration (plugins add menu items, toolbar buttons)
- [ ] Panel registration (plugins add dockable panels)
- [ ] Event subscription (plugins listen to document events)
- [ ] Settings API (plugins save/load preferences)
- [ ] Resource access (plugins access icons, translations)

### Deliverables
✅ Plugin system proven with 4 diverse plugins
✅ Plugin installation/management working
✅ Plugin API documented for developers
✅ Community can start developing plugins

---

## Phase 3: Feature Plugins (Weeks 31-44 | 3-4 months)

**Goal:** Rich plugin ecosystem with premium offerings

**Status:** ⏳ Pending
**Target Version:** 0.4.0-beta
**Timeline:** 3-4 months

### Free Plugins
- [ ] **PDF Exporter** (reportlab) - Export to PDF with basic formatting
- [ ] **TXT/RTF Import/Export** - Plain text and rich text formats
- [ ] **Spell Checker** (hunspell) - Multi-language spell checking
- [ ] **Themes** (Dark, Savanna, Midnight) - UI color schemes
- [ ] **Keyboard Shortcuts Editor** - Custom shortcut configuration

### Premium Plugin: AI Assistant Pro ($19-29)
- [ ] 4 animals (Lion, Meerkat, Elephant, Cheetah) with unique personalities
- [ ] Advanced personality system (mood detection, context awareness)
- [ ] AI-powered suggestions (OpenAI/Claude API integration)
- [ ] Context-aware prompts (character development, plot suggestions)
- [ ] Flow state detection (don't interrupt when in the zone)
- [ ] Custom personality creation (user-defined assistant)
- [ ] License verification system

### Premium Plugin: Advanced Analytics ($14-19)
- [ ] Timeline visualization (plot events on interactive timeline)
- [ ] Character mention tracking (heatmap, character presence per chapter)
- [ ] Pacing analysis (action/dialogue/description ratios)
- [ ] Reading level analysis (Flesch-Kincaid, Gunning Fog)
- [ ] Sentiment analysis (emotional tone per chapter)
- [ ] Productivity trends (daily/weekly/monthly word counts)
- [ ] License verification system

### Core Features
- [ ] **Character Bank** - Character cards with photos, traits, relationships
- [ ] **Location Bank** - Location cards with maps, descriptions, photos
- [ ] **Notes System** - Yellow sticky notes attachable to chapters
- [ ] **Writer's Calendar** - Goals, deadlines, writing schedule

### Deliverables
✅ Rich free plugin ecosystem (5+ plugins)
✅ 2 premium plugins released and tested
✅ License verification working
✅ Character and Location banks functional

---

## Phase 4: Advanced Plugins (Weeks 45-56 | 2-3 months)

**Goal:** Professional writer's toolkit complete + Plugin ecosystem tools

**Status:** ⏳ Pending
**Target Version:** 0.5.0-rc
**Timeline:** 2-3 months

### Developer Tools for Plugin Creators
- [ ] **Plugin Development Guide** (comprehensive documentation)
  - Step-by-step tutorial with working examples
  - Plugin manifest reference (all fields explained)
  - Lifecycle hooks documentation (on_init, on_activate, on_deactivate)
  - Extension Points API reference
  - Event Bus usage patterns
- [ ] **Developer Mode in Kalahari** (optional, hidden by default)
  - Menu → Tools → Developer Tools (enable in Settings → Advanced)
  - Plugin Creator Wizard (step-by-step GUI)
  - Plugin Validator (manifest + structure checks)
  - Plugin Packager (.kplugin ZIP creator)
  - Live plugin reload (for development)
- [ ] **CLI Tools** (optional, for automation)
  - `tools/create_plugin.py` - Template generator
  - `tools/validate_plugin.py` - Validation script
  - `tools/package_plugin.py` - ZIP packager
- [ ] **Plugin Template Repository**
  - examples/hello_plugin/ (working minimal example)
  - examples/advanced_plugin/ (all features demonstrated)
  - plugin_manifest_schema.json (VSCode autocomplete)

**Why Phase 4?** API stable after Phase 2-3, community feedback available, marketplace preparation

### Premium Plugin: Professional Export Suite ($24-34)
- [ ] EPUB export (ebooklib - e-book publishing ready)
- [ ] Advanced PDF (custom formatting, TOC, index, headers/footers)
- [ ] Advanced DOCX (publisher-ready templates, styles)
- [ ] HTML export (website-ready, responsive)
- [ ] LaTeX export (academic writing, thesis templates)
- [ ] Export templates (Kindle Direct Publishing, IngramSpark)
- [ ] Batch export (multiple formats at once)

### Premium Plugin: Research & Sources Pro ($19-24)
- [ ] OCR for scanned documents (pytesseract/Tesseract)
- [ ] Web scraping assistant (article extraction)
- [ ] Citation management (Zotero integration)
- [ ] Advanced source organization (tagging, categorization)
- [ ] Automatic fact-checking hints (highlight potential errors)
- [ ] Bibliography generation (APA, MLA, Chicago formats)
- [ ] Research timeline (when facts were verified)

### Premium Plugin: Collaboration Pack ($29-39)
- [ ] Beta-reader mode (comments, suggestions, annotations)
- [ ] Editor mode (track changes, accept/reject)
- [ ] Version comparison (git-like diffs, side-by-side)
- [ ] Shared notes & annotations (multi-user)
- [ ] Real-time writing sprints (online sessions with friends)
- [ ] Export with comments (PDF with annotations)

### Deliverables
✅ 5 premium plugins complete
✅ Professional export capabilities
✅ Research tools functional
✅ Collaboration features working

---

## Phase 5: Polish & Release (Weeks 57-68 | 2-3 months)

**Goal:** Production-ready public release

**Status:** ⏳ Pending
**Target Version:** 1.0.0
**Timeline:** 2-3 months

### Testing & Quality
- [ ] Unit test coverage 70%+ (core + plugins)
- [ ] Integration tests (critical workflows)
- [ ] Beta testing program (20-30 real writers)
- [ ] Bug fixing marathon (prioritized backlog)
- [ ] Performance optimization (load time, memory usage)
- [ ] Accessibility review (screen readers, keyboard navigation)
- [ ] Security audit (plugin sandboxing, file handling)

### Documentation
- [ ] **User Manual** (English + Polish) - Complete guide for writers
- [ ] **Plugin API Documentation** - For plugin developers
- [ ] **Getting Started Guide** - Quick start tutorials
- [ ] **Video Tutorials** - Screencasts for key features
- [ ] **FAQ** - Common questions and troubleshooting
- [ ] **Release Notes** - Feature list and known issues

### Packaging & Distribution
- [ ] **Windows Installer** (NSIS - silent install, file associations)
- [ ] **macOS Installer** (DMG - drag-to-Applications, code signing)
- [ ] **Linux Packages** (AppImage universal + DEB/RPM optional)
- [ ] Embedded Python bundling (all platforms)
- [ ] Code signing (Windows Authenticode, macOS Developer ID)
- [ ] Auto-update system (check for new versions)
- [ ] GitHub Release automation (CI/CD release workflow)

### Launch
- [ ] **Website** (kalahari.app) - Project homepage, download links
- [ ] **GitHub Public Release** - MIT License, source code
- [ ] **Social Media** - Announcements (Twitter, Reddit, forums)
- [ ] **Blog Posts** - Launch announcement, feature highlights
- [ ] **Community Forum** - Support and discussion platform
- [ ] **Press Kit** - Screenshots, logo, description

### Deliverables
✅ Kalahari 1.0 - Public Release 🎉
✅ Professional installers for all platforms
✅ Complete documentation (user + developer)
✅ Marketing website and community forum
✅ Open source release on GitHub

---

## Post-1.0: Future Expansion

### Phase 6: Cloud Sync (3-6 months post-1.0)

**Goal:** Cloud synchronization and mobile companion

**Target Version:** 1.1.0
**Business:** Cloud Sync Pro subscription ($5-10/month)

Features:
- [ ] Cloud Sync Pro subscription system
- [ ] Dropbox/Google Drive integration (MVP)
- [ ] Own backend infrastructure (Phase 2)
- [ ] End-to-end encryption (AES-256)
- [ ] Conflict resolution (smart merging)
- [ ] Mobile companion app (iOS/Android - read-only)
- [ ] Web access (basic editor in browser)
- [ ] Automatic cloud backups (unlimited storage)
- [ ] Cross-device sessions (pick up where you left off)
- [ ] Premium support (email response within 24h)

### Phase 7: Collaboration (6-12 months post-1.0)

**Goal:** Multi-user collaboration and feedback

**Target Version:** 1.2.0
**Business:** Enhanced Collaboration Pack plugin

Features:
- [ ] **Serengeti** - Collaborative writing tool (separate app)
- [ ] Real-time co-writing (Google Docs style)
- [ ] Role-based permissions (author, editor, beta-reader)
- [ ] Comment threads (discussions on specific passages)
- [ ] Task assignments (editorial workflow)
- [ ] Version history (time machine for documents)
- [ ] Integration with Kalahari (seamless switching)

### Phase 8: Ecosystem (12-18 months post-1.0)

**Goal:** Complete writer's ecosystem

**Target Version:** 2.0.0
**Business:** Marketplace + ecosystem revenue

Features:
- [ ] **Plugin Marketplace** - Own platform for plugin distribution
- [ ] **Template Marketplace** - Pre-made project templates (genres, formats)
- [ ] **Okavango** - Research & knowledge management (separate app)
- [ ] **Kilimanjaro** - Project management for writers (separate app)
- [ ] **Victoria** - Advanced cloud sync (own backend, web app)
- [ ] **Zambezi** - Publishing toolkit (formatting, distribution)
- [ ] Community content (user-contributed themes, templates)
- [ ] Publishing partnerships (IngramSpark, Kindle Direct, etc.)

---

## Success Metrics

### Phase 1 Success (Documentation → Foundation)
- ✅ All 11 core documents complete (100%)
- ✅ All 7 architectural decisions finalized
- ✅ Architecture validated (03_architecture.md with C++ examples)
- ✅ Plugin system designed (04_plugin_system.md with Extension Points)
- ⏳ CMake + vcpkg working on all platforms (Phase 0 Week 1)
- ⏳ Plugin system loading sample plugin (Phase 0 Week 6)

### Phase 2 Success (MVP 1.0 Release)
- 🎯 1,000+ GitHub stars
- 🎯 10,000+ active users (downloads)
- 🎯 50+ community contributors
- 🎯 Positive reviews (Reddit, ProductHunt, HackerNews)
- 🎯 10+ third-party plugins created

### Phase 3 Success (Premium Features)
- 🎯 10% conversion to premium plugins
- 🎯 Sustainable revenue (covering development costs)
- 🎯 5,000+ premium plugin sales
- 🎯 4.5+ star average rating

### Phase 4 Success (Cloud Services)
- 🎯 Cloud service profitability
- 🎯 3-5% conversion to subscription
- 🎯 Multi-platform sync working flawlessly
- 🎯 Enterprise customers (writing teams, publishers)

---

## Risk Mitigation

### Technical Risks
- **Cross-platform issues:** Continuous CI/CD testing, regular platform smoke tests
- **Plugin stability:** Sandboxing, version compatibility checks, plugin review process
- **Performance:** Profiling from Phase 1, optimization sprints, lazy loading
- **wxWidgets limitations:** Early prototyping, fallback plans, community support

### Business Risks
- **Low conversion:** Focus on exceptional free tier, gradual premium upsell
- **Competition:** Differentiate with unique features (Command Registry, customization)
- **Market fit:** Beta testing with real writers, community feedback loops
- **Sustainability:** Diversified revenue (plugins + cloud), open source community

### Timeline Risks
- **Scope creep:** Strict phase boundaries, MVP focus, defer nice-to-haves
- **Platform parity:** Automated testing, dedicated platform QA weeks
- **Plugin complexity:** Start simple (Phase 2), iterate based on feedback
- **Documentation lag:** Document parallel with implementation, dedicated docs phase

---

## Dependencies

### External Dependencies
- **wxWidgets:** GUI framework (stable, mature, well-documented)
- **vcpkg:** Package manager (official, Microsoft-backed)
- **Python 3.11:** Plugin runtime (stable, 5+ years support)
- **pybind11:** C++/Python binding (active development, proven in production)

### Internal Dependencies
- **Phase 0 → Phase 1:** Plugin system must be functional before plugins
- **Phase 1 → Phase 2:** Core editor needed for plugin testing
- **Phase 2 → Phase 3:** Plugin API stability required for premium plugins
- **Phase 4 → Phase 5:** All features complete before polish/testing

### Community Dependencies
- **Beta testers:** Recruit during Phase 3-4, active in Phase 5
- **Plugin developers:** Onboard during Phase 2, showcase in Phase 3+
- **Translators:** Recruit during Phase 4, active in Phase 5

---

## Review & Updates

### Roadmap Review Cycle
- **Monthly:** Progress review, adjust timelines if needed
- **Per Phase:** Retrospective, lessons learned, update estimates
- **Quarterly:** Community update, public roadmap sync

### Update Process
1. Propose changes (issue or PR)
2. Discuss with core team (if applicable)
3. Update ROADMAP.md
4. Update CHANGELOG.md (document decision)
5. Sync with CLAUDE.md (if architectural change)

### Version History
- **v1.0** (2025-10-25) - Initial roadmap based on 6-phase architecture

---

## Links

- **Master Project File:** [CLAUDE.md](CLAUDE.md)
- **Changelog:** [CHANGELOG.md](CHANGELOG.md)
- **Documentation Index:** [project_docs/README.md](project_docs/README.md)
- **Roadmap Maintenance Rules:** [project_docs/06_roadmap.md](project_docs/06_roadmap.md)
- **MVP Tasks:** [project_docs/07_mvp_tasks.md](project_docs/07_mvp_tasks.md)

---

**Last Updated:** 2025-11-04
**Next Review:** Task #00019 completion (Day 15)
