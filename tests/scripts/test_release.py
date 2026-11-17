#!/usr/bin/env python3
"""
Tests of scripts/release.py: the version check, the bump and the cut of the CHANGELOG,
and the notes of a release. Plain unittest, run by CTest (scripts.release).
"""

import contextlib
import io
import json
import sys
import tempfile
import unittest
from collections import Counter
from pathlib import Path
from typing import Dict, Optional

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))

import release  # noqa: E402  (the path is set just above)

CMAKE = """cmake_minimum_required(VERSION 3.21)

project(Kalahari
    VERSION 0.3.0
    DESCRIPTION "Writer's IDE"
    LANGUAGES CXX
)

set(KALAHARI_VERSION_SUFFIX "alpha")
"""

VCPKG = """{
  "name": "kalahari",
  "version": "0.3.0",
  "dependencies": [
    {
      "name": "qtbase",
      "version>=": "6.5.0"
    }
  ]
}
"""

README = """# Kalahari

**Current Version:** 0.3.0-alpha
**Next Release:** 0.4.0-alpha
"""

ROADMAP = """# Roadmap

**Version:** 0.3.0-alpha
**Next Release:** 0.4.0-alpha
"""

CHANGELOG = """# Changelog

Intro.

---

## [Unreleased]

A summary of the release.

- One highlight.

### Added

- **New B** - 2026-11-02. Second block of Added.

### Fixed

- **Fix A** - 2026-11-01.

### Added

- **New A** - 2026-10-01. Older block of Added.
  Second line of the entry.

### Changed

- **Change A** - 2026-10-02.

### Removed

- **Gone A** - 2026-10-03.

### Changed

- **Change B** - 2026-09-01.

---

## [0.3.0-alpha] - 2025-11-20

### Added

- **Qt** - 2025-11-20.
"""


class Repository:
    """A folder with the five files of the version, as the repository has them."""

    def __init__(self, folder: Path, changed: Dict[str, str]) -> None:
        self.root = folder
        files = {
            "CMakeLists.txt": CMAKE,
            "vcpkg.json": VCPKG,
            "README.md": README,
            "ROADMAP.md": ROADMAP,
            "CHANGELOG.md": CHANGELOG,
        }
        files.update(changed)
        for name, text in files.items():
            with open(folder / name, "w", encoding="utf-8", newline="") as stream:
                stream.write(text)

    def text(self, name: str) -> str:
        with open(self.root / name, encoding="utf-8", newline="") as stream:
            return stream.read()


def entry_lines(text: str):
    """The lines of a CHANGELOG that are not headings, separators or blank."""
    return Counter(
        line for line in text.splitlines()
        if line.strip() and not line.startswith("#") and line.strip() != "---"
    )


class ReleaseTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.path = Path(self.folder.name)

    def repository(self, changed: Optional[Dict[str, str]] = None) -> Repository:
        return Repository(self.path, changed or {})


class CheckTest(ReleaseTestCase):
    def test_files_that_agree_pass(self) -> None:
        repo = self.repository()
        self.assertEqual(release.check(repo.root, None), [])
        self.assertEqual(release.check(repo.root, "v0.3.0-alpha"), [])

    def test_a_tag_of_another_version_fails(self) -> None:
        repo = self.repository()
        problems = release.check(repo.root, "v0.4.0-alpha")
        self.assertEqual(len(problems), 1)
        self.assertIn("v0.4.0-alpha", problems[0])
        self.assertEqual(len(release.check(repo.root, "0.3.0-alpha")), 1)

    def test_each_file_of_another_version_is_named(self) -> None:
        repo = self.repository({
            "vcpkg.json": VCPKG.replace('"version": "0.3.0"', '"version": "0.2.0"'),
            "README.md": README.replace("0.3.0-alpha", "0.3.1-alpha", 1),
            "ROADMAP.md": ROADMAP.replace("0.3.0-alpha", "0.3.2-alpha", 1),
        })
        problems = release.check(repo.root, None)
        self.assertEqual(len(problems), 3)
        self.assertTrue(any(p.startswith("vcpkg.json") for p in problems))
        self.assertTrue(any(p.startswith("README.md") and "0.3.1-alpha" in p for p in problems))
        self.assertTrue(any(p.startswith("ROADMAP.md") and "0.3.2-alpha" in p for p in problems))

    def test_the_version_of_the_dependencies_is_not_taken_for_the_program(self) -> None:
        # "version>=" of a dependency is not the "version" of the manifest
        repo = self.repository()
        self.assertEqual(release.check(repo.root, None), [])

    def test_a_changelog_without_the_section_of_the_version_fails(self) -> None:
        repo = self.repository({"CHANGELOG.md": CHANGELOG.replace("## [0.3.0-alpha] - 2025-11-20", "## [0.2.0-alpha] - 2025-11-15")})
        problems = release.check(repo.root, None)
        self.assertEqual(len(problems), 1)
        self.assertIn("[0.3.0-alpha]", problems[0])

    def test_a_section_without_a_date_does_not_count(self) -> None:
        repo = self.repository({"CHANGELOG.md": CHANGELOG.replace("## [0.3.0-alpha] - 2025-11-20", "## [0.3.0-alpha]")})
        self.assertEqual(len(release.check(repo.root, None)), 1)

    def test_unreleased_first_and_only_once(self) -> None:
        twice = CHANGELOG + "\n## [Unreleased]\n\n### Added\n\n- **Old** - 2025-11-19.\n"
        repo = self.repository({"CHANGELOG.md": twice})
        problems = release.check(repo.root, None)
        self.assertTrue(any("2 times" in p for p in problems))

        moved = CHANGELOG.replace("## [Unreleased]", "## [0.3.1-alpha] - 2025-12-01", 1)
        repo = self.repository({"CHANGELOG.md": moved})
        problems = release.check(repo.root, None)
        self.assertTrue(any("first section" in p for p in problems))

    def test_a_suffix_left_empty_is_a_final_release(self) -> None:
        repo = self.repository({
            "CMakeLists.txt": CMAKE.replace('"alpha"', '""'),
            "README.md": README.replace("0.3.0-alpha", "0.3.0", 1),
            "ROADMAP.md": ROADMAP.replace("0.3.0-alpha", "0.3.0", 1),
            "CHANGELOG.md": CHANGELOG.replace("## [0.3.0-alpha]", "## [0.3.0]"),
        })
        self.assertEqual(release.check(repo.root, "v0.3.0"), [])


class BumpTest(ReleaseTestCase):
    def test_every_file_takes_the_new_version(self) -> None:
        repo = self.repository()
        self.assertEqual(release.bump(repo.root, "0.4.0", "alpha", "2026-11-20"), "0.4.0-alpha")
        self.assertIn("VERSION 0.4.0\n", repo.text("CMakeLists.txt"))
        self.assertIn('set(KALAHARI_VERSION_SUFFIX "alpha")', repo.text("CMakeLists.txt"))
        manifest = json.loads(repo.text("vcpkg.json"))
        self.assertEqual(manifest["version"], "0.4.0")
        self.assertEqual(manifest["dependencies"][0]["version>="], "6.5.0")
        self.assertIn("**Current Version:** 0.4.0-alpha\n", repo.text("README.md"))
        self.assertIn("**Next Release:** 0.4.0-alpha", repo.text("README.md"))
        self.assertIn("**Version:** 0.4.0-alpha\n", repo.text("ROADMAP.md"))
        self.assertEqual(release.check(repo.root, "v0.4.0-alpha"), [])

    def test_the_changelog_is_cut_with_one_heading_per_kind(self) -> None:
        repo = self.repository()
        release.bump(repo.root, "0.4.0", "alpha", "2026-11-20")
        text = repo.text("CHANGELOG.md")
        headings = [line for line in text.splitlines() if line.startswith("#")]
        self.assertEqual(headings, [
            "# Changelog",
            "## [Unreleased]",
            "## [0.4.0-alpha] - 2026-11-20",
            "### Added",
            "### Changed",
            "### Removed",
            "### Fixed",
            "## [0.3.0-alpha] - 2025-11-20",
            "### Added",
        ])
        # Nothing is lost or doubled, and the newer block of a kind stays first
        self.assertEqual(entry_lines(text), entry_lines(CHANGELOG))
        self.assertLess(text.index("**New B**"), text.index("**New A**"))
        self.assertLess(text.index("**Change A**"), text.index("**Change B**"))
        self.assertIn("- **New A** - 2026-10-01. Older block of Added.\n  Second line of the entry.\n", text)

    def test_unreleased_starts_empty_and_the_summary_goes_with_the_release(self) -> None:
        repo = self.repository()
        release.bump(repo.root, "0.4.0", "alpha", "2026-11-20")
        text = repo.text("CHANGELOG.md")
        self.assertIn(
            "---\n\n## [Unreleased]\n\n---\n\n## [0.4.0-alpha] - 2026-11-20\n\n"
            "A summary of the release.\n\n- One highlight.\n\n### Added\n\n- **New B**",
            text,
        )
        self.assertIn("- **Fix A** - 2026-11-01.\n\n---\n\n## [0.3.0-alpha] - 2025-11-20\n", text)
        self.assertTrue(text.endswith("- **Qt** - 2025-11-20.\n"))

    def test_the_line_ends_of_the_changelog_stay(self) -> None:
        repo = self.repository({"CHANGELOG.md": CHANGELOG.replace("\n", "\r\n")})
        release.bump(repo.root, "0.4.0", "alpha", "2026-11-20")
        text = repo.text("CHANGELOG.md")
        self.assertNotIn("\n", text.replace("\r\n", ""))
        self.assertIn("## [0.4.0-alpha] - 2026-11-20\r\n", text)

    def test_a_version_released_already_changes_nothing(self) -> None:
        repo = self.repository()
        with self.assertRaises(release.ReleaseError):
            release.bump(repo.root, "0.3.0", "alpha", "2026-11-20")
        self.assertEqual(repo.text("CMakeLists.txt"), CMAKE)
        self.assertEqual(repo.text("CHANGELOG.md"), CHANGELOG)

    def test_nothing_to_release_changes_nothing(self) -> None:
        empty = CHANGELOG.split("A summary")[0] + "---\n\n## [0.3.0-alpha] - 2025-11-20\n"
        repo = self.repository({"CHANGELOG.md": empty})
        with self.assertRaises(release.ReleaseError):
            release.bump(repo.root, "0.4.0", "alpha", "2026-11-20")
        self.assertEqual(repo.text("vcpkg.json"), VCPKG)

    def test_a_file_without_its_field_changes_nothing(self) -> None:
        repo = self.repository({"ROADMAP.md": "# Roadmap\n"})
        with self.assertRaises(release.ReleaseError):
            release.bump(repo.root, "0.4.0", "alpha", "2026-11-20")
        self.assertEqual(repo.text("CMakeLists.txt"), CMAKE)
        self.assertEqual(repo.text("README.md"), README)
        self.assertEqual(repo.text("CHANGELOG.md"), CHANGELOG)

    def test_wrong_versions_and_dates_are_refused(self) -> None:
        repo = self.repository()
        for number, suffix, date in [
            ("0.4", "alpha", "2026-11-20"),
            ("v0.4.0", "alpha", "2026-11-20"),
            ("0.4.0", "al pha", "2026-11-20"),
            ("0.4.0", "alpha", "20.11.2026"),
            ("0.4.0", "alpha", "2026-02-30"),
        ]:
            with self.subTest(number=number, suffix=suffix, date=date):
                with self.assertRaises(release.ReleaseError):
                    release.bump(repo.root, number, suffix, date)
        self.assertEqual(repo.text("CMakeLists.txt"), CMAKE)

    def test_a_final_release_has_no_suffix(self) -> None:
        repo = self.repository()
        self.assertEqual(release.bump(repo.root, "1.0.0", "", "2027-02-23"), "1.0.0")
        self.assertIn('set(KALAHARI_VERSION_SUFFIX "")', repo.text("CMakeLists.txt"))
        self.assertIn("## [1.0.0] - 2027-02-23\n", repo.text("CHANGELOG.md"))
        self.assertEqual(release.check(repo.root, "v1.0.0"), [])


class NotesTest(ReleaseTestCase):
    def test_the_notes_are_the_summary_and_a_link(self) -> None:
        repo = self.repository()
        release.bump(repo.root, "0.4.0", "alpha", "2026-11-20")
        notes = release.release_notes(repo.text("CHANGELOG.md"), "0.4.0-alpha")
        self.assertEqual(
            notes,
            "A summary of the release.\n\n- One highlight.\n\n"
            "Every change of this version is in "
            "[CHANGELOG.md](https://github.com/bartoszwarzocha/kalahari/blob/v0.4.0-alpha/CHANGELOG.md).\n",
        )

    def test_a_section_without_a_summary_has_no_notes(self) -> None:
        repo = self.repository()
        with self.assertRaises(release.ReleaseError):
            release.release_notes(repo.text("CHANGELOG.md"), "0.3.0-alpha")
        with self.assertRaises(release.ReleaseError):
            release.release_notes(repo.text("CHANGELOG.md"), "0.9.0-alpha")


class CommandLineTest(ReleaseTestCase):
    def run_main(self, argv, root: Path):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = release.main(argv, root=root)
        return code, out.getvalue(), err.getvalue()

    def test_exit_codes(self) -> None:
        repo = self.repository()
        self.assertEqual(self.run_main(["check"], repo.root)[0], 0)
        code, _out, err = self.run_main(["check", "--tag", "v9.9.9"], repo.root)
        self.assertEqual(code, 1)
        self.assertIn("v9.9.9", err)
        self.assertEqual(self.run_main(["bump", "0.4.0", "--date", "2026-11-20"], repo.root)[0], 0)
        self.assertEqual(self.run_main(["check", "--tag", "v0.4.0-alpha"], repo.root)[0], 0)
        code, out, _err = self.run_main(["notes"], repo.root)
        self.assertEqual(code, 0)
        self.assertTrue(out.startswith("A summary of the release."))
        code, _out, err = self.run_main(["bump", "0.4.0", "--date", "2026-11-21"], repo.root)
        self.assertEqual(code, 1)
        self.assertIn("already there", err)


if __name__ == "__main__":
    unittest.main(verbosity=2)
