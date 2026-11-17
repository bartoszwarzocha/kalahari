"""A pretend program, for the automaton's own tests and for working on the page without
Windows. It does nothing on the screen: every action "works" after a short pause and every
check passes, unless a note says otherwise ("fake:fail" or "fake:slow" on a check,
"fake:missing" or "fake:long" (a hundred times longer) on a step, "fake": "start_fails" on
the scenario). Its results are marked as a
simulation, so they are never mistaken for a test of Kalahari.
"""

import time
from pathlib import Path

from automat import stopping
from automat.runner import StepFailed
from automat.scenarios import limit_of

WHERE = "symulacja (bez programu)"

# A 1x1 grey PNG, so the page has a picture to show
_PIXEL = bytes.fromhex(
    "89504e470d0a1a0a0000000d4948445200000001000000010806000000"
    "1f15c4890000000d49444154789c63f8cfc0f01f0005000201a0f1d8e10000000049454e44ae426082")


class FakeSession:
    def __init__(self, config, scenario, folder: Path, delay: float = 0.05):
        self.scenario = scenario
        self.folder = Path(folder)
        self.delay = delay
        self.started = False
        self.stopped = False

    def start(self) -> None:
        self.started = True
        if self.scenario.data.get("fake") == "start_fails":
            raise StepFailed("program nie uruchomił się (symulacja)")

    def act(self, step: dict) -> dict:
        start = time.perf_counter()
        stopping.sleep(self.delay * (100 if step.get("note") == "fake:long" else 1))
        if step.get("note") == "fake:missing":
            raise StepFailed(f"nie ma elementu {step.get('target')} (symulacja)")
        done = {"start": start, "t0": time.perf_counter(), "element": None, "note": "symulacja"}
        if step.get("do") == "sweep":
            done["items"] = [
                {"name": "Plik > Nowa książka...", "kind": "MenuItem", "ok": True, "reaction": "okno „Nowa książka” (symulacja)"},
                {"name": "Plik > Zakończ", "kind": "MenuItem", "ok": True, "skipped": "pominięta w scenariuszu"},
                {"name": "Wstaw > Znak specjalny...", "kind": "MenuItem",
                 "ok": step.get("note") != "fake:fail", "detail": "symulacja błędu"}]
        return done

    def observe(self, checks: list, action: dict) -> list:
        out = []
        for check in checks:
            limit = limit_of(check)
            note = check.get("note", "")
            ms = float((limit or 300) + 150 if note == "fake:slow" else min(40, limit or 40))
            fails = note == "fake:fail"
            out.append({"ok": not fails and (limit is None or ms <= limit), "ms": ms,
                        "detail": "symulacja"})
        return out

    def health(self, since: float) -> dict:
        return {"ok": True, "detail": "", "busy_ms": 12.0}

    def screenshot(self, path: Path, area=None) -> bool:
        Path(path).write_bytes(_PIXEL)
        return True

    def stop(self) -> dict:
        self.stopped = True
        (self.folder / "kalahari.log").write_text("symulacja\n", encoding="utf-8")
        return {"log": "kalahari.log"}


def session_factory(config, scenario, folder):
    return FakeSession(config, scenario, folder)
