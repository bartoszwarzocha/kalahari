#!/usr/bin/env python3
"""
UserPromptSubmit hook - validates agent triggers from CLAUDE.md

Runs BEFORE Claude processes the user's prompt.
Checks for trigger words and adds context about which agent to dispatch.
"""

import json
import sys
import re

# Agent triggers from CLAUDE.md - sorted by priority (multi-word first)
TRIGGERS = [
    # task-manager (multi-word first)
    ("nowe zadanie", "task-manager"),
    ("new task", "task-manager"),
    ("kontynuuj task", "task-manager"),
    ("continue task", "task-manager"),
    ("status taska", "task-manager"),
    ("zamknij task", "task-manager"),
    ("gdzie jesteśmy", "task-manager"),
    ("task gotowy", "task-manager"),
    ("co dalej", "task-manager"),
    ("session", "task-manager"),
    ("wznów", "task-manager"),
    ("status", "task-manager"),

    # architect
    ("jak to zrobić", "architect"),
    ("gdzie to dodać", "architect"),
    ("zaprojektuj", "architect"),
    ("przeanalizuj", "architect"),
    ("design", "architect"),

    # code-writer
    ("utwórz klasę", "code-writer"),
    ("dodaj nową funkcję", "code-writer"),
    ("nowy plik", "code-writer"),
    ("new class", "code-writer"),
    ("napisz", "code-writer"),
    ("create", "code-writer"),

    # code-editor
    ("napraw", "code-editor"),
    ("popraw", "code-editor"),
    ("zmień", "code-editor"),
    ("refaktoruj", "code-editor"),
    ("fix", "code-editor"),
    ("modify", "code-editor"),
    ("change", "code-editor"),

    # ui-designer
    ("dialog", "ui-designer"),
    ("panel", "ui-designer"),
    ("toolbar", "ui-designer"),
    ("widget", "ui-designer"),
    ("layout", "ui-designer"),
    ("UI", "ui-designer"),

    # code-reviewer
    ("sprawdź kod", "code-reviewer"),
    ("przed commitem", "code-reviewer"),
    ("czy mogę commitować", "code-reviewer"),
    ("code review", "code-reviewer"),
    ("review", "code-reviewer"),

    # tester
    ("uruchom testy", "tester"),
    ("run tests", "tester"),
    ("przetestuj", "tester"),
    ("czy działa", "tester"),
    ("testy", "tester"),
    ("QA", "tester"),

    # devops
    ("napraw CI", "devops"),
    ("GitHub Actions", "devops"),
    ("workflow failed", "devops"),
    ("pipeline", "devops"),
    ("deploy", "devops"),
    ("CI", "devops"),
    ("CD", "devops"),
]


def find_triggers(prompt: str) -> list:
    """Find matching triggers in the prompt."""
    found = []
    prompt_lower = prompt.lower()

    for trigger, agent in TRIGGERS:
        trigger_lower = trigger.lower()

        # Multi-word: substring match
        if " " in trigger:
            if trigger_lower in prompt_lower:
                found.append((trigger, agent))
        else:
            # Single word: word boundary match
            pattern = rf"\b{re.escape(trigger)}\b"
            if re.search(pattern, prompt, re.IGNORECASE):
                found.append((trigger, agent))

    # Deduplicate by agent (keep first match per agent)
    seen = set()
    unique = []
    for trigger, agent in found:
        if agent not in seen:
            seen.add(agent)
            unique.append((trigger, agent))

    return unique


def main():
    # Read hook input from stdin
    try:
        input_data = json.load(sys.stdin)
    except (json.JSONDecodeError, EOFError):
        sys.exit(0)

    prompt = input_data.get("prompt", "")
    if not prompt:
        sys.exit(0)

    # Find triggers
    found = find_triggers(prompt)

    if found:
        # Build context message
        lines = ["[AGENT DISPATCH REQUIRED]"]
        for trigger, agent in found:
            lines.append(f"  Trigger: '{trigger}' -> Agent: {agent}")
        lines.append("")
        lines.append("Use Task tool with subagent_type parameter. Do NOT perform agent work yourself!")

        context = "\n".join(lines)

        output = {
            "hookSpecificOutput": {
                "hookEventName": "UserPromptSubmit",
                "additionalContext": context
            }
        }
        print(json.dumps(output))

    sys.exit(0)


if __name__ == "__main__":
    main()
