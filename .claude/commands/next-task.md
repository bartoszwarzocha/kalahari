---
description: Analyze ROADMAP and tasks to propose next work item
---

**Smart task planning based on project state**

**Automatically detects incomplete work or proposes next logical task**

## Execution Steps:

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

### 2. **Analyze ROADMAP.md State**

```bash
# Extract current phase and status
grep -A 5 "Current Status:" ROADMAP.md

# Find incomplete checklist items in current phase
grep "^- \[ \]" ROADMAP.md | head -10
```

**Extract key information:**
- Current phase (Phase 0-5)
- Current week number
- Completed tasks (checked boxes)
- Remaining tasks (unchecked boxes)
- Next phase preparation items

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

#### **Case B: All Tasks Complete - Propose Next**

Cross-reference ROADMAP with task files:

```markdown
✅ All tasks complete - Ready for next work

📊 Current State:
- Phase: Phase 0 (Foundation)
- Progress: Week 6/8 (75%)
- Last task: #00018 (bwx_sdk refactoring) ✅

📋 ROADMAP Analysis:

Phase 0 remaining items:
- [ ] Document Model serialization (.klh format)
- [ ] Unit tests for core modules
- [ ] Cross-platform build verification

🎯 RECOMMENDED NEXT TASK:

**Option 1 (HIGH PRIORITY): Document Model Serialization**
- Align with: ROADMAP Phase 0, Week 7
- Dependencies: Document Model (✅ completed in #00012)
- Estimated time: 4-6 hours
- Creates foundation for: .klh file save/load

**Option 2 (ALTERNATIVE): Unit Tests for Plugin System**
- Align with: ROADMAP Phase 0, Week 7
- Dependencies: Plugin Manager (✅), Extension Points (✅)
- Estimated time: 3-4 hours
- Increases test coverage to 80%+

**Option 3 (PREPARATION): Phase 1 Planning**
- Align with: ROADMAP Phase 1 preparation
- Create Phase 1 Week 1 tasks (wxRichTextCtrl integration)
- Estimated time: 1-2 hours
- Enables smooth Phase 0 → Phase 1 transition

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

### **ROADMAP Drift Detection**

If tasks don't align with ROADMAP:

```markdown
⚠️ ROADMAP DRIFT DETECTED

ROADMAP says (Phase 0 Week 7):
- [ ] Document Model serialization
- [ ] Unit tests

Last 3 completed tasks:
- #00018: bwx_sdk refactoring (NOT in ROADMAP)
- #00016: TipTap integration (REJECTED)
- #00014: wxRichTextCtrl (REJECTED)

📊 Analysis:
- Exploration phase (evaluating rich text options)
- ROADMAP not updated with research tasks
- Core Phase 0 items still pending

🎯 Recommendation:
1. Update ROADMAP.md to reflect actual progress
2. Return to Phase 0 core tasks
3. Complete foundation before Phase 1

Proceed with ROADMAP update? (yes/no)
```

## Configuration:

**Task priority weights:**
1. BLOCKED tasks: 🔴 Priority 1 (resolve first)
2. IN_PROGRESS tasks: 🟡 Priority 2
3. PAUSED tasks: 🔵 Priority 3
4. ROADMAP alignment: 🟢 Priority 4

**Proposal limit:** Maximum 3 options (avoid choice paralysis)
**Lookahead:** Show next 2-3 ROADMAP items for context
