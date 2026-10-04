"""Shared helpers for Kalahari Claude Code hooks (stdlib only, cross-platform)."""

import json
import os
import sys
from pathlib import Path


def project_dir() -> Path:
    """Project root: $CLAUDE_PROJECT_DIR, falling back to the repo containing this file."""
    env = os.environ.get("CLAUDE_PROJECT_DIR")
    return Path(env) if env else Path(__file__).resolve().parents[2]


def read_input() -> dict:
    """Parse the hook input JSON from stdin (empty dict on missing/invalid input)."""
    try:
        return json.load(sys.stdin)
    except (json.JSONDecodeError, ValueError):
        return {}


def emit_context(event: str, context: str, **extra) -> None:
    """Print hookSpecificOutput JSON that adds context for the model."""
    output = {"hookEventName": event, "additionalContext": context}
    output.update(extra)
    print(json.dumps({"hookSpecificOutput": output}))
