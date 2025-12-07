---
name: OpenSpec Archive
description: Archive a deployed OpenSpec change
category: OpenSpec
tags: [openspec, archive, complete]
---

# OpenSpec: Archive Change

Archive a completed OpenSpec change and update specs.

## Guardrails

- Favor straightforward, minimal implementations
- Keep changes tightly scoped
- Refer to `openspec/AGENTS.md` for conventions

## Steps

1. **Identify change to archive:**
   - If change ID provided, use it
   - Otherwise run `openspec list` to find candidates
   - Confirm with user if unclear

2. **Validate change:**
   - Run `openspec list` or `openspec show <id>`
   - Verify change is complete (not already archived)

3. **Archive change:**
   ```bash
   openspec archive <id> --yes
   ```
   - Moves change to `changes/archive/`
   - Updates target specs

4. **Verify archive:**
   - Check `changes/archive/<id>/` exists
   - Run `openspec validate --strict`

5. **Update documentation:**
   - Mark feature `[x]` in ROADMAP.md
   - Add entry to CHANGELOG.md [Unreleased]

## Reference

- `openspec list` to confirm change IDs
- `openspec list --specs` to inspect updated specs
- `openspec show <id>` for change details

## Output

Change archived to `changes/archive/`, specs updated, documentation current.
