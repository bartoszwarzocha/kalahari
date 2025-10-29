# KALAHARI - Writer's IDE

> **Advanced writing environment for book authors** | C++20 + wxWidgets | Cross-platform Desktop App

**Status:** 🔄 Phase 0 Week 3 - Settings System
**Version:** 5.0 (Compact)
**Last Update:** 2025-10-29

---

## 🔴 CARDINAL RULES - READ FIRST

### 1. MCP Tools - MANDATORY Usage

**Serena MCP (Code Exploration):**
- ✅ **ALWAYS use FIRST** before reading files
- ✅ **ALWAYS use** `get_symbols_overview` before reading full files
- ✅ **ALWAYS use** `find_symbol` for targeted code reading
- ❌ **NEVER read entire files** without exploring structure first
- ❌ **NEVER use Read tool** for code exploration (use Serena)

**Context7 MCP (Library Documentation):**
- ✅ **ALWAYS use** before generating code with external libraries
- ✅ **Process:** `resolve-library-id` → `get-library-docs` → generate code
- ❌ **NEVER guess** API syntax (outdated knowledge)
- ❌ **NEVER propose code** without checking current documentation

**Example correct workflow:**
```
User: "Add a checkbox to Settings Dialog"
→ 1. mcp__serena__get_symbols_overview("src/gui/settings_dialog.cpp")
→ 2. mcp__serena__find_symbol("DiagnosticsPanel", include_body=true)
→ 3. mcp__context7__resolve-library-id("wxWidgets")
→ 4. mcp__context7__get-library-docs(received_id, topic="wxCheckBox")
→ 5. THEN generate code
```

### 2. wxWidgets Layout - ABSOLUTE Rules

**MANDATORY patterns:**
- ✅ **ALWAYS use** wxStaticBoxSizer for configuration sections
- ✅ **ALWAYS use** wxEXPAND flag for stretching controls
- ✅ **ALWAYS use** proportions (0=fixed, 1+=flexible) in Add() calls
- ✅ **ALWAYS parent** controls to StaticBox: `diagBox->GetStaticBox()`
- ✅ **ALWAYS use** sizers - ALL panels MUST have SetSizer() called

**FORBIDDEN patterns:**
- ❌ **NEVER use** fixed pixel sizes (no Wrap(500), no SetSize(400, 300))
- ❌ **NEVER use** hardcoded dimensions (only proportions and flags)
- ❌ **NEVER skip** wxEXPAND for controls that should stretch
- ❌ **NEVER forget** to add panels to parent sizer (causes invisible panels!)

**Example correct layout:**
```cpp
// 1. Create sizer for panel
wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

// 2. Create StaticBoxSizer for visual grouping
wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Group Title");

// 3. Add controls with wxEXPAND and proportions
wxCheckBox* checkbox = new wxCheckBox(box->GetStaticBox(), wxID_ANY, "Label");
box->Add(checkbox, 0, wxALL | wxEXPAND, 5);  // proportion=0 (fixed height)

wxStaticText* text = new wxStaticText(box->GetStaticBox(), wxID_ANY, "Description");
box->Add(text, 1, wxALL | wxEXPAND, 5);  // proportion=1 (fills remaining space)

// 4. Add box to main sizer
mainSizer->Add(box, 1, wxALL | wxEXPAND, 10);

// 5. CRITICAL: Set sizer on panel
SetSizer(mainSizer);
```

### 3. Task Workflow - NEVER Skip

- ✅ **ALWAYS create** task file BEFORE implementation (tasks/NNNNN_name.md)
- ✅ **ALWAYS wait** for user approval ("Approved, proceed")
- ✅ **ALWAYS update** CHANGELOG.md on completion
- ❌ **NEVER start coding** without approved task file
- ❌ **NEVER skip** task file for "quick fixes" (document it!)

**Full workflow:** See [project_docs/12_dev_protocols.md](project_docs/12_dev_protocols.md)

### 4. End-of-Session Checklist - MANDATORY

**Before EVERY session end:**
1. ✅ Update CHANGELOG.md (all changes to [Unreleased])
2. ✅ Update ROADMAP.md (if milestone completed)
3. ✅ Verify no temporary files (.tmp, _backup, _old)
4. ✅ Report session summary to user
5. ✅ Ask about git commit if changes made

**User can skip ONLY if explicitly says:** "Skip checklist" or "WIP session"

---

## 📋 Project Identity

**Name:** Kalahari
**Type:** Writer's IDE (Desktop Application)
**Purpose:** Complete writing environment for book authors
**Target Audience:** Novelists, non-fiction authors, book journalists
**License:** MIT (core) + Trademark ("Kalahari" name)
**Current Phase:** Phase 0 Week 3 (Foundation)

**Key Features:**
- African naming convention (ecosystem: Serengeti, Okavango, Victoria, Zambezi)
- 8 animal assistants (4 in MVP: Lion, Meerkat, Elephant, Cheetah)
- Complete writing toolkit (editor, project management, statistics, export)
- Plugin system from day zero (Python 3.11 embedded)
- Cross-platform (Windows, macOS, Linux)

---

## 🛠️ Technology Stack

**Core:** C++20 (GCC 10+, Clang 10+, MSVC 2019+)
**GUI:** wxWidgets 3.3.0+ + wxAUI (dockable panels)
**Build:** CMake 3.21+ + vcpkg (manifest mode)
**Testing:** Catch2 v3 (BDD style)
**Logging:** spdlog
**JSON:** nlohmann_json
**Compression:** libzip (.klh files)
**Database:** SQLite3 (Phase 2+)
**Plugins:** Python 3.11 Embedded + pybind11

**📄 Full details:** [project_docs/02_tech_stack.md](project_docs/02_tech_stack.md)

---

## 📚 Documentation Map

**All project documentation:** [project_docs/README.md](project_docs/README.md)

**Core Documents (11/11 Complete):**
1. [01_overview.md](project_docs/01_overview.md) - Vision, goals, target audience
2. [02_tech_stack.md](project_docs/02_tech_stack.md) - Complete tech stack
3. [03_architecture.md](project_docs/03_architecture.md) - MVP pattern, threading, error handling
4. [04_plugin_system.md](project_docs/04_plugin_system.md) - Extension Points, Event System
5. [05_business_model.md](project_docs/05_business_model.md) - Open Core + Plugins + SaaS
6. [06_roadmap.md](project_docs/06_roadmap.md) - Rules for maintaining ROADMAP/CHANGELOG
7. [07_mvp_tasks.md](project_docs/07_mvp_tasks.md) - Week-by-week Phase 0-1 tasks
8. [08_gui_design.md](project_docs/08_gui_design.md) - Command Registry, toolbars
9. [09_i18n.md](project_docs/09_i18n.md) - wxLocale + gettext pattern
10. [10_branding.md](project_docs/10_branding.md) - Logo, colors, animal designs
11. [11_user_documentation_plan.md](project_docs/11_user_documentation_plan.md) - MkDocs strategy
12. [12_dev_protocols.md](project_docs/12_dev_protocols.md) - Task workflow, session protocols

**Strategic Files:**
- [ROADMAP.md](ROADMAP.md) - Actual project roadmap (18-month plan)
- [CHANGELOG.md](CHANGELOG.md) - Version history (Keep a Changelog format)

---

## 💼 Business Model

**Strategy:** Open Core + Plugin Marketplace + SaaS

- **Core (MIT):** Free editor, project management, basic stats, DOCX/PDF export
- **Premium Plugins:** 5 paid ($14-39 each, $79 bundle)
  - AI Assistant Pro, Advanced Analytics, Export Suite, Research Pro, Collaboration Pack
- **Cloud SaaS:** Cloud Sync Pro ($5-10/month)

**📄 Full details:** [project_docs/05_business_model.md](project_docs/05_business_model.md)

---

## 🚀 Roadmap

**Timeline:** 18 months (Phases 0-5)
**Target Release:** Kalahari 1.0 (Q2-Q3 2026)
**Current Status:** 🔄 Phase 0 Week 3 - Settings System

**6 Development Phases:**
1. **Phase 0:** Foundation (Weeks 1-8) - CMake, vcpkg, plugin system
2. **Phase 1:** Core Editor (Weeks 9-20) - Rich text, wxAUI, .klh files
3. **Phase 2:** Plugin System MVP (Weeks 21-30) - 4 working plugins
4. **Phase 3:** Feature Plugins (Weeks 31-44) - Premium plugins
5. **Phase 4:** Advanced Plugins (Weeks 45-56) - Export Suite, Research Pro
6. **Phase 5:** Polish & Release (Weeks 57-68) - Testing, docs, packaging 🎉

**📄 Full roadmap:** [ROADMAP.md](ROADMAP.md)

---

## ✅ What is DECIDED

### Project Fundamentals
- ✅ Name: **Kalahari** (Writer's IDE)
- ✅ Type: **Desktop App** (C++20 + wxWidgets)
- ✅ License: **MIT** (core) + Trademark
- ✅ Platforms: **Windows, macOS, Linux** (all in MVP)
- ✅ Languages: **EN + PL** (MVP requirement, +4 in Phase 2)

### Technology
- ✅ Language: **C++20** (modern STL, smart pointers)
- ✅ GUI: **wxWidgets 3.3.0+ + wxAUI**
- ✅ Build: **CMake 3.21+ + vcpkg**
- ✅ Testing: **Catch2 v3**
- ✅ Plugins: **Python 3.11 Embedded + pybind11**

### Architecture Patterns
- ✅ GUI Pattern: **MVP** (Model-View-Presenter)
- ✅ Error Handling: **Hybrid** (exceptions + error codes + wxLog*)
- ✅ Dependency Management: **Hybrid** (Singletons + DI)
- ✅ Threading: **Dynamic pool** (2-4 workers, CPU-aware)
- ✅ Memory: **Lazy loading** (metadata eager, content on-demand)
- ✅ Undo/Redo: **Command pattern** (100 commands default)
- ✅ Document Model: **Composite** (Book → Parts → Chapters)

### Business Model
- ✅ Strategy: **Open Core + Plugins + SaaS**
- ✅ Core: **MIT** (open source)
- ✅ Plugins: **5 paid** ($14-39 each, $79 bundle)
- ✅ Cloud: **Subscription** ($5-10/month)

### Graphical Assistant
- ✅ Concept: **8 animals** (4 in MVP: Lion, Meerkat, Elephant, Cheetah)
- ✅ Default: **Lion** (brand symbol)
- ✅ Style: **Realistic** (photorealistic)
- ✅ Format: **Static images** (6-8 moods per animal)
- ✅ UI: **Dockable panel** (bottom-right)

**📄 Full list:** All 11 project_docs/ documents contain complete decisions

---

## ❓ What is NOT YET DECIDED

- ⏳ **Coding start date:** When Phase 0 Week 1 implementation begins?
- ⏳ **Testing coverage:** 70%? 80%? Which modules require tests?
- ⏳ **CI/CD details:** Exact GitHub Actions workflow, caching strategy
- ⏳ **Plugin signing:** Code signing certificates, verification process
- ⏳ **Analytics:** Telemetry? Usage statistics? Privacy policy?

---

## 🤖 Instructions for AI (Claude Code)

### General Rules
1. **Ask when uncertain** - If <90% certain about user's intent, ASK
2. **Use African convention** - Names, examples, comments
3. **English for code** - All code, comments, config files (MANDATORY)
4. **Follow CARDINAL RULES** - MCP tools, wxWidgets layout, task workflow
5. **Quality over size** - Correct code > file size optimization

### C++ Coding Conventions

**File names:** snake_case (`character_bank.cpp`, `settings_dialog.h`)
**Class names:** PascalCase (`CharacterCard`, `PluginManager`)
**Functions:** camelCase (`getTitle()`, `addChapter()`)
**Members:** m_prefix (`m_title`, `m_chapters`)
**Constants:** UPPER_SNAKE_CASE (`MAX_CHAPTERS`)
**Namespaces:** lowercase (`kalahari::core`, `kalahari::gui`)
**Comments:** Doxygen style (`///`), English, detailed
**Headers:** `#pragma once` (preferred) or header guards

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

    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

private:
    std::string m_name;
    std::vector<std::string> m_traits;
};

} // namespace core
} // namespace kalahari
```

### When Proposing Features
- Check alignment with project_docs/ documentation
- Propose in context of roadmap (which phase?)
- Consider UX impact
- Think through technical consequences

### When Updating Documentation
- **ALWAYS ask** before changing CLAUDE.md
- Propose changes, don't apply automatically
- Update "Update History" with version bump (if approved)

---

## 🔄 Development Protocols

**Detailed workflows:** [project_docs/12_dev_protocols.md](project_docs/12_dev_protocols.md)

### Task Management Workflow
1. **PLAN** - AI creates task file (tasks/NNNNN_name.md)
2. **REVIEW** - User examines proposed approach
3. **APPROVAL** - User says "Approved, proceed"
4. **IMPLEMENTATION** - AI executes checklist step-by-step
5. **VERIFICATION** - Build, test, check memory leaks
6. **COMPLETION** - Update CHANGELOG, ROADMAP, commit

**NEVER start implementation without approved task file!**

### End-of-Session Checklist
1. ✅ Update CHANGELOG.md ([Unreleased] section)
2. ✅ Update ROADMAP.md (if milestone completed)
3. ✅ Check for temporary files (.tmp, _backup)
4. ✅ Report session summary
5. ✅ Ask about git commit

**User can skip ONLY if explicitly says so!**

---

## 🤖 Claude Code Resources

**3 Skills:** `kalahari-wxwidgets`, `kalahari-plugin-system`, `kalahari-i18n`
**6 Commands:** `/code-review`, `/architecture-review`, `/best-practices`, `/dependency-check`, `/health-check`, `/testing-strategy`
**6 Agents:** `software-architect`, `ux-designer`, `qa-engineer`, `security-engineer`, `deployment-engineer`, `session-manager`

**Quick commands:**
```bash
/health-check              # AI-driven project health
./tools/project-status.sh  # Automated file/tool checks
./tools/pre-commit-check.sh # 35+ quality checks
./tools/check-ci.sh status # CI/CD monitoring
```

---

## 📞 Contact and Roles

**Project Manager:** User
**Main Executor:** Claude (AI)
**Work Model:** User leads, Claude executes and proposes

---

## 🎯 Current Status (Phase 0 Week 3)

**Completed:**
- [x] CMake build system (all platforms)
- [x] vcpkg integration (manifest mode)
- [x] wxWidgets basic application window
- [x] Settings Dialog with diagnostic toggle (Task #00008)

**In Progress:**
- [ ] Logging system (spdlog integration)
- [ ] Python 3.11 embedding

**Next:**
- [ ] Plugin manager skeleton
- [ ] Event bus implementation

**📄 Detailed tasks:** [project_docs/07_mvp_tasks.md](project_docs/07_mvp_tasks.md)

---

## 📝 Update History

### v5.0 - 2025-10-29 (COMPACT VERSION)
- 🔥 **Major refactoring:** 1263 → 300 lines (76% reduction)
- 🔴 **CARDINAL RULES section** added at TOP (MCP tools, wxWidgets layout, task workflow)
- 📤 **Moved to project_docs/12_dev_protocols.md:**
  - Task Management Workflow (full section)
  - Project Status Update Protocol (full section)
  - ROADMAP/CHANGELOG update rules (full section)
- 🔗 **Replaced duplications with links:**
  - Tech stack → link to 02_tech_stack.md
  - Business model → link to 05_business_model.md
  - Roadmap → link to ROADMAP.md
  - All detailed docs → links to project_docs/
- ✅ **Serena MCP instructions** added to CARDINAL RULES
- 📊 **"What is DECIDED"** simplified (full details in project_docs/)
- 🎯 **Current Status** section added (Phase 0 Week 3 progress)

### v4.2 - 2025-10-25 (ARCHITECTURAL DECISIONS COMPLETE)
- All 7 core architectural decisions finalized
- 3 major documents completed (03, 04, 07)
- PROJECT STATUS UPDATE PROTOCOL added
- .claude/ directory cleanup (51 files, -32%)

### v4.1 - 2025-10-25 (PROJECT ORGANIZATION)
- Versioning system established
- Created ROADMAP.md and CHANGELOG.md
- Semantic Versioning 2.0.0 adopted

### v4.0 - 2025-10-24 (MAJOR UPDATE - C++ ARCHITECTURE)
- Complete stack rewrite: Python → C++20
- Plugin architecture from day zero
- Timeline updated: 18 months

**Earlier versions:** See previous Update History in git history

---

**Document Version:** 5.0 (Compact)
**Last Update:** 2025-10-29
**Updated By:** Claude (with user approval)
**Size:** ~300 lines (target achieved!)
