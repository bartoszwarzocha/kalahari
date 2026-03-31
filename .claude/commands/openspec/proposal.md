---
name: OpenSpec Proposal
description: Create a new OpenSpec change proposal
category: OpenSpec
tags: [openspec, change, proposal]
---

# OpenSpec: Create Proposal

## Guardrails

- Max 15-25 tasks per OpenSpec — split larger features
- Each task completable in one session
- **NO CODE during proposal stage**

## Steps

1. Gather requirements (goal, scope, acceptance criteria)
2. Find next number: `ls openspec/changes/ | sort -r | head -1`
3. Create change directory:
   ```
   openspec/changes/NNNNN-name/
   ├── proposal.md      # Goal, scope, acceptance criteria, status: PENDING
   ├── tasks.md         # Hierarchical checklist (1.1, 1.2, 2.1), max 15-25 items
   └── specs/           # Delta specs (what changes in requirements)
       └── <area>.md    # One file per affected spec area
   ```

4. Write `proposal.md` with status PENDING
5. Write `tasks.md` with hierarchical tasks
6. Write delta specs in `specs/` using operations:
   ```markdown
   ## ADDED Requirements
   ### Requirement: <Name>
   Description of new requirement.

   ## MODIFIED Requirements
   ### Requirement: <Existing Name>
   Updated description.

   ## REMOVED Requirements
   ### Requirement: <Name>
   Reason for removal.
   ```

7. Review existing main specs in `openspec/specs/` to understand what already exists

## Output

New OpenSpec change created in `openspec/changes/<id>/` with delta specs, ready for approval.
