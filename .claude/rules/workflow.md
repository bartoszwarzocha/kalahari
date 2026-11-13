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

The Superpowers plugin is enabled in `.claude/settings.json` (`enabledPlugins`). If its skills are
not available in a session, follow the same steps without them — do not block on the plugin.

```
NEW FEATURE / CHANGE:
  1. Design      → plan mode  (+ superpowers:brainstorming for open-ended intent,
                                 superpowers:writing-plans for multi-step specs)
                   architect  → analysis & design docs
  2. Implement   → coder      (one worker: creates, edits, UI)
                   Workflow    → fan out when tasks are independent
  3. Review      → code-reviewer  AND/OR  /code-review   (bundled, multi-agent)
  4. Verify      → tester  +  /run   (launch the real app, not just tests)
  5. SonarQube   → scripts\sonar_analysis.bat (Windows) / python scripts/sonar_analysis.py
                   quality gate passed, no issues in the code the stage adds or changes
  6. Finish      → superpowers:finishing-a-development-branch  (merge / PR / cleanup)

BUG FIX:
  superpowers:systematic-debugging → superpowers:test-driven-development
  → coder (fix) → tester + /run → SonarQube
```

### SonarQube (user rule, 2026-11-13)
Every stage ends with a SonarQube analysis; results: http://localhost:9000/dashboard?id=kalahari.
All SonarQube rules are binding. The quality gate must pass and a stage may not leave issues in
the code it adds or changes. Issues that existed before are removed by the cleanup plan; an issue
is never marked as accepted or false positive without the user's consent. The analysis needs a
finished build (compile_commands.json and Qt's generated files); `sonar-project.properties`
defines what is analysed, and C++ goes through the sonar-cxx plugin and clang-tidy.

### When to reach for what
- **plan mode** — any non-trivial change; approve scope before touching files.
- **`Workflow` tool** — comprehensive/parallel work (audits, migrations, multi-file sweeps,
  find→verify pipelines). Deterministic orchestration of many agents.
- **`superpowers:brainstorming`** — before creative work when intent/requirements are fuzzy.
- **`superpowers:writing-plans`** — turn a spec into a step-by-step plan under `docs/superpowers/plans/`.
- **`superpowers:systematic-debugging`** — any bug/test failure, before proposing a fix.
- **`superpowers:test-driven-development`** — write the failing test first.
- **`/code-review`, `/simplify`, `/run`** — bundled native skills, complementary to the agents.

**CRITICAL:** Never skip review + verification. Work is NOT complete until `code-reviewer`
(or `/code-review`) passes AND the build/tests are green (`tester`, plus `/run` for UI changes)
AND the SonarQube analysis of the stage passes (see above).
