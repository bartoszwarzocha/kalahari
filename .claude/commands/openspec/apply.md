---
name: OpenSpec Apply
description: Implement an approved OpenSpec change
category: OpenSpec
tags: [openspec, apply, implementation]
---

# OpenSpec: Apply Change

Implement an approved OpenSpec change and keep tasks in sync.

## Guardrails

- Favor straightforward, minimal implementations
- Keep changes tightly scoped
- Refer to `openspec/AGENTS.md` for conventions

## Steps

1. **Read change documents:**
   - `changes/<id>/proposal.md`
   - `changes/<id>/design.md` (if present)
   - `changes/<id>/tasks.md`

2. **Confirm scope:**
   - Review acceptance criteria
   - Verify understanding

3. **Implement sequentially:**
   - Work through tasks in order
   - Keep edits minimal and focused
   - Run build after each change:
     ```bash
     scripts/build_windows.bat Debug
     ```

4. **Update task status:**
   - Mark completed: `- [x]`
   - Update status in proposal.md

5. **Verify completion:**
   - All tasks marked `[x]`
   - Build passes
   - Tests pass

## Reference

- `openspec show <id> --json --deltas-only` for context
- `openspec list` for active changes

## Output

Change implemented, all tasks complete, ready for archive.
