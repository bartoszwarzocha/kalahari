"""PreToolUse hook (Bash git commit) - non-blocking documentation and hygiene reminders.

Checks the staged diff:
- C++ sources staged without a CHANGELOG.md entry,
- newly added TODO/FIXME lines in src/, include/ and tests/.
Never blocks the commit; it only adds context so Claude can decide.
"""

import re
import subprocess

from _common import emit_context, project_dir

SOURCE_RE = re.compile(r"^(src|include)/.*\.(cpp|h|hpp)$")
TODO_RE = re.compile(r"^\+(?!\+\+).*\b(TODO|FIXME)\b")


def git(*args: str) -> str:
    result = subprocess.run(
        ["git", *args], cwd=project_dir(), capture_output=True, text=True, encoding="utf-8"
    )
    return result.stdout if result.returncode == 0 else ""


def main() -> None:
    staged = git("diff", "--cached", "--name-only").splitlines()
    if not staged:
        return

    notes = []
    if any(SOURCE_RE.match(p) for p in staged) and "CHANGELOG.md" not in staged:
        notes.append("C++ sources are staged but CHANGELOG.md is not - add an [Unreleased] entry "
                     "for user-visible changes (skip for pure refactors/tests).")

    added_todos = [l for l in git("diff", "--cached", "-U0", "--", "src", "include", "tests").splitlines() if TODO_RE.match(l)]
    if added_todos:
        notes.append(f"The staged diff adds {len(added_todos)} TODO/FIXME line(s) - resolve them "
                     "or reference a tracked issue.")

    if notes:
        emit_context("PreToolUse", "[PRE-COMMIT] " + " ".join(notes))


if __name__ == "__main__":
    main()
