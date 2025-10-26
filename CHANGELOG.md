# Changelog

All notable changes to the Kalahari project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Project Cleanup & Optimization (2025-10-26)

**Massive cleanup based on dependency analysis: removed 54.6KB of unused files, streamlined quality framework**

#### Removed
- **`.claude/hooks/` directory (entire, 31.6KB)** - NON-FUNCTIONAL hooks
  - `pre-commit-quality.sh` (7.6KB) - Not executed (missing Claude Code settings)
  - `session-start.sh` (13KB) - Not executed (missing configuration)
  - `quality-reminder.sh` (11KB) - Not executed (missing configuration)
  - **Reason**: Hooks require user configuration in Claude Code settings that was never set up
  - **Impact**: Zero - these files were never executed automatically

- **`.claude/INTEGRATION.md` (23KB)** - REDUNDANT documentation
  - Created 30 minutes prior for CLAUDE.md size optimization
  - Never loaded automatically (requires manual @ reference)
  - Duplicated information from skills/commands/agents files
  - **Reason**: 41.5KB CLAUDE.md is acceptable (only 3.6% over 40k threshold)
  - **Impact**: Removed unnecessary file to manage

**Total removed: 54.6KB of non-functional/redundant files**

#### Changed
- **`.claude/QUALITY_CHECKLIST.md` - Complete redesign (454 → 253 lines, -44%)**
  - **Before**: Comprehensive 100+ point checklist for every commit
  - **After**: Release-focused manual verification checklist
  - **Rationale**:
    - Only 15% of points were enforced by hooks (8 checks vs 100+ points)
    - Hooks don't work anyway (not configured)
    - Most checks can be automated in bash script
  - **New focus**:
    - Manual UX testing
    - Documentation review (comprehension, not just presence)
    - Release notes writing
    - Installer testing (Windows/macOS/Linux)
    - Stakeholder approval
  - **Version**: 1.0 → 2.0 (Release-Focused)
  - **Automation**: Refers to `tools/pre-commit-check.sh` for automated checks

- **`tools/health-check.sh` → `tools/project-status.sh` (RENAMED)**
  - **Reason**: Naming conflict with `/health-check` slash command
  - **Distinction**:
    - `/health-check` - AI-driven project analysis (Claude Code slash command)
    - `tools/project-status.sh` - Automated file/tool checks (bash script)
  - **No functional changes** - only renamed for clarity

- **CLAUDE.md references updated**
  - Removed reference to deleted `.claude/INTEGRATION.md`
  - Removed "3 Hooks" from resources list
  - Updated Quick Start section:
    - Changed `.claude/hooks/pre-commit-quality.sh` → `./tools/pre-commit-check.sh`
    - Changed `tools/health-check.sh` → `tools/project-status.sh`
  - Added new Resources section explaining quality tools

#### Added
- **`tools/pre-commit-check.sh` (17KB, executable)** - FUNCTIONAL automated quality verification
  - **Replaces**: Non-functional `.claude/hooks/pre-commit-quality.sh`
  - **Difference**: Actually works (bash script, not Claude Code hook)
  - **12 check categories with 35+ automated checks**:
    1. Code Formatting (clang-format verification)
    2. Naming Conventions (m_ prefix, snake_case files)
    3. Modern C++ Practices (no raw new/delete, smart pointers)
    4. Documentation (Doxygen comments, minimal commented code)
    5. Architecture Compliance (MVP pattern, no wx in Model)
    6. Internationalization (UI strings wrapped in _("..."))
    7. Build System (CMakeLists.txt ↔ vcpkg.json consistency)
    8. Documentation Consistency (CLAUDE.md ↔ CHANGELOG.md dates)
    9. Code Annotations (TODO/FIXME tracking)
    10. Security (hardcoded secrets detection)
    11. Testing (test coverage ratio verification)
    12. File Size (>1000 line warnings)
  - **Quality gates**:
    - <70%: ❌ DO NOT COMMIT (exit 1)
    - 70-89%: ⚠️ ACCEPTABLE (exit 0, with warnings)
    - 90-100%: ✅ EXCELLENT (exit 0)
  - **Color-coded output** with issues/warnings summary
  - **Execution**: `./tools/pre-commit-check.sh` (manual or git hook)

#### Summary

**Files before cleanup**: 22 files
**Files after cleanup**: 17 files (-23%)

**Cleanup strategy**:
- ❌ Remove: Non-functional files (hooks never executed)
- ❌ Remove: Redundant documentation (INTEGRATION.md duplicated info)
- ♻️ Redesign: QUALITY_CHECKLIST.md (release-focused, 44% smaller)
- 🆕 Replace: Functional pre-commit-check.sh (actually works, 35+ checks)
- 📝 Clarify: Renamed health-check.sh to avoid /health-check conflict

**Result**: Cleaner project structure with ONLY functional files that provide value.

---

### Directory Restructure & AI Rules Enhancement (2025-10-26)

**Renamed `work/` → `tools/` for better clarity and updated all references + added critical AI assistant rules**

#### Changed
- **`work/` → `tools/` directory rename**
  - **Rationale**: Name `tools/` is more universal and flexible for future development/QA scripts
  - **Impact**: All 4 files moved (check-ci.sh, pre-commit-check.sh, project-status.sh, README.md)
  - **References updated** in 6 files:
    - `.claude/QUALITY_CHECKLIST.md` (4 references)
    - `.claude/settings.local.json` (1 permission + removed 7 obsolete entries)
    - `CLAUDE.md` (4 references in Quick Start + Resources sections)
    - `CHANGELOG.md` (current section references)
    - `tools/project-status.sh` (internal path checks)
    - `tools/README.md` (~50 usage examples)

- **CLAUDE.md AI Instructions - 2 new critical rules added**
  - **Rule #7: Ask when uncertainty ≥10%**
    - "If less than 90% certain about user's intentions, ALWAYS ask for clarification before proceeding"
    - **Purpose**: Prevent chaotic work patterns, reduce mistakes from assumptions
    - **Trigger**: User feedback about chaotic work and unclear intentions

  - **Rule #8: Quality over size**
    - "Prioritize content quality and correctness over file size or token count. A high-quality document is better than incomplete one."
    - **Purpose**: Focus on quality/completeness, not arbitrary size limits
    - **Rationale**: Previous over-optimization (CLAUDE.md 50.7KB→41.5KB, creating/deleting INTEGRATION.md) was counterproductive

#### Summary

**Files renamed**: 1 directory (`work/` → `tools/`)
**Files updated**: 6 documentation files
**References changed**: ~65 total
**New AI rules**: 2 (uncertainty handling + quality priority)

**Impact**: Better project organization, clearer naming, and AI assistant with stronger quality guardrails.

---

### Quality Assurance Framework (2025-10-26)

#### Added
- **Quality Assurance Infrastructure**
  - `.claude/QUALITY_CHECKLIST.md` - Comprehensive 10-section quality checklist (454 lines)
    - Code Quality (formatting, style, organization, documentation)
    - Architecture Compliance (MVP pattern, DI, threading, memory)
    - Plugin System (Extension Points, Event Bus, pybind11, GIL)
    - Internationalization (wxLocale, gettext, 6 languages)
    - Testing (Catch2 BDD, coverage targets, test execution)
    - Documentation (CLAUDE.md, CHANGELOG.md, ROADMAP.md, project_docs/)
    - Build System (CMake, vcpkg, compiler warnings)
    - CI/CD (GitHub Actions, 3 platforms)
    - Security (code security, plugin security, dependencies)
    - Performance (startup time, memory usage, responsiveness)
    - Pre-commit quick check commands
    - Quality scoring system (70%/90%/100% gates)
    - Release-specific checklist (versioning, installers, docs)

  - `work/health-check.sh` - Comprehensive project health dashboard (29KB, executable)
    - 8 health categories with automated checks
    - Documentation consistency verification
    - Claude Code resources validation (skills, commands, agents, hooks)
    - Code quality tools status (clang-format, clang-tidy, Doxygen)
    - Build system health (CMake, vcpkg, binaries)
    - Source structure validation (MVP directories)
    - Git repository status
    - CI/CD status with GitHub CLI integration
    - Work scripts verification
    - Health score calculation (0-100%)
    - Issues/warnings/info categorization
    - Actionable recommendations

  - `.claude/hooks/` - 3 new functional quality hooks (31KB total)
    - `pre-commit-quality.sh` (7.6KB) - 8 automated quality checks
      - Code formatting verification
      - Build system consistency
      - Documentation consistency
      - Code annotations (TODO/FIXME tracking)
      - Sensitive data detection
      - Test file verification
      - File size checks
      - Commit preparation validation
    - `session-start.sh` (13KB) - Session initialization dashboard
      - Project information display
      - Git status summary
      - CI/CD latest run status
      - Build status verification
      - Claude Code resources enumeration
      - Project phase tracking
      - Documentation status
      - Quality tools availability
      - Quick actions guide
      - Session readiness score (0-100%)
    - `quality-reminder.sh` (11KB) - Quality practices reminder
      - 5-step quality workflow (before/during/after work)
      - Available tools reference
      - Slash commands documentation
      - Quick reference (architecture, threading, i18n, plugins)
      - Anti-patterns to avoid
      - Coding conventions
      - Resources & documentation links

- **CLAUDE.md Optimization** (Performance Warning Resolution)
  - **Problem**: CLAUDE.md exceeded 40k character warning threshold (50,727 bytes = 25% OVER)
  - **Impact**: Reduced performance (CLAUDE.md loaded automatically at every prompt)
  - **Solution**: Extracted detailed Integration documentation to separate file
  - **Results**:
    - CLAUDE.md size: 50,727 → 41,465 bytes (18.3% reduction, -9,262 bytes)
    - Line count: 1,346 → 1,054 lines (-292 lines)
    - Threshold status: 25% OVER → 3.6% over (significant improvement)
  - **Changes**:
    - Created `.claude/INTEGRATION.md` (15KB) - Complete integration guide
      - Session initialization process (6 automatic steps)
      - Skills documentation (3 Kalahari-specific with tables)
      - Slash Commands (6 commands with arguments and examples)
      - Agents (6 specialized with categories and experience)
      - Resource discovery & verification
      - Quality automation (health-check.sh, check-ci.sh, hooks)
      - Best practices for skills, commands, agents
      - Development workflow integration (5-step typical workflow)
      - Extending framework (add skill/command/agent)
      - Troubleshooting (4 common issues)
      - Performance optimization strategies
    - Replaced 334-line Integration section in CLAUDE.md with 42-line compact reference
    - CLAUDE.md now references `.claude/INTEGRATION.md` for detailed documentation
  - **Strategy**: Lightweight CLAUDE.md (essentials) + Linked Resources (details)

- **Code Quality Tools**
  - `.clang-format` - Code formatting configuration
    - Based on Google style with Kalahari customizations
    - IndentWidth: 4, ColumnLimit: 100, BreakBeforeBraces: Allman
    - Include sorting (kalahari/ → wx/ → third-party → std)
  - `.clang-tidy` - Static code analysis configuration
    - Comprehensive checks: bugprone, cert, clang-analyzer, concurrency, cppcoreguidelines, modernize, performance, readability
    - Member prefix: m_, Constant case: UPPER_CASE
  - `Doxyfile` - API documentation generator configuration
    - Project: Kalahari Writer's IDE v0.0.1-dev
    - Output: docs/api/html/
    - Source browser, class diagrams, call graphs enabled
    - Warnings for undocumented code
  - `cmake/PrecompiledHeaders.cmake` - PCH helper function
    - `kalahari_add_pch(target)` function for faster compilation
    - Precompiles: STL containers, spdlog, nlohmann_json

- **CMake Enhancements**
  - Sanitizers support (Debug builds, GCC/Clang)
    - `ENABLE_ASAN` - AddressSanitizer for memory errors
    - `ENABLE_UBSAN` - UndefinedBehaviorSanitizer for UB detection
  - Link-Time Optimization (Release builds)
    - Automatic IPO/LTO when supported (CMAKE_INTERPROCEDURAL_OPTIMIZATION)
  - clang-tidy integration
    - `ENABLE_CLANG_TIDY` option for static analysis during build
  - Custom CMake module path for cmake/ directory

- **Project Structure (MVP-Compliant)**
  - `src/` - Source code with MVP layer separation
    - `src/core/model/` - Model layer (business logic, pure C++)
    - `src/core/utils/` - Utility classes
    - `src/gui/views/` - View layer (wxWidgets UI)
    - `src/gui/widgets/` - Custom widgets
    - `src/presenters/` - Presenter layer (MVP coordination)
    - `src/services/` - Singleton services (PluginManager, EventBus)
  - `include/kalahari/` - Public headers (same structure as src/)
  - `tests/` - Test structure mirroring src/
    - `tests/core/` - Model layer tests
    - `tests/gui/` - View layer tests
    - `tests/presenters/` - Presenter tests
    - `tests/services/` - Service tests
  - `src/README.md` - Architecture guide (MVP pattern, conventions, namespace structure)
  - `tests/README.md` - Testing guide (Catch2 BDD, tagging, coverage targets, mocking)

- **Claude Code Skills (3 Kalahari-Specific)**
  - `.claude/skills/kalahari-wxwidgets/` (8.2KB)
    - wxWidgets 3.3.0+ expertise
    - wxAUI docking system setup
    - wxRichTextCtrl configuration
    - MVP pattern with wxWidgets
    - Dark mode support
    - Event handling patterns
    - Cross-platform UI guidelines
  - `.claude/skills/kalahari-plugin-system/` (14KB)
    - pybind11 C++/Python interop
    - Extension Points architecture
    - Event Bus implementation
    - .kplugin format specification
    - Python GIL handling
    - Thread safety patterns
    - Plugin lifecycle management
  - `.claude/skills/kalahari-i18n/` (12KB)
    - wxLocale setup for 6 languages (EN/PL/DE/RU/FR/ES)
    - gettext workflow (.po/.mo files)
    - Plural forms handling
    - Translation extraction (xgettext)
    - Context-aware translations
    - bwx_sdk pattern integration

- **CI/CD Improvements**
  - `.github/workflows/ci-windows.yml` - Fixed MinGW→MSVC conflict
    - Added `ilammy/msvc-dev-cmd@v1` for MSVC toolchain setup
    - Explicit `CC: cl` and `CXX: cl` for compiler selection
    - Explicit `-DVCPKG_TARGET_TRIPLET=x64-windows`
    - Timeout: 90 minutes (prevents runaway builds)
    - fail-fast: false (all jobs complete)
    - Improved cache key: `windows-msvc-vcpkg-{hash}-{build_type}`
  - `.github/workflows/ci-linux.yml` - Fixed OpenSSL timeout
    - Added `linux-libc-dev` dependency (required for OpenSSL)
    - Timeout: 90 minutes
    - fail-fast: false
    - Improved cache key: `linux-gcc-vcpkg-{hash}-{build_type}`
  - `.github/workflows/ci-macos.yml` - Consistency improvements
    - Timeout: 90 minutes
    - fail-fast: false
    - Improved cache key: `macos-clang-vcpkg-{hash}-{build_type}`

#### Changed
- **CLAUDE.md Optimization** (1459→1011 lines, -31%, -448 lines)
  - Removed duplicate "PROJECT STATUS UPDATE PROTOCOL" section (-113 lines)
  - Compacted Business Model section (full details → link to 05_business_model.md, -159 lines)
  - Compacted Roadmap section (full details → link to ROADMAP.md, -181 lines)
  - Preserved Update History (144 lines - valuable project evolution context)
  - Added comprehensive "Claude Code Integration" section (+334 lines)

- **.claude/ Directory Cleanup** (60→22 files, -63%, -19 directories)
  - **Removed:**
    - Puste katalogi: `agents/core/data/`, `agents/core/development/`, `agents/core/strategy/`
    - Nadmiarowa hierarchia: całe `agents/core/` (agenty przeniesione do płaskiej struktury)
    - Assets my_name_is_claude: `assets/` (3 pliki diagramów frameworkowych)
    - Frameworkowe templates: `templates/config/`, `templates/serena/`
    - Całe `prompts/` (31 plików, 5 subdirektorii) - zastąpione przez agents/ i skills/
    - Nieaktualne hooks: `hooks/README.md`, `hooks/pre-commit-validation.sh`, `hooks/user-prompt-submit-hook.sh`
  - **Przeniesione:**
    - `agents/core/*/*.md` → `agents/*.md` (płaska struktura, 6 agentów)
  - **Zachowane:**
    - `agents/` (6 agentów: deployment-engineer, qa-engineer, security-engineer, session-manager, software-architect, ux-designer)
    - `skills/` (3 skills Kalahari-specific)
    - `commands/` (6 slash commands)
    - `templates/` (2 szablony: agent_template.md, prompt_template.md)
    - `hooks/` (3 nowe funkcjonalne hooks)

- **.gitignore** - Added `docs/api/` (Doxygen output)

#### Fixed
- Windows CI/CD build failure (MinGW vs MSVC conflict) - switched to MSVC toolchain
- Linux CI/CD timeout - added `linux-libc-dev` dependency, increased timeout
- Hooks referencing deleted `prompts/` directory - replaced with new functional hooks

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
- **Week 2: wxWidgets GUI & Threading (2025-10-26)**
  - **Day 1-2: Basic GUI Window (Task #00001)**
    - Logger singleton (spdlog wrapper, thread-safe)
      - Dual sinks (console + file)
      - Platform-agnostic log paths (via wxStandardPaths)
      - Debug/Release build awareness
    - KalahariApp (wxApp subclass)
      - Application initialization with logging setup
      - Splash screen placeholder (Phase 1)
      - wxIMPLEMENT_APP macro in main.cpp
    - MainWindow (wxFrame subclass)
      - 4 menus: File, Edit, View, Help
      - 5 toolbar buttons (stock icons via wxArtProvider)
      - 3-pane status bar (status | position | time)
      - Placeholder main panel with centered text
      - Event table with 8 event handlers
      - Stub implementations (Phase 1 completion)
    - Internationalization structure
      - English translation template (locales/en/kalahari.pot)
      - Polish translations (locales/pl/kalahari.po)
      - All UI strings wrapped with _() macro
    - Build system updates
      - Updated src/CMakeLists.txt with new source files
      - WIN32_EXECUTABLE for Windows (no console)
      - MACOSX_BUNDLE for macOS
      - Links: wx::core, wx::base, spdlog, nlohmann_json, libzip
    - **Build Results:**
      - Local Linux: 166 MB Debug executable
      - CI/CD: macOS (1m9s), Windows (4m19s), Linux (4m36s) - ALL SUCCESS
      - Code size: 1,704 lines added (13 files)
  - **Day 3: Threading Infrastructure (Task #00002)**
    - Hybrid threading approach (std::thread + wxQueueEvent)
      - std::thread for thread creation (modern C++, Python GIL compatible)
      - wxQueueEvent for thread→GUI communication (Bartosz's proven pattern)
      - wxSemaphore for thread pool limiting (max 4 threads)
      - CallAfter for simple GUI updates (convenience)
    - Core threading components
      - submitBackgroundTask() API (68 lines)
        - Semaphore check (TryWait → work → Post pattern)
        - Thread tracking (std::vector<std::thread::id>)
        - Exception handling (try/catch → wxEVT_KALAHARI_TASK_FAILED)
        - wxQueueEvent for GUI communication (thread-safe, deep copy)
        - Logging at every step (debug/info/error levels)
      - Custom events (KALAHARI naming convention)
        - wxEVT_KALAHARI_TASK_COMPLETED
        - wxEVT_KALAHARI_TASK_FAILED
        - Handlers: onTaskCompleted(), onTaskFailed()
      - Thread safety mechanisms
        - wxMutex protects thread pool vector
        - wxSemaphore(4, 4) limits concurrent tasks
        - Detached threads (fire-and-forget, like wxTHREAD_DETACHED)
      - Destructor cleanup (20 lines)
        - Wait up to 5 seconds for tasks to finish
        - Timeout protection (prevents infinite hang)
        - Graceful vs forced shutdown logging
    - Example usage
      - onFileOpen() demonstrates pattern (32 lines)
      - 2-second simulated file load
      - Status bar updates during operation
      - Thread limit warning dialog
      - Tests thread limiting (click File→Open 5x rapidly)
    - **Build Results:**
      - Local Linux: 166 MB Debug (unchanged, incremental)
      - CI/CD: macOS (59s), Windows (4m16s), Linux (4m36s) - ALL SUCCESS
      - Code size: 241 lines added (2 files), +229 net
    - **Integration Points (Future):**
      - Week 3-4: Python plugin execution with GIL handling
      - Week 5+: File I/O, AI API calls, document parsing
      - Phase 1: Heavy operations (DOCX import, statistics generation)

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
