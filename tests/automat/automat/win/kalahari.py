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
from automat.runner import Paused, StepFailed
from automat.scenarios import limit_of
from automat.win import busy, clipboard, guard, screen, system, uia, windows
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
        self.start_ms = None
        self._clip_since = 0
        self._cursor = None  # where the automaton left the cursor

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
            self._open_chapter(opened["chapter"])
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
        found, _ = uia.wait_until(lambda: self._find_once(target)[0], OPEN_TIMEOUT, 0.2)
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
            tree = self.ui.dump(element, depth=10, limit=3000)
            (self.folder / name).write_text(json.dumps(tree, ensure_ascii=False, indent=1), encoding="utf-8")
        except Exception:  # noqa: BLE001 - a dump is a help for writing scenarios, not a result
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
            time.sleep(0.1)
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
        ended = {}
        if self.process is not None and self.process.poll() is None:
            self.process.kill()
            try:
                self.process.wait(10)
            except subprocess.TimeoutExpired:
                ended["warnings"] = ["program nie zakończył się po zabiciu procesu"]
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
        tops = windows.top_level(self.pid)
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
        for actual, _, _, element in self.ui.snapshot(root, kinds=kinds):
            if norm(actual) == norm(name) or (rows and _row_matches(actual, name)):
                loose.append(element)
                if actual != name:
                    self._note(f"napis w programie {_q(actual)}, w scenariuszu {_q(name)}")
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
    def _guard_point(self, x: int, y: int) -> None:
        """Never click outside the program: the user's other windows stay untouched."""
        root = windows.root_at(x, y)
        if not root or windows.owner_pid(root) != self.pid:
            title = window_events.window_title(root) if root else ""
            raise StepFailed(f"w miejscu kliknięcia ({x}, {y}) jest inne okno"
                             f"{' ' + _q(title) if title else ''}; automat nie klika poza programem")

    def _guard_keys(self) -> None:
        front = windows.foreground()
        if not front or windows.owner_pid(front) != self.pid:
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
        return {"start": start, "t0": t0 or time.perf_counter(), "element": described,
                "note": "; ".join(self.notes) or None}

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
        time.sleep(max(0, int(step["ms"])) / 1000)
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

    def _do_menu(self, step):
        path = [str(p) for p in step["path"]]
        roots, tops = self._windows_for(step.get("window", "*Kalahari"))
        if not roots:
            raise StepFailed(f"nie ma okna {_q(step.get('window'))}; otwarte okna: {_quote_list([w['title'] for w in tops])}")
        bars = roots[0].find_all(kinds=("MenuBar",))
        if not bars:
            raise StepFailed(f"okno {_q(roots[0].name)} nie ma paska menu")
        first = [e for e in self._named(bars[0], path[0], ("MenuItem",)) if e.visible()]
        if not first:
            names = self._names_under([bars[0]], ("MenuItem",))
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
        return moment, element

    # --- observing ------------------------------------------------------------------------
    def _area_rect(self, area, wait_until: float = 0.0):
        """The screen rectangle of an area: a window (plain Win32, never waits for the
        program), the clicked element, or any element."""
        while True:
            if area == "clicked":
                return self.last_element.rect if self.last_element is not None else None
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
            time.sleep(0.01)

    def _editor(self, target):
        if target:
            return self._find_once(target)
        roots, _ = self._windows_for(None)
        editors = []
        for root in roots:
            editors += [e for e in root.find_all(kinds=("Edit", "Document")) if e.visible()]
            if editors:
                break
        if not editors:
            return None, "nie ma otwartego edytora"
        focused = [e for e in editors if e.has_focus]
        if focused:
            return focused[0], None

        def area(e):
            left, top, right, bottom = e.rect
            return (right - left) * (bottom - top)

        return max(editors, key=area), None

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
                    ok = max(abs(a - b) for a, b in zip(mean, rgb)) <= int(check.get("tolerance", 24))
                return ok, f"średni kolor {hex_color}, jasność {light:.2f}", None
            color.close = shots.close
            return color
        return lambda: (False, f"automat nie umie sprawdzenia {_q(kind)}", None)

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
            elif kind == "screen_settled":
                thread = threading.Thread(target=self._settled, args=(results, index, check, t0, limit, watch_for),
                                          name="screen-settled", daemon=True)
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
                    pending.remove(item)
                elif now >= deadline:
                    results[index] = {"ok": False, "ms": None,
                                      "detail": f"nie nastąpiło w ciągu {deadline - t0:.0f} s; {detail}"}
                    pending.remove(item)
            if pending:
                time.sleep(0.01)
        for probe in probes:
            closer = getattr(probe, "close", None)
            if closer:
                closer()
        for thread in threads:
            thread.join(OBSERVE_AT_LEAST + 5)
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
        return results

    def _settled(self, results, index, check, t0, limit, watch_for) -> None:
        rect = self._area_rect(check["area"], wait_until=t0 + watch_for)
        if rect is None:
            results[index] = {"ok": False, "ms": None, "detail": "nie ma tego obszaru na ekranie"}
            return
        shots = screen.Screen()
        try:
            seen = shots.watch(rect, t0, settle_ms=check.get("quiet_ms", self.config.quiet_ms),
                               timeout=watch_for)
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

    # --- health and pictures ----------------------------------------------------------------
    def health(self, since: float) -> dict:
        now = time.perf_counter()
        longest = self.probe.longest_ms(since, now) if self.probe else None
        if self.process is None or self.process.poll() is not None:
            code = None if self.process is None else self.process.returncode
            return {"ok": False, "detail": f"program się zamknął (kod {code})", "busy_ms": longest}
        if self.probe is not None and not self.probe.wait_responsive(now, HANG_LIMIT):
            return {"ok": False, "detail": f"program nie odpowiadał przez {HANG_LIMIT:.0f} s", "busy_ms": longest}
        return {"ok": True, "detail": "", "busy_ms": longest}

    def screenshot(self, path: Path, area=None) -> bool:
        if area is not None:
            rect = self._area_rect(area)
        else:
            rects = [w["rect"] for w in windows.top_level(self.pid)] if self.pid else []
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


def session_factory(config, scenario, folder):
    return KalahariSession(config, scenario, folder)
