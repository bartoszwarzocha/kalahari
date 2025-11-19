---
description: Save session with intelligent mode detection (quick/sync/full)
argument-hint: [--sync | --full]
---

**This command activates session-manager agent**

**MANDATORY at end of every work session**

## Three-Tier Session Save System

Save session with mode automatically embedded in metadata for intelligent restoration.

### Quick Mode Selection

```text
┌──────────────────────────────────────────────────┐
│ Use case                   │ Command             │
├──────────────────────────────────────────────────┤
│ Hourly checkpoint (WIP)    │ /save-session       │
│ Before risky changes       │ /save-session       │
│ Coffee break / lunch       │ /save-session       │
│ End of day (unfinished)    │ /save-session --sync│
│ Subtask complete           │ /save-session --sync│
│ Task milestone complete    │ /save-session --full│
│ Phase complete             │ /save-session --full│
│ Before PR / code review    │ /save-session --full│
└──────────────────────────────────────────────────┘
```

---

## Mode 1: Quick (Default) ⚡ ~10-15 seconds

**Trigger:** `/save-session` (no arguments) or `/save-session --quick`

**Use:** Frequent checkpoints, WIP snapshots, offline work

**What it does:**

1. **Auto-detect session context:**
   - Parse last git commit message
   - Check current task file (tasks/NNNNN_*.md with status=IN_PROGRESS)
   - Generate session name: `session_YYYY-MM-DD_[task-NNNNN or commit-summary]`
   - Example: `session_2025-11-05_task-00019_WIP`

2. **Create local git commit (if uncommitted changes):**
   - Auto-stage modified files (exclude temp files)
   - Create WIP commit: "WIP: [session context] - checkpoint"
   - **NO PUSH** (local only, saves 5-30s network time)

3. **Create lightweight Serena memory with frontmatter:**
   ```markdown
   ---
   mode: quick
   saved_at: 2025-11-05T14:30:00
   duration: 12s
   git_commits: 1
   git_pushed: false
   ci_verified: false
   changelog_updated: false
   roadmap_updated: false
   ---

   # Session: session_2025-11-05_task-00019_WIP

   ## Context
   - Last 3 commits: [short hashes + messages]
   - Current task: Task #00019 (IN_PROGRESS)

   ## Status
   Quick checkpoint - work in progress

   ## Next Steps
   - Continue Task #00019 implementation
   ```

4. **Output:**
   ```
   ⚡ Quick checkpoint saved (~12s)
   Session: session_2025-11-05_task-00019_WIP
   Commits: 1 local (abc1234)
   Status: Work in progress

   💡 TIP: Use --sync to push to GitHub, --full for milestone verification
   ```

**Time:** ~10-15 seconds
**Network:** None (offline-capable)
**Verification:** Minimal (git commit only)

---

## Mode 2: Sync (--sync) 🚀 ~20-40 seconds

**Trigger:** `/save-session --sync` or `/save-session sync`

**Use:** End of day, subtask complete, cloud backup needed

**What it does:**

1. **Everything from Quick Mode** (steps 1-2)

2. **Git push to GitHub:**
   - Push all local commits to origin/main
   - Trigger CI/CD (but don't wait for results)
   - Verify push success (handle network errors gracefully)

3. **Enhanced Serena memory with frontmatter:**
   ```markdown
   ---
   mode: sync
   saved_at: 2025-11-05T18:45:00
   duration: 28s
   git_commits: 5
   git_pushed: true
   ci_verified: false
   ci_triggered: true
   changelog_updated: false
   roadmap_updated: false
   ---

   # Session: session_2025-11-05_task-00019_subtask-complete

   ## Context
   - Commits pushed: 5 (abc1234...xyz7890)
   - GitHub push: ✅ Successful
   - CI/CD: Triggered at 18:45

   ## Completed Work
   [Brief summary from commit messages]

   ## Next Steps
   - Check CI/CD results with /check-ci
   - Continue next subtask
   ```

4. **Output:**
   ```
   🚀 Session synced to GitHub (~28s)
   Session: session_2025-11-05_task-00019_subtask-complete
   Commits: 5 pushed (abc1234...xyz7890)
   GitHub: ✅ Push successful
   CI/CD: Triggered (check later with /check-ci)

   💡 TIP: Use --full for comprehensive verification with CI/CD wait
   ```

**Time:** ~20-40 seconds
**Network:** Required (git push)
**Verification:** Push success + CI/CD trigger

---

## Mode 3: Full (--full) 📋 ~3-6 minutes

**Trigger:** `/save-session --full` or `/save-session full`

**Use:** Task complete, phase milestone, production checkpoint

**What it does:**

1. **Everything from Sync Mode** (steps 1-2 + git push)

2. **Comprehensive CHANGELOG/ROADMAP verification:**
   - Check CHANGELOG.md [Unreleased] section for commits since last full save
   - Check ROADMAP.md task status matches reality

   **IF MISSING ENTRIES:**
   - Extract commit messages from git log
   - Auto-generate CHANGELOG entries (categorized: Added/Fixed/Changed/Removed)
   - Auto-generate ROADMAP updates (task checkboxes, status)
   - Present proposals to user:

   ```
   ⚠️  Documentation missing for 3 commits

   Proposed CHANGELOG entries:

   ### [Unreleased]

   #### Added
   - Menu integration for EditorWidget (Cut/Copy/Paste/SelectAll)
   - Editor Mode submenu (Full/Page/Typewriter/Publisher)

   #### Fixed
   - ViewMode type resolution error on Windows
   - Editor margin removed (fills panel)

   Source: Commits abc1234, def5678, ghi9012

   Proposed ROADMAP updates:
   - Task #00019: Update status to COMPLETE
   - Phase 1: Mark "Text Editor Integration" as ✅

   Accept these updates? (yes/edit/skip)
   ```

   **User options:**
   - `yes` → Agent commits and pushes docs
   - `edit` → User provides corrections, agent applies
   - `skip` → Session saved, but docs remain incomplete (NOT RECOMMENDED)

3. **CI/CD monitoring:**
   - Wait for GitHub Actions to start (30s timeout)
   - Monitor CI/CD progress (Linux, macOS, Windows)
   - Poll status every 30s (max 10 minutes)
   - Report results:
     - ✅ All pass (Linux, macOS, Windows)
     - ⚠️ Partial (some platforms fail)
     - ❌ All fail
   - If failures: Extract error logs, add to session blockers

4. **Enhanced Serena memory with frontmatter:**
   ```markdown
   ---
   mode: full
   saved_at: 2025-11-05T14:30:00
   duration: 4m 30s
   git_commits: 8
   git_pushed: true
   ci_verified: true
   ci_status: all_pass
   ci_duration: 3m 45s
   changelog_updated: true
   roadmap_updated: true
   task_completed: 00019
   phase_status: phase1_in_progress
   ---

   # Session: session_2025-11-05_task-00019_complete

   ## Context
   - Task: #00019 - Text Editor Integration (Days 11-12)
   - Status: ✅ COMPLETE
   - Commits: 8 pushed and verified

   ## Completed Work
   - Menu integration for EditorWidget (Edit menu)
   - Editor Mode submenu (4 view modes)
   - ViewMode type fixes for Windows build
   - Full panel integration (removed margins)

   ## Decisions Made
   - Editor fills entire panel (no margins)
   - ViewMode exposed via menu (no toolbar yet)

   ## Verification
   - ✅ Linux: PASS (3m 16s)
   - ✅ macOS: PASS (3m 36s)
   - ✅ Windows: PASS (9m 34s)

   ## Next Steps
   - Task #00020: Chapter navigation
   - Phase 1 milestone: Project Navigator panel
   ```

5. **Comprehensive verification report:**
   ```
   📋 Full verification complete (~4m 30s)
   Session: session_2025-11-05_task-00019_complete

   ✅ Git:
   - Commits: 8 pushed (abc1234...xyz7890)
   - GitHub: Push successful

   ✅ Documentation:
   - CHANGELOG.md: Updated (4 entries added)
   - ROADMAP.md: Task #00019 marked COMPLETE

   ✅ Session Memory:
   - Serena: session_2025-11-05_task-00019_complete (2.8 KB)

   ✅ CI/CD (waited 3m 45s):
   - Linux: ✅ PASS (3m 16s)
   - macOS: ✅ PASS (3m 36s)
   - Windows: ✅ PASS (9m 34s)

   🎉 Task #00019 checkpoint complete - ready for next milestone!
   ```

**Time:** ~3-6 minutes
**Network:** Required (git push + CI/CD monitoring)
**Verification:** Full (docs + tests + builds + cross-platform)

---

## Parameter Detection

Agent detects mode from `$1` argument:

```bash
if [ -z "$1" ] || [ "$1" = "--quick" ]; then
  mode="quick"
elif [ "$1" = "--sync" ] || [ "$1" = "sync" ]; then
  mode="sync"
elif [ "$1" = "--full" ] || [ "$1" = "full" ]; then
  mode="full"
else
  echo "Invalid mode: $1"
  echo "Usage: /save-session [--sync | --full]"
  exit 1
fi
```

---

## Error Handling

**Network failures (--sync/--full):**
- Gracefully degrade to quick mode
- Save session locally
- Report: "⚠️ Network error - session saved locally, push manually later"

**CHANGELOG missing (--full):**
- Auto-generate proposals from commits
- Present to user for approval
- Never block session save

**CI/CD failures (--full):**
- Report errors with links to logs
- Mark session as "verified_with_failures"
- Add failures to "Blockers" section
- Don't block session save

**Uncommitted changes (quick):**
- Auto-stage and commit as WIP
- Never require manual staging

---

## Implementation

**This command activates session-manager agent via Task tool**

```
Agent execution:
1. Parse $1 argument → detect mode
2. Execute mode-specific workflow
3. Generate mode-appropriate report
4. Save session with frontmatter metadata
5. Return verification report
```

**Frontmatter fields:**

```yaml
mode: quick | sync | full
saved_at: ISO 8601 timestamp
duration: Human readable (e.g., "28s", "4m 30s")
git_commits: Count of commits in session
git_pushed: boolean
ci_verified: boolean (only true for --full with passing CI)
ci_status: all_pass | partial_fail | all_fail (only for --full)
ci_duration: Human readable (only for --full)
changelog_updated: boolean
roadmap_updated: boolean
task_completed: Task number (e.g., "00019") or null
phase_status: Current phase (e.g., "phase1_in_progress")
```

---

## Examples

**Example 1: Hourly checkpoint**
```
User: /save-session
Agent: ⚡ Quick checkpoint saved (~12s)
        Session: session_2025-11-05_task-00019_WIP
        Commits: 1 local (abc1234)
        Status: Work in progress
```

**Example 2: End of day**
```
User: /save-session --sync
Agent: 🚀 Session synced to GitHub (~25s)
        Session: session_2025-11-05_end-of-day
        Commits: 5 pushed
        GitHub: ✅ Push successful
        CI/CD: Triggered
```

**Example 3: Task complete with auto-docs**
```
User: /save-session --full
Agent: 📋 Checking prerequisites...
        ⚠️  CHANGELOG missing entries for 3 commits

        [shows auto-generated proposals]

        Accept? (yes/edit/skip)
User: yes
Agent: ✅ Documentation committed and pushed
        ✅ Waiting for CI/CD (~3-5 min)...
        [monitors builds]
        ✅ All platforms passing!

        📋 Full verification complete (~4m 30s)
        [comprehensive report]
        🎉 Ready for next milestone!
```

---

## Output

- **Quick:** Lightweight confirmation (~12s)
- **Sync:** Push verification + CI/CD trigger (~25s)
- **Full:** Comprehensive report with CI/CD monitoring (~4m 30s)

All modes save session with frontmatter metadata for intelligent restoration by `/load-session`.
