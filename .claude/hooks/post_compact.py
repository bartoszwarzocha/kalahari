"""PostCompact hook - injects critical context after auto-compaction.

When Claude Code auto-compacts the conversation, earlier context is lost.
This hook fires AFTER compaction, so injected context survives and is visible.
Also saves a state snapshot to disk for recovery.
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
        "event": "post_compact",
        "active_openspec": openspec,
    }

    os.makedirs(os.path.dirname(STATE_FILE), exist_ok=True)
    with open(STATE_FILE, "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2)

    if openspec:
        context = (
            f"[POST-COMPACTION RECOVERY] Context was compacted. "
            f"Active OpenSpec: #{openspec}. "
            f"Re-read these files before making decisions: "
            f"openspec/changes/{openspec}/proposal.md, "
            f"openspec/changes/{openspec}/tasks.md"
        )
    else:
        context = "[POST-COMPACTION RECOVERY] Context was compacted. No active OpenSpec."

    result = {"additionalContext": context}
    print(json.dumps(result))


if __name__ == "__main__":
    main()
