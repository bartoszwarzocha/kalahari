# Agent Dispatch & Workflow

Dispatch an agent when a task benefits from specialization or parallelism. Do NOT force
dispatch on every message — trivial edits and questions are handled inline.

## Agents (5)

| Agent | Role | Effort |
|-------|------|--------|
| `architect` | Analyzes code, designs solutions (no production code) | xhigh |
| `coder` | Implements C++/Qt6 — new files AND edits AND UI components | high |
| `code-reviewer` | Quality & pattern review before commit (no fixes) | xhigh |
| `tester` | Runs build + test suite, reports pass/fail | medium |
| `devops` | CI/CD, GitHub Actions (standalone) | medium |

The main loop picks the right agent from these descriptions — there is no trigger-word table.
For several independent pieces of work, dispatch multiple agents in one message (they run
concurrently), or use the native `Workflow` tool to orchestrate fan-out deterministically.

## Workflow — native tools as the backbone, Superpowers where it helps

```
NEW FEATURE / CHANGE:
  1. Design      → plan mode  (+ superpowers:brainstorming for open-ended intent,
                                 superpowers:writing-plans for multi-step specs)
                   architect  → analysis & design docs
  2. Implement   → coder      (one worker: creates, edits, UI)
                   Workflow    → fan out when tasks are independent
  3. Review      → code-reviewer  AND/OR  /code-review   (bundled, multi-agent)
  4. Verify      → tester  +  /verify   (drive the real app, not just tests)
  5. Finish      → superpowers:finishing-a-development-branch  (merge / PR / cleanup)

BUG FIX:
  superpowers:systematic-debugging → superpowers:test-driven-development
  → coder (fix) → tester + /verify
```

### When to reach for what
- **plan mode** — any non-trivial change; approve scope before touching files.
- **`Workflow` tool** — comprehensive/parallel work (audits, migrations, multi-file sweeps,
  find→verify pipelines). Deterministic orchestration of many agents.
- **`superpowers:brainstorming`** — before creative work when intent/requirements are fuzzy.
- **`superpowers:writing-plans`** — turn a spec into a step-by-step plan under `docs/superpowers/plans/`.
- **`superpowers:systematic-debugging`** — any bug/test failure, before proposing a fix.
- **`superpowers:test-driven-development`** — write the failing test first.
- **`/code-review`, `/simplify`, `/verify`** — bundled native skills, complementary to the agents.

**CRITICAL:** Never skip review + verification. Work is NOT complete until `code-reviewer`
(or `/code-review`) passes AND the build/tests are green (`tester` / `/verify`).
