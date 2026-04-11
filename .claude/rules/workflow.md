# Agent Dispatch & Workflow

Use agents when the task benefits from specialization. Do NOT force dispatch on every message.

## Agents

| Agent | Role |
|-------|------|
| architect | Analyzes code, designs solutions |
| code-writer | Writes NEW code (new files, new classes) |
| code-editor | Modifies EXISTING code |
| ui-designer | Creates UI components (Qt6 widgets) |
| code-reviewer | Code review, quality checks |
| tester | Runs build and tests, reports results |
| devops | CI/CD specialist (standalone) |

## Workflow

Superpowers skills drive the workflow. Agents are dispatched as implementation workers.

```
NEW FEATURE / CHANGE:
  brainstorming → writing-plans → subagent-driven-development → finishing-a-development-branch

IMPLEMENTATION AGENTS:
  - architect           → During brainstorming (analysis, design decisions)
  - code-writer         → New files, new classes
  - code-editor         → Modifications to existing code
  - ui-designer         → Qt6 widgets, dialogs, panels
  - code-reviewer       → Two-stage review (spec compliance + code quality)
  - tester              → Build + tests verification

BUG FIX:
  systematic-debugging → test-driven-development → verification-before-completion
```

**CRITICAL:** Never skip code-reviewer or tester! Work is NOT complete until both pass.
