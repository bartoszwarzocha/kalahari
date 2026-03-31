# Agent Dispatch & Workflow

Use agents when the task benefits from specialization. Do NOT force dispatch on every message.

## Agents

| Agent | Role |
|-------|------|
| task-manager | Creates/tracks/closes OpenSpec, manages workflow |
| architect | Analyzes code, designs solutions |
| code-writer | Writes NEW code (new files, new classes) |
| code-editor | Modifies EXISTING code |
| ui-designer | Creates UI components (Qt6 widgets) |
| code-reviewer | Code review, quality checks |
| tester | Runs build and tests, reports results |
| devops | CI/CD specialist (standalone) |

## Workflow

```
NEW TASK:
  explore → proposal → apply → verify → archive

AGENTS PER STEP:
1. /openspec:explore    → architect (analysis)
2. /openspec:proposal   → task-manager (creates OpenSpec)
3. /openspec:apply      → code-writer / code-editor / ui-designer
4. code-reviewer        → Reviews
5. tester               → Build + tests
6. /openspec:verify     → Validates implementation vs spec
7. /openspec:archive    → Merges delta specs, closes task
```

**CRITICAL:** Never skip code-reviewer or tester! Task is NOT DEPLOYED until all pass.
