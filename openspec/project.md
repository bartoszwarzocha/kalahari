# Project Context

## Purpose

**Kalahari** - Complete **Writer's IDE** for book authors (novelists, non-fiction, journalists)

### Vision
Advanced writing environment combining:
- Professional text editor (rich text, Qt6)
- Project management (chapters, characters, locations)
- Research tools (sources, citations)
- AI assistant (8 African animal personalities)
- Analytics (progress tracking, pacing analysis)
- Export tools (DOCX, PDF, EPUB)
- Python plugin system (extensibility)

**Think:** Scrivener + VS Code + AI Assistant = Kalahari

### Philosophy
- **Writer-centric** - Minimal distractions, focus modes
- **Open & extensible** - MIT license core, Python plugins
- **Native performance** - C++ core, responsive UI
- **Cross-platform** - Windows, macOS, Linux (all in MVP)

---

## Tech Stack

### Core Technologies
- **Language:** C++20 (GCC 10+, Clang 10+, MSVC 2019+)
- **GUI:** Qt6 6.5.0+ (Widgets, QDockWidget, automatic DPI scaling)
  - **Migration:** From wxWidgets 3.3.0+ to Qt6 (2025-11-19, Clean Slate)
- **Build:** CMake 3.21+ with vcpkg (manifest mode)
  - CMAKE_AUTOMOC, CMAKE_AUTORCC, CMAKE_AUTOUIC enabled
- **Testing:** Catch2 v3 (BDD style, 70%+ coverage target)
- **Logging:** spdlog (fast, structured)
- **JSON:** nlohmann_json (project files, settings)
- **Compression:** libzip (for .klh project files)
- **Database:** SQLite3 (Phase 2+ for full-text search)

### Plugin System
- **Language:** Python 3.11 (embedded, bundled with app)
- **Binding:** pybind11 (C++ ↔ Python)
- **Format:** .kplugin (ZIP archives)
- **Extension Points:** IExporter, IPanelProvider, ICommandProvider, IAssistant

### Dependencies (vcpkg)
```json
{
  "dependencies": [
    "qtbase",
    "qttools",
    "sqlite3",
    "nlohmann-json",
    "fmt",
    "spdlog",
    "pybind11",
    "python3",
    "libzip",
    "pugixml",
    "curl",
    "openssl"
  ]
}
```

---

## Project Conventions

### Code Style

**File names:** snake_case
- Examples: `character_bank.cpp`, `settings_dialog.h`, `main_window.cpp`

**Class names:** PascalCase
- Examples: `CharacterCard`, `PluginManager`, `DocumentArchive`, `MainWindow`

**Functions:** camelCase
- Examples: `getTitle()`, `addChapter()`, `saveDocument()`, `onButtonClick()`

**Member variables:** m_prefix
- Examples: `m_title`, `m_chapters`, `m_isModified`, `m_navigatorDock`

**Constants:** UPPER_SNAKE_CASE
- Examples: `MAX_CHAPTERS`, `DEFAULT_FONT_SIZE`, `AUTOSAVE_INTERVAL_MS`

**Namespaces:** lowercase
- Examples: `kalahari::core`, `kalahari::gui`, `kalahari::plugin`

**Comments:**
- Doxygen style (`///`)
- English only (MANDATORY)
- All public APIs documented
- Examples:
```cpp
/// @file document.h
/// @brief Document class representing a book project

/// @brief Adds a new chapter to the document
/// @param title Chapter title
/// @return Pointer to created chapter, or nullptr on failure
Chapter* addChapter(const std::string& title);
```

**Headers:**
- Prefer `#pragma once` over header guards
- Include order: STL → Qt → third-party → project

**Modern C++20:**
- Smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- RAII everywhere
- No raw `new`/`delete`
- Concepts for template constraints
- Ranges for functional iteration

---

### Architecture Patterns

**GUI Pattern:** MVP (Model-View-Presenter) + Qt Signals/Slots
- Model: Pure C++, no Qt widgets, inherits QObject for signals
- View: Qt widgets (QWidget subclasses), minimal logic
- Presenter: Business logic, connects model↔view via signals/slots

**Error Handling:** Hybrid approach
- Exceptions for programming errors (assertions in debug)
- Error codes for expected failures (file I/O)
- spdlog for logging
- QMessageBox for user-facing errors

**Dependency Management:** Hybrid
- Singletons for infrastructure (Logger, SettingsManager, PluginManager)
- Dependency Injection for business logic (testable)

**Threading:**
- Dynamic thread pool (2-4 workers, CPU-aware)
- Qt signals/slots for cross-thread communication
- QMetaObject::invokeMethod for GUI thread marshalling

**Memory:**
- Lazy loading (Phase 1+: metadata eager, content on-demand)
- Smart pointers (RAII, automatic cleanup)

**Undo/Redo:**
- Command pattern (100 commands default, configurable)
- Merge consecutive edits (typing)

**Document Model:**
- Composite pattern: Book → Parts → Chapters
- Tree structure for flexible nesting

---

### Testing Strategy

**Framework:** Catch2 v3 (BDD style)

**Coverage Targets:**
- Core: 70%+ (critical path testing)
- Plugins: 50%+ (feature testing)
- GUI: Manual + Phase 1 QTest (integration)

**Test Organization:**
```
tests/
├── unit/               # Unit tests (core classes)
├── integration/        # Integration tests (plugins, file I/O)
└── fixtures/           # Test data (sample .klh files)
```

**Example:**
```cpp
#include <catch2/catch_test_macros.hpp>
#include "core/document.h"

TEST_CASE("Document chapter management", "[document]") {
    Document doc("Test Book");

    SECTION("Adding chapters") {
        auto* ch1 = doc.addChapter("Chapter 1");
        REQUIRE(ch1 != nullptr);
        CHECK(ch1->getTitle() == "Chapter 1");
        CHECK(doc.getChapterCount() == 1);
    }
}
```

---

### Git Workflow

**Branching Strategy:**
- `main` - Stable releases (protected)
- Feature branches - Short-lived, merged via PR
- Naming: `feature/`, `bugfix/`, `refactor/`

**Commit Conventions:**
- Format: `type(scope): description`
- Types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`
- Examples:
  - `feat(menu): Add hierarchical menu support`
  - `fix(settings): Resolve Windows crash on Apply`
  - `docs(readme): Update installation instructions`

**Commit Message Footer:**
```
🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude <noreply@anthropic.com>
```

**CI/CD:**
- GitHub Actions (matrix builds: Windows, macOS, Linux)
- Automated testing on all PRs
- Automated releases (tags → GitHub Releases)

---

## Domain Context

### Book Structure (PROJECT-first paradigm)
```
My Novel.klh (ZIP archive)
├── manifest.json           # Book metadata
├── book.json              # Structure
├── chapters/
│   ├── ch_001.rtf         # Chapter content
│   ├── ch_002.rtf
│   └── ...
├── resources/
│   ├── images/
│   └── sources/
└── settings.json
```

**Hierarchy:**
```
Book (.klh file)
├─ Structure (Front Matter, Body, Back Matter)
│  └─ Parts
│     └─ Chapters
├─ Objects (Characters, Locations, Events, Notes)
├─ Research (PDFs, web clips, annotations)
└─ Metadata (Author, Genre, Language, Statistics)
```

### Key Terminology
- **Book** - Top-level project (not "document" or "project")
- **Chapter** - Unit of writing (not "file" or "section")
- **Part** - Grouping of chapters (optional)
- **Navigator** - Project tree panel (chapters, characters, locations)
- **Assistant** - AI companion (8 African animals)
- **Panel** - Dockable UI element (QDockWidget)
- **Perspective** - Saved workspace layout

### African Naming Convention
All products named after African landmarks:
- **Kalahari** - Writer's IDE (this project)
- **Serengeti** - Collaborative writing (future)
- **Okavango** - Research & knowledge management (future)
- **Victoria** - Cloud sync service (future)
- **Zambezi** - Publishing toolkit (future)

---

## Important Constraints

### Technical
- **C++20 minimum** - No C++23 features (compiler support)
- **Qt6 6.5.0+** - LGPL dynamic linking (proprietary Python plugins OK)
- **Python 3.11** - Embedded, bundled (no system Python dependency)
- **vcpkg manifest** - Reproducible builds (vcpkg.json lockfile)

### Business
- **MIT license** - Core application (open source)
- **Proprietary plugins** - Premium features ($14-39 each)
- **No telemetry** - Privacy-first (local storage, offline-first)

### UX
- **Cross-platform parity** - Feature parity on Windows, macOS, Linux
- **Offline-first** - All core features work without internet
- **Distraction-free** - Focus modes, minimal UI chrome

### Quality
- **Zero crashes** - Graceful degradation, never lose data
- **Auto-save** - 5-minute default (configurable)
- **Undo stack** - 100 commands (configurable)

---

## External Dependencies

### C++ Libraries (vcpkg)
- Qt6 (qtbase, qttools) - GUI framework
- nlohmann_json - JSON parsing
- spdlog - Logging
- Catch2 - Testing
- libzip - Archive handling
- SQLite3 - Database (Phase 2+)
- pybind11 - Python bindings
- curl + OpenSSL - HTTP client

### Python Libraries (plugins)
- python-docx - DOCX import/export
- reportlab - PDF generation
- ebooklib - EPUB e-books
- markdown - Markdown parsing
- openai - OpenAI API (GPT)
- anthropic - Anthropic API (Claude)
- spacy - NLP toolkit
- matplotlib - Charts/graphs

### Development Tools
- CMake 3.21+ - Build system
- vcpkg - Package manager
- Git - Version control
- GitHub Actions - CI/CD
- Visual Studio 2019+ / CLion / VS Code - IDEs

---

## Additional Context

### Current Phase
**Phase 0:** Qt Foundation (COMPLETE as of 2025-11-21)
- Qt6 setup (QMainWindow, QDockWidget, Settings Dialog)
- Command Registry (15 commands registered)
- Plugin integration foundation (ICommandProvider, EventBus Qt6)

**Next Phase:** Phase 1 - Core Editor (Weeks 1-20)
- Rich text editing
- Project management (.klh files)
- Search functionality
- Complete menu system (200+ commands)

### Development Model
- **Atomic tasks** - 30-120 minutes each
- **User approval required** - Every plan must be approved
- **OpenSpec workflow** - Spec-driven development (starting Task #00017)
- **Serena MCP** - Session context & decision rationale
- **Git commits** - Atomic, one task = one commit

### Documentation
**Location:** `project_docs/` (11 documents, all complete)
1. Overview - Vision, target audience
2. Tech Stack - Detailed technology decisions
3. Architecture - MVP pattern, error handling, threading
4. Plugin System - Extension Points, Event Bus
5. Business Model - Open Core + Plugins + SaaS
6. Roadmap - 18-month plan (Phases 0-5)
7. MVP Tasks - Week-by-week breakdown
8. GUI Design - Command Registry, toolbars, panels
9. i18n - Qt i18n system (tr() + .ts/.qm)
10. Branding - Logo, colors, animal designs
11. User Documentation Plan - MkDocs strategy
12. Dev Protocols - Task workflow, session protocols

**Additional Files:**
- `ROADMAP.md` - Actual project roadmap (SINGLE SOURCE OF TRUTH)
- `CHANGELOG.md` - Version history (Keep a Changelog format)
- `CLAUDE.md` - AI assistant instructions
- `tasks/*.md` - Historical task files (migrating to OpenSpec)

---

## Notes for AI Assistants

### When Writing Code
1. **Always** read existing code before proposing changes
2. **Use** Serena MCP to explore code structure first
3. **Check** Context7 MCP for library documentation (Qt6, Python)
4. **Follow** C++ conventions strictly (m_prefix, camelCase, PascalCase)
5. **Document** all public APIs with Doxygen comments
6. **Test** manually or write Catch2 tests

### When Creating Specs (OpenSpec)
1. **Read** `@/openspec/AGENTS.md` for workflow
2. **Reference** existing specs in `openspec/specs/`
3. **Be explicit** in spec_delta.yaml (ADDED/MODIFIED/REMOVED)
4. **Include** implementation tasks
5. **Document** technical decisions

### When Stuck
1. **Ask** the user (if <90% certain)
2. **Check** project_docs/ for architectural guidance
3. **Review** ROADMAP.md for phase context
4. **Read** existing similar code
5. **Consult** CLAUDE.md for conventions

---

**Project Manager:** User (Bartosz)
**Main Executor:** Claude (AI)
**Work Model:** User leads, Claude executes and proposes
**Quality Standard:** "Opus magnum" - highest quality, no shortcuts

---

_Last Updated: 2025-11-22_
_OpenSpec Version: 0.16.0_
