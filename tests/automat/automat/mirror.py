"""A kept copy of a folder, brought up to date before every run and used to put the folder
back after it.

The first run copies the whole folder (the grammar components alone are about 500 MB); every
later run compares the folder with the copy (size and modification time of each file, read
in a fraction of a second) and copies only the files that differ. After the run, the files
the tests changed or removed come back from the copy, and the files they added are moved
out into the run's folder. Nothing in the user's folder is deleted.

A file is copied under a temporary name and renamed when complete, so a copy stopped
halfway never looks finished; the next run copies that file again.
"""

import os
import shutil
from pathlib import Path

from automat import stopping

LISTING_LIMIT = 50000  # entries; a bigger folder is only reported, not copied
CHUNK = 4 * 1024 * 1024
PARTIAL = ".automat-kopia"  # the ending of a file still being copied


def listing(folder: Path):
    """{relative path: (size, mtime_ns)} (folders end with os.sep), or None when the folder
    is too big to walk."""
    folder = Path(folder)
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
                elif not entry.name.endswith(PARTIAL):
                    stat = entry.stat(follow_symlinks=False)
                    found[relative] = (stat.st_size, stat.st_mtime_ns)
            except OSError:
                continue
    return found


def files_of(found: dict) -> dict:
    return {k: v for k, v in found.items() if not k.endswith(os.sep)}


def differences(old: dict, new: dict) -> dict:
    files_old, files_new = files_of(old), files_of(new)
    return {"added": sorted(set(files_new) - set(files_old)),
            "removed": sorted(set(files_old) - set(files_new)),
            "changed": sorted(k for k in set(files_old) & set(files_new) if files_old[k] != files_new[k]),
            "new_dirs": sorted((set(new) - set(old)) - set(files_new))}


def size_of(found: dict) -> int:
    return sum(v[0] for v in files_of(found).values())


def _copy_file(source: Path, target: Path, progress) -> None:
    """Copy with the time of modification, in chunks (each one reported and stoppable)."""
    target.parent.mkdir(parents=True, exist_ok=True)
    partial = target.with_name(target.name + PARTIAL)
    try:
        with open(source, "rb") as reader, open(partial, "wb") as writer:
            while True:
                stopping.check()
                chunk = reader.read(CHUNK)
                if not chunk:
                    break
                writer.write(chunk)
                progress(len(chunk))
        shutil.copystat(source, partial)
        os.replace(partial, target)
    finally:
        if partial.exists():
            partial.unlink()


class Mirror:
    def __init__(self, folder: Path, copy: Path):
        self.folder = Path(folder)
        self.copy = Path(copy)
        self.state = None  # the folder's listing when the copy was brought up to date
        self.failed = []  # files that could not be copied (in use, no access)

    def update(self, progress=None) -> dict:
        """Bring the copy up to date; progress(done_bytes, total_bytes) after each chunk.
        Returns {"files", "bytes", "copied_files", "copied_bytes"}; raises Stopped."""
        progress = progress or (lambda done, total: None)
        state = listing(self.folder)
        if state is None:
            raise ValueError(f"folder {self.folder} ma ponad {LISTING_LIMIT} plików")
        kept = listing(self.copy) or {}
        wanted, have = files_of(state), files_of(kept)
        stale = [k for k, v in wanted.items() if have.get(k) != v]
        total = sum(wanted[k][0] for k in stale)
        done = 0

        def step(count):
            nonlocal done
            done += count
            progress(done, total)

        progress(0, total)
        self.failed = []
        for relative in sorted(stale):
            try:
                _copy_file(self.folder / relative, self.copy / relative, step)
            except stopping.Stopped:
                raise
            except OSError as error:
                self.failed.append(f"{relative} ({error.strerror or error})")
        for relative in set(have) - set(wanted):  # gone from the folder: gone from the copy
            try:
                (self.copy / relative).unlink()
            except OSError:
                pass
        self.state = state
        return {"files": len(wanted), "bytes": size_of(state), "copied_files": len(stale) - len(self.failed),
                "copied_bytes": done}

    def restore(self, moved_to: Path, kept_to: Path) -> dict:
        """Put the folder back as it was when the copy was updated. Added files go to
        moved_to, the tests' versions of changed files to kept_to. Returns the differences
        (with "lost": changed files that had no copy)."""
        now = listing(self.folder) or {}
        diff = differences(self.state, now)
        diff["lost"] = []
        if not (diff["added"] or diff["removed"] or diff["changed"] or diff["new_dirs"]):
            return diff
        failed = {f.split(" (")[0] for f in self.failed}
        for relative in diff["added"]:
            target = Path(moved_to) / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(self.folder / relative), str(target))
        for relative in diff["changed"] + diff["removed"]:
            if relative in failed or not (self.copy / relative).is_file():
                diff["lost"].append(relative)
                continue
            if relative in diff["changed"]:
                kept = Path(kept_to) / relative
                kept.parent.mkdir(parents=True, exist_ok=True)
                shutil.move(str(self.folder / relative), str(kept))
            target = self.folder / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(self.copy / relative, target)
        for relative in sorted(diff["new_dirs"], key=len, reverse=True):
            try:
                (self.folder / relative).rmdir()  # only an empty folder the tests made
            except OSError:
                pass
        if not files_of(self.state) and self.folder.is_dir():
            try:
                self.folder.rmdir()
            except OSError:
                pass
        return diff
