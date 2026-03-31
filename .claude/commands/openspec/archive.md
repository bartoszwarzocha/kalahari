---
name: OpenSpec Archive
description: Archive a deployed OpenSpec change and merge delta specs
category: OpenSpec
tags: [openspec, archive, complete]
---

# OpenSpec: Archive Change

Archive a completed change: merge delta specs into main specs, move to archive.

## Steps

1. **Identify change:** Find DEPLOYED change in `openspec/changes/`

2. **Verify completeness:**
   - All tasks in tasks.md checked `[x]`
   - Status = DEPLOYED in proposal.md
   - CHANGELOG.md has entry
   - ROADMAP.md updated (if feature)

3. **Merge delta specs** from `openspec/changes/<id>/specs/` into `openspec/specs/`:
   - Apply operations in order: RENAMED → REMOVED → MODIFIED → ADDED
   - If a main spec doesn't exist yet, create it from ADDED requirements
   - If no delta specs exist, skip this step

4. **Move to archive:**
   ```bash
   mv openspec/changes/NNNNN-name openspec/archive/NNNNN-name
   ```

5. **Verify:** Main specs in `openspec/specs/` reflect merged changes

## Output

Change archived to `openspec/archive/`, main specs updated with living documentation.
