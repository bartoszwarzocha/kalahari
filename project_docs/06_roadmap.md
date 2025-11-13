# ROADMAP & CHANGELOG - Maintenance Rules

> **META-DOCUMENT**: Rules for maintaining `/ROADMAP.md` and `/CHANGELOG.md`

**Status:** ✅ Complete
**Last Updated:** 2025-10-29
**Version:** 2.0 (Meta-Document)

---

## Purpose

This document defines the RULES and PROTOCOLS for maintaining the project's strategic planning documents:

- **`/ROADMAP.md`** (root) - The ACTUAL project roadmap showing development state
- **`/CHANGELOG.md`** (root) - Complete version history following Keep a Changelog format

**NOTE:** This file NO LONGER contains the actual roadmap. For the current roadmap, see **`/ROADMAP.md`**.

---

## 1. ROADMAP.md - Maintenance Rules

### 1.1 File Location
- **Path:** `/ROADMAP.md` (project root)
- **Purpose:** Show actual project development state and timeline
- **Format:** Markdown with phase sections

### 1.2 When to Update

**ALWAYS update when:**
- ✅ Completing a major milestone (mark checkbox ✅, update status)
- ✅ Changing phase timeline estimates
- ✅ Adding/removing features from phase scope
- ✅ Making significant architectural decisions affecting roadmap
- ✅ Monthly progress review (update Current Status)
- ✅ Starting new phase (update phase status to "🔄 In Progress")

**DO NOT update for:**
- ❌ Minor task completion (tracked in todos, not roadmap)
- ❌ Daily progress updates (too granular)
- ❌ Experimental features (only finalized features)

### 1.3 Update Process

1. **AI proposes update:** "I see we completed X. Should I update ROADMAP.md?"
2. **User approves:** "Yes, update" or provides corrections
3. **AI updates file:**
   - Mark completed checkboxes ✅
   - Update status indicators (🔴 Not Started → 🔄 In Progress → ✅ Complete)
   - Adjust timelines if needed
   - Update "Last Updated" timestamp
4. **AI reports summary:** "Updated ROADMAP.md: marked X complete, updated Phase Y status"

### 1.4 Structure

**ATOMIC TASK MODEL (2025-11-09):** ROADMAP now uses Phase → Zagadnienie → Checkbox structure WITHOUT task numbers in checkboxes. Task numbers are assigned when work starts.

**Required sections:**
- **Overview** - Timeline, phases, strategy
- **Phase N: Name** - For each phase (0-5):
  - Status indicator (🔴/🔄/✅)
  - Duration estimate
  - Focus area
  - Deliverable
  - **Zagadnienie subsections** (main topics within phase, e.g., 1.2 Command Registry Architecture)
  - Each Zagadnienie contains checkboxes WITHOUT task numbers
  - Risk assessment
- **Milestones Summary** - Table showing all phases
- **Risk Management** - Known risks and mitigations
- **Success Metrics** - How we measure success

**Status Indicators:**
- 🔴 **Not Started** - Phase hasn't begun
- 🔄 **In Progress** - Currently working on this phase
- ✅ **Complete** - Phase finished and verified

**Zagadnienie Structure:**
```markdown
### 1.2 Command Registry Architecture 🚀 IN PROGRESS
**Status:** 🚀 IN PROGRESS (7/12 tasks complete)

#### Core Structures ✅ COMPLETE
- [x] IconSet struct (16/24/32px bitmap storage)
- [x] KeyboardShortcut struct (toString/fromString parsing, operators)

#### Dynamic UI Generation 📋 PLANNED (5 tasks remaining)
- [ ] Create MenuBuilder class (buildFromRegistry, addSeparator, addSubmenu)
- [ ] Replace hardcoded createMenuBar() with MenuBuilder
```

**Task File Naming Convention:**
- **From ROADMAP:** `NNNNN_P_Z_description.md`
  - NNNNN = sequential task number (00001-99999)
  - P = phase number (0-5)
  - Z = zagadnienie number in phase (1-9)
  - Example: `00034_1_2_dynamic_menu_builder.md` (Task 34, Phase 1, Zagadnienie 1.2)
- **Custom tasks:** `NNNNN_description.md` (no P_Z for fixes/tests not in ROADMAP)

**Key Principle:** Task numbers appear ONLY in task files (tasks/NNNNN_*.md), NOT in ROADMAP checkboxes. This prevents divergence when plans change.

### 1.5 Linking

**From ROADMAP.md:**
- Link to detailed task docs: `[07_mvp_tasks.md](project_docs/07_mvp_tasks.md)`
- Link to architecture: `[03_architecture.md](project_docs/03_architecture.md)`

**To ROADMAP.md:**
- From CLAUDE.md: Link in "Roadmap" section
- From README.md: Link in "Quick Start" section
- From project_docs/README.md: Reference in documentation index

---

## 2. CHANGELOG.md - Maintenance Rules

### 2.1 File Location
- **Path:** `/CHANGELOG.md` (project root)
- **Purpose:** Document all changes following [Keep a Changelog](https://keepachangelog.com)
- **Format:** Markdown with version sections

### 2.2 Versioning Strategy

**Semantic Versioning 2.0.0** ([semver.org](https://semver.org))

**Pre-Development (Documentation Phase):**
```
[DOCS-X.Y] - YYYY-MM-DD
```
- **MAJOR (X):** Major documentation restructure
- **MINOR (Y):** New complete document or significant updates

**Development (Alpha/Beta):**
```
[0.MINOR.PATCH-alpha/beta/rc.N] - YYYY-MM-DD
```
- **0.x.x:** Pre-release (not production ready)
- **alpha:** Early development, unstable
- **beta:** Feature complete, testing phase
- **rc:** Release candidate, final testing

**Production:**
```
[MAJOR.MINOR.PATCH] - YYYY-MM-DD
```
- **MAJOR:** Breaking changes, incompatible API
- **MINOR:** New features, backward compatible
- **PATCH:** Bug fixes, backward compatible

### 2.3 When to Update

**ALWAYS update when:**
- ✅ Making a key architectural decision
- ✅ Creating new files/directories
- ✅ Changing project structure
- ✅ Completing documentation milestone
- ✅ Completing development phase
- ✅ Public release (alpha, beta, rc, stable)

**Categories to use:**
- **Added** - New features, files, documentation
- **Changed** - Changes to existing functionality
- **Deprecated** - Soon-to-be removed features
- **Removed** - Removed features/files
- **Fixed** - Bug fixes
- **Security** - Security fixes
- **Decided** - Key project decisions (pre-development only)

### 2.4 Update Process

1. **AI proposes update:** "I see we changed X. Should I update CHANGELOG.md?"
2. **User approves:** "Yes, update" or provides corrections
3. **AI updates file:**
   - Add entry under `[Unreleased]` section
   - Choose appropriate category (Added/Changed/Fixed/etc.)
   - Write clear, concise description
   - If milestone reached, create new version section
4. **AI reports summary:** "Updated CHANGELOG.md: Added X to [Unreleased]"

### 2.5 Structure

**Required sections:**

```markdown
# Changelog

All notable changes to Kalahari will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- New feature description

### Changed
- Change description

### Fixed
- Bug fix description

## [VERSION] - YYYY-MM-DD

### Category
- Description
```

**Version Format Examples:**
- `[DOCS-1.0] - 2025-10-22` - Documentation milestone
- `[0.1.0-alpha.1] - 2025-11-15` - First alpha release
- `[0.2.0-beta.1] - 2025-12-20` - First beta release
- `[1.0.0] - 2026-06-01` - Production release

### 2.6 Writing Good Entries

**DO:**
- ✅ Be specific: "Added Settings Dialog with diagnostic toggle"
- ✅ Use active voice: "Implemented" not "Implementation of"
- ✅ Include context: "Fixed empty panel issue by adding sizer to m_contentPanel"
- ✅ Group related changes: Multiple fixes to same feature under one entry

**DON'T:**
- ❌ Be vague: "Updated stuff", "Fixed things"
- ❌ Use passive voice: "Was added", "Has been changed"
- ❌ Skip context: "Fixed bug" (what bug?)
- ❌ Duplicate entries: Same change in multiple categories

**Examples:**

**Good:**
```markdown
### Added
- Settings Dialog with tree navigation and diagnostic mode toggle (Task #00008)
- CMake build scripts for Linux/Windows/macOS parallel builds
- Context7 integration for wxWidgets documentation lookup

### Fixed
- Settings Dialog empty panel issue - added sizer to m_contentPanel (settings_dialog.cpp:131-139)
- Compiler warning about unused parameter in onOK handler (added [[maybe_unused]])
```

**Bad:**
```markdown
### Added
- New stuff
- Files

### Fixed
- Bug
- Issue with dialog
```

---

## 3. Cross-Reference Protocol

### 3.1 CHANGELOG ↔ ROADMAP Integration

**When completing phase milestones:**
1. Update ROADMAP.md (mark phase complete, update status)
2. Update CHANGELOG.md (create new version section if milestone)
3. CHANGELOG should reference ROADMAP: "Completed Phase 0 (see ROADMAP.md)"
4. Both should be updated in same session

**Example:**
```markdown
# CHANGELOG.md
## [0.1.0-alpha.1] - 2025-11-15

### Added
- Complete Phase 0 foundation (see ROADMAP.md Phase 0)
- CMake build system for all platforms
- vcpkg integration with manifest mode
- Plugin manager skeleton

# ROADMAP.md
## Phase 0: Foundation

**Status:** ✅ Complete
**Completed:** 2025-11-15
```

### 3.2 CLAUDE.md Integration

**CLAUDE.md should reference both:**
- In "Roadmap" section: Link to `/ROADMAP.md`
- In "Update History": Mention when ROADMAP/CHANGELOG updated
- Keep CLAUDE.md concise - don't duplicate ROADMAP content

**Example:**
```markdown
# CLAUDE.md

## 🚀 Roadmap

**See full roadmap:** [ROADMAP.md](ROADMAP.md)

**Current Phase:** Phase 0 Week 3 - Settings System
**Next Milestone:** Complete Phase 0 foundation (Week 8)
```

### 3.3 README.md Integration

**Root README.md should link:**
- Quick Start → Link to ROADMAP.md for current phase
- Changelog → Link to CHANGELOG.md for version history

**project_docs/README.md should:**
- Reference root ROADMAP.md, not duplicate it
- Explain this file (06_roadmap.md) is META-DOCUMENT with rules only

---

## 4. End-of-Session Checklist

**Before closing ANY work session, verify:**

### 4.1 CHANGELOG.md
- [ ] All changes added to `[Unreleased]` section
- [ ] Correct categories used (Added/Changed/Fixed/etc.)
- [ ] Entries are specific and clear
- [ ] Date matches current date if version section created

### 4.2 ROADMAP.md
- [ ] Completed tasks marked with ✅
- [ ] Phase status updated (🔴/🔄/✅)
- [ ] Timeline adjustments documented
- [ ] "Last Updated" timestamp current

### 4.3 Cross-References
- [ ] Both files consistent (if phase complete, both updated)
- [ ] Links between files working
- [ ] CLAUDE.md references up to date
- [ ] README.md links correct

### 4.4 Git Commit
- [ ] Both files staged if updated
- [ ] Commit message descriptive
- [ ] No temporary files left

---

## 5. Version Release Protocol

### 5.1 When to Create Version Section

**Documentation Milestones (DOCS-X.Y):**
- Completing major documentation restructure
- Finishing all Phase 0 planning documents
- Major update to CLAUDE.md or architecture

**Development Milestones (0.X.Y):**
- Completing phase milestone (Phase 0, 1, 2, etc.)
- First working build (0.1.0-alpha.1)
- Feature complete (0.X.0-beta.1)
- Release candidate (1.0.0-rc.1)

**Production Release (X.Y.Z):**
- Public release (1.0.0)
- Major feature releases (1.1.0, 2.0.0)
- Patch releases (1.0.1, 1.0.2)

### 5.2 Release Checklist

**When creating version section:**
1. Move all `[Unreleased]` entries to new version section
2. Add version number and date: `## [X.Y.Z] - YYYY-MM-DD`
3. Organize entries by category (Added, Changed, Fixed, etc.)
4. Create new empty `[Unreleased]` section at top
5. Update ROADMAP.md if phase milestone
6. Create git tag if development milestone: `git tag v0.1.0-alpha.1`
7. Push tag to GitHub: `git push --tags`

**Example:**
```markdown
## [Unreleased]
<!-- Empty after release -->

## [0.1.0-alpha.1] - 2025-11-15

### Added
- CMake build system for all platforms
- vcpkg integration (manifest mode)
- Plugin manager skeleton
- Event bus implementation

### Fixed
- Memory leak in Python interpreter initialization
- Compiler warnings on macOS

### Decided
- Use Catch2 v3 for testing framework
- Adopt MVP pattern for GUI architecture
```

---

## 6. AI Assistant Rules

### 6.1 MUST

- ✅ **ALWAYS propose updates** to ROADMAP/CHANGELOG (never edit automatically)
- ✅ **ALWAYS update both** if phase milestone completed
- ✅ **ALWAYS run End-of-Session Checklist** before closing session
- ✅ **ALWAYS use correct version format** (DOCS-X.Y or 0.X.Y-prerelease)
- ✅ **ALWAYS ask user for confirmation** before version bumps

### 6.2 SHOULD

- 💡 **Proactively remind** user if session ending without updates
- 💡 **Suggest** checklist items that might apply
- 💡 **Warn** if inconsistencies detected between files
- 💡 **Report summary** at end of session showing what was updated

### 6.3 MUST NOT

- ❌ **Never close session** without running End-of-Session Checklist
- ❌ **Never update** strategic files automatically without approval
- ❌ **Never create** version sections without user confirmation
- ❌ **Never skip** cross-reference updates (CHANGELOG + ROADMAP together)

### 6.4 Reporting Format

**After updates, AI MUST report:**
```
📝 Session Summary:
- ✅ CHANGELOG.md updated (3 changes to [Unreleased])
- ✅ ROADMAP.md updated (Phase 0 Week 3 marked complete)
- ✅ Cross-references verified (CLAUDE.md, README.md)
- ⏭️ Next: Start Phase 0 Week 4 tasks
```

---

## 7. Examples

### 7.1 Example: Completing Atomic Task

**Scenario:** Just finished Task #00024 (IconSet struct - Phase 1, Zagadnienie 1.2)

**CHANGELOG.md update:**
```markdown
## [Unreleased]

### Added
- IconSet struct for multi-resolution icon storage (command.h)
- Support for 16px/24px/32px icon variants in CommandRegistry
- Icon size management in IconRegistry with DPI awareness
```

**ROADMAP.md update (Zagadnienie checkbox):**
```markdown
## PHASE 1: Core Editor 🚀 IN PROGRESS

### 1.2 Command Registry Architecture 🚀 IN PROGRESS
**Status:** 🚀 IN PROGRESS (8/12 tasks complete)

#### Core Structures 🚀 IN PROGRESS
- [x] IconSet struct (16/24/32px bitmap storage)  <!-- ✅ JUST COMPLETED (Task #00024) -->
- [x] KeyboardShortcut struct (toString/fromString parsing, operators)
- [x] Command struct (ID, label, callback, icon, shortcut)
- [ ] CommandRegistry class (singleton, register/query commands)
```

**Note:** Task number (00024) appears ONLY in comment and task file, NOT in checkbox text. This prevents ROADMAP from breaking when task numbers change.

### 7.2 Example: Completing Phase

**Scenario:** Phase 0 complete!

**CHANGELOG.md - Create version section:**
```markdown
## [Unreleased]
<!-- Empty after release -->

## [0.1.0-alpha.1] - 2025-11-15

### Added
- CMake build system supporting Windows, macOS, Linux
- vcpkg manifest mode integration
- wxWidgets 3.3.0+ application skeleton
- Settings system with JSON persistence
- Logging system with spdlog
- Python 3.11 embedding via pybind11
- Plugin manager skeleton
- Event bus implementation
- Document model (Book, Chapter, Project classes)
- Catch2 v3 testing framework
- GitHub Actions CI/CD pipeline

### Decided
- MVP pattern for GUI architecture
- Hybrid error handling (exceptions + error codes)
- Lazy loading strategy from Phase 1
- Command pattern for undo/redo
```

**ROADMAP.md update:**
```markdown
## Phase 0: Foundation

**Status:** ✅ Complete
**Completed:** 2025-11-15
**Duration:** 8 weeks (as planned)

### Deliverable Checkpoint
- ✅ Builds on all 3 platforms
- ✅ Python embedding works
- ✅ Can load simple "hello world" Python plugin
- ✅ Event bus functional
- ✅ Document model serializes to JSON
- ✅ Basic tests passing

## Phase 1: Core Editor

**Status:** 🔄 In Progress
**Started:** 2025-11-16
**Duration:** Weeks 9-20 (3-4 months)
```

### 7.3 Example: Architectural Decision

**Scenario:** Decided to use MVP pattern for GUI

**CHANGELOG.md update:**
```markdown
## [Unreleased]

### Decided
- Adopted MVP (Model-View-Presenter) pattern for GUI architecture
- wxWidgets views will be thin wrappers, business logic in Presenters
- Enables testable GUI code without running actual GUI
```

**ROADMAP.md - No update needed** (architecture decisions don't change timeline)

**CLAUDE.md update:**
```markdown
## ✅ What is DECIDED

### Architectural Patterns
- ✅ **GUI Pattern:** MVP (Model-View-Presenter)
  - Clear separation: Model ↔ Presenter ↔ View
  - Testable business logic (Presenters unit testable)
  - wxWidgets views are thin wrappers
```

---

## 8. Troubleshooting

### 8.1 Common Issues

**Issue:** CHANGELOG entries too vague
**Solution:** Be specific - include file paths, line numbers, task IDs

**Issue:** ROADMAP checkboxes not updating
**Solution:** Mark completed tasks with [x], update phase status indicator

**Issue:** Version numbers inconsistent
**Solution:** Follow semantic versioning strictly, use correct format for phase

**Issue:** Cross-references broken
**Solution:** Use relative paths, verify links after restructuring

### 8.2 Verification Commands

```bash
# Check CHANGELOG format
grep -E "^\[.*\] - [0-9]{4}-[0-9]{2}-[0-9]{2}$" CHANGELOG.md

# Find broken links in ROADMAP
grep -E "\[.*\]\(.*\)" ROADMAP.md | grep -v "^#"

# Verify version consistency
head -20 CHANGELOG.md | grep "## \["

# Check for unreleased changes
grep -A 20 "## \[Unreleased\]" CHANGELOG.md
```

---

## References

- **Keep a Changelog:** https://keepachangelog.com/en/1.1.0/
- **Semantic Versioning:** https://semver.org/spec/v2.0.0.html
- **Actual Roadmap:** `/ROADMAP.md`
- **Version History:** `/CHANGELOG.md`
- **Project Instructions:** `/CLAUDE.md`

---

**Document Version:** 2.0 (Meta-Document)
**Last Updated:** 2025-10-29
**Status:** ✅ Complete
