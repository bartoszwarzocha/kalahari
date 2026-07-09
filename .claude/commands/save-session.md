---
description: Save session state to .claude/session-state.json
argument-hint: [--sync | --full]
---

# Save Session Command

Saves a checkpoint at the end of a work session.

## Usage

```bash
/save-session          # Quick local checkpoint (~10-15s)
/save-session --sync   # + push to GitHub (~20-40s)
/save-session --full   # + docs + CI verification (~3-6min)
```

| Use case | Command |
|----------|---------|
| Hourly / WIP checkpoint | `/save-session` |
| Before risky changes | `/save-session` |
| End of day (unfinished) | `/save-session --sync` |
| Milestone / phase complete | `/save-session --full` |

---

## Mode 1: Quick (default)

1. **Local git commit** if there are uncommitted changes (WIP commit, exclude temp files, **no push**).
2. **Write `.claude/session-state.json`** (lean schema — no task-numbering, no OpenSpec):
   ```json
   {
     "mode": "quick",
     "saved_at": "2026-07-08T14:30:00",
     "git_commit": "abc1234",
     "git_pushed": false,
     "working_on": "Short description of the active task",
     "next_steps": ["Continue implementation", "Run tests"],
     "active_plan": "docs/superpowers/plans/2026-07-08-<name>.md",
     "blocker": null
   }
   ```
   Set `active_plan` when a Superpowers plan is in flight; omit or null otherwise.
3. **Output:** `Quick checkpoint saved · commit abc1234 (local) · working on <...>`

## Mode 2: Sync (`--sync`)

1. Everything from Quick.
2. **Push** local commits to origin (triggers CI, don't wait).
3. Set `"mode": "sync"`, `"git_pushed": true`.
4. **Output:** `Synced to GitHub · N commits pushed · CI triggered`

## Mode 3: Full (`--full`)

1. Everything from Sync.
2. **Docs check:** CHANGELOG.md `[Unreleased]` has entries; ROADMAP.md updated. If missing → propose entries for approval.
3. **CI monitoring:** wait for GitHub Actions (max ~10 min), report per platform.
4. Set `"mode": "full"`.
5. **Output:** per-platform CI results + docs status.

---

## Blocker Tracking

If work is blocked, include a `blocker` object:
```json
{
  "blocker": {
    "type": "build_error | test_failure | dependency | tooling | other",
    "description": "...",
    "resolution": "...",
    "appeared_after": "..."
  }
}
```

## Error Handling

- **Network failure:** degrade to quick mode, save locally.
- **CHANGELOG missing:** propose entries, don't block the save.
- **CI failure:** report, don't block the save.

## Output

Writes `.claude/session-state.json` for restoration by `/load-session`.
