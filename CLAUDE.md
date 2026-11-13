# KALAHARI - Writer's IDE

C++20 + Qt6 | Desktop Application

## Essential Commands

```bash
scripts/build_windows.bat Debug          # Build (Windows)
./build-windows/bin/kalahari-tests.exe   # Tests (Windows)
scripts/build_linux.sh                   # Build (Linux)
./build-linux/bin/kalahari-tests         # Tests (Linux)
scripts/sonar_analysis.bat               # SonarQube analysis (Windows) - before finishing every stage
python scripts/sonar_analysis.py         # SonarQube analysis (any OS, after a build)
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

## Local, per-machine files (git-ignored)

- `.claude/settings.local.json` — personal permissions
- `.claude/session-state.json` — written by `/save-session`, read by `/load-session` and the SessionStart hook
- `.mcp.json` — MCP servers (copy from `.mcp.json.example`)
