---
name: OpenSpec Explore
description: Discuss and explore an idea before creating a formal OpenSpec
category: OpenSpec
tags: [openspec, explore, thinking]
---

# OpenSpec: Explore Idea

Thinking mode — discuss an idea before committing to a formal proposal.

## Purpose

Use when you want to:
- Discuss feasibility of a feature
- Explore different approaches
- Understand impact on existing code
- Decide scope before creating OpenSpec

**No files are created. No commitments made.**

## Steps

1. **Understand the idea** from $ARGUMENTS or ask user

2. **Analyze existing code:**
   - What areas would be affected?
   - What dependencies exist?
   - What's the complexity?

3. **Check existing specs** in `openspec/specs/` for related requirements

4. **Check ROADMAP.md** for related planned work

5. **Present analysis:**
   - Feasibility assessment
   - Estimated scope (small / medium / large)
   - Suggested approach(es)
   - Risks and open questions
   - Whether to split into multiple OpenSpecs

6. **Ask user:** Create OpenSpec now, or keep exploring?

## Output

Analysis and discussion. If user decides to proceed → run `/openspec:proposal`.
