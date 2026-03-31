---
name: OpenSpec Apply
description: Implement an approved OpenSpec change
category: OpenSpec
tags: [openspec, apply, implementation]
---

# OpenSpec: Apply Change

Implement an approved OpenSpec change and keep tasks in sync.

## Steps

1. **Read change documents:**
   - `openspec/changes/<id>/proposal.md`
   - `openspec/changes/<id>/tasks.md`
   - `openspec/changes/<id>/specs/` (delta specs)

2. **Set status to IN_PROGRESS** in proposal.md

3. **Implement sequentially:**
   - Work through tasks in order (1.1 → 1.2 → 2.1 ...)
   - Keep edits minimal and focused
   - Build after each change: `scripts/build_windows.bat Debug`

4. **Update task status:** Mark completed `- [x]`

5. **Update delta specs** if implementation reveals new requirements or changes to existing ones

## Output

Change implemented, tasks checked off, ready for verify.
