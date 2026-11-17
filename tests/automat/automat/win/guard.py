"""The user's data that a test run must leave as it was, and a computer that stays awake.

APPDATA moves only the program's settings folder. Its other data folders come from the
system's known folders (Qt 6.9 reads them with SHGetKnownFolderPath), so they are the
user's own: Documents\\Kalahari (books, archives, backups), the dictionaries in the roaming
AppData and the grammar components in the local AppData. Before the first scenario each of
them is compared with a kept copy in the automaton's data folder and only the files that
differ are copied (automat.mirror); after the last one the files the tests changed or
removed come back from that copy. Nothing is deleted: a file the tests added, and the
tests' version of a file they changed, are moved into the run's folder.
"""

import ctypes
import os
import time
from pathlib import Path

from automat import config as config_mod
from automat import mirror
from automat.win import system

ORGANISATION = "Bartosz W. Warzocha & Kalahari Team"  # QCoreApplication::organizationName
FOLDERS = [
    ("dokumenty", "documents", "Kalahari"),
    ("slowniki", "roaming_app_data", os.path.join(ORGANISATION, "Kalahari")),
    ("dane-lokalne", "local_app_data", "Kalahari"),
]
NAMES = {"dokumenty": "Dokumenty\\Kalahari", "slowniki": "słowniki użytkownika",
         "dane-lokalne": "dane lokalne programu (składniki gramatyki)"}
listing = mirror.listing
differences = mirror.differences


def folders() -> list:
    """[(key, path)] of the watched folders on this computer."""
    out = []
    for key, known, sub in FOLDERS:
        base = system.known_folder(known)
        if base:
            out.append((key, Path(base) / sub))
    return out


def _quote(names, limit=8) -> str:
    names = list(names)
    shown = ", ".join(f"„{n}”" for n in names[:limit])
    return shown + (f" i {len(names) - limit} innych" if len(names) > limit else "")


def describe(folder: Path, diff: dict) -> str:
    parts = []
    if diff["added"]:
        parts.append("nowe: " + _quote(diff["added"]))
    if diff["changed"]:
        parts.append("zmienione: " + _quote(diff["changed"]))
    if diff["removed"]:
        parts.append("usunięte: " + _quote(diff["removed"]))
    return f"{folder} ({'; '.join(parts)})"


def snapshot() -> dict:
    return {str(path): listing(path) for _, path in folders()}


def changes(before: dict, after: dict) -> list:
    """Sentences about the watched folders that changed between two snapshots."""
    out = []
    for folder, old in before.items():
        new = after.get(folder)
        if old is None or new is None:
            continue
        diff = differences(old, new)
        if diff["added"] or diff["removed"] or diff["changed"]:
            out.append("program zmienił folder " + describe(Path(folder), diff))
    return out


def _mb(size: int) -> str:
    return f"{size / 1024 ** 2:.0f} MB" if size >= 1024 ** 2 else f"{max(1, size // 1024)} kB"


class FolderGuard:
    """Bring the kept copies up to date before the run, put the folders back after it.
    progress(text, done, total) reports the copying (bytes) as it goes."""

    def __init__(self, backup: Path, kept: Path = None):
        self.backup = Path(backup)
        self.kept = Path(kept) if kept else config_mod.data_dir() / "kopia-twoich-danych"
        self.mirrors = []  # (key, Mirror)
        self.messages = []
        self.progress = None

    def _say(self, text, done=None, total=None) -> None:
        if self.progress:
            self.progress(text, done, total)

    def before(self) -> None:
        for key, folder in folders():
            name = NAMES.get(key, key)
            kept = mirror.Mirror(folder, self.kept / key)
            began = time.perf_counter()
            self._say(f"Kopia: {name} – porównuję z kopią z poprzedniego przebiegu")
            try:
                result = kept.update(lambda done, total, name=name: self._say(
                    f"Kopia: {name} – {_mb(done)} z {_mb(total)}", done, total))
            except ValueError as error:
                self.messages.append(f"{error}: automat go nie kopiuje, tylko sprawdza, czy testy go nie zmieniły")
                continue
            seconds = time.perf_counter() - began
            copied = (f"skopiowano {result['copied_files']} plików ({_mb(result['copied_bytes'])})"
                      if result["copied_files"] else "bez zmian od poprzedniej kopii")
            self._say(f"Kopia: {name} ({result['files']} plików, {_mb(result['bytes'])}) – {copied}, "
                      f"{seconds:.1f} s".replace(".", ","))
            if kept.failed:
                self.messages.append(f"nie dało się skopiować z {folder}: {_quote(kept.failed)}")
            self.mirrors.append((key, kept))

    def after(self) -> list:
        """Put the folders back; returns sentences about what the run changed there."""
        out = list(self.messages)
        for key, kept in self.mirrors:
            moved = self.backup / "dodane-przez-testy" / key
            diff = kept.restore(moved, self.backup / "zmienione-przez-testy" / key)
            if diff["added"] or diff["removed"] or diff["changed"]:
                out.append("przywrócono " + describe(kept.folder, diff) +
                           (f"; pliki dodane przez testy są w {moved}" if diff["added"] else ""))
            if diff["lost"]:
                out.append(f"NIE DAŁO SIĘ PRZYWRÓCIĆ (brak kopii): {_quote(diff['lost'])} w {kept.folder}")
        return out


# --- Awake --------------------------------------------------------------------------------

ES_CONTINUOUS = 0x80000000
ES_SYSTEM_REQUIRED = 0x00000001
ES_DISPLAY_REQUIRED = 0x00000002


def keep_awake(on: bool) -> None:
    """No sleep and no screen switched off while a run lasts. It does not stop the screen
    saver or the lock after idle time (Microsoft Docs); real input does reset that timer."""
    flags = ES_CONTINUOUS | (ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED if on else 0)
    ctypes.windll.kernel32.SetThreadExecutionState(flags)


class RunGuard:
    """What a run on the user's computer starts and ends with: the folders copied and put
    back (when the settings say so) and the computer kept awake."""

    def __init__(self, backup: Path, config):
        self.config = config
        self.folders = FolderGuard(backup) if getattr(config, "restore_folders", True) else None

    def before(self) -> None:
        if getattr(self.config, "keep_awake", True):
            keep_awake(True)
        if self.folders is not None:
            self.folders.progress = getattr(self, "progress", None)
            self.folders.before()

    def after(self) -> list:
        try:
            return self.folders.after() if self.folders is not None else []
        finally:
            keep_awake(False)
