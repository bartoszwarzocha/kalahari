# Kalahari Development Roadmap

> **Writer's IDE** - 18-Month Journey from Concept to Public Release

**Current Status:** ✅ Phase 0 COMPLETE → 🚀 Starting Phase 1 (Custom Text Editor Control)
**Version:** 0.0.1-dev
**Last Updated:** 2025-11-03

---

## Overview

This roadmap outlines the development journey of Kalahari from initial concept to public 1.0 release. The project follows a **6-phase development strategy** spanning 14-20 months (realistically 18 months), with an **Open Core + Premium Plugins** business model.

**Key Milestones:**
- ✅ **Documentation Complete** (2025-10-25) - All 11 core documents finalized (100%)
- ✅ **Architectural Decisions** (2025-10-25) - All 7 core decisions finalized
- ✅ **Phase 0: Foundation** (2025-10-26 to 2025-11-03) - **COMPLETE** 🎉
  - Core infrastructure + Plugin system + Document model + bwx_sdk integration + CI/CD optimization
- 🚀 **Phase 1: Core Editor** (Weeks 9-20 | 3-4 months) - **STARTING NOW**
  - Next: Task #00019 (Custom wxWidgets Text Editor Control)
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

## Phase 1: Core Editor (Weeks 9-20 | 12 weeks) 🚀 STARTING

**Goal:** Functional rich text editor with project management

**Status:** 🚀 **IN PROGRESS** (Starting 2025-11-03)
**Target Version:** 0.2.0-alpha
**Timeline:** 12 weeks (3 months)
**Strategic Decision:** Custom wxWidgets text editor control (not wxRichTextCtrl, not web-based)

### Phase 1 Task List - UPDATED (2025-11-03)

**Week 9: Foundation (UI Infrastructure)** ✅
- [x] **Task #00013:** wxAUI Docking System + Panel Management **✅ COMPLETE**
  - 6 core panels: Navigator, Editor, Properties, Statistics, Search, Assistant
  - PerspectiveManager (4 default perspectives)
  - IconRegistry + ArtProvider (Material Design SVG icons)
  - AboutDialog + ManagePerspectivesDialog
  - **Status:** ✅ COMPLETE (2025-11-01) | **File:** [tasks/00013_wxaui_docking_system.md](tasks/00013_wxaui_docking_system.md)
  - **Note:** Optional UX polish (drag & drop perspective customization) deferred to Phase 1 end

**Week 10-12: Core Editor (Custom Control)** 🚀 NEXT
- [ ] **Task #00019:** Custom wxWidgets Text Editor Control **🚀 NEXT TASK**
  - Custom text rendering control (not wxRichTextCtrl!)
  - bwx_sdk patterns integration
  - Basic text rendering + formatting API
  - Undo/Redo support
  - **Status:** 📋 Planned (Next task) | **File:** [tasks/00019_custom_text_editor_control.md](tasks/00019_custom_text_editor_control.md)
  - **Note:** See `bwx_sdk_custom_control_template_comprehensive.md` for implementation pattern
- [x] **Task #00015:** wxRichTextCtrl Integration **❌ REJECTED**
  - **Reason:** Insufficient control over features, custom control chosen instead
  - **File:** [tasks/00015_wxrichtextctrl_integration_REJECTED.md](tasks/00015_wxrichtextctrl_integration_REJECTED.md)
- [x] **Task #00016:** TipTap + wxWebView Rich Text Editor **❌ REJECTED**
  - **Reason:** Browser engine overhead, web-based complexity, native approach chosen
  - **File:** [tasks/00016_tiptap_rich_text_editor.md](tasks/00016_tiptap_rich_text_editor.md)

**Week 13: Document Structure** (After #00019)
- [ ] **Task #00020:** Project Navigator Panel + wxTreeCtrl
  - Hierarchical tree view: Book → Parts → Chapters
  - Node selection → load chapter in editor
  - Context menu (right-click CRUD operations)
  - **Status:** 📋 Planned (deferred after #00019) | **File:** [tasks/00020_project_navigator_panel.md](tasks/00020_project_navigator_panel.md)
  - **Dependency:** Task #00019 MUST be complete first (avoids refactoring)

**Week 14: CRUD Operations**
- [ ] **Task #TBD:** Chapter Management CRUD Operations
  - Add, delete, rename, move chapters and parts
  - Drag & drop reordering
  - **Status:** 📋 Planned | **Priority:** P0

**Week 15: Persistence Enhancement**
- [ ] **Task #TBD:** Content Save/Load Integration
  - Serialization from custom editor control
  - Content storage in .klh ZIP (chapters/*.dat or *.rtf)
  - Lazy loading strategy
  - **Status:** 📋 Planned | **Priority:** P0

**Week 16: Advanced Formatting**
- [ ] **Task #TBD:** Text Styles + Paragraph Formatting
  - Styles: H1-H6, body, quotes, code
  - Paragraph spacing, indentation
  - Bullet/numbered lists
  - **Status:** 📋 Planned | **Priority:** P1

**Week 15: Editor Reliability**
- [ ] **Task #00019:** Undo/Redo Command Pattern
  - ICommand interface (execute/undo)
  - TextEditCommand + StructureEditCommand
  - Command history (100 default, configurable)
  - **Status:** 📋 Planned | **Priority:** P0

**Week 16: Editor Utility**
- [ ] **Task #00020:** Find & Replace
  - Search options: case-sensitive, whole word, regex
  - Find Next/Previous, Replace All
  - Search scope: current/all chapters
  - **Status:** 📋 Planned | **Priority:** P1

**Week 17: Data Safety (Auto-Save)**
- [ ] **Task #00021:** Auto-Save System
  - Configurable interval (1-10 minutes)
  - Background thread, dirty flag tracking
  - Crash recovery
  - **Status:** 📋 Planned | **Priority:** P0

**Week 18: Data Safety (Backup)**
- [ ] **Task #00022:** Backup System
  - Rolling snapshots (hourly, daily, weekly)
  - Retention policy, restore UI
  - Background backup thread
  - **Status:** 📋 Planned | **Priority:** P1

**Week 19: UX Polish**
- [ ] **Task #00023:** Focus Modes + Perspectives
  - 3 modes: Normal, Focused, Distraction-Free
  - Perspective save/load (4 defaults)
  - **Status:** 📋 Planned | **Priority:** P2

- [ ] **Task #00024:** Keyboard Shortcuts System
  - 80+ default shortcuts
  - Customization UI, conflict detection
  - Platform-specific defaults
  - **Status:** 📋 Planned | **Priority:** P1

**Week 20: Writing Feedback & Polish**
- [ ] **Task #00025:** Word Count Live + Statistics Panel
  - Live word count (status bar)
  - Session statistics, writing streak
  - Statistics panel with charts
  - **Status:** 📋 Planned | **Priority:** P1

- [ ] **Task #00026:** Status Bar + Info Bar
  - Status bar (8 segments: word count, cursor, time, etc.)
  - Info bar (5 message types)
  - **Status:** 📋 Planned | **Priority:** P2

- [ ] **Task #00027:** Spell Checking Integration
  - Red wavy underline, suggestions
  - Language selection, custom dictionary
  - **Status:** 📋 Planned | **Priority:** P2

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

**Last Updated:** 2025-10-25
**Next Review:** Start of Phase 0 (project kickoff)
