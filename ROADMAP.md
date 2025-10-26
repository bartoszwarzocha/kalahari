# Kalahari Development Roadmap

> **Writer's IDE** - 18-Month Journey from Concept to Public Release

**Current Status:** 🔄 Phase 0 - Foundation (Week 2 Complete)
**Version:** 0.0.1-dev
**Last Updated:** 2025-10-26

---

## Overview

This roadmap outlines the development journey of Kalahari from initial concept to public 1.0 release. The project follows a **6-phase development strategy** spanning 14-20 months (realistically 18 months), with an **Open Core + Premium Plugins** business model.

**Key Milestones:**
- ✅ **Documentation Complete** (2025-10-25) - All 11 core documents finalized (100%)
- ✅ **Architectural Decisions** (2025-10-25) - All 7 core decisions finalized
- 🔄 **Phase 0: Foundation** (Weeks 1-8 | 2-3 months) - NEXT
- ⏳ **Phase 1: Core Editor** (Weeks 9-20 | 3-4 months)
- ⏳ **Phase 2: Plugin System MVP** (Weeks 21-30 | 2-3 months)
- ⏳ **Phase 3: Feature Plugins** (Weeks 31-44 | 3-4 months)
- ⏳ **Phase 4: Advanced Plugins** (Weeks 45-56 | 2-3 months)
- ⏳ **Phase 5: Polish & Release** (Weeks 57-68 | 2-3 months)
- 🎯 **Public Release: Kalahari 1.0** (Target: Q2-Q3 2026)

---

## Phase 0: Foundation (Weeks 1-8 | 2-3 months)

**Goal:** Build technical infrastructure and plugin architecture foundation

**Status:** 🔄 In Progress (Week 2/8 Complete - GUI & Threading Ready)
**Target Version:** 0.1.0-alpha
**Timeline:** 2-3 months from project start
**Started:** 2025-10-26
**Week 2 Completed:** 2025-10-26

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
- [ ] Settings system (JSON persistence with nlohmann_json)
- [x] **Logging system (spdlog - structured, multi-level)** ✅ Week 2

### Plugin Architecture
- [ ] Python 3.11 embedding (bundled with application)
- [ ] pybind11 integration (C++/Python interop)
- [ ] Plugin Manager (discovery, loading, unloading, lifecycle)
- [ ] Extension Points system (C++ interfaces for plugins)
- [ ] Event Bus (async, thread-safe, GUI-aware marshalling)
- [ ] .kplugin format handler (ZIP reading/writing with libzip)
- [ ] Plugin API versioning (semantic versioning checks)

### Document Model
- [ ] Core C++ classes (Document, Chapter, Book, Project)
- [ ] JSON serialization (nlohmann_json)
- [ ] .klh file format (ZIP container with JSON metadata)
- [ ] Basic CRUD operations (create, read, update, delete)
- [ ] In-memory document management

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

## Phase 1: Core Editor (Weeks 9-20 | 3-4 months)

**Goal:** Functional rich text editor with project management

**Status:** ⏳ Pending
**Target Version:** 0.2.0-alpha
**Timeline:** 3-4 months

### Editor
- [ ] wxRichTextCtrl integration and customization
- [ ] Basic text formatting (bold, italic, underline, font, size, color)
- [ ] Text styles (headings H1-H6, body text, quotes, code)
- [ ] Paragraph formatting (alignment, spacing, indentation)
- [ ] Undo/redo system (command pattern)
- [ ] Find & Replace (simple text search)
- [ ] Live word count and statistics
- [ ] Spell checking integration (basic)

### Project Management
- [ ] Project Navigator panel (wxTreeCtrl - hierarchical view)
- [ ] Chapter management (add, delete, move, rename, reorder)
- [ ] Part/Section organization (nested structure)
- [ ] Document metadata (title, author, dates, custom fields)
- [ ] Save/Load .klh files (ZIP + JSON + RTF content)
- [ ] Auto-save system (configurable interval, background thread)
- [ ] Backup system (rolling snapshots, configurable retention)
- [ ] Recent projects list (MRU - Most Recently Used)

### UI/UX
- [ ] wxAUI docking system (6 core panels)
- [ ] Panel visibility controls (View menu, toolbar buttons)
- [ ] 3 focus modes (Normal, Focused, Distraction-Free)
- [ ] Perspectives system (saveable layouts)
- [ ] Keyboard shortcuts (80+ defaults, customizable)
- [ ] Toolbar customization UI (drag & drop)
- [ ] Status bar (8 segments: word count, cursor, time, etc.)
- [ ] Info bar (5 message types: success, warning, error, info, hint)

### Testing
- [ ] Unit tests for document model (Catch2)
- [ ] Integration tests for save/load
- [ ] Manual testing on all platforms

### Deliverables
✅ Functional rich text editor
✅ Complete project management (create, save, load, organize)
✅ Professional desktop UI with docking panels
✅ Auto-save and backup working reliably

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

**Goal:** Professional writer's toolkit complete

**Status:** ⏳ Pending
**Target Version:** 0.5.0-rc
**Timeline:** 2-3 months

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
- **Detailed Roadmap:** [project_docs/06_roadmap.md](project_docs/06_roadmap.md)
- **MVP Tasks:** [project_docs/07_mvp_tasks.md](project_docs/07_mvp_tasks.md)

---

**Last Updated:** 2025-10-25
**Next Review:** Start of Phase 0 (project kickoff)
