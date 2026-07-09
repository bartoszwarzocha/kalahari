---
description: Restore session state from .claude/session-state.json
---

# Load Session Command

Restores working context at the start of a session.

## Usage

```bash
/load-session    # auto-detects mode from saved state
```

---

## How It Works

1. **Read session state** from `.claude/session-state.json` (lean schema):
   ```json
   {
     "mode": "quick",
     "saved_at": "2026-07-08T11:30:00",
     "git_commit": "abc1234",
     "git_pushed": false,
     "working_on": "Short description of the active task",
     "next_steps": ["Continue implementation", "Run tests"],
     "active_plan": "docs/superpowers/plans/2026-07-08-<name>.md",
     "blocker": null
   }
   ```
   `active_plan` (optional) points at the current Superpowers plan if one is in flight.

2. **Mode-aware restoration:**

   | Mode | What load-session does |
   |------|------------------------|
   | **quick** | Light: git-gap check, basic consistency |
   | **sync**  | Medium: git-gap check + verify commits on remote / CI status |
   | **full**  | Deep: git gaps + CHANGELOG/ROADMAP consistency + CI results |

3. **ALWAYS check git gaps** (all modes).

4. **If `active_plan` is set**, re-read that plan file before continuing.

---

## Git Gap Detection

A **gap** = commits made between sessions without a session record.

```
GAP DETECTED: commits since last session without a checkpoint:
- abc1234 (2h ago) "feat: add menu integration"

What happened? Explain (creates a recovery note) or type "skip".
```

---

## Restore Output

```
Session restored from 2026-07-08T11:30:00 (mode: quick)
Working on: <working_on>
Active plan: docs/superpowers/plans/2026-07-08-<name>.md   (if any)
Git: clean / N commits on remote
Next steps:
- <next_steps...>
```

If a **blocker** is present, display it prominently and require it be addressed before continuing:
```
⚠️ BLOCKER FROM PREVIOUS SESSION
Type: <type>   Description: <...>   Resolution: <...>
```

---

## No Session / Errors

- **No session file:** report first session; suggest `/save-session` to create a checkpoint.
- **Corrupted state:** fall back to a light git-gap check.
- **Network failure:** local verification only.

## Output

Restores context from `.claude/session-state.json` and verifies consistency for the saved mode.
Fits the native workflow: after restore, resume via plan mode / the active Superpowers plan.
