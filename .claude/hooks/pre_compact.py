"""PreCompact hook - saves critical context before auto-compaction.

When Claude Code auto-compacts the conversation, earlier context is lost.
This hook saves a snapshot of the current working state so the agent
can recover after compaction.
"""

import json
import os
import sys
from datetime import datetime

PROJECT_DIR = os.environ.get("CLAUDE_PROJECT_DIR", r"E:\Python\Projekty\Kalahari")
STATE_FILE = os.path.join(PROJECT_DIR, ".claude", "compact-state.json")
OPENSPEC_DIR = os.path.join(PROJECT_DIR, "openspec", "changes")


def find_active_openspec():
    """Find the most recent IN_PROGRESS OpenSpec."""
    if not os.path.isdir(OPENSPEC_DIR):
        return None
    folders = sorted(os.listdir(OPENSPEC_DIR), reverse=True)
    for folder in folders:
        proposal = os.path.join(OPENSPEC_DIR, folder, "proposal.md")
        if os.path.isfile(proposal):
            with open(proposal, "r", encoding="utf-8") as f:
                content = f.read()
            if "IN_PROGRESS" in content:
                return folder
    return None


def main():
    openspec = find_active_openspec()

    state = {
        "timestamp": datetime.now().isoformat(),
        "event": "pre_compact",
        "active_openspec": openspec,
    }

    # Save state
    os.makedirs(os.path.dirname(STATE_FILE), exist_ok=True)
    with open(STATE_FILE, "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2)

    # Output warning as additionalContext
    result = {
        "additionalContext": (
            f"[CONTEXT COMPACTION] Conversation is being compressed. "
            f"Active OpenSpec: #{openspec or 'none'}. "
            f"State saved to .claude/compact-state.json. "
            f"After compaction, re-read critical files before making decisions: "
            f"openspec/changes/{openspec}/ANALYSIS_PHASE15_ISSUES.md, "
            f"openspec/changes/{openspec}/tasks.md"
            if openspec
            else "[CONTEXT COMPACTION] No active OpenSpec. State saved."
        )
    }

    print(json.dumps(result))


if __name__ == "__main__":
    main()
