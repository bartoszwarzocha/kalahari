"""The user's data that a test run must leave as it was, and a computer that stays awake.

APPDATA moves only the program's settings folder. Its other data folders come from the
system's known folders (Qt 6.9 reads them with SHGetKnownFolderPath), so they are the
user's own: Documents\\Kalahari (books, archives, backups), the dictionaries in the roaming
AppData and the grammar components in the local AppData. The run copies them before the
first scenario and puts them back after the last one (the accepted design, 17.11.2026).
Nothing is deleted: a file the tests added, and the tests' version of a file they
changed, are moved into the run's folder.
"""

import ctypes
import os
import shutil
from pathlib import Path

from automat.win import system

ORGANISATION = "Bartosz W. Warzocha & Kalahari Team"  # QCoreApplication::organizationName
FOLDERS = [
    ("dokumenty", "documents", "Kalahari"),
    ("slowniki", "roaming_app_data", os.path.join(ORGANISATION, "Kalahari")),
    ("dane-lokalne", "local_app_data", "Kalahari"),
]
LISTING_LIMIT = 20000  # entries; a bigger folder is only reported, not copied
COPY_LIMIT = 2 * 1024 ** 3  # bytes


def folders() -> list:
    """[(key, path)] of the watched folders on this computer."""
    out = []
    for key, known, sub in FOLDERS:
        base = system.known_folder(known)
        if base:
            out.append((key, Path(base) / sub))
    return out


def listing(folder: Path):
    """{relative path: (size, mtime_ns)} (folders end with os.sep), or None when the folder
    is too big to walk."""
    if not folder.is_dir():
        return {}
    found = {}
    stack = [folder]
    while stack:
        current = stack.pop()
        try:
            entries = list(os.scandir(current))
        except OSError:
            continue
        for entry in entries:
            if len(found) >= LISTING_LIMIT:
                return None
            try:
                relative = os.path.relpath(entry.path, folder)
                if entry.is_dir(follow_symlinks=False):
                    stack.append(Path(entry.path))
                    found[relative + os.sep] = (0, 0)
                else:
                    stat = entry.stat(follow_symlinks=False)
                    found[relative] = (stat.st_size, stat.st_mtime_ns)
            except OSError:
                continue
    return found


def _quote(names, limit=8) -> str:
    names = list(names)
    shown = ", ".join(f"„{n}”" for n in names[:limit])
    return shown + (f" i {len(names) - limit} innych" if len(names) > limit else "")


def differences(old: dict, new: dict) -> dict:
    files_old = {k: v for k, v in old.items() if not k.endswith(os.sep)}
    files_new = {k: v for k, v in new.items() if not k.endswith(os.sep)}
    return {"added": sorted(set(files_new) - set(files_old)),
            "removed": sorted(set(files_old) - set(files_new)),
            "changed": sorted(k for k in set(files_old) & set(files_new) if files_old[k] != files_new[k]),
            "new_dirs": sorted((set(new) - set(old)) - set(files_new))}


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


class FolderGuard:
    """Copy before the run, put back after it."""

    def __init__(self, backup: Path):
        self.backup = Path(backup)
        self.saved = []  # (key, folder, listing or None when not copied)
        self.messages = []

    def before(self) -> None:
        for key, folder in folders():
            found = listing(folder)
            if found is None:
                self.messages.append(f"folder {folder} ma ponad {LISTING_LIMIT} plików: automat go nie "
                                     "kopiuje, tylko sprawdza, czy testy go nie zmieniły")
                self.saved.append((key, folder, None))
                continue
            size = sum(v[0] for k, v in found.items() if not k.endswith(os.sep))
            if size > COPY_LIMIT:
                self.messages.append(f"folder {folder} ma {size // 1024 ** 2} MB: automat go nie kopiuje")
                self.saved.append((key, folder, None))
                continue
            if folder.is_dir():
                shutil.copytree(folder, self.backup / "kopia" / key, symlinks=True)
            self.saved.append((key, folder, found))

    def after(self) -> list:
        """Put the folders back; returns sentences about what the run changed there."""
        out = list(self.messages)
        clean = True
        for key, folder, old in self.saved:
            if old is None:
                continue
            new = listing(folder) or {}
            diff = differences(old, new)
            if not (diff["added"] or diff["removed"] or diff["changed"] or diff["new_dirs"]):
                continue
            clean = False
            copy = self.backup / "kopia" / key
            moved = self.backup / "dodane-przez-testy" / key
            for relative in diff["added"]:
                target = moved / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.move(str(folder / relative), str(target))
            for relative in diff["changed"]:
                kept = self.backup / "zmienione-przez-testy" / key / relative
                kept.parent.mkdir(parents=True, exist_ok=True)
                shutil.move(str(folder / relative), str(kept))
            for relative in diff["changed"] + diff["removed"]:
                target = folder / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(copy / relative, target)
            for relative in sorted(diff["new_dirs"], key=len, reverse=True):
                try:
                    (folder / relative).rmdir()  # only an empty folder the tests made
                except OSError:
                    pass
            if not old and folder.is_dir():
                try:
                    folder.rmdir()
                except OSError:
                    pass
            out.append("przywrócono " + describe(folder, diff) +
                       (f"; pliki dodane przez testy są w {moved}" if diff["added"] else ""))
        if clean:
            shutil.rmtree(self.backup / "kopia", ignore_errors=True)
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
            self.folders.before()

    def after(self) -> list:
        try:
            return self.folders.after() if self.folders is not None else []
        finally:
            keep_awake(False)
