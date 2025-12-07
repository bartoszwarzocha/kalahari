---
description: Save session state to .claude/session-state.json
argument-hint: [--sync | --full]
---

# Save Session Command

**MANDATORY at end of every work session**

## Usage

```bash
/save-session          # Quick checkpoint (~10-15s)
/save-session --sync   # Push to GitHub (~20-40s)
/save-session --full   # Full verification (~3-6min)
```

## Mode Selection Guide

| Use Case                  | Command              |
|---------------------------|----------------------|
| Hourly checkpoint (WIP)   | `/save-session`      |
| Before risky changes      | `/save-session`      |
| End of day (unfinished)   | `/save-session --sync` |
| Subtask complete          | `/save-session --sync` |
| Task milestone complete   | `/save-session --full` |
| Phase complete            | `/save-session --full` |

---

## Mode 1: Quick (Default) ~10-15 seconds

**What it does:**

1. **Create local git commit (if uncommitted changes):**
   - Auto-stage modified files (exclude temp files)
   - Create WIP commit: "WIP: [context] - checkpoint"
   - **NO PUSH** (local only)

2. **Save session state to `.claude/session-state.json`:**
   ```json
   {
     "mode": "quick",
     "saved_at": "2025-11-27T14:30:00",
     "git_commit": "abc1234",
     "git_pushed": false,
     "current_task": "OpenSpec #00026",
     "next_steps": ["Continue implementation", "Run tests"]
   }
   ```

3. **Output:**
   ```
   Quick checkpoint saved (~12s)
   Session: .claude/session-state.json
   Commit: abc1234 (local only)
   ```

---

## Mode 2: Sync (--sync) ~20-40 seconds

**What it does:**

1. **Everything from Quick Mode**

2. **Git push to GitHub:**
   - Push all local commits to origin
   - Trigger CI/CD (but don't wait)

3. **Update session state:**
   ```json
   {
     "mode": "sync",
     "saved_at": "2025-11-27T18:45:00",
     "git_commits": ["abc1234", "def5678"],
     "git_pushed": true,
     "ci_triggered": true,
     "current_task": "OpenSpec #00026",
     "next_steps": ["Check CI/CD results", "Continue next subtask"]
   }
   ```

4. **Output:**
   ```
   Session synced to GitHub (~28s)
   Commits: 5 pushed
   GitHub: Push successful
   CI/CD: Triggered
   ```

---

## Mode 3: Full (--full) ~3-6 minutes

**What it does:**

1. **Everything from Sync Mode**

2. **Documentation verification:**
   - Check CHANGELOG.md [Unreleased] has entries
   - Check ROADMAP.md task status
   - **IF MISSING:** Auto-generate proposals for user approval

3. **CI/CD monitoring:**
   - Wait for GitHub Actions (max 10 min)
   - Report results (Linux, macOS, Windows)

4. **Comprehensive session state:**
   ```json
   {
     "mode": "full",
     "saved_at": "2025-11-27T14:30:00",
     "duration": "4m 30s",
     "git_commits": ["abc1234", "def5678", "ghi9012"],
     "git_pushed": true,
     "ci_verified": true,
     "ci_status": {
       "linux": "pass",
       "macos": "pass",
       "windows": "pass"
     },
     "changelog_updated": true,
     "roadmap_updated": true,
     "completed_task": "00026",
     "next_steps": ["Start OpenSpec #00027"]
   }
   ```

5. **Output:**
   ```
   Full verification complete (~4m 30s)

   Git: 8 commits pushed
   Docs: CHANGELOG/ROADMAP updated
   CI/CD:
   - Linux: PASS
   - macOS: PASS
   - Windows: PASS

   Ready for next milestone!
   ```

---

## Error Handling

- **Network failure:** Degrade to quick mode, save locally
- **CHANGELOG missing:** Auto-generate proposals
- **CI/CD failure:** Report errors, don't block save

---

## Output

Session state saved to `.claude/session-state.json` for restoration by `/load-session`.
