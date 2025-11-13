---
description: Analyze ROADMAP and tasks to propose next work item
---

**Smart task planning based on project state**

**Automatically detects incomplete work or proposes next logical task**

**⚠️ NEW ATOMIC MODEL (2025-11-09):** Tasks are now small, focused units (30-120 min). EPICs are broken down into numbered atomic tasks (e.g., 00021, 00022) before execution.

## Execution Steps:

### 0. **Atomic Task Model Rules (NEW - 2025-11-09)**

**MANDATORY: Understand atomic model before proposing tasks**

#### Key Concepts:

**EPIC** (Large Feature):
- Example: "Auto-Save System", "Chapter CRUD Operations"
- Contains 5-15 small steps
- Takes days/weeks
- **NO task number until broken down**
- Listed in ROADMAP as "EPIC: Name"

**ATOMIC TASK** (Small Step):
- Example: "#00021: Fix Windows Settings crash"
- ONE functionality, ONE file (max 2-3)
- 30-120 minutes
- **HAS task number** (00021, 00022, etc.)
- Verifiable independently

#### When Proposing Next Task:

**✅ DO:**
- Propose next atomic task in sequence (e.g., after 00020 → propose 00021)
- Check ROADMAP for atomic tasks (00021-00030 defined)
- If no atomic tasks exist for EPIC, suggest creating breakdown first
- ONE task at a time (no parallel suggestions)

**❌ DON'T:**
- Don't propose large EPICs as tasks
- Don't skip task numbers (if 00021 exists, don't propose 00025)
- Don't suggest "quick fixes" bypassing task files

#### Current Project State (as of 2025-11-09):

**Completed:**
- Tasks 00001-00019: ✅ COMPLETE
- Task 00020: ⚠️ COMPLETE (with bugs)

**Next Atomic Tasks (Settings System Fixes):**
- Task 00021: Fix Windows Settings crash (P0 CRITICAL)
- Task 00022: Apply button event binding (P1 HIGH)
- Task 00023: Icon size Apply implementation (P1 HIGH)
- Task 00024-00030: Font scaling, verification tasks

**When /next-task is run:**
- If no task in progress → Propose Task #00021
- If 00021 in progress → Remind to complete before 00022
- If 00021 complete → Propose Task #00022
- And so on...

### 1. **Scan for Incomplete Tasks**

Check tasks directory for work in progress:

```bash
# Find tasks with IN_PROGRESS, BLOCKED, PAUSED status
grep -l "Status:.*IN_PROGRESS\|Status:.*BLOCKED\|Status:.*PAUSED" tasks/*.md

# Get most recent task number
ls -1 tasks/ | grep -E "^[0-9]+" | sort -n | tail -1
```

**Priority order:**
1. 🔴 **BLOCKED** tasks (highest priority - resolve blockers)
2. 🟡 **IN_PROGRESS** tasks (continue current work)
3. 🔵 **PAUSED** tasks (resume if blocker removed)
4. 🟢 **New task** (if all complete)

### 2. **Analyze ROADMAP.md State (ATOMIC MODEL)**

**NEW (2025-11-09):** ROADMAP uses Phase → Zagadnienie → Checkbox structure

```bash
# Extract current phase and status
grep -A 5 "PHASE [0-9].*IN PROGRESS" ROADMAP.md

# Find current Zagadnienie (main topic)
grep -E "^### [0-9]\.[0-9].*IN PROGRESS" ROADMAP.md

# Find incomplete checklist items in current Zagadnienie
grep -A 30 "### [0-9]\.[0-9]" ROADMAP.md | grep "^- \[ \]" | head -10
```

**Extract key information:**
- Current phase (Phase 0-5)
- Current Zagadnienie (e.g., 1.2 Command Registry Architecture)
- Zagadnienie status (e.g., "7/12 tasks complete")
- Completed checkboxes (marked [x])
- Remaining checkboxes (marked [ ])
- Next Zagadnienie in sequence

### 3. **Check Git and CHANGELOG**

```bash
# Verify CHANGELOG up-to-date
git log -5 --oneline

# Check [Unreleased] section
grep -A 20 "## \[Unreleased\]" CHANGELOG.md

# Detect uncommitted work
git status --porcelain
```

**Validation:**
- ✅ All recent commits documented in CHANGELOG
- ✅ No uncommitted changes (clean state)
- ⚠️ If dirty: Suggest commit/cleanup first

### 4. **Decision Logic**

Based on findings, determine next action:

#### **Case A: Incomplete Task Found**

```markdown
🔴 INCOMPLETE TASK DETECTED

Task: #00015 - Project Navigator Panel
Status: IN_PROGRESS
Last Updated: 2025-11-01
Progress: 60% (3/5 checklist items)

Remaining work:
- [ ] Implement tree navigation
- [ ] Add drag-drop support

📋 Proposal: Continue Task #00015

Do you want to:
1. Resume this task
2. Mark as PAUSED and start new task
3. Mark as BLOCKED (specify blocker)

Choose option (1/2/3):
```

#### **Case B: All Tasks Complete - Propose Next (ATOMIC MODEL)**

Cross-reference ROADMAP Zagadnienie with task files:

```markdown
✅ All tasks complete - Ready for next work

📊 Current State:
- Phase: Phase 1 (Core Editor)
- Current Zagadnienie: 1.2 Command Registry Architecture
- Progress: 7/12 tasks complete (58%)
- Last task: #00030 (Format Menu Command Registration) ✅

📋 ROADMAP Analysis (Zagadnienie 1.2):

Current Zagadnienie remaining checkboxes:
- [ ] Create MenuBuilder class (buildFromRegistry, addSeparator, addSubmenu)
- [ ] Replace hardcoded createMenuBar() with MenuBuilder
- [ ] Add ToolbarBuilder class (buildFromRegistry, addSeparator, toggles)
- [ ] Replace hardcoded createToolBar() with ToolbarBuilder
- [ ] Integrate builders into MainWindow (dynamic menu/toolbar creation)

Next Zagadnienie: 1.3 Settings System Enhancement (7 tasks)

🎯 RECOMMENDED NEXT TASK:

**Option 1 (HIGH PRIORITY): Create MenuBuilder class**
- Zagadnienie: 1.2 Command Registry Architecture
- Task will be: #00031_1_2_menu_builder_class.md
- Dependencies: CommandRegistry (✅), Menu registration (✅)
- Estimated time: 90-120 minutes (atomic task)
- Enables: Dynamic menu creation from CommandRegistry

**Option 2 (ALTERNATIVE): Settings System verification tasks**
- Zagadnienie: 1.3 Settings System Enhancement
- Tasks #00036-00042 already exist (verification tasks)
- Estimated time: 30-60 minutes each
- Focus: Verify and fix Settings Dialog issues

**Option 3 (BLOCKED INVESTIGATION): Navigator Panel bugs**
- Investigate Task #00020 bugs (created Navigator but has 6 issues)
- May require breakdown into atomic fixes
- Estimated time: varies (needs analysis first)

Which option? (1/2/3 or propose different task)
```

#### **Case C: Phase Complete - Milestone Transition**

```markdown
🎉 PHASE 0 COMPLETE - Ready for Phase 1

✅ Phase 0 Achievements:
- 8/8 weeks completed
- 18 tasks completed
- All infrastructure in place
- CI/CD: 100% passing

📋 Next Milestone: Phase 1 - Core Editor

Week 9 Tasks (ROADMAP):
1. wxRichTextCtrl integration
2. Basic text editing (bold, italic, underline)
3. Chapter management UI
4. Auto-save system

🎯 RECOMMENDED ACTION:

Create Phase 1 kickoff plan:
1. Create tasks/00019_phase1_kickoff.md
2. Update ROADMAP.md (mark Phase 0 complete)
3. Update CHANGELOG.md (prepare v0.1.0-alpha release)
4. Tag Phase 0 completion in git

Proceed with Phase 1 planning? (yes/no)
```

### 5. **Task Proposal Format**

If creating new task, generate task file preview:

```markdown
📝 PROPOSED TASK #00019

Title: Document Model Serialization (.klh format)
Priority: High
Estimated Time: 4-6 hours
Phase: Phase 0 - Week 7

Objectives:
1. Implement .klh file format (ZIP container)
2. Serialize Document to JSON
3. Save/Load with libzip
4. Add metadata section
5. Unit tests for serialization

Dependencies:
- ✅ Document Model (#00012)
- ✅ JSON support (nlohmann_json)
- ✅ libzip integration

Success Criteria:
- [ ] .klh files save correctly
- [ ] .klh files load without data loss
- [ ] Metadata includes author, title, creation date
- [ ] Tests: 10+ test cases (save, load, corruption handling)

Create this task file? (yes/no)
```

### 6. **User Interaction**

**If incomplete task:**
- Display task details
- Ask: Resume, Pause, or Block?

**If all complete:**
- Show 2-3 options (aligned with ROADMAP)
- Explain rationale for each
- Ask user to choose or propose alternative

**If phase complete:**
- Celebrate milestone
- Propose phase transition steps
- Ask for approval before proceeding

### 7. **Output Summary**

Always end with actionable summary:

```
📊 TASK PLANNING SUMMARY

Current State:
- Phase: 0 (Foundation)
- Last Task: #00018 ✅
- ROADMAP: 75% Phase 0 complete

Recommendation: Option 1 - Document Model Serialization

Next Steps:
1. Create tasks/00019_document_model_serialization.md
2. Implement .klh save/load
3. Update CHANGELOG.md on completion

Estimated Timeline: 4-6 hours
Blocks: Phase 1 Week 9 (requires .klh format working)

Proceed? (yes/no)
```

## Special Cases:

### **Blocked Task Handling**

If task marked BLOCKED:

```markdown
🚨 BLOCKED TASK: #00015 - Project Navigator Panel

Blocker: "Waiting for wxAUI layout decision"
Blocked Since: 2025-11-01 (2 days ago)

Resolution Options:
1. Review blocker - is it still valid?
2. Workaround - implement without blocker resolution
3. Escalate - discuss with user
4. Skip - move to different task

What should we do? (1/2/3/4)
```

### **ROADMAP Drift Detection (ATOMIC MODEL)**

If tasks don't align with ROADMAP Zagadnienie:

```markdown
⚠️ ROADMAP DRIFT DETECTED

Current Zagadnienie (1.2 Command Registry Architecture):
**Status:** 🚀 IN PROGRESS (7/12 tasks complete)

Remaining checkboxes in 1.2:
- [ ] Create MenuBuilder class
- [ ] Replace hardcoded createMenuBar() with MenuBuilder
- [ ] Add ToolbarBuilder class
- [ ] Replace hardcoded createToolBar() with ToolbarBuilder
- [ ] Integrate builders into MainWindow

Last 3 completed tasks:
- #00030: Format Menu Registration (✅ in Zagadnienie 1.2)
- #00029: Edit Menu Registration (✅ in Zagadnienie 1.2)
- #00028: File Menu Registration (✅ in Zagadnienie 1.2)

📊 Analysis:
- Tasks aligned with Zagadnienie 1.2
- Menu registration complete, but MenuBuilder not started
- Should continue with dynamic UI generation (MenuBuilder/ToolbarBuilder)

🎯 Recommendation:
1. Continue Zagadnienie 1.2 (5 tasks remaining)
2. Create Task #00031: MenuBuilder class
3. Complete dynamic UI before moving to Zagadnienie 1.3

Proceed with Task #00031? (yes/no)
```

**Zagadnienie Benefits:**
- Clearer focus (work on one main topic at a time)
- Natural grouping (related tasks in same Zagadnienie)
- Progress tracking (e.g., "7/12 tasks complete in 1.2")
- Prevents task number chaos (checkboxes don't have numbers)

## Configuration:

**Task priority weights:**
1. BLOCKED tasks: 🔴 Priority 1 (resolve first)
2. IN_PROGRESS tasks: 🟡 Priority 2
3. PAUSED tasks: 🔵 Priority 3
4. ROADMAP alignment: 🟢 Priority 4

**Proposal limit:** Maximum 3 options (avoid choice paralysis)
**Lookahead:** Show next 2-3 ROADMAP items for context
