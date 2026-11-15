#!/usr/bin/env python3
"""Run clang-tidy on the changed lines of a pull request.

Reads a diff made with -U0 on stdin (after drop_comment_only_hunks.py) and runs
clang-tidy only where the diff can get a finding, reporting only the changed lines
(-line-filter):

- every changed source file of the compilation database;
- every changed header in one translation unit that includes it (Ninja's dependency log
  of the build): the first changed source that includes it, else the unit that includes
  the most of the headers left. clang-tidy-diff analyses a header on its own, with the
  compile command of a file it guesses from the name, which can lack the header's include
  folders (tests/include/kalahari_tests/gui/document_window.h got the command of
  src/gui/document_windows.cpp); and it analyses a header included by many changed files
  once more for each of them.

Usage (.github/workflows/ci-linux.yml):
    git diff -U0 origin/main...HEAD -- '*.cpp' '*.h' \\
        | python3 .github/scripts/drop_comment_only_hunks.py \\
        | python3 .github/scripts/clang_tidy_changed.py -p build -j 4

Exit status: 1 when clang-tidy fails on a unit, which it does when it reports a warning in
a changed line (.clang-tidy makes every warning an error); 0 otherwise.
"""
import argparse
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple

SOURCE_SUFFIXES = (".cpp", ".cc", ".cxx", ".c")
HEADER_SUFFIXES = (".h", ".hpp", ".hxx")
SUMMARY = re.compile(r"^\d+ warnings? (and \d+ errors? )?generated\.$")


def changed_lines(diff: str) -> Dict[str, List[Tuple[int, int]]]:
    """{repository-relative path: [(first, last) line ranges the diff adds or changes]}."""
    ranges: Dict[str, List[Tuple[int, int]]] = {}
    current = None
    for line in diff.splitlines():
        if line.startswith("+++ "):
            name = line[4:].strip()
            current = name[2:] if name.startswith("b/") else None
        elif line.startswith("@@") and current:
            match = re.search(r"\+(\d+)(?:,(\d+))?", line)
            first, count = int(match.group(1)), int(match.group(2) or "1")
            if count > 0:
                ranges.setdefault(current, []).append((first, first + count - 1))
    return ranges


def key(path: Path) -> str:
    return os.path.normcase(os.path.normpath(str(path)))


def names(path: str) -> List[str]:
    """The names clang-tidy may give a file: it keeps a finding whose file name ends with a
    name of the filter. On Windows the folder of an #include has backslashes and the
    included name slashes, so every split of the path is given."""
    if os.name != "nt":
        return [path]
    parts = path.split("/")
    variants = {path, "\\".join(parts)}
    for split in range(1, len(parts)):
        variants.add("\\".join(parts[:split]) + "\\" + "/".join(parts[split:]))
    return sorted(variants)


def read_dependencies(build: Path, objects: Dict[str, str]) -> Dict[str, Set[str]]:
    """{source key: keys of the headers it includes}, from Ninja's dependency log."""
    ninja = shutil.which("ninja")
    deps = subprocess.run([ninja, "-C", str(build), "-t", "deps"], capture_output=True,
                          text=True, check=False).stdout if ninja else ""
    headers_of: Dict[str, Set[str]] = {}
    current = None
    for line in deps.splitlines():
        if line and not line.startswith((" ", "\t")) and ": #deps" in line:
            output = line.split(": #deps", 1)[0]
            current = objects.get(key(build / output)) if "(VALID)" in line else None
            if current is not None:
                headers_of.setdefault(current, set())
        elif current is not None and line.strip():
            header = Path(line.strip())
            headers_of[current].add(key(header if header.is_absolute() else build / header))
    return headers_of


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("-p", dest="build", required=True, help="build folder")
    parser.add_argument("-j", dest="jobs", type=int, default=os.cpu_count() or 4)
    parser.add_argument("--clang-tidy", default=None, help="clang-tidy executable")
    parser.add_argument("--extra-arg", action="append", default=[],
                        help="extra compiler argument (as clang-tidy's --extra-arg)")
    args = parser.parse_args()

    root = Path.cwd()
    build = (root / args.build).resolve()
    ranges = changed_lines(sys.stdin.read())
    if not ranges:
        print("clang-tidy: no changed C++ line to check")
        return 0

    database = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    units: Dict[str, Path] = {}          # source key -> source path
    objects: Dict[str, str] = {}         # object key -> source key
    for entry in database:
        source = Path(entry["file"])
        if not source.is_absolute():
            source = Path(entry["directory"]) / source
        units.setdefault(key(source), source)
        if "output" in entry:
            objects[key(Path(entry["directory"]) / entry["output"])] = key(source)
    headers_of = read_dependencies(build, objects)

    changed = {key(root / path): path for path in ranges}
    chosen = sorted(unit for unit in units if unit in changed)
    owner: Dict[str, str] = {}           # header key -> the unit that checks it
    headers = {path for path in changed if path.endswith(HEADER_SUFFIXES)}
    for unit in chosen:
        for header in sorted((headers_of.get(unit, set()) & headers) - owner.keys()):
            owner[header] = unit
    left = headers - owner.keys()
    for unit in sorted(units, key=lambda unit: (-len(headers_of.get(unit, set()) & left), unit)):
        if not left:
            break
        covered = headers_of.get(unit, set()) & left
        if covered:
            if unit not in chosen:
                chosen.append(unit)
            for header in covered:
                owner[header] = unit
            left -= covered
    for header in sorted(left):
        print(f"clang-tidy: no translation unit includes {changed[header]} - not checked")
    for path in sorted(changed[path] for path in changed
                       if path.endswith(SOURCE_SUFFIXES) and path not in units):
        print(f"clang-tidy: {path} is not in the compilation database - not checked")
    if not chosen:
        return 0

    def line_filter(unit: str) -> str:
        files = [changed[unit]] if unit in changed else []
        files += sorted(changed[header] for header, checker in owner.items() if checker == unit)
        return json.dumps([{"name": name, "lines": [list(span) for span in ranges[path]]}
                           for path in files for name in names(path)])

    clang_tidy = args.clang_tidy or shutil.which("clang-tidy") or "clang-tidy"
    print(f"clang-tidy: {len(chosen)} translation units for {len(ranges)} changed files",
          flush=True)

    def run(unit: str) -> Tuple[int, str]:
        command = [clang_tidy, "-p", str(build), "-quiet", f"-line-filter={line_filter(unit)}"]
        command += [f"--extra-arg={extra}" for extra in args.extra_arg]
        result = subprocess.run(command + [str(units[unit])], capture_output=True, text=True,
                                check=False)
        return result.returncode, result.stdout + result.stderr

    failed = False
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for returncode, output in pool.map(run, sorted(chosen)):
            text = "\n".join(line for line in output.splitlines() if not SUMMARY.match(line))
            if text.strip():
                print(text.strip(), flush=True)
            failed = failed or returncode != 0
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
