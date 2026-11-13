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
  3. run sonar-scanner in the repository root (what is analysed is defined in
     sonar-project.properties).

clang-tidy runs with every check (--checks=*) by default. Which of them count
is decided by the quality profile of the project in SonarQube: the plugin
imports only the issues of rules that are active in that profile.

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
    --checks GLOBS      clang-tidy checks (default: *)
    --jobs N            parallel clang-tidy processes (default: number of CPUs)
    --clang-tidy PATH   clang-tidy executable (default: from PATH or LLVM's folder)
    --scanner PATH      sonar-scanner executable (default: $SONAR_SCANNER or PATH)
    --host URL          SonarQube server (default: $SONAR_HOST_URL or http://localhost:9000)
"""

import argparse
import concurrent.futures
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


def run_clang_tidy(clang_tidy: str, build_dir: Path, unit: Path, checks: str,
                   environment: Optional[Dict[str, str]]) -> Tuple[Path, int, str, str]:
    command = [
        clang_tidy,
        "-p", str(build_dir),
        f"--checks={checks}",
        "--warnings-as-errors=-*",           # keep "warning:"; errors would rename the check
        f"--header-filter={header_filter()}",
        "--quiet",
        # The build treats compiler warnings as errors (/WX, -Werror); here they are findings
        "--extra-arg=/WX-" if os.name == "nt" else "--extra-arg=-Wno-error",
        str(unit),
    ]
    try:
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                                errors="replace", env=environment, cwd=build_dir,
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
    jobs = args.jobs or os.cpu_count() or 4
    print(f"[INFO] clang-tidy: {clang_tidy} ({' '.join(version[1:2]).strip()})")
    print(f"[INFO] {len(units)} translation units, checks: {args.checks}, jobs: {jobs}")

    unique: Dict[tuple, str] = {}
    checks = Counter()
    failed_units: List[str] = []
    started = time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(run_clang_tidy, clang_tidy, build_dir, unit, args.checks,
                               environment) for unit in units]
        for done, future in enumerate(concurrent.futures.as_completed(futures), start=1):
            unit, returncode, stdout, stderr = future.result()
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


def scan_step(args, build_dir: Path, report: Path) -> None:
    if not os.environ.get("SONAR_TOKEN"):
        fail("SONAR_TOKEN is not set - the scanner needs a SonarQube user token")
    if not report.exists():
        fail(f"{report} not found - run without --skip-clang-tidy first")
    scanner = find_scanner(args.scanner)
    host = args.host or os.environ.get("SONAR_HOST_URL") or DEFAULT_HOST
    relative_build = build_dir.resolve().relative_to(REPO_ROOT).as_posix()
    command = [
        scanner,
        f"-Dsonar.host.url={host}",
        f"-Dsonar.cxx.jsonCompilationDatabase={relative_build}/compile_commands.json",
        f"-Dsonar.cxx.clangtidy.reportPaths={report.resolve().relative_to(REPO_ROOT).as_posix()}",
    ]
    if os.name == "nt" and scanner.lower().endswith((".bat", ".cmd")):
        command = ["cmd", "/c", *command]
    print(f"[INFO] sonar-scanner: {scanner}")
    result = subprocess.run(command, cwd=REPO_ROOT, check=False)
    if result.returncode != 0:
        fail(f"sonar-scanner ended with code {result.returncode}")
    print(f"[OK] Results: {host}/dashboard?id={PROJECT_KEY}")


def main() -> None:
    parser = argparse.ArgumentParser(description="SonarQube analysis of Kalahari")
    parser.add_argument("--build-dir", type=Path, default=default_build_dir())
    parser.add_argument("--checks", default="*")
    parser.add_argument("--jobs", type=int, default=0)
    parser.add_argument("--only", help="regular expression for repository-relative file paths")
    parser.add_argument("--clang-tidy")
    parser.add_argument("--scanner")
    parser.add_argument("--host")
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
