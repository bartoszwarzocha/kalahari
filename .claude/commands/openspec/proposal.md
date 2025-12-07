---
name: OpenSpec Proposal
description: Create a new OpenSpec change proposal
category: OpenSpec
tags: [openspec, change, proposal]
---

# OpenSpec: Create Proposal

Scaffold a new OpenSpec change with strict validation.

## Guardrails

- Favor straightforward, minimal implementations
- Keep changes tightly scoped
- Refer to `openspec/AGENTS.md` for conventions
- Ask follow-up questions before editing files
- **NO CODE during proposal stage** - only design documents

## Steps

1. **Review current state:**
   - Read `openspec/project.md`
   - Run `openspec list` and `openspec list --specs`
   - Inspect related code/docs

2. **Choose change ID:**
   - Unique, verb-led name
   - Format: `NNNNN-verb-description`
   - Example: `00027-add-stats-panel`

3. **Create scaffold:**
   ```
   openspec/changes/NNNNN-name/
   ├── proposal.md    # Change overview
   ├── tasks.md       # Implementation checklist
   └── design.md      # Architecture (if needed)
   ```

4. **Draft documents:**
   - `proposal.md`: Goal, scope, acceptance criteria
   - `tasks.md`: Ordered, verifiable work items
   - `design.md`: Architecture decisions (if complex)

5. **Validate:**
   ```bash
   openspec validate <id> --strict
   ```

## Reference

- `openspec show <id> --json --deltas-only` for details
- `rg -n "Requirement:|Scenario:" openspec/specs` for existing requirements

## Output

New OpenSpec change created in `openspec/changes/<id>/` ready for approval.
