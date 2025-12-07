---
description: Run workflow orchestrator for a task
argument-hint: <task description>
allowed-tools: Bash(python:*), Bash(cd:*)
---

# Workflow Orchestrator

Run the workflow orchestrator for the given task.

## Instructions

1. Get task description from argument: $ARGUMENTS
2. Run the Python orchestrator:

```bash
cd E:\Python\Projekty\Kalahari\.claude && python -m orchestrator.main "$ARGUMENTS"
```

3. The orchestrator automatically:
   - Runs: task-manager → architect → implementation → code-review → tests → close
   - Asks for decisions at key moments
   - Shows summary at the end

## Notes

- Orchestrator uses SDK to call agents
- Each agent must end response with [WORKFLOW_STATUS] block
- On issues, orchestrator asks user for next steps
