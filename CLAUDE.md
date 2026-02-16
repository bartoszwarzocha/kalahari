# KALAHARI - Writer's IDE

C++20 + Qt6 | Desktop Application

## Core Rule

**For EVERY user message:** Check triggers in `.claude/workflow.json` → dispatch to appropriate agent via `Task` tool. Never do agent's work yourself.

## Essential Commands

```bash
/workflow "task description"    # Full orchestration (PREFERRED)
scripts/build_windows.bat Debug # Build (Windows)
```

## Project Context

@.claude/context/project-brief.txt

## Rules (auto-loaded from .claude/rules/)

| What | Where |
|------|-------|
| Agent dispatch & workflow | `.claude/rules/workflow.md` |
| C++ code patterns | `.claude/rules/patterns.md` |
| Naming conventions | `.claude/rules/naming.md` |
| Build commands | `.claude/rules/build.md` |
| Workflow triggers | `.claude/workflow.json` |
