"""SessionStart hook - restores working context on startup, resume, clear and compact.

Reads the local (git-ignored) .claude/session-state.json written by /save-session.
After compaction it reminds Claude to re-read the active plan, since that context is lost.
"""

import json

from _common import emit_context, project_dir, read_input


def main() -> None:
    source = read_input().get("source", "startup")
    state_file = project_dir() / ".claude" / "session-state.json"

    if not state_file.is_file():
        if source == "startup":
            emit_context("SessionStart", "[SESSION] No saved session state (.claude/session-state.json).")
        return

    try:
        state = json.loads(state_file.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        emit_context("SessionStart", "[SESSION] .claude/session-state.json is unreadable - ignore it.")
        return

    lines = [f"[SESSION] Saved state from {state.get('saved_at', '?')} (may be stale - verify against git)."]
    if state.get("working_on"):
        lines.append(f"Working on: {state['working_on']}")
    if state.get("active_plan"):
        lines.append(f"Active plan: {state['active_plan']}")
    if state.get("blocker"):
        lines.append(f"BLOCKER: {json.dumps(state['blocker'], ensure_ascii=False)}")

    if source == "compact":
        lines.insert(0, "[POST-COMPACTION] Context was compacted.")
        if state.get("active_plan"):
            lines.append("Re-read the active plan before continuing.")
    elif source == "startup":
        lines.append("Run /load-session for a full restore.")

    emit_context("SessionStart", "\n".join(lines))


if __name__ == "__main__":
    main()
