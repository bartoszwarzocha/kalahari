---
description: Load last session state from Serena and Memory MCP
---

**This command activates session-manager agent**

**MANDATORY at start of every work session**

Restore complete project state to continue work:

1. **Find last session:**
   - Query Serena for most recent `session_YYYY-MM-DD_*.md`
   - Extract session date and context (task/commit)
   - Display: "Last session: YYYY-MM-DD - [context]"

2. **Load session state:**
   - Read Serena memory
   - Extract: completed work, decisions, blockers, **next steps**
   - Display summary to user

3. **Detect changes since last session:**
   - Git commits since session date (if any)
   - CHANGELOG.md updates since session
   - Task files status changes
   - **GAP DETECTION:** Commits without session = LOST CONTEXT

4. **Verify Memory MCP consistency:**
   - Read graph entities from last session
   - Check if all decisions/components documented
   - Highlight missing context

5. **Present restoration:**

```
📅 Last session: 2025-11-03 (Task #00018 - bwx_sdk refactoring)

✅ Completed:
- [list from Serena]

⚠️ Blockers found:
- [list from Serena]

📋 Next steps (from last session):
- [list from Serena]

🔍 Changes since last session:
- X new commits [⚠️ NOT DOCUMENTED if gap detected]
- Y task files updated

❓ Gaps detected:
- [list commits without session documentation]
- ACTION REQUIRED: User must explain what happened
```

6. **Recovery mode (if gaps):**
   - Ask user: "Commits found without session documentation. What happened in those sessions?"
   - Create recovery memory with user's explanation
   - Update Memory MCP with recovered context

**Implementation:**

1. Activate session-manager agent via Task tool
2. Agent executes all steps above
3. Agent returns restoration summary

**Output:** Complete state restoration + gap report + action items
