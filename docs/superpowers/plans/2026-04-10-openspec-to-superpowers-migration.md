# OpenSpec → Superpowers Migration Plan

> **Goal:** Replace OpenSpec task management with Superpowers workflow while preserving Kalahari's agent specialization and project-specific skills.

**Architecture:** Remove OpenSpec ceremony (proposals, delta specs, archival), adopt Superpowers' brainstorming → writing-plans → subagent-driven-development pipeline. Keep existing agents (architect, code-writer, code-editor, ui-designer, code-reviewer, tester) as implementation workers.

---

## Phase 1: Remove OpenSpec Infrastructure

### Task 1: Delete OpenSpec commands

**Files to delete:**
- `.claude/commands/openspec/proposal.md`
- `.claude/commands/openspec/apply.md`
- `.claude/commands/openspec/verify.md`
- `.claude/commands/openspec/explore.md`
- `.claude/commands/openspec/archive.md`

### Task 2: Delete OpenSpec skill

**File to delete:**
- `.claude/skills/openspec-workflow/SKILL.md`

### Task 3: Remove task-manager agent

**File to delete:**
- `.claude/agents/task-manager.md`

**Reason:** Superpowers handles task lifecycle through skills (brainstorming, writing-plans, finishing-a-development-branch). A dedicated task-manager agent is no longer needed — the main orchestrator coordinates via Superpowers skills directly.

### Task 4: Update post_compact.py hook

**File:** `.claude/hooks/post_compact.py`

**Change:** Remove `find_active_openspec()` function and OpenSpec-specific recovery. Replace with plan-aware recovery:

```python
def find_active_plan():
    """Find the most recent implementation plan."""
    plans_dir = os.path.join(PROJECT_DIR, "docs", "superpowers", "plans")
    if not os.path.isdir(plans_dir):
        return None
    plans = sorted(
        [f for f in os.listdir(plans_dir) if f.endswith(".md")],
        reverse=True
    )
    return plans[0] if plans else None
```

Recovery message: `"[POST-COMPACTION RECOVERY] Context was compacted. Active plan: docs/superpowers/plans/{plan}. Re-read this file before continuing."`

### Task 5: Clean up session-state.json

**File:** `.claude/session-state.json`

**Change:** Remove OpenSpec-specific fields (`openspec`, `openspec_status`). The file can remain for other session data or be simplified.

---

## Phase 2: Update Configuration Files

### Task 6: Rewrite workflow.md

**File:** `.claude/rules/workflow.md`

**New content:**

```markdown
# Agent Dispatch & Workflow

Use agents when the task benefits from specialization. Do NOT force dispatch on every message.

## Agents

| Agent | Role |
|-------|------|
| architect | Analyzes code, designs solutions |
| code-writer | Writes NEW code (new files, new classes) |
| code-editor | Modifies EXISTING code |
| ui-designer | Creates UI components (Qt6 widgets) |
| code-reviewer | Code review, quality checks |
| tester | Runs build and tests, reports results |
| devops | CI/CD specialist (standalone) |

## Workflow

Superpowers skills drive the workflow. Agents are dispatched as implementation workers.

```
NEW FEATURE / CHANGE:
  brainstorming → writing-plans → subagent-driven-development → finishing-a-development-branch

IMPLEMENTATION AGENTS:
  - architect           → During brainstorming (analysis, design decisions)
  - code-writer         → New files, new classes
  - code-editor         → Modifications to existing code
  - ui-designer         → Qt6 widgets, dialogs, panels
  - code-reviewer       → Two-stage review (spec compliance + code quality)
  - tester              → Build + tests verification

BUG FIX:
  systematic-debugging → test-driven-development → verification-before-completion
```

**CRITICAL:** Never skip code-reviewer or tester! Work is NOT complete until both pass.
```

### Task 7: Update naming.md

**File:** `.claude/rules/naming.md`

**Change:** Remove the "OpenSpec (Task Management)" section (lines 21-26). Remove `"openspec/**"` from the paths frontmatter.

### Task 8: Update project-brief.txt

**File:** `.claude/context/project-brief.txt`

**Changes:**
- Remove "MainWindow Architecture (OpenSpec #00038)" — replace with "MainWindow Architecture" (keep the content, drop the reference)
- Replace the "OpenSpec (Task Management)" section with:

```
## Task Management
- Plans: docs/superpowers/plans/
- Specs: docs/superpowers/specs/
- Workflow: brainstorming → writing-plans → implementation → finishing
```

### Task 9: Update CLAUDE.md

**File:** `CLAUDE.md`

**Change:** Remove any OpenSpec-specific blocks (e.g., `<!-- OPENSPEC:START -->` ... `<!-- OPENSPEC:END -->`). The `@.claude/context/project-brief.txt` reference stays.

### Task 10: Update settings.json permissions

**File:** `.claude/settings.json`

**Change:** Replace `openspec/**` permissions with `docs/superpowers/**`:
- `Read(docs/superpowers/**)` 
- `Edit(docs/superpowers/**)`
- `Write(docs/superpowers/**)`

Keep `Read(openspec/archive/**)` temporarily for historical reference.

---

## Phase 3: Create Directory Structure

### Task 11: Create Superpowers directories

```
docs/superpowers/
├── specs/      # Design documents from brainstorming
└── plans/      # Implementation plans from writing-plans
```

---

## Phase 4: Migrate Pending Proposals

### Task 12: Convert 00044B-F to design docs

Convert the 5 pending OpenSpec proposals into a single Superpowers design document. These form a cohesive feature chain (Text Styling System) and should be treated as one brainstormed design with a phased implementation plan.

**Output:** `docs/superpowers/specs/2026-04-10-text-styling-system-design.md`

**Content:** Consolidate the goals, scope, acceptance criteria, and dependencies from proposals 00044B through 00044F into the Superpowers design doc format. Drop delta specs — they add no value for unstarted work.

### Task 13: Convert to implementation plan

**Output:** `docs/superpowers/plans/2026-04-10-text-styling-system.md`

**Content:** Convert the tasks from 00044B-F into Superpowers plan format with:
- Exact file paths and line references
- Code snippets for each step
- Test commands and expected output
- Bite-sized steps (2-5 min each)

This is the largest task — the plan must be self-contained and executable by subagents.

### Task 14: Move pending proposals to archive

Move `openspec/changes/00044B-F` to `openspec/archive/` with status changed to `MIGRATED` (not DEPLOYED — they were never implemented).

---

## Phase 5: Update Memory & Documentation

### Task 15: Update MEMORY.md

Remove OpenSpec references. Update framework description to reflect Superpowers workflow.

### Task 16: Update CHANGELOG.md

Add entry:
```
- **Workflow Migration:** OpenSpec → Superpowers - 2026-04-10
  - Replaced OpenSpec task management with Superpowers skills workflow
  - Migrated 5 pending proposals (00044B-F) to Superpowers plan
  - Removed: task-manager agent, openspec commands, openspec-workflow skill
  - Added: docs/superpowers/ for specs and plans
```

### Task 17: Archive openspec/ root files

Keep `openspec/archive/` as read-only historical reference. Remove or archive:
- `openspec/AGENTS.md` → no longer needed
- `openspec/project.md` → no longer needed  
- `openspec/specs/` → merge any useful content into design docs
- `openspec/changes/` → should be empty after Task 14

---

## Verification

After all tasks:
- [ ] No files in `openspec/changes/`
- [ ] `docs/superpowers/specs/` has text-styling design doc
- [ ] `docs/superpowers/plans/` has text-styling plan
- [ ] `scripts/build_windows.bat Debug` builds successfully
- [ ] No broken references to OpenSpec commands in `.claude/`
- [ ] Post-compact hook works without OpenSpec
- [ ] Superpowers skills load correctly (test: invoke `superpowers:brainstorming`)

---

## What We Keep

- **openspec/archive/** — 47 completed tasks, read-only historical record
- **Existing agents** — architect, code-writer, code-editor, ui-designer, code-reviewer, tester, devops
- **Project skills** — architecture-patterns, kalahari-coding, qt6-desktop-ux, quality-checklist, testing-procedures
- **CHANGELOG.md, ROADMAP.md** — continue maintaining manually
- **Historical OpenSpec references** in ROADMAP.md and code comments — leave as-is

## What We Remove

- **OpenSpec commands** (5 files in .claude/commands/openspec/)
- **OpenSpec skill** (.claude/skills/openspec-workflow/)
- **task-manager agent** (.claude/agents/task-manager.md)
- **OpenSpec sections** from naming.md, project-brief.txt, CLAUDE.md
- **Delta specs system** — no longer needed
- **Living specs** (openspec/specs/) — replaced by design docs
