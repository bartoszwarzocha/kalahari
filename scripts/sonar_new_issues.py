#!/usr/bin/env python3
"""
sonar_new_issues.py - SonarQube issues in the lines a branch changes.

SonarQube Community Build analyses one branch per project. The branch of a piece of
work is analysed into a project of its own (sonar_analysis.py --project), where the
first analysis counts every line as new; this script keeps the issues, security
hotspots and coverage of the lines the branch adds or changes since a commit (its
merge base with main), which is what the quality gate checks after the merge.

Usage (in the repository, with the branch checked out and analysed):
    python scripts/sonar_new_issues.py --project kalahari-check --base origin/main
    python scripts/sonar_new_issues.py --project kalahari-check --base origin/main \\
        --coverage-report build-windows/sonar/coverage/coverage-report/sonarqube.xml

Options:
    --project KEY       SonarQube project of the analysis of the branch
    --base COMMIT       the lines changed since this commit count (default: origin/main;
                        the merge base of HEAD and COMMIT is taken)
    --coverage-report F SonarQube generic coverage report (CI artifact "coverage-report")
                        for the coverage of the changed lines
    --host URL          SonarQube server (default: $SONAR_HOST_URL or http://localhost:9000)
    --json FILE         also write the result as JSON

The SONAR_TOKEN environment variable holds the SonarQube user token.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ElementTree
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Set

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_HOST = "http://localhost:9000"
SEVERITY_ORDER = ["BLOCKER", "CRITICAL", "MAJOR", "MINOR", "INFO"]
PAGE_SIZE = 500


def fail(message: str) -> None:
    print(f"[ERROR] {message}", file=sys.stderr)
    sys.exit(1)


def git(*args: str) -> str:
    result = subprocess.run(["git", *args], cwd=REPO_ROOT, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", check=False)
    if result.returncode != 0:
        fail(f"git {' '.join(args)}: {result.stderr.strip()}")
    return result.stdout


def changed_lines(base: str) -> Dict[str, Set[int]]:
    """{repository-relative path: numbers of the lines HEAD adds or changes since base}."""
    merge_base = git("merge-base", "HEAD", base).strip()
    lines: Dict[str, Set[int]] = defaultdict(set)
    current: Optional[str] = None
    for line in git("diff", "-U0", "--no-color", "--no-renames", merge_base, "HEAD").splitlines():
        if line.startswith("+++ "):
            name = line[4:]
            current = name[2:] if name.startswith("b/") else None
        elif line.startswith("@@") and current:
            match = re.search(r"\+(\d+)(?:,(\d+))?", line)
            first, count = int(match.group(1)), int(match.group(2) or "1")
            lines[current].update(range(first, first + count))
    return {path: numbers for path, numbers in lines.items() if numbers}


class SonarQube:
    def __init__(self, host: str, token: str):
        self.host = host.rstrip("/")
        self.token = token

    def get(self, path: str, **params) -> dict:
        url = f"{self.host}{path}?{urllib.parse.urlencode(params)}"
        request = urllib.request.Request(url)
        request.add_header("Authorization", f"Bearer {self.token}")
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        try:
            with opener.open(request, timeout=120) as response:
                return json.loads(response.read().decode("utf-8"))
        except urllib.error.HTTPError as error:
            fail(f"GET {path}: HTTP {error.code} {error.read().decode('utf-8', 'replace')[:300]}")
        except urllib.error.URLError as error:
            fail(f"GET {path}: {error.reason} - is SonarQube running at {self.host}?")
        return {}

    def pages(self, path: str, key: str, **params) -> List[dict]:
        items: List[dict] = []
        page = 1
        while True:
            data = self.get(path, ps=PAGE_SIZE, p=page, **params)
            items.extend(data.get(key, []))
            total = data.get("paging", {}).get("total", data.get("total", 0))
            if page * PAGE_SIZE >= total or not data.get(key):
                return items
            page += 1


def severity_of(issue: dict) -> str:
    """The severity SonarQube shows: the highest of the issue's impacts, else the old one."""
    impacts = [impact.get("severity", "") for impact in issue.get("impacts", [])]
    named = {"HIGH": "CRITICAL", "MEDIUM": "MAJOR", "LOW": "MINOR"}
    candidates = [named.get(value, value) for value in impacts] or [issue.get("severity", "INFO")]
    return min(candidates, key=lambda value: SEVERITY_ORDER.index(value)
               if value in SEVERITY_ORDER else len(SEVERITY_ORDER))


def coverage_of_lines(report: Path, lines: Dict[str, Set[int]]) -> Optional[tuple]:
    """(covered, to cover) among the changed lines, from a generic coverage report."""
    if not report.exists():
        fail(f"{report} not found")
    covered = to_cover = 0
    for file in ElementTree.parse(report).getroot().iter("file"):
        path = file.get("path", "").replace("\\", "/")
        relative = path[len(str(REPO_ROOT).replace("\\", "/")) + 1:] \
            if os.path.isabs(path) else path
        wanted = lines.get(relative)
        if not wanted:
            continue
        for line in file.iter("lineToCover"):
            if int(line.get("lineNumber", "0")) in wanted:
                to_cover += 1
                covered += line.get("covered") == "true"
    return (covered, to_cover) if to_cover else None


def main() -> None:
    sys.stdout.reconfigure(line_buffering=True)
    parser = argparse.ArgumentParser(description="SonarQube issues in the changed lines")
    parser.add_argument("--project", required=True)
    parser.add_argument("--base", default="origin/main")
    parser.add_argument("--coverage-report", type=Path)
    parser.add_argument("--host")
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    token = os.environ.get("SONAR_TOKEN")
    if not token:
        fail("SONAR_TOKEN is not set - the SonarQube API needs a user token")
    server = SonarQube(args.host or os.environ.get("SONAR_HOST_URL") or DEFAULT_HOST, token)
    lines = changed_lines(args.base)
    head = git("rev-parse", "--short", "HEAD").strip()
    print(f"[INFO] {len(lines)} files changed since {args.base} (HEAD {head})")

    issues = []
    hotspots = []
    for path in sorted(lines):
        component = f"{args.project}:{path}"
        for issue in server.pages("/api/issues/search", "issues", components=component,
                                  resolved="false"):
            line = issue.get("line")
            if line is None or line in lines[path]:
                issues.append({"file": path, "line": line, "rule": issue["rule"],
                               "severity": severity_of(issue), "type": issue.get("type", ""),
                               "message": issue.get("message", "")})
        for hotspot in server.pages("/api/hotspots/search", "hotspots", project=args.project,
                                    files=path):
            line = hotspot.get("line")
            if line is None or line in lines[path]:
                hotspots.append({"file": path, "line": line,
                                 "rule": hotspot.get("ruleKey", ""),
                                 "message": hotspot.get("message", "")})

    issues.sort(key=lambda item: (SEVERITY_ORDER.index(item["severity"])
                                  if item["severity"] in SEVERITY_ORDER else 9,
                                  item["file"], item["line"] or 0))
    counts = Counter(item["severity"] for item in issues)
    print(f"[RESULT] {len(issues)} issues in the changed lines: " +
          ", ".join(f"{name} {counts[name]}" for name in SEVERITY_ORDER if counts[name]))
    for item in issues:
        where = f"{item['file']}:{item['line']}" if item["line"] else f"{item['file']} (file)"
        print(f"  {item['severity']:<8} {item['rule']:<55} {where}  {item['message']}")
    print(f"[RESULT] {len(hotspots)} security hotspots in the changed lines")
    for item in hotspots:
        print(f"  {item['rule']:<55} {item['file']}:{item['line']}  {item['message']}")

    coverage = None
    if args.coverage_report:
        coverage = coverage_of_lines(args.coverage_report, lines)
        if coverage:
            print(f"[RESULT] coverage of the changed lines: "
                  f"{100.0 * coverage[0] / coverage[1]:.1f}% ({coverage[0]}/{coverage[1]})")
        else:
            print("[RESULT] the coverage report has no line to cover among the changed lines")

    if args.json:
        args.json.write_text(json.dumps({"head": head, "base": args.base, "issues": issues,
                                         "hotspots": hotspots, "coverage": coverage},
                                        ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
