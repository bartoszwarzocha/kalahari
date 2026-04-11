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
PLANS_DIR = os.path.join(PROJECT_DIR, "docs", "superpowers", "plans")


def find_active_plan():
    """Find the most recent implementation plan."""
    if not os.path.isdir(PLANS_DIR):
        return None
    plans = sorted(
        [f for f in os.listdir(PLANS_DIR) if f.endswith(".md")],
        reverse=True,
    )
    return plans[0] if plans else None


def main():
    plan = find_active_plan()

    state = {
        "timestamp": datetime.now().isoformat(),
        "event": "post_compact",
        "active_plan": plan,
    }

    os.makedirs(os.path.dirname(STATE_FILE), exist_ok=True)
    with open(STATE_FILE, "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2)

    if plan:
        context = (
            f"[POST-COMPACTION RECOVERY] Context was compacted. "
            f"Active plan: docs/superpowers/plans/{plan}. "
            f"Re-read this file before continuing."
        )
    else:
        context = "[POST-COMPACTION RECOVERY] Context was compacted. No active plan."

    result = {"additionalContext": context}
    print(json.dumps(result))


if __name__ == "__main__":
    main()
