#!/usr/bin/env python3
"""
Tests of scripts/package.py: the version of the packages, the lists of files of the installer
of Windows, the folder of the Qt plugins of the Linux packages, the check of a program, the
DLLs of the Windows package and the libraries of Kalahari.app, the checks of the installed
packages, and the files of the packages agreeing with each other. Plain unittest, run by CTest
(scripts.package). The packages themselves are made by the CI (BUILDING.md, "Packages").
"""

import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "scripts"))

import package  # noqa: E402  (the path is set just above)
import release  # noqa: E402


def icon_sizes() -> list:
    """PNG_SIZES of scripts/make_app_icons.py (read from the file: the script needs Pillow)."""
    script = (REPO_ROOT / "scripts" / "make_app_icons.py").read_text(encoding="utf-8")
    return [int(size) for size in re.search(r"^PNG_SIZES = \[([\d, ]+)\]", script,
                                            re.MULTILINE).group(1).split(",")]


def write(path: Path, text: str = "x") -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def pe_file(imports=(), delayed=(), pe32_plus=True, delay_addresses=False) -> bytes:
    """A Windows library with the tables of @p imports and @p delayed imports, in one section
    (at the address 0x1000, at 0x400 in the file). @p delay_addresses: the delayed imports name
    addresses, as old linkers wrote them."""
    image_base = 0x140000000 if pe32_plus else 0x400000
    address, raw = 0x1000, 0x400
    delayed_at = (len(imports) + 1) * 20
    section = bytearray(delayed_at + (len(delayed) + 1) * 32)

    def name(text: str) -> int:
        at = address + len(section)
        section.extend(text.encode("ascii") + b"\0")
        return at

    for index, dll in enumerate(imports):
        struct.pack_into("<IIIII", section, 20 * index, 1, 0, 0, name(dll), 1)
    for index, dll in enumerate(delayed):
        at = name(dll)
        struct.pack_into("<II", section, delayed_at + 32 * index, 0 if delay_addresses else 1,
                         at + image_base if delay_addresses else at)
    header, optional_size = 0x80, 240 if pe32_plus else 224
    data = bytearray(raw)
    data[0:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, header)
    data[header:header + 4] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, header + 4, 0x8664, 1, 0, 0, 0, optional_size, 0x2022)
    optional = header + 24
    struct.pack_into("<H", data, optional, 0x20B if pe32_plus else 0x10B)
    if pe32_plus:
        struct.pack_into("<Q", data, optional + 24, image_base)
        directories = optional + 112
    else:
        struct.pack_into("<I", data, optional + 28, image_base)
        directories = optional + 96
    struct.pack_into("<I", data, directories - 4, 16)
    if imports:
        struct.pack_into("<II", data, directories + 8, address, delayed_at)
    if delayed:
        struct.pack_into("<II", data, directories + 8 * 13, address + delayed_at, 32 * len(delayed))
    struct.pack_into("<8sIIII", data, optional + optional_size, b".idata", len(section), address,
                     len(section), raw)
    return bytes(data + section)


def write_pe(path: Path, imports=(), delayed=()) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(pe_file(imports, delayed))
    return path


class VersionTest(unittest.TestCase):
    def test_version_of_the_program(self):
        number, suffix = release.cmake_version(REPO_ROOT)
        self.assertEqual(package.program_version(), (release.version_string(number, suffix), number))

    def test_release_stage_sorts_before_the_release_for_dpkg(self):
        self.assertEqual(package.debian_version("0.4.0-alpha"), "0.4.0~alpha")
        self.assertEqual(package.debian_version("1.0.0-rc.1"), "1.0.0~rc.1")
        self.assertEqual(package.debian_version("1.0.0"), "1.0.0")


class CMakeCacheTest(unittest.TestCase):
    def test_variables_of_the_cache(self):
        with tempfile.TemporaryDirectory() as folder:
            write(Path(folder) / "CMakeCache.txt",
                  "# comment\nQt6_DIR:PATH=/opt/Qt/6.9.1/gcc_64/lib/cmake/Qt6\n"
                  "CMAKE_BUILD_TYPE:STRING=Release\n//help\nEMPTY:STRING=\n")
            cache = package.cmake_cache(Path(folder))
        self.assertEqual(cache["Qt6_DIR"], "/opt/Qt/6.9.1/gcc_64/lib/cmake/Qt6")
        self.assertEqual(cache["CMAKE_BUILD_TYPE"], "Release")
        self.assertEqual(cache["EMPTY"], "")

    def test_a_folder_without_a_build(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(package.PackageError):
                package.cmake_cache(Path(folder))


class NsisListsTest(unittest.TestCase):
    def test_files_written_and_removed(self):
        with tempfile.TemporaryDirectory() as folder:
            stage = Path(folder) / "stage"
            write(stage / "kalahari.exe")
            write(stage / "LICENSE.txt")
            write(stage / "platforms" / "qwindows.dll")
            write(stage / "resources" / "icons" / "twotone" / "file.new.svg")
            write(stage / "Lib" / "encodings" / "$name.py")
            work = Path(folder) / "work"
            work.mkdir()
            install, uninstall = package.nsis_lists(stage, work)
            install_lines = install.read_text(encoding="utf-8").splitlines()
            uninstall_lines = uninstall.read_text(encoding="utf-8").splitlines()

        # Each folder is entered once, before its files; the source paths are read as they are
        self.assertEqual(install_lines[0], 'SetOutPath "$INSTDIR"')
        self.assertEqual(sum(line == 'SetOutPath "$INSTDIR"' for line in install_lines), 1)
        self.assertIn('SetOutPath "$INSTDIR\\resources\\icons\\twotone"', install_lines)
        self.assertIn(f'File "{stage / "Lib" / "encodings" / "$name.py"}"', install_lines)
        self.assertEqual(sum(line.startswith("File ") for line in install_lines), 5)

        # The uninstaller removes those files ($ written $$ at run time) and then their
        # folders, the deepest first, and nothing else
        self.assertIn('Delete "$INSTDIR\\Lib\\encodings\\$$name.py"', uninstall_lines)
        self.assertIn('Delete "$INSTDIR\\kalahari.exe"', uninstall_lines)
        folders = [line for line in uninstall_lines if line.startswith("RMDir")]
        self.assertEqual(folders[0], 'RMDir "$INSTDIR\\resources\\icons\\twotone"')
        self.assertLess(folders.index('RMDir "$INSTDIR\\resources\\icons"'),
                        folders.index('RMDir "$INSTDIR\\resources"'))
        self.assertEqual(len(folders), 6)
        self.assertFalse(any("/r" in line for line in uninstall_lines))

    @unittest.skipIf(sys.platform == "win32", "Windows has no quotes in file names")
    def test_a_name_nsis_cannot_take(self):
        with tempfile.TemporaryDirectory() as folder:
            stage = Path(folder) / "stage"
            write(stage / 'a"b.txt')
            with self.assertRaises(package.PackageError):
                package.nsis_lists(stage, Path(folder))


@unittest.skipIf(sys.platform == "win32", "The Linux packages are made on Linux")
class QtPluginsViewTest(unittest.TestCase):
    def test_only_the_plugins_the_packages_take(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            plugins = root / "plugins"
            for name in ("platforms/libqxcb.so", "imageformats/libqjpeg.so",
                         "sqldrivers/libqsqlite.so", "sqldrivers/libqsqlpsql.so",
                         "sqldrivers/libqsqlodbc.so", "platformthemes/libqgtk3.so",
                         "platformthemes/libqxdgdesktopportal.so", "platformthemes/libqother.so"):
                write(plugins / name)
            qmake = write(root / "qmake",
                          "#!/bin/sh\n"
                          f"echo 'QT_INSTALL_PREFIX:{root}'\n"
                          f"echo 'QT_INSTALL_PLUGINS:{plugins}'\n"
                          "echo 'QT_VERSION:6.9.1'\n")
            qmake.chmod(0o755)
            work = root / "work"
            work.mkdir()

            wrapper = package.qt_plugins_view(qmake, work)
            view = work / "qt-plugins"
            self.assertEqual(sorted(p.name for p in (view / "sqldrivers").iterdir()),
                             ["libqsqlite.so"])
            self.assertEqual(sorted(p.name for p in (view / "platformthemes").iterdir()),
                             ["libqgtk3.so", "libqxdgdesktopportal.so"])
            self.assertTrue((view / "platforms" / "libqxcb.so").is_file())
            self.assertTrue((view / "imageformats" / "libqjpeg.so").is_file())

            query = package.run([wrapper, "-query"], capture=True)
            self.assertIn(f"QT_INSTALL_PLUGINS:{view}\n", query)
            self.assertIn("QT_VERSION:6.9.1", query)
            self.assertNotIn(f"QT_INSTALL_PLUGINS:{plugins}\n", query)


@unittest.skipIf(sys.platform == "win32", "The test programs are shell scripts")
class CheckProgramTest(unittest.TestCase):
    def program(self, folder: Path, body: str) -> Path:
        program = write(folder / "kalahari", "#!/bin/sh\n" + body)
        program.chmod(0o755)
        return program

    def test_a_working_installation(self):
        with tempfile.TemporaryDirectory() as folder:
            # The check runs with settings of its own, and with the libraries of the program
            # (not those LD_LIBRARY_PATH of the build machine names)
            program = self.program(Path(folder), '[ "$1" = "--self-test" ] || exit 2\n'
                                                 '[ "$HOME" != "' + os.environ.get("HOME", "") +
                                                 '" ] || exit 3\n'
                                                 '[ -z "$LD_LIBRARY_PATH" ] || exit 4\n'
                                                 f'echo "OK      Python: 3.15.2 (main) [GCC 13.3.0] '
                                                 f'({folder}/usr)"\n')
            saved = os.environ.get("LD_LIBRARY_PATH")
            os.environ["LD_LIBRARY_PATH"] = "/opt/hostedtoolcache/Python/3.15.2/x64/lib"
            try:
                package.check_program(program)
            finally:
                if saved is None:
                    del os.environ["LD_LIBRARY_PATH"]
                else:
                    os.environ["LD_LIBRARY_PATH"] = saved

    def test_an_installation_with_the_python_of_the_build_machine(self):
        with tempfile.TemporaryDirectory() as folder:
            program = self.program(Path(folder), 'echo "OK      Python: 3.15.2 (main) [GCC 13.3.0] '
                                                 f'({sys.base_prefix})"\n')
            with self.assertRaises(package.PackageError):
                package.check_program(program)

    def test_the_folder_of_python_in_the_check(self):
        windows = ("OK      Python: 3.12.1 (tags/v3.12.1:2305ca5, Dec  7 2023) "
                   "[MSC v.1937 64 bit (AMD64)] (C:\\Program Files (x86)\\Kalahari)")
        self.assertEqual(package.python_home_of_check("Kalahari 0.4.0\n" + windows + "\n"),
                         Path("C:\\Program Files (x86)\\Kalahari"))
        self.assertIsNone(package.python_home_of_check("FAILED  Python: no Lib\n"))

    def test_an_incomplete_installation(self):
        with tempfile.TemporaryDirectory() as folder:
            program = self.program(Path(folder), 'echo "FAILED  Python: no Lib"\nexit 1\n')
            with self.assertRaises(package.PackageError):
                package.check_program(program)

    def test_no_program(self):
        with self.assertRaises(package.PackageError):
            package.check_program(Path("/no/such/kalahari"))

    def test_the_python_of_the_installation(self):
        with tempfile.TemporaryDirectory() as folder:
            installation = Path(folder) / "opt" / "kalahari"
            program = self.program(Path(folder), 'echo "OK      Python: 3.15.2 (main) [GCC 13.3.0] '
                                                 f'({installation})"\n')
            package.check_program(program, python_within=installation)
            with self.assertRaises(package.PackageError):
                package.check_program(program, python_within=Path(folder) / "usr")
            silent = self.program(Path(folder), 'echo "OK      Resources"\n')
            with self.assertRaises(package.PackageError):
                package.check_program(silent, python_within=installation)

    def test_the_compiled_modules_of_an_installed_program(self):
        # The program of a writer puts them where it tells Python to (the cache of the user);
        # only the check of a folder that becomes a package keeps Python from writing them
        with tempfile.TemporaryDirectory() as folder:
            program = self.program(Path(folder), '[ -z "$PYTHONDONTWRITEBYTECODE" ] || exit 5\n')
            package.check_program(program, as_installed=True)
            with self.assertRaises(package.PackageError):
                package.check_program(program)


class DebianPackageTest(unittest.TestCase):
    def test_the_names_of_the_libraries_in_shlibs(self):
        self.assertEqual(package.shlibs_name("libQt6Core.so.6"), ("libQt6Core", "6"))
        self.assertEqual(package.shlibs_name("libpython3.15.so.1.0"), ("libpython3.15", "1.0"))
        self.assertEqual(package.shlibs_name("libhunspell-1.7.so.0"), ("libhunspell-1.7", "0"))
        self.assertEqual(package.shlibs_name("libfoo-2.1.so"), ("libfoo", "2.1"))
        # Libraries without a version: the program's own and the modules of Python
        self.assertIsNone(package.shlibs_name("libkalahari_core.so"))
        self.assertIsNone(package.shlibs_name("_ssl.cpython-315-x86_64-linux-gnu.so"))

    @unittest.skipIf(sys.platform == "win32", "Links and paths of Linux")
    def test_the_libraries_of_the_modules_of_python(self):
        # A C module of Python finds the libraries of the package in its own folder (the
        # RUNPATH $ORIGIN linuxdeploy gives it), also those its libraries load
        with tempfile.TemporaryDirectory() as folder:
            lib = Path(folder) / "usr" / "lib"
            modules = lib / "python3.15" / "lib-dynload"
            elf = b"\x7fELF" + bytes(12)
            needed = {
                "_sqlite3.cpython-315-x86_64-linux-gnu.so": ["libsqlite3.so.0", "libc.so.6"],
                "_ssl.cpython-315-x86_64-linux-gnu.so": ["libssl.so.3"],
                "_json.cpython-315-x86_64-linux-gnu.so": [],
                "libssl.so.3": ["libcrypto.so.3", "libc.so.6"],
                "libsqlite3.so.0": ["libm.so.6"],
                "libcrypto.so.3": [],
            }
            for name in needed:
                path = (modules if name.startswith("_") else lib) / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(elf)
            linked = package.link_module_libraries(lib, lambda path: needed[path.name])
            self.assertEqual(sorted(path.name for path in modules.iterdir() if path.is_symlink()),
                             ["libcrypto.so.3", "libsqlite3.so.0", "libssl.so.3"])
            self.assertEqual(os.readlink(modules / "libssl.so.3"), "../../libssl.so.3")
            self.assertTrue((modules / "libsqlite3.so.0").resolve().samefile(lib / "libsqlite3.so.0"))
            self.assertEqual(len(linked), 3)
            # Again (the links are there already): nothing new
            self.assertEqual(package.link_module_libraries(lib, lambda path: needed[path.name]), [])

    @unittest.skipIf(sys.platform == "win32", "Links and paths of Linux")
    def test_the_libraries_the_package_does_not_load_from_itself(self):
        appdir = Path("/build/AppDir")
        ldd = ("\tlinux-vdso.so.1 (0x00007ffc6a5f2000)\n"
               "\tlibsqlite3.so.0 => /lib/x86_64-linux-gnu/libsqlite3.so.0 (0x00007f1c2a000000)\n"
               "\tlibssl.so.3 => /build/AppDir/usr/lib/libssl.so.3 (0x00007f1c29e00000)\n"
               "\tlibgtk-3.so.0 => /lib/x86_64-linux-gnu/libgtk-3.so.0 (0x00007f1c29a00000)\n"
               "\tlibXau.so.6 => /lib/x86_64-linux-gnu/libXau.so.6 (0x00007f1c29800000)\n"
               "\tlibgone.so.1 => not found\n"
               "\t/lib64/ld-linux-x86-64.so.2 (0x00007f1c2a400000)\n")
        # libXau.so.6: the package has it, but only libgtk-3 of the system needs it
        own = {"libsqlite3.so.0", "libssl.so.3", "libXau.so.6"}
        direct = {"libsqlite3.so.0", "libssl.so.3", "libgtk-3.so.0"}
        self.assertEqual(package.foreign_libraries(ldd, own, appdir, direct), [
            "libsqlite3.so.0 from /lib/x86_64-linux-gnu/libsqlite3.so.0, not from the package",
            "libgone.so.1 not found"])

    def test_the_libraries_left_to_the_system(self):
        with tempfile.TemporaryDirectory() as folder:
            lib = Path(folder)
            for name in ("libgtk-3.so.0", "libglib-2.0.so.0", "libQt6Core.so.6", "libpng16.so.16"):
                write(lib / name)
            package.remove_system_libraries(lib)
            self.assertEqual(sorted(p.name for p in lib.iterdir()),
                             ["libQt6Core.so.6", "libpng16.so.16"])


class WindowsLibrariesTest(unittest.TestCase):
    """The DLLs of the Windows package: each one in it or in Windows."""

    def test_the_tables_of_imports(self):
        with tempfile.TemporaryDirectory() as folder:
            library = write_pe(Path(folder) / "Qt6Core.dll", ["KERNEL32.dll", "VCRUNTIME140.dll"],
                               ["dwmapi.dll"])
            self.assertEqual(package.pe_imports(library),
                             (["KERNEL32.dll", "VCRUNTIME140.dll"], ["dwmapi.dll"]))
            old = Path(folder) / "old.dll"
            old.write_bytes(pe_file(["user32.dll"], ["winmm.dll"], pe32_plus=False,
                                    delay_addresses=True))
            self.assertEqual(package.pe_imports(old), (["user32.dll"], ["winmm.dll"]))
            self.assertEqual(package.pe_imports(write_pe(Path(folder) / "none.dll")), ([], []))

    def test_a_file_that_is_no_library(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(package.PackageError):
                package.pe_imports(write(Path(folder) / "readme.dll", "MZ but no more"))

    def test_the_dlls_missing_from_the_package(self):
        with tempfile.TemporaryDirectory() as folder:
            stage = Path(folder)
            write_pe(stage / "kalahari.exe", ["KERNEL32.dll", "Qt6Core.dll", "VCRUNTIME140.dll",
                                              "api-ms-win-crt-runtime-l1-1-0.dll", "zlib1.dll"])
            write_pe(stage / "Qt6Core.dll", ["kernel32.dll", "MSVCP140.dll"])
            write_pe(stage / "python312.dll", ["kernel32.dll"])
            # A Qt plugin finds the DLLs of the folder of the program; a delayed DLL Windows
            # lacks fails only the part that calls it, the runtime of Visual C++ is never in it
            write_pe(stage / "platforms" / "qwindows.dll", ["Qt6Core.dll", "dwmapi.dll"],
                     ["wtsapi32.dll", "optional.dll", "vcruntime140_1.dll"])
            # A module of Python finds the DLLs of its own folder and of the program
            write_pe(stage / "DLLs" / "_ssl.pyd", ["libssl-3-x64.dll", "python312.dll"])
            write_pe(stage / "DLLs" / "libssl-3-x64.dll", ["kernel32.dll"])
            write(stage / "resources" / "notes.txt")
            # The build machine has the runtime of Visual C++ in System32
            system = {"kernel32.dll", "dwmapi.dll", "wtsapi32.dll", "vcruntime140.dll",
                      "msvcp140.dll", "vcruntime140_1.dll"}
            from_windows = set()
            missing, delayed = package.missing_windows_libraries(stage, system, from_windows)
            self.assertEqual(sorted(missing), [
                "Qt6Core.dll: MSVCP140.dll", "kalahari.exe: VCRUNTIME140.dll",
                "kalahari.exe: zlib1.dll", "platforms/qwindows.dll: vcruntime140_1.dll"])
            self.assertEqual(delayed, ["platforms/qwindows.dll: optional.dll"])
            self.assertEqual(from_windows, {"kernel32.dll", "dwmapi.dll", "wtsapi32.dll"})

            for name in ("vcruntime140.dll", "msvcp140.dll", "vcruntime140_1.dll", "zlib1.dll"):
                write_pe(stage / name, ["kernel32.dll"])
            self.assertEqual(package.missing_windows_libraries(stage, system),
                             ([], ["platforms/qwindows.dll: optional.dll"]))

    def test_the_libraries_of_windows(self):
        with tempfile.TemporaryDirectory() as folder:
            windows = Path(folder)
            for name in ("KERNEL32.dll", "winspool.drv"):
                write(windows / "System32" / name)
            write(windows / "System32" / "drivers" / "etc" / "hosts")
            self.assertEqual(package.windows_system_libraries(windows),
                             {"kernel32.dll", "winspool.drv"})

    def test_the_runtime_of_visual_cpp_in_the_package(self):
        packaging = (REPO_ROOT / "cmake" / "Packaging.cmake").read_text(encoding="utf-8")
        self.assertIn("include(InstallRequiredSystemLibraries)", packaging)
        self.assertIn("install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION .)",
                      packaging)


class InstalledPackagesTest(unittest.TestCase):
    """The parts of the checks of the installed packages that run on any system."""

    def test_the_files_of_the_zip_of_the_program(self):
        with tempfile.TemporaryDirectory() as folder:
            stage = Path(folder) / "Kalahari"
            write(stage / "kalahari.exe")
            write(stage / "Lib" / "json" / "__init__.py")
            archive = Path(folder) / "Kalahari-windows-x64.zip"
            package.make_zip(stage, archive)
            self.assertEqual(package.files_of_zip(archive),
                             {"kalahari.exe", "Lib/json/__init__.py"})

    def test_the_differences_of_the_files(self):
        # Windows does not tell small letters from capitals
        self.assertEqual(package.differences({"Qt6Core.dll", "Lib/os.py", "LICENSE.txt"},
                                             {"qt6core.dll", "Lib/os.py", "kalahari.log"}),
                         ["missing LICENSE.txt", "extra kalahari.log"])
        self.assertEqual(package.differences({"a"}, {"a"}), [])

    def test_the_libraries_of_the_bundle(self):
        with tempfile.TemporaryDirectory() as folder:
            bundle = Path(folder) / "Kalahari.app"
            macho = b"\xcf\xfa\xed\xfe" + bytes(28)
            program = bundle / "Contents" / "MacOS" / "kalahari"
            library = bundle / "Contents" / "Frameworks" / "libkalahari_core.dylib"
            for path in (program, library):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(macho)
            write(bundle / "Contents" / "Info.plist")
            dependencies = {
                program: ["@rpath/libkalahari_core.dylib", "/usr/lib/libSystem.B.dylib",
                          "/System/Library/Frameworks/Cocoa.framework/Versions/A/Cocoa",
                          "/opt/homebrew/lib/libintl.8.dylib", "@rpath/libmissing.dylib"],
                library: ["@rpath/libkalahari_core.dylib", "/usr/lib/libc++.1.dylib"],
            }
            self.assertEqual(package.bundle_library_problems(bundle, dependencies.__getitem__), [
                "Contents/MacOS/kalahari: /opt/homebrew/lib/libintl.8.dylib",
                "Contents/MacOS/kalahari: @rpath/libmissing.dylib"])

    @unittest.skipIf(shutil.which("bash") is None or sys.platform == "win32", "No bash")
    def test_the_script_of_the_linux_packages(self):
        script = REPO_ROOT / "packaging" / "linux" / "check-packages.sh"
        self.assertEqual(subprocess.run(["bash", "-n", str(script)], check=False).returncode, 0)
        self.assertTrue(os.access(script, os.X_OK))

    def test_the_ci_installs_the_packages(self):
        for system in ("linux", "windows", "macos"):
            workflow = (REPO_ROOT / ".github" / "workflows" / f"ci-{system}.yml").read_text(
                encoding="utf-8")
            self.assertIn("scripts/package.py install-check --dist dist", workflow, system)


class WindowsFileTypesTest(unittest.TestCase):
    """The registration of the files of Kalahari the check of the Windows installer reads."""

    PROGRAM = Path(r"C:\Users\writer\AppData\Local\Programs\Kalahari\kalahari.exe")

    def registry(self) -> dict:
        keys = {
            r"Software\Kalahari\Capabilities": {"ApplicationName": "Kalahari"},
            r"Software\Kalahari\Capabilities\FileAssociations": {},
            r"Software\RegisteredApplications": {"Other": "x",
                                                  "Kalahari": r"Software\Kalahari\Capabilities"},
        }
        for extension, prog_id, _ in package.FILE_TYPES:
            keys[rf"Software\Classes\{extension}"] = {"": prog_id}
            keys[rf"Software\Classes\{extension}\OpenWithProgids"] = {prog_id: ""}
            keys[rf"Software\Classes\{prog_id}\shell\open\command"] = {
                "": f'"{self.PROGRAM}" "%1"'}
            keys[rf"Software\Classes\{prog_id}"] = {"": "Kalahari"}
            keys[r"Software\Kalahari\Capabilities\FileAssociations"][extension] = prog_id
        return keys

    def test_the_files_registered_as_the_installer_registers_them(self):
        keys = self.registry()
        self.assertEqual(package.file_type_problems(keys.get, self.PROGRAM,
                                                    lambda extension: str(self.PROGRAM)), [])

    def test_what_the_registration_lacks(self):
        keys = self.registry()
        del keys[r"Software\Classes\.kchapter\OpenWithProgids"]
        keys[r"Software\Kalahari\Capabilities\FileAssociations"][".klh"] = "Other.Type"
        del keys[r"Software\RegisteredApplications"]["Kalahari"]
        word = r"C:\Word\winword.exe"
        other = {".klh": str(self.PROGRAM), ".kchapter": word}
        command = f'"{self.PROGRAM}" "%1"'
        self.assertEqual(package.file_type_problems(keys.get, self.PROGRAM, other.get), [
            "Capabilities of .klh: 'Other.Type'",
            f".kchapter: 'Kalahari.Document', OpenWithProgids [], {command!r}",
            f"Windows opens .kchapter with {word!r}",
            "RegisteredApplications: None"])

    def test_what_an_uninstaller_left(self):
        self.assertEqual(package.file_type_keys_left({}.get), [])
        keys = {r"Software\Classes\.kchapter": {"": "Kalahari.Document"},
                r"Software\RegisteredApplications": {"Kalahari": "x"}}
        self.assertEqual(package.file_type_keys_left(keys.get), [
            r"Software\Classes\.kchapter", r"Software\RegisteredApplications\Kalahari"])


class MacBundleTest(unittest.TestCase):
    """Info.plist of Kalahari.app as Finder and codesign read it."""

    # The types of the files of Kalahari, as packaging/macos/Info.plist.in declares them
    TYPES = "".join(
        "<key>{}</key><array>{}</array>".format(key, "".join(item.format(
            extension=extension.lstrip("."), type=content_type)
            for extension, _, content_type in package.FILE_TYPES))
        for key, item in (
            ("CFBundleDocumentTypes",
             "<dict><key>LSHandlerRank</key><string>Owner</string>"
             "<key>LSItemContentTypes</key><array><string>{type}</string></array></dict>"),
            ("UTExportedTypeDeclarations",
             "<dict><key>UTTypeIdentifier</key><string>{type}</string>"
             "<key>UTTypeTagSpecification</key><dict><key>public.filename-extension</key>"
             "<array><string>{extension}</string></array></dict></dict>")))
    PLIST = ('<?xml version="1.0" encoding="UTF-8"?>\n<plist version="1.0"><dict>'
             "<key>CFBundleExecutable</key><string>kalahari</string>{types}"
             "<key>NSHumanReadableCopyright</key><string>{copyright}</string></dict></plist>\n")

    def bundle(self, folder: str, copyright_text: str, types: bool = True) -> Path:
        bundle = Path(folder) / "Kalahari.app"
        write(bundle / "Contents" / "MacOS" / "kalahari")
        write(bundle / "Contents" / "Info.plist", self.PLIST.format(
            types=self.TYPES if types else "", copyright=copyright_text))
        return bundle

    def test_a_bundle_the_system_reads(self):
        with tempfile.TemporaryDirectory() as folder:
            values = package.check_info_plist(self.bundle(folder, "Writers &amp; Team"))
            self.assertEqual(values["NSHumanReadableCopyright"], "Writers & Team")

    def test_an_ampersand_that_breaks_the_xml(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(package.PackageError):
                package.check_info_plist(self.bundle(folder, "Writers & Team"))

    def test_a_program_the_bundle_does_not_have(self):
        with tempfile.TemporaryDirectory() as folder:
            bundle = self.bundle(folder, "Team")
            (bundle / "Contents" / "MacOS" / "kalahari").unlink()
            with self.assertRaises(package.PackageError):
                package.check_info_plist(bundle)

    def test_a_bundle_without_the_files_of_kalahari(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaisesRegex(package.PackageError, "kalahari.document"):
                package.check_info_plist(self.bundle(folder, "Team", types=False))

    def test_the_types_of_the_files_in_the_bundle(self):
        values = {
            "CFBundleDocumentTypes": [
                {"LSItemContentTypes": ["io.github.bartoszwarzocha.kalahari.book"],
                 "LSHandlerRank": "Owner"},
                {"LSItemContentTypes": ["io.github.bartoszwarzocha.kalahari.document"],
                 "LSHandlerRank": "Alternate"}],
            "UTExportedTypeDeclarations": [
                {"UTTypeIdentifier": "io.github.bartoszwarzocha.kalahari.book",
                 "UTTypeTagSpecification": {"public.filename-extension": ["klh"]}},
                {"UTTypeIdentifier": "io.github.bartoszwarzocha.kalahari.document",
                 "UTTypeTagSpecification": {"public.filename-extension": ["kchap"]}}],
        }
        self.assertEqual(package.bundle_file_type_problems(values), [
            "io.github.bartoszwarzocha.kalahari.document: extensions ['kchap']",
            "io.github.bartoszwarzocha.kalahari.document: the program opens it as 'Alternate'"])

    def test_the_copyright_escaped_for_the_xml(self):
        # cmake/Packaging.cmake writes an "&" of the copyright (the name of a company may have
        # one, as the publisher has) as "&amp;" into Info.plist
        packaging = (REPO_ROOT / "cmake" / "Packaging.cmake").read_text(encoding="utf-8")
        self.assertIsNotNone(re.search(r'set\(KALAHARI_COPYRIGHT "([^"]+)"\)', packaging))
        self.assertIn("&", re.search(r'set\(KALAHARI_PUBLISHER "([^"]+)"\)', packaging).group(1))
        self.assertIn('string(REPLACE "&" "&amp;" _kalahari_plist_copyright', packaging)
        self.assertIn('MACOSX_BUNDLE_COPYRIGHT "${_kalahari_plist_copyright}"', packaging)


class PackagingFilesTest(unittest.TestCase):
    """The files of the packages name the same things."""

    def test_the_licence_and_its_additional_terms(self):
        # Every package carries the GNU GPL (LICENSE) and its additional terms (NOTICE.md)
        packaging = (REPO_ROOT / "cmake" / "Packaging.cmake").read_text(encoding="utf-8")
        self.assertEqual(packaging.count('"${CMAKE_SOURCE_DIR}/LICENSE"'), 3)
        self.assertEqual(packaging.count('"${CMAKE_SOURCE_DIR}/NOTICE.md"'), 3)
        licence = (REPO_ROOT / "LICENSE").read_text(encoding="utf-8")
        self.assertIn("GNU GENERAL PUBLIC LICENSE", licence)
        self.assertIn("Version 3, 29 June 2007", licence)
        notice = (REPO_ROOT / "NOTICE.md").read_text(encoding="utf-8")
        self.assertIn("Kalahari – created by BW Digital Systems & Publishing, "
                      "https://bwdsp.com.pl/", notice)
        # The Debian package: the notice, then the licence, in its copyright file
        script = (REPO_ROOT / "scripts" / "package.py").read_text(encoding="utf-8")
        self.assertIn('(REPO_ROOT / "NOTICE.md").read_text(encoding="utf-8") + "\\n\\n" +', script)
        # The publisher of the installer of Windows and of the program file
        installer = (REPO_ROOT / "packaging" / "windows" / "kalahari.nsi").read_text(
            encoding="utf-8")
        self.assertIn('!define PUBLISHER "BW Digital Systems & Publishing"', installer)
        resource = (REPO_ROOT / "packaging" / "windows" / "kalahari.rc.in").read_text(
            encoding="utf-8")
        self.assertIn('VALUE "CompanyName", "@KALAHARI_PUBLISHER@"', resource)

    def test_the_sizes_of_the_icon(self):
        packaging = (REPO_ROOT / "cmake" / "Packaging.cmake").read_text(encoding="utf-8")
        sizes = re.search(r"set\(KALAHARI_ICON_SIZES ([\d ]+)\)", packaging).group(1).split()
        self.assertEqual([int(size) for size in sizes], icon_sizes())
        art = (REPO_ROOT / "src" / "core" / "art_provider.cpp").read_text(encoding="utf-8")
        program = re.search(r"APPLICATION_ICON_SIZES\[\] = \{([\d, ]+)\}", art).group(1)
        self.assertEqual([int(size) for size in program.split(",")], icon_sizes())
        for size in icon_sizes():
            self.assertTrue((REPO_ROOT / "resources" / "images" / "app_icon" /
                             f"kalahari_{size}.png").is_file(), size)
        for name in ("kalahari.ico", "kalahari.icns"):
            self.assertTrue((REPO_ROOT / "packaging" / "icons" / name).is_file(), name)

    def test_the_menu_entry_and_the_type_of_the_books(self):
        desktop = (REPO_ROOT / "packaging" / "linux" / "kalahari.desktop").read_text(encoding="utf-8")
        mime = (REPO_ROOT / "packaging" / "linux" / "kalahari-mime.xml").read_text(encoding="utf-8")
        self.assertIn("Icon=kalahari\n", desktop)
        self.assertIn("Exec=kalahari %f\n", desktop)  # make_deb() puts the path of /opt into it
        types = re.findall(r'<mime-type type="([^"]+)">.*?<glob pattern="\*(\.\w+)"/>', mime,
                           re.DOTALL)
        self.assertEqual([extension for _, extension in types],
                         [extension for extension, _, _ in package.FILE_TYPES])
        self.assertIn("MimeType={};\n".format(";".join(name for name, _ in types)), desktop)
        # The program names its windows after the menu entry (Wayland)
        main = (REPO_ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn('setDesktopFileName(QStringLiteral("kalahari"))', main)

    def test_the_installer_of_windows(self):
        nsi = (REPO_ROOT / "packaging" / "windows" / "kalahari.nsi").read_text(encoding="utf-8")
        for define in ("VERSION", "VERSION_NUMBER", "STAGE", "OUTFILE", "FILES", "UNFILES"):
            self.assertIn(define, re.search(r"!ifndef (.*)", nsi).group(1))
        self.assertIn('!include "${FILES}"', nsi)
        self.assertIn('!include "${UNFILES}"', nsi)
        self.assertIn('"$INSTDIR\\${APP_EXE}" "%1"', nsi)  # the files open in the program
        defines = dict(re.findall(r'^!define (\w+) "([^"]*)"', nsi, re.MULTILINE))
        self.assertEqual(defines["CAPABILITIES_KEY"].replace("${REGISTRY_KEY}", package.REGISTRY_KEY),
                         package.CAPABILITIES_KEY)
        self.assertEqual(defines["REGISTERED_APPLICATIONS_KEY"], package.REGISTERED_APPLICATIONS_KEY)
        self.assertEqual(defines["APP_NAME"], package.REGISTERED_NAME)
        registered = re.findall(r'^  !insertmacro REGISTER_FILE_TYPE "([^"]+)" "\$\{(\w+)\}"', nsi,
                                re.MULTILINE)
        removed = re.findall(r'^  !insertmacro UNREGISTER_FILE_TYPE \$\{ROOT\} "([^"]+)" "\$\{(\w+)\}"',
                             nsi, re.MULTILINE)
        expected = [(extension, prog_id) for extension, prog_id, _ in package.FILE_TYPES]
        self.assertEqual([(extension, defines[name]) for extension, name in registered], expected)
        self.assertEqual([(extension, defines[name]) for extension, name in removed], expected)
        rc = (REPO_ROOT / "packaging" / "windows" / "kalahari.rc.in").read_text(encoding="utf-8")
        self.assertIn("@KALAHARI_ICO_PATH@", rc)


if __name__ == "__main__":
    unittest.main()
