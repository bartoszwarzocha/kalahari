#!/usr/bin/env python3
"""
package.py - The packages of Kalahari: the installers of Windows, macOS and Linux.

Each package is made from a Release build of the program, from what `cmake --install` puts in
the folder of an installation (cmake/Packaging.cmake), and is checked with the program's own
check of an installation (`kalahari --self-test`) before it is written.

Usage:
    python scripts/package.py linux --build BUILD [--out dist] [--tools DIR]
        Kalahari-<version>-x86_64.AppImage and kalahari_<version>_amd64.deb (Ubuntu 24.04,
        Linux Mint 22 and newer). Both carry the program's Qt and Python; linuxdeploy and its
        Qt plugin (pinned below, checked by SHA-256) are downloaded into --tools when they are
        not there. Needs dpkg-deb and dpkg-shlibdeps (the package dpkg-dev).

    python scripts/package.py windows --build BUILD [--out dist] [--makensis PATH]
        Kalahari-<version>-windows-x64.zip (the program without installation) and, with
        makensis (NSIS 3), Kalahari-<version>-windows-x64-setup.exe.

    python scripts/package.py windows-installer --stage DIR [--out dist] [--makensis PATH]
        Only the installer, from the folder of the program made by `windows` (or by
        cmake --install of a Windows build); makensis of Linux compiles it as well.

    python scripts/package.py macos --build BUILD [--out dist]
        Kalahari-<version>-macos-<arch>.dmg with Kalahari.app, signed ad hoc (no Apple
        Developer ID: on the first start macOS asks the writer to allow the program).

    python scripts/package.py check --program PATH
        Run the check of an installation of the program PATH (kalahari, kalahari.exe, an
        AppImage), with the settings of the check in a temporary folder.

    python scripts/package.py install-check [--dist dist]
        Install the packages of this system from --dist as a writer does and check the
        installed program. Windows: the installer silently for the writer alone and for all
        users, then its uninstaller (the machine must not have Kalahari installed). macOS:
        Kalahari.app copied from the .dmg. Linux: the .deb with apt and the AppImage on a clean
        Ubuntu 24.04 (packaging/linux/check-packages.sh; needs docker).

BUILD is the folder of a Release build. The packages go to --out (dist by default; the folder
is in .gitignore). Exit code 0 on success, 1 when a package cannot be made or its check fails.
"""

import argparse
import fnmatch
import hashlib
import os
import platform
import plistlib
import re
import shutil
import stat
import struct
import subprocess
import time
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path
from xml.parsers.expat import ExpatError
from typing import Callable, Dict, Iterable, List, Optional, Sequence, Set, Tuple

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import release  # noqa: E402  (scripts/release.py: the version of the program)

# linuxdeploy and its Qt plugin: the releases and the SHA-256 of their files
LINUXDEPLOY = (
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/"
    "linuxdeploy-x86_64.AppImage",
    "c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d",
)
LINUXDEPLOY_PLUGIN_QT = (
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/"
    "linuxdeploy-plugin-qt-x86_64.AppImage",
    "15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724",
)

# The Qt plugins of a folder the Linux packages take only in part: the database of the program
# (SQLite; the drivers of database servers need their client libraries) and the platform
# themes that give the system's own windows for files (GTK and the desktop portal)
QT_PLUGINS_TAKEN = {
    "sqldrivers": {"libqsqlite.so"},
    "platformthemes": {"libqgtk3.so", "libqxdgdesktopportal.so"},
}
# The libraries of GTK the Linux packages leave to the system: the GTK theme of Qt uses the
# GTK of the desktop, with its themes and modules (a copy of GTK in the package would load the
# modules of another GTK on a newer system). GLib goes with them: the GIO modules of the system
# need the GLib of the system
SYSTEM_LIBRARIES = [
    "libgtk-3.so*", "libgdk-3.so*", "libgdk_pixbuf-2.0.so*", "libatk-1.0.so*",
    "libatk-bridge-2.0.so*", "libatspi.so*", "libpango-1.0.so*", "libpangocairo-1.0.so*",
    "libpangoft2-1.0.so*", "libcairo.so*", "libcairo-gobject.so*", "libepoxy.so*",
    "libglib-2.0.so*", "libgobject-2.0.so*", "libgio-2.0.so*", "libgmodule-2.0.so*",
    "libgthread-2.0.so*", "libwayland-cursor.so*", "libwayland-egl.so*",
]
DEB_DESCRIPTION = """Writer's IDE: write, plan and publish books
 Kalahari is a writing environment for authors of books: the editor of the
 manuscript with its chapters, notes and versions, the material of the book
 (characters, places, plot, timeline), statistics and goals of writing, and
 the export and publication of the book.
"""
HOMEPAGE = "https://github.com/bartoszwarzocha/kalahari"
# How long the check of an installation may take (the first start of Python included)
CHECK_TIMEOUT_S = 180
# How long an installer or an uninstaller of Windows may take
INSTALL_TIMEOUT_S = 600

# The DLLs Windows gives every program: the API sets name parts of Windows, not files
WINDOWS_API_SETS = ("api-ms-win-", "ext-ms-win-")
# The runtime of Visual C++ and the debug runtime: a build machine has them in System32 (with
# Visual Studio), a Windows of a writer may not, so the package itself has to carry them
VISUAL_CPP_RUNTIME = re.compile(
    r"^(vcruntime|msvcp|msvcr|concrt|vccorlib|vcomp|vcamp|mfc|ucrtbased)", re.IGNORECASE)
WINDOWS_BINARIES = (".exe", ".dll", ".pyd")
# The Windows installer: its keys in the registry (packaging/windows/kalahari.nsi)
REGISTRY_KEY = r"Software\Kalahari"
UNINSTALL_KEY = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\Kalahari"
BOOK_TYPE = "Kalahari.Book"
BOOKS_KEY = r"Software\Classes\.klh"
BOOK_TYPE_KEY = rf"Software\Classes\{BOOK_TYPE}"
BOOK_COMMAND_KEY = rf"Software\Classes\{BOOK_TYPE}\shell\open\command"


class PackageError(Exception):
    """A package that cannot be made, with the reason."""


# =============================================================================
# Common
# =============================================================================

def log(text: str) -> None:
    print(f"package.py: {text}", flush=True)


def run(command: Sequence[object], cwd: Optional[Path] = None,
        env: Optional[Dict[str, str]] = None, capture: bool = False) -> str:
    """Run @p command; raise PackageError when it fails. Returns its output when @p capture."""
    args = [str(part) for part in command]
    log("$ " + " ".join(args))
    result = subprocess.run(args, cwd=cwd, env=env, text=True, check=False,
                            stdout=subprocess.PIPE if capture else None,
                            stderr=subprocess.STDOUT if capture else None)
    if result.returncode != 0:
        if capture and result.stdout:
            print(result.stdout)
        raise PackageError(f"{args[0]} ended with code {result.returncode}")
    return result.stdout if capture else ""


def program_version(root: Path = REPO_ROOT) -> Tuple[str, str]:
    """The version of the program (e.g. "0.4.0-alpha") and its number ("0.4.0")."""
    number, suffix = release.cmake_version(root)
    return release.version_string(number, suffix), number


def debian_version(version: str) -> str:
    """The version for dpkg: a release stage sorts before the release (0.4.0~alpha < 0.4.0)."""
    return version.replace("-", "~", 1)


def cmake_cache(build: Path) -> Dict[str, str]:
    """The variables of the CMake cache of @p build."""
    cache = build / "CMakeCache.txt"
    if not cache.is_file():
        raise PackageError(f"{build} is not a CMake build folder (no CMakeCache.txt)")
    values: Dict[str, str] = {}
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        match = re.match(r"^([A-Za-z0-9_.+-]+):[A-Z_]+=(.*)$", line)
        if match:
            values[match.group(1)] = match.group(2)
    return values


def fresh_folder(path: Path) -> Path:
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True)
    return path


def cmake_install(build: Path, prefix: Path, component: Optional[str] = None) -> None:
    command: List[object] = ["cmake", "--install", build, "--prefix", prefix, "--config", "Release"]
    if component:
        command += ["--component", component]
    run(command)


def files_of(folder: Path) -> List[Path]:
    """The files of @p folder and its subfolders, sorted, relative to it."""
    return sorted(path.relative_to(folder) for path in folder.rglob("*") if path.is_file())


def isolated_environment(home: Path) -> Dict[str, str]:
    """The environment of a check: its settings in @p home, not in the writer's."""
    env = dict(os.environ)
    # The libraries and the Python of the package, not those of the build machine (the
    # setup-python of the CI sets LD_LIBRARY_PATH to its own Python, ahead of the RUNPATH)
    for name in ("LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH", "PYTHONHOME", "PYTHONPATH"):
        env.pop(name, None)
    env["HOME"] = str(home)
    env["XDG_CONFIG_HOME"] = str(home / ".config")
    env["APPDATA"] = str(home / "AppData" / "Roaming")
    env["LOCALAPPDATA"] = str(home / "AppData" / "Local")
    env["PYTHONDONTWRITEBYTECODE"] = "1"  # no compiled modules in the folder of the program
    if sys.platform.startswith("linux"):
        if not env.get("DISPLAY") and not env.get("WAYLAND_DISPLAY"):
            env["QT_QPA_PLATFORM"] = "offscreen"  # no screen: the packages carry this plugin
        env["APPIMAGE_EXTRACT_AND_RUN"] = "1"     # no FUSE needed
    return env


def check_program(program: Path, python_within: Optional[Path] = None,
                  as_installed: bool = False) -> None:
    """Run the check of an installation of @p program; raise PackageError when it fails, when
    its Python is that of the build machine, or when it is not in @p python_within (the folder
    of the installation, when the check knows it). @p as_installed: Python writes its compiled
    modules where the program tells it to (the cache of the user), as for a writer."""
    if not program.is_file():
        raise PackageError(f"No program {program}")
    with tempfile.TemporaryDirectory(prefix="kalahari-check-") as home:
        env = isolated_environment(Path(home))
        if as_installed:
            env.pop("PYTHONDONTWRITEBYTECODE")
        log(f"$ {program} --self-test")
        try:
            result = subprocess.run([str(program), "--self-test"], env=env, cwd=home,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    timeout=CHECK_TIMEOUT_S, check=False)
        except subprocess.TimeoutExpired as error:
            raise PackageError(f"The check of {program} did not end in {CHECK_TIMEOUT_S} s") from error
        output = result.stdout.decode("utf-8", errors="replace")
        print(output.rstrip())
        if result.returncode != 0:
            raise PackageError(f"The check of {program} failed (code {result.returncode})")
        home = python_home_of_check(output)
        if home is not None and is_inside(home, Path(sys.base_prefix)):
            raise PackageError(f"{program} uses the Python of the build machine ({home}), "
                               "not its own")
        if python_within is not None and (home is None or not is_inside(home, python_within)):
            raise PackageError(f"{program} uses a Python outside {python_within} ({home})")


def python_home_of_check(output: str) -> Optional[Path]:
    """The folder of the Python the check of an installation names: its line "Python:" has
    the version of Python (sys.version, which ends with the compiler in brackets) and the
    folder in parentheses."""
    match = re.search(r"^OK\s+Python: .*\] \((.+)\)\s*$", output, re.MULTILINE)
    return Path(match.group(1)) if match else None


def is_inside(path: Path, folder: Path) -> bool:
    """@p path is @p folder or in it (the case of the letters aside on Windows)."""
    path_text = os.path.normcase(os.path.realpath(path))
    folder_text = os.path.normcase(os.path.realpath(folder))
    return path_text == folder_text or path_text.startswith(folder_text.rstrip(os.sep) + os.sep)


def write_sha256(package: Path) -> None:
    digest = hashlib.sha256(package.read_bytes()).hexdigest()
    package.with_name(package.name + ".sha256").write_text(f"{digest}  {package.name}\n",
                                                           encoding="utf-8")
    log(f"{package} ({package.stat().st_size // (1024 * 1024)} MB, SHA-256 {digest})")


# =============================================================================
# Linux: AppImage and .deb
# =============================================================================

def download_tool(url_and_hash: Tuple[str, str], tools: Path) -> Path:
    """The tool of @p url_and_hash in @p tools, downloaded when it is not there."""
    url, expected = url_and_hash
    tool = tools / url.rsplit("/", 1)[1]
    if not tool.is_file() or hashlib.sha256(tool.read_bytes()).hexdigest() != expected:
        tools.mkdir(parents=True, exist_ok=True)
        log(f"Downloading {url}")
        with urllib.request.urlopen(url, timeout=120) as response:  # noqa: S310 (fixed https URL)
            data = response.read()
        digest = hashlib.sha256(data).hexdigest()
        if digest != expected:
            raise PackageError(f"{url}: SHA-256 {digest}, expected {expected}")
        tool.write_bytes(data)
    tool.chmod(tool.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    return tool


def find_qmake(cache: Dict[str, str]) -> Path:
    """The qmake of the Qt the build uses (linuxdeploy-plugin-qt reads the paths of Qt from it)."""
    candidates: List[Path] = []
    if os.environ.get("QMAKE"):
        candidates.append(Path(os.environ["QMAKE"]))
    qt_dir = cache.get("Qt6_DIR", "")
    if qt_dir:
        prefix = Path(qt_dir).resolve().parents[2]  # <prefix>/lib/cmake/Qt6
        candidates += [prefix / "bin" / "qmake", prefix / "bin" / "qmake6"]
    candidates += [Path("/usr/lib/qt6/bin/qmake")]
    for name in ("qmake6", "qmake"):
        found = shutil.which(name)
        if found:
            candidates.append(Path(found))
    for candidate in candidates:
        if candidate.is_file():
            version = run([candidate, "-query", "QT_VERSION"], capture=True).strip()
            if version.startswith("6."):
                return candidate
    raise PackageError("No qmake of Qt 6: give its path in QMAKE")


def qt_plugins_view(qmake: Path, work: Path) -> Path:
    """A qmake that names a folder of the Qt plugins with only QT_PLUGINS_TAKEN of its folders.

    linuxdeploy-plugin-qt deploys every plugin of a folder it takes (all the SQL drivers, also
    those of database servers whose libraries the build machine may not have)."""
    query = run([qmake, "-query"], capture=True)
    plugins = Path(re.search(r"^QT_INSTALL_PLUGINS:(.*)$", query, re.MULTILINE).group(1).strip())
    view = fresh_folder(work / "qt-plugins")
    for folder in sorted(plugins.iterdir()):
        if not folder.is_dir():
            continue
        taken = QT_PLUGINS_TAKEN.get(folder.name)
        if taken is None:
            (view / folder.name).symlink_to(folder, target_is_directory=True)
            continue
        (view / folder.name).mkdir()
        for name in sorted(taken):
            if (folder / name).is_file():
                (view / folder.name / name).symlink_to(folder / name)
    wrapper = work / "qmake"
    wrapper.write_text(
        "#!/bin/sh\n"
        "# qmake of the Qt of the build, with the folder of the plugins of package.py\n"
        f"if [ \"$1\" = \"-query\" ] && [ $# -eq 1 ]; then\n"
        f"  '{qmake}' -query | sed 's|^QT_INSTALL_PLUGINS:.*|QT_INSTALL_PLUGINS:{view}|'\n"
        "else\n"
        f"  exec '{qmake}' \"$@\"\n"
        "fi\n", encoding="utf-8")
    wrapper.chmod(0o755)
    return wrapper


def library_folders(binaries: Iterable[Path]) -> List[str]:
    """The folders of the libraries @p binaries load that are not in the folders of the system
    (the Python and PDFium of the build): linuxdeploy finds them there."""
    folders: List[str] = []
    for binary in binaries:
        output = run(["ldd", binary], capture=True)
        for match in re.finditer(r"=>\s+(/\S+)", output):
            folder = str(Path(match.group(1)).parent)
            if not folder.startswith(("/lib", "/usr/lib")) and folder not in folders:
                folders.append(folder)
    return folders


def make_appdir(build: Path, work: Path, tools: Path, version: str) -> Tuple[Path, Path]:
    """The AppDir of the program and the AppImage made from it."""
    cache = cmake_cache(build)
    appdir = fresh_folder(work / "AppDir")
    cmake_install(build, appdir / "usr")
    cmake_install(build, appdir / "usr", component="python")

    linuxdeploy = download_tool(LINUXDEPLOY, tools)
    plugin_qt = download_tool(LINUXDEPLOY_PLUGIN_QT, tools)
    env = dict(os.environ)
    env["QMAKE"] = str(qt_plugins_view(find_qmake(cache), work))
    env["DEPLOY_PLATFORM_THEMES"] = "1"
    # A screen of its own for the check of the package where there is no screen
    env["EXTRA_PLATFORM_PLUGINS"] = "libqoffscreen.so"
    env["APPIMAGE_EXTRACT_AND_RUN"] = "1"
    env["LINUXDEPLOY_OUTPUT_VERSION"] = version
    env["PATH"] = str(plugin_qt.parent) + os.pathsep + env.get("PATH", "")
    built = [build / "bin" / "kalahari"] + sorted((build / "lib").glob("*.so*"))
    extra = library_folders(path for path in built if path.is_file())
    env["LD_LIBRARY_PATH"] = os.pathsep.join(
        extra + [p for p in env.get("LD_LIBRARY_PATH", "").split(os.pathsep) if p])

    env["LINUXDEPLOY_EXCLUDED_LIBRARIES"] = ";".join(SYSTEM_LIBRARIES)
    excluded: List[object] = []
    for pattern in SYSTEM_LIBRARIES:
        excluded += ["--exclude-library", pattern]

    output = fresh_folder(work / "appimage")
    # The program, its libraries and the Qt plugins with theirs. The Qt plugin of this release
    # copies the libraries of the plugins without the exclusions (it has no --exclude-library
    # and does not read LINUXDEPLOY_EXCLUDED_LIBRARIES): the libraries of the system it copied
    # are removed before the AppImage is made of the folder
    run([linuxdeploy, "--appdir", appdir, "--executable", appdir / "usr" / "bin" / "kalahari",
         "--desktop-file", REPO_ROOT / "packaging" / "linux" / "kalahari.desktop",
         "--icon-file", REPO_ROOT / "resources" / "images" / "app_icon" / "kalahari_256.png",
         "--icon-filename", "kalahari", "--plugin", "qt"] + excluded, cwd=output, env=env)
    remove_system_libraries(appdir / "usr" / "lib")
    for text in link_module_libraries(appdir / "usr" / "lib"):
        log(text)
    run([linuxdeploy, "--appdir", appdir, "--output", "appimage"] + excluded, cwd=output, env=env)
    check_appdir_libraries(appdir)
    images = sorted(output.glob("*.AppImage"))
    if len(images) != 1:
        raise PackageError(f"linuxdeploy made {len(images)} AppImages in {output}")
    return appdir, images[0]


def elf_needed(binary: Path) -> List[str]:
    """The libraries an ELF file names (DT_NEEDED), as readelf shows them."""
    result = subprocess.run(["readelf", "-d", str(binary)], stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL, text=True, check=False)
    return re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", result.stdout)


def link_module_libraries(lib: Path,
                          needed: Callable[[Path], List[str]] = elf_needed) -> List[str]:
    """Links in the folder of the C modules of Python (lib-dynload) to the libraries of the
    package in @p lib they load, and to those these load. linuxdeploy gives each module the
    RUNPATH of a library in usr/lib ($ORIGIN), which is the folder of the module: without the
    links a module finds a library only when the program loaded it before (sqlite3 did not find
    libsqlite3.so.0 on a system without it). Returns what was linked."""
    linked = []
    for folder in sorted(lib.glob("python3*/lib-dynload")):
        wanted = [name for module in elf_files(folder) for name in needed(module)]
        found: Set[str] = set()
        while wanted:
            name = wanted.pop()
            if name not in found and (lib / name).is_file():
                found.add(name)
                wanted += needed(lib / name)
        for name in sorted(found):
            link = folder / name
            if not link.exists() and not link.is_symlink():
                link.symlink_to(Path("..") / ".." / name)
                linked.append(f"{link.relative_to(lib)} -> ../../{name}")
    return linked


def foreign_libraries(ldd_output: str, own: Set[str], appdir: Path,
                      direct: Set[str]) -> List[str]:
    """The libraries of an output of ldd that are not found, or that the file needs itself
    (@p direct: its DT_NEEDED) and finds outside @p appdir although the AppDir has them (@p own:
    the names in its usr/lib). A library of the system finds its own libraries in the system:
    those are not the package's to load."""
    problems = []
    for name, found in re.findall(r"^\s*(\S+) => (.+?)(?: \(0x[0-9a-f]+\))?$", ldd_output,
                                  re.MULTILINE):
        if found == "not found":
            problems.append(f"{name} not found")
        elif name in own and name in direct and not is_inside(Path(found), appdir):
            problems.append(f"{name} from {found}, not from the package")
    return problems


def check_appdir_libraries(appdir: Path) -> None:
    """Every program and library of the AppDir loads the libraries the AppDir has from it: one
    that loads the copy of the build machine instead would miss it on a system without it."""
    env = {name: value for name, value in os.environ.items() if name != "LD_LIBRARY_PATH"}
    own = {path.name for path in (appdir / "usr" / "lib").iterdir()}
    problems = []
    binaries = elf_files(appdir / "usr")
    for binary in binaries:
        result = subprocess.run(["ldd", str(binary)], stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL, text=True, env=env, check=False)
        direct = set(elf_needed(binary))
        problems += [f"{binary.relative_to(appdir)}: {text}"
                     for text in foreign_libraries(result.stdout, own, appdir, direct)]
    if problems:
        raise PackageError("Libraries the package does not load from itself:\n  " +
                           "\n  ".join(problems))
    log(f"{len(binaries)} programs and libraries load the libraries of the package from it")


def remove_system_libraries(lib: Path) -> None:
    """Remove the SYSTEM_LIBRARIES from the folder @p lib of an AppDir."""
    for path in sorted(lib.iterdir()):
        if any(fnmatch.fnmatch(path.name, pattern) for pattern in SYSTEM_LIBRARIES):
            log(f"{path.name}: left to the system")
            path.unlink()


def shlibs_name(file_name: str) -> Optional[Tuple[str, str]]:
    """The name and version of a library in a shlibs file (as dpkg-shlibdeps reads its
    soname): libfoo.so.1.2 -> (libfoo, 1.2), libfoo-1.0.so -> (libfoo, 1.0); None for a
    library without a version."""
    match = re.match(r"^(.+)\.so\.(\d.*)$", file_name) or re.match(r"^(.+)-(\d[^-]*)\.so$", file_name)
    return (match.group(1), match.group(2)) if match else None


def elf_files(folder: Path) -> List[Path]:
    files = []
    for path in sorted(folder.rglob("*")):
        if path.is_file() and not path.is_symlink():
            with path.open("rb") as handle:
                if handle.read(4) == b"\x7fELF":
                    files.append(path)
    return files


def make_deb(appdir: Path, work: Path, out: Path, version: str) -> Path:
    """The .deb package: the AppDir in /opt/kalahari, its menu entry, icons and type of files
    in /usr/share, the command kalahari in /usr/bin."""
    root = fresh_folder(work / "deb")
    program = root / "opt" / "kalahari"
    shutil.copytree(appdir / "usr", program, symlinks=True)
    shared = root / "usr" / "share"
    for part in ("applications", "icons", "mime", "metainfo"):
        if (program / "share" / part).exists():
            shutil.move(str(program / "share" / part), str(shared / part))
    desktop = shared / "applications" / "kalahari.desktop"
    desktop.write_text(desktop.read_text(encoding="utf-8").replace(
        "Exec=kalahari %f", "Exec=/opt/kalahari/bin/kalahari %f"), encoding="utf-8")
    (root / "usr" / "bin").mkdir(parents=True)
    (root / "usr" / "bin" / "kalahari").symlink_to("/opt/kalahari/bin/kalahari")
    doc = shared / "doc" / "kalahari"
    doc.mkdir(parents=True)
    shutil.copyfile(REPO_ROOT / "LICENSE", doc / "copyright")

    # The libraries of the system the package needs; its own ones are in /opt/kalahari. Each
    # of its own is in shlibs.local without a package: dpkg-shlibdeps would otherwise name the
    # package of the system that has the same library (and its symbols file may name a version
    # only its own source package fills in, such as ${binary:Version})
    libraries = [path for path in elf_files(program) if ".so" in path.name]
    private = sorted({str(path.parent) for path in libraries})
    control_dir = fresh_folder(work / "shlibdeps")
    (control_dir / "debian").mkdir()
    (control_dir / "debian" / "control").write_text(
        "Source: kalahari\n\nPackage: kalahari\nArchitecture: amd64\n", encoding="utf-8")
    own = sorted({" ".join(name) for name in map(shlibs_name, (p.name for p in libraries)) if name})
    (control_dir / "debian" / "shlibs.local").write_text(
        "".join(line + "\n" for line in own), encoding="utf-8")
    command: List[object] = ["dpkg-shlibdeps", "-O", "--ignore-missing-info"]
    command += [f"-l{folder}" for folder in private]
    command += elf_files(program)
    substitutions = run(command, cwd=control_dir, capture=True)
    match = re.search(r"^shlibs:Depends=(.*)$", substitutions, re.MULTILINE)
    depends = match.group(1).strip() if match else ""
    if "${" in depends:
        raise PackageError(f"Depends of the package with a variable of another package: {depends}")
    size_kb = sum(path.stat().st_size for path in root.rglob("*")
                  if path.is_file() and not path.is_symlink()) // 1024

    (root / "DEBIAN").mkdir()
    control = [
        "Package: kalahari",
        f"Version: {debian_version(version)}",
        "Architecture: amd64",
        "Maintainer: Kalahari Team <https://github.com/bartoszwarzocha/kalahari/issues>",
        f"Installed-Size: {size_kb}",
        f"Depends: {depends}" if depends else "",
        "Section: editors",
        "Priority: optional",
        f"Homepage: {HOMEPAGE}",
        "Description: " + DEB_DESCRIPTION,
    ]
    (root / "DEBIAN" / "control").write_text(
        "\n".join(line for line in control if line).rstrip("\n") + "\n", encoding="utf-8")
    for path in root.rglob("*"):
        if path.is_dir():
            path.chmod(0o755)
    package = out / f"kalahari_{debian_version(version)}_amd64.deb"
    run(["dpkg-deb", "--root-owner-group", "-Zxz", "--build", root, package])
    run(["dpkg-deb", "--info", package])
    return package


def package_linux(build: Path, out: Path, tools: Optional[Path]) -> List[Path]:
    version, _ = program_version()
    work = build / "_packaging"
    appdir, image = make_appdir(build, work, tools or work / "tools", version)
    out.mkdir(parents=True, exist_ok=True)
    appimage = out / f"Kalahari-{version}-x86_64.AppImage"
    shutil.move(str(image), str(appimage))
    check_program(appimage)
    deb = make_deb(appdir, work, out, version)
    installed = work / "deb" / "opt" / "kalahari"
    check_program(installed / "bin" / "kalahari", python_within=installed)
    for package in (appimage, deb):
        write_sha256(package)
    return [appimage, deb]


# =============================================================================
# Windows: the folder of the program, its ZIP and its installer
# =============================================================================

def nsis_text(text: str) -> str:
    """@p text in a string of NSIS: "$" is "$$", a quote "$\\"\"."""
    return text.replace("$", "$$").replace('"', '$\\"')


def nsis_lists(stage: Path, work: Path) -> Tuple[Path, Path]:
    """The lists of the files of @p stage for kalahari.nsi: what the installer writes and
    what the uninstaller removes."""
    files = sorted(files_of(stage), key=lambda path: (path.parent.parts, path.name))
    install = []
    current = None
    for relative in files:
        folder = relative.parent
        if folder != current:
            target = "$INSTDIR" + "".join("\\" + nsis_text(part) for part in folder.parts)
            install.append(f'SetOutPath "{target}"')
            current = folder
        source = str(stage / relative)
        if '"' in source or "${" in source:
            raise PackageError(f"NSIS cannot take the name of the file {source}")
        install.append(f'File "{source}"')  # a path of the compiling machine, read as it is
    uninstall = ['Delete "$INSTDIR\\' + nsis_text("\\".join(relative.parts)) + '"'
                 for relative in files]
    folders = sorted({parent for relative in files for parent in relative.parents
                      if parent != Path(".")}, key=lambda path: len(path.parts), reverse=True)
    uninstall += ['RMDir "$INSTDIR\\' + nsis_text("\\".join(folder.parts)) + '"'
                  for folder in folders]
    install_list = work / "install-files.nsh"
    uninstall_list = work / "uninstall-files.nsh"
    install_list.write_text("\n".join(install) + "\n", encoding="utf-8")
    uninstall_list.write_text("\n".join(uninstall) + "\n", encoding="utf-8")
    return install_list, uninstall_list


def find_makensis(given: Optional[str]) -> Optional[Path]:
    candidates = [given] if given else []
    candidates += [shutil.which("makensis") or "",
                   r"C:\Program Files (x86)\NSIS\makensis.exe", r"C:\Program Files\NSIS\makensis.exe"]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return Path(candidate)
    return None


def make_installer(stage: Path, out: Path, makensis: Path, work: Path) -> Path:
    if not (stage / "kalahari.exe").is_file() or not (stage / "LICENSE.txt").is_file():
        raise PackageError(f"{stage} is not the folder of the program (kalahari.exe, LICENSE.txt)")
    version, number = program_version()
    install_list, uninstall_list = nsis_lists(stage, fresh_folder(work / "nsis"))
    installer = out / f"Kalahari-{version}-windows-x64-setup.exe"
    out.mkdir(parents=True, exist_ok=True)
    option = "/" if sys.platform == "win32" else "-"
    run([makensis, f"{option}INPUTCHARSET", "UTF8", f"{option}V2",
         f"{option}DVERSION={version}", f"{option}DVERSION_NUMBER={number}.0",
         f"{option}DSTAGE={stage.resolve()}", f"{option}DOUTFILE={installer.resolve()}",
         f"{option}DFILES={install_list.resolve()}", f"{option}DUNFILES={uninstall_list.resolve()}",
         REPO_ROOT / "packaging" / "windows" / "kalahari.nsi"])
    return installer


def make_zip(stage: Path, package: Path) -> None:
    with zipfile.ZipFile(package, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for relative in files_of(stage):
            archive.write(stage / relative, str(Path("Kalahari") / relative))


class PeFile:
    """The headers of a Windows program or library (PE32 or PE32+) and the names in its tables
    of imports. Raises ValueError or struct.error for a file of another kind."""

    IMPORTS = 1           # the numbers of the data directories
    DELAYED_IMPORTS = 13

    def __init__(self, data: bytes) -> None:
        self.data = data
        if data[:2] != b"MZ":
            raise ValueError("no MZ header")
        header = self.number("<I", 0x3C)
        if data[header:header + 4] != b"PE\0\0":
            raise ValueError("no PE header")
        optional = header + 24
        magic = self.number("<H", optional)
        if magic == 0x20B:    # PE32+ (64-bit)
            self.image_base, self.directories = self.number("<Q", optional + 24), optional + 112
        elif magic == 0x10B:  # PE32
            self.image_base, self.directories = self.number("<I", optional + 28), optional + 96
        else:
            raise ValueError(f"optional header {magic:#x}")
        self.directory_count = self.number("<I", self.directories - 4)
        first = optional + self.number("<H", header + 20)
        self.sections = []
        for index in range(self.number("<H", header + 6)):
            size, address, raw_size, raw = struct.unpack_from("<IIII", data, first + 40 * index + 8)
            self.sections.append((address, max(size, raw_size), raw))

    def number(self, fmt: str, offset: int) -> int:
        return struct.unpack_from(fmt, self.data, offset)[0]

    def offset_of(self, address: int) -> int:
        for start, size, raw in self.sections:
            if start <= address < start + size:
                return address - start + raw
        raise ValueError(f"address {address:#x} outside the sections")

    def name_at(self, address: int) -> str:
        start = self.offset_of(address)
        return self.data[start:self.data.index(b"\0", start)].decode("ascii")

    def table(self, index: int) -> int:
        """The offset in the file of the data directory @p index, -1 when there is none."""
        if index >= self.directory_count:
            return -1
        address = self.number("<I", self.directories + 8 * index)
        return self.offset_of(address) if address else -1

    def imports(self) -> List[str]:
        names = []
        entry = self.table(self.IMPORTS)
        while entry >= 0:
            lookup, _, _, name, thunks = struct.unpack_from("<IIIII", self.data, entry)
            if not (lookup or name or thunks):
                break
            names.append(self.name_at(name))
            entry += 20
        return names

    def delayed_imports(self) -> List[str]:
        names = []
        entry = self.table(self.DELAYED_IMPORTS)
        while entry >= 0:
            attributes, name = struct.unpack_from("<II", self.data, entry)
            if not name:
                break
            # Old linkers wrote addresses, not offsets from the start of the image
            names.append(self.name_at(name if attributes & 1 else name - self.image_base))
            entry += 32
        return names


def pe_imports(binary: Path) -> Tuple[List[str], List[str]]:
    """The DLLs a Windows program or library loads: at its start (its table of imports) and
    when it first calls one of them (its table of delayed imports)."""
    try:
        pe = PeFile(binary.read_bytes())
        return pe.imports(), pe.delayed_imports()
    except (ValueError, struct.error) as error:
        raise PackageError(f"{binary} is not a Windows program or library: {error}") from error


def windows_system_libraries() -> Set[str]:
    """The names (in small letters) of the DLLs in System32 of this Windows."""
    system = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32"
    return {path.name.lower() for path in system.glob("*.dll")}


def windows_library_place(dll: str, own: Set[str], system: Set[str], at_start: bool) -> str:
    """Where Windows finds the DLL @p dll (in small letters) a library of the package loads at
    its start or later (@p at_start), with the DLLs of its folders @p own and of System32 @p
    system: "package", "api-set", "windows", "missing" (the program does not start) or
    "delayed" (only a part of it fails, when it first calls the DLL)."""
    if dll in own:
        return "package"
    if dll.startswith(WINDOWS_API_SETS):
        return "api-set"
    if VISUAL_CPP_RUNTIME.match(dll):
        return "missing"  # on the build machine only
    if dll in system:
        return "windows"
    return "missing" if at_start else "delayed"


def missing_windows_libraries(stage: Path, system: Set[str],
                              from_windows: Optional[Set[str]] = None
                              ) -> Tuple[List[str], List[str]]:
    """The DLLs the programs and libraries of @p stage need that neither the folder of the
    program nor Windows gives (@p system: the DLLs of System32, in small letters). Windows
    looks for a DLL in the folder of the library, in the folder of the program and in
    System32; the runtime of Visual C++ has to be in the package (VISUAL_CPP_RUNTIME).
    Returns the missing DLLs and the delayed ones Windows does not have (a part of the program
    fails when it first calls them); @p from_windows gets the DLLs Windows gives."""
    names: Dict[Path, Set[str]] = {}

    def names_of(folder: Path) -> Set[str]:
        if folder not in names:
            names[folder] = {path.name.lower() for path in folder.iterdir() if path.is_file()}
        return names[folder]

    missing: List[str] = []
    delayed_missing: List[str] = []
    for relative in files_of(stage):
        if relative.suffix.lower() not in WINDOWS_BINARIES:
            continue
        binary = stage / relative
        own = names_of(stage) | names_of(binary.parent)
        imports, delayed = pe_imports(binary)
        loaded = [(name, True) for name in imports] + [(name, False) for name in delayed]
        for name, at_start in loaded:
            place = windows_library_place(name.lower(), own, system, at_start)
            if place == "missing":
                missing.append(f"{relative.as_posix()}: {name}")
            elif place == "delayed":
                delayed_missing.append(f"{relative.as_posix()}: {name}")
            elif place == "windows" and from_windows is not None:
                from_windows.add(name.lower())
    return missing, delayed_missing


def check_windows_libraries(stage: Path) -> None:
    """Every DLL the folder of the program needs is in it or in Windows; raise PackageError."""
    if sys.platform != "win32":
        log("The DLLs of the package are checked on Windows only")
        return
    from_windows: Set[str] = set()
    missing, delayed = missing_windows_libraries(stage, windows_system_libraries(), from_windows)
    log("DLLs of Windows the package uses: " + ", ".join(sorted(from_windows)))
    for text in delayed:
        log(f"Delayed DLL not in Windows (the part that calls it fails): {text}")
    if missing:
        raise PackageError("DLLs missing from the package:\n  " + "\n  ".join(missing))
    log("Every DLL of the package is in it or in Windows")


def package_windows(build: Path, out: Path, makensis_path: Optional[str]) -> List[Path]:
    version, _ = program_version()
    work = build / "_packaging"
    stage = fresh_folder(work / "Kalahari")
    cmake_install(build, stage)
    check_windows_libraries(stage)
    check_program(stage / "kalahari.exe", python_within=stage)
    # The check leaves the log of the program in its folder: not a file of the package
    (stage / "kalahari.log").unlink(missing_ok=True)
    out.mkdir(parents=True, exist_ok=True)
    packages = [out / f"Kalahari-{version}-windows-x64.zip"]
    make_zip(stage, packages[0])
    makensis = find_makensis(makensis_path)
    if makensis:
        packages.append(make_installer(stage, out, makensis, work))
    else:
        log("No makensis (NSIS 3): only the ZIP. Give its path with --makensis.")
    for package in packages:
        write_sha256(package)
    return packages


# =============================================================================
# macOS: Kalahari.app in a .dmg
# =============================================================================

SYSTEM_PREFIXES = ("/System/", "/usr/lib/", "@rpath/", "@executable_path/", "@loader_path/")


def macho_files(bundle: Path) -> List[Path]:
    files = []
    for path in sorted(bundle.rglob("*")):
        if path.is_file() and not path.is_symlink():
            with path.open("rb") as handle:
                magic = handle.read(4)
            if magic in (b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe", b"\xca\xfe\xba\xbe"):
                files.append(path)
    return files


def dependencies(binary: Path) -> List[str]:
    lines = run(["otool", "-L", binary], capture=True).splitlines()[1:]
    return [line.strip().split(" (")[0] for line in lines if line.strip()]


def fix_bundle_libraries(bundle: Path) -> None:
    """The libraries of the bundle name each other by @rpath; any other library outside the
    system ends the package (it would be missing on the writer's Mac)."""
    frameworks = bundle / "Contents" / "Frameworks"
    own = {path.name for path in frameworks.glob("*.dylib")} if frameworks.is_dir() else set()
    for name in own:
        run(["install_name_tool", "-id", f"@rpath/{name}", frameworks / name])
    missing = []
    for binary in macho_files(bundle):
        for dependency in dependencies(binary):
            if dependency.startswith(SYSTEM_PREFIXES) or dependency == f"@rpath/{binary.name}":
                continue
            name = Path(dependency).name
            if name in own:
                run(["install_name_tool", "-change", dependency, f"@rpath/{name}", binary])
            else:
                missing.append(f"{binary.relative_to(bundle)}: {dependency}")
    if missing:
        raise PackageError("Libraries outside the bundle and the system:\n  " + "\n  ".join(missing))
    for binary in macho_files(bundle):
        print(f"{binary.relative_to(bundle)}:")
        for dependency in dependencies(binary):
            print(f"    {dependency}")


def check_info_plist(bundle: Path) -> Dict[str, object]:
    """The description of the bundle, read as Finder and codesign read it: a value that breaks
    its XML (an "&" of the copyright) leaves the system no Info.plist to read."""
    info = bundle / "Contents" / "Info.plist"
    try:
        values = plistlib.loads(info.read_bytes())
    except (ExpatError, ValueError) as error:
        raise PackageError(f"{info} is not a property list: {error}") from error
    executable = values.get("CFBundleExecutable")
    if not executable or not (bundle / "Contents" / "MacOS" / str(executable)).is_file():
        raise PackageError(f"{info}: no program {executable!r} in Contents/MacOS")
    return values


def package_macos(build: Path, out: Path) -> List[Path]:
    version, _ = program_version()
    work = build / "_packaging"
    stage = fresh_folder(work / "macos")
    cmake_install(build, stage)
    bundle = stage / "Kalahari.app"
    (stage / "kalahari.app").rename(bundle)
    check_info_plist(bundle)
    fix_bundle_libraries(bundle)
    for binary in macho_files(bundle):
        if binary.parent.name != "MacOS":
            run(["codesign", "--force", "--sign", "-", "--timestamp=none", binary])
    run(["codesign", "--force", "--deep", "--sign", "-", "--timestamp=none", bundle])
    run(["codesign", "--verify", "--deep", "--strict", "--verbose=2", bundle])
    check_bundle_libraries(bundle)
    check_program(bundle / "Contents" / "MacOS" / "kalahari", python_within=bundle)

    image_folder = fresh_folder(work / "dmg")
    shutil.copytree(bundle, image_folder / "Kalahari.app", symlinks=True)
    (image_folder / "Applications").symlink_to("/Applications")
    out.mkdir(parents=True, exist_ok=True)
    package = out / f"Kalahari-{version}-macos-{platform.machine()}.dmg"
    package.unlink(missing_ok=True)
    run(["hdiutil", "create", "-volname", f"Kalahari {version}", "-srcfolder", image_folder,
         "-fs", "HFS+", "-format", "UDZO", "-ov", package])
    write_sha256(package)
    return [package]


# =============================================================================
# The packages installed as a writer installs them (install-check)
# =============================================================================

def files_of_zip(archive: Path) -> Set[str]:
    """The files of the ZIP of the program, without its folder Kalahari/ (make_zip)."""
    with zipfile.ZipFile(archive) as opened:
        return {name.split("/", 1)[1] for name in opened.namelist()
                if not name.endswith("/") and "/" in name}


def differences(expected: Set[str], found: Set[str]) -> List[str]:
    """The files @p found lacks and has besides @p expected, in small letters (Windows)."""
    expected_names = {name.lower(): name for name in expected}
    found_names = {name.lower(): name for name in found}
    lacking = sorted(expected_names.keys() - found_names.keys())
    extra = sorted(found_names.keys() - expected_names.keys())
    return ([f"missing {expected_names[name]}" for name in lacking] +
            [f"extra {found_names[name]}" for name in extra])


def registry_values(root: int, key: str) -> Optional[Dict[str, object]]:
    """The values of a key of the registry (its 64-bit view), None when there is no key."""
    import winreg  # noqa: PLC0415 (Windows only)
    try:
        with winreg.OpenKey(root, key, 0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as handle:
            values: Dict[str, object] = {}
            index = 0
            while True:
                try:
                    name, value, _ = winreg.EnumValue(handle, index)
                except OSError:
                    return values
                values[name] = value
                index += 1
    except FileNotFoundError:
        return None


def same_path(first: object, second: Path) -> bool:
    def normal(path: object) -> str:
        return os.path.normcase(os.path.normpath(str(path)))
    return normal(first) == normal(second)


def wait_until(condition: Callable[[], bool], timeout_s: int) -> bool:
    deadline = time.monotonic() + timeout_s
    while not condition():
        if time.monotonic() > deadline:
            return False
        time.sleep(1)
    return True


class WindowsInstallation:
    """An installation of the Windows installer: for the writer alone ("CurrentUser") or for
    all users ("AllUsers"), with the places kalahari.nsi writes to."""

    def __init__(self, mode: str) -> None:
        import winreg  # noqa: PLC0415 (Windows only)
        menu = Path("Microsoft") / "Windows" / "Start Menu" / "Programs" / "Kalahari.lnk"
        self.mode = mode
        if mode == "CurrentUser":
            self.root = winreg.HKEY_CURRENT_USER
            self.folder = Path(os.environ["LOCALAPPDATA"]) / "Programs" / "Kalahari"
            self.shortcut = Path(os.environ["APPDATA"]) / menu
        else:
            self.root = winreg.HKEY_LOCAL_MACHINE
            program_files = os.environ.get("ProgramW6432") or os.environ["ProgramFiles"]
            self.folder = Path(program_files) / "Kalahari"
            self.shortcut = Path(os.environ["ProgramData"]) / menu

    def values(self, key: str) -> Optional[Dict[str, object]]:
        return registry_values(self.root, key)

    def problems(self, expected: Set[str], version: str) -> List[str]:
        """What the installation lacks: the files @p expected, its keys, its shortcut, the
        program of the .klh books."""
        if not self.folder.is_dir():
            return [f"no folder {self.folder}"]
        found = {relative.as_posix() for relative in files_of(self.folder)}
        problems = [f"files: {text}" for text in differences(expected, found)]
        program = self.values(REGISTRY_KEY) or {}
        if not same_path(program.get("InstallLocation", ""), self.folder):
            problems.append(f"InstallLocation {program.get('InstallLocation')!r}")
        if program.get("InstallMode") != self.mode:
            problems.append(f"InstallMode {program.get('InstallMode')!r}")
        uninstall = self.values(UNINSTALL_KEY) or {}
        if uninstall.get("DisplayVersion") != version:
            problems.append(f"DisplayVersion {uninstall.get('DisplayVersion')!r}")
        if str(self.uninstaller()).lower() not in self.quiet_uninstall().lower():
            problems.append(f"QuietUninstallString {self.quiet_uninstall()!r}")
        if not self.shortcut.is_file():
            problems.append(f"no shortcut {self.shortcut}")
        books = (self.values(BOOKS_KEY) or {}).get("")
        command = (self.values(BOOK_COMMAND_KEY) or {}).get("")
        if books != BOOK_TYPE or str(command).lower() != f'"{self.program()}" "%1"'.lower():
            problems.append(f".klh books: {books!r}, {command!r}")
        return problems

    def left_after_uninstall(self) -> List[str]:
        left = []
        if self.folder.exists():
            left = [str(path) for path in sorted(self.folder.rglob("*"))][:20] or [str(self.folder)]
        if self.shortcut.exists():
            left.append(str(self.shortcut))
        left += [key for key in (REGISTRY_KEY, UNINSTALL_KEY, BOOKS_KEY, BOOK_TYPE_KEY)
                 if self.values(key) is not None]
        return left

    def program(self) -> Path:
        return self.folder / "kalahari.exe"

    def uninstaller(self) -> Path:
        return self.folder / "uninstall.exe"

    def quiet_uninstall(self) -> str:
        return str((self.values(UNINSTALL_KEY) or {}).get("QuietUninstallString", ""))


def check_windows_installer(dist: Path) -> None:
    """The installer of @p dist, silently, for the writer alone and for all users: the files
    of the ZIP of the program, the keys of the registry, the Start menu and the .klh books; the
    installed program passes its check with its own Python; its uninstaller removes it all."""
    installers = sorted(dist.glob("Kalahari-*-windows-x64-setup.exe"))
    if len(installers) != 1:
        raise PackageError(f"{len(installers)} installers of Windows in {dist}")
    installer = installers[0]
    expected = files_of_zip(installer.with_name(installer.name.replace("-setup.exe", ".zip")))
    expected.add("uninstall.exe")
    version, _ = program_version()
    for mode in ("CurrentUser", "AllUsers"):
        installation = WindowsInstallation(mode)
        log(f"Installation {mode}: {installation.folder}")
        if installation.folder.exists() or installation.values(REGISTRY_KEY) is not None:
            raise PackageError(f"Kalahari is installed already ({installation.folder})")
        log(f"$ {installer} /S /{mode}")
        result = subprocess.run([str(installer), "/S", f"/{mode}"], timeout=INSTALL_TIMEOUT_S,
                                check=False)
        if result.returncode != 0:
            raise PackageError(f"The installer ended with code {result.returncode}")
        problems = installation.problems(expected, version)
        if problems:
            raise PackageError(f"The installation {mode}:\n  " + "\n  ".join(problems))
        check_program(installation.program(), python_within=installation.folder, as_installed=True)

        # As Settings > Apps uninstalls it: the uninstaller copies itself to the temporary
        # folder, starts the copy and ends at once; the copy removes the files and the keys
        quiet = installation.quiet_uninstall()
        log(f"$ {quiet}")
        subprocess.run(quiet, timeout=INSTALL_TIMEOUT_S, check=False)
        wait_until(lambda: not installation.left_after_uninstall(), INSTALL_TIMEOUT_S)
        left = installation.left_after_uninstall()
        if left:
            raise PackageError(f"The uninstaller of {mode} left:\n  " + "\n  ".join(left))
        log(f"Installation {mode}: installed, checked and removed")


def bundle_library_problems(bundle: Path,
                            dependencies_of: Callable[[Path], List[str]]) -> List[str]:
    """The libraries the programs and libraries of a bundle load from neither the bundle nor
    macOS (an absolute path outside /System and /usr/lib, or a name the bundle has no file of)."""
    names = {path.name for path in bundle.rglob("*")}
    problems = []
    for binary in macho_files(bundle):
        for dependency in dependencies_of(binary):
            if dependency.startswith(("/System/", "/usr/lib/")):
                continue
            if dependency.startswith("@") and Path(dependency).name in names:
                continue
            problems.append(f"{binary.relative_to(bundle).as_posix()}: {dependency}")
    return problems


def check_bundle_libraries(bundle: Path) -> None:
    problems = bundle_library_problems(bundle, dependencies)
    if problems:
        raise PackageError("Libraries outside the bundle and macOS:\n  " + "\n  ".join(problems))
    log(f"Every library of {bundle.name} is in it or in macOS")


def check_macos_image(dist: Path) -> None:
    """The .dmg of @p dist as a writer uses it: Kalahari.app copied from the opened image into
    a folder of applications; its signature, Info.plist and libraries, and its check with the
    Python of the bundle."""
    images = sorted(dist.glob("Kalahari-*-macos-*.dmg"))
    if len(images) != 1:
        raise PackageError(f"{len(images)} images of macOS in {dist}")
    with tempfile.TemporaryDirectory(prefix="kalahari-dmg-") as work:
        volume = Path(work) / "volume"
        applications = Path(work) / "Applications"
        volume.mkdir()
        applications.mkdir()
        run(["hdiutil", "attach", "-nobrowse", "-readonly", "-noautoopen",
             "-mountpoint", volume, images[0]])
        try:
            shown = sorted(path.name for path in volume.iterdir() if not path.name.startswith("."))
            link = volume / "Applications"
            if shown != ["Applications", "Kalahari.app"] or not link.is_symlink() or \
                    os.readlink(link) != "/Applications":
                raise PackageError(f"The image shows {shown}, not Kalahari.app and Applications")
            run(["ditto", volume / "Kalahari.app", applications / "Kalahari.app"])
        finally:
            if subprocess.run(["hdiutil", "detach", str(volume)], check=False).returncode != 0:
                time.sleep(5)
                run(["hdiutil", "detach", "-force", volume])
        bundle = applications / "Kalahari.app"
        check_info_plist(bundle)
        run(["codesign", "--verify", "--deep", "--strict", "--verbose=2", bundle])
        check_bundle_libraries(bundle)
        program = bundle / "Contents" / "MacOS" / "kalahari"
        log("Processors of the program: " + run(["lipo", "-archs", program], capture=True).strip())
        # Gatekeeper: a program signed ad hoc (no Developer ID) is not accepted at once; the
        # writer allows it on its first start (System Settings > Privacy & Security)
        gatekeeper = subprocess.run(["spctl", "--assess", "--type", "execute", "--verbose",
                                     str(bundle)], stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, check=False)
        log(f"Gatekeeper (for information): {gatekeeper.stdout.strip()}")
        check_program(program, python_within=bundle, as_installed=True)
    log("The image of macOS: copied, checked")


def check_linux_packages(dist: Path) -> None:
    """The .deb and the AppImage of @p dist on a clean Ubuntu 24.04 (packaging/linux/
    check-packages.sh, in a container of docker)."""
    run(["bash", REPO_ROOT / "packaging" / "linux" / "check-packages.sh", dist])


def install_check(dist: Path) -> None:
    if sys.platform == "win32":
        check_windows_installer(dist)
    elif sys.platform == "darwin":
        check_macos_image(dist)
    else:
        check_linux_packages(dist)


# =============================================================================
# Command line
# =============================================================================

def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="The packages of Kalahari")
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("linux", "windows", "macos"):
        command = commands.add_parser(name)
        command.add_argument("--build", type=Path, required=True, help="a Release build")
        command.add_argument("--out", type=Path, default=REPO_ROOT / "dist")
        if name == "linux":
            command.add_argument("--tools", type=Path, help="linuxdeploy and its Qt plugin")
        if name == "windows":
            command.add_argument("--makensis", help="makensis of NSIS 3")
    installer = commands.add_parser("windows-installer")
    installer.add_argument("--stage", type=Path, required=True, help="the folder of the program")
    installer.add_argument("--out", type=Path, default=REPO_ROOT / "dist")
    installer.add_argument("--makensis", help="makensis of NSIS 3")
    check = commands.add_parser("check")
    check.add_argument("--program", type=Path, required=True)
    installed = commands.add_parser("install-check")
    installed.add_argument("--dist", type=Path, default=REPO_ROOT / "dist",
                           help="the folder of the packages")
    args = parser.parse_args(argv)

    try:
        if args.command == "linux":
            packages = package_linux(args.build.resolve(), args.out.resolve(), args.tools)
        elif args.command == "windows":
            packages = package_windows(args.build.resolve(), args.out.resolve(), args.makensis)
        elif args.command == "windows-installer":
            makensis = find_makensis(args.makensis)
            if not makensis:
                raise PackageError("No makensis (NSIS 3): give its path with --makensis")
            work = Path(tempfile.mkdtemp(prefix="kalahari-nsis-"))
            try:
                packages = [make_installer(args.stage.resolve(), args.out.resolve(), makensis, work)]
                write_sha256(packages[0])
            finally:
                shutil.rmtree(work, ignore_errors=True)
        elif args.command == "macos":
            packages = package_macos(args.build.resolve(), args.out.resolve())
        elif args.command == "install-check":
            install_check(args.dist.resolve())
            return 0
        else:
            check_program(args.program.resolve())
            return 0
    except (PackageError, OSError, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        print(f"package.py: {error}", file=sys.stderr)
        return 1
    for package in packages:
        log(f"Made {package}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
