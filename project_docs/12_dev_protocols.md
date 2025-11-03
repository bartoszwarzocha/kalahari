# Development Protocols

> **Reference document**: Detailed workflows for AI assistant and developers

**Status:** ✅ Complete
**Last Updated:** 2025-10-29
**Version:** 1.0

---

## Purpose

This document contains detailed development protocols, workflows, and rules extracted from CLAUDE.md to keep the main file concise. These are reference materials for:

- AI assistant (Claude Code) operational rules
- Development workflow processes
- Code quality standards
- Session management protocols

**For quick reference, see:** [CLAUDE.md](../CLAUDE.md) - Concise version with CARDINAL RULES

---

## 1. MCP Tools Usage Protocols

### 1.1 Serena MCP (Code Exploration)

**Purpose:** Symbolic code exploration without reading entire files

**MANDATORY Usage Rules:**
- ✅ **ALWAYS use FIRST** before reading any code file
- ✅ **ALWAYS explore structure** before targeted reads
- ✅ **ALWAYS use** `get_symbols_overview` for file structure
- ✅ **ALWAYS use** `find_symbol` for targeted reading
- ❌ **NEVER read entire files** without symbolic exploration first
- ❌ **NEVER use Read tool** for code exploration

**Workflow Example:**

```
User: "Add a new method to PluginManager class"

Step 1: Explore file structure
→ mcp__serena__get_symbols_overview("src/core/plugin_manager.cpp")
  Returns: List of classes, methods, functions

Step 2: Find specific symbol
→ mcp__serena__find_symbol("PluginManager", relative_path="src/core/plugin_manager.cpp",
                            depth=1, include_body=false)
  Returns: PluginManager class structure with all methods

Step 3: Read specific method if needed
→ mcp__serena__find_symbol("PluginManager/loadPlugin", include_body=true)
  Returns: Full loadPlugin method body

Step 4: THEN propose changes
```

**Available Serena Tools:**
- `list_dir` - List directory contents
- `find_file` - Find files by pattern
- `search_for_pattern` - Flexible regex search (for non-code searches)
- `get_symbols_overview` - Top-level symbols in file
- `find_symbol` - Find symbol by name path
- `find_referencing_symbols` - Find references to symbol
- `replace_symbol_body` - Replace symbol implementation
- `insert_after_symbol` - Insert code after symbol
- `insert_before_symbol` - Insert code before symbol
- `rename_symbol` - Rename symbol across codebase

**Common Mistakes:**
- ❌ Using Read tool immediately without exploring
- ❌ Reading entire 500-line file to find one method
- ❌ Not using depth parameter to see child symbols
- ❌ Forgetting to use relative_path to narrow search

### 1.2 Context7 MCP (Library Documentation)

**Purpose:** Get current, accurate documentation for external libraries

**MANDATORY Usage Rules:**
- ✅ **ALWAYS use** before generating code with external libraries
- ✅ **ALWAYS follow process:** resolve-library-id → get-library-docs → generate code
- ❌ **NEVER guess** API syntax (AI knowledge may be outdated)
- ❌ **NEVER propose code** without checking current documentation

**Workflow Example:**

```
User: "Create a wxListCtrl with checkboxes"

Step 1: Resolve library ID
→ mcp__context7__resolve-library-id("wxWidgets")
  Returns: "/wxwidgets/wxwidgets"

Step 2: Get documentation
→ mcp__context7__get-library-docs("/wxwidgets/wxwidgets", topic="wxListCtrl checkboxes")
  Returns: Current documentation for wxListCtrl, checkbox support, examples

Step 3: Generate code based on current API
```

**Available Context7 Tools:**
- `resolve-library-id` - Get Context7-compatible library ID
- `get-library-docs` - Fetch current documentation (specify topic for focused results)

**Common Mistakes:**
- ❌ Assuming API hasn't changed since training cutoff
- ❌ Not specifying topic parameter (gets generic docs)
- ❌ Using outdated method names (e.g., wxWidgets 3.0 vs 3.3)

---

## 2. Task Management Workflow

### 2.1 Overview

**All development tasks follow:** PLAN → REVIEW → APPROVAL → IMPLEMENTATION → VERIFICATION → COMPLETION

This ensures quality, predictability, and traceability of all work.

### 2.2 Task Files Structure

**Location:** `tasks/` directory (tracked in git)

**File naming:** `NNNNN_task_name.md` (5-digit zero-padded)

**Examples:**
- `00001_plugin_manager_skeleton.md`
- `00042_rtf_editor_widget.md`
- `00123_ai_assistant_integration.md`

**Working directories (git-ignored):**
- `tasks/.wip/` - Work in progress, temporary notes
- `tasks/.scratch/` - Experiments, sketches, throwaway content

### 2.3 Task File Template

```markdown
# Task #NNNNN: Task Name

## Context
- **Phase:** Phase X Week Y
- **Roadmap Reference:** ROADMAP.md reference
- **Related Docs:** Links to relevant documentation
- **Dependencies:** Previous tasks or requirements

## Objective
Clear, concise description of what needs to be accomplished.

## Proposed Approach
1. Step-by-step approach
2. Key technical decisions
3. Libraries/tools to use
4. Architecture considerations
5. Potential challenges

## Implementation Plan (Checklist)
- [ ] Specific actionable item 1
- [ ] Specific actionable item 2
- [ ] Write unit tests (if applicable)
- [ ] Update CMakeLists.txt (if applicable)
- [ ] Document API/code
- [ ] Update related documentation

## Risks & Open Questions
- Q: Question that needs answering?
- Q: Alternative approach to consider?
- Risk: Potential issue to watch for

## Status
- **Created:** YYYY-MM-DD
- **Approved:** YYYY-MM-DD (by User)
- **Started:** YYYY-MM-DD
- **Completed:** YYYY-MM-DD

## Implementation Notes
(Added during implementation)
- Decision: Why we chose X over Y
- Issue: Problem encountered and solution
- Change: Deviation from original plan

## Verification
- [ ] Code compiles on all platforms
- [ ] Tests pass (Catch2)
- [ ] No memory leaks (valgrind/ASAN if available)
- [ ] Code reviewed
- [ ] Documentation updated
```

### 2.4 Workflow Process

#### Step 1: PLAN (AI creates task file)
- AI identifies task from ROADMAP.md or user request
- AI creates task file with proposed approach
- AI presents file to user for review

#### Step 2: REVIEW (User examines plan)
- User reads proposed approach
- User asks questions if unclear
- User suggests alternatives if needed
- User identifies missing considerations

#### Step 3: APPROVAL (User approves explicitly)
- User says "Approved, proceed" or similar
- AI updates Status: Approved + date
- Only then can implementation begin

#### Step 4: IMPLEMENTATION (AI executes checklist)
- AI works through checklist step-by-step
- AI marks checkboxes as completed
- AI adds Implementation Notes for non-obvious decisions
- AI asks user if encountering blockers

#### Step 5: VERIFICATION (AI runs checks)
- AI compiles code on all platforms (if applicable)
- AI runs tests
- AI checks for memory issues
- AI verifies documentation updated

#### Step 6: COMPLETION (Mark done, update docs)
- AI marks task as Completed + date
- AI updates CHANGELOG.md (Added/Changed entries) - **ALWAYS**
- AI updates ROADMAP.md (task status, phase status) - **ALWAYS, not "if milestone completed"**
- AI reports completion to user

**CRITICAL:** CHANGELOG.md and ROADMAP.md must be updated in parallel! See Section 3 below.

### 2.5 Rules for AI

**MUST:**
- ✅ **NEVER start implementation** without approved task file
- ✅ **ALWAYS create task file first** for any non-trivial work
- ✅ **ALWAYS update checklist** as you progress (real-time)
- ✅ **ALWAYS add Implementation Notes** for non-obvious decisions
- ✅ **ALWAYS update related docs** (CHANGELOG, ROADMAP) on completion

**SHOULD:**
- 💡 **ASK if approach unclear** or multiple valid options exist
- 💡 **WARN about risks** identified during planning
- 💡 **SUGGEST alternatives** if better approach discovered
- 💡 **REPORT blockers immediately** (don't struggle silently)

**MUST NOT:**
- ❌ **Skip task file** for "quick fixes" (still document in task file)
- ❌ **Approve own plans** (only user approves)
- ❌ **Mark complete** if tests fail or implementation incomplete
- ❌ **Deviate from plan** without discussing with user

### 2.6 What Requires Task File?

**✅ ALWAYS create task file for:**
- New features (any size)
- Refactoring (more than single function)
- Architecture changes
- Plugin implementation
- Library integration
- GUI components
- Testing infrastructure
- Documentation (major documents)
- Bug fixes (non-trivial)

**❌ MAY skip task file for:**
- Typo fixes in comments
- Formatting adjustments
- Single-line bug fixes (obvious)
- Adding missing semicolon
- User explicitly says "skip task file"

**When in doubt, create task file!** Better over-documented than under-documented.

### 2.7 Example Workflow Session

```
User: "Let's implement Plugin Manager skeleton"

AI: "Creating task file tasks/00001_plugin_manager_skeleton.md..."
    [AI creates file with proposed approach]
    "I propose this approach:
     1. Create PluginManager class (singleton)
     2. Implement discoverPlugins() - scan plugins/ folder
     3. Add logging with spdlog
     4. Write unit tests with Catch2
     Should I proceed?"

User: "Looks good, but add plugin dependency resolution. Approved."

AI: "Updating task file with dependency resolution requirement..."
    "Starting implementation..."
    [✓] Create PluginManager class
    [✓] Implement discoverPlugins()
    [✓] Add dependency resolution
    [✓] Add logging with spdlog
    [✓] Write unit tests
    ...
    "Implementation complete. Running verification..."
    "All checks passed. Updating CHANGELOG.md..."
    "Task #00001 completed! ✅"
```

---

## 3. Session Management Protocol

### 3.1 When to Update Strategic Files

**After EVERY work session that involves:**
- ✅ Making architectural decisions
- ✅ Completing documentation
- ✅ Finishing phase/milestone
- ✅ Adding/removing features
- ✅ Changing project structure
- ✅ Major refactoring

### 3.2 End-of-Session Checklist

**MANDATORY steps before closing ANY work session:**

#### 1. CHANGELOG.md Update
- [ ] Add all changes to `[Unreleased]` section
- [ ] Use correct categories (Added/Changed/Decided/Removed/Fixed)
- [ ] Date the entry (YYYY-MM-DD)
- [ ] Be specific (not "updated docs", but "completed 03_architecture.md with MVP pattern")

#### 2. ROADMAP.md Update - **ALWAYS, not "if applicable"**
- [ ] Update task status (checkbox, status text)
- [ ] Update phase status if phase completed
- [ ] Update "Current Status" header
- [ ] Update "Last Updated" date
- [ ] Mark completed milestones in Key Milestones section
- [ ] Add architectural decisions to relevant phase descriptions
- [ ] Update timeline if estimates changed
- [ ] Add new risks/dependencies if identified

**Rule:** ROADMAP.md should ALWAYS be updated in parallel with CHANGELOG.md! If you updated CHANGELOG, check ROADMAP status fields.

#### 3. CLAUDE.md Update (if applicable)
- [ ] Update TODO section (mark DONE, add new tasks)
- [ ] Add entry to "Update History" with version bump (if major change)
- [ ] Update "What is DECIDED" if new decisions made
- [ ] Update "What is NOT YET DECIDED" (remove decided items)

#### 4. project_docs/README.md Update
- [ ] Update document statuses (✅ Complete / 🔄 In Progress / ⏳ Pending)
- [ ] Update "Document History" section
- [ ] Verify all links work

#### 5. Final Verification
- [ ] All TODO items from session marked as DONE or moved to next session
- [ ] No temporary files left (.tmp, _backup, _FULL, _old)
- [ ] All created files added to git tracking
- [ ] Consistency check: CLAUDE.md ↔ CHANGELOG.md ↔ ROADMAP.md

### 3.3 AI Assistant Rules

**Claude Code MUST:**
1. **Never close session** without running End-of-Session Checklist
2. **Always propose updates** to CLAUDE.md (never edit automatically)
3. **Ask user for confirmation** before version bumps
4. **Report what was updated** at end of session:
   ```
   📝 Session Summary:
   - ✅ CHANGELOG.md updated (3 changes)
   - ✅ ROADMAP.md updated (Phase 0 details)
   - ✅ CLAUDE.md updated (v5.0, compact version)
   - ✅ project_docs/README.md current
   ```

**Claude Code SHOULD:**
- Proactively remind user if session is ending without updates
- Suggest checklist items that might apply
- Warn if inconsistencies detected between strategic files

### 3.4 User Override

**User can skip checklist ONLY if explicitly says:**
- "Skip checklist this time"
- "WIP session, don't update"
- "Temporary work, no updates needed"

---

## 4. wxWidgets Layout Protocols

### 4.1 Sizer Hierarchy Rules

**MANDATORY patterns for ALL GUI panels:**

```cpp
// 1. Create main sizer for panel
wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

// 2. Create grouping sizer (StaticBoxSizer for visual frames)
wxStaticBoxSizer* groupBox = new wxStaticBoxSizer(
    wxVERTICAL,           // Orientation
    this,                  // Parent window
    "Section Title"        // Label
);

// 3. Add controls to group
//    - Parent to GetStaticBox() (NOT 'this')
//    - Use wxEXPAND for stretching
//    - Use proportions (0=fixed, 1+=flexible)
wxCheckBox* checkbox = new wxCheckBox(
    groupBox->GetStaticBox(),  // Parent = StaticBox
    wxID_ANY,
    "Checkbox Label"
);
groupBox->Add(checkbox, 0, wxALL | wxEXPAND, 5);  // proportion=0, fixed height

// 4. Add group to main sizer
mainSizer->Add(groupBox, 1, wxALL | wxEXPAND, 10);  // proportion=1, fills space

// 5. CRITICAL: Set sizer on panel
SetSizer(mainSizer);

// 6. Optional: Trigger layout (for dynamic changes)
Layout();
FitInside();  // Update scrollbars if scrolled window
```

### 4.2 Common Layout Patterns

**Fixed-size control (no stretching):**
```cpp
wxButton* btn = new wxButton(parent, wxID_ANY, "Click Me");
sizer->Add(btn, 0, wxALL, 5);  // proportion=0, no wxEXPAND
```

**Stretching control (fills available space):**
```cpp
wxTextCtrl* text = new wxTextCtrl(parent, wxID_ANY);
sizer->Add(text, 1, wxALL | wxEXPAND, 5);  // proportion=1, wxEXPAND
```

**Multi-line text with word wrap:**
```cpp
wxStaticText* desc = new wxStaticText(parent, wxID_ANY, "Long description...");
sizer->Add(desc, 1, wxALL | wxEXPAND, 5);  // proportion=1 for vertical fill
// NO Wrap(pixels) - let sizer handle wrapping naturally
```

**Two-column layout:**
```cpp
wxBoxSizer* rowSizer = new wxBoxSizer(wxHORIZONTAL);

wxStaticText* label = new wxStaticText(parent, wxID_ANY, "Label:");
rowSizer->Add(label, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);  // Fixed width

wxTextCtrl* input = new wxTextCtrl(parent, wxID_ANY);
rowSizer->Add(input, 1, wxALL | wxEXPAND, 5);  // Fills remaining space

mainSizer->Add(rowSizer, 0, wxEXPAND);  // Row doesn't stretch vertically
```

### 4.3 Forbidden Patterns

**❌ NEVER do this:**

```cpp
// Fixed pixel sizes
text->Wrap(500);  // WRONG - not responsive
panel->SetSize(400, 300);  // WRONG - breaks on different displays

// Hardcoded positions
button->SetPosition(wxPoint(10, 20));  // WRONG - use sizers

// Missing sizers
wxPanel* panel = new wxPanel(parent);
wxButton* btn = new wxButton(panel, wxID_ANY, "Test");
// WRONG - button not added to any sizer, panel has no sizer

// Forgetting to add to sizer
wxPanel* panel = new wxPanel(parent);
wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
wxButton* btn = new wxButton(panel, wxID_ANY, "Test");
panel->SetSizer(sizer);
// WRONG - button created but never added to sizer (invisible!)

// Wrong parent
wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, "Title");
wxCheckBox* checkbox = new wxCheckBox(this, wxID_ANY, "Check");  // WRONG parent
box->Add(checkbox, 0, wxEXPAND, 5);
// SHOULD BE: new wxCheckBox(box->GetStaticBox(), ...)
```

### 4.4 Debugging Layout Issues

**Problem:** Control not visible
**Solutions:**
1. Check if control added to sizer: `sizer->Add(control, ...)`
2. Check if panel has sizer: `panel->SetSizer(sizer)`
3. Check if wxEXPAND flag used (if control should stretch)
4. Check if parent correct (StaticBox controls → GetStaticBox())
5. Call `Layout()` after changes
6. Call `FitInside()` for scrolled windows

**Problem:** Control not stretching
**Solutions:**
1. Check proportion > 0: `sizer->Add(control, 1, ...)`
2. Check wxEXPAND flag present: `sizer->Add(control, 1, wxEXPAND, ...)`
3. Check parent sizer also stretches: All parent sizers must have proportion > 0

**Problem:** Text not wrapping
**Solutions:**
1. Use wxEXPAND flag: `sizer->Add(text, 1, wxALL | wxEXPAND, 5)`
2. DON'T use Wrap(pixels) - let sizer handle it
3. Set proportion > 0 for vertical space

---

## 5. Code Quality Standards

### 5.1 C++20 Modern Practices

**Memory Management:**
- ✅ Use `std::unique_ptr` for exclusive ownership
- ✅ Use `std::shared_ptr` for shared ownership
- ✅ Use `std::make_unique` and `std::make_shared`
- ❌ Never use raw `new`/`delete` (wxWidgets objects are exception - managed by parent)

**Error Handling:**
- ✅ Use exceptions for programmer errors (`std::invalid_argument`, `std::logic_error`)
- ✅ Use `std::optional<T>` for expected failures (e.g., findById)
- ✅ Use `std::expected<T, E>` for operations that can fail with error info
- ✅ Use wxLog* for user-facing messages

**Concurrency:**
- ✅ Use `std::thread` for worker threads
- ✅ Use `std::mutex` for thread safety
- ✅ Use `wxTheApp->CallAfter` for GUI updates from background threads
- ✅ Use Python GIL handling: `py::gil_scoped_acquire` / `py::gil_scoped_release`

### 5.2 Testing Requirements

**Unit Tests (Catch2 v3):**
- All core business logic must have unit tests
- Test coverage target: 70%+ for Phase 0-1, 80%+ for later phases
- Use BDD style: `TEST_CASE`, `SECTION`, `REQUIRE`

**Integration Tests:**
- Plugin loading/unloading
- Event bus under load
- File I/O (.klh, .kplugin)

**Example:**
```cpp
#include <catch2/catch_test_macros.hpp>
#include "plugin_manager.h"

TEST_CASE("PluginManager discovers plugins", "[plugin]") {
    SECTION("Empty plugins folder returns empty list") {
        PluginManager mgr;
        auto plugins = mgr.discoverPlugins();
        REQUIRE(plugins.empty());
    }

    SECTION("Valid plugin is loaded successfully") {
        PluginManager mgr;
        // Setup test plugin
        auto result = mgr.loadPlugin("test_plugin");
        REQUIRE(result.has_value());
        REQUIRE(result->getName() == "test_plugin");
    }
}
```

### 5.3 Documentation Standards

**Code Comments:**
- Use Doxygen style (`///`) for public APIs
- Document parameters, return values, exceptions
- Explain WHY, not WHAT (code shows what)

**Example:**
```cpp
/// @brief Load a plugin from file system
///
/// Discovers plugin manifest, validates dependencies, and loads
/// the plugin into memory. Thread-safe.
///
/// @param pluginPath Absolute path to plugin directory or .kplugin file
/// @return Plugin instance if successful, nullopt if load failed
/// @throws std::invalid_argument if pluginPath doesn't exist
/// @throws PluginException if plugin manifest invalid
std::optional<Plugin> loadPlugin(const std::filesystem::path& pluginPath);
```

---

## 6. Integration with Other Protocols

### 6.1 ROADMAP/CHANGELOG Integration

**See:** [project_docs/06_roadmap.md](06_roadmap.md) - Complete rules for maintaining ROADMAP.md and CHANGELOG.md

**Quick reference:**
- Update CHANGELOG.md immediately when change occurs
- Update ROADMAP.md when completing milestones
- Both must be synchronized on phase completions

### 6.2 Git Workflow

**Commit Message Format:**
```
type(scope): Short description

Longer explanation if needed.

- Bullet point 1
- Bullet point 2

Refs: #123 (if related to task/issue)
```

**Types:**
- `feat` - New feature
- `fix` - Bug fix
- `refactor` - Code restructure (no functional change)
- `docs` - Documentation only
- `test` - Adding/updating tests
- `chore` - Build process, dependencies

**Example:**
```
feat(plugin): Add plugin dependency resolution

Implemented topological sort for plugin loading order based
on declared dependencies in plugin.json manifest.

- Added PluginDependency class
- Implemented DependencyResolver
- Added unit tests for cycle detection

Refs: tasks/00001_plugin_manager_skeleton.md
```

---

## 7. Troubleshooting Common Issues

### 7.1 Build Issues

**Problem:** vcpkg dependencies not found
**Solution:**
```bash
# Regenerate vcpkg manifest
rm -rf build/
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
```

**Problem:** Compiler errors about missing includes
**Solution:** Check CMakeLists.txt `target_include_directories`

### 7.2 Runtime Issues

**Problem:** Plugin not loading
**Solution:**
1. Check plugin.json manifest exists and is valid
2. Check plugin dependencies satisfied
3. Check Python interpreter initialized
4. Check logs: `logs/kalahari.log`

**Problem:** GUI not updating after background task
**Solution:** Use `wxTheApp->CallAfter` to marshal to GUI thread

### 7.3 Testing Issues

**Problem:** Tests failing in CI but passing locally
**Solution:**
1. Check platform-specific code
2. Check absolute vs relative paths
3. Check test data files included in repo

---

## References

- **Main Config:** [CLAUDE.md](../CLAUDE.md)
- **ROADMAP/CHANGELOG Rules:** [06_roadmap.md](06_roadmap.md)
- **Architecture:** [03_architecture.md](03_architecture.md)
- **Plugin System:** [04_plugin_system.md](04_plugin_system.md)
- **Tasks:** [07_mvp_tasks.md](07_mvp_tasks.md)

---

**Document Version:** 1.0
**Last Updated:** 2025-10-29
**Status:** ✅ Complete
