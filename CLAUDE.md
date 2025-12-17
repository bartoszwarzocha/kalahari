# KALAHARI - Writer's IDE

C++20 + Qt6 | Desktop Application
Full context: `.claude/context/project-brief.txt`

## MANDATORY: Agent Dispatch

**ALWAYS, for EVERY user message, BEFORE doing anything else:**

1. **CHECK** if the message contains ANY trigger from `.claude/workflow.json` -> `triggers`
2. **If match found** → use `Task` tool to launch the corresponding agent
3. **You MUST NOT perform the agent's work yourself**

**Trigger matching examples (full list in workflow.json):**

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

**Triggers are defined in:** `.claude/workflow.json` -> `triggers` section (single source of truth)

| Agent | Role |
|-------|------|
| task-manager | Creates/tracks/closes OpenSpec, manages workflow, SESSION RESTORE |
| architect | Analyzes code, designs solutions |
| code-writer | Writes NEW code (new files, new classes) |
| code-editor | Modifies EXISTING code |
| ui-designer | Creates UI components (Qt6 widgets) |
| code-reviewer | Code review, quality checks |
| tester | Runs build and tests, reports results |
| devops | CI/CD specialist (standalone, not in main workflow) |

## Workflow

**PREFERRED: Use `/workflow` command for automatic orchestration:**

```bash
/workflow "description of task"       # Full orchestration
/workflow-mock "test task"            # Mock mode (no API costs)
```

The orchestrator automatically enforces the full workflow sequence.

**Manual workflow (if orchestrator not used):**

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

**CRITICAL:** Never skip code-reviewer or tester steps! Task is NOT DEPLOYED until all steps pass.

## MCP Servers

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
