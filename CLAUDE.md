# KALAHARI - Writer's IDE

C++20 + Qt6 | Desktop Application
Full context: `.claude/context/project-brief.txt`

## MANDATORY: Agent Dispatch

**ALWAYS, for EVERY user message, BEFORE doing anything else:**

1. **CHECK** if the message contains ANY trigger from the Agents table below
2. **If match found** → use `Task` tool to launch the corresponding agent
3. **You MUST NOT perform the agent's work yourself**

**Trigger matching examples:**

| User message | Trigger match | Agent | Action |
|--------------|---------------|-------|--------|
| "session" | "session" | task-manager | `Task(subagent_type="task-manager", ...)` - SESSION RESTORE |
| "kontynuuj task 00027" | "kontynuuj task" | task-manager | `Task(subagent_type="task-manager", ...)` |
| "zaprojektuj panel statystyk" | "zaprojektuj" | architect | `Task(subagent_type="architect", ...)` |
| "napraw ten błąd w ustawieniach" | "napraw" | code-editor | `Task(subagent_type="code-editor", ...)` |
| "zrób review przed commitem" | "review" | code-reviewer | `Task(subagent_type="code-reviewer", ...)` |
| "uruchom testy" | "uruchom testy" | tester | `Task(subagent_type="tester", ...)` |
| "stwórz nowy dialog" | "dialog" | ui-designer | `Task(subagent_type="ui-designer", ...)` |

**Why this matters:** Each agent has specialized skills, tools, and context. Doing their work yourself bypasses this specialization and loses quality.

---

## Agents (8)

| Triggers | Agent | Role |
|----------|-------|------|
| "session", "nowe zadanie", "new task", "kontynuuj task", "continue task", "wznów", "co dalej", "status taska", "zamknij task", "status", "gdzie jesteśmy", "task gotowy" | task-manager | Creates/tracks/closes OpenSpec, manages workflow, SESSION RESTORE |
| "zaprojektuj", "przeanalizuj", "jak to zrobić", "gdzie to dodać", "design" | architect | Analyzes code (Serena), designs solutions |
| "napisz", "utwórz klasę", "dodaj nową funkcję", "nowy plik", "create", "new class" | code-writer | Writes NEW code (new files, new classes) |
| "zmień", "popraw", "napraw", "refaktoruj", "fix", "modify", "change" | code-editor | Modifies EXISTING code |
| "dialog", "panel", "toolbar", "UI", "widget", "layout" | ui-designer | Creates UI components (Qt6 widgets) |
| "review", "sprawdź kod", "przed commitem", "czy mogę commitować", "code review" | code-reviewer | Code review, quality checks |
| "testy", "przetestuj", "uruchom testy", "QA", "czy działa", "run tests" | tester | Runs build and tests, reports results |
| "CI", "CD", "napraw CI", "pipeline", "GitHub Actions", "workflow failed", "deploy" | devops | CI/CD specialist (standalone, not in main workflow) |

## Workflow

```
NEW TASK:
1. task-manager  → Creates OpenSpec (requirements gathering)
2. architect     → Analyzes codebase, creates design
3. code-writer / code-editor / ui-designer → Implements
4. code-reviewer → Reviews code quality
5. tester        → Runs tests
6. task-manager  → Closes task (verify docs, commit)

CONTINUE TASK:
1. task-manager  → Loads OpenSpec, shows status, asks for confirmation
2. architect     → Reviews/updates design if needed
3. ... (continues from step 3 above)
```

## MCP Servers

- **Serena:** Code navigation ONLY (get_symbols_overview, find_symbol, find_referencing_symbols)
- **Context7:** External library docs (resolve-library-id → get-library-docs)

## Build Commands

```bash
# Windows (ALWAYS use this, NEVER cmake directly)
scripts/build_windows.bat Debug

# Linux
scripts/build_linux.sh
```

## Mandatory Patterns (CORRECT API)

```cpp
// Icons - ALWAYS through ArtProvider
core::ArtProvider::getInstance().getIcon("file.new")
core::ArtProvider::getInstance().createAction("file.new", parent)

// Icon Colors - ALWAYS through ArtProvider
core::ArtProvider::getInstance().getPrimaryColor()
core::ArtProvider::getInstance().getSecondaryColor()

// Config - ALWAYS through SettingsManager
core::SettingsManager::getInstance().getValue("key", "default")

// Themes - through ThemeManager
core::ThemeManager::getInstance().getCurrentTheme()

// UI Strings - ALWAYS through tr()
tr("User visible text")

// Logging
core::Logger::getInstance().info("Message: {}", value)
```

## NEVER Use

```cpp
Theme::instance()            // Does NOT exist!
ArtProvider::instance()      // Wrong! Use getInstance()
QIcon("path/to/icon.svg")    // Use ArtProvider
QColor(255, 0, 0)            // Use ArtProvider colors
"Hardcoded string"           // Use tr()
```

## OpenSpec (Task Management)

- Location: `openspec/changes/NNNNN-name/`
- Files: `proposal.md`, `tasks.md`
- Numbering: 5 digits with leading zeros (00001, 00027)
- Lifecycle: `PENDING → IN_PROGRESS → DEPLOYED`

## Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Files | snake_case | `character_card.cpp` |
| Classes | PascalCase | `CharacterCard` |
| Methods | camelCase | `getTitle()` |
| Members | m_prefix | `m_title` |
| Constants | UPPER_SNAKE_CASE | `MAX_CHAPTERS` |

## Language Policy

- **Code, comments, docs:** English ONLY
- **Conversation:** Polish OK
