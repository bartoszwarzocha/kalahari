---
description: Restore session state from .claude/session-state.json
---

# Load Session Command

**MANDATORY at start of every work session**

## Usage

```bash
/load-session    # Single command - auto-detects mode
```

---

## How It Works

1. **Read session state:**
   ```bash
   .claude/session-state.json
   ```

2. **Parse mode and status from saved state:**
   ```json
   {
     "mode": "sync",
     "saved_at": "2025-12-06T11:30:00",
     "openspec": "00027",
     "openspec_status": "IN_PROGRESS",
     "working_on": "Theme Color Configuration",
     "implementation_status": "COMPLETE",
     "task_progress": {
       "completed": 17,
       "total": 22,
       "percentage": 77
     },
     "blocker": {
       "type": "vscode_terminal",
       "description": "VSCode terminal issues",
       "resolution": "Use CLI version"
     }
   }
   ```

3. **Mode-aware restoration:**

   | Session Mode | What load-session Does |
   |--------------|------------------------|
   | **quick**    | Light check: git gaps, basic consistency |
   | **sync**     | Medium check: git gaps, CI/CD status |
   | **full**     | Deep check: git gaps, docs consistency, CI/CD |

4. **ALWAYS check git gaps** (critical for all modes)

---

## Git Gap Detection

**A gap = commits between sessions without documentation**

```
2025-11-27 14:30 - /save-session
2025-11-27 15:45 - commit abc1234 (NOT IN SESSION!)
2025-11-27 18:00 - /load-session
                   GAP DETECTED: 1 commit without session!
```

### Gap Report

```
GAP DETECTED: Undocumented commits found!

Commits since last session:
- abc1234 (2 hours ago) "feat: Add menu integration"

What happened? Please explain or type "skip":
```

### User Response

- **Explain (RECOMMENDED):** Creates recovery note
- **Skip:** Gap remains undocumented

---

## Session Restore Output

### With Progress Tracking

```
Session restored from 2025-12-06T11:30:00

OpenSpec: #00027 (IN_PROGRESS)
Working on: Theme Color Configuration
Implementation: COMPLETE (17/22 tasks, 77%)

Completed:
- Backend API complete
- Unit tests written
- UI integration
... and 14 more

Pending:
- Manual testing
- Documentation
... and 3 more

BLOCKER DETECTED:
Type: vscode_terminal
Issue: VSCode terminal issues
Resolution: Use CLI version

Next steps:
1. Run build in CLI: scripts\build_windows.bat Debug
2. Run tests: build-windows\bin\kalahari-tests.exe
3. Manual testing of Theme Color Configuration UI
4. Close task #00027 after all tests pass
```

---

## Mode 1: Quick Session Restore ~5-10 seconds

**Detected when:** `mode: quick`

**Output:**
```
Quick session restored (~6s)
Last: 2025-11-27 14:30 (WIP checkpoint)
OpenSpec: #00027 (IN_PROGRESS)
Progress: 5/8 tasks (62%)

Git: Clean (no new commits)
Session: Loaded

Next steps:
- Continue implementation
- Run tests
```

---

## Mode 2: Sync Session Restore ~15-25 seconds

**Detected when:** `mode: sync`

**Verifies:**
- Commits exist on remote
- CI/CD status (if triggered)

**Output:**
```
Sync session restored (~18s)
Last: 2025-11-27 18:45 (synced)
OpenSpec: #00027 (IN_PROGRESS)
Progress: 6/8 tasks (75%)

Git: All commits on remote
CI/CD: All passing

Next steps:
- Continue next subtask
```

---

## Mode 3: Full Session Restore ~30-60 seconds

**Detected when:** `mode: full`

**Verifies:**
- Git commits on remote
- CHANGELOG/ROADMAP consistency
- CI/CD results

**Output:**
```
Full session restored (~45s)
Last: 2025-11-27 14:30 (verified)
OpenSpec: #00026 COMPLETE → Ready for #00027

Git: 8 commits on remote
Docs: CHANGELOG/ROADMAP consistent
CI/CD: All platforms passing

Next steps:
- Start OpenSpec #00027
```

---

## Blocker Display

If session has blocker, prominently display:

```
⚠️ BLOCKER FROM PREVIOUS SESSION:

Type: vscode_terminal
Description: VSCode terminal not executing commands properly
Resolution: Switch to CLI version of Claude Code
Appeared after: Extended debugging session

ACTION REQUIRED: Address blocker before continuing!
```

---

## No Session Found

```
No previous session found

This appears to be the first session.

Project state:
- Git: X commits total
- OpenSpec: Y active tasks

Use /save-session to create first checkpoint
```

---

## Error Handling

- **No session file:** Report first session
- **Corrupted state:** Fall back to quick mode
- **Network failure:** Local verification only
- **Git gaps:** Ask user for explanation

---

## Output

Restores context from `.claude/session-state.json` and verifies consistency based on saved mode.
