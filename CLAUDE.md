# KALAHARI - Project

> **Writer's IDE** - Advanced writing environment for book authors

---

## 📋 Basic Information

**Project Name:** Kalahari
**Type:** Desktop Application
**Purpose:** Comprehensive support for the book writing process
**Target Audience:** Novel writers, non-fiction/historical authors, book journalists
**Status:** 🟡 Concept Phase
**Concept Version:** 2.2
**Start Date:** 2025-10-22

---

## 🎯 Main Project Goals

### Vision
Create a complete writing environment (Writer's IDE) that allows authors to focus on creativity, eliminating technical and organizational barriers associated with book writing.

### Key Distinguishing Features
1. **African Naming Convention** - entire tool ecosystem inspired by Africa
2. **Graphical Assistant** - talking heads of African animals with different personalities
3. **Comprehensiveness** - everything a writer needs in one place
4. **Multi-language Support** - full internationalization with focus on English and Polish

### Tool Ecosystem (Planned)
- **Kalahari** - main writing environment (this project)
- **Serengeti** - collaborative writing tool (future)
- **Okavango** - research & knowledge management (future)
- **Kilimanjaro** - project management (future)
- **Victoria** - cloud sync (future)
- **Zambezi** - publishing toolkit (future)

---

## 🛠️ Technology Stack

### Core Stack (CONFIRMED) ✅ FINALIZED

**Language & Build:**
- **Language:** C++20 (GCC 10+, Clang 10+, MSVC 2019+)
- **Build System:** CMake 3.21+
- **Package Manager:** vcpkg (manifest mode)
- **Testing Framework:** Catch2 v3 (BDD style)

**GUI & Core Libraries:**
- **GUI Framework:** wxWidgets 3.3.0+ (native cross-platform UI, dark mode support from 3.3.0)
- **Layout System:** wxAUI (Advanced User Interface - dockable panels)
- **Logging:** spdlog (fast, structured logging)
- **JSON:** nlohmann_json (modern C++ JSON)
- **Compression:** libzip (for .klh project files)
- **Database:** SQLite3 (metadata - Phase 2+)
- **Formats:** JSON (project structure), RTF (internal), Markdown

**Python Plugin System:**
- **Python:** 3.11 Embedded (bundled with application)
- **Binding:** pybind11 (seamless C++/Python interop)
- **Plugin Format:** .kplugin (ZIP container)
- **Plugin Runtime:** Embedded Python 3.11 + pip for dependencies

**Python Plugins (via embedded Python):**
- **Import/Export:** python-docx, reportlab, ebooklib, markdown
- **AI/NLP:** OpenAI/Anthropic API, spaCy, langchain
- **OCR:** pytesseract (Tesseract)
- **Language:** language-tool-python, hunspell
- **Visualizations:** matplotlib, networkx, plotly
- **Statistics:** numpy, pandas (for advanced analytics plugins)

---

## 🌍 Language & Platform Policy

### Code Language (MANDATORY)
**Everything in ENGLISH:**
- ✅ All source code
- ✅ All code comments
- ✅ All configuration files (CLAUDE.md, .serena/project.yml, etc.)
- ✅ Variable names, function names, class names
- ✅ Docstrings
- ✅ Commit messages
- ✅ Technical documentation

**Why English?**
- Industry standard for open source projects
- Wider contributor base potential
- Better collaboration opportunities
- Professional appearance
- Easier code sharing and reuse

### Communication Language
**User ↔ AI communication:**
- ✅ **English** (preferred)
- ✅ **Polish** (alternative)

Both languages are acceptable for discussing the project, asking questions, and planning.

### Application UI Languages

**Priority order:**
1. 🇬🇧 **English** - PRIMARY (must be complete at all times)
2. 🇵🇱 **Polish** - SECONDARY (MVP requirement)
3. 🇩🇪 **German** - Phase 2+
4. 🇷🇺 **Russian** - Phase 2+
5. 🇫🇷 **French** - Phase 2+
6. 🇪🇸 **Spanish** - Phase 2+

**Implementation:**
- Use wxWidgets i18n/l10n mechanisms (wxLocale)
- Standard gettext format (.po/.mo files)
- Language files in `locales/` directory
- User can switch language at runtime

**MVP Language Requirement:**
- EN + PL must be 100% complete before release
- Other languages can be added incrementally

### Platform Support ✅ FINALIZED

**All platforms from MVP** - parallel development with priority testing:

1. 🪟 **Windows** - PRIMARY DEVELOPMENT
   - Target: Windows 10/11 x64
   - Daily development and testing
   - Primary debugging platform
   - ~50-70% of writer audience

2. 🍎 **macOS** - PARALLEL SUPPORT
   - Target: macOS 11+ (Intel + Apple Silicon)
   - Smoke testing every 2-3 days
   - CI/CD automated builds
   - ~20-30% of writer audience

3. 🐧 **Linux** - PARALLEL SUPPORT
   - Target: Ubuntu 22.04+, Fedora 38+
   - Weekly smoke testing
   - CI/CD automated builds
   - ~5-10% of writer audience

**Cross-platform Strategy:**
- wxWidgets provides native look & feel on all platforms
- GitHub Actions matrix builds (Windows + macOS + Linux) on every commit
- 99% code sharing across platforms (C++ + wxWidgets)
- Platform-specific code only for: installers, file associations, native integrations
- **All three platforms in MVP 1.0** - no delayed releases

---

## 💼 Business Model

**Strategy:** Open Core + SaaS Hybrid ✅ FINALIZED

**Core (Open Source):**
- License: MIT License (GitHub public)
- Free features: Text editor, project management, basic stats, DOCX/PDF export, auto-save

**Premium (Closed Source Plugins):**
- 5 paid plugins: AI Assistant Pro, Advanced Analytics, Export Suite, Research Pro, Collaboration Pack
- Pricing: $14-39 each, $79 bundle (40% savings)
- Distribution: .kplugin files, GitHub (MVP) → Own platform (Post-1.0)

**Cloud (SaaS Subscription):**
- Cloud Sync Pro: $5-10/month
- Features: Multi-device sync, cloud backups, E2E encryption, premium support

**Target Conversion:** 80-85% free, 10-15% premium plugins, 3-5% cloud subscription

**📄 Full Details:** See [project_docs/05_business_model.md](project_docs/05_business_model.md)

---

## 🦁 Graphical Assistant - Concept ✅ CONFIRMED

### Main Idea
**ONE assistant to choose** - user selects ONE animal that becomes their personal writing assistant.

### Implementation ✅ CONFIRMED
- **Status:** IN MVP (Phase 1)
- **Style:** Realistic (photorealistic animal renderings)
- **Format:** Static images (6-8 moods per animal)
- **UI:** Dockable panel (bottom-right), speech bubbles
- **Animals in MVP:** 4 (Lion, Meerkat, Elephant, Cheetah)
- **Phase 2:** Add 4 more animals (Giraffe, Buffalo, Parrot, Chameleon)

### Common Functions (All Animals)
- Monitor writer's progress
- Praise achievements
- Remind about breaks and health (20-20-20 rule)
- Motivate and encourage
- Writing tips
- Detect inconsistencies
- Flow state detection (don't interrupt)

### Difference = COMMUNICATION STYLE
Each animal has its own **personality** and **speaking style**.

### 4 MVP Assistants

| Animal | Personality | Style | Default |
|---------|-----------|------|---------|
| **Lion** | Majestic, authoritative | Strong, demanding, mentor | ✅ DEFAULT |
| **Meerkat** | Friendly, alert, helpful | Warm, enthusiastic, exclamations | |
| **Elephant** | Wise, calm, patient | Reflective, full of wisdom | |
| **Cheetah** | Fast, energetic, focused | Short, dynamic, tempo | |

**Why Lion is default:**
- Symbol of Kalahari brand (logo)
- Represents authority and wisdom
- Consistent branding (Lion = storyteller)

### Phase 2 Assistants (Future)

| Animal | Personality | Style |
|---------|-----------|------|
| **Giraffe** | Gentle, sees "big picture" | Perspective, long-term |
| **Buffalo** | Persistent, consistent, strong | Simple, direct, willpower |
| **Parrot** | Talkative, linguistic | Rich vocabulary, synonyms |
| **Chameleon** | Flexible, adaptive | Changes tone by context |

---

## 📁 Project Structure

```
/mnt/e/Python/Projekty/Kalahari/
├── CLAUDE.md                           # This file - Master project document
│
├── project_docs/                       # 📚 CURRENT DOCUMENTATION (organized)
│   ├── README.md                       # Documentation index
│   ├── 01_overview.md                  # ✅ Project vision & goals
│   ├── 02_tech_stack.md                # ✅ C++20, wxWidgets, vcpkg
│   ├── 03_architecture.md              # ✅ System design (MVP, patterns, threading)
│   ├── 04_plugin_system.md             # ✅ Plugin API (Extension Points, Event System)
│   ├── 05_business_model.md            # ✅ Open Core + Plugins
│   ├── 06_roadmap.md                   # ✅ Phases 0-5, 18 months
│   ├── 07_mvp_tasks.md                 # ✅ Detailed tasks (week-by-week Phase 0-1)
│   ├── 08_gui_design.md                # ✅ GUI architecture (Command Registry, toolbars)
│   ├── 09_i18n.md                      # ✅ i18n/l10n (wxLocale + gettext)
│   ├── 10_branding.md                  # ✅ Logo, colors, animals
│   ├── 11_user_documentation_plan.md   # ✅ User docs strategy (MkDocs)
│   └── diagrams/                       # SVG diagrams (to be created)
│
├── concept_files/                      # 📦 Concept documents and design materials
│   ├── Kalahari - koncepcja.txt       # Original concept (PL, archived)
│   ├── Kalahari-koncepcja-v2.md       # Python-based concept (archived, outdated)
│   ├── Asystenci - analiza.txt        # Assistant analysis (PL, archived)
│   ├── MVP_breakdown.md               # Python timeline (archived, outdated)
│   ├── GUI_specification.md           # Python GUI spec (archived, outdated)
│   ├── koncepcja_GUI.jpg              # Hand-drawn layout sketch
│   └── wxFormBuilder/                 # GUI mockups and layouts
│
└── [code structure - to be created in Phase 0]
```

---

## 📖 Key Documents

### Current Documentation (project_docs/)

**Primary Reference:** `/mnt/e/Python/Projekty/Kalahari/project_docs/README.md`
- Complete documentation index
- Links to all 10 core documents
- Status indicators (complete, pending, waiting for input)

**All Core Documents (11/11 Complete):**
- **01_overview.md** - Project vision, goals, target audience
- **02_tech_stack.md** - Complete technical stack (C++20, wxWidgets, Python)
- **03_architecture.md** - System design (MVP pattern, error handling, threading)
- **04_plugin_system.md** - Plugin API spec (Extension Points, Event System)
- **05_business_model.md** - Open Core + Plugin Marketplace + SaaS
- **06_roadmap.md** - Phases 0-5, 18-month timeline
- **07_mvp_tasks.md** - Week-by-week tasks (Phase 0-1 detailed breakdown)
- **08_gui_design.md** - GUI implementation (Command Registry, customizable toolbars)
- **09_i18n.md** - Internationalization (wxLocale + gettext, bwx_sdk pattern)
- **10_branding.md** - Logo, colors, animal designs
- **11_user_documentation_plan.md** - User docs strategy (MkDocs + Material)

### Historical Documents (concept_files/ - ARCHIVED & ONGOING)

These documents include:
- **Archived:** Python-based concepts (outdated, superseded by project_docs/)
  - `Kalahari-koncepcja-v2.md` - Original concept (replaced by project_docs/)
  - `MVP_breakdown.md` - Python timeline (replaced by 06_roadmap.md)
  - `GUI_specification.md` - Python GUI spec (replaced by 08_gui_design.md)
  - `Asystenci - analiza.txt` - Assistant analysis (still relevant)
- **Ongoing:** New concept materials (GUI mockups, design sketches, brainstorming)

---

## ✅ What is DECIDED ✅ FINALIZED (2025-10-24)

### Project Fundamentals
- ✅ Project name: **Kalahari**
- ✅ Type: **Writer's IDE** (opus magnum project)
- ✅ Convention: **African** (names, animals, ecosystem)
- ✅ Target audience: **Book writers** (novelists, non-fiction authors)
- ✅ Application type: **Desktop** (native, cross-platform)
- ✅ License: **MIT License** (core) + **Trademark** ("Kalahari" name protected)

### Technology Stack
- ✅ Language: **C++20** (GCC 10+, Clang 10+, MSVC 2019+)
- ✅ GUI: **wxWidgets 3.3.0+** (native look & feel, dark mode support)
- ✅ Layout: **wxAUI** (dockable panels)
- ✅ Build: **CMake 3.21+** (only CMake, no alternatives)
- ✅ Package Manager: **vcpkg** (manifest mode)
- ✅ Testing: **Catch2 v3** (BDD style)
- ✅ Logging: **spdlog**
- ✅ JSON: **nlohmann_json**
- ✅ Compression: **libzip** (for .klh files)

### Plugin Architecture
- ✅ Plugins: **From day zero** (core feature, not retrofit)
- ✅ Python: **3.11 Embedded** (bundled with app, zero friction)
- ✅ Binding: **pybind11** (C++/Python interop)
- ✅ Format: **Mixed** - folders (dev) + .kplugin (distribution)
- ✅ API Versioning: **Semantic Versioning** (MAJOR.MINOR.PATCH)
- ✅ Event System: **Async + thread marshalling** (thread-safe, GUI-aware)
- ✅ Marketplace: **Hybrid** - GitHub (MVP) → Own platform (Post-1.0)

### Languages & Platforms
- ✅ Code language: **English** (all code, comments, config files - MANDATORY)
- ✅ UI languages: **i18n from start** (EN primary, PL secondary in MVP)
- ✅ i18n system: **wxWidgets wxLocale + gettext** (.po/.mo files)
- ✅ Platforms: **All three from MVP** (Windows, macOS, Linux)
- ✅ Primary dev: **Windows** (daily testing)
- ✅ Secondary: **macOS + Linux** (CI/CD auto-build, periodic testing)
- ✅ CI/CD: **GitHub Actions** (matrix builds on every commit)

### Business Model
- ✅ Strategy: **Open Core + Plugin Marketplace + SaaS**
- ✅ Core: **MIT** (open source, GitHub public)
- ✅ Premium Plugins: **5 paid plugins** ($14-39 each, $79 bundle)
  - AI Assistant Pro, Advanced Analytics, Export Suite, Research Pro, Collaboration Pack
- ✅ Cloud: **Subscription** ($5-10/month) - Cloud Sync Pro
- ✅ Conversion target: 80% free, 10-15% plugins, 3-5% subscription

### MVP Features (4 Free Plugins)
- ✅ Plugin 1: **DOCX Exporter** (python-docx)
- ✅ Plugin 2: **Markdown Tools** (import/export)
- ✅ Plugin 3: **Basic Statistics** (word count, reading time)
- ✅ Plugin 4: **Assistant Lion** (1 animal - Lion, basic messages)

### Graphical Assistant
- ✅ Concept: **8 animals total, 4 in MVP** (Lion, Meerkat, Elephant, Cheetah)
- ✅ Default: **Lion** (brand symbol, storyteller archetype)
- ✅ Style: **Realistic** (photorealistic renderings)
- ✅ Format: **Static images** (6-8 moods per animal)
- ✅ UI: **Dockable panel** (bottom-right, speech bubbles)
- ✅ Implementation: **Python plugin** (flexibility, easy iterations)
- ✅ Premium: **All 8 animals + custom** in AI Assistant Pro plugin

### GUI Layout
- ✅ Framework: **wxWidgets + wxAUI** (dockable panels)
- ✅ Panels: **6+ dockable panels** (Project Navigator, Editor, Preview, Assistant, Stats, Output)
- ✅ Modes: **3 focus modes** (Normal, Focused, Distraction-free)
- ✅ Menu bar: **Standard** ([File] [Edit] [View] [Project] [Tools] [Help])
- ✅ Toolbar: **Customizable** (icons + labels, can be hidden)
- ✅ Status bar: **Live stats** (words, chars, progress, cursor position, session time)
- ✅ Perspectives: **Saveable layouts** (Default, Writer, Editor, Research)
- ✅ Themes: **4 built-in** (Light, Dark, Savanna, Midnight) - extensible

### Logo & Branding
- ✅ App icon: **Solo Lion** (with book & quill - storyteller)
- ✅ Marketing logo: **Multi-animal** (all 8 animals in composition)
- ✅ Splashscreen: **Dynamic** (random animal each launch)
- ✅ Style: **Realistic** (photorealistic animals, professional)

### MVP Timeline & Phases
- ✅ Total: **14-20 months** (realistically 18 months)
- ✅ Phase 0: **Foundation** (2-3mo) - CMake, vcpkg, plugin manager, Python embedding
- ✅ Phase 1: **Core Editor** (3-4mo) - Rich text, project management, save/load
- ✅ Phase 2: **Plugin System MVP** (2-3mo) - 4 basic plugins working
- ✅ Phase 3: **Feature Plugins** (3-4mo) - AI Assistant, statistics, advanced exports
- ✅ Phase 4: **Advanced Plugins** (2-3mo) - OCR, timeline, graphs
- ✅ Phase 5: **Polish & Release** (2-3mo) - Testing, documentation, installers

### Data & Persistence
- ✅ MVP: **JSON** (project structure, settings, metadata)
- ✅ Phase 2+: **SQLite3** (metadata, search index)
- ✅ File format: **.klh** (ZIP container with JSON + RTF)
- ✅ Internal format: **RTF** (rich text) + **Markdown** (drafts)

### Integrations
- ✅ Git: **Plugin** (later, not core feature)
- ✅ Cloud: **Integration** (Dropbox/GDrive in MVP, own backend in 2.0)
- ✅ AI: **API-based** (Claude/OpenAI, user provides key)
- ✅ Phase 2+: **Local models** (Ollama, Llama for offline work)

### Architectural Patterns ✅ FINALIZED (2025-10-25)
- ✅ **GUI Pattern:** MVP (Model-View-Presenter)
  - Clear separation: Model (data) ↔ Presenter (logic) ↔ View (GUI)
  - Testable business logic (Presenters can be unit tested)
  - wxWidgets views are thin wrappers
- ✅ **Error Handling:** Hybrid approach
  - **Exceptions** for programmer errors (invalid_argument, logic_error)
  - **Error codes** (std::optional, std::expected) for expected failures
  - **wxLog*** for user-facing messages
- ✅ **Dependency Management:** Hybrid approach
  - **Singletons** for infrastructure (EventBus, CommandRegistry, PluginManager)
  - **Dependency Injection** for business logic (Models, Presenters)
  - Facilitates testing while keeping global services accessible
- ✅ **Threading:** Dynamic thread pool
  - 2-4 worker threads (CPU-aware, adjustable ±2)
  - Python GIL handling (py::gil_scoped_acquire/release)
  - GUI marshalling (wxTheApp->CallAfter for UI updates)
- ✅ **Memory Management:** Lazy loading (from Phase 1)
  - Metadata eager (titles, structure, stats)
  - Content on-demand (chapter text loaded when opened)
  - Smart pointers everywhere (std::unique_ptr, std::shared_ptr)
- ✅ **Undo/Redo:** Command pattern
  - 100 commands default (configurable)
  - Mergeable consecutive edits
  - Full implementation with ICommand interface
- ✅ **Document Model:** Composite pattern
  - Book → Parts → Chapters (flexible nested structure)
  - Tree operations (add, remove, move, reorder)

---

## ❓ What is NOT YET DECIDED

- ⏳ **Coding start date:** When do we start implementation? (Documentation phase complete, awaiting user decision)
- ⏳ **Plugin development workflow:** Local plugin development tools? Hot reload?
- ⏳ **Testing coverage target:** 70%? 80%? Which parts must have tests?
- ⏳ **Installer specifics:** NSIS configuration for Windows, DMG for macOS, AppImage vs Snap for Linux?
- ⏳ **CI/CD details:** Exact GitHub Actions workflow? Caching strategy? Release automation?
- ⏳ **Documentation platform:** Sphinx? MkDocs? Docusaurus? For API docs and user manual?
- ⏳ **Plugin signing:** Code signing for plugins? Certificate requirements?
- ⏳ **Analytics:** Telemetry? Usage statistics? Privacy policy implications?

---

## 🎨 Branding

### Color Palette (Preliminary)
- Sandy beige: `#E6D5B8`
- Warm orange: `#D97642`
- Sunset red: `#8B3A3A`
- Savanna green: `#6B8E23`
- Sky blue: `#87CEEB`

### Graphic Styles (To Choose)
- Minimalist geometric (modern, clean)
- Hand-drawn (warm, friendly)
- Realistic (professional)

---

## 🚀 Roadmap ✅ FINALIZED (C++ Architecture)

**Timeline:** 14-20 months (realistically 18 months)
**Target:** Kalahari 1.0 (Q2-Q3 2026)
**Current Status:** 🔄 Phase 0 - Foundation (Week 1 complete)

**6 Development Phases:**
1. **Phase 0: Foundation** (Weeks 1-8) - CMake, vcpkg, plugin system, Python embedding
2. **Phase 1: Core Editor** (Weeks 9-20) - Rich text, wxAUI, project management, .klh files
3. **Phase 2: Plugin System MVP** (Weeks 21-30) - 4 working plugins (DOCX, Markdown, Stats, Lion)
4. **Phase 3: Feature Plugins** (Weeks 31-44) - Premium plugins (AI Assistant Pro, Analytics)
5. **Phase 4: Advanced Plugins** (Weeks 45-56) - Export Suite, Research Pro, Character/Location banks
6. **Phase 5: Polish & Release** (Weeks 57-68) - Testing, docs, packaging, public release 🎉

**Post-1.0 Expansion:**
- **Phase 6:** Cloud Sync Pro subscription
- **Phase 7:** Collaboration Pack plugin
- **Phase 8:** Plugin marketplace, ecosystem (Serengeti, Okavango, Victoria, Zambezi)

**📄 Full Details:** See [ROADMAP.md](ROADMAP.md) and [project_docs/06_roadmap.md](project_docs/06_roadmap.md)

---

## 🤖 Claude Code Resources

Kalahari uses **Claude Code's automatic resource discovery** for instant access to project knowledge and quality tools.

### Available Resources

- **3 Skills**: `kalahari-wxwidgets`, `kalahari-plugin-system`, `kalahari-i18n`
- **6 Commands**: `/code-review`, `/architecture-review`, `/best-practices`, `/dependency-check`, `/health-check`, `/testing-strategy`
- **6 Agents**: `software-architect`, `ux-designer`, `qa-engineer`, `security-engineer`, `deployment-engineer`, `session-manager`

### Quick Start

```bash
# AI-driven project health analysis
/health-check

# Automated file/tool checks
./tools/project-status.sh

# Pre-commit quality verification (35+ automated checks)
./tools/pre-commit-check.sh

# CI/CD monitoring
./tools/check-ci.sh status

# View release checklist
cat .claude/QUALITY_CHECKLIST.md
```

### Resources

- **Skills** auto-activate based on context (wxWidgets code → kalahari-wxwidgets)
- **Slash Commands** trigger AI workflows (`/code-review --scope=src/`)
- **Agents** activate implicitly by keywords or explicitly via slash commands
- **Quality Tools** in `tools/` provide automated verification
  - `project-status.sh`: Project health dashboard (files, tools, CI/CD)
  - `pre-commit-check.sh`: 35+ automated quality checks (70%/90%/100% gates)
  - `check-ci.sh`: GitHub Actions monitoring without browser

---

## 🤖 Instructions for AI (Claude Code)

### General Rules
1. **Ask about decisions** - project is in concept phase, don't assume if unsure
2. **Use African convention** - names, examples, comments
3. **Remember the 8 animal assistants** - when designing UI/functions
4. **English as code language** - all code, comments, config files in English
5. **Communication languages** - English or Polish for communication with user
6. **Suggest, don't decide** - user leads, AI executes
7. **Ask when uncertainty ≥10%** - If less than 90% certain about user's intentions, ALWAYS ask for clarification before proceeding
8. **Quality over size** - Prioritize content quality and correctness over file size or token count. A high-quality document is better than incomplete one.

### Using Tools

#### Context7 (External Library Documentation)
**MANDATORY use of Context7 in the following situations:**

✅ **ALWAYS use when:**
- Generating code using external libraries (wxPython, python-docx, reportlab, etc.)
- Proposing code with external service APIs
- Creating library installation instructions
- Writing configuration examples
- Checking current syntax/API
- Looking for function usage examples

❌ **NEVER:**
- Guess API syntax
- Rely solely on internal knowledge (may be outdated)
- Propose code without checking documentation

📝 **Process (ALWAYS in this order):**
1. `resolve-library-id` (e.g. "wxPython") → get Context7 ID
2. `get-library-docs` with received ID → get current documentation
3. Only then generate code based on current documentation

**Example:**
```
User: "Create a basic wxPython window"
→ First: resolve-library-id("wxPython")
→ Then: get-library-docs(received_id, topic="basic window")
→ Only then: generate code
```

### When Working on Code
- **C++ conventions:** Modern C++20, STL, smart pointers
- **File names:** snake_case (e.g. `character_bank.cpp`, `character_bank.h`)
- **Class names:** PascalCase (e.g. `CharacterCard`)
- **Function/method names:** camelCase (e.g. `getTitle()`, `addChapter()`)
- **Member variables:** m_prefix (e.g. `m_title`, `m_chapters`)
- **Constants:** UPPER_SNAKE_CASE (e.g. `MAX_CHAPTERS`)
- **Namespaces:** lowercase (e.g. `namespace kalahari { namespace core { } }`)
- **Comments:** Doxygen style (`///`), in English, detailed
- **Headers:** Header guards or `#pragma once`
- **TODO:** Always ask about priority before implementation

**Example:**
```cpp
/// @file character_card.h
/// @brief Character card data structure

#pragma once

#include <string>
#include <vector>

namespace kalahari {
namespace core {

/// Character card representing a book character
class CharacterCard {
public:
    CharacterCard(const std::string& name);

    /// Get character name
    const std::string& getName() const { return m_name; }

    /// Set character name
    void setName(const std::string& name) { m_name = name; }

private:
    std::string m_name;
    std::vector<std::string> m_traits;
};

} // namespace core
} // namespace kalahari
```

### When Proposing Features
- Check if it doesn't conflict with concept document
- Propose in context of roadmap (which phase?)
- Consider UX impact
- Think through technical consequences

### When Updating Documentation
- **Always ask** before changing CLAUDE.md
- Propose changes, don't apply automatically
- Log changes in "Update History" section (see below)

---

## 📋 PROJECT STATUS UPDATE PROTOCOL

### When to Update Strategic Files

**After EVERY work session that involves:**
- ✅ Making architectural decisions
- ✅ Completing documentation
- ✅ Finishing phase/milestone
- ✅ Adding/removing features
- ✅ Changing project structure
- ✅ Major refactoring

### End-of-Session Checklist

**MANDATORY steps before closing ANY work session:**

#### 1. CHANGELOG.md Update
- [ ] Add all changes to `[Unreleased]` section
- [ ] Use correct categories (Added/Changed/Decided/Removed/Fixed)
- [ ] Date the entry (YYYY-MM-DD)
- [ ] Be specific (not "updated docs", but "completed 03_architecture.md with MVP pattern")

#### 2. ROADMAP.md Update (if applicable)
- [ ] Add architectural decisions to relevant phase descriptions
- [ ] Update timeline if estimates changed
- [ ] Mark completed milestones
- [ ] Add new risks/dependencies if identified

#### 3. CLAUDE.md Update
- [ ] Update TODO section (mark DONE, add new tasks)
- [ ] Add entry to "Update History" with version bump
- [ ] Update "What is DECIDED" if new decisions made
- [ ] Update "What is NOT YET DECIDED" (remove decided items)

#### 4. project_docs/README.md Update
- [ ] Update document statuses (✅ Complete / 🔄 In Progress / ⏳ Pending)
- [ ] Update "Document History" section
- [ ] Verify all links work

#### 5. Final Verification
- [ ] All TODO items from session marked as DONE or moved to next session
- [ ] No temporary files left (.tmp, _backup, _FULL, etc.)
- [ ] All created files added to git tracking
- [ ] Consistency check: CLAUDE.md ↔ CHANGELOG.md ↔ ROADMAP.md

### Version Bumping Rules

**CLAUDE.md versions:**
- **MAJOR (X.0):** Complete stack/architecture change (e.g., Python → C++)
- **MINOR (X.Y):** New major decisions, completed phases, significant updates
- **NO PATCH:** CLAUDE.md uses only MAJOR.MINOR

**When to bump:**
- Added 3+ decisions to "What is DECIDED" → MINOR bump
- Completed full phase (Phase 0, 1, etc.) → MINOR bump
- Major architectural change → MAJOR bump
- Daily work updates → Update History entry, no version bump

### AI Assistant Rules

**Claude Code MUST:**
1. **Never close session** without running End-of-Session Checklist
2. **Always propose updates** to CLAUDE.md (never edit automatically)
3. **Ask user for confirmation** before version bumps
4. **Report what was updated** at end of session:
   ```
   📝 Session Summary:
   - ✅ CHANGELOG.md updated (3 changes)
   - ✅ ROADMAP.md updated (Phase 0 details)
   - ✅ CLAUDE.md updated (v4.2, 7 decisions)
   - ✅ project_docs/README.md current
   ```

**Claude Code SHOULD:**
- Proactively remind user if session is ending without updates
- Suggest checklist items that might apply
- Warn if inconsistencies detected between strategic files

### Example Workflow

**User:** "Completed 03_architecture.md with MVP pattern and error handling"

**Claude's End-of-Session Process:**
1. ✅ Update CHANGELOG.md → Added: "03_architecture.md (MVP pattern, error handling)"
2. ✅ Update ROADMAP.md → Add architectural decisions to Phase 0 description
3. ✅ Update CLAUDE.md → Mark TODO item DONE, add v4.2 in Update History
4. ✅ Update project_docs/README.md → Mark 03_architecture.md as ✅ Complete
5. ✅ Report summary to user
6. ✅ Ask: "Should I create a git commit for documentation updates?"

---

## 🚨 CRITICAL RULE: No Session Ends Without Checklist

**If Claude Code is about to end session, it MUST:**
1. Run End-of-Session Checklist
2. Report what needs updating
3. Get user approval
4. Execute updates
5. Report summary

**User can skip checklist ONLY if explicitly says:**
- "Skip checklist this time"
- "WIP session, don't update"
- "Temporary work, no updates needed"

---

## 📝 CLAUDE.md Update History

### v4.2 - 2025-10-25 (ARCHITECTURAL DECISIONS COMPLETE)
- 🏗️ **All 7 core architectural decisions finalized**
  - GUI Pattern: MVP (Model-View-Presenter)
  - Error Handling: Hybrid (exceptions + error codes + wxLog*)
  - Dependency Management: Hybrid (Singletons infrastructure + DI business logic)
  - Threading: Dynamic pool (2-4 workers, CPU-aware)
  - Memory: Lazy loading from Phase 1 (metadata eager, content on-demand)
  - Undo/Redo: Command pattern (100 commands default, configurable)
  - Document Model: Composite pattern (Book → Parts → Chapters)
- 📚 **3 major documents completed**
  - 03_architecture.md (547 lines) - Complete architectural design with C++ examples
  - 04_plugin_system.md (495 lines) - Plugin API, Extension Points, Event System
  - 07_mvp_tasks.md (405 lines) - Week-by-week breakdown for Phase 0-1
- 📋 **PROJECT STATUS UPDATE PROTOCOL added**
  - End-of-Session Checklist (5-step mandatory process)
  - Version bumping rules for CLAUDE.md
  - AI Assistant rules for session management
  - Critical rule: No session ends without checklist
- 🧹 **.claude/ directory cleanup**
  - Removed 24 files (32% reduction)
  - 51 relevant files remaining (all C++ desktop app focused)
- ✅ **"What is DECIDED" updated**
  - Added all 7 architectural decisions
  - Removed from "What is NOT YET DECIDED"
- 📝 **Documentation status**
  - 11/11 documents complete (100%)
  - Project 100% ready for Phase 0

### v4.1 - 2025-10-25 (PROJECT ORGANIZATION)
- 📊 **Versioning system established**
  - Created ROADMAP.md - 18-month development plan (Phases 0-5)
  - Created CHANGELOG.md - Version history (Keep a Changelog format)
  - Added versioning rules section in CLAUDE.md
  - Semantic Versioning 2.0.0 adopted
  - Pre-development: DOCS-X.Y versions
  - Development: 0.MINOR.PATCH-alpha/beta/rc
- 📁 **Project structure cleanup**
  - Renamed init_concept/ → concept_files/ (better naming)
  - Created work_scripts/ - Temporary utility scripts (git-ignored)
  - Updated .gitignore - C++/wxWidgets/Python specific patterns
  - Updated all references across project files
- 📋 **Documentation updates**
  - Updated README.md references (init_concept → concept_files)
  - Updated .claude/ directory references
  - Synced historical concept documents

### v4.0 - 2025-10-24 (MAJOR UPDATE - C++ ARCHITECTURE)
- 🏗️ **Complete stack rewrite: Python → C++20**
  - Language: Python 3.11+ → C++20
  - GUI: wxPython → wxWidgets 3.2+
  - Build: (none) → CMake 3.21+
  - Package Manager: Poetry → vcpkg
  - Testing: (none) → Catch2 v3
  - Python: main language → embedded 3.11 (plugins only)
- 🔌 **Plugin architecture from day zero**
  - pybind11 for C++/Python interop
  - .kplugin format (ZIP containers)
  - Extension Points system
  - Event Bus (async, thread-safe)
  - API Versioning (semantic)
- 💼 **Finalized business model**
  - 5 premium plugins ($14-39 each, $79 bundle)
  - Cloud Sync Pro subscription ($5-10/month)
  - Plugin marketplace: GitHub (MVP) → Own platform (Post-1.0)
- 🚀 **Timeline updated: 5-6 months → 18 months**
  - Phase 0: Foundation (2-3mo)
  - Phase 1: Core Editor (3-4mo)
  - Phase 2: Plugin System MVP (2-3mo)
  - Phase 3: Feature Plugins (3-4mo)
  - Phase 4: Advanced Plugins (2-3mo)
  - Phase 5: Polish & Release (2-3mo)
- 🌍 **All platforms from MVP**
  - Windows, macOS, Linux (parallel development)
  - CI/CD matrix builds on every commit
- ✅ **"What is DECIDED" - complete restructure**
  - 10 major categories
  - 50+ finalized decisions
  - C++ conventions, plugin architecture, business model, timeline
- 📚 **Updated AI instructions**
  - C++ coding conventions (Doxygen comments, modern C++20)
  - Plugin development guidelines
  - Context7 usage for wxWidgets documentation

### v3.0 - 2025-10-24 (MAJOR UPDATE)
- 🦁 **Assistant section completely updated**
  - Confirmed: IN MVP (not Phase 2)
  - Default: Lion (brand symbol)
  - Style: Realistic (photorealistic)
  - MVP: 4 animals (Lion, Meerkat, Elephant, Cheetah)
  - Phase 2: 4 more animals
- ✅ **"What is DECIDED" - massive expansion**
  - Added: MVP scope (5-6mo, Alpha→Beta→1.0)
  - Added: GUI layout (6+ dockable panels, 3 modes)
  - Added: Logo system (solo Lion for app, multi-animal for marketing)
  - Added: AI integration (Claude API for MVP)
  - Reorganized into categories (Project Basics, Languages, Business, Assistant, GUI, Logo, MVP, AI)
- ⏳ **"What is NOT YET DECIDED" - cleaned up**
  - Removed: MVP scope (now decided)
  - Removed: AI model (now decided)
  - Updated: Focus on remaining open questions
- 🚀 **Roadmap completely rewritten**
  - Phase 1: Detailed breakdown (Alpha → Beta → 1.0)
  - Assistant moved to MVP Beta phase
  - Phase 2: Split into Core + Premium
  - Phases 3-4: Updated with ecosystem details

### v2.1 - 2025-10-23
- 💼 **New section: "Business Model"**
- 📊 Confirmed strategy: Open Core + SaaS Hybrid
- 📜 Core license: MIT License
- 💎 Premium features: Closed source plugins
- ☁️ Cloud services: SaaS subscription model
- 🎯 Development phases outlined (4 phases)
- 💰 Revenue model and success metrics defined
- ✅ Updated "What is DECIDED" with business model
- ⏳ Updated "What is NOT YET DECIDED" (removed business model, added pricing details)

### v2.0 - 2025-10-23 (MAJOR UPDATE)
- 🌍 **Complete translation to English**
- 📝 All sections now in English (code, docs, config)
- 🌐 New section: "Language & Platform Policy"
- ✅ Confirmed decisions:
  - Code language: English (mandatory)
  - UI languages: EN (primary), PL (secondary), +4 more planned
  - Primary platform: Windows
  - Communication: EN or PL acceptable
- 🔧 Updated "What is DECIDED" section with new rules
- 🗂️ Translated .serena/project.yml to English

### v1.1 - 2025-10-23
- 🔧 Added "Using Tools" section
- 📚 Mandatory Context7 usage rules for library documentation
- ✅ Process: resolve-library-id → get-library-docs → code generation
- ⚠️ Prohibition of guessing API syntax

### v1.0 - 2025-10-22
- ✨ Initial version (lightweight)
- 📋 Basic project information
- 🦁 Assistant concept description
- 📁 Document structure
- 🎯 Decision list (decided vs undecided)

---

## 🔄 This Document Update Process

### When AI (Claude) Updates CLAUDE.md:

**RULE:** Always propose, never update automatically!

#### Process:
1. **AI notices:** "This information in CLAUDE.md is outdated/incomplete"
2. **AI proposes:** "I see we decided X. Should I update CLAUDE.md?"
3. **User decides:** "Yes, update" or "No, wait"
4. **AI updates:** Makes change + adds entry in "Update History"

#### What Should Trigger Update Proposal:
- ✅ Making a key decision (e.g. chosen business model)
- ✅ Establishing new convention (e.g. variable naming)
- ✅ Adding new document to project
- ✅ Changing folder structure
- ✅ Finalizing MVP (which features are included)
- ✅ Starting new phase (e.g. transition to implementation)

#### What Does NOT Require Update:
- ❌ Minor concept changes (only in Kalahari-koncepcja-v2.md)
- ❌ "To consider" ideas (can go to TODO in concept)
- ❌ Temporary experiments
- ❌ Discussions without final decision

### Example of Good Update Proposal:

> 💬 **AI:** "I noticed we decided that MVP will have only 4 animal assistants (Meerkat, Elephant, Cheetah, Lion). Should I update CLAUDE.md in 'What is DECIDED' section and add details in 'Graphical Assistant' section?"

### Update Frequency:
- **Not more often than:** Per work session (if there were significant decisions)
- **Not less often than:** Weekly (collective update)
- **Versioning:** Only for major changes (v1.1, v1.2, v2.0)

---

## 📊 ROADMAP.md and CHANGELOG.md Update Rules

### Purpose

**ROADMAP.md** tracks future development plans and phases.
**CHANGELOG.md** documents all changes following [Keep a Changelog](https://keepachangelog.com) format.

Both files use **Semantic Versioning 2.0.0** ([semver.org](https://semver.org)).

### When to Update CHANGELOG.md

**ALWAYS update when:**
- ✅ Making a key architectural decision (document in [Unreleased] → Decided)
- ✅ Creating new files/directories (document in [Unreleased] → Added)
- ✅ Changing project structure (document in [Unreleased] → Changed)
- ✅ Completing documentation milestone (create new [DOCS-X.Y] section)
- ✅ Completing development phase (create new [X.Y.Z-prerelease] section)
- ✅ Public release (create new [X.Y.Z] section)

**Categories to use:**
- **Added** - New features, files, documentation
- **Changed** - Changes to existing functionality
- **Deprecated** - Soon-to-be removed features
- **Removed** - Removed features/files
- **Fixed** - Bug fixes
- **Security** - Security fixes
- **Decided** - Key project decisions (pre-development only)

**Version Format:**
- **Pre-Development:** [DOCS-X.Y] - Documentation milestones
- **Development:** [0.MINOR.PATCH-alpha/beta/rc] - Alpha, beta, release candidate
- **Production:** [MAJOR.MINOR.PATCH] - Semantic versioning

### When to Update ROADMAP.md

**Update when:**
- ✅ Completing a major milestone (mark checkbox ✅, update status)
- ✅ Changing phase timeline estimates
- ✅ Adding/removing features from phase scope
- ✅ Making significant architectural decisions affecting roadmap
- ✅ Monthly progress review (update Current Status)
- ✅ Starting new phase (update phase status to "🔄 In Progress")

**Do NOT update for:**
- ❌ Minor task completion (tracked in todos, not roadmap)
- ❌ Daily progress updates (too granular)
- ❌ Experimental features (only finalized features)

### Update Process

1. **AI proposes update:** "I see we completed X. Should I update CHANGELOG.md and/or ROADMAP.md?"
2. **User approves:** "Yes, update" or provides corrections
3. **AI updates files:**
   - CHANGELOG.md: Add entry under appropriate version/category
   - ROADMAP.md: Update checkboxes, status indicators, timelines
   - Update "Last Updated" timestamp in both files
4. **AI may update CLAUDE.md** if architectural decision impacts project rules

### Example Changelog Entry

```markdown
## [Unreleased]

### Added
- ROADMAP.md - 18-month development plan (Phases 0-5)
- CHANGELOG.md - Version history tracking

### Decided
- Versioning strategy: Semantic Versioning 2.0.0
- Pre-release format: 0.MINOR.PATCH-alpha/beta/rc
```

### Example Roadmap Update

```markdown
## Phase 0: Foundation (Weeks 1-8 | 2-3 months)

**Status:** 🔄 In Progress
**Target Version:** 0.1.0-alpha

### Core Infrastructure
- [x] CMake build system (all platforms)
- [x] vcpkg integration (manifest mode)
- [ ] wxWidgets 3.3.0+ basic application window (dark mode support)
```

### Cross-Reference Rules

- CHANGELOG.md should reference ROADMAP.md for phase completions
- ROADMAP.md should reference detailed docs (project_docs/)
- CLAUDE.md should reference both for versioning decisions
- All three should stay synchronized on architectural decisions

### Review Cycle

- **CHANGELOG.md:** Updated immediately when change occurs
- **ROADMAP.md:** Monthly review + phase transitions
- **Both:** Reviewed together before major releases

---

## 📞 Contact and Roles

**Project Manager:** User
**Main Executor:** Claude (AI)
**Work Model:** User leads, Claude executes and proposes

---

## 🎯 Next Steps (TODO)

**Phase: Documentation & Architecture**

- [x] Finish refining concept - **DONE**
- [x] Decide on business model - **Open Core + SaaS Hybrid**
- [x] Decide on tech stack - **C++20 + wxWidgets + Python plugins**
- [x] Define MVP (specific feature list) - **DONE (Phases 0-5)**
- [x] Choose platform strategy - **All platforms from MVP**
- [x] Finalize assistant concept - **DONE (8 animals, 4 in MVP, Lion default)**
- [x] Finalize branding - **DONE (realistic animals, African theme)**
- [x] Organize documentation - **DONE (project_docs/ structure)**
- [x] Create core documentation - **DONE (11/11 docs complete, 100%)**
- [x] i18n system finalized - **DONE (09_i18n.md, bwx_sdk pattern)**
- [x] GUI design finalized - **DONE (08_gui_design.md, Command Registry, toolbars)**
- [x] User documentation planned - **DONE (11_user_documentation_plan.md, MkDocs)**
- [x] **ALL 7 Architectural decisions finalized:**
  - **GUI Pattern:** MVP (Model-View-Presenter)
  - **Error Handling:** Hybrid (exceptions + error codes + wxLog*)
  - **Dependency Management:** Hybrid (Singletons + DI)
  - **Threading:** Dynamic pool (2-4 workers, CPU-aware)
  - **Memory:** Lazy loading from Phase 1
  - **Undo/Redo:** Command pattern (100 commands default)
  - **Document Model:** Composite pattern (Book → Parts → Chapters)
- [x] **DONE:** Complete 03_architecture.md (547 lines, MVP pattern + all decisions)
- [x] **DONE:** Complete 04_plugin_system.md (495 lines, Extension Points + Event System)
- [x] **DONE:** Complete 07_mvp_tasks.md (405 lines, week-by-week Phase 0-1)
- [x] **DONE:** Add PROJECT STATUS UPDATE PROTOCOL to CLAUDE.md
- [x] **DONE:** Cleanup .claude/ directory (51 files, -32%)
- [ ] **NEXT:** Start Phase 0 Week 1 - Project Structure & CMake
- [ ] **Phase 0:** Foundation (8 weeks total)

---

**Document Version:** 4.2 (Architectural Decisions Complete)
**Last Update:** 2025-10-25
**Updated By:** Claude (with user approval)
