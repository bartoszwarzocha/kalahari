---
description: Optimize Serena memory by consolidating and removing outdated files
---

**Smart Serena memory optimization command**

**When to use:**
- Serena memories grow too large (>10 files or >150KB total)
- Environment load becomes noticeable
- After major milestones (phase completion, multiple tasks done)
- Before important work sessions (clean slate)

**What this command does:**

1. **Analyze current memory state:**
   - List all Serena memory files with sizes
   - Categorize by type:
     - **Session snapshots** (session_YYYY-MM-DD_*)
     - **Task archives** (task_NNNNN_*)
     - **Strategic documents** (phase_*, project_status_*, *_MASTER, *_decisions)
     - **SDK/Integration docs** (bwx_sdk_*, *_template_*)
   - Calculate total size and file count
   - Report findings to user

2. **Identify cleanup candidates:**
   - **Old session files:** Keep only 2 most recent, archive/delete older ones
   - **Completed task files:** Consolidate into phase archives
   - **Duplicate information:** Detect overlapping content between files
   - **Outdated snapshots:** Files with information now in CHANGELOG/ROADMAP
   - Present cleanup plan to user for approval

3. **Execute approved cleanup:**
   - **Consolidate tasks:** Merge completed task files into phase archives
     - Example: task_00008, task_00009, task_00011 → phase0_tasks_archive
   - **Delete old sessions:** Remove session files older than current session
   - **Archive strategic docs:** Keep only latest version of project_status files
   - **Preserve critical files:** NEVER delete *_MASTER, *_decisions, *_complete files

4. **Safety rules (NEVER violate):**
   - ❌ NEVER delete without user approval
   - ❌ NEVER delete *_MASTER files (master strategies)
   - ❌ NEVER delete *_decisions files (architectural decisions)
   - ❌ NEVER delete *_complete files (phase completion records)
   - ❌ NEVER delete current session file
   - ✅ ALWAYS create consolidated archives before deleting sources
   - ✅ ALWAYS verify archive completeness before deletion
   - ✅ ALWAYS report what was deleted and what was kept

5. **Generate cleanup report:**
   - Files deleted: [count] ([list names])
   - Files consolidated: [count] into [archive names]
   - Files kept: [count] ([categories])
   - Size reduction: [before] → [after] (-XX%)
   - Memory count: [before] → [after] (-XX files)
   - Estimated performance improvement: [qualitative assessment]

**Example workflow:**

```
User: /cleanup-serena

Agent:
📊 Serena Memory Analysis:
- Total: 14 files, 144 KB, 4,017 lines
- Session snapshots: 5 files (oldest: 2025-10-31)
- Task files: 4 files (tasks #00008, #00009, #00011)
- Strategic docs: 5 files (MASTER, decisions, templates)

🧹 Cleanup Plan:
1. Delete 4 old session files (keep only current)
2. Consolidate 4 task files → phase0_tasks_completed_archive
3. Keep 5 strategic docs (no duplicates detected)

Expected result: 14 → 7 files (-50%), ~144 KB → ~100 KB (-30%)

Proceed? (User confirms)

✅ Cleanup Complete:
- Deleted: 4 session files
- Consolidated: 4 task files → 1 archive
- Kept: 7 strategic files
- Result: 7 files, ~100 KB (-31% size, -50% file count)
```

**Implementation:**
1. Use `mcp__serena__list_memories` to get current state
2. Read file sizes and analyze content structure
3. Generate cleanup plan with clear categories
4. Wait for user approval ("Wykonaj", "Proceed", "Yes")
5. Execute deletions using `mcp__serena__delete_memory`
6. Create consolidations using `mcp__serena__write_memory`
7. Verify final state with `mcp__serena__list_memories`
8. Report results

**Output:** Cleanup report with before/after metrics and kept/deleted file lists
