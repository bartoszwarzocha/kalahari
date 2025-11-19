# KALAHARI - Writer's IDE

> **Advanced writing environment for book authors** | C++20 + Qt6 | Cross-platform Desktop App

**Status:** 🔄 Phase 0 - Qt Migration (Clean Slate Approach)
**Version:** 6.0 (Qt Migration)
**Last Update:** 2025-11-19

---

## 🔴 CARDINAL RULES - READ FIRST

### 0. AGENT USAGE - MANDATORY (CHECK EVERY TIME)

**BEFORE every action, check if agent activation required:**

| Trigger | Agent | Action | Priority |
|---------|-------|--------|----------|
| **Conversation start** | session-manager | Run `/load-session` (auto-detects mode) | 🔴 CRITICAL |
| **User signals end** ("zakończmy", "koniec", "bye") | session-manager | Run `/save-session` (choose mode) | 🔴 CRITICAL |
| **Before conversation end** | session-manager | BLOCK until `/save-session` complete | 🔴 CRITICAL |
| **Hourly checkpoint / WIP** | session-manager | `/save-session` (quick, ~15s) | 🟢 RECOMMENDED |
| **End of day / subtask done** | session-manager | `/save-session --sync` (~30s) | 🟡 HIGH |
| **Task/phase complete** | session-manager | `/save-session --full` (~4min) | 🔴 CRITICAL |
| **CI/CD failure** ("build failed", "GitHub Actions") | deployment-engineer | Activate via Task tool | 🟡 HIGH |
| **Build error** ("CMake error", "vcpkg error") | deployment-engineer | Activate via Task tool | 🟡 HIGH |
| **Test failure** ("Catch2 failed", "tests fail") | qa-engineer | Activate via Task tool | 🟡 HIGH |
| **GUI work** ("panel", "dialog", "perspective") | ux-designer | Activate via Task tool | 🟡 MEDIUM |
| **New module complete** | qa-engineer | Design test strategy | 🟡 MEDIUM |

**Execution rules:**
- 🔴 CRITICAL: NEVER skip, BLOCK other work until complete
- 🟡 HIGH: Strongly recommended, don't skip without reason
- 🟢 RECOMMENDED: Use frequently for safety
- Use `Task` tool with appropriate `subagent_type` or slash command

**Session save modes (intelligent system):**
- `/save-session` (quick) - Local commit, no push, ~15s, offline-capable
- `/save-session --sync` - Push to GitHub, trigger CI/CD, ~30s
- `/save-session --full` - Full verification, CHANGELOG auto-gen, CI/CD wait, ~4min
- `/load-session` - Single command, auto-detects mode from last session metadata

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
→ 2. mcp__serena__find_symbol("SettingsDialog", include_body=true)
→ 3. mcp__context7__resolve-library-id("Qt")
→ 4. mcp__context7__get-library-docs(received_id, topic="QCheckBox")
→ 5. THEN generate code
```

### 2. Qt6 Layout - ABSOLUTE Rules

**MANDATORY patterns:**
- ✅ **ALWAYS use** QVBoxLayout/QHBoxLayout for panel layouts
- ✅ **ALWAYS use** QGroupBox for visual grouping
- ✅ **ALWAYS use** stretch factors (0=fixed, 1+=flexible) in addWidget()
- ✅ **ALWAYS set layout** on widget: `setLayout(layout)`
- ✅ **ALWAYS use** margins and spacing for consistent UI

**FORBIDDEN patterns:**
- ❌ **NEVER use** fixed pixel sizes without scalability
- ❌ **NEVER use** hardcoded dimensions (prefer size policies)
- ❌ **NEVER forget** to set layout on parent widget
- ❌ **NEVER skip** QSizePolicy for responsive controls

**DPI & Font Scaling:**
- Qt6 handles DPI scaling automatically (no manual code needed)
- Global font scaling: `QApplication::setFont(QFont("Segoe UI", baseSize * scale))`
- Per-widget styling: Use QSS (Qt Style Sheets)

**Example correct layout:**
```cpp
// 1. Create main layout
QVBoxLayout *mainLayout = new QVBoxLayout(this);

// 2. Create QGroupBox for visual grouping
QGroupBox *groupBox = new QGroupBox("Settings", this);
QVBoxLayout *groupLayout = new QVBoxLayout(groupBox);

// 3. Add controls with stretch factors
QCheckBox *checkbox = new QCheckBox("Enable Feature", groupBox);
groupLayout->addWidget(checkbox, 0);  // 0 = fixed height

QLabel *label = new QLabel("Description text here", groupBox);
label->setWordWrap(true);
groupLayout->addWidget(label, 1);  // 1 = fills remaining space

// 4. Add group to main layout
mainLayout->addWidget(groupBox);

// 5. CRITICAL: Set layout on widget
setLayout(mainLayout);
```

### 3. Atomic Task Workflow - NEVER Skip

**NEW MODEL (since 2025-11-09):** Tasks are now **ATOMIC** - small, focused, 30-120 minute units.

**ATOMIC TASK RULES:**
- ✅ **ONE task at a time** - No simultaneous work
- ✅ **ONE functionality per task** - No scope creep
- ✅ **ONE file changed** (or max 2-3 tightly related)
- ✅ **100% verifiable** - Clear acceptance criteria
- ✅ **30-120 minutes** - If longer, split into smaller tasks
- ❌ **NEVER "przy okazji"** - No "while I'm here" fixes
- ❌ **NEVER skip approval** - Every plan needs "Approved, proceed"
- ❌ **NEVER start next** until current is 100% complete
- ❌ **NEVER create task files in advance** - ONLY when starting that task

**TASK FILE CREATION RULE (CRITICAL):**
- ✅ ROADMAP = Source of truth for what tasks exist
- ✅ Create task file ONLY when STARTING that specific task
- ✅ Complete current task 100% BEFORE creating next task file
- ❌ NEVER create multiple task files ahead of time
- ❌ Task files created prematurely become stale/wrong when plans change

**Why this rule exists:**
- Requirements change during implementation
- ROADMAP is updated, but old task files remain unchanged → CONFUSION
- Premature task files violate atomic workflow (one task at a time)
- Prevents divergence between ROADMAP (master) and task files (details)

**TASK FILE NAMING CONVENTION:**
- **From ROADMAP:** `NNNNN_P_Z_description.md`
  - NNNNN = sequential task number (00001-99999)
  - P = phase number (0-5)
  - Z = zagadnienie (main topic) number in phase (1-9)
  - Example: `00034_1_2_dynamic_menu_builder.md` (Task 34, Phase 1, Zagadnienie 1.2)
- **Custom tasks:** `NNNNN_description.md`
  - No phase/zagadnienie (for fixes, tests, refactors not in ROADMAP)
  - Example: `00043_fix_windows_crash.md`

**EPIC vs ATOMIC:**
- **EPIC** = Large feature (e.g., "Auto-Save System") - NOT a task number yet
- **ATOMIC TASK** = Small step (e.g., "#00045: Bind auto-save timer event") - HAS task number

**Example:**
- ❌ BAD: Task #00020 "Navigator Panel + Settings fixes" (too big, mixed concerns)
- ✅ GOOD: Task #00021 "Fix Windows Settings crash" (30 min, one file, one bug)

**Full workflow:** See [project_docs/12_dev_protocols.md](project_docs/12_dev_protocols.md)

### 4. Documentation Update Rules - MANDATORY

**CHANGELOG.md and ROADMAP.md MUST be updated in parallel:**

| Trigger Event | CHANGELOG.md | ROADMAP.md |
|--------------|--------------|------------|
| Task status change | ✅ Add to [Unreleased] | ✅ Update task checkbox/status |
| Phase complete | ✅ Document completion | ✅ Mark phase ✅ COMPLETE |
| Architectural decision | ✅ Add to Decided section | ✅ Update relevant phase notes |
| Task reordering | ✅ Document reasoning | ✅ Update task sequence |
| Milestone achieved | ✅ Add entry | ✅ Update Key Milestones |
| Timeline change | ✅ Document why | ✅ Update timeline estimates |

**Rule:** If you update CHANGELOG.md, ask yourself: "Does ROADMAP.md need updating?" (Answer is almost always YES!)

### 5. End-of-Session Protocol - MANDATORY

**Use intelligent session save system (choose appropriate mode):**

**Quick checkpoints (hourly/WIP):**
- Run `/save-session` (default, ~15s)
- Local commit only, no push
- Lightweight memory snapshot
- Skip docs verification

**End of day / Subtask complete:**
- Run `/save-session --sync` (~30s)
- Push to GitHub, trigger CI/CD
- Don't wait for CI/CD results
- Enhanced session memory

**Task/Phase milestone complete:**
- Run `/save-session --full` (~4min)
- **Auto-generates CHANGELOG/ROADMAP** if missing (with user approval)
- Waits for CI/CD verification (Linux, macOS, Windows)
- Comprehensive verification report

**Agent handles all verification automatically:**
- ✅ Git commit/push
- ✅ CHANGELOG.md auto-generation (--full mode)
- ✅ ROADMAP.md auto-generation (--full mode)
- ✅ Temporary files check
- ✅ CI/CD monitoring (--full mode)
- ✅ Session summary report

**User can skip ONLY if explicitly says:** "Skip session save" or "WIP - no save"

---

## 📋 Project Identity

**Name:** Kalahari
**Type:** Writer's IDE (Desktop Application)
**Purpose:** Complete writing environment for book authors
**Target Audience:** Novelists, non-fiction authors, book journalists
**License:** MIT (core) + Trademark ("Kalahari" name)
**Current Phase:** Phase 0 - Qt Migration (2025-11-19)

**Key Features:**
- African naming convention (ecosystem: Serengeti, Okavango, Victoria, Zambezi)
- 8 animal assistants (4 in MVP: Lion, Meerkat, Elephant, Cheetah)
- Complete writing toolkit (editor, project management, statistics, export)
- Plugin system from day zero (Python 3.11 embedded)
- Cross-platform (Windows, macOS, Linux)

---

## 🛠️ Technology Stack

**Core:** C++20 (GCC 10+, Clang 10+, MSVC 2019+)
**GUI:** Qt6 6.5.0+ (Widgets, automatic DPI scaling)
**Build:** CMake 3.21+ + vcpkg (manifest mode)
**Testing:** Catch2 v3 (BDD style)
**Logging:** spdlog
**JSON:** nlohmann_json
**Compression:** libzip (.klh files)
**Database:** SQLite3 (Phase 2+)
**Plugins:** Python 3.11 Embedded + pybind11

**Migration Note:** Migrated from wxWidgets to Qt6 (2025-11-19, Clean Slate approach)
**Archive:** wxwidgets-archive branch + v0.2.0-alpha-wxwidgets tag

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
9. [09_i18n.md](project_docs/09_i18n.md) - Qt i18n system (tr() + .ts/.qm files)
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

**Timeline:** 18 months (Phases 0-5) + 4 weeks Qt migration
**Target Release:** Kalahari 1.0 (Q3-Q4 2026)
**Current Status:** 🔄 Phase 0 - Qt Migration (Week 1 of 4, Started 2025-11-19)

**Migration Context:**
- **Decision:** Migrated from wxWidgets to Qt6 (2025-11-19)
- **Strategy:** Clean Slate Approach (Option B)
- **Archived:** wxwidgets-archive branch + v0.2.0-alpha-wxwidgets tag
- **Preserved:** Core (5,966 LOC), Tests (5,912 LOC), Bindings (120 LOC), Plugin system

**6 Development Phases:**
1. **Phase 0:** Qt Foundation (4 weeks) - Qt6 setup, QMainWindow, basic GUI
2. **Phase 1:** Core Editor (Weeks 1-20) - Rich text, QDockWidget, .klh files
3. **Phase 2:** Plugin System MVP (Weeks 21-30) - 4 working plugins
4. **Phase 3:** Feature Plugins (Weeks 31-44) - Premium plugins
5. **Phase 4:** Advanced Plugins (Weeks 45-56) - Export Suite, Research Pro
6. **Phase 5:** Polish & Release (Weeks 57-68) - Testing, docs, packaging 🎉

**📄 Full roadmap:** [ROADMAP.md](ROADMAP.md)

---

## ✅ What is DECIDED

### Project Fundamentals
- ✅ Name: **Kalahari** (Writer's IDE)
- ✅ Type: **Desktop App** (C++20 + Qt6)
- ✅ License: **MIT** (core) + Trademark
- ✅ Platforms: **Windows, macOS, Linux** (all in MVP)
- ✅ Languages: **EN + PL** (MVP requirement, +4 in Phase 2)

### Technology
- ✅ Language: **C++20** (modern STL, smart pointers)
- ✅ GUI: **Qt6 6.5.0+** (Widgets, QDockWidget, automatic DPI)
- ✅ Build: **CMake 3.21+ + vcpkg**
- ✅ Testing: **Catch2 v3**
- ✅ Plugins: **Python 3.11 Embedded + pybind11**
- 🔄 **Migration:** wxWidgets → Qt6 (2025-11-19, Clean Slate)

### Architecture Patterns
- ✅ GUI Pattern: **MVP** (Model-View-Presenter)
- ✅ Error Handling: **Hybrid** (exceptions + error codes + spdlog)
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

### Atomic Task Management Workflow

**ATOMIC TASK LIFECYCLE (MANDATORY):**

```
┌─────────────────────────────────────────────┐
│ 1. ANALYSIS (5-10 min)                      │
│    - Read relevant code (Serena MCP!)       │
│    - Understand context                     │
│    - Identify exact change needed           │
│    - Verify ONE small change only           │
├─────────────────────────────────────────────┤
│ 2. PLAN (5-10 min)                          │
│    - Write task file (tasks/NNNNN_name.md) │
│    - List acceptance criteria               │
│    - Identify test cases                    │
│    - Estimate time (30-120 min)            │
├─────────────────────────────────────────────┤
│ 3. USER APPROVAL ⛔                         │
│    - Present plan to user                   │
│    - Wait for "Approved, proceed"           │
│    - BLOCK until approval received          │
│    - NO CODING before this point!           │
├─────────────────────────────────────────────┤
│ 4. IMPLEMENTATION (20-60 min)              │
│    - Code changes (ONE functionality)       │
│    - Build verification                     │
│    - NO scope creep!                        │
│    - NO "przy okazji" fixes!                │
├─────────────────────────────────────────────┤
│ 5. TESTING (10-20 min)                      │
│    - Manual testing (user or AI-guided)     │
│    - Acceptance criteria check              │
│    - PASS/FAIL decision                     │
│    - If FAIL → fix or rollback              │
├─────────────────────────────────────────────┤
│ 6. COMPLETION (5-10 min)                    │
│    - Update task file (mark DONE)           │
│    - Update ROADMAP.md (checkbox)           │
│    - Update CHANGELOG.md (if significant)   │
│    - Git commit (single atomic change)      │
│    - Move to NEXT task                      │
└─────────────────────────────────────────────┘
```

**CRITICAL RULES:**
- ❌ NO simultaneous tasks (one at a time)
- ❌ NO scope changes mid-task (stay focused)
- ❌ NO "quick fixes" while working on task
- ✅ FULL verification before next task
- ✅ User approval for EVERY plan
- ✅ 100% completion or rollback (no partial state)

**EPIC Breakdown Process:**

When large feature (EPIC) is ready:
1. Create breakdown document (tasks/.wip/EPIC-NAME-breakdown.md)
2. Split into 5-15 atomic tasks
3. User approves breakdown
4. Execute tasks one by one (00031, 00032, 00033...)
5. When EPIC complete, update ROADMAP

### End-of-Session Checklist
1. ✅ Update CHANGELOG.md ([Unreleased] section)
2. ✅ Update ROADMAP.md (if milestone completed)
3. ✅ Check for temporary files (.tmp, _backup)
4. ✅ Report session summary
5. ✅ Ask about git commit

**User can skip ONLY if explicitly says so!**

---

## 🤖 Claude Code Resources

**2 Skills:** `kalahari-plugin-system`, `kalahari-i18n`
**6 Commands:** `/code-review`, `/architecture-review`, `/best-practices`, `/dependency-check`, `/health-check`, `/testing-strategy`
**6 Agents:** `software-architect`, `ux-designer`, `qa-engineer`, `security-engineer`, `deployment-engineer`, `session-manager`

**Note:** kalahari-wxwidgets skill removed during Qt migration (2025-11-19)

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

## 🎯 Current Status (Phase 0 - Qt Migration 🔄)

### Qt Migration: IN PROGRESS (Started 2025-11-19)

**Strategy:** Clean Slate Approach (Option B)
**Timeline:** 4 weeks (Phase 0)
**Current:** Day 1, Step 0.4 (Update CLAUDE.md)

**Completed Steps:**
- ✅ **Step 0.1:** Archive Current State (30 min)
  - Created wxwidgets-archive branch
  - Created v0.2.0-alpha-wxwidgets tag
  - Deleted 3 feature branches (dpi-scaling, dpi-support-clean, theme-manager)
  - Reset main to commit e191390

- ✅ **Step 0.2:** Clean Main Branch (60 min)
  - Deleted 28,098 LOC (103 files)
  - Removed src/gui/ and include/kalahari/gui/
  - Removed 49 wxWidgets task files
  - Removed bwx_sdk submodule
  - Removed wxWidgets skills

- ✅ **Step 0.3:** Update Project Configuration (90 min)
  - vcpkg.json: wxWidgets → Qt6 6.5.0+
  - CMakeLists.txt: Qt6 integration (AUTOMOC/AUTORCC/AUTOUIC)
  - src/main.cpp: Qt6 placeholder with QApplication

**Preserved Core (12,000 LOC):**
- ✅ Core library (5,966 LOC - ZERO wx dependencies)
- ✅ Tests (5,912 LOC - 50 test cases, 2,239 assertions)
- ✅ Python bindings (120 LOC - pure pybind11)
- ✅ Plugin architecture (Extension Points, Event Bus)
- ✅ Settings system (JSON persistence)
- ✅ Document model (Book, Part, Document)

**Next Steps (Phase 0 - Weeks 1-4):**
- ⏳ **Step 0.4:** Update CLAUDE.md (60 min) - IN PROGRESS
- ⏳ **Step 0.5:** Create Fresh ROADMAP.md (90 min)
- ⏳ **Step 0.6:** Update CHANGELOG.md (30 min)
- ⏳ **Week 1:** Qt Foundation (QMainWindow, basic GUI)
- ⏳ **Weeks 2-4:** Settings, Dialogs, Core Editor

**📄 Full plan:** [QT_MIGRATION_ROADMAP.md](QT_MIGRATION_ROADMAP.md)

---

## 📝 Update History

### v6.0 - 2025-11-19 (QT MIGRATION)

- 🔄 **BREAKING CHANGE: wxWidgets → Qt6 migration**
- 🚀 **Status updated:** Phase 1 Week 13 → Phase 0 Qt Migration Day 1
- 🗑️ **Removed:** All wxWidgets-specific content and patterns
- ✅ **Added:** Qt6 layout patterns (QVBoxLayout, QGroupBox, QSS)
- ✅ **Added:** Qt DPI & font scaling patterns (automatic DPI, QApplication::setFont())
- ✅ **Updated:** Technology Stack (Qt6 6.5.0+)
- ✅ **Updated:** Current Status section (Qt migration progress)
- ✅ **Updated:** Skills list (removed kalahari-wxwidgets)
- ✅ **Updated:** i18n reference (Qt tr() + .ts/.qm files)
- 📋 **Migration Context added:**
  - Clean Slate Approach (Option B)
  - wxwidgets-archive branch + v0.2.0-alpha-wxwidgets tag
  - Preserved core: 12,000 LOC (5,966 core + 5,912 tests + 120 bindings)
  - Deleted GUI: 28,098 LOC (103 files)
- 📄 **Reference:** [QT_MIGRATION_ROADMAP.md](QT_MIGRATION_ROADMAP.md)

### v5.2 - 2025-11-11 (ATOMIC TASKS + PHASE 1 UPDATE)

- 🚀 **Phase 1 status updated** - Current Status section reflects Phase 1 Week 13
- ✅ **Atomic Task Model** already documented in v5.1 (CARDINAL RULES #3)
- 📋 **Phase 1 Progress added:**
  - Task #00019 COMPLETE (Custom Text Editor, 100%)
  - Task #00020 COMPLETE WITH BUGS (Navigator Panel structure)
  - Task #00021 COMPLETE (Windows Settings crash fix)
  - Task #00022 IN PROGRESS (Apply button)
- 📊 **Current Status section** updated:
  - Phase 0: 100% Complete (2025-10-31)
  - Phase 1: IN PROGRESS (Started 2025-11-04, Week 13)
  - Next: Tasks #00023-#00030 (atomic fixes)
- 🗓️ **Last Update:** 2025-11-11

### v5.1 - 2025-11-05 (INTELLIGENT SESSION SYSTEM)

- 🧠 **Session system redesigned** with intelligent mode detection
- 🔴 **CARDINAL RULES #0 updated** with session save modes:
  - `/save-session` (quick) - Local, ~15s, offline-capable
  - `/save-session --sync` - GitHub push + CI/CD trigger, ~30s
  - `/save-session --full` - Full verification + auto-docs, ~4min
  - `/load-session` - Auto-detects mode from session metadata
- ✅ **End-of-Session Protocol** redesigned:
  - Agent handles all verification (git, docs, CI/CD)
  - Auto-generates CHANGELOG/ROADMAP in --full mode
  - Intelligent mode selection guide
- 📋 **New slash commands:**
  - `.claude/commands/save-session.md` - 3-tier system with frontmatter metadata
  - `.claude/commands/load-session.md` - Mode-aware restoration with gap detection
- 🔍 **Git gap detection** - Finds undocumented commits between sessions
- 🤖 **session-manager agent** - Fully automated session handling

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

**Document Version:** 5.1 (Intelligent Sessions)
**Last Update:** 2025-11-05
**Updated By:** Claude (with user approval)
**Size:** ~330 lines (session system added)
