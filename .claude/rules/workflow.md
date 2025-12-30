# Agent Dispatch & Workflow

**ALWAYS, for EVERY user message, BEFORE doing anything else:**

1. **CHECK** if the message contains ANY trigger from `.claude/workflow.json` -> `triggers`
2. **If match found** → use `Task` tool to launch the corresponding agent
3. **You MUST NOT perform the agent's work yourself**

## Agents

| Agent | Role |
|-------|------|
| task-manager | Creates/tracks/closes OpenSpec, manages workflow, SESSION RESTORE |
| architect | Analyzes code, designs solutions |
| code-writer | Writes NEW code (new files, new classes) |
| code-editor | Modifies EXISTING code |
| ui-designer | Creates UI components (Qt6 widgets) |
| code-reviewer | Code review, quality checks |
| tester | Runs build and tests, reports results |
| devops | CI/CD specialist (standalone) |

## Workflow

**PREFERRED:** `/workflow "description"` for automatic orchestration.

**Manual workflow:**
```
NEW TASK:
1. task-manager  → Creates OpenSpec
2. architect     → Analyzes, designs
3. code-writer / code-editor / ui-designer → Implements
4. code-reviewer → Reviews
5. tester        → Tests
6. task-manager  → Closes task

CONTINUE TASK:
1. task-manager  → Loads OpenSpec, shows status
2. architect     → Reviews design if needed
3. ... (continues from step 3)
```

**CRITICAL:** Never skip code-reviewer or tester! Task is NOT DEPLOYED until all pass.
