"""Probe of a Windows machine for the test automaton.

Drives probe/qt_probe_app.py (a Qt Widgets program) with the automaton's own Windows layer
and answers: is there a desktop that takes real input; do mouse, keyboard (also Polish
letters), UI Automation of Qt, window events, the busy probe, screenshots and the
clipboard work; and how far the moments measured from outside are from the moments the
program itself knows.

    python tests/automat/probe/probe.py --out probe-out
"""

import argparse
import ctypes
import json
import os
import subprocess
import sys
import threading
import time
import traceback
from ctypes import wintypes
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

from automat.win import make_dpi_aware  # noqa: E402

DPI_AWARE = make_dpi_aware()

from automat.win import busy, clipboard, events, input as inp, screen, system, uia  # noqa: E402

PL_TEXT = "Zażółć gęślą jaźń – „cudzysłów” i… ŹĆŃĄŚŁÓĘŻ"


def ctypes_rect(hwnd) -> tuple:
    rect = wintypes.RECT()
    events.user32.GetWindowRect(hwnd, ctypes.byref(rect))
    return rect.left, rect.top, rect.right, rect.bottom


class Probe:
    def __init__(self, out: Path):
        self.out = out
        self.report = {"env": {}, "checks": [], "timings": {}, "app_events": [], "errors": []}
        self.offset = time.perf_counter() - system.qpc()  # app moments -> our clock
        self.app_lines = []
        self.proc = None

    # --- reporting ---------------------------------------------------------------------
    def check(self, name: str, ok: bool, detail="") -> bool:
        self.report["checks"].append({"name": name, "ok": bool(ok), "detail": detail})
        print(f"[{'OK ' if ok else 'BAD'}] {name}: {detail}", flush=True)
        return ok

    def timing(self, name: str, value) -> None:
        self.report["timings"][name] = value
        print(f"      {name}: {value}", flush=True)

    def app_moment(self, event: str, after: float):
        """Our-clock moment of the first program event `event` noted after `after`."""
        for line in list(self.app_lines):
            moment = line["qpc"] + self.offset
            if line["event"] == event and moment >= after:
                return moment
        return None

    def wait_app(self, event: str, after: float, timeout: float = 5.0):
        deadline = time.perf_counter() + timeout
        while time.perf_counter() < deadline:
            moment = self.app_moment(event, after)
            if moment is not None:
                return moment
            time.sleep(0.01)
        return None

    def shot(self, name: str, scr, rect=None) -> None:
        scr.grab(rect).save(self.out / f"{name}.png")

    # --- the program -------------------------------------------------------------------
    def launch(self):
        env = dict(os.environ, PYTHONIOENCODING="utf-8")
        self.proc = subprocess.Popen([sys.executable, str(HERE / "qt_probe_app.py")],
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                     text=True, encoding="utf-8", env=env)

        def read(stream, sink):
            for line in stream:
                line = line.strip()
                if not line:
                    continue
                try:
                    sink.append(json.loads(line))
                except json.JSONDecodeError:
                    self.report["errors"].append(line)

        threading.Thread(target=read, args=(self.proc.stdout, self.app_lines), daemon=True).start()
        threading.Thread(target=read, args=(self.proc.stderr, self.report["errors"]), daemon=True).start()
        return self.proc

    # --- the probe ---------------------------------------------------------------------
    def run(self) -> int:
        self.out.mkdir(parents=True, exist_ok=True)
        env = system.session_info()
        env["dpi_aware"] = DPI_AWARE
        self.report["env"] = env
        print(json.dumps(env, ensure_ascii=False, indent=1), flush=True)
        self.check("pulpit przyjmuje wejście", env["input_desktop"] is not None,
                   f"pulpit wejścia: {env['input_desktop']}, ekran {env['screen']}, DPI {env['dpi']}")
        scr = screen.Screen()
        self.shot("00-pulpit", scr)

        launched = time.perf_counter()
        proc = self.launch()
        hook = events.WindowEvents(proc.pid).start()
        try:
            self._steps(scr, hook, launched)
        except Exception:  # noqa: BLE001 - every failure goes to the report
            self.report["errors"].append(traceback.format_exc())
            print(traceback.format_exc(), flush=True)
            self.check("przebieg sondy bez wyjątku", False, "wyjątek – szczegóły w raporcie")
            try:
                self.shot("99-blad", scr)
            except Exception:  # noqa: BLE001
                pass
        finally:
            hook_events = [e.as_dict(launched) for e in hook.events()]
            hook.stop()
            proc.terminate()
            try:
                proc.wait(5)
            except subprocess.TimeoutExpired:
                proc.kill()
            self.report["window_events"] = hook_events
            self.report["app_events"] = [dict(line, t_ms=round((line["qpc"] + self.offset - launched) * 1000, 1))
                                         for line in self.app_lines]
            print("ZDARZENIA OKIEN:", json.dumps(hook_events, ensure_ascii=False), flush=True)
            print("ZDARZENIA PROGRAMU:", json.dumps(self.report["app_events"], ensure_ascii=False), flush=True)
            print("BŁĘDY:", json.dumps(self.report["errors"][:20], ensure_ascii=False), flush=True)
        (self.out / "raport.json").write_text(json.dumps(self.report, ensure_ascii=False, indent=1),
                                              encoding="utf-8")
        self.summary()
        return 0 if all(c["ok"] for c in self.report["checks"]) else 1

    def _steps(self, scr, hook, launched):
        main_event = hook.wait_shown("Sonda automatu", launched, 30)
        if not self.check("okno programu pokazane (zdarzenie systemowe)", main_event is not None,
                          "" if main_event is None else f"{(main_event.time - launched) * 1000:.0f} ms od uruchomienia"):
            return
        hwnd = main_event.hwnd
        probe = busy.BusyProbe(hwnd).start()
        try:
            self.wait_app("ready", 0, 10)
            time.sleep(0.5)
            self.opening_times(scr, hook, hwnd, "bez UI Automation", 3)
            ui = uia.Uia()
            self._with_window(ui, scr, hook, probe, hwnd)
            self.opening_times(scr, hook, hwnd, "z UI Automation", 5)
        finally:
            probe.stop()

    def opening_times(self, scr, hook, hwnd, label, count):
        """Open the dialog with its shortcut `count` times and compare the moments seen from
        outside (window event, screenshots) with the program's own (key, show, first paint)."""
        rect = ctypes_rect(hwnd)
        inp.click((rect[0] + rect[2]) // 2, rect[1] + 12)  # the title bar: the window comes to the front
        time.sleep(0.3)
        rows = []
        for _ in range(count):
            t0 = inp.press("Ctrl+Shift+I")
            shown = hook.wait_shown("Znak specjalny", t0 - 0.05, 8)
            if shown is None:
                rows.append({"błąd": "okno się nie pokazało"})
                break
            watch = scr.watch(ctypes_rect(shown.hwnd), t0, settle_ms=150, timeout=3)
            key = self.wait_app("key", t0 - 0.05, 2)
            paint = self.wait_app("dialog_first_paint", t0 - 0.05, 2)
            show_event = self.app_moment("dialog_show_event", t0 - 0.05)
            row = {"okno pokazane (zdarzenie)": round((shown.time - t0) * 1000, 1),
                   "gotowe na ekranie (zrzuty)": watch["last_change_ms"],
                   "klawisz w programie": None if key is None else round((key - t0) * 1000, 1),
                   "showEvent w programie": None if show_event is None else round((show_event - t0) * 1000, 1),
                   "pierwsze malowanie w programie": None if paint is None else round((paint - t0) * 1000, 1)}
            rows.append(row)
            time.sleep(0.2)
            t1 = inp.press("Esc")
            hook.wait_closed("Znak specjalny", t1 - 0.05, 3)
            time.sleep(0.3)
        self.timing(f"otwieranie skrótem {label} (ms)", rows)
        self.check(f"skrót Ctrl+Shift+I otwiera okno {count} razy ({label})",
                   len(rows) == count and all("błąd" not in r for r in rows))

    def _with_window(self, ui, scr, hook, probe, hwnd):
        main = ui.from_handle(hwnd)
        tree = ui.dump(main, depth=8)
        (self.out / "drzewo-okna-glownego.json").write_text(json.dumps(tree, ensure_ascii=False, indent=1),
                                                            encoding="utf-8")
        self.check("UI Automation widzi okno Qt", main is not None and main.name == "Sonda automatu",
                   f"{main!r}")
        self.shot("01-okno", scr)

        # Mouse: the cursor really moves.
        cx, cy = main.center
        inp.move_to(cx, cy)
        self.check("kursor myszy się porusza", inp.cursor_position() == (cx, cy),
                   f"cel {cx},{cy}, jest {inp.cursor_position()}")

        # Menu with the mouse.
        menu_items = main.find_all(name="Wstaw", kinds=("MenuItem",))
        if not self.check("pozycja menu „Wstaw” znaleziona", bool(menu_items), f"{menu_items}"):
            return
        inp.click(*menu_items[0].center)
        item, _ = uia.wait_until(lambda: self._find_anywhere(ui, "Znak specjalny...", ("MenuItem",)), 3)
        popup = [w.describe() for w in ui.top_windows(self.proc.pid)]
        (self.out / "okna-po-menu.json").write_text(json.dumps(popup, ensure_ascii=False, indent=1), encoding="utf-8")
        if not self.check("menu rozwinięte myszą, pozycja „Znak specjalny...” widoczna", item is not None,
                          f"okna procesu: {[(w['type'], w['name']) for w in popup]}"):
            self.shot("02-menu-blad", scr)
            return
        self.shot("02-menu", scr)
        t0 = inp.click(*item.center)
        shown = hook.wait_shown("Znak specjalny", t0 - 0.05, 8)
        if not self.check("okno „Znak specjalny” otwarte z menu", shown is not None):
            return
        dialog = ui.from_handle(shown.hwnd)
        watch = scr.watch(dialog.rect, t0, settle_ms=150, timeout=3)
        app_show = self.app_moment("dialog_show_event", t0)
        app_paint = self.app_moment("dialog_first_paint", t0)
        app_action = self.app_moment("menu_action", t0)
        self.timing("menu: od puszczenia przycisku do akcji w programie (ms)",
                    None if app_action is None else round((app_action - t0) * 1000, 1))
        self.timing("okno: zdarzenie systemowe – program (showEvent) (ms)",
                    None if app_show is None else round((shown.time - app_show) * 1000, 1))
        self.timing("okno: pokazane wg zdarzenia od T0 (ms)", round((shown.time - t0) * 1000, 1))
        self.timing("okno: gotowe na ekranie wg zrzutów od T0 (ms)", watch["last_change_ms"])
        self.timing("okno: pierwsze malowanie wg programu od T0 (ms)",
                    None if app_paint is None else round((app_paint - t0) * 1000, 1))
        self.timing("zrzut regionu okna: średni czas klatki (ms)", watch["frame_ms"])
        self.shot("03-okno-znak", scr)

        tree = ui.dump(dialog, depth=8)
        (self.out / "drzewo-okna-znak.json").write_text(json.dumps(tree, ensure_ascii=False, indent=1),
                                                        encoding="utf-8")

        # A check button.
        lista = dialog.find_all(name="Lista", kinds=uia.ROLES["button"])
        if self.check("przycisk „Lista” znaleziony", bool(lista), f"{lista}"):
            before = lista[0].toggle_state()
            inp.click(*lista[0].center)
            state, _ = uia.wait_until(lambda: (lista[0].toggle_state() == "on" or lista[0].is_selected()) and "on", 1)
            self.check("klik myszą zaznacza „Lista” (wzorce Toggle albo SelectionItem)", state == "on",
                       f"{lista[0].control_type}; przed: {before}/{lista[0].is_selected()}, po: {lista[0].toggle_state()}")

        # Typing into the search field.
        search = dialog.find_all(name="Szukaj znaku", kinds=uia.ROLES["edit"])
        if self.check("pole „Szukaj znaku” znalezione (nazwa dostępności)", bool(search), f"{search}"):
            inp.click(*search[0].center)
            inp.type_text("pauza")
            value, _ = uia.wait_until(lambda: search[0].value() == "pauza" and "pauza", 1)
            self.check("pisanie w polu wyszukiwania", value == "pauza", f"wartość: {search[0].value()!r}")

        # Table cell by row and column names.
        tables = dialog.find_all(name="Znaki", kinds=uia.ROLES["list"])
        if self.check("tabela „Znaki” znaleziona", bool(tables), f"{tables}"):
            table = tables[0]
            headers = [h.name for h in table.column_headers()]
            pauza = table.find_all(name="Pauza", kinds=uia.ROLES["cell"])
            ok = "★" in headers and bool(pauza) and pauza[0].grid_position() is not None
            self.check("komórka wg wiersza „Pauza” i kolumny „★” (wzorce Table i Grid)", ok,
                       f"nagłówki: {headers}, komórki „Pauza”: {pauza}, pozycja: {pauza[0].grid_position() if pauza else None}")
            if ok:
                row = pauza[0].grid_position()[0]
                star = table.cell(row, headers.index("★"))
                t0 = inp.click(*star.center)
                done, moment = uia.wait_until(lambda: star.name == "★" and "★", 5)
                probe.wait_responsive(t0 + 0.01, 5)
                app_clicked = self.app_moment("star_clicked", t0)
                app_set = self.app_moment("star_set", t0)
                longest = probe.longest_ms(t0, time.perf_counter())
                self.check("gwiazdka zaznaczona (nazwa komórki „★”)", done == "★", f"nazwa: {star.name!r}")
                self.timing("gwiazdka: widoczna przez UI Automation od T0 (ms)",
                            None if moment is None else round((moment - t0) * 1000, 1))
                self.timing("gwiazdka: program zajęty – sonda WM_NULL (ms)", longest)
                self.timing("gwiazdka: program zajęty – według programu (ms)",
                            None if not (app_clicked and app_set) else round((app_set - app_clicked) * 1000, 1))
                self.check("sonda zajętości widzi 0,8 s pracy programu (±100 ms)", abs(longest - 800) <= 100,
                           f"{longest} ms")
                self.shot("04-gwiazdka", scr)

        # Esc closes the dialog.
        self.where_input_goes(ui, "przed Esc")
        t0 = inp.press("Esc")
        closed = hook.wait_closed("Znak specjalny", t0 - 0.05, 3)
        self.check("Esc zamyka okno (zdarzenie systemowe)", closed is not None,
                   ("" if closed is None else f"{(closed.time - t0) * 1000:.0f} ms; ")
                   + f"okno widoczne: {bool(events.user32.IsWindowVisible(shown.hwnd))}")
        if closed is None:
            self.where_input_goes(ui, "po Esc")
            self.shot("04b-po-esc", scr)
            self.close_dialog(ui, scr, hook, shown.hwnd, "Znak specjalny")

        # A slow window: the moment it is shown and the busy time before.
        slow = main.find_all(name="Wolne okno", kinds=uia.ROLES["button"])
        if self.check("przycisk „Wolne okno” znaleziony", bool(slow)):
            t0 = inp.click(*slow[0].center)
            shown = hook.wait_shown("Wolne okno", t0 - 0.05, 5)
            probe.wait_responsive(t0 + 0.01, 5)
            self.check("wolne okno pokazane po ok. 0,4 s (0,35–0,8 s)",
                       shown is not None and 350 <= (shown.time - t0) * 1000 <= 800,
                       "" if shown is None else f"{(shown.time - t0) * 1000:.0f} ms; zajęty {probe.longest_ms(t0, time.perf_counter())} ms")
            if shown:
                t1 = inp.press("Esc")
                hook.wait_closed("Wolne okno", t1 - 0.05, 3)

        # Polish text in the editor, read back by UI Automation and through the clipboard.
        editor = main.find_all(name="Tekst", kinds=uia.ROLES["edit"])
        if self.check("pole tekstu znalezione", bool(editor)):
            left, top, right, bottom = editor[0].rect
            inp.click(left + 30, bottom - 30)
            inp.type_text(PL_TEXT, interval=0.01)
            time.sleep(0.3)
            text = editor[0].value()
            if text is None:
                text = editor[0].text()
            self.check("pisanie polskich znaków (VK_PACKET), odczyt przez UI Automation",
                       text == PL_TEXT, f"odczyt: {text!r}")
            before = clipboard.sequence()
            inp.press("Ctrl+A")
            inp.press("Ctrl+C")
            uia.wait_until(lambda: clipboard.sequence() != before, 2)
            copied = clipboard.text()
            self.check("schowek: Ctrl+C w programie, odczyt tekstu", copied == PL_TEXT, f"{copied!r}")
            self.timing("schowek: HTML Format dostępny", clipboard.html() is not None)
            self.shot("05-tekst", scr)

    def where_input_goes(self, ui, label: str) -> None:
        foreground = events.user32.GetForegroundWindow()
        focused = ui.focused()
        info = {"okno na wierzchu": events.window_title(foreground) if foreground else None,
                "fokus": None if focused is None else focused.describe()}
        self.report.setdefault("input", {})[label] = info
        print(f"      wejście {label}: {info}", flush=True)

    def close_dialog(self, ui, scr, hook, hwnd, title) -> None:
        """Close a window with the mouse: the close button of its title bar."""
        window = ui.from_handle(hwnd)
        if window is None:
            return
        buttons = [b for b in window.find_all(kinds=("Button",)) if b.automation_id == "Close"
                   or b.name in ("Zamknij", "Close")]
        if buttons:
            t0 = inp.click(*buttons[0].center)
            closed = hook.wait_closed(title, t0, 3)
            self.check(f"okno „{title}” zamknięte przyciskiem na pasku tytułu", closed is not None,
                       f"przyciski: {buttons}")
        else:
            self.check(f"przycisk zamknięcia okna „{title}”", False,
                       f"{[b.describe() for b in window.find_all(kinds=('Button',))]}")

    def _find_anywhere(self, ui, name, kinds):
        for window in ui.top_windows(self.proc.pid):
            found = window.find_all(name=name, kinds=kinds)
            visible = [f for f in found if f.visible()]
            if visible:
                return visible[0]
        return None

    def summary(self) -> None:
        lines = ["## Sonda automatu testów", "", "| Sprawdzenie | Wynik | Szczegóły |", "| --- | --- | --- |"]
        for check in self.report["checks"]:
            detail = str(check["detail"]).replace("|", "\\|")
            lines.append(f"| {check['name']} | {'tak' if check['ok'] else '**NIE**'} | {detail} |")
        lines += ["", "| Pomiar | Wartość |", "| --- | --- |"]
        for name, value in self.report["timings"].items():
            lines.append(f"| {name} | {value} |")
        lines += ["", "```", json.dumps(self.report["env"], ensure_ascii=False, indent=1), "```"]
        text = "\n".join(lines) + "\n"
        (self.out / "podsumowanie.md").write_text(text, encoding="utf-8")
        target = os.environ.get("GITHUB_STEP_SUMMARY")
        if target:
            with open(target, "a", encoding="utf-8") as handle:
                handle.write(text)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default="probe-out")
    args = parser.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    return Probe(Path(args.out)).run()


if __name__ == "__main__":
    sys.exit(main())
