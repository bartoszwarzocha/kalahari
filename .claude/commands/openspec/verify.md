---
name: OpenSpec Verify
description: Validate implementation against OpenSpec specification
category: OpenSpec
tags: [openspec, verify, validation]
---

# OpenSpec: Verify Change

Validate that implementation matches the specification before archiving.

## Steps

1. **Read change documents:**
   - `openspec/changes/<id>/proposal.md` — acceptance criteria
   - `openspec/changes/<id>/tasks.md` — all tasks should be [x]
   - `openspec/changes/<id>/specs/` — delta specs

2. **Check tasks completion:**
   - Count `[x]` vs `[ ]` in tasks.md
   - Report any incomplete tasks

3. **Verify acceptance criteria:**
   - For each criterion in proposal.md, check if code satisfies it
   - Read relevant source files to confirm

4. **Verify delta specs match reality:**
   - For each ADDED requirement: is it implemented?
   - For each MODIFIED requirement: does the code reflect the change?
   - For each REMOVED requirement: is the old code actually removed?

5. **Check build and docs:**
   - Build passes: `scripts/build_windows.bat Debug`
   - CHANGELOG.md has entry in [Unreleased]
   - ROADMAP.md updated if new feature

6. **Report:**
   ```
   Verify #NNNNN: PASS / FAIL
   Tasks: X/Y complete
   Criteria: X/Y met
   Build: PASS/FAIL
   Docs: OK/MISSING
   ```

## Output

Verification report. If PASS → ready for `/openspec:archive`.
