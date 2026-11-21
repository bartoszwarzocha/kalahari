#!/usr/bin/env python3
"""
Tests of scripts/package.py: the version of the packages, the lists of files of the installer
of Windows, the folder of the Qt plugins of the Linux packages, the check of a program, and
the files of the packages agreeing with each other. Plain unittest, run by CTest
(scripts.package). The packages themselves are made by the CI (BUILDING.md, "Packages").
"""

import os
import re
import sys
import tempfile
import unittest
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


class DebianPackageTest(unittest.TestCase):
    def test_the_names_of_the_libraries_in_shlibs(self):
        self.assertEqual(package.shlibs_name("libQt6Core.so.6"), ("libQt6Core", "6"))
        self.assertEqual(package.shlibs_name("libpython3.15.so.1.0"), ("libpython3.15", "1.0"))
        self.assertEqual(package.shlibs_name("libhunspell-1.7.so.0"), ("libhunspell-1.7", "0"))
        self.assertEqual(package.shlibs_name("libfoo-2.1.so"), ("libfoo", "2.1"))
        # Libraries without a version: the program's own and the modules of Python
        self.assertIsNone(package.shlibs_name("libkalahari_core.so"))
        self.assertIsNone(package.shlibs_name("_ssl.cpython-315-x86_64-linux-gnu.so"))

    def test_the_libraries_left_to_the_system(self):
        with tempfile.TemporaryDirectory() as folder:
            lib = Path(folder)
            for name in ("libgtk-3.so.0", "libglib-2.0.so.0", "libQt6Core.so.6", "libpng16.so.16"):
                write(lib / name)
            package.remove_system_libraries(lib)
            self.assertEqual(sorted(p.name for p in lib.iterdir()),
                             ["libQt6Core.so.6", "libpng16.so.16"])


class PackagingFilesTest(unittest.TestCase):
    """The files of the packages name the same things."""

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
        book_type = re.search(r'<mime-type type="([^"]+)"', mime).group(1)
        self.assertIn(f"MimeType={book_type};\n", desktop)
        self.assertIn('<glob pattern="*.klh"/>', mime)
        # The program names its windows after the menu entry (Wayland)
        main = (REPO_ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn('setDesktopFileName(QStringLiteral("kalahari"))', main)

    def test_the_installer_of_windows(self):
        nsi = (REPO_ROOT / "packaging" / "windows" / "kalahari.nsi").read_text(encoding="utf-8")
        for define in ("VERSION", "VERSION_NUMBER", "STAGE", "OUTFILE", "FILES", "UNFILES"):
            self.assertIn(define, re.search(r"!ifndef (.*)", nsi).group(1))
        self.assertIn('!include "${FILES}"', nsi)
        self.assertIn('!include "${UNFILES}"', nsi)
        self.assertIn('"$INSTDIR\\${APP_EXE}" "%1"', nsi)  # the books open in the program
        rc = (REPO_ROOT / "packaging" / "windows" / "kalahari.rc.in").read_text(encoding="utf-8")
        self.assertIn("@KALAHARI_ICO_PATH@", rc)


if __name__ == "__main__":
    unittest.main()
