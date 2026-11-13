#!/usr/bin/env python3
"""
release.py - The version number of Kalahari and the cut of its CHANGELOG.

The version of the program is written in five files: CMakeLists.txt (the number and the
release stage), vcpkg.json, README.md, ROADMAP.md and CHANGELOG.md (the section of the
release). This script changes them together and checks that they agree.

Usage:
    python scripts/release.py check [--tag TAG]
        Check that the files name the same version. With --tag, also that the tag is
        "v" and that version (the release workflow passes the tag it was started by).

    python scripts/release.py bump VERSION [--suffix SUFFIX] [--date YYYY-MM-DD]
        Set the version (e.g. 0.4.0, release stage "alpha" unless --suffix is given;
        --suffix "" for a final release) and cut the CHANGELOG: the entries of
        [Unreleased] become the section of the version, dated --date (today by default),
        with one heading for each kind of change, and [Unreleased] starts empty.

    python scripts/release.py notes [--version VERSION]
        Print the notes of a release for its page on GitHub: the summary written under
        the heading of the version in the CHANGELOG (the text before its first "###"),
        and a link to the CHANGELOG of the tag. The current version by default.

Exit code 0 on success, 1 when a check fails or a file cannot be changed.
"""

import argparse
import datetime
import json
import re
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parent.parent

# The order of the kinds of change of Keep a Changelog 1.1.0; other kinds follow them
CATEGORY_ORDER = ["Added", "Changed", "Deprecated", "Removed", "Fixed", "Security"]

UNRELEASED_HEADING = "## [Unreleased]"
REPOSITORY_URL = "https://github.com/bartoszwarzocha/kalahari"

CMAKE_VERSION_RE = re.compile(r"(project\(Kalahari\s+VERSION\s+)(\d+\.\d+\.\d+)")
CMAKE_SUFFIX_RE = re.compile(r'(set\(KALAHARI_VERSION_SUFFIX\s+")([^"]*)("\))')
VCPKG_VERSION_RE = re.compile(r'^(\s*"version"\s*:\s*")([^"]*)(")', re.MULTILINE)
README_VERSION_RE = re.compile(r"^(\*\*Current Version:\*\*\s+)(\S+)", re.MULTILINE)
ROADMAP_VERSION_RE = re.compile(r"^(\*\*Version:\*\*\s+)(\S+)", re.MULTILINE)
VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")
SUFFIX_RE = re.compile(r"^[0-9A-Za-z.]*$")
DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")


class ReleaseError(Exception):
    """A file that cannot be read or changed as the release needs."""


# =============================================================================
# Files
# =============================================================================


def read_text(path: Path) -> str:
    try:
        with open(path, encoding="utf-8", newline="") as stream:
            return stream.read()
    except OSError as error:
        raise ReleaseError(f"{path.name}: cannot be read ({error.strerror})") from error


def write_text(path: Path, text: str) -> None:
    with open(path, "w", encoding="utf-8", newline="") as stream:
        stream.write(text)


def line_end_of(text: str) -> str:
    return "\r\n" if "\r\n" in text else "\n"


def version_string(number: str, suffix: str) -> str:
    return f"{number}-{suffix}" if suffix else number


def replace_one(pattern: "re.Pattern[str]", text: str, value: str, name: str) -> str:
    """Put value in place of group 2 of the only match of pattern."""
    matches = list(pattern.finditer(text))
    if len(matches) != 1:
        raise ReleaseError(f"{name}: expected one version field, found {len(matches)}")
    match = matches[0]
    return text[: match.start(2)] + value + text[match.end(2) :]


def find_one(pattern: "re.Pattern[str]", text: str, name: str, group: int = 2) -> str:
    matches = list(pattern.finditer(text))
    if len(matches) != 1:
        raise ReleaseError(f"{name}: expected one version field, found {len(matches)}")
    return matches[0].group(group)


def cmake_version(root: Path) -> Tuple[str, str]:
    """The number (X.Y.Z) and the release stage of CMakeLists.txt."""
    text = read_text(root / "CMakeLists.txt")
    number = find_one(CMAKE_VERSION_RE, text, "CMakeLists.txt")
    suffix = find_one(CMAKE_SUFFIX_RE, text, "CMakeLists.txt (KALAHARI_VERSION_SUFFIX)")
    return number, suffix


# =============================================================================
# CHANGELOG
# =============================================================================


def section_heading_re(version: str) -> "re.Pattern[str]":
    return re.compile(r"^## \[" + re.escape(version) + r"\] - (\d{4}-\d{2}-\d{2})\s*$")


def split_lines(text: str) -> List[str]:
    return text.splitlines()


def find_unreleased(lines: List[str]) -> Tuple[int, int]:
    """The index of the [Unreleased] heading and of the first line after its section."""
    starts = [i for i, line in enumerate(lines) if line.rstrip() == UNRELEASED_HEADING]
    if len(starts) != 1:
        raise ReleaseError(f"CHANGELOG.md: expected one {UNRELEASED_HEADING}, found {len(starts)}")
    start = starts[0]
    end = len(lines)
    for i in range(start + 1, len(lines)):
        if lines[i].startswith("## "):
            end = i
            break
    return start, end


def strip_blank_edges(lines: List[str]) -> List[str]:
    first = 0
    last = len(lines)
    while first < last and not lines[first].strip():
        first += 1
    while last > first and not lines[last - 1].strip():
        last -= 1
    return lines[first:last]


def strip_separator(lines: List[str]) -> List[str]:
    """The body of a section without the "---" that closes it."""
    body = strip_blank_edges(lines)
    while body and body[-1].strip() == "---":
        body = strip_blank_edges(body[:-1])
    return body


def group_categories(body: List[str]) -> Tuple[List[str], Dict[str, List[List[str]]], List[str]]:
    """The summary before the first "###", the blocks of each kind of change, their order."""
    summary: List[str] = []
    blocks: Dict[str, List[List[str]]] = {}
    seen: List[str] = []
    current: Optional[List[str]] = None
    for line in body:
        if line.startswith("### "):
            name = line[4:].strip()
            if name not in blocks:
                blocks[name] = []
                seen.append(name)
            current = []
            blocks[name].append(current)
        elif current is None:
            summary.append(line)
        else:
            current.append(line)
    known = [name for name in CATEGORY_ORDER if name in blocks]
    others = [name for name in seen if name not in CATEGORY_ORDER]
    return strip_blank_edges(summary), blocks, known + others


def cut_changelog(text: str, version: str, date: str) -> str:
    """The CHANGELOG with the entries of [Unreleased] moved to the section of version."""
    newline = line_end_of(text)
    lines = split_lines(text)
    if any(section_heading_re(version).match(line) for line in lines):
        raise ReleaseError(f"CHANGELOG.md: the section [{version}] is already there")

    start, end = find_unreleased(lines)
    body = strip_separator(lines[start + 1 : end])
    summary, blocks, order = group_categories(body)
    if not order:
        raise ReleaseError("CHANGELOG.md: [Unreleased] has no entries to release")

    section: List[str] = [f"## [{version}] - {date}", ""]
    if summary:
        section += summary + [""]
    for name in order:
        entries: List[str] = []
        for block in blocks[name]:
            block = strip_blank_edges(block)
            if block:
                if entries:
                    entries.append("")
                entries += block
        section += [f"### {name}", ""] + entries + [""]
    section += ["---", ""]

    released = [UNRELEASED_HEADING, "", "---", ""] + section
    result = lines[:start] + released + lines[end:]
    return newline.join(result) + newline


def section_of(text: str, version: str) -> Tuple[str, List[str]]:
    """The date and the lines of the section of version."""
    lines = split_lines(text)
    heading = section_heading_re(version)
    for i, line in enumerate(lines):
        match = heading.match(line)
        if match:
            end = len(lines)
            for j in range(i + 1, len(lines)):
                if lines[j].startswith("## "):
                    end = j
                    break
            return match.group(1), strip_separator(lines[i + 1 : end])
    raise ReleaseError(f"CHANGELOG.md: no section [{version}] with a date")


def release_notes(text: str, version: str) -> str:
    _date, body = section_of(text, version)
    summary, _blocks, _order = group_categories(body)
    if not summary:
        raise ReleaseError(f"CHANGELOG.md: the section [{version}] has no summary before its first ###")
    link = f"{REPOSITORY_URL}/blob/v{version}/CHANGELOG.md"
    return "\n".join(summary) + f"\n\nEvery change of this version is in [CHANGELOG.md]({link}).\n"


# =============================================================================
# Commands
# =============================================================================


def check(root: Path, tag: Optional[str]) -> List[str]:
    """The disagreements between the files; empty when they all name one version."""
    problems: List[str] = []
    number, suffix = cmake_version(root)
    version = version_string(number, suffix)

    vcpkg = read_text(root / "vcpkg.json")
    try:
        manifest = json.loads(vcpkg)
    except ValueError as error:
        raise ReleaseError(f"vcpkg.json: not valid JSON ({error})") from error
    if manifest.get("version") != number:
        problems.append(f'vcpkg.json: "version" is {manifest.get("version")!r}, CMakeLists.txt has {number!r}')

    readme = find_one(README_VERSION_RE, read_text(root / "README.md"), "README.md (**Current Version:**)")
    if readme != version:
        problems.append(f"README.md: **Current Version:** is {readme!r}, CMakeLists.txt has {version!r}")

    roadmap = find_one(ROADMAP_VERSION_RE, read_text(root / "ROADMAP.md"), "ROADMAP.md (**Version:**)")
    if roadmap != version:
        problems.append(f"ROADMAP.md: **Version:** is {roadmap!r}, CMakeLists.txt has {version!r}")

    changelog = read_text(root / "CHANGELOG.md")
    lines = split_lines(changelog)
    headings = [line.rstrip() for line in lines if line.startswith("## ")]
    if not headings or headings[0] != UNRELEASED_HEADING:
        problems.append(f"CHANGELOG.md: the first section is not {UNRELEASED_HEADING}")
    if headings.count(UNRELEASED_HEADING) > 1:
        problems.append(f"CHANGELOG.md: {UNRELEASED_HEADING} is there {headings.count(UNRELEASED_HEADING)} times")
    if not any(section_heading_re(version).match(line) for line in lines):
        problems.append(f"CHANGELOG.md: no section \"## [{version}] - YYYY-MM-DD\"")

    if tag is not None and tag != f"v{version}":
        problems.append(f"tag {tag!r} is not the version of the files, v{version}")
    return problems


def bump(root: Path, number: str, suffix: str, date: str) -> str:
    if not VERSION_RE.match(number):
        raise ReleaseError(f"version {number!r} is not MAJOR.MINOR.PATCH")
    if not SUFFIX_RE.match(suffix):
        raise ReleaseError(f"release stage {suffix!r} may have only letters, digits and dots")
    if not DATE_RE.match(date):
        raise ReleaseError(f"date {date!r} is not YYYY-MM-DD")
    try:
        datetime.date.fromisoformat(date)
    except ValueError as error:
        raise ReleaseError(f"date {date!r} is not a day of the calendar") from error
    version = version_string(number, suffix)

    # Every file is changed in memory first, so a file that cannot be changed changes nothing
    changed: Dict[Path, str] = {}
    cmake = read_text(root / "CMakeLists.txt")
    cmake = replace_one(CMAKE_VERSION_RE, cmake, number, "CMakeLists.txt")
    changed[root / "CMakeLists.txt"] = replace_one(CMAKE_SUFFIX_RE, cmake, suffix, "CMakeLists.txt (KALAHARI_VERSION_SUFFIX)")
    changed[root / "vcpkg.json"] = replace_one(VCPKG_VERSION_RE, read_text(root / "vcpkg.json"), number, "vcpkg.json")
    changed[root / "README.md"] = replace_one(README_VERSION_RE, read_text(root / "README.md"), version, "README.md (**Current Version:**)")
    changed[root / "ROADMAP.md"] = replace_one(ROADMAP_VERSION_RE, read_text(root / "ROADMAP.md"), version, "ROADMAP.md (**Version:**)")
    changed[root / "CHANGELOG.md"] = cut_changelog(read_text(root / "CHANGELOG.md"), version, date)

    for path, text in changed.items():
        write_text(path, text)
    return version


def main(argv: Optional[List[str]] = None, root: Path = REPO_ROOT) -> int:
    parser = argparse.ArgumentParser(description="The version number of Kalahari and the cut of its CHANGELOG.")
    commands = parser.add_subparsers(dest="command", required=True)

    check_parser = commands.add_parser("check", help="check that the files name the same version")
    check_parser.add_argument("--tag", help="a tag that must be v and the version, e.g. v0.4.0-alpha")

    bump_parser = commands.add_parser("bump", help="set the version and cut the CHANGELOG")
    bump_parser.add_argument("version", help="MAJOR.MINOR.PATCH, e.g. 0.4.0")
    bump_parser.add_argument("--suffix", default="alpha", help='release stage, "alpha" by default; "" for none')
    bump_parser.add_argument("--date", default=datetime.date.today().isoformat(), help="date of the release, YYYY-MM-DD")

    notes_parser = commands.add_parser("notes", help="print the notes of a release for its GitHub page")
    notes_parser.add_argument("--version", help="the version, e.g. 0.4.0-alpha; the current one by default")

    args = parser.parse_args(argv)
    try:
        if args.command == "check":
            problems = check(root, args.tag)
            for problem in problems:
                print(f"error: {problem}", file=sys.stderr)
            if problems:
                return 1
            number, suffix = cmake_version(root)
            print(f"Version {version_string(number, suffix)}: CMakeLists.txt, vcpkg.json, README.md, ROADMAP.md and CHANGELOG.md agree")
            return 0
        if args.command == "bump":
            version = bump(root, args.version, args.suffix, args.date)
            print(f"Version {version}, dated {args.date}: CMakeLists.txt, vcpkg.json, README.md, ROADMAP.md and CHANGELOG.md changed")
            return 0
        version = args.version or version_string(*cmake_version(root))
        sys.stdout.write(release_notes(read_text(root / "CHANGELOG.md"), version))
        return 0
    except ReleaseError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
