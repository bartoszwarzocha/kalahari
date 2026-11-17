"""Going through every option of a kind, as a person who tries each one in turn.

A sweep is one step of a scenario, {"do": "sweep", "what": ...}:
- "menu": every item of the menu bar, submenus included;
- "toolbars": every button, switch and list of the main window's toolbars (the hidden ones
  too, shown with a toolbar's extension button);
- "controls": every button, switch, list and tab of one window ("window"), or of one part
  of it ("in"); "open" gives the steps that open the window again after a button closed it;
- "pages": every page of the tree in a window (Settings) and, with "controls": true, every
  control of each page.
"skip" and "only" are name patterns (* and ?) of the options to leave out or to keep.

What a sweep expects comes from the user's decision of 17.11.2026 (every option of the
program is clicked through): each option reacts without an error window, the program keeps
answering and stays alive, and a switch changes its state. What an option should do is
checked by the other scenarios; a sweep finds the options that break the program, and the
coverage report the options no scenario checks.

After each option the sweep puts the program back: it closes the windows the option opened
(Esc, then the window's close button), switches the switches back, leaves a mode that hides
the bars with Esc, and starts the program again when its main window is gone, shows another
book or does not answer.
"""

import fnmatch
import re
import time
import unicodedata

from automat import stopping
from automat.coverage import norm
from automat.runner import StepFailed
from automat.win import uia, windows
from automat.win import input as inp

WINDOW_WAIT = 1.0  # a window an option opens may come this long after the click
HANG_LIMIT = 30.0
MAX_RESTARTS = 25
FIND_TIMEOUT = 5.0
TOOL_KINDS = ("Button", "SplitButton", "CheckBox", "RadioButton", "ComboBox")
CONTROL_KINDS = ("Button", "SplitButton", "CheckBox", "RadioButton", "ComboBox", "TabItem", "Hyperlink")
SWITCH_KINDS = ("CheckBox", "RadioButton", "TabItem")
# Buttons that close their window: tried after the other controls of the window
CLOSERS = re.compile(r"^(ok|anuluj|zamknij|cancel|close|zastosuj|apply|gotowe|tak|nie|yes|no)$", re.I)
DEFAULT_SKIP = {"menu": ["* > Zakończ"]}  # ends the program: a scenario of its own


def _plain(title: str) -> str:
    """A window title without the mark of unsaved changes."""
    return re.sub(r"\s+", " ", (title or "").replace("*", "")).strip()


def _passive(window: dict) -> bool:
    """Windows no option opens: tooltips."""
    return "tooltip" in (window.get("class") or "").lower()


def _width(rect) -> int:
    return rect[2] - rect[0]


def _shown(entry: dict) -> bool:
    rect = entry["rect"]
    return not entry["offscreen"] and rect[2] > rect[0] and rect[3] > rect[1]


def _identities(entries: list) -> list:
    """(name, kind, n): the n-th element of that name and kind, in reading order."""
    counts = {}
    out = []
    for entry in sorted(entries, key=lambda e: (e["rect"][1], e["rect"][0])):
        key = (entry["name"], entry["type"])
        counts[key] = counts.get(key, 0) + 1
        out.append(((entry["name"], entry["type"], counts[key]), entry))
    return out


def _label(ident) -> str:
    name, kind, number = ident
    if name:
        return name if number == 1 else f"{name} ({number})"
    return f"bez nazwy nr {number} ({kind})"


class Sweep:
    def __init__(self, session, step: dict):
        self.s = session
        self.step = step
        self.what = step["what"]
        self.skip = [str(p) for p in step.get("skip", [])] + DEFAULT_SKIP.get(self.what, [])
        self.only = [str(p) for p in step.get("only", [])]
        self.items = []
        self.restarts = 0
        self.keep = set()
        self.title = ""

    # --- the whole sweep -------------------------------------------------------------------
    def run(self) -> list:
        self._remember_start()
        handler = {"menu": self._menu, "context_menu": self._context, "toolbars": self._toolbars,
                   "controls": self._controls, "pages": self._pages}.get(self.what)
        if handler is None:
            raise StepFailed(f"nie ma przeglądu „{self.what}”; są: menu, context_menu, toolbars, controls, pages")
        handler()
        self._baseline()
        return self.items

    def _wanted(self, label: str) -> bool:
        text = norm(label)
        if self.only and not any(fnmatch.fnmatchcase(text, norm(p)) for p in self.only):
            return False
        return not any(fnmatch.fnmatchcase(text, norm(p)) for p in self.skip)

    def _add(self, item: dict) -> dict:
        item.setdefault("ok", True)
        self.items.append(item)
        return item

    # --- the program's state -----------------------------------------------------------------
    def _remember_start(self) -> None:
        s = self.s
        self.keep = {w["hwnd"] for w in windows.top_level(s.pid)}
        self.title = _plain(windows.window_title(s.main_hwnd))

    def _alive(self) -> bool:
        return self.s.process is not None and self.s.process.poll() is None

    def _main(self):
        return self.s.ui.from_handle(self.s.main_hwnd) if self._alive() else None

    def _extra(self, before: set) -> list:
        """The program's windows that were not there before (menus and dialogs included)."""
        return [w for w in windows.top_level(self.s.pid)
                if w["hwnd"] not in before and w["hwnd"] != self.s.main_hwnd and not _passive(w)]

    def _foreign(self) -> set:
        return {w["hwnd"] for w in windows.all_top_level() if windows.owner_pid(w["hwnd"]) != self.s.pid}

    def _esc(self) -> bool:
        front = windows.foreground()
        if front and windows.owner_pid(front) == self.s.pid:
            inp.press("Esc")
            return True
        return False

    def _close_extra(self, keep: set) -> bool:
        """Close the windows that are not in keep: Esc first, then the close button."""
        for attempt in range(4):
            extra = self._extra(keep)
            if not extra:
                return True
            windows.bring_forward(extra[0]["hwnd"])
            stopping.sleep(0.1)
            self._esc()
            stopping.sleep(0.35)
            if attempt >= 1:
                for window in self._extra(keep):
                    windows.ask_to_close(window["hwnd"])
                stopping.sleep(0.5)
        return not self._extra(keep)

    def _problem(self) -> str:
        """What keeps the program from its starting state, or ''."""
        s = self.s
        if not self._alive():
            return f"program się zamknął (kod {s.process.returncode if s.process else '–'})"
        if not any(w["hwnd"] == s.main_hwnd for w in windows.top_level(s.pid)):
            return "nie ma głównego okna"
        title = _plain(windows.window_title(s.main_hwnd))
        if title != self.title:
            return f"główne okno ma tytuł „{title}” zamiast „{self.title}”"
        extra = self._extra(self.keep)
        if extra:
            return "otwarte okna: " + ", ".join(f"„{w['title']}”" for w in extra[:4])
        main = self._main()
        if main is None or not [b for b in s._menu_bars(main) if b.visible()]:
            return "nie widać paska menu"
        return ""

    def _baseline(self) -> None:
        """The program as it was when the sweep began: its main window alone, in front, with
        its menu bar, answering."""
        s = self.s
        if self._alive() and s.probe is not None:
            s.probe.wait_responsive(time.perf_counter(), 10)
        for attempt in range(3):
            if not self._alive():
                break
            if attempt:
                self._close_extra(self.keep)
            problem = self._problem()
            if not problem:
                self._collapse_toolbars()
                windows.bring_forward(s.main_hwnd)
                return
            if problem == "nie widać paska menu":
                windows.bring_forward(s.main_hwnd)
                stopping.sleep(0.1)
                self._esc()  # Distraction-Free and full screen end with Esc
                stopping.sleep(0.6)
        self._relaunch(self._problem())

    def _relaunch(self, why: str) -> None:
        s = self.s
        self.restarts += 1
        if self.restarts > MAX_RESTARTS:
            raise StepFailed(f"program trzeba było uruchomić od nowa {MAX_RESTARTS} razy; ostatnio: {why}")
        if self.items:
            self.items[-1].setdefault("notes", []).append(f"potem program uruchomiony od nowa ({why})")
        if s.process is not None and s.process.poll() is None:
            s.process.kill()
            try:
                s.process.wait(10)
            except Exception:  # noqa: BLE001 - the next start says what is wrong
                pass
        s._stop_watchers()
        s._launch()
        self._remember_start()

    def _collapse_toolbars(self) -> None:
        """A toolbar expanded with its extension button folds back."""
        main = self._main()
        if main is None:
            return
        for entry in self.s.ui.controls(main, kinds=("CheckBox",)):
            if not entry["name"] and _width(entry["rect"]) <= 14 and entry["toggle"] == "on" and _shown(entry):
                self._click(entry["element"], "rozwinięcie paska")
                stopping.sleep(0.3)

    # --- one option ---------------------------------------------------------------------------
    def _click(self, element, name: str, where: dict = None) -> float:
        target = dict(where or {}, name=name)
        return self.s._click_element(target, element)

    def _react(self, t0: float, before: set, foreign_before: set):
        """(what the option did, what went wrong or '', the longest time without an answer)."""
        s = self.s
        if s.probe is not None and not s.probe.wait_responsive(t0, HANG_LIMIT):
            return "", f"program nie odpowiadał przez {HANG_LIMIT:.0f} s", None
        deadline = max(t0 + WINDOW_WAIT, time.perf_counter() + 0.25)
        shown = []
        while self._alive():
            shown = self._extra(before)
            if shown or time.perf_counter() >= deadline:
                break
            stopping.sleep(0.05)
        if shown and self._alive():
            stopping.sleep(0.25)
            if s.probe is not None:
                s.probe.wait_responsive(time.perf_counter(), HANG_LIMIT)
            shown = self._extra(before) or shown
        busy = s.probe.longest_ms(t0, time.perf_counter()) if s.probe is not None else None
        if not self._alive():
            return "", f"program się zamknął (kod {s.process.returncode})", busy
        error = s._error_window()
        if error:
            return "", error[0], busy
        said = []
        for window in shown:
            if window["title"]:
                said.append(f"okno „{window['title']}”")
                self._inventory_window(window)
            else:
                said.append("menu albo okienko bez tytułu")
        for hwnd in self._foreign() - foreign_before:
            title = windows.window_title(hwnd)
            if not title:
                continue
            said.append(f"okno innego programu „{title}”")
            if windows.window_class(hwnd) == "CabinetWClass":  # an Explorer window it opened
                windows.ask_to_close(hwnd)
        return (", ".join(dict.fromkeys(said)) or "bez nowego okna"), "", busy

    def _inventory_window(self, window: dict) -> None:
        """A window an option opened: what is in it goes to the coverage report, once."""
        s = self.s
        if window["title"] in s._dumped:
            return
        s._dumped.add(window["title"])
        s._dump(f"drzewo-okna-{_slug(window['title'])}.json", window["hwnd"])

    def _try(self, label: str, kind: str, press, state=None, restore=None, keep=None) -> dict:
        """One option: press() clicks it and returns the moment; state() reads a switch;
        restore() puts the switches back; keep: the windows that belong to the start."""
        item = {"name": label, "kind": kind}
        keep = self.keep if keep is None else keep
        before = {w["hwnd"] for w in windows.top_level(self.s.pid)}
        foreign_before = self._foreign()
        first = state() if state else None
        try:
            t0 = press()
        except stopping.Stopped:
            raise
        except StepFailed as error:
            if "niedostępn" in str(error):
                item["skipped"] = "niedostępna w tej chwili"
            else:
                item.update(ok=False, detail=f"nie da się jej kliknąć: {error}")
            return self._add(item)
        except Exception as error:  # noqa: BLE001 - one option must not end the whole sweep
            item.update(ok=False, detail=f"automat: {type(error).__name__}: {error}")
            return self._add(item)
        reaction, problem, busy = self._react(t0, before, foreign_before)
        item["busy_ms"] = busy
        if problem:
            item.update(ok=False, detail=problem)
            return self._add(item)
        item["reaction"] = reaction
        self._close_extra(keep | before)
        if state is not None and first is not None:
            now = state()
            if now is None:
                item.setdefault("notes", []).append("po kliknięciu przełącznik zniknął z ekranu")
            elif now == first and first != "on":
                item.update(ok=False, detail=f"przełącznik został w stanie „{first}”")
            elif now != first:
                item["reaction"] = f"przełącznik: {first} → {now}" + (
                    "" if reaction == "bez nowego okna" else f"; {reaction}")
        if restore is not None:
            note = restore()
            if note:
                item.setdefault("notes", []).append(note)
        return self._add(item)

    # --- menus ------------------------------------------------------------------------------
    def _bar_items(self) -> list:
        main = self._main()
        if main is None:
            return []
        found = []
        for bar in self.s._menu_bars(main):
            found += [e for e in self.s.ui.controls(bar, kinds=("MenuItem",)) if _shown(e) and e["name"]]
        return sorted(found, key=lambda e: e["rect"][0])

    def _new_popup(self, known: set, timeout: float = 1.5):
        def probe():
            fresh = self._extra(known)
            return fresh[0] if fresh else None

        found, _ = uia.wait_until(probe, timeout, 0.03)
        return found

    def _read_popup(self, window: dict, path: list, known: set, out: list, depth: int) -> None:
        element = self.s.ui.from_handle(window["hwnd"])
        entries = self.s.ui.controls(element, kinds=("MenuItem",)) if element is not None else []
        entries = sorted([e for e in entries if _shown(e) and e["name"]], key=lambda e: e["rect"][1])
        for entry in entries:
            item_path = path + [entry["name"]]
            out.append({"path": item_path, "enabled": entry["enabled"], "submenu": entry["expandable"],
                        "toggle": entry["toggle"]})
            self.s.see("menu", "MenuItem", " > ".join(item_path), entry["enabled"], title="menu")
            if not (entry["expandable"] and entry["enabled"] and depth < 4):
                continue
            x, y = entry["element"].center
            self.s._guard_point(x, y)
            inp.click(x, y)
            child = self._new_popup(known | {window["hwnd"]})
            if child is None:
                continue
            self._read_popup(child, item_path, known | {window["hwnd"], child["hwnd"]}, out, depth + 1)
            self._esc()  # closes the deepest menu only
            uia.wait_until(lambda hwnd=child["hwnd"]: hwnd not in {w["hwnd"] for w in windows.top_level(self.s.pid)},
                           1.0, 0.03)

    def _bar_opener(self, top: str):
        """Opens the menu of the menu bar's item `top`."""
        def opener():
            tops = [e for e in self._bar_items() if norm(e["name"]) == norm(top)]
            if not tops:
                raise StepFailed(f"w pasku menu nie ma „{top}”")
            return self._click(tops[0]["element"], top, {"role": "menu_item"})
        return opener

    def _context_opener(self, target):
        """Opens the context menu of the target with a click of the right mouse button."""
        def opener():
            element = self.s.find(target)
            if not element.visible():
                raise StepFailed(f"„{element.name}” jest niewidoczny")
            x, y = element.center
            self.s._guard_point(x, y)
            self.s._touch(element, x, y)
            moment = inp.click(x, y, button="right")
            self.s.last_target, self.s.last_element = target, element
            return moment
        return opener

    def _popup_tree(self, opener, root: str) -> list:
        """The items of the menu opener() opens, submenus included."""
        self._baseline()
        before = {w["hwnd"] for w in windows.top_level(self.s.pid)}
        opener()
        popup = self._new_popup(before)
        tree = []
        if popup is not None:
            self._read_popup(popup, [root], before | {popup["hwnd"]}, tree, 1)
        self._close_extra(self.keep)
        return tree

    def _menu_leaf(self, path: list, opener):
        """Open the menus down to the last item of path, without choosing it; its element."""
        s = self.s
        opener()
        element = None
        for depth, name in enumerate(path[1:], 1):
            def in_menus(name=name):
                for window in self._extra(self.keep):
                    popup = s.ui.from_handle(window["hwnd"])
                    if popup is None:
                        continue
                    found = [e for e in s._named(popup, name, ("MenuItem",)) if e.visible()]
                    if found:
                        return found[0]
                return None

            element, _ = uia.wait_until(in_menus, FIND_TIMEOUT, 0.03)
            if element is None:
                raise StepFailed(f"w menu nie ma pozycji „{name}”")
            if depth < len(path) - 1:
                self._click(element, name, {"role": "menu_item"})
        return element

    def _choose(self, path: list, opener) -> float:
        """Choose the menu item at the end of path; the moment of the click."""
        element = self._menu_leaf(path, opener)
        if not element.enabled:
            raise StepFailed(f"pozycja menu „{path[-1]}” jest niedostępna")
        moment = self._click(element, path[-1], {"role": "menu_item"})
        for depth in range(2, len(path) + 1):
            self.s.touched.append({"window": "menu", "type": "MenuItem", "name": " > ".join(path[:depth]),
                                   "sweep": True})
        return moment

    def _menu_state(self, path: list, opener):
        """The check mark of a menu item, read by opening its menu (and closing it again)."""
        try:
            self._baseline()
            element = self._menu_leaf(path, opener)
            return element.toggle_state() if element is not None else None
        except StepFailed:
            return None
        finally:
            self._close_extra(self.keep)

    def _menu(self) -> None:
        for top in [e["name"] for e in self._bar_items()]:
            opener = self._bar_opener(top)
            self._popup_items(self._popup_tree(opener, top), opener)

    def _context(self) -> None:
        target = self.step.get("target")
        if target == "editor":
            label = "menu podręczne edytora"
        elif isinstance(target, dict):
            label = f"menu podręczne „{target.get('name') or target.get('row') or target.get('window')}”"
        else:
            raise StepFailed("przegląd menu podręcznego potrzebuje elementu („target”)")
        opener = self._context_opener(target)
        tree = self._popup_tree(opener, label)
        if not tree:
            raise StepFailed(f"prawy przycisk myszy nie otworzył menu ({label})")
        self._popup_items(tree, opener)

    def _popup_items(self, tree: list, opener) -> None:
        for entry in tree:
            label = " > ".join(entry["path"])
            if entry["submenu"] or len(entry["path"]) < 2:
                continue
            if not self._wanted(label) and not self._wanted(entry["path"][-1]):
                self._add({"name": label, "kind": "MenuItem", "skipped": "pominięta w scenariuszu"})
                continue
            if not entry["enabled"]:
                self._add({"name": label, "kind": "MenuItem", "skipped": "niedostępna w tej chwili"})
                continue
            self._baseline()
            path = entry["path"]

            def press(path=path):
                return self._choose(path, opener)

            item = self._try(label, "MenuItem", press)
            if entry["toggle"] is None or not item.get("ok") or item.get("skipped"):
                continue
            # A menu item with a check mark: its mark changes, and a second choice puts it back
            now = self._menu_state(path, opener)
            if now is not None and now == entry["toggle"] and entry["toggle"] != "on":
                item.update(ok=False, detail=f"znacznik pozycji został w stanie „{now}”")
            elif now is not None and now != entry["toggle"]:
                item["reaction"] = f"znacznik: {entry['toggle']} → {now}"
                self._baseline()
                try:
                    before = {w["hwnd"] for w in windows.top_level(self.s.pid)}
                    self._react(press(), before, self._foreign())
                except StepFailed as error:
                    item.setdefault("notes", []).append(f"nie dało się przełączyć z powrotem: {error}")
                self._close_extra(self.keep)

    # --- toolbars ------------------------------------------------------------------------
    def _bar_members(self, bar):
        members, extension = [], None
        for entry in self.s.ui.controls(bar, kinds=TOOL_KINDS):
            if not _shown(entry):
                continue
            if entry["type"] == "CheckBox" and not entry["name"] and _width(entry["rect"]) <= 14:
                extension = entry
                continue
            members.append(entry)
        return members, extension

    def _toolbar(self, name: str):
        main = self._main()
        if main is None:
            return None
        for entry in self.s.ui.controls(main, kinds=("ToolBar",)):
            if entry["name"] == name and _shown(entry):
                return entry["element"]
        return None

    def _tool(self, bar_name: str, ident, expand: bool = True):
        """The toolbar's element by its identity; a hidden one is shown with the extension."""
        bar = self._toolbar(bar_name)
        if bar is None:
            return None
        members, extension = self._bar_members(bar)
        for found, entry in _identities(members):
            if found == ident:
                return entry
        if expand and extension is not None and extension["toggle"] != "on":
            self._click(extension["element"], "rozwinięcie paska")
            stopping.sleep(0.4)
            return self._tool(bar_name, ident, expand=False)
        return None

    def _toolbars(self) -> None:
        main = self._main()
        bars = [e["name"] for e in self.s.ui.controls(main, kinds=("ToolBar",)) if _shown(e)] if main else []
        for bar_name in bars:
            self._baseline()
            bar = self._toolbar(bar_name)
            if bar is None:
                continue
            members, extension = self._bar_members(bar)
            idents = [ident for ident, _ in _identities(members)]
            if extension is not None:
                self._click(extension["element"], "rozwinięcie paska")
                stopping.sleep(0.4)
                bar = self._toolbar(bar_name)
                if bar is not None:
                    more, _ = self._bar_members(bar)
                    idents += [ident for ident, _ in _identities(more) if ident not in idents]
                self._baseline()
            for ident in idents:
                self._tool_item(bar_name, ident)

    def _switch_states(self, bar_name: str) -> dict:
        bar = self._toolbar(bar_name)
        if bar is None:
            return {}
        members, _ = self._bar_members(bar)
        return {ident: entry["toggle"] for ident, entry in _identities(members) if entry["toggle"] is not None}

    def _tool_item(self, bar_name: str, ident) -> None:
        label = f"pasek „{bar_name}” > {_label(ident)}"
        if not self._wanted(label) and not self._wanted(_label(ident)):
            self._add({"name": label, "kind": ident[1], "skipped": "pominięty w scenariuszu"})
            return
        self._baseline()
        entry = self._tool(bar_name, ident)
        if entry is None:
            self._add({"name": label, "kind": ident[1], "skipped": "nie widać go na pasku"})
            return
        if not entry["enabled"]:
            self._add({"name": label, "kind": ident[1], "skipped": "niedostępny w tej chwili"})
            self._baseline()
            return
        states = self._switch_states(bar_name)
        if entry["toggle"] is not None:
            states.setdefault(ident, entry["toggle"])
        where = {"window": "*Kalahari", "in": bar_name}

        def press():
            found = self._tool(bar_name, ident)
            if found is None:
                raise StepFailed("zniknął z paska")
            if not found["enabled"]:
                raise StepFailed("jest niedostępny")
            return self._click(found["element"], ident[0], where)

        def state():
            found = self._tool(bar_name, ident, expand=True)
            return found["toggle"] if found is not None else None

        def restore():
            return self._restore_tools(bar_name, states)

        is_switch = entry["toggle"] is not None
        self._try(label, ident[1], press, state=state if is_switch else None,
                  restore=restore if states else None)

    def _restore_tools(self, bar_name: str, states: dict) -> str:
        """Click again the switches of the toolbar that differ from before the option: first
        the ones that were on (one of a group brings the others of the group back off), then
        the ones that were off."""
        for wanted in ("on", "off"):
            for ident, was in states.items():
                if was != wanted:
                    continue
                self._baseline()
                entry = self._tool(bar_name, ident)
                if entry is None or entry["toggle"] in (None, was):
                    continue
                self._click(entry["element"], ident[0], {"window": "*Kalahari", "in": bar_name})
                if self.s.probe is not None:
                    self.s.probe.wait_responsive(time.perf_counter(), HANG_LIMIT)
                stopping.sleep(0.2)
                self._close_extra(self.keep)
        self._baseline()
        changed = [_label(i) for i, was in states.items()
                   if (e := self._tool(bar_name, i)) is not None and e["toggle"] not in (None, was)]
        return f"nie wróciły do stanu sprzed kliknięcia: {', '.join(changed)}" if changed else ""

    # --- controls of a window ------------------------------------------------------------
    def _window(self, pattern: str):
        for window in windows.top_level(self.s.pid):
            if self.s.matches(window["title"], pattern):
                return window
        return None

    def _open_window(self, pattern: str, steps: list, inside=None) -> dict:
        """The window (and its part `inside`), opened again with the steps of "open" when an
        option closed it."""
        window = self._window(pattern)
        if window is not None and (not inside or self._root(window, inside) is not None):
            return window
        if not steps:
            what = f"okno „{pattern}”" + (f" z częścią „{inside}”" if inside else "")
            raise StepFailed(f"{what} jest zamknięte, a przegląd nie ma kroków, które je otwierają („open”)")
        if window is None:
            self._baseline()
        for step in steps:
            handler = getattr(self.s, "_do_" + str(step.get("do")), None)
            if handler is None:
                raise StepFailed(f"automat nie umie działania „{step.get('do')}”")
            handler(step)
            if self.s.probe is not None:
                self.s.probe.wait_responsive(time.perf_counter(), HANG_LIMIT)
        def ready():
            window = self._window(pattern)
            return window if window is not None and (not inside or self._root(window, inside) is not None) else None

        found, _ = uia.wait_until(ready, 10.0, 0.1)
        if found is None:
            raise StepFailed(f"okno „{pattern}”{' z częścią „' + inside + '”' if inside else ''} nie otworzyło się")
        stopping.sleep(0.3)
        return found

    def _root(self, window: dict, inside):
        root = self.s.ui.from_handle(window["hwnd"])
        if root is None or not inside:
            return root
        found = [e for e in self.s._named(root, inside, None) if e.visible()]
        found.sort(key=lambda e: e.control_type not in ("Window", "Pane", "Group", "Custom", "Tab"))
        return found[0] if found else None

    def _control_entries(self, root, exclude=()) -> list:
        bars = [e["rect"] for e in self.s.ui.controls(root, kinds=("TitleBar",))]

        def in_title_bar(rect):
            return any(b[1] <= rect[1] and rect[3] <= b[3] and b[0] - 40 <= rect[0] for b in bars)

        entries = []
        for entry in self.s.ui.controls(root, kinds=CONTROL_KINDS):
            if not _shown(entry) or in_title_bar(entry["rect"]):
                continue
            entries.append(entry)
        return [(ident, entry) for ident, entry in _identities(entries) if ident not in exclude]

    def _controls(self) -> None:
        pattern = self.step.get("window")
        if not pattern:
            raise StepFailed("przegląd kontrolek potrzebuje okna („window”)")
        steps = self.step.get("open", [])
        window = self._open_window(pattern, steps, self.step.get("in"))
        root = self._root(window, self.step.get("in"))
        if root is None:
            raise StepFailed(f"w oknie „{window['title']}” nie ma „{self.step.get('in')}”")
        self._see_controls(root, window["title"], self.step.get("in"))
        idents = [ident for ident, _ in self._control_entries(root)]
        idents.sort(key=lambda i: bool(CLOSERS.match(i[0] or "")))
        prefix = window["title"] + (f" > {self.step['in']}" if self.step.get("in") else "")
        for ident in idents:
            self._control_item(pattern, steps, self.step.get("in"), ident, f"{prefix} > {_label(ident)}")

    def _control(self, pattern: str, steps: list, inside, ident, page=None):
        window = self._open_window(pattern, steps, inside)
        if page is not None:
            self._select_page(window, page)
        root = self._root(window, inside)
        if root is None:
            return None, window
        for found, entry in self._control_entries(root):
            if found == ident:
                return entry, window
        return None, window

    def _peek(self, pattern: str, inside, ident, page=None):
        """The control when its window and part are open (never opens them again)."""
        window = self._window(pattern)
        if window is None:
            return None, None
        if page is not None:
            try:
                self._select_page(window, page)
            except StepFailed:
                return None, window
        root = self._root(window, inside)
        if root is None:
            return None, window
        for found, entry in self._control_entries(root):
            if found == ident:
                return entry, window
        return None, window

    def _control_item(self, pattern, steps, inside, ident, label, page=None) -> None:
        if not self._wanted(label) and not self._wanted(_label(ident)):
            self._add({"name": label, "kind": ident[1], "skipped": "pominięta w scenariuszu"})
            return
        entry, window = self._control(pattern, steps, inside, ident, page)
        if entry is None:
            self._add({"name": label, "kind": ident[1], "skipped": "nie widać jej w oknie"})
            return
        if not entry["enabled"]:
            self._add({"name": label, "kind": ident[1], "skipped": "niedostępna w tej chwili"})
            return
        keep = self.keep | {window["hwnd"]}
        root = self._root(window, inside)
        switches = {i: (e["toggle"] if e["toggle"] is not None else e["selected"])
                    for i, e in self._control_entries(root) if i[1] in SWITCH_KINDS} if root else {}

        def press():
            found, _ = self._control(pattern, steps, inside, ident, page)
            if found is None:
                raise StepFailed("zniknęła z okna")
            if not found["enabled"]:
                raise StepFailed("jest niedostępna")
            return self._click(found["element"], ident[0], {"window": pattern})

        def state():
            found, _ = self._peek(pattern, inside, ident, page)
            if found is None:
                return None
            value = found["toggle"] if found["toggle"] is not None else found["selected"]
            return {True: "on", False: "off"}.get(value, value)

        def restore():
            return self._restore_controls(pattern, steps, inside, switches, page)

        is_switch = ident[1] in ("CheckBox",) and entry["toggle"] is not None
        self._try(label, ident[1], press, state=state if is_switch else None,
                  restore=restore if switches else None, keep=keep)

    def _restore_controls(self, pattern, steps, inside, switches: dict, page=None) -> str:
        if self._window(pattern) is None:
            return ""  # the window closed: its next opening starts from the saved settings
        def now_of(ident):
            found, _ = self._peek(pattern, inside, ident, page)
            if found is None:
                return None
            return found["toggle"] if found["toggle"] is not None else found["selected"]

        for ident, was in switches.items():
            if self._window(pattern) is None:
                return ""
            if ident[1] != "CheckBox" and not was:
                continue  # a radio button or tab that is off now comes back when the one that was on does
            found, window = self._peek(pattern, inside, ident, page)
            if found is None or window is None:
                continue
            now = found["toggle"] if found["toggle"] is not None else found["selected"]
            if now is None or now == was:
                continue
            self._click(found["element"], ident[0], {"window": pattern})
            stopping.sleep(0.2)
            self._close_extra(self.keep | {window["hwnd"]})
        if self._window(pattern) is None:
            return ""
        changed = [_label(i) for i, was in switches.items() if now_of(i) not in (None, was)]
        return f"nie wróciły do stanu sprzed kliknięcia: {', '.join(changed)}" if changed else ""

    def _see_controls(self, root, title: str, label=None) -> None:
        """Every element of the window or page, for the coverage report."""
        entries = [e for e in self.s.ui.controls(root) if _shown(e)]
        self.s.remember_entries(entries, title, label)

    # --- pages of a tree (Settings) -----------------------------------------------------
    def _tree_items(self, window: dict) -> list:
        root = self.s.ui.from_handle(window["hwnd"])
        if root is None:
            return []
        trees = [e for e in self.s.ui.controls(root, kinds=("Tree",)) if _shown(e)]
        if not trees:
            return []
        items = [e for e in self.s.ui.controls(trees[0]["element"], kinds=("TreeItem",)) if _shown(e)]
        items.sort(key=lambda e: e["rect"][1])
        out, stack = [], []
        for entry in items:
            while stack and stack[-1][0] >= entry["rect"][0]:
                stack.pop()
            path = [name for _, name in stack] + [entry["name"]]
            stack.append((entry["rect"][0], entry["name"]))
            out.append((tuple(path), entry))
        return out

    def _selected_page(self, window: dict):
        """(path, entry) of the selected page of the tree, or (None, None)."""
        for path, entry in self._tree_items(window):
            if entry["selected"]:
                return path, entry
        return None, None

    def _walk(self, window: dict, arrive) -> None:
        """Go through the tree with the keyboard, as a person does: from the first page down
        with the arrow key, opening each closed branch with the right arrow; arrive(path,
        entry) for each page; it returns True to stop."""
        items = self._tree_items(window)
        if not items:
            raise StepFailed(f"w oknie „{window['title']}” nie ma drzewa stron")
        self._click(items[0][1]["element"], items[0][0][-1], {"window": window["title"]})
        stopping.sleep(0.2)
        self._esc_safe_key("Home")
        stopping.sleep(0.3)
        previous = None
        for _ in range(300):
            path, entry = self._selected_page(window)
            if path is None or path == previous:
                return
            if arrive(path, entry):
                return
            previous = path
            if entry["expandable"] and entry["expanded"] is False:
                self._esc_safe_key("Right")
                stopping.sleep(0.25)
            self._esc_safe_key("Down")
            if self.s.probe is not None:
                self.s.probe.wait_responsive(time.perf_counter(), HANG_LIMIT)
            stopping.sleep(0.3)
            window = self._window(window["title"]) or window

    def _select_page(self, window: dict, page: tuple) -> None:
        path, _ = self._selected_page(window)
        if path == page:
            return
        for path, entry in self._tree_items(window):
            if path == page:
                self._click(entry["element"], page[-1], {"window": window["title"]})
                stopping.sleep(0.3)
                return
        found = []
        self._walk(window, lambda path, entry: found.append(path) or path[-1] == page[-1])
        if not found or found[-1][-1] != page[-1]:
            raise StepFailed(f"w drzewie nie ma strony „{' > '.join(page)}”")

    def _pages(self) -> None:
        pattern = self.step.get("window")
        if not pattern:
            raise StepFailed("przegląd stron potrzebuje okna („window”)")
        steps = self.step.get("open", [])
        window = self._open_window(pattern, steps)
        pages = []

        def arrive(path, entry):
            label = f"{window['title']} > {' > '.join(path)}"
            error = self.s._error_window()
            current = self._window(pattern)
            root = self.s.ui.from_handle(current["hwnd"]) if current is not None else None
            if error or root is None:
                self._add({"name": label, "kind": "TreeItem", "ok": False,
                           "detail": error[0] if error else "okno zniknęło"})
                return True
            self._see_controls(root, current["title"], " > ".join(path))
            pages.append((path, {ident for ident, _ in self._control_entries(root)}))
            self._add({"name": label, "kind": "TreeItem", "reaction": "strona pokazana"})
            return False

        self._walk(window, arrive)
        if not self.step.get("controls") or not pages:
            return
        common = set.intersection(*[own for _, own in pages]) if len(pages) > 1 else set()
        for path, own in pages:
            for ident in sorted(own - common, key=lambda i: (bool(CLOSERS.match(i[0] or "")), i[0], i[2])):
                self._control_item(pattern, steps, None, ident,
                                   f"{window['title']} > {' > '.join(path)} > {_label(ident)}", page=path)
        for ident in sorted(common, key=lambda i: bool(CLOSERS.match(i[0] or ""))):
            self._control_item(pattern, steps, None, ident, f"{window['title']} > {_label(ident)}",
                               page=pages[0][0])

    def _esc_safe_key(self, chord: str) -> None:
        front = windows.foreground()
        if front and windows.owner_pid(front) == self.s.pid:
            inp.press(chord)


def _slug(text) -> str:
    plain = unicodedata.normalize("NFKD", str(text)).encode("ascii", "ignore").decode("ascii")
    return re.sub(r"[^a-z0-9]+", "-", plain.lower())[:40].strip("-") or "okno"
