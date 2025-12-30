# KALAHARI - Writer's IDE

C++20 + Qt6 | Desktop Application

## Quick Reference

| What | Where |
|------|-------|
| Agent dispatch & workflow | `.claude/rules/workflow.md` |
| C++ code patterns | `.claude/rules/patterns.md` |
| Naming conventions | `.claude/rules/naming.md` |
| Build commands | `.claude/rules/build.md` |
| Full project context | `.claude/context/project-brief.txt` |
| Workflow triggers | `.claude/workflow.json` |

## Essential Commands

```bash
/workflow "task description"    # Full orchestration (PREFERRED)
scripts/build_windows.bat Debug # Build (Windows)
```

## Core Rule

**For EVERY user message:** Check triggers in `.claude/workflow.json` → dispatch to appropriate agent via `Task` tool. Never do agent's work yourself.
