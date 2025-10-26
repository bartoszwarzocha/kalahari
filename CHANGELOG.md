# Changelog

All notable changes to the Kalahari project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Development Phase (Phase 0 - Foundation)

#### Added
- **Week 1: Project Setup & Infrastructure (2025-10-26)**
  - Day 1: Project structure (src/, include/, tests/, docs/, plugins/, resources/)
  - Day 2: CMake build system (C++20, multi-platform support, version.h template)
  - Day 3: vcpkg dependency management (wxWidgets 3.3.1+, spdlog, nlohmann_json, libzip, Catch2)
  - Day 4: GitHub Actions CI/CD workflow
    - Matrix build: Windows/macOS/Linux × Debug/Release (6 combinations)
    - vcpkg caching for 3-5x faster builds
    - Automated testing with ctest
    - Build artifacts upload (binaries + test results)
    - Platform-specific dependencies (GTK3, Ninja, system libraries)
- Console application (main.cpp) - Displays version info, platform, build type
- Unit tests (test_main.cpp) - 7 assertions, 2 test cases (Catch2 BDD style)
- Compiled binaries: kalahari + kalahari-tests (both working)

### Documentation Phase (Pre-Development)

#### Added
- Complete project documentation structure (10 core documents)
- 01_overview.md - Project vision, goals, target audience
- 02_tech_stack.md - C++20, wxWidgets 3.2+, vcpkg, Python plugins
- 03_architecture.md - System architecture and design patterns
- 04_plugin_system.md - Plugin API specification and extension points
- 05_business_model.md - Open Core + Plugin Marketplace + SaaS strategy
- 06_roadmap.md - 18-month development timeline (Phases 0-5)
- 07_mvp_tasks.md - Detailed week-by-week task breakdown
- 08_gui_design.md - Comprehensive GUI architecture with customizable toolbars
- 09_i18n.md - Internationalization system (wxLocale + gettext)
- 10_branding.md - Visual identity, logo system, animal mascots
- CLAUDE.md - Master project file with AI instructions and decisions
- work_scripts/ directory - Temporary utility scripts (git-ignored)
- .gitignore - Comprehensive ignore patterns for C++/wxWidgets/Python

#### Changed
- Renamed init_concept/ → concept_files/ (better reflects ongoing use)
- Updated project language policy: All code/comments in English (mandatory)
- Finalized tech stack: Python → C++20 for core, Python for plugins only
- Business model finalized: 5 premium plugins + Cloud Sync subscription

#### Decided
- Platform strategy: Windows/macOS/Linux from MVP (parallel development)
- Plugin architecture: From day zero (pybind11, .kplugin format)
- GUI system: wxWidgets + wxAUI with customizable toolbars (killer feature)
- Assistant default: Lion (brand symbol, storyteller archetype)
- MVP timeline: 18 months (realistically, 14-20 months estimated)
- Versioning strategy: Semantic Versioning 2.0.0
- **Architectural Decisions (2025-10-25):**
  - GUI Pattern: MVP (Model-View-Presenter)
  - Error Handling: Hybrid (exceptions + error codes + wxLog*)
  - Dependency Management: Hybrid (Singletons infrastructure + DI business logic)
  - Threading: Dynamic thread pool (2-4 workers, CPU-aware)
  - Memory: Lazy loading from Phase 1 (metadata eager, content on-demand)
  - Undo/Redo: Command pattern (100 commands default, configurable)
  - Document Model: Composite pattern (Book → Parts → Chapters)

#### Removed
- .claude/ cleanup (24 files, 32% reduction):
  - docs/ (framework glossary - enterprise focused)
  - templates/version-management/ (redundant with CHANGELOG.md)
  - prompts/workflows/ (9 enterprise multi-team workflows)
  - prompts/agents/security/ (6 enterprise security prompts)
  - hooks/ (17 enterprise automation scripts)
  - Result: 51 files remaining (all relevant for C++ desktop app)

---

## Version History

### Documentation Releases

#### [DOCS-1.1] - 2025-10-25
**GUI Design & i18n Complete**

Added:
- 08_gui_design.md (1,719 lines) - Complete GUI architecture
  - Command Registry system (unified core + plugin commands)
  - Customizable toolbars (6 default toolbars, QAT, Live Customization)
  - 4 Perspectives (Writer, Editor, Researcher, Planner)
  - Panel catalog (9 core panels + plugin support)
  - Gamification system (25+ badges, challenges, streak tracking)
  - Focus modes (Normal, Focused, Distraction-Free)
  - Command Palette (VS Code style)
- 09_i18n.md verification (1,087 lines) - Complete i18n system
  - wxLocale + gettext integration
  - bwx_sdk proven pattern
  - English (primary) + Polish (secondary) for MVP

Changed:
- README.md documentation index - All 10 documents marked complete
- CLAUDE.md version → 4.0 (C++ architecture finalized)

#### [DOCS-1.0] - 2025-10-24
**C++ Architecture Finalized**

Added:
- Complete documentation structure (project_docs/)
- CLAUDE.md v4.0 with finalized C++ architecture
- 6 core documents (Overview, Tech Stack, Business Model, Roadmap, Branding, i18n)
- 4 placeholder documents (Architecture, Plugin System, MVP Tasks, GUI Design)

Changed:
- Tech stack: Python 3.11+ → C++20 + wxWidgets 3.2+
- Build system: (none) → CMake 3.21+ + vcpkg
- Python role: Main language → Embedded 3.11 (plugins only)
- Timeline: 5-6 months → 18 months (realistic estimate)
- Plugin architecture: Retrofit → From day zero

Decided:
- Business model: Open Core (MIT) + 5 Premium Plugins + Cloud Sync SaaS
- Plugin format: .kplugin (ZIP containers)
- Plugin binding: pybind11 (C++/Python interop)
- Platform strategy: All three from MVP (Windows, macOS, Linux)
- CI/CD: GitHub Actions matrix builds
- Testing: Catch2 v3 (BDD style, 70%+ coverage target)
- Assistant: 8 animals total, 4 in MVP, Lion as default

#### [DOCS-0.1] - 2025-10-22
**Initial Concept**

Added:
- CLAUDE.md v1.0 - Initial lightweight project file
- concept_files/ directory with original concept documents
- Basic project structure and naming conventions

Decided:
- Project name: Kalahari (Writer's IDE)
- African naming convention for entire ecosystem
- Graphical assistant concept (8 animals with personalities)
- Target audience: Novel writers, non-fiction authors

---

## Future Releases (Planned)

### [0.1.0-alpha] - Phase 0 Complete (Target: +2-3 months)
Foundation infrastructure

Expected features:
- CMake + vcpkg build system working on all platforms
- wxWidgets basic application window
- Plugin Manager (discovery, loading, unloading)
- Python 3.11 embedding with pybind11
- Extension Points system
- Event Bus (async, thread-safe)
- .klh file format (ZIP + JSON)

### [0.2.0-alpha] - Phase 1 Complete (Target: +5-7 months)
Core editor functional

Expected features:
- Rich text editor (wxRichTextCtrl)
- Project Navigator panel
- Chapter management
- Save/Load (.klh files)
- Auto-save and backup system
- wxAUI docking (6 panels)
- 3 focus modes

### [0.3.0-beta] - Phase 2 Complete (Target: +7-10 months)
Plugin system proven

Expected features:
- 4 working plugins (DOCX, Markdown, Statistics, Assistant Lion)
- Plugin installation UI
- Plugin Management panel
- .kplugin format working

### [0.4.0-beta] - Phase 3 Complete (Target: +10-14 months)
Feature-rich

Expected features:
- Premium plugin: AI Assistant Pro (4 animals)
- Premium plugin: Advanced Analytics
- Free plugins: PDF export, spell checker, themes
- Character & Location banks

### [0.5.0-rc] - Phase 4 Complete (Target: +12-17 months)
Professional toolkit

Expected features:
- Premium plugin: Export Suite (EPUB, advanced DOCX)
- Premium plugin: Research Pro (OCR, citations)
- Notes system
- Writer's calendar

### [1.0.0] - Public Release (Target: +14-20 months)
MVP complete

Expected features:
- All MVP features tested and polished
- User manual (EN + PL)
- Plugin API documentation
- Installers (Windows NSIS, macOS DMG, Linux AppImage)
- Code signing
- Public GitHub release

---

## Notes

### Versioning Scheme

**During Development (Pre-1.0):**
- 0.MINOR.PATCH-PRERELEASE (e.g., 0.1.0-alpha, 0.3.0-beta)
- MINOR = Phase number (0.1 = Phase 0, 0.2 = Phase 1, etc.)
- PATCH = Bug fixes within phase
- PRERELEASE = alpha (Phases 0-1), beta (Phases 2-4), rc (Phase 5)

**Documentation Versions:**
- DOCS-X.Y (independent of code versions)
- Tracks major documentation milestones

**After 1.0 Release:**
- MAJOR.MINOR.PATCH (standard SemVer)
- MAJOR = Breaking changes to core or plugin API
- MINOR = New features, backward-compatible
- PATCH = Bug fixes, no new features

### Changelog Categories

- **Added** - New features
- **Changed** - Changes in existing functionality
- **Deprecated** - Soon-to-be removed features
- **Removed** - Removed features
- **Fixed** - Bug fixes
- **Security** - Security vulnerability fixes
- **Decided** - Key project decisions (pre-development only)

### References

- [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
- [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
- Project Roadmap: [ROADMAP.md](ROADMAP.md)
- Master Project File: [CLAUDE.md](CLAUDE.md)
