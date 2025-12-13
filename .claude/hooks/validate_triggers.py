#!/usr/bin/env python3
"""
UserPromptSubmit hook - validates agent triggers from workflow.json

Runs BEFORE Claude processes the user's prompt.
Checks for trigger words and adds context about which agent to dispatch.

Single source of truth: .claude/workflow.json -> triggers section
"""

import json
import sys
import re
from pathlib import Path


def load_triggers() -> dict:
    """Load triggers from workflow.json (single source of truth)."""
    # Find workflow.json relative to this script
    script_dir = Path(__file__).parent.parent  # .claude/
    workflow_path = script_dir / "workflow.json"

    if not workflow_path.exists():
        return {}

    try:
        with open(workflow_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data.get("workflow", {}).get("triggers", {})
    except (json.JSONDecodeError, IOError):
        return {}


def build_trigger_list(triggers_dict: dict) -> list:
    """
    Build sorted trigger list from dict.
    Multi-word triggers first (higher priority), then single words.
    Returns list of (trigger, agent) tuples.
    """
    multi_word = []
    single_word = []

    for agent, trigger_list in triggers_dict.items():
        if agent.startswith("_"):  # Skip _comment and similar
            continue
        for trigger in trigger_list:
            if " " in trigger:
                multi_word.append((trigger, agent))
            else:
                single_word.append((trigger, agent))

    # Multi-word first (more specific), then single word
    return multi_word + single_word


def find_triggers(prompt: str, triggers: list) -> list:
    """Find matching triggers in the prompt."""
    found = []
    prompt_lower = prompt.lower()

    for trigger, agent in triggers:
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


def is_workflow_command(prompt: str) -> bool:
    """Check if prompt is a /workflow command (orchestrator handles these)."""
    prompt_stripped = prompt.strip()
    return prompt_stripped.startswith("/workflow")


# Auto-action patterns - detect user confirmations that should trigger automatic actions
AUTO_ACTION_PATTERNS = {
    "auto_commit": [
        r"test\s+manualny\s+OK",
        r"testy\s+OK",
        r"manual\s+test\s+OK",
        r"tests?\s+pass(ed)?",
        r"działa",
        r"works",
        r"LGTM",
        r"zatwierdzone",
        r"zatwierdzam",
        r"approved",
        r"wygląda\s+dobrze",
        r"looks\s+good",
    ],
    "auto_save_session": [
        r"koniec\s+sesji",
        r"end\s+session",
        r"zapisz\s+i\s+zakończ",
        r"save\s+and\s+quit",
    ],
}


def detect_auto_actions(prompt: str) -> list:
    """Detect patterns that trigger automatic actions without asking."""
    detected = []
    prompt_lower = prompt.lower()

    for action, patterns in AUTO_ACTION_PATTERNS.items():
        for pattern in patterns:
            if re.search(pattern, prompt_lower, re.IGNORECASE):
                detected.append(action)
                break  # One match per action is enough

    return detected


def main():
    # Read hook input from stdin
    try:
        input_data = json.load(sys.stdin)
    except (json.JSONDecodeError, EOFError):
        sys.exit(0)

    prompt = input_data.get("prompt", "")
    if not prompt:
        sys.exit(0)

    # Skip validation for /workflow commands (orchestrator handles dispatch)
    if is_workflow_command(prompt):
        sys.exit(0)

    # Load triggers from workflow.json
    triggers_dict = load_triggers()
    if not triggers_dict:
        # No triggers configured, silently pass
        sys.exit(0)

    triggers = build_trigger_list(triggers_dict)

    # Find triggers
    found = find_triggers(prompt, triggers)

    # Check for auto-action patterns
    auto_actions = detect_auto_actions(prompt)

    if auto_actions or found:
        lines = []

        # Auto-actions take priority
        if auto_actions:
            lines.append("[AUTO-ACTION DETECTED]")
            if "auto_commit" in auto_actions:
                lines.append("User confirmed tests/changes. AUTO-COMMIT without asking:")
                lines.append("1. Run: git status")
                lines.append("2. Run: git add -A (or relevant files)")
                lines.append("3. Run: git commit with appropriate message")
                lines.append("4. Do NOT ask for permission - user already confirmed!")
                lines.append("")
            if "auto_save_session" in auto_actions:
                lines.append("User requested session end. Run /save-session automatically.")
                lines.append("")

        # Agent dispatch
        if found:
            lines.append("[AGENT DISPATCH REQUIRED]")
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
