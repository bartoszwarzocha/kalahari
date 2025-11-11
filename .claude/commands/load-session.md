---
description: Intelligent session restore with mode-aware verification
---

**This command activates session-manager agent**

**MANDATORY at start of every work session**

## Intelligent Session Restoration

Automatically detects session save mode from metadata and adjusts verification depth accordingly.

**Single command - no parameters needed!**

```bash
/load-session    # Always use this - agent detects mode automatically
```

---

## How It Works

1. **Find last session:**
   - Scan `.serena/memories/session_*.md` files
   - Sort by date (newest first)
   - Parse frontmatter metadata

2. **Parse session metadata:**
   ```yaml
   mode: full                   # Detected: full verification saved
   saved_at: 2025-11-05T14:30:00
   git_pushed: true             # Expect pushed commits
   ci_verified: true            # CI/CD was verified
   changelog_updated: true      # Docs were updated
   task_completed: 00019        # Task milestone reached
   ```

3. **Intelligent mode-aware restoration:**

   | Session Mode | What load-session Does |
   |--------------|------------------------|
   | **quick**    | Light check: git gaps, basic consistency |
   | **sync**     | Medium check: git gaps, CI/CD status, sync verification |
   | **full**     | Deep check: git gaps, docs consistency, CI/CD results, full graph |

4. **ALWAYS check git gaps** (critical for all modes):
   - Get last session date/time
   - List all commits since session
   - **IF NEW COMMITS FOUND:** Report as undocumented gap
   - Ask user to explain missing context

---

## Mode 1: Quick Session Restore ⚡ ~5-10 seconds

**Detected when:** `mode: quick` in frontmatter

**What it verifies:**

1. **Session state:**
   - Last checkpoint: 2025-11-05 14:30
   - Context: Task #00019 WIP
   - Commits: 1 local (abc1234)

2. **Git gap detection:**
   ```bash
   git log --since="2025-11-05T14:30:00" --oneline
   ```
   - **IF EMPTY:** ✅ No gaps
   - **IF NEW COMMITS:** ⚠️ Gap detected, ask user

3. **Output:**
   ```
   ⚡ Quick session restored (~6s)
   Last session: 2025-11-05 14:30 (quick checkpoint)
   Context: Task #00019 - Text Editor Integration (WIP)

   ✅ Git status: Clean (no new commits)
   ✅ Session: Checkpoint loaded

   📋 Next steps (from last session):
   - Continue Task #00019 implementation
   - Run tests before next commit

   💡 Note: Last save was quick mode (local only, not pushed)
   ```

**Time:** ~5-10 seconds
**Verification:** Light (git gaps + basic state)

---

## Mode 2: Sync Session Restore 🚀 ~15-25 seconds

**Detected when:** `mode: sync` in frontmatter

**What it verifies:**

1. **Everything from Quick Mode**

2. **Git push verification:**
   - Check if commits mentioned in session exist on remote
   - Verify session commits: abc1234...xyz7890
   - **IF MISSING:** ⚠️ Commits not found on GitHub (force push happened?)

3. **CI/CD status check:**
   - Find CI/CD run triggered at session save time
   - Report final status (if completed)
   - **IF RUNNING:** Report "still in progress"
   - **IF FAILED:** Extract failure summary from logs

4. **Output:**
   ```
   🚀 Sync session restored (~18s)
   Last session: 2025-11-05 18:45 (synced to GitHub)
   Context: Task #00019 - Subtask complete
   Commits: 5 pushed (abc1234...xyz7890)

   ✅ Git status:
   - Remote: ✅ All 5 commits found on GitHub
   - Local: Clean (no new commits since session)

   ✅ CI/CD status (from session):
   - Linux: ✅ PASS (3m 16s)
   - macOS: ✅ PASS (3m 36s)
   - Windows: ✅ PASS (9m 34s)

   ✅ Session: Sync state loaded

   📋 Next steps:
   - Continue next subtask
   - Check ROADMAP.md for priorities

   💡 Note: Last save was sync mode (pushed but not fully verified)
   ```

**Time:** ~15-25 seconds
**Verification:** Medium (git gaps + remote verification + CI/CD status)

---

## Mode 3: Full Session Restore 📋 ~30-60 seconds

**Detected when:** `mode: full` in frontmatter

**What it verifies:**

1. **Everything from Sync Mode**

2. **CHANGELOG/ROADMAP consistency:**
   - Read frontmatter: `changelog_updated: true`, `task_completed: 00019`
   - Verify CHANGELOG.md [Unreleased] has entries for session commits
   - Verify ROADMAP.md Task #00019 status matches frontmatter
   - **IF MISMATCH:** ⚠️ Report inconsistency

3. **CI/CD deep verification:**
   - Verify all 3 platforms passed (Linux, macOS, Windows)
   - Check test counts, code coverage (if available)
   - Verify no regressions since session
   - **IF NEW FAILURES:** ⚠️ Report regression

4. **Output:**
   ```
   📋 Full session restored (~45s)
   Last session: 2025-11-05 14:30 (full verification)
   Context: Task #00019 - Text Editor Integration COMPLETE ✅
   Commits: 8 pushed and verified (abc1234...xyz7890)

   ✅ Git status:
   - Remote: ✅ All 8 commits on GitHub
   - Local: Clean (no new commits)

   ✅ Documentation consistency:
   - CHANGELOG.md: ✅ All session commits documented
   - ROADMAP.md: ✅ Task #00019 marked COMPLETE
   - Frontmatter: ✅ Matches reality

   ✅ CI/CD verification:
   - Session CI/CD: ✅ All passed (3m 45s total)
   - Current status: ✅ No regressions
   - Linux: ✅ PASS (3m 16s)
   - macOS: ✅ PASS (3m 36s)
   - Windows: ✅ PASS (9m 34s)

   ✅ Phase status:
   - Phase 1: IN_PROGRESS
   - Next milestone: Project Navigator panel

   📋 Next steps:
   - Task #00020: Chapter navigation
   - Begin Project Navigator implementation

   🎉 Task #00019 milestone verified - ready to continue!
   ```

**Time:** ~30-60 seconds
**Verification:** Full (git + docs + graph + CI/CD + phase status)

---

## Git Gap Detection (All Modes)

**Critical feature - ALWAYS executed regardless of mode!**

### What is a "gap"?

A gap occurs when commits exist between sessions without session documentation.

**Example scenario:**
```
2025-11-05 14:30 - /save-session (Task #00019 WIP)
2025-11-05 15:45 - commit abc1234 "feat: Add menu integration"
2025-11-05 16:20 - commit def5678 "fix: ViewMode resolution"
2025-11-05 18:00 - /load-session (next day)
                    ⚠️ GAP DETECTED: 2 commits without session!
```

### How gaps are detected:

```bash
# Get session timestamp
session_time=$(grep "saved_at:" session.md | cut -d' ' -f2)

# Find commits since session
git log --since="$session_time" --oneline

# If output is not empty → GAP DETECTED
```

### Gap report format:

```
⚠️  GAP DETECTED: Undocumented commits found!

Commits since last session (2025-11-05 14:30):
- abc1234 (2 hours ago) "feat: Add menu integration"
- def5678 (1 hour ago) "fix: ViewMode resolution"

❓ What happened in these commits?

This work is not documented in any session memory!
Please explain what was done so I can:
1. Create a recovery session memory
2. Ensure continuity for future sessions

Type your explanation or "skip" to ignore:
```

### User response options:

**Option 1: Explain (RECOMMENDED)**
```
User: I added Edit menu integration and fixed Windows build error
Agent: ✅ Creating recovery session...
        ✅ Recovery memory saved: session_2025-11-05_recovery_gap

        Now continuing with normal session restore...
```

**Option 2: Skip (NOT RECOMMENDED)**
```
User: skip
Agent: ⚠️  Gap remains undocumented - knowledge may be lost!
        Continuing session restore...
```

---

## No Session Found (First Run)

If no previous session exists:

```
📅 No previous session found

This appears to be the first session in this project.

✅ Project state:
- Git: X commits total
- Tasks: Y in progress
- Phase: Z status

💡 Starting fresh session - use /save-session to create first checkpoint
```

---

## Implementation

**This command activates session-manager agent via Task tool**

```
Agent execution:
1. Find most recent session_*.md file
2. Parse frontmatter to detect mode
3. Execute mode-specific verification workflow
4. ALWAYS check for git gaps (critical!)
5. Generate mode-appropriate report
6. Handle gaps if detected
7. Return restoration summary
```

**Frontmatter detection:**

```python
with open(".serena/memories/session_2025-11-05_task-00019.md") as f:
    content = f.read()

# Parse YAML frontmatter
frontmatter = yaml.load(content.split("---")[1])

mode = frontmatter["mode"]  # quick | sync | full
saved_at = frontmatter["saved_at"]
git_pushed = frontmatter["git_pushed"]
ci_verified = frontmatter["ci_verified"]

# Adjust verification depth based on mode
if mode == "full":
    verify_docs()
    verify_ci_deep()
    verify_graph_full()
elif mode == "sync":
    verify_remote()
    verify_ci_status()
elif mode == "quick":
    verify_basic()

# ALWAYS check gaps (critical!)
check_git_gaps(saved_at)
```

---

## Error Handling

**No session file found:**
- Report: "First session - no previous state to restore"
- Show current project status
- Suggest creating first checkpoint

**Corrupted frontmatter:**
- Report: "Session metadata corrupted"
- Fall back to quick mode verification
- Ask user if they want to investigate

**Git gaps with no explanation:**
- Report gap details
- Mark as "unresolved gap"
- Continue session (don't block work)
- Remind user to document gaps

**Network failures (sync/full mode):**
- Report: "⚠️ Cannot verify remote/CI (offline?)"
- Fall back to local verification only
- Continue session (don't block work)

**CHANGELOG/ROADMAP mismatch (full mode):**
- Report specific inconsistencies
- Don't auto-fix (require user decision)
- Continue session with warning

---

## Examples

**Example 1: Quick session, no gaps**
```
User: /load-session
Agent: ⚡ Quick session restored (~6s)
        Last: 2025-11-05 14:30 (Task #00019 WIP)
        ✅ No gaps - ready to continue!
```

**Example 2: Sync session, with gap**
```
User: /load-session
Agent: 🚀 Sync session restored (~18s)
        Last: 2025-11-05 18:45 (5 commits pushed)

        ⚠️  GAP: 2 new commits since session
        [shows commits]

        What happened? (explain/skip)
User: Fixed Windows build errors
Agent: ✅ Recovery session created
        Continuing...
```

**Example 3: Full session, all verified**
```
User: /load-session
Agent: 📋 Full session restored (~45s)
        Last: 2025-11-05 14:30 (Task #00019 COMPLETE)

        ✅ Comprehensive verification:
        - Git: ✅ 8 commits on remote
        - Docs: ✅ CHANGELOG/ROADMAP consistent
        - CI/CD: ✅ All platforms passing
        - Memory: ✅ Graph verified (5 entities, 8 relations)

        🎉 Milestone verified - ready for Task #00020!
```

---

## Output

- **Quick mode:** Light report (~6s)
- **Sync mode:** Medium report with remote/CI (~18s)
- **Full mode:** Comprehensive report with full verification (~45s)

All modes ALWAYS check for git gaps and offer recovery mechanism.

**Key benefit:** User never needs to remember which mode they used - system detects and adapts automatically!
