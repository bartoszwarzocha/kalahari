---
description: Run workflow orchestrator in mock/test mode
argument-hint: <task description>
allowed-tools: Bash(python:*), Bash(cd:*)
---

# Workflow Orchestrator (Mock Mode)

Run the workflow orchestrator in test mode (without real agent calls).

## Instructions

1. Get task description from argument: $ARGUMENTS
2. Run orchestrator in mock mode:

```bash
cd E:\Python\Projekty\Kalahari\.claude && python -m orchestrator.main --mock "$ARGUMENTS"
```

3. Mock mode simulates agent responses - useful for testing flow without API costs.
