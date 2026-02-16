# Framework Upgrade Analysis: Claude Code 2.0.55 → 2.1.42

**Date:** 2026-02-16
**Current version:** 2.1.42
**Framework designed for:** 2.0.55 (November 2025)
**Gap:** ~90 versions, including major 2.1.0 release (January 7, 2026)

---

## CRITICAL ISSUES (Causing real problems NOW)

### 1. `permissionMode: manual` is INVALID
**Impact:** Agent permission handling unpredictable
**Where:** All agent .md files using `permissionMode: manual`
**Fix:** Change to `default` (read-only agents) or `bypassPermissions` (code agents)
**Valid values:** `default`, `acceptEdits`, `delegate`, `dontAsk`, `bypassPermissions`, `plan`

### 2. PreToolUse hooks use DEPRECATED format
**Impact:** `decision: "approve"/"block"` is deprecated
**Where:** `.claude/settings.json` PreToolUse hooks
**Fix:** Migrate to `hookSpecificOutput.permissionDecision: "allow"/"deny"/"ask"`

### 3. No `maxTurns` on agents — runaway execution risk
**Impact:** Agents can loop indefinitely, burning tokens and making bad changes
**Where:** All agent .md files
**Fix:** Add `maxTurns: 30` (or appropriate limit) to each agent

### 4. MCP tool name changed
**Impact:** `mcp__context7__get-library-docs` → `mcp__context7__query-docs`
**Where:** Architect agent references

---

## HIGH PRIORITY (Would have prevented the month-long issue)

### 5. No `PreCompact` hook — silent context loss
**Impact:** When conversation gets long, auto-compaction silently drops context.
Agent loses track of what it was doing, makes decisions without full picture.
THIS is likely what caused the O(n) rendering bug — agent lost context about
viewport culling requirements during a long refactoring session.
**Fix:** Add `PreCompact` hook to:
  - Back up transcript before compaction
  - Warn user that context is being compressed
  - Optionally save critical decisions to a file

### 6. No `TaskCompleted` hook — no quality gates
**Impact:** Tasks can be marked complete without verification.
Agent says "done" but the work has bugs (like our font/rendering issues).
**Fix:** Add `TaskCompleted` hook that runs build + tests before allowing closure.

### 7. No `Stop` hook — agent stops prematurely
**Impact:** Agent can decide it's "done" without verifying.
No enforcement that build passes before stopping.
**Fix:** Add `Stop` hook (prompt-based) that checks if work is actually complete.

### 8. No agent `memory` — knowledge lost between sessions
**Impact:** Each session starts from scratch. Architect re-discovers same patterns.
Code decisions made in session 1 are unknown in session 2.
**Fix:** Add `memory: project` to architect, code-reviewer, code-editor agents.

### 9. TodoWrite → Tasks API migration
**Impact:** Old TodoWrite was ephemeral (context window only). New Tasks API persists
across sessions with dependencies and status tracking.
**Tools:** `TaskCreate`, `TaskList`, `TaskGet`, `TaskUpdate`
**Fix:** Use native Tasks API for subtask tracking alongside OpenSpec for features.

---

## MEDIUM PRIORITY (Significant improvements)

### 10. Auto Memory not utilized
**What:** Claude auto-saves learnings to `~/.claude/projects/<project>/memory/`
**Fix:** Use MEMORY.md for cross-session knowledge (C++ patterns, Qt6 quirks, build issues)

### 11. CLAUDE.md imports not used
**What:** `@path/to/file` syntax to import files
**Fix:** Replace verbose CLAUDE.md with imports:
```markdown
@.claude/context/project-brief.txt
@.claude/rules/patterns.md
```

### 12. Path-scoped rules not used
**What:** Rules can target specific file patterns
**Fix:** Add frontmatter to `.claude/rules/patterns.md`:
```yaml
---
paths:
  - "src/**/*.cpp"
  - "include/**/*.h"
---
```

### 13. PostToolUse hooks not used
**What:** Run checks AFTER tool execution (async possible)
**Fix:** Add async `PostToolUse` on `Write|Edit` for `.cpp|.h` to run build check

### 14. Agent-based hooks not used
**What:** `type: "agent"` hooks spawn subagent with tool access for verification
**Fix:** Replace simple prompt hooks with agent hooks for C++ pattern validation

### 15. Background subagents not leveraged
**What:** `Ctrl+B` backgrounds running task; `run_in_background: true`
**Fix:** Use for parallel builds while continuing conversation

---

## LOW PRIORITY (Nice to have)

### 16. Agent Teams (experimental)
**What:** Multiple Claude sessions working in parallel with shared task list
**Enable:** `CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS=1`

### 17. Plugin packaging
**What:** Bundle agents/skills/hooks into distributable package
**Impact:** Easier versioning and sharing of Kalahari framework

### 18. Plan Mode (`permissionMode: plan`)
**What:** Native read-only exploration mode for agents
**Fix:** Use for architect agent instead of custom restrictions

### 19. Skill improvements
**What:** `context: fork`, `$ARGUMENTS`, `argument-hint`, `!command` dynamic content
**Fix:** Modernize skills with new features

### 20. `SlashCommand` → `Skill` permission migration
**What:** `SlashCommand(...)` syntax may be deprecated
**Fix:** Replace with `Skill(...)` in settings

---

## ROOT CAUSE ANALYSIS: Why did the month-long bug happen?

The Phase 15 refactoring introduced O(n) rendering and broken fonts. Analysis suggests
these contributing factors from the framework:

1. **No context preservation** — Long refactoring sessions hit auto-compaction,
   losing earlier decisions about viewport culling. Agent "forgot" the requirement.

2. **No quality gates** — No `TaskCompleted` or `Stop` hooks to enforce build+test
   before marking work as done.

3. **No agent memory** — Each session's architect started fresh, without memory of
   previous architectural decisions about rendering.

4. **No `maxTurns`** — Agents could make unlimited changes without checkpoint,
   accumulating errors.

5. **Invalid `permissionMode`** — Unpredictable permission behavior may have
   caused agents to skip important verification steps.

---

## RECOMMENDED UPGRADE ORDER

```
Phase 1: CRITICAL FIXES (30 min)
  1. Fix permissionMode in all agents
  2. Add maxTurns to all agents
  3. Fix deprecated PreToolUse hook format
  4. Fix MCP tool name

Phase 2: QUALITY GATES (1 hour)
  5. Add PreCompact hook
  6. Add TaskCompleted hook
  7. Add Stop hook
  8. Add PostToolUse async build check

Phase 3: MEMORY & PERSISTENCE (1 hour)
  9. Enable auto memory (MEMORY.md)
  10. Add memory: project to key agents
  11. Migrate session tracking to Tasks API
  12. Add CLAUDE.md imports

Phase 4: MODERNIZATION (2 hours)
  13. Path-scoped rules
  14. Agent-based hooks (replace prompt hooks)
  15. Skill improvements
  16. Permission syntax migration
```
