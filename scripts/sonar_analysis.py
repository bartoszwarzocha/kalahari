#!/usr/bin/env python3
"""
sonar_analysis.py - SonarQube analysis of Kalahari.

C++ is analysed by the community plugin sonar-cxx: SonarQube Community Build
has no C/C++ analyser of its own. The plugin reads the compilation database
and imports the clang-tidy report written by this script; the other languages
(Python, YAML, JSON) are analysed by SonarQube itself.

Steps:
  1. read the compilation database written by CMake
     (build-windows/compile_commands.json on Windows, build-linux/ elsewhere);
  2. run clang-tidy on every project translation unit (src/ and tests/) in
     parallel and write one report without duplicates:
     <build dir>/sonar/clang-tidy.txt (+ clang-tidy-summary.json with counts);
  3. take the line coverage of the analysed commit from its CI run (artifact
     "coverage-report" of .github/workflows/ci-linux.yml, downloaded with gh);
  4. run sonar-scanner in the repository root (what is analysed is defined in
     sonar-project.properties).

clang-tidy runs with every check but one (DEFAULT_CHECKS) by default. Which of
them count is decided by the quality profile of the project in SonarQube: the
plugin imports only the issues of rules that are active in that profile.

A file whose source, compile command and headers did not change since an earlier
run takes its result from <build dir>/sonar/clang-tidy-cache (the headers come from
Ninja's dependency log of the last build), so after a small change only the files
it touches are analysed again.

Requirements:
  - a finished build (scripts/build_windows.bat Debug or scripts/build_linux.sh):
    the analysis needs the files that Qt generates during the build;
  - clang-tidy (LLVM) and SonarScanner CLI;
  - the SONAR_TOKEN environment variable with a SonarQube user token
    (on the author's PC it is set by C:\\SonarQube\\token.ps1).

Usage:
    python scripts/sonar_analysis.py                    # clang-tidy, then the scan
    python scripts/sonar_analysis.py --skip-scan        # clang-tidy only
    python scripts/sonar_analysis.py --skip-clang-tidy  # scan with the existing report
    python scripts/sonar_analysis.py --only "src/editor/.*"  # clang-tidy on some files

Options:
    --build-dir DIR     build directory with compile_commands.json
    --checks GLOBS      clang-tidy checks (default: every check but one, see DEFAULT_CHECKS)
    --no-cache          analyse every file again (by default unchanged files reuse their result)
    --jobs N            parallel clang-tidy processes (default: number of CPUs)
    --clang-tidy PATH   clang-tidy executable (default: from PATH or LLVM's folder)
    --scanner PATH      sonar-scanner executable (default: $SONAR_SCANNER or PATH)
    --host URL          SonarQube server (default: $SONAR_HOST_URL or http://localhost:9000)
    --coverage-report F SonarQube generic coverage report to import instead of the CI one
    --no-coverage       scan without a coverage report
"""

import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from collections import Counter
from pathlib import Path
from typing import Dict, List, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parent.parent
PROJECT_KEY = "kalahari"
DEFAULT_HOST = "http://localhost:9000"
# Project translation units are the ones under these top-level folders
SOURCE_DIRS = ("src", "tests")
# Headers whose diagnostics are reported (dependencies and generated code are not)
HEADER_DIRS = ("include", "src", "tests")
CLANG_TIDY_TIMEOUT_S = 1800
# Every check except llvm-prefer-static-over-anonymous-namespace: it contradicts
# misc-use-anonymous-namespace, and the project uses anonymous namespaces (decision of
# 2026-11-14). sonar-cxx does not know that check (its findings land in the one rule
# "Unknown Clang-Tidy rule"), so it cannot be turned off in the SonarQube profile.
DEFAULT_CHECKS = "*,-llvm-prefer-static-over-anonymous-namespace"

# "file:line:column: level: message [check]" - the file may start with a drive letter
DIAGNOSTIC_LINE = re.compile(
    r"^(?P<file>(?:[A-Za-z]:)?[^:]+):(?P<line>\d+):(?P<col>\d+): "
    r"(?P<level>warning|error|note): (?P<message>.*?)(?: \[(?P<check>[^\]]+)\])?$"
)


def fail(message: str) -> None:
    print(f"[ERROR] {message}", file=sys.stderr)
    sys.exit(1)


def default_build_dir() -> Path:
    return REPO_ROOT / ("build-windows" if os.name == "nt" else "build-linux")


def msvc_environment() -> Optional[Dict[str, str]]:
    """Environment of vcvarsall x64, found the same way as scripts/build_windows.bat.

    clang-tidy reads MSVC compile commands; with this environment it finds the same
    standard library and Windows SDK headers as the build.
    """
    if os.name != "nt" or os.environ.get("VCINSTALLDIR"):
        return None
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    vswhere = vswhere / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        print("[WARNING] vswhere.exe not found - clang-tidy runs in the current environment")
        return None
    query = [str(vswhere), "-latest", "-prerelease", "-products", "*", "-requires",
             "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"]
    install_dir = subprocess.run(query, capture_output=True, text=True, check=False).stdout.strip()
    vcvarsall = Path(install_dir) / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
    if not install_dir or not vcvarsall.exists():
        print("[WARNING] vcvarsall.bat not found - clang-tidy runs in the current environment")
        return None
    output = subprocess.run(f'cmd /s /c ""{vcvarsall}" x64 >nul 2>&1 && set"',
                            capture_output=True, text=True, check=False).stdout
    environment = dict(line.split("=", 1) for line in output.splitlines() if "=" in line)
    if "VCINSTALLDIR" not in environment:
        print("[WARNING] vcvarsall.bat failed - clang-tidy runs in the current environment")
        return None
    print(f"[OK] MSVC environment: {install_dir}")
    return environment


def find_clang_tidy(explicit: Optional[str]) -> str:
    candidates = [explicit, shutil.which("clang-tidy")]
    if os.name == "nt":
        candidates.append(r"C:\Program Files\LLVM\bin\clang-tidy.exe")
    for candidate in candidates:
        if candidate and Path(candidate).exists():
            return candidate
    fail("clang-tidy not found - install LLVM or pass --clang-tidy")
    return ""


def find_scanner(explicit: Optional[str]) -> str:
    candidates = [explicit, os.environ.get("SONAR_SCANNER"), shutil.which("sonar-scanner")]
    for candidate in candidates:
        if candidate and Path(candidate).exists():
            return candidate
    fail("sonar-scanner not found - add it to PATH, set SONAR_SCANNER or pass --scanner")
    return ""


def normalized(path: Path) -> str:
    return os.path.normcase(os.path.normpath(str(path)))


def project_translation_units(database: Path, only: Optional[str]) -> List[Path]:
    """Source files of the project from the compilation database, each one once."""
    entries = json.loads(database.read_text(encoding="utf-8"))
    roots = [normalized(REPO_ROOT / name) + os.sep for name in SOURCE_DIRS]
    pattern = re.compile(only) if only else None
    units: Dict[str, Path] = {}
    for entry in entries:
        path = Path(entry["file"])
        if not path.is_absolute():
            path = Path(entry["directory"]) / path
        key = normalized(path)
        if not any(key.startswith(root) for root in roots):
            continue
        relative = path.resolve().relative_to(REPO_ROOT).as_posix()
        if pattern and not pattern.fullmatch(relative):
            continue
        units.setdefault(key, path)
    return sorted(units.values(), key=lambda unit: normalized(unit))


def header_filter() -> str:
    """Regular expression for the project's own headers (clang-tidy --header-filter).

    Anchored at the repository root, so headers of dependencies installed under
    build-*/vcpkg_installed/.../include are not matched. Both path separators are
    accepted, because clang prints the paths the way the include flags spell them.
    """
    root = str(REPO_ROOT)
    pattern = ""
    for char in root:
        if char in "\\/":
            pattern += "[/\\\\]"
        elif char.isalpha() and len(pattern) == 0 and os.name == "nt":
            pattern += f"[{char.upper()}{char.lower()}]"  # drive letter
        elif char in ".^$|?*+()[]{}":
            pattern += "\\" + char
        else:
            pattern += char
    return f"^{pattern}[/\\\\]({'|'.join(HEADER_DIRS)})[/\\\\].*"


def unique_database(database: Path, target_dir: Path) -> Tuple[Path, Dict[str, dict]]:
    """Copy of the compilation database with one command per source file.

    clang-tidy runs every command it finds for a file; a file built by two targets
    would be analysed twice for the same findings. Returns the folder of the copy and
    its entries by normalized source path.
    """
    entries = json.loads(database.read_text(encoding="utf-8"))
    unique: Dict[str, dict] = {}
    for entry in entries:
        path = Path(entry["file"])
        if not path.is_absolute():
            path = Path(entry["directory"]) / path
        unique.setdefault(normalized(path), entry)
    target_dir.mkdir(parents=True, exist_ok=True)
    target = target_dir / "compile_commands.json"
    target.write_text(json.dumps(list(unique.values()), indent=1), encoding="utf-8")
    return target_dir, unique


def ninja_dependencies(build_dir: Path, environment: Optional[Dict[str, str]]) -> Dict[str, List[str]]:
    """Headers of every object file as Ninja recorded them during the last build.

    Returns {normalized object path: [header paths]}; empty when Ninja is not available.
    """
    path = (environment or os.environ).get("PATH")
    ninja = shutil.which("ninja", path=path)
    if not ninja:
        return {}
    result = subprocess.run([ninja, "-C", str(build_dir), "-t", "deps"], capture_output=True,
                            text=True, encoding="utf-8", errors="replace", env=environment,
                            check=False)
    dependencies: Dict[str, List[str]] = {}
    current: Optional[List[str]] = None
    for line in result.stdout.splitlines():
        if not line.strip():
            current = None
        elif not line.startswith((" ", "\t")) and ": #deps" in line:
            output = line.split(": #deps", 1)[0]
            current = [] if "(VALID)" in line else None
            if current is not None:
                dependencies[normalized(build_dir / output)] = current
        elif current is not None:
            header = line.strip()            # relative paths are relative to the build folder
            current.append(header if os.path.isabs(header) else str(build_dir / header))
    return dependencies


class ResultCache:
    """clang-tidy results of files whose source, headers and command did not change.

    The key of a file covers the clang-tidy version and options, its compile command,
    the content of the file and of every project header it includes (from Ninja's
    dependency log) and the size and time of the headers outside the project.
    """

    def __init__(self, folder: Path, settings: List[str]):
        self.folder = folder
        self.settings = settings
        self.content_hashes: Dict[str, str] = {}
        folder.mkdir(parents=True, exist_ok=True)

    def _file_signature(self, path: str) -> str:
        key = normalized(Path(path))
        if key.startswith(normalized(REPO_ROOT) + os.sep):
            if key not in self.content_hashes:
                try:
                    self.content_hashes[key] = hashlib.sha256(Path(path).read_bytes()).hexdigest()
                except OSError:
                    self.content_hashes[key] = "missing"
            return f"{key}={self.content_hashes[key]}"
        try:
            info = os.stat(path)
            return f"{key}={info.st_size}:{info.st_mtime_ns}"
        except OSError:
            return f"{key}=missing"

    def key(self, unit: Path, entry: dict, headers: Optional[List[str]]) -> Optional[str]:
        if headers is None:
            return None                      # no dependency record: always run clang-tidy
        parts = [*self.settings, entry.get("command") or json.dumps(entry.get("arguments")),
                 self._file_signature(str(unit)),
                 *sorted(self._file_signature(header) for header in headers)]
        return hashlib.sha256("\n".join(parts).encode("utf-8")).hexdigest()

    def load(self, key: Optional[str]) -> Optional[Tuple[int, str, str]]:
        if key is None:
            return None
        file = self.folder / f"{key}.json"
        if not file.exists():
            return None
        data = json.loads(file.read_text(encoding="utf-8"))
        return data["returncode"], data["stdout"], data["stderr"]

    def store(self, key: Optional[str], returncode: int, stdout: str, stderr: str) -> None:
        if key is not None and returncode >= 0:
            (self.folder / f"{key}.json").write_text(
                json.dumps({"returncode": returncode, "stdout": stdout, "stderr": stderr}),
                encoding="utf-8")


def clang_tidy_extra_args() -> List[str]:
    if os.name == "nt":
        # The build treats compiler warnings as errors (/WX), here they are findings;
        # MSVC precompiled headers cannot be read by clang (/Y- turns them off)
        return ["--extra-arg=/WX-", "--extra-arg=/Y-"]
    return ["--extra-arg=-Wno-error"]


def run_clang_tidy(clang_tidy: str, database_dir: Path, unit: Path, checks: str,
                   environment: Optional[Dict[str, str]]) -> Tuple[Path, int, str, str]:
    command = [
        clang_tidy,
        "-p", str(database_dir),
        f"--checks={checks}",
        "--warnings-as-errors=-*",           # report findings as warnings, not errors
        f"--header-filter={header_filter()}",
        "--quiet",
        *clang_tidy_extra_args(),
        str(unit),
    ]
    try:
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                                errors="replace", env=environment, cwd=database_dir,
                                timeout=CLANG_TIDY_TIMEOUT_S, check=False)
        return unit, result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return unit, -1, "", f"timeout after {CLANG_TIDY_TIMEOUT_S} s"


def diagnostic_blocks(output: str) -> List[Tuple[tuple, str, Optional[str]]]:
    """Split clang-tidy output into blocks: a warning/error line with its notes and excerpts."""
    blocks = []
    current_key: Optional[tuple] = None
    current_check: Optional[str] = None
    current_lines: List[str] = []
    for line in output.splitlines():
        match = DIAGNOSTIC_LINE.match(line)
        if match and match.group("level") in ("warning", "error"):
            if current_key is not None:
                blocks.append((current_key, "\n".join(current_lines), current_check))
            current_key = (normalized(Path(match.group("file"))), int(match.group("line")),
                           int(match.group("col")), match.group("level"),
                           match.group("message"), match.group("check"))
            current_check = match.group("check")
            current_lines = [line]
        elif current_key is not None:
            current_lines.append(line)
    if current_key is not None:
        blocks.append((current_key, "\n".join(current_lines), current_check))
    return blocks


def clang_tidy_step(args, build_dir: Path, report: Path) -> None:
    database = build_dir / "compile_commands.json"
    if not database.exists():
        fail(f"{database} not found - build the project first "
             "(scripts/build_windows.bat Debug or scripts/build_linux.sh)")
    units = project_translation_units(database, args.only)
    if not units:
        fail("no project translation units in the compilation database")
    clang_tidy = find_clang_tidy(args.clang_tidy)
    version = subprocess.run([clang_tidy, "--version"], capture_output=True, text=True,
                             check=False).stdout.strip().splitlines()
    environment = msvc_environment()
    database_dir, entries = unique_database(database, report.parent)
    jobs = args.jobs or os.cpu_count() or 4
    print(f"[INFO] clang-tidy: {clang_tidy} ({' '.join(version[1:2]).strip()})")
    print(f"[INFO] {len(units)} translation units, checks: {args.checks}, jobs: {jobs}")

    cache = None if args.no_cache else ResultCache(
        report.parent / "clang-tidy-cache",
        [" ".join(version), args.checks, header_filter(), *clang_tidy_extra_args()])
    dependencies = ninja_dependencies(build_dir, environment) if cache else {}
    if cache and not dependencies:
        print("[WARNING] no Ninja dependency log - every file is analysed again")
    cached: Dict[Path, Tuple[int, str, str]] = {}
    keys: Dict[Path, Optional[str]] = {}
    for unit in units:
        if cache:
            entry = entries[normalized(unit)]
            output = entry.get("output")
            headers = dependencies.get(normalized(Path(entry["directory"]) / output)) if output else None
            keys[unit] = cache.key(unit, entry, headers)
            hit = cache.load(keys[unit])
            if hit:
                cached[unit] = hit
    to_run = [unit for unit in units if unit not in cached]
    if cache:
        print(f"[INFO] {len(cached)} files unchanged since an earlier run, {len(to_run)} to analyse")

    unique: Dict[tuple, str] = {}
    checks = Counter()
    failed_units: List[str] = []
    started = time.monotonic()

    def results():
        for unit, (returncode, stdout, stderr) in cached.items():
            yield unit, returncode, stdout, stderr
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
            futures = [pool.submit(run_clang_tidy, clang_tidy, database_dir, unit, args.checks,
                                   environment) for unit in to_run]
            for future in concurrent.futures.as_completed(futures):
                unit, returncode, stdout, stderr = future.result()
                if cache:
                    cache.store(keys[unit], returncode, stdout, stderr)
                yield unit, returncode, stdout, stderr

    for done, (unit, returncode, stdout, stderr) in enumerate(results(), start=1):
        for key, text, check in diagnostic_blocks(stdout):
            if key not in unique:
                unique[key] = text
                checks[check or "(no check name)"] += 1
        if returncode != 0:
            first_error = next((line for line in (stderr + "\n" + stdout).splitlines()
                                if "error" in line.lower()), stderr.strip()[:200])
            failed_units.append(f"{unit.relative_to(REPO_ROOT).as_posix()}: {first_error}")
        if done % 25 == 0 or done == len(units):
            minutes = (time.monotonic() - started) / 60
            print(f"[INFO] {done}/{len(units)} files, {len(unique)} findings, {minutes:.1f} min")

    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text("\n".join(unique.values()) + "\n", encoding="utf-8")
    summary = {
        "clang_tidy": version[1].strip() if len(version) > 1 else clang_tidy,
        "checks_option": args.checks,
        "translation_units": len(units),
        "findings": len(unique),
        "units_with_errors": failed_units,
        "findings_per_check": dict(checks.most_common()),
    }
    report.with_name("clang-tidy-summary.json").write_text(
        json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"[OK] {len(unique)} findings from {len(checks)} checks -> {report}")
    if failed_units:
        print(f"[WARNING] {len(failed_units)} files ended with an error "
              "(see units_with_errors in clang-tidy-summary.json)")


def ci_coverage_report(target_dir: Path) -> Optional[Path]:
    """Line coverage of the analysed commit from the CI job "Sanitizers, Coverage & clang-tidy".

    The job (.github/workflows/ci-linux.yml) runs the tests with gcov and uploads the
    "coverage-report" artifact with sonarqube.xml (SonarQube generic coverage format,
    paths relative to the repository root). The commit has to be pushed and its CI run
    finished; local changes that are not committed are not measured.
    """
    if not shutil.which("gh"):
        print("[WARNING] GitHub CLI (gh) not found - no coverage report")
        return None
    sha = subprocess.run(["git", "rev-parse", "HEAD"], cwd=REPO_ROOT, capture_output=True,
                         text=True, check=False).stdout.strip()
    dirty = subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"],
                           cwd=REPO_ROOT, capture_output=True, text=True, check=False).stdout
    if dirty.strip():
        print("[WARNING] uncommitted changes: the CI coverage report measures the commit only")
    listed = subprocess.run(["gh", "run", "list", "--workflow", "ci-linux.yml", "--commit", sha,
                             "--json", "databaseId,status,conclusion,event", "--limit", "10"],
                            cwd=REPO_ROOT, capture_output=True, text=True, check=False)
    runs = json.loads(listed.stdout or "[]") if listed.returncode == 0 else []
    finished = [run for run in runs if run.get("status") == "completed"]
    if not finished:
        state = "still running" if runs else "not found"
        print(f"[WARNING] CI run of commit {sha[:8]} {state} - no coverage report; "
              "the coverage condition of the quality gate will fail")
        return None
    if target_dir.exists():
        shutil.rmtree(target_dir)
    download = subprocess.run(["gh", "run", "download", str(finished[0]["databaseId"]),
                               "--name", "coverage-report", "--dir", str(target_dir)],
                              cwd=REPO_ROOT, capture_output=True, text=True, check=False)
    report = target_dir / "sonarqube.xml"
    if download.returncode != 0 or not report.exists():
        print(f"[WARNING] coverage report of CI run {finished[0]['databaseId']} not available "
              f"({(download.stderr or 'no sonarqube.xml in the artifact').strip()[:200]})")
        return None
    print(f"[OK] Coverage: CI run {finished[0]['databaseId']} of commit {sha[:8]}")
    return report


def scan_step(args, build_dir: Path, report: Path) -> None:
    if not os.environ.get("SONAR_TOKEN"):
        fail("SONAR_TOKEN is not set - the scanner needs a SonarQube user token")
    if not report.exists():
        fail(f"{report} not found - run without --skip-clang-tidy first")
    scanner = find_scanner(args.scanner)
    host = args.host or os.environ.get("SONAR_HOST_URL") or DEFAULT_HOST
    database = build_dir / "compile_commands.json"
    command = [
        scanner,
        f"-Dsonar.host.url={host}",
        f"-Dsonar.cxx.jsonCompilationDatabase={database.resolve().relative_to(REPO_ROOT).as_posix()}",
        f"-Dsonar.cxx.clangtidy.reportPaths={report.resolve().relative_to(REPO_ROOT).as_posix()}",
    ]
    coverage = None
    if args.coverage_report:
        coverage = Path(args.coverage_report)
    elif not args.no_coverage:
        coverage = ci_coverage_report(report.parent / "coverage")
    if coverage:
        command.append(f"-Dsonar.coverageReportPaths={coverage.resolve().as_posix()}")
    if os.name == "nt" and scanner.lower().endswith((".bat", ".cmd")):
        command = ["cmd", "/c", *command]
    print(f"[INFO] sonar-scanner: {scanner}")
    result = subprocess.run(command, cwd=REPO_ROOT, check=False)
    if result.returncode != 0:
        fail(f"sonar-scanner ended with code {result.returncode}")
    print(f"[OK] Results: {host}/dashboard?id={PROJECT_KEY}")


def main() -> None:
    # Progress lines must show up at once also when the output goes to a log file
    sys.stdout.reconfigure(line_buffering=True)
    parser = argparse.ArgumentParser(description="SonarQube analysis of Kalahari")
    parser.add_argument("--build-dir", type=Path, default=default_build_dir())
    parser.add_argument("--checks", default=DEFAULT_CHECKS)
    parser.add_argument("--jobs", type=int, default=0)
    parser.add_argument("--only", help="regular expression for repository-relative file paths")
    parser.add_argument("--clang-tidy")
    parser.add_argument("--scanner")
    parser.add_argument("--host")
    parser.add_argument("--coverage-report", help="SonarQube generic coverage report to import")
    parser.add_argument("--no-coverage", action="store_true")
    parser.add_argument("--no-cache", action="store_true")
    parser.add_argument("--skip-clang-tidy", action="store_true")
    parser.add_argument("--skip-scan", action="store_true")
    args = parser.parse_args()

    build_dir = args.build_dir if args.build_dir.is_absolute() else REPO_ROOT / args.build_dir
    report = build_dir / "sonar" / "clang-tidy.txt"
    if not args.skip_clang_tidy:
        clang_tidy_step(args, build_dir, report)
    if not args.skip_scan:
        scan_step(args, build_dir, report)


if __name__ == "__main__":
    main()
