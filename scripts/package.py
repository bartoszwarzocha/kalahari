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

BUILD is the folder of a Release build. The packages go to --out (dist by default; the folder
is in .gitignore). Exit code 0 on success, 1 when a package cannot be made or its check fails.
"""

import argparse
import hashlib
import os
import platform
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

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
# GTK of the desktop, with its themes and modules (a copy of GTK in the package would not load
# them)
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


def check_program(program: Path) -> None:
    """Run the check of an installation of @p program; raise PackageError when it fails."""
    if not program.is_file():
        raise PackageError(f"No program {program}")
    with tempfile.TemporaryDirectory(prefix="kalahari-check-") as home:
        env = isolated_environment(Path(home))
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

    output = fresh_folder(work / "appimage")
    command: List[object] = [linuxdeploy, "--appdir", appdir,
                             "--executable", appdir / "usr" / "bin" / "kalahari",
                             "--desktop-file", REPO_ROOT / "packaging" / "linux" / "kalahari.desktop",
                             "--icon-file", REPO_ROOT / "resources" / "images" / "app_icon" /
                             "kalahari_256.png", "--icon-filename", "kalahari",
                             "--plugin", "qt", "--output", "appimage"]
    for pattern in SYSTEM_LIBRARIES:
        command += ["--exclude-library", pattern]
    run(command, cwd=output, env=env)
    images = sorted(output.glob("*.AppImage"))
    if len(images) != 1:
        raise PackageError(f"linuxdeploy made {len(images)} AppImages in {output}")
    return appdir, images[0]


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

    # The libraries of the system the package needs; its own ones are in /opt/kalahari
    private = sorted({str(path.parent) for path in elf_files(program) if ".so" in path.name})
    control_dir = fresh_folder(work / "shlibdeps")
    (control_dir / "debian").mkdir()
    (control_dir / "debian" / "control").write_text(
        "Source: kalahari\n\nPackage: kalahari\nArchitecture: amd64\n", encoding="utf-8")
    command: List[object] = ["dpkg-shlibdeps", "-O", "--ignore-missing-info"]
    command += [f"-l{folder}" for folder in private]
    command += elf_files(program)
    substitutions = run(command, cwd=control_dir, capture=True)
    match = re.search(r"^shlibs:Depends=(.*)$", substitutions, re.MULTILINE)
    depends = match.group(1).strip() if match else ""
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
    check_program(work / "deb" / "opt" / "kalahari" / "bin" / "kalahari")
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


def package_windows(build: Path, out: Path, makensis_path: Optional[str]) -> List[Path]:
    version, _ = program_version()
    work = build / "_packaging"
    stage = fresh_folder(work / "Kalahari")
    cmake_install(build, stage)
    check_program(stage / "kalahari.exe")
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


def package_macos(build: Path, out: Path) -> List[Path]:
    version, _ = program_version()
    work = build / "_packaging"
    stage = fresh_folder(work / "macos")
    cmake_install(build, stage)
    bundle = stage / "Kalahari.app"
    (stage / "kalahari.app").rename(bundle)
    fix_bundle_libraries(bundle)
    for binary in macho_files(bundle):
        if binary.parent.name != "MacOS":
            run(["codesign", "--force", "--sign", "-", "--timestamp=none", binary])
    run(["codesign", "--force", "--deep", "--sign", "-", "--timestamp=none", bundle])
    run(["codesign", "--verify", "--deep", "--strict", "--verbose=2", bundle])
    check_program(bundle / "Contents" / "MacOS" / "kalahari")

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
        else:
            check_program(args.program.resolve())
            return 0
    except (PackageError, OSError, subprocess.SubprocessError) as error:
        print(f"package.py: {error}", file=sys.stderr)
        return 1
    for package in packages:
        log(f"Made {package}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
