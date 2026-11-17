"""Kalahari under test, seen from outside, as a person sees it.

Each scenario gets a fresh program: its own settings folder (APPDATA), its own working
folder (kalahari.log goes there) and fresh copies of the books of the test kit. The
program's other data folders, which APPDATA does not move, are copied before the run and
put back after it (automat.win.guard); each scenario reports what it changed there.

Actions go through real input. Their effects are read through UI Automation (states and
texts), window events (the moments windows appear and go), screenshots ("ready on
screen") and the busy probe (how long the program did not answer).

The moment of an action (T0) is the moment of the input that sets it off: the release of
the mouse button, the press of the main key (programs act on the key press). Window events
are looked for from 50 ms before T0, so an event between a press and its T0 still counts.
"""

import difflib
import fnmatch
import json
import os
import re
import shutil
import subprocess
import threading
import time
import unicodedata
from pathlib import Path

from automat import books as books_mod
from automat import config as config_mod
from automat import coverage
from automat import files as files_mod
from automat import stopping
from automat.runner import Paused, StepFailed
from automat.scenarios import limit_of
from automat.win import busy, clipboard, guard, ocr, screen, system, sweep, uia, windows
from automat.win import events as window_events
from automat.win import input as inp

START_TIMEOUT = 90.0  # the first start of a fresh build can be slow (fonts, caches)
OPEN_TIMEOUT = 30.0  # a book from the command line and its chapter
FIND_TIMEOUT = 5.0  # an element named by an action
OBSERVE_AT_LEAST = 10.0  # how long a check is watched; a late effect is still measured
CLOSE_TIMEOUT = 30.0
HANG_LIMIT = 30.0  # a program that does not answer this long ends its scenario
EARLY = 0.05  # window events this long before T0 still belong to the action
MOVED_PX = 4  # the cursor this far from where the automaton left it: someone moved the mouse

MAIN_TITLE = re.compile(r"(^|\s[-–]\s)Kalahari$")
# A window that says something went wrong (the program's, or the system's about the program)
ERROR_TITLE = re.compile(r"błąd|error|exception|wyjątek|assert|runtime library|krytyczn|critical|"
                         r"awaria|przestał działać|stopped working|not responding|nie odpowiada", re.I)
SYSTEM_ERROR_PROGRAMS = {"werfault.exe", "werfaultsecure.exe", "wermgr.exe"}
FILE_DIALOG_CLASS = "#32770"
DIALOG_TIMEOUT = 10.0  # a system file dialog to appear, and to close after Enter
FILES = config_mod.AUTOMAT_DIR / "pliki"  # files the scenarios use: an image, a text to import
YOUR_SETTINGS = "twoje-obecne"  # the preset that copies the user's own settings

# Other programs a scenario may use ("app"): how to start them and how their windows are titled
APPS = {
    "notatnik": {"name": "Notatnik", "run": ["notepad.exe"], "titles": ["*Notatnik", "*Notepad"]},
    "eksplorator": {"name": "Eksplorator plików", "run": ["explorer.exe"], "titles": []},
    "word": {"name": "Word", "run": ["winword.exe"], "titles": ["*Word"]},
}

# --- Names --------------------------------------------------------------------------------

def norm(text) -> str:
    """A name as a person reads it: no '&' of the access keys, '…' and '...' alike, spaces
    and case not counted, the shortcut after a tab left out."""
    text = unicodedata.normalize("NFC", str(text or "")).split("\t")[0]
    text = text.replace("&&", "\0").replace("&", "").replace("\0", "&")
    text = text.replace("…", "...")
    return " ".join(text.split()).casefold()


def title_matches(title: str, pattern: str) -> bool:
    if pattern is None:
        return True
    if fnmatch.fnmatchcase(title or "", pattern):
        return True
    return norm(title) == norm(pattern) or fnmatch.fnmatchcase(norm(title), norm(pattern))


def _row_matches(name: str, row: str) -> bool:
    """A row named by its first words: "Pauza" is the row "Pauza U+2014" or "Pauza (—)"."""
    name, row = norm(name), norm(row)
    return name == row or (name.startswith(row) and not name[len(row)].isalnum())


def _q(text) -> str:
    return f"„{text}”"


def _quote_list(names, limit: int = 12) -> str:
    names = [n for n in dict.fromkeys(names) if n]
    if not names:
        return "żadnych"
    shown = ", ".join(_q(n) for n in names[:limit])
    return shown + (f" i {len(names) - limit} innych" if len(names) > limit else "")


def _area_key(area) -> str:
    return area if isinstance(area, str) else json.dumps(area, sort_keys=True, ensure_ascii=False)


def similar(name: str, names) -> list:
    unique = sorted({n for n in names if n})
    close = difflib.get_close_matches(name, unique, n=6, cutoff=0.5)
    if not close:
        lowered = {norm(n): n for n in unique}
        close = [lowered[n] for n in difflib.get_close_matches(norm(name), list(lowered), n=6, cutoff=0.5)]
    return close or unique[:12]


ROLE_WORDS = {"button": "przycisku", "edit": "pola", "list": "listy", "cell": "komórki",
              "menu_item": "pozycji menu", "tab": "karty", "check_box": "pola wyboru",
              "radio": "przycisku opcji", "combo_box": "listy rozwijanej", "tree_item": "pozycji drzewa",
              "list_item": "pozycji listy", "text": "napisu", "window": "okna"}


# --- The session -------------------------------------------------------------------------

class KalahariSession:
    """One scenario's program (automat.runner.Session)."""

    def __init__(self, config, scenario, folder: Path):
        self.config = config
        self.scenario = scenario
        self.folder = Path(folder)
        self.setup = (scenario.data.get("setup") or {}) if scenario is not None else {}
        self.work = self.folder / "_program"
        self.appdata = self.work / "AppData"
        self.process = None
        self.ui = None
        self.events = None
        self.probe = None
        self.main_hwnd = 0
        self.books = {}
        self.last_target = None
        self.last_element = None
        self.notes = []
        self.watched_before = {}
        self.launches = 0
        self.failed_checks = 0
        self._before = {}  # areas grabbed before an action, for checks that they must change
        self._before_clicked = False
        self.start_ms = None
        self._clip_since = 0
        self._cursor = None  # where the automaton left the cursor
        self.app = None  # another program a scenario uses: {"key", "name", "pid", "hwnd", "process"}
        self._in_app = False  # the current step works in that program
        self._expected_windows = []  # windows the current step expects (not error windows)
        self._dumped = set()  # windows whose UI Automation tree this scenario saved
        self.start_warnings = []  # said once, with the scenario's result
        self.seen = {}  # elements seen in the program's windows, for the coverage report
        self.touched = []  # elements the actions reached
        self._items = None  # what a sweep went through (the step's list of options)
        self.sweeping = False  # the current step is a sweep (its clicks count apart in the report)

    @property
    def pid(self) -> int:
        return self.process.pid if self.process else 0

    # --- start and stop ------------------------------------------------------------------
    def start(self) -> None:
        program = Path(self.config.program)
        if not program.is_file():
            raise StepFailed(f"nie ma programu {program} – ustaw jego ścieżkę w ustawieniach automatu")
        desktop = system.input_desktop()
        if desktop != "Default":
            raise StepFailed(f"ekran jest zablokowany albo nie ma pulpitu do pracy (pulpit: {desktop}); "
                             "automat potrzebuje odblokowanego ekranu")
        running = system.processes_of(program)
        if running:
            raise StepFailed(f"Kalahari z tego pliku już działa (proces {', '.join(map(str, running))}). "
                             "Zamknij go: automat uruchamia program sam, a drugi start oddałby książkę "
                             "działającemu oknu.")
        self.ui = uia.Uia()
        self._prepare()
        self.watched_before = guard.snapshot()
        self._launch()

    def _prepare(self) -> None:
        settings_dir = self.appdata / "Kalahari"
        settings_dir.mkdir(parents=True, exist_ok=True)
        preset_name = self.setup.get("settings", "domyslne")
        if preset_name == YOUR_SETTINGS:
            settings = self._your_settings()
        else:
            preset = Path(self.config.settings_presets) / f"{preset_name}.json"
            if not preset.is_file():
                raise StepFailed(f"nie ma zestawu ustawień {_q(preset_name)} (plik {preset})")
            try:
                settings = json.loads(preset.read_text(encoding="utf-8"))
            except ValueError as error:
                raise StepFailed(f"zestaw ustawień {_q(preset_name)} to nie jest poprawny JSON: {error}") from error
        (settings_dir / "settings.json").write_text(json.dumps(settings, ensure_ascii=False, indent=2),
                                                    encoding="utf-8")
        try:
            self.books = books_mod.make_books(self.setup.get("books", []), self.work / "ksiazki")
        except ValueError as error:
            raise StepFailed(str(error)) from error

    def _your_settings(self) -> dict:
        """A copy of the user's own settings (their file stays untouched), in Polish, because
        the scenarios name what they click in Polish. Without the file: the defaults."""
        own = Path(os.environ.get("APPDATA", "")) / "Kalahari" / "settings.json"
        try:
            settings = json.loads(own.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            self.start_warnings.append(f"nie da się odczytać Twoich ustawień ({own}): {error}; "
                                       "scenariusz działa na ustawieniach domyślnych")
            return {"version": "1.4", "ui": {"language": "pl"}}
        if not isinstance(settings, dict):
            return {"version": "1.4", "ui": {"language": "pl"}}
        if (settings.get("ui") or {}).get("language") != "pl":
            settings.setdefault("ui", {})["language"] = "pl"
            self.start_warnings.append("w kopii Twoich ustawień język programu zmieniono na polski")
        return settings

    def _launch(self) -> None:
        env = dict(os.environ)
        env["APPDATA"] = str(self.appdata)
        env.pop("KALAHARI_TEST_MODE", None)
        args = [str(self.config.program)]
        opened = self.setup.get("open") or {}
        if opened.get("book") in self.books:
            args.append(str(self.books[opened["book"]]))
        self.launches += 1
        output = open(self.work / f"wyjscie-{self.launches}.txt", "wb")  # noqa: SIM115 - the program keeps it
        began = time.perf_counter()
        self.process = subprocess.Popen(args, cwd=self.work, env=env, stdout=output, stderr=subprocess.STDOUT)
        output.close()
        self.events = window_events.WindowEvents(self.process.pid).start()
        main, _ = uia.wait_until(self._find_main, START_TIMEOUT, 0.1)
        if not main:
            code = self.process.poll()
            if code is not None:
                raise StepFailed(f"program zakończył się przy starcie (kod {code})")
            raise StepFailed(f"okno programu nie pojawiło się w ciągu {START_TIMEOUT:.0f} s")
        self.main_hwnd = main["hwnd"]
        self.probe = busy.BusyProbe(self.main_hwnd).start()
        if not self.probe.wait_responsive(time.perf_counter(), 30):
            raise StepFailed("program nie odpowiada po starcie")
        self.start_ms = round((time.perf_counter() - began) * 1000)
        windows.bring_forward(self.main_hwnd)
        if opened.get("chapter"):
            try:
                self._open_chapter(opened["chapter"])
            except StepFailed:
                self._dump("drzewo-po-bledzie.json", self.main_hwnd)
                raise
        self._calm(self.main_hwnd)
        if self.launches == 1:
            self._dump("drzewo-okna-glownego.json", self.main_hwnd)

    def _find_main(self):
        tops = windows.top_level(self.pid)
        for window in tops:
            if MAIN_TITLE.search(window["title"] or ""):
                return window
        return None

    def _open_chapter(self, chapter: str) -> None:
        """The chapter of setup.open, opened as a person does: a double click on its name in
        the book's tree."""
        target = {"name": chapter, "role": "tree_item"}

        def tree_item():
            found, _ = self._find_once(target)
            if found is not None:
                return found
            # The tree shows more than the name (a status such as "[Szkic]"): an item whose
            # text starts with the chapter's name and goes on with a non-letter is the chapter.
            main = self.ui.from_handle(self.main_hwnd)
            if main is None:
                return None
            for actual, _, offscreen, element in self.ui.snapshot(main, kinds=uia.ROLES["tree_item"]):
                if not offscreen and _row_matches(actual, chapter):
                    return element
            return None

        found, _ = uia.wait_until(tree_item, OPEN_TIMEOUT, 0.2)
        if found is None:
            _, problem = self._find_once(target)
            raise StepFailed(f"rozdział {_q(chapter)} nie pojawił się w drzewie książki: {problem}")
        x, y = found.center
        self._guard_point(x, y)
        inp.click(x, y, double=True)

        def editor_focused():
            focused = self.ui.focused()
            return focused is not None and focused.control_type in ("Edit", "Document") and focused.pid == self.pid

        ready, _ = uia.wait_until(editor_focused, OPEN_TIMEOUT, 0.1)
        if not ready:
            focused = self.ui.focused()
            raise StepFailed(f"rozdział {_q(chapter)} nie otworzył się w edytorze "
                             f"(fokus: {focused.describe() if focused else 'brak'})")
        self.probe.wait_responsive(time.perf_counter(), 10)

    def _calm(self, hwnd: int, quiet_ms: int = 300, timeout: float = 10.0) -> None:
        """Wait until the window stops changing, so a step starts on a quiet screen."""
        area = windows.rect(hwnd)
        if area[2] <= area[0]:
            return
        shots = screen.Screen()
        try:
            shots.watch(area, time.perf_counter(), settle_ms=quiet_ms, timeout=timeout)
        finally:
            shots.close()

    def _dump(self, name: str, hwnd: int) -> None:
        element = self.ui.from_handle(hwnd)
        if element is None:
            return
        try:
            tree = self.ui.dump(element, depth=12, limit=4000)
            (self.folder / name).write_text(json.dumps(tree, ensure_ascii=False, indent=1), encoding="utf-8")
            self._remember(tree, windows.window_title(hwnd))
        except Exception:  # noqa: BLE001 - a dump is a help for writing scenarios, not a result
            pass

    def _remember(self, tree: list, title: str, label: str = None) -> None:
        """Add the elements of a dumped tree to what this scenario saw (coverage report)."""
        window = _window_key(title)
        stack = list(tree)
        while stack:
            node = stack.pop(0)
            stack[0:0] = node.get("children", [])
            kind = node.get("type")
            if kind not in coverage.ACTIVE:
                continue
            left, top, right, bottom = node.get("rect") or (0, 0, 0, 0)
            if right - left <= 0 or bottom - top <= 0:
                continue
            if kind == "CheckBox" and not node.get("name") and right - left <= 14:
                continue  # Qt's own handle at the end of each toolbar
            key = (window, kind, node.get("name") or "", label or "")
            if key not in self.seen:
                self.seen[key] = {"window": window, "window_title": title, "type": kind, "name": node.get("name") or "",
                                  "description": node.get("description"), "enabled": node.get("enabled", True),
                                  "path": label}

    def see(self, window: str, kind: str, name: str, enabled: bool = True, title: str = None, path: str = None) -> None:
        """One element for the coverage report (the menus give their items with full paths)."""
        key = (window, kind, name, path or "")
        if key not in self.seen:
            self.seen[key] = {"window": window, "window_title": title or window, "type": kind, "name": name,
                              "description": None, "enabled": enabled, "path": path}

    def remember_entries(self, entries: list, title: str, label: str = None) -> None:
        """Elements read with Uia.controls, for the coverage report."""
        tree = [{"type": e["type"], "name": e["name"], "rect": list(e["rect"]), "enabled": e["enabled"]}
                for e in entries]
        self._remember(tree, title, label)

    @staticmethod
    def matches(title: str, pattern: str) -> bool:
        return title_matches(title, pattern)

    def _touch(self, element, x=None, y=None) -> None:
        try:
            hwnd = windows.root_at(x, y) if x is not None else 0
            title = windows.window_title(hwnd) if hwnd else ""
            kind = element.control_type
            if kind == "MenuItem" and not title:
                return  # an item of an open menu: the menu step records its whole path
            self.touched.append({"window": _window_key(title), "type": kind, "name": element.name,
                                 "sweep": self.sweeping})
        except Exception:  # noqa: BLE001 - only for the coverage report
            pass

    def _save_elements(self) -> None:
        try:
            data = {"seen": list(self.seen.values()), "touched": self.touched}
            (self.folder / coverage.ELEMENTS_FILE).write_text(json.dumps(data, ensure_ascii=False, indent=0),
                                                              encoding="utf-8")
        except OSError:
            pass

    def _close_program(self) -> None:
        """Close the program as its window's close button does, and wait for it to end."""
        if self.process is None or self.process.poll() is not None:
            return
        since = time.perf_counter()
        for window in windows.top_level(self.pid):
            if MAIN_TITLE.search(window["title"] or ""):
                windows.ask_to_close(window["hwnd"])
        deadline = since + CLOSE_TIMEOUT
        while self.process.poll() is None and time.perf_counter() < deadline:
            asked = [e for e in self.events.events(since) if e.kind == "shown" and e.title
                     and not MAIN_TITLE.search(e.title)]
            if asked:
                raise StepFailed(f"przy zamykaniu program pokazał okno {_q(asked[0].title)}")
            stopping.sleep(0.1)
        if self.process.poll() is None:
            raise StepFailed(f"program nie zamknął się w ciągu {CLOSE_TIMEOUT:.0f} s")
        self._stop_watchers()

    def _stop_watchers(self) -> None:
        if self.probe:
            self.probe.stop()
            self.probe = None
        if self.events:
            self.events.stop()
            self.events = None

    def stop(self) -> dict:
        self._save_elements()
        ended = {"warnings": list(self.start_warnings)} if self.start_warnings else {}
        self._close_app()
        if self.process is not None and self.process.poll() is None:
            self.process.kill()
            try:
                self.process.wait(10)
            except subprocess.TimeoutExpired:
                ended.setdefault("warnings", []).append("program nie zakończył się po zabiciu procesu")
        self._stop_watchers()
        log = self.work / "kalahari.log"
        if log.is_file():
            shutil.copyfile(log, self.folder / "kalahari.log")
            ended["log"] = "kalahari.log"
        if self.watched_before:
            changes = guard.changes(self.watched_before, guard.snapshot())
            if changes:
                ended.setdefault("warnings", []).extend(changes)
        return ended

    # --- finding elements ----------------------------------------------------------------
    def _windows_for(self, pattern):
        """The program's visible top-level windows that match, the one in front first, as
        UI Automation elements; and all of them (for messages)."""
        tops = windows.top_level(self._pid_now())
        if pattern is None:
            front = windows.foreground()
            ordered = sorted(tops, key=lambda w: w["hwnd"] != front)
        else:
            ordered = [w for w in tops if title_matches(w["title"], pattern)]
        found = []
        for window in ordered:
            element = self.ui.from_handle(window["hwnd"])
            if element is not None:
                found.append(element)
        return found, tops

    def _named(self, root, name: str, kinds, rows: bool = False) -> list:
        """Elements under root whose name is `name`: exactly, then as a person reads it."""
        exact = root.find_all(name=name, kinds=kinds)
        if exact:
            return exact
        for variant in {name.replace("...", "…"), name.replace("…", "...")} - {name}:
            exact = root.find_all(name=variant, kinds=kinds)
            if exact:
                return exact
        loose = []
        unnamed = []
        for actual, kind, _, element in self.ui.snapshot(root, kinds=kinds):
            if norm(actual) == norm(name) or (rows and _row_matches(actual, name)):
                loose.append(element)
                if actual != name:
                    self._note(f"napis w programie {_q(actual)}, w scenariuszu {_q(name)}")
            elif not actual and kind in ("Edit", "ComboBox", "Spinner"):
                unnamed.append(element)
        if not loose:
            # A field without a name: a person reads the hint in it (the placeholder text,
            # which Qt gives screen readers as the description)
            for element in unnamed:
                if norm(element.description) == norm(name):
                    loose.append(element)
                    self._note(f"pole bez nazwy dla czytników ekranu, z podpowiedzią {_q(element.description)}")
        return loose

    def _names_under(self, roots, kinds) -> list:
        names = []
        for root in roots:
            names += [n for n, _, offscreen, _ in self.ui.snapshot(root, kinds=kinds) if n and not offscreen]
        return names

    def _note(self, text: str) -> None:
        if text not in self.notes:
            self.notes.append(text)

    def _find_once(self, target):
        """(element, None) or (None, why not)."""
        if target == "editor":
            return self._editor(None)
        if target == "clicked":
            if self.last_element is None:
                return None, "żadne działanie nie wskazało jeszcze elementu"
            if self.last_target not in (None, "clicked"):
                found, _ = self._find_once(self.last_target)
                if found is not None:
                    return found, None
            return self.last_element, None
        if not isinstance(target, dict):
            return None, f"zły opis elementu: {target!r}"
        roots, tops = self._windows_for(target.get("window"))
        if not roots:
            return None, (f"nie ma okna {_q(target.get('window'))}; otwarte okna programu: "
                          f"{_quote_list([w['title'] for w in tops])}")
        if not ({"name", "row", "in"} & set(target)):
            return roots[0], None
        if "in" in target:
            containers = []
            for root in roots:
                containers += [c for c in self._named(root, target["in"], None) if c.visible()]
            if not containers:
                return None, (f"nie ma {_q(target['in'])}; podobne: "
                              f"{_quote_list(similar(target['in'], self._names_under(roots, None)))}")
            roots = containers
        if "row" in target:
            return self._find_row(roots, target)
        role = target.get("role")
        kinds = uia.ROLES.get(role) if role else None
        found = []
        for root in roots:
            found += self._named(root, target["name"], kinds)
        shown = [e for e in found if e.visible()]
        what = ROLE_WORDS.get(role, "elementu") + " "
        if not shown:
            if found:
                return None, f"{_q(target['name'])} jest, ale niewidoczny (poza ekranem albo zwinięty)"
            names = self._names_under(roots, kinds)
            return None, (f"nie ma {what}{_q(target['name'])}; podobne: {_quote_list(similar(target['name'], names))}")
        index = int(target.get("index", 1))
        if index < 1 or index > len(shown):
            return None, f"widocznych elementów {_q(target['name'])} jest {len(shown)}, a scenariusz chce {index}."
        return shown[index - 1], None

    def _find_row(self, roots, target):
        lists = []
        for root in roots:
            if "name" in target:
                lists += self._named(root, target["name"], uia.ROLES["list"])
            else:
                lists += root.find_all(kinds=uia.ROLES["list"])
        lists = [lst for lst in lists if lst.visible()]
        if not lists:
            names = self._names_under(roots, uia.ROLES["list"])
            return None, f"nie ma listy {_q(target.get('name', ''))}; listy w oknie: {_quote_list(names)}"
        row = str(target["row"])
        for lst in lists:
            cells = self._named(lst, row, uia.ROLES["cell"], rows=True)
            if not cells:
                continue
            cell = next((c for c in cells if c.visible()), cells[0])
            if "part" in target:
                return self._row_part(lst, cell, row, str(target["part"]))
            if "column" not in target:
                return cell, None
            headers = [h.name for h in lst.column_headers()]
            if not headers:
                headers = [n for n, _, _, _ in self.ui.snapshot(lst, kinds=("HeaderItem",))]
            column = next((i for i, h in enumerate(headers) if norm(h) == norm(target["column"])), None)
            if column is None:
                return None, (f"lista {_q(lst.name)} nie ma kolumny {_q(target['column'])}; "
                              f"kolumny: {_quote_list(headers)}")
            position = cell.grid_position()
            if position is None:
                return None, f"lista {_q(lst.name)} nie podaje położenia komórek (wiersz, kolumna)"
            found = lst.cell(position[0], column)
            if found is None:
                return None, f"w wierszu {_q(row)} nie ma komórki w kolumnie {_q(target['column'])}"
            return found, None
        names = []
        for lst in lists:
            names += [n for n, _, offscreen, _ in self.ui.snapshot(lst, kinds=uia.ROLES["cell"]) if n and not offscreen]
        return None, f"lista {_q(lists[0].name)} nie ma wiersza {_q(row)}; podobne: {_quote_list(similar(row, names))}"

    def _row_part(self, lst, cell, row: str, part: str):
        """An element named `part` in the row of `cell`: inside the cell itself, or in another
        cell of the same row (a star button, a check box)."""
        inside = [e for e in self._named(cell, part, None) if e.visible()]
        if inside:
            return inside[0], None
        position = cell.grid_position()
        names = []
        if position is not None:
            column = 0
            while column < 64:
                other = lst.cell(position[0], column)
                if other is None:
                    break
                if norm(other.name) == norm(part):
                    return other, None
                found = [e for e in self._named(other, part, None) if e.visible()]
                if found:
                    return found[0], None
                names.append(other.name)
                names += self._names_under([other], None)
                column += 1
        else:
            names = self._names_under([cell], None)
        return None, f"w wierszu {_q(row)} nie ma {_q(part)}; są tam: {_quote_list(names)}"

    def find(self, target, timeout: float = FIND_TIMEOUT):
        found, moment = uia.wait_until(lambda: self._find_once(target)[0], timeout, 0.05)
        if found is None:
            _, problem = self._find_once(target)
            self._dump("drzewo-po-bledzie.json", windows.foreground() or self.main_hwnd)
            raise StepFailed(problem or "nie ma takiego elementu")
        return found

    # --- acting -------------------------------------------------------------------------
    def _pid_now(self) -> int:
        """The process whose windows the current step works with."""
        return self.app["pid"] if self._in_app and self.app else self.pid

    def _allowed_pids(self) -> set:
        allowed = {self.pid}
        if self.app:
            allowed.add(self.app["pid"])
        return allowed

    def _guard_point(self, x: int, y: int) -> None:
        """Never click outside the program (and the other program a scenario opened): the
        user's other windows stay untouched."""
        root = windows.root_at(x, y)
        if not root or windows.owner_pid(root) not in self._allowed_pids():
            title = window_events.window_title(root) if root else ""
            raise StepFailed(f"w miejscu kliknięcia ({x}, {y}) jest inne okno"
                             f"{' ' + _q(title) if title else ''}; automat nie klika poza programem")

    def _guard_keys(self) -> None:
        front = windows.foreground()
        if not front or windows.owner_pid(front) not in self._allowed_pids():
            title = window_events.window_title(front) if front else ""
            raise StepFailed(f"klawisze trafiłyby do innego okna{' ' + _q(title) if title else ''}; "
                             "automat pisze tylko do programu")

    def _point(self, element, offset):
        left, top, right, bottom = element.rect
        if offset is None:
            return (left + right) // 2, (top + bottom) // 2
        if not (isinstance(offset, list) and len(offset) == 2 and all(isinstance(v, int) for v in offset)):
            raise StepFailed("„offset” to dwie liczby: [x, y] od lewego górnego rogu (ujemne – od prawego dolnego)")
        x = left + offset[0] if offset[0] >= 0 else right + offset[0]
        y = top + offset[1] if offset[1] >= 0 else bottom + offset[1]
        return x, y

    def _click_element(self, target, element, step=None) -> float:
        step = step or {}
        if not element.visible():
            raise StepFailed(f"{_q(element.name)} jest niewidoczny (poza ekranem albo zwinięty)")
        if not element.enabled:
            self._note(f"{_q(element.name)} był niedostępny")
        x, y = self._point(element, step.get("offset"))
        self._guard_point(x, y)
        if self._before_clicked:
            # The clicked element as it looks with the cursor over it, before the press: a
            # check that its picture must change compares with this
            inp.move_to(x, y)
            time.sleep(0.05)
            self._before["clicked"] = self._grab(element.rect)
        self._touch(element, x, y)
        moment = inp.click(x, y, button=step.get("button", "left"), double=bool(step.get("double")),
                           modifiers=tuple(step.get("modifiers", [])))
        self.last_target, self.last_element = target, element
        return moment

    def act(self, step: dict) -> dict:
        if self._cursor is not None and getattr(self.config, "pause_on_mouse", True):
            x, y = inp.cursor_position()
            if max(abs(x - self._cursor[0]), abs(y - self._cursor[1])) > MOVED_PX:
                self._cursor = None
                raise Paused("ktoś poruszył myszą")
        self.notes = []
        self._items = None
        self._before = {}
        self._in_app = bool(step.get("_app"))
        self._expected_windows = _expected_windows(step)
        changing = [c for c in step.get("expect", [])
                    if c.get("check") == "screen_settled" and c.get("must_change")]
        self._before_clicked = any(c.get("area") == "clicked" for c in changing)
        for check in changing:
            if check.get("area") != "clicked":
                rect = self._area_rect(check["area"])
                if rect is not None:
                    self._before[_area_key(check["area"])] = self._grab(rect)
        start = time.perf_counter()
        self._clip_since = clipboard.sequence()
        handler = getattr(self, "_do_" + str(step.get("do")), None)
        if handler is None:
            raise StepFailed(f"automat nie umie działania {_q(step.get('do'))}")
        try:
            t0, element = handler(step)
        except inp.InputError as error:
            raise StepFailed(f"system nie przyjął ruchu myszy albo klawisza: {error}") from error
        self._cursor = inp.cursor_position()
        described = None
        if element is not None:
            try:
                described = element.describe()
            except Exception:  # noqa: BLE001 - the element may be gone already
                described = None
        done = {"start": start, "t0": t0 or time.perf_counter(), "element": described,
                "note": "; ".join(self.notes) or None}
        if self._items is not None:
            done["items"] = self._items
        return done

    def _do_click(self, step):
        element = self.find(step["target"])
        return self._click_element(step["target"], element, step), element

    def _do_keys(self, step):
        element = None
        if step.get("target"):
            element = self.find(step["target"])
            self._click_element(step["target"], element)
            time.sleep(0.05)
        self._guard_keys()
        try:
            return inp.press(step["keys"]), element
        except ValueError as error:
            raise StepFailed(f"nieznany klawisz w {_q(step['keys'])}: {error}") from error

    def _do_type(self, step):
        element = None
        if step.get("target"):
            element = self.find(step["target"])
            self._click_element(step["target"], element)
            time.sleep(0.05)
        self._guard_keys()
        if step.get("replace"):
            inp.press("Ctrl+A")
        return inp.type_text(str(step["text"])), element

    def _do_select(self, step):
        element = self.find(step["target"])
        item = str(step["item"])
        kinds = ("ListItem", "TreeItem", "DataItem", "TabItem", "MenuItem", "RadioButton", "Button")
        if element.control_type == "ComboBox":
            self._click_element(step["target"], element)

            def in_popup():
                for window in self._windows_for(None)[0]:
                    found = [e for e in self._named(window, item, ("ListItem", "MenuItem")) if e.visible()]
                    if found:
                        return found[0]
                return None

            choice, _ = uia.wait_until(in_popup, FIND_TIMEOUT, 0.05)
            if choice is None:
                raise StepFailed(f"lista rozwijana {_q(element.name)} nie ma pozycji {_q(item)}")
        else:
            found = [e for e in self._named(element, item, kinds) if e.visible()]
            if not found:
                names = self._names_under([element], kinds)
                raise StepFailed(f"w {_q(element.name)} nie ma pozycji {_q(item)}; są: {_quote_list(similar(item, names))}")
            choice = found[0]
        return self._click_element({"in": element.name, "name": item}, choice), choice

    def _checked(self, element):
        state = element.toggle_state()
        if state is not None:
            return state == "on"
        return element.is_selected()

    def _do_toggle(self, step):
        element = self.find(step["target"])
        want = step.get("state")
        if want is not None and self._checked(element) == bool(want):
            self._note(f"{_q(element.name)} był już {'zaznaczony' if want else 'odznaczony'} – bez kliknięcia")
            self.last_target, self.last_element = step["target"], element
            return time.perf_counter(), element
        return self._click_element(step["target"], element), element

    def _do_scroll(self, step):
        element = self.find(step["target"])
        x, y = element.center
        self._guard_point(x, y)
        self.last_target, self.last_element = step["target"], element
        return inp.wheel(x, y, int(step["notches"])), element

    def _do_wait(self, step):
        stopping.sleep(max(0, int(step["ms"])) / 1000)
        return time.perf_counter(), None

    def _do_screenshot(self, step):
        name = re.sub(r"[^a-z0-9-]+", "-", str(step.get("name", "zrzut")).lower()).strip("-") or "zrzut"
        path = self.folder / f"{name}.png"
        number = 2
        while path.exists():
            path = self.folder / f"{name}-{number}.png"
            number += 1
        if self.screenshot(path, step.get("area")):
            self._note(f"zrzut {path.name}")
        return time.perf_counter(), None

    def _do_restart(self, step):
        self._close_program()
        self._launch()
        return time.perf_counter(), None

    def _do_drag(self, step):
        """Press on the target, move to `to` and release there; in a step of another program
        the target is in that program and `to` in Kalahari (a file from the Explorer)."""
        element = self.find(step["target"])
        if not element.visible():
            raise StepFailed(f"{_q(element.name)} jest niewidoczny")
        x1, y1 = self._point(element, step.get("offset"))
        self._guard_point(x1, y1)
        in_app, self._in_app = self._in_app, False
        try:
            to = step["to"]
            if isinstance(to, dict) and set(to) == {"offset"}:
                dx, dy = to["offset"]
                x2, y2 = x1 + int(dx), y1 + int(dy)
                destination = element
            else:
                destination = self.find(to)
                x2, y2 = destination.center
        finally:
            self._in_app = in_app
        self._guard_point(x2, y2)
        moment = inp.drag(x1, y1, x2, y2)
        self.last_target, self.last_element = step.get("to"), destination
        return moment, element

    def _file_dialog(self):
        for window in windows.top_level(self.pid):
            if window["class"] == FILE_DIALOG_CLASS:
                return window
        return None

    def _do_file_dialog(self, step):
        """The system window for choosing a file or a folder, which the program opened: the
        path goes into its name field and Enter confirms it (the dialog's own default button
        when Enter only opens the folder). Its texts follow the language of Windows, so the
        scenario gives only the path."""
        path = Path(self._path(step["path"]))
        found, _ = uia.wait_until(self._file_dialog, DIALOG_TIMEOUT, 0.05)
        if not found:
            titles = [w["title"] for w in windows.top_level(self.pid)]
            raise StepFailed(f"program nie pokazał systemowego okna wyboru pliku; jego okna: {_quote_list(titles)}")
        hwnd = found["hwnd"]
        windows.bring_forward(hwnd)
        self._calm(hwnd, quiet_ms=200, timeout=5)
        path.parent.mkdir(parents=True, exist_ok=True)
        dialog = self.ui.from_handle(hwnd)
        field = None
        if dialog is not None:
            edits = [e for e in dialog.find_all(kinds=("Edit",)) if e.visible()]
            named = [e for e in edits if e.automation_id in ("1148", "1001", "1152")]
            field = (named or edits or [None])[0]
        if field is not None:
            x, y = field.center
            self._guard_point(x, y)
            inp.click(x, y)
            time.sleep(0.05)
        self._guard_keys()
        inp.press("Ctrl+A")
        inp.type_text(str(path), interval=0.005)
        moment = inp.press("Enter")
        closed, _ = uia.wait_until(lambda: windows.rect(hwnd)[2] <= windows.rect(hwnd)[0]
                                   or not any(w["hwnd"] == hwnd for w in windows.top_level(self.pid)), 1.5, 0.05)
        if not closed and dialog is not None:
            # A folder: Enter went into it; the default button (IDOK) chooses it
            buttons = [b for b in dialog.find_all(kinds=("Button",)) if b.automation_id == "1" and b.visible()]
            if buttons:
                x, y = buttons[0].center
                self._guard_point(x, y)
                moment = inp.click(x, y)
        closed, _ = uia.wait_until(lambda: not any(w["hwnd"] == hwnd for w in windows.top_level(self.pid)),
                                   DIALOG_TIMEOUT, 0.05)
        if not closed:
            other = [w for w in windows.top_level(self.pid) if w["class"] == FILE_DIALOG_CLASS and w["hwnd"] != hwnd]
            said = _window_text(self.ui, other[0]["hwnd"]) if other else ""
            raise StepFailed(f"okno wyboru pliku nie zamknęło się po wpisaniu {_q(path)}"
                             + (f"; system pyta: {_q(said)}" if said else ""))
        self._note(f"ścieżka {path}")
        return moment, None

    def _do__app_open(self, step):
        """Start another program (Notatnik, the Explorer, Word) and wait for its window."""
        key = step["app"]
        known = APPS.get(key)
        if known is None:
            raise StepFailed(f"automat nie zna programu {_q(key)}; zna: {', '.join(APPS)}")
        args = list(known["run"])
        opened = step.get("open")
        titles = list(known["titles"])
        if opened:
            target = Path(self._path(opened))
            args.append(str(target))
            if key == "eksplorator":
                titles = [target.name, f"{target.name} *", f"* {target.name}"]
        before = {w["hwnd"] for w in windows.all_top_level()}
        try:
            process = subprocess.Popen(args, cwd=self.work)  # noqa: S603 - fixed programs
        except OSError as error:
            raise StepFailed(f"nie da się uruchomić programu {known['name']}: {error}") from error

        def window():
            for found in windows.all_top_level():
                if found["hwnd"] not in before and any(title_matches(found["title"], t) for t in titles):
                    return found
            return None

        found, moment = uia.wait_until(window, START_TIMEOUT / 3, 0.1)
        if not found:
            process.kill()
            raise StepFailed(f"okno programu {known['name']} nie pojawiło się")
        self.app = {"key": key, "name": known["name"], "pid": windows.owner_pid(found["hwnd"]),
                    "hwnd": found["hwnd"], "process": process}
        windows.bring_forward(found["hwnd"])
        self._calm(found["hwnd"], quiet_ms=300, timeout=10)
        return moment, None

    def _do__app_close(self, step):
        self._close_app()
        if self.main_hwnd:
            windows.bring_forward(self.main_hwnd)
        return time.perf_counter(), None

    def _close_app(self) -> None:
        app, self.app = self.app, None
        if not app:
            return
        if app["key"] == "eksplorator":
            windows.ask_to_close(app["hwnd"])  # the shell's process stays: only its window goes
            return
        try:
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(app["pid"])], capture_output=True, timeout=15,
                           check=False)
        except (OSError, subprocess.SubprocessError):
            pass
        if app["process"].poll() is None:
            app["process"].kill()

    @staticmethod
    def _menu_bars(root) -> list:
        """The window's menu bars without the system menu of its title bar (Windows puts a
        menu bar with the single item "System" there)."""
        titles = [t.rect for t in root.find_all(kinds=("TitleBar",))]

        def in_title(rect):
            # By height only: the system menu starts left of the title bar's own rectangle.
            return any(t[1] <= rect[1] and rect[3] <= t[3] for t in titles)

        return [bar for bar in root.find_all(kinds=("MenuBar",)) if not in_title(bar.rect)]

    def _do_menu(self, step):
        path = [str(p) for p in step["path"]]
        roots, tops = self._windows_for(step.get("window", "*Kalahari"))
        if not roots:
            raise StepFailed(f"nie ma okna {_q(step.get('window'))}; otwarte okna: {_quote_list([w['title'] for w in tops])}")
        bars = self._menu_bars(roots[0])
        if not bars:
            raise StepFailed(f"okno {_q(roots[0].name)} nie ma paska menu")
        first = []
        for bar in bars:
            first = [e for e in self._named(bar, path[0], ("MenuItem",)) if e.visible()]
            if first:
                break
        if not first:
            names = self._names_under(bars, ("MenuItem",))
            raise StepFailed(f"w pasku menu nie ma {_q(path[0])}; są: {_quote_list(names)}")
        moment = self._click_element({"name": path[0], "role": "menu_item"}, first[0])
        element = first[0]
        bar_window = roots[0].hwnd

        def popups():
            """The open menus: the program's top-level windows other than the one with the bar."""
            found = []
            for window in windows.top_level(self.pid):
                if window["hwnd"] != bar_window:
                    popup = self.ui.from_handle(window["hwnd"])
                    if popup is not None:
                        found.append(popup)
            return found

        for name in path[1:]:
            def in_menus(name=name):
                for popup in popups():
                    found = [e for e in self._named(popup, name, ("MenuItem",)) if e.visible()]
                    if found:
                        return found[0]
                return None

            element, _ = uia.wait_until(in_menus, FIND_TIMEOUT, 0.03)
            if element is None:
                names = self._names_under(popups(), ("MenuItem",))
                raise StepFailed(f"w menu nie ma pozycji {_q(name)}; są: {_quote_list(names)}")
            if not element.enabled:
                raise StepFailed(f"pozycja menu {_q(name)} jest niedostępna")
            moment = self._click_element({"name": name, "role": "menu_item"}, element)
        for depth in range(2, len(path) + 1):
            self.touched.append({"window": "menu", "type": "MenuItem", "name": " > ".join(path[:depth]),
                                 "sweep": self.sweeping})
        return moment, element

    def _do_sweep(self, step):
        """Every option of a kind in turn (automat.win.sweep); the list goes with the step."""
        began = time.perf_counter()
        self.sweeping = True
        try:
            self._items = sweep.Sweep(self, step).run()
        finally:
            self.sweeping = False
        return began, None

    # --- observing ------------------------------------------------------------------------
    def _area_rect(self, area, wait_until: float = 0.0):
        """The screen rectangle of an area: a window (plain Win32, never waits for the
        program), the clicked element, or any element."""
        while True:
            if area == "clicked":
                return self.last_element.rect if self.last_element is not None else None
            if area == "editor":
                element, _ = self._editor(None)
                if element is not None and element.visible():
                    return element.rect
            if isinstance(area, dict) and set(area) <= {"window"}:
                pattern = area.get("window")
                front = windows.foreground()
                tops = sorted(windows.top_level(self.pid), key=lambda w: w["hwnd"] != front)
                match = [w for w in tops if pattern is None or title_matches(w["title"], pattern)]
                if match:
                    return match[0]["rect"]
            else:
                element, _ = self._find_once(area)
                if element is not None and element.visible():
                    return element.rect
            if time.perf_counter() >= wait_until:
                return None
            stopping.sleep(0.01)

    def _editor(self, target):
        if target:
            return self._find_once(target)
        # The book's editor is in the main window, also while a dialog is in front (whose
        # own fields, such as a search field, are not the editor).
        main = self.ui.from_handle(self.main_hwnd) if self.main_hwnd else None
        if main is None:
            return None, "nie ma okna programu"
        editors = [e for e in main.find_all(kinds=("Edit", "Document")) if e.visible()]
        if not editors:
            return None, "nie ma otwartego edytora"

        def area(e):
            left, top, right, bottom = e.rect
            return (right - left) * (bottom - top)

        largest = max(editors, key=area)
        # Of several editors (a split view) the one with the focus; a small field with the
        # focus (the navigator's filter) is not one.
        focused = [e for e in editors if e.has_focus and area(e) * 4 >= area(largest)]
        return (focused[0] if focused else largest), None

    def _text_of(self, element) -> str:
        if element.control_type in ("Edit", "Document", "ComboBox", "Spinner"):
            value = element.value()
            if value is not None:
                return value
        return element.name

    @staticmethod
    def _compare(text, check):
        text = unicodedata.normalize("NFC", text or "")
        if "equals" in check:
            return text == unicodedata.normalize("NFC", str(check["equals"]))
        if "not_contains" in check:
            return unicodedata.normalize("NFC", str(check["not_contains"])) not in text
        if "contains" in check:
            return unicodedata.normalize("NFC", str(check["contains"])) in text
        return True

    def _probe(self, check, since: float):
        """A function that looks once: (satisfied, detail, moment or None)."""
        kind = check["check"]
        target = check.get("target")
        want = check.get("value", True)

        if kind in ("window_shown", "window_closed"):
            pattern = check["window"]
            kinds = ("shown",) if kind == "window_shown" else ("hidden", "destroyed")

            def window_probe():
                for event in self.events.events(since) if self.events else []:
                    if event.kind in kinds and title_matches(event.title, pattern):
                        return True, f"okno {_q(event.title)}", event.time
                open_now = [w["title"] for w in windows.top_level(self.pid) if w["title"]]
                return False, f"otwarte okna programu: {_quote_list(open_now)}", None
            return window_probe

        def element_probe(test):
            def run():
                element, problem = self._find_once(target)
                if element is None:
                    return False, problem, None
                return test(element)
            return run

        if kind == "visible":
            return element_probe(lambda e: (e.visible(), "widoczny" if e.visible() else "niewidoczny", None))
        if kind == "not_visible":
            def hidden():
                element, _ = self._find_once(target)
                gone = element is None or not element.visible()
                return gone, "niewidoczny" if gone else "nadal widoczny", None
            return hidden
        if kind == "enabled":
            return element_probe(lambda e: (e.enabled == bool(want), "dostępny" if e.enabled else "niedostępny", None))
        if kind == "checked":
            def checked(e):
                state = self._checked(e)
                word = {True: "zaznaczony", False: "niezaznaczony", None: "bez stanu zaznaczenia"}[state]
                return state == bool(want), word, None
            return element_probe(checked)
        if kind == "selected":
            def selected(e):
                state = e.is_selected()
                if state is None:
                    state = self._checked(e)
                return state == bool(want), "wybrany" if state else "niewybrany", None
            return element_probe(selected)
        if kind == "focused":
            return element_probe(lambda e: (e.has_focus, "ma fokus" if e.has_focus else "bez fokusu", None))
        if kind == "text":
            return element_probe(lambda e: (self._compare(self._text_of(e), check), f"tekst {_q(self._text_of(e))}", None))
        if kind == "value":
            def value(e):
                current = e.value()
                if current is None and e.range_value() is not None:
                    current = str(e.range_value())
                return self._compare(current, check), f"wartość {_q(current)}", None
            return element_probe(value)
        if kind == "editor_text":
            def editor():
                element, problem = self._editor(target)
                if element is None:
                    return False, problem, None
                text = element.text()
                if text is None:
                    text = element.value()
                shown = (text or "")[:160]
                return self._compare(text, check), f"tekst w edytorze: {_q(shown)}", None
            return editor
        if kind == "clipboard":
            def clip():
                if clipboard.sequence() == self._clip_since:
                    return False, "schowek bez zmian", None
                if "html_contains" in check:
                    html = clipboard.html() or ""
                    return check["html_contains"] in html, f"HTML: {_q(html[:160])}", None
                text = clipboard.text()
                return self._compare(text, check), f"tekst: {_q((text or '')[:160])}", None
            return clip
        if kind == "color":
            shots = screen.Screen()

            def color():
                rect = self._area_rect(check["area"])
                if rect is None:
                    return False, "nie ma tego obszaru na ekranie", None
                mean = screen.mean_color(shots.grab(rect))
                light = screen.luminance(mean)
                hex_color = "#{:02x}{:02x}{:02x}".format(*mean)
                if check.get("dark"):
                    ok = light < 0.2
                elif check.get("light"):
                    ok = light > 0.5
                else:
                    wanted = str(check.get("equals", "")).lstrip("#")
                    try:
                        rgb = tuple(int(wanted[i:i + 2], 16) for i in (0, 2, 4))
                    except ValueError:
                        return False, f"zły kolor {_q(check.get('equals'))}", None
                    ok = max(abs(a - b) for a, b in zip(mean, rgb, strict=False)) <= int(check.get("tolerance", 24))
                return ok, f"średni kolor {hex_color}, jasność {light:.2f}", None
            color.close = shots.close
            return color
        if kind == "file":
            def file_probe():
                matches = self._files(check["path"])
                if not matches:
                    return False, f"nie ma pliku {_q(self._path(check['path']))}", None
                if "contains" not in check:
                    return True, f"plik {_q(matches[0].name)}", None
                details = []
                for path in matches:
                    text, problem = files_mod.text_of(path)
                    if text is None:
                        details.append(problem)
                        continue
                    if self._compare(text, check):
                        return True, f"plik {_q(path.name)}", None
                    shown = " ".join(text.split())[:160]
                    details.append(f"{path.name}: {_q(shown)}")
                return False, "; ".join(details), None
            return file_probe
        if kind == "ocr":
            shots = screen.Screen()

            def ocr_probe():
                rect = self._area_rect(check["area"])
                if rect is None:
                    return False, "nie ma tego obszaru na ekranie", None
                text, problem = ocr.read(shots.grab(rect))
                if text is None:
                    return False, problem, None
                found = norm(check["contains"]) in norm(text)
                return found, f"rozpoznany tekst: {_q(text[:160])}", None
            ocr_probe.close = shots.close
            return ocr_probe
        return lambda: (False, f"automat nie umie sprawdzenia {_q(kind)}", None)

    # --- paths of files ----------------------------------------------------------------------
    def _path(self, text) -> str:
        """A path of a scenario with its placeholders filled: {praca} (the program's working
        folder of this scenario), {ksiazki} (the copies of the books), {ksiazka:karawana} (one
        book's folder), {pliki} (the automaton's files for scenarios), {wyniki} (this
        scenario's results)."""
        def fill(match):
            key, _, name = match.group(1).partition(":")
            if key == "praca":
                return str(self.work)
            if key == "ksiazki":
                return str(self.work / "ksiazki")
            if key == "ksiazka" and name in self.books:
                return str(Path(self.books[name]).parent)
            if key == "pliki":
                return str(FILES)
            if key == "wyniki":
                return str(self.folder)
            raise StepFailed(f"nieznane miejsce {_q('{' + match.group(1) + '}')} w ścieżce {_q(text)}; "
                             "znane: {praca}, {ksiazki}, {ksiazka:nazwa}, {pliki}, {wyniki}")
        filled = re.sub(r"\{([a-z]+(?::[a-z0-9-]+)?)\}", fill, str(text))
        return str(Path(filled))

    def _files(self, pattern) -> list:
        """The files the path names; it may hold * and ** (any file of an export folder)."""
        path = self._path(pattern)
        if any(c in path for c in "*?["):
            anchor = Path(path).anchor
            return sorted(p for p in Path(anchor).glob(str(Path(path).relative_to(anchor))) if p.is_file())
        return [Path(path)] if Path(path).is_file() else []

    def observe(self, checks: list, action: dict) -> list:
        t0 = action["t0"]
        since = t0 - EARLY
        results = [None] * len(checks)
        pending = []
        threads = []
        later = []
        probes = []
        for index, check in enumerate(checks):
            limit = limit_of(check, self.config.limits)
            watch_for = max(OBSERVE_AT_LEAST, 3 * (limit or 0) / 1000)
            kind = check.get("check")
            if kind == "busy_at_most_ms":
                later.append(index)
            elif kind in ("screen_settled", "image"):
                worker = self._settled if kind == "screen_settled" else self._image
                thread = threading.Thread(target=worker, args=(results, index, check, t0, limit, watch_for),
                                          name=kind, daemon=True)
                thread.start()
                threads.append(thread)
            else:
                probe = self._probe(check, since)
                probes.append(probe)
                pending.append([index, check, limit, t0 + watch_for, probe, None])
        while pending:
            for item in list(pending):
                index, check, limit, deadline, probe, _ = item
                try:
                    ok, detail, moment = probe()
                except Exception as error:  # noqa: BLE001 - a broken element is a failed look
                    ok, detail, moment = False, f"{type(error).__name__}: {error}", None
                now = time.perf_counter()
                item[5] = detail
                if ok:
                    ms = round(max(0.0, ((moment or now) - t0) * 1000), 1)
                    results[index] = {"ok": limit is None or ms <= limit, "ms": ms, "detail": detail}
                    if check.get("check") == "window_shown":
                        self._dump_window(check["window"])
                    pending.remove(item)
                elif now >= deadline:
                    results[index] = {"ok": False, "ms": None,
                                      "detail": f"nie nastąpiło w ciągu {deadline - t0:.0f} s; {detail}"}
                    pending.remove(item)
            if pending:
                stopping.check()
                time.sleep(0.01)
        for probe in probes:
            closer = getattr(probe, "close", None)
            if closer:
                closer()
        for thread in threads:
            while thread.is_alive():
                thread.join(0.05)
                stopping.check()
        stopping.check()
        for index in later:
            check = checks[index]
            if self.probe is None:
                results[index] = {"ok": False, "ms": None, "detail": "program nie działa"}
                continue
            responsive = self.probe.wait_responsive(time.perf_counter(), OBSERVE_AT_LEAST)
            end = time.perf_counter()
            longest = self.probe.longest_ms(action["start"], end)
            detail = f"najdłużej bez odpowiedzi: {longest:.0f} ms"
            if not responsive:
                detail += "; program wciąż nie odpowiada"
            results[index] = {"ok": responsive and longest <= check["value"], "ms": longest, "detail": detail}
        for index, result in enumerate(results):
            if result is None:
                results[index] = {"ok": False, "ms": None, "detail": "sprawdzenie nie skończyło się"}
        if not all(r["ok"] for r in results):
            # What the windows held when a check failed: the help for reading the result.
            self.failed_checks += 1
            front = windows.foreground()
            if front and windows.owner_pid(front) == self.pid and front != self.main_hwnd:
                self._dump(f"drzewo-po-niezgodnosci-{self.failed_checks}.json", front)
        return results

    @staticmethod
    def _grab(rect):
        """The screen rectangle now, or None when it cannot be grabbed."""
        if rect is None or rect[2] <= rect[0] or rect[3] <= rect[1]:
            return None
        shots = screen.Screen()
        try:
            return shots.grab(rect)
        except Exception:  # noqa: BLE001 - without it the check only sees changes after the action
            return None
        finally:
            shots.close()

    def _settled(self, results, index, check, t0, limit, watch_for) -> None:
        rect = self._area_rect(check["area"], wait_until=t0 + watch_for)
        if rect is None:
            results[index] = {"ok": False, "ms": None, "detail": "nie ma tego obszaru na ekranie"}
            return
        before = self._before.get(_area_key(check["area"])) if check.get("must_change") else None
        shots = screen.Screen()
        try:
            seen = shots.watch(rect, t0, settle_ms=check.get("quiet_ms", self.config.quiet_ms),
                               timeout=watch_for, before=before)
        except Exception as error:  # noqa: BLE001
            results[index] = {"ok": False, "ms": None, "detail": f"zrzuty: {error}"}
            return
        finally:
            shots.close()
        if seen["changed"]:
            ms = seen["last_change_ms"]
            detail = f"ostatnia zmiana po {ms:.0f} ms"
        else:
            ms = seen["first_frame_ms"]
            detail = f"bez zmian od pierwszej klatki ({ms:.0f} ms po działaniu)"
        detail += f"; klatki: {seen['frames']}, średnio {seen['frame_ms']:.0f} ms"
        if not seen["settled"]:
            results[index] = {"ok": False, "ms": None, "detail": f"obraz nie przestał się zmieniać w ciągu {watch_for:.0f} s"}
            return
        if check.get("must_change") and not seen["changed"]:
            results[index] = {"ok": False, "ms": None, "detail": "obraz się nie zmienił, a miał się zmienić; " + detail}
            return
        results[index] = {"ok": limit is None or ms <= limit, "ms": ms, "detail": detail}

    def _image(self, results, index, check, t0, limit, watch_for) -> None:
        """An area against the pattern the user approved for this screen. Without one, the
        area as it is once it stops changing is saved as a proposal and the check waits for
        approval (it does not decide the result)."""
        name = str(check["pattern"])
        stored = patterns_dir() / f"{name}.png"
        rect = self._area_rect(check["area"], wait_until=t0 + watch_for)
        if rect is None:
            results[index] = {"ok": False, "ms": None, "detail": "nie ma tego obszaru na ekranie"}
            return
        shots = screen.Screen()
        try:
            if not stored.is_file():
                seen = shots.watch(rect, t0, settle_ms=check.get("quiet_ms", self.config.quiet_ms), timeout=watch_for)
                proposal = self.folder / f"wzorzec-{_slug(name)}.png"
                seen["image"].save(proposal)
                results[index] = {"ok": True, "ms": None, "pending": True, "proposal_file": str(proposal),
                                  "pattern": name, "screen": system.screen_key(),
                                  "detail": f"nie ma jeszcze zatwierdzonego wzorca {_q(name)} dla tego ekranu "
                                            f"({system.screen_text()}); zrzut zapisano jako propozycję do zatwierdzenia"}
                return
            from PIL import Image  # noqa: PLC0415
            pattern = Image.open(stored).convert("RGB")
            masks = check.get("mask") or []
            tolerance = int(check.get("tolerance", 24))
            deadline = t0 + watch_for
            last = None
            while True:
                picture = shots.grab(rect)
                moment = time.perf_counter()
                last = screen.compare(pattern, picture, tolerance, masks)
                if last["same"]:
                    ms = round(max(0.0, (moment - t0) * 1000), 1)
                    results[index] = {"ok": limit is None or ms <= limit, "ms": ms, "pattern": name,
                                      "detail": "zgodny z wzorcem; " + last["detail"]}
                    return
                if moment >= deadline:
                    break
                stopping.check()
                time.sleep(0.02)
            actual = self.folder / f"ekran-{_slug(name)}.png"
            picture.save(actual)
            outcome = {"ok": False, "ms": None, "pattern": name, "actual_file": str(actual),
                       "detail": "niezgodny z wzorcem; " + last["detail"]}
            if last.get("diff") is not None:
                diff = self.folder / f"roznice-{_slug(name)}.png"
                last["diff"].save(diff)
                outcome["diff_file"] = str(diff)
            results[index] = outcome
        except Exception as error:  # noqa: BLE001
            results[index] = {"ok": False, "ms": None, "detail": f"porównanie z wzorcem: {error}"}
        finally:
            shots.close()

    def _dump_window(self, pattern) -> None:
        """The tree of a window the scenario opened, once per window: what is in it, for
        whoever writes the next scenario."""
        for window in windows.top_level(self._pid_now()):
            title = window["title"] or ""
            if title_matches(title, pattern) and title not in self._dumped:
                self._dumped.add(title)
                self._dump(f"drzewo-okna-{_slug(title)[:40]}.json", window["hwnd"])
                return

    # --- health and pictures ----------------------------------------------------------------
    def health(self, since: float) -> dict:
        now = time.perf_counter()
        longest = self.probe.longest_ms(since, now) if self.probe else None
        if self.process is None or self.process.poll() is not None:
            code = None if self.process is None else self.process.returncode
            return {"ok": False, "detail": f"program się zamknął (kod {code})", "busy_ms": longest}
        if self.probe is not None and not self.probe.wait_responsive(now, HANG_LIMIT):
            return {"ok": False, "detail": f"program nie odpowiadał przez {HANG_LIMIT:.0f} s", "busy_ms": longest}
        problem = self._error_window()
        if problem:
            self._dump("drzewo-okna-bledu.json", problem[1])
            return {"ok": False, "detail": problem[0], "busy_ms": longest,
                    "say": "nie ma okna błędu programu ani systemu"}
        return {"ok": True, "detail": "", "busy_ms": longest}

    def _error_window(self):
        """(what it says, hwnd) of a window that reports an error and that the step did not
        expect: the program's own, a system message of its process, or Windows Error
        Reporting about it. None when there is none."""
        for window in windows.top_level(self.pid):
            title = window["title"] or ""
            if not ERROR_TITLE.search(title):
                continue
            if any(title_matches(title, expected) for expected in self._expected_windows):
                continue
            text = _window_text(self.ui, window["hwnd"])
            return f"program pokazał okno {_q(title)}{': ' + _q(text) if text else ''}", window["hwnd"]
        for window in windows.all_top_level():
            image = (system.process_image(windows.owner_pid(window["hwnd"])) or "").lower()
            if os.path.basename(image) in SYSTEM_ERROR_PROGRAMS:
                text = _window_text(self.ui, window["hwnd"])
                if "kalahari" in (window["title"] + " " + text).lower():
                    return f"system pokazał okno błędu {_q(window['title'])}: {_q(text)}", window["hwnd"]
        return None

    def screenshot(self, path: Path, area=None) -> bool:
        if area is not None:
            rect = self._area_rect(area)
        else:
            rects = [w["rect"] for w in windows.top_level(self.pid)] if self.pid else []
            if self.app:
                rects += [w["rect"] for w in windows.top_level(self.app["pid"])]
            if not rects:
                return False
            rect = (min(r[0] for r in rects), min(r[1] for r in rects),
                    max(r[2] for r in rects), max(r[3] for r in rects))
        if rect is None:
            return False
        shots = screen.Screen()
        try:
            left, top, right, bottom = shots.bounds()
            rect = (max(rect[0], left), max(rect[1], top), min(rect[2], right), min(rect[3], bottom))
            if rect[2] <= rect[0] or rect[3] <= rect[1]:
                return False
            shots.grab(rect).save(path)
            return True
        except Exception:  # noqa: BLE001 - a missing picture must not stop a run
            return False
        finally:
            shots.close()


def _expected_windows(step: dict) -> list:
    """The windows a step names in its checks: not error windows, whatever their titles."""
    found = []
    for check in step.get("expect", []):
        if check.get("window"):
            found.append(check["window"])
        for key in ("target", "area"):
            if isinstance(check.get(key), dict) and check[key].get("window"):
                found.append(check[key]["window"])
    return found


def _window_key(title: str) -> str:
    """The window an element is in, for the coverage report: every book window is one."""
    if MAIN_TITLE.search(title or ""):
        return "okno książki"
    return title or "okno bez tytułu"


def _slug(text) -> str:
    plain = unicodedata.normalize("NFKD", str(text)).encode("ascii", "ignore").decode("ascii")
    return re.sub(r"[^a-z0-9]+", "-", plain.lower()).strip("-") or "okno"


def _window_text(ui, hwnd: int) -> str:
    """The texts of a message window (what it says), shortened."""
    element = ui.from_handle(hwnd)
    if element is None:
        return ""
    texts = [n for n, kind, offscreen, _ in ui.snapshot(element, kinds=("Text", "Document", "Edit"))
             if n and not offscreen]
    return " ".join(texts)[:300]


def patterns_dir() -> Path:
    """The image patterns the user approved for this screen (size and scale), outside the
    repository: another screen needs its own."""
    return config_mod.data_dir() / "wzorce" / system.screen_key()


def _has_word() -> bool:
    try:
        import winreg  # noqa: PLC0415
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                            r"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\winword.exe"):
            return True
    except OSError:
        return shutil.which("winword") is not None


def skip_reason(scenario):
    """Why the scenario cannot run on this computer (None when it can)."""
    uses = {step.get("app") for step in scenario.steps if step.get("do") == "_app_open"}
    if ("word" in uses or "wymaga-worda" in scenario.tags) and not _has_word():
        return "na tym komputerze nie ma Worda (scenariusz ze znacznikiem „wymaga-worda”)"
    return None


def session_factory(config, scenario, folder):
    return KalahariSession(config, scenario, folder)


session_factory.skip_reason = skip_reason
