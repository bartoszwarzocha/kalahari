---
description: Save current session state to Serena and Memory MCP
---

**This command activates session-manager agent**

**MANDATORY at end of every work session**

Save complete session state to prevent knowledge loss:

1. **Auto-detect session context:**
   - Parse last git commit message
   - Check current task file (tasks/NNNNN_*.md with status=IN_PROGRESS)
   - Generate session name: `session_YYYY-MM-DD_[task-NNNNN or commit-summary]`
   - Example: `session_2025-11-03_task-00018_bwx-sdk-refactoring`
   - Example: `session_2025-11-03_fix-catch2-threading`

2. **Verify prerequisites:**
   - CHANGELOG.md updated with all changes
   - ROADMAP.md updated (task/phase status)
   - All commits pushed to git
   - No uncommitted work-in-progress
   - If check fails: STOP and report to user (missing CHANGELOG/ROADMAP updates!)

3. **Create Serena memory:**
   - **Session name:** Auto-generated from step 1
   - **Git commits:** List all commits from this session (since last save-session)
   - **Completed work:** Parse CHANGELOG.md [Unreleased], git commits, closed tasks
   - **Decisions made:** Extract from commit messages, task files, code changes
   - **Active tasks:** Current tasks/*.md files with status
   - **Blockers/Issues:** Known problems from commit messages or failed CI/CD
   - **Next session plan:** From task files or ROADMAP.md next milestone

4. **Update Memory MCP graph:**
   - Create entities for new decisions/components (from commits/tasks)
   - Add observations with session date tag
   - Create relations between new and existing entities

5. **Generate verification report:**
   - Session saved as: [filename]
   - Commits included: [count] ([list short hashes])
   - Tasks updated: [list]
   - Memory MCP entities: [count new/updated]
   - CI/CD status: [last run result]

**Implementation:**
1. Activate session-manager agent via Task tool
2. Agent executes all steps above
3. Agent returns verification report

**Output:** "Session checkpoint complete: [session-name]" + verification report
