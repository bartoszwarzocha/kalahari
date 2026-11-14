#!/usr/bin/env python3
"""Drop the hunks of a unified diff that add only line comments or blank lines.

Reads a diff made with -U0 on stdin and writes it to stdout without such hunks,
for clang-tidy-diff in .github/workflows/ci-linux.yml: none of the checks in
.clang-tidy reports on a // comment, but clang-tidy-diff would still analyse every
file of the hunk. A pull request that adds a licence header to every file would
otherwise run clang-tidy on the whole code base.

Block comments are kept: a line starting with "*" may also be a dereference.
"""
import re
import sys

COMMENT_OR_BLANK = re.compile(r"^\s*(//.*)?$")
FILE_LINES = ("diff --git", "--- ", "+++ ", "index ", "new file", "deleted file",
              "similarity", "rename ", "old mode", "new mode")


def main() -> None:
    output = []
    hunk = []

    def flush() -> None:
        added = [line[1:] for line in hunk[1:] if line.startswith("+")]
        if any(not COMMENT_OR_BLANK.match(line.rstrip("\n")) for line in added):
            output.extend(hunk)
        hunk.clear()

    for line in sys.stdin:
        if line.startswith("@@"):
            flush()
            hunk.append(line)
        elif line.startswith(FILE_LINES):
            flush()
            output.append(line)
        elif hunk:
            hunk.append(line)
        else:
            output.append(line)
    flush()
    sys.stdout.write("".join(output))


if __name__ == "__main__":
    main()
