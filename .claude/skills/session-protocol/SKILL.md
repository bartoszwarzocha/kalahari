---
name: session-protocol
description: Session management protocol. Use for saving and loading session state.
---

# Session Protocol

## 1. Session State Location

```
.claude/session-state.json
```

**DO NOT commit this file** - it's local workspace state.

## 2. Session State Format

```json
{
  "timestamp": "2025-11-27T15:30:00",
  "mode": "quick|sync|full",
  "openspec": "00027",
  "openspec_status": "PENDING|IN_PROGRESS|DEPLOYED",
  "working_on": "Brief description of current work",
  "git_branch": "main",
  "git_commit": "abc1234",
  "uncommitted_changes": true,
  "next_steps": ["Step 1", "Step 2"]
}
```

## 3. Save Modes

| Mode | Duration | Use Case |
|------|----------|----------|
| **quick** | ~15s | Hourly checkpoints, WIP, before risky changes |
| **sync** | ~30s | End of day, subtask complete |
| **full** | ~4min | Task/phase complete, milestone |

## 4. Integration with OpenSpec

When tracking status (`status taska`):
1. Read session-state.json first (if exists)
2. Cross-reference with active OpenSpec
3. Report both session state and task progress

When closing task (`zamknij task`):
1. Verify OpenSpec completeness
2. Suggest `/save-session --full` after successful close

## 5. Commands

- `/save-session` - Save current state (see save-session.md for details)
- `/load-session` - Restore previous state (see load-session.md for details)

## 6. Key Rules

1. **Session state ≠ Serena memories** - Use session-state.json, not MCP memories
2. **OpenSpec = truth for tasks** - Session state only tracks "where we are"
3. **Always suggest save** - Before ending work, remind about `/save-session`
4. **Gap detection** - On load, check for commits made outside sessions
