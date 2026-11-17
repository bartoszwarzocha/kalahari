"""Running scenarios one after another, each from scratch, and writing the results.

The runner knows nothing about Windows: a Session (automat.win.kalahari on Windows, a fake
one in the automaton's own tests) starts the program, performs the actions and observes
their effects. The runner orders the work, decides pass or fail, takes the screenshots,
writes the run file and tells listeners (the page) what is happening.
"""

import platform
import threading
import time
import traceback
from datetime import datetime
from pathlib import Path

from automat import results as results_mod
from automat.scenarios import action_sentence, check_sentence, limit_of


class StepFailed(Exception):
    """An action could not be performed (no such element, input refused): the scenario stops."""


class Paused(Exception):
    """Someone used the mouse: the run waits until the page says to go on, then repeats the step."""


class Session:
    """What a runner needs from the program under test (see automat.win.kalahari)."""

    def start(self) -> None: ...
    def act(self, step: dict) -> dict: ...  # {"start": float, "t0": float, "element": dict}
    def observe(self, checks: list, action: dict) -> list: ...  # one dict per check
    def health(self, since: float) -> dict: ...  # {"ok": bool, "detail": str, "busy_ms": float}
    def screenshot(self, path: Path, area=None) -> bool: ...
    def stop(self) -> dict: ...  # {"log": str}


class Runner:
    """guard_factory(folder) gives an object with before() and after() -> [sentences]: what
    the run must leave as it was on this computer (automat.win.guard.FolderGuard)."""

    def __init__(self, config, library, approvals, store, session_factory, where: str = None,
                 guard_factory=None):
        self.config = config
        self.library = library
        self.approvals = approvals
        self.store = store
        self.session_factory = session_factory
        self.guard_factory = guard_factory
        self.where = where or f"ten komputer ({platform.node()})"
        self._listeners = []
        self._lock = threading.Lock()
        self._thread = None
        self._stop = threading.Event()
        self._go_on = threading.Event()  # "Dalej" (step by step) and "Wznów" (after a pause)
        self.waiting = None  # None, "next" or "paused"
        self.current = None  # the run in progress, as written to przebieg.json

    # --- listeners (the page's live view) ----------------------------------------------
    def listen(self, callback) -> None:
        with self._lock:
            self._listeners.append(callback)

    def unlisten(self, callback) -> None:
        with self._lock:
            if callback in self._listeners:
                self._listeners.remove(callback)

    def _emit(self, kind: str, **data) -> None:
        event = {"type": kind, "time": datetime.now().strftime("%H:%M:%S.%f")[:-3], **data}
        with self._lock:
            listeners = list(self._listeners)
        for callback in listeners:
            try:
                callback(event)
            except Exception:  # noqa: BLE001 - a broken page must not stop a run
                pass

    # --- control -----------------------------------------------------------------------
    @property
    def running(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    def start(self, ids: list, options: dict = None) -> str:
        """Start a run of the scenarios `ids` in the background; returns the run id."""
        if self.running:
            raise RuntimeError("przebieg już trwa")
        run = self._new_run(ids, options or {})
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, args=(run,), name="automat-run", daemon=True)
        self._thread.start()
        return run["id"]

    def run_now(self, ids: list, options: dict = None) -> dict:
        """Run in this thread (the command line and CI); returns the finished run."""
        run = self._new_run(ids, options or {})
        self._stop.clear()
        self._run(run)
        return run

    def stop(self) -> None:
        self._stop.set()
        self._go_on.set()

    def go_on(self) -> None:
        """The next step (step by step) or the end of a pause."""
        self._go_on.set()

    def _wait(self, why: str, **data) -> bool:
        """Wait for go_on() or stop(); False when the run was stopped."""
        self.waiting = why
        self._emit("waiting", why=why, **data)
        self._go_on.wait()
        self._go_on.clear()
        self.waiting = None
        self._emit("going_on", why=why)
        return not self._stop.is_set()

    def wait(self, timeout=None) -> None:
        if self._thread:
            self._thread.join(timeout)

    # --- the run -----------------------------------------------------------------------
    def _new_run(self, ids: list, options: dict) -> dict:
        program = Path(self.config.program)
        info = {"path": str(program)}
        if program.exists():
            info["modified"] = datetime.fromtimestamp(program.stat().st_mtime).isoformat(timespec="seconds")
        run = {
            "id": results_mod.new_run_id(), "started": datetime.now().isoformat(timespec="seconds"),
            "finished": None, "where": self.where, "state": "running", "program": info,
            "options": {"repeat": max(1, int(options.get("repeat", 1))),
                        "stop_on_fail": bool(options.get("stop_on_fail", False)),
                        "step_by_step": bool(options.get("step_by_step", False)),
                        "screenshots": options.get("screenshots", self.config.screenshots)},
            "limits": dict(self.config.limits),
            "scenarios": [{"id": i, "state": "waiting"} for i in ids],
        }
        while self.store.run_dir(run["id"]).exists():  # two runs in one second
            time.sleep(1)
            run["id"] = results_mod.new_run_id()
        return run

    def _run(self, run: dict) -> None:
        self.current = run
        self._go_on.clear()
        began = time.perf_counter()
        self.store.save(run)
        self._emit("run_started", run=results_mod.summary(run), ids=[s["id"] for s in run["scenarios"]])
        guard = None
        try:
            if self.guard_factory is not None:
                guard = self.guard_factory(self.store.run_dir(run["id"]) / "_twoje-dane")
                self._emit("log", text="Kopia Twoich folderów programu przed przebiegiem")
                guard.before()
            for index, slot in enumerate(run["scenarios"]):
                if self._stop.is_set():
                    slot["state"] = "stopped"
                    continue
                result = self._scenario(run, index, slot["id"])
                run["scenarios"][index] = result
                self.store.save(run)
                self._emit("scenario_finished", index=index, result=result)
                if result["state"] in ("fail", "error") and run["options"]["stop_on_fail"]:
                    self._stop.set()
        except Exception as error:  # noqa: BLE001 - the run file says what broke
            run["error"] = f"{type(error).__name__}: {error}"
            run["trace"] = traceback.format_exc()
            self._emit("log", text=f"BŁĄD AUTOMATU: {error}", level="error")
        finally:
            if guard is not None:
                try:
                    run["your_data"] = guard.after()
                except Exception as error:  # noqa: BLE001
                    run["your_data"] = [f"NIE UDAŁO SIĘ PRZYWRÓCIĆ TWOICH FOLDERÓW: {error}; kopia jest w "
                                        f"{self.store.run_dir(run['id']) / '_twoje-dane'}"]
                for line in run["your_data"]:
                    self._emit("log", text=f"Twoje dane: {line}", level="warn")
                if not run["your_data"]:
                    self._emit("log", text="Twoje dane: bez zmian")
            run["state"] = "stopped" if self._stop.is_set() else "done"
            run["finished"] = datetime.now().isoformat(timespec="seconds")
            run["duration_ms"] = round((time.perf_counter() - began) * 1000)
            self.store.save(run)
            self._emit("run_finished", run=results_mod.summary(run))
            self.current = None

    def _scenario(self, run: dict, index: int, scenario_id: str) -> dict:
        scenario = self.library.get(scenario_id)
        if scenario is None:
            return {"id": scenario_id, "state": "error", "reason": "nie ma takiego scenariusza w folderze"}
        approval = self.approvals.status(scenario.id, scenario.expectations_hash() if scenario.valid else None)
        result = {"id": scenario.id, "title": scenario.title, "area": scenario.area,
                  "file": scenario.summary()["file"], "approval": approval["state"],
                  "hash": scenario.expectations_hash() if scenario.valid else None,
                  "sources": scenario.data.get("sources", []), "state": "running", "steps": []}
        self._emit("scenario_started", index=index, id=scenario.id, title=scenario.title,
                   steps=[{"say": action_sentence(s), "expect": [check_sentence(c) for c in s.get("expect", [])],
                           "limits": [limit_of(c, run["limits"]) for c in s.get("expect", [])]}
                          for s in scenario.steps])
        if not scenario.valid:
            result.update(state="error", reason="błędy w pliku scenariusza: " + "; ".join(map(str, scenario.problems)))
            return result
        missing = scenario.unsupported()
        if missing:
            result.update(state="skipped", reason="automat jeszcze tego nie umie: " + ", ".join(missing))
            return result
        attempts = []
        began = time.perf_counter()
        for attempt in range(run["options"]["repeat"]):
            if self._stop.is_set():
                break
            attempts.append(self._attempt(run, scenario, attempt))
            if attempts[-1]["state"] == "error":
                break
        result["duration_ms"] = round((time.perf_counter() - began) * 1000)
        if not attempts:
            result["state"] = "stopped"
            return result
        result["steps"] = merge_attempts(attempts)
        states = [a["state"] for a in attempts]
        result["state"] = "error" if "error" in states else "fail" if "fail" in states else (
            "stopped" if "stopped" in states else "ok")
        result["attempts"] = len(attempts)
        for key in ("reason", "log", "trace"):
            for a in attempts:
                if a.get(key):
                    result[key] = a[key]
                    break
        warnings = [w for a in attempts for w in a.get("warnings", [])]
        if warnings:
            result["warnings"] = list(dict.fromkeys(warnings))
        return result

    def _attempt(self, run: dict, scenario, attempt: int) -> dict:
        folder = self.store.run_dir(run["id"]) / scenario.id
        if run["options"]["repeat"] > 1:
            folder = folder / f"proba-{attempt + 1}"
        folder.mkdir(parents=True, exist_ok=True)
        outcome = {"state": "ok", "steps": []}
        session = None
        try:
            session = self.session_factory(self.config, scenario, folder)
            self._emit("log", text=f"„{scenario.title}”: przygotowanie (kopia książek, osobny folder ustawień) i uruchomienie programu")
            session.start()
            previous_shot = None
            for number, step in enumerate(scenario.steps, 1):
                if run["options"]["step_by_step"] and not self._stop.is_set():
                    self._wait("next", number=number, say=action_sentence(step))
                if self._stop.is_set():
                    outcome["state"] = "stopped"
                    break
                step_result, previous_shot = self._step(run, scenario, session, folder, number, step, previous_shot)
                if step_result is None:  # stopped during a pause
                    outcome["state"] = "stopped"
                    break
                outcome["steps"].append(step_result)
                if not step_result["ok"]:
                    outcome["state"] = "fail"
                    if step_result.get("stop"):
                        outcome["reason"] = f"krok {number}: {step_result.get('reason')}"
                        break
        except StepFailed as error:
            outcome.update(state="error", reason=str(error))
        except Exception as error:  # noqa: BLE001 - every problem goes to the result
            outcome.update(state="error", reason=f"{type(error).__name__}: {error}",
                           trace=traceback.format_exc())
            self._emit("log", text=f"BŁĄD AUTOMATU: {error}", level="error")
        finally:
            if session is not None:
                try:
                    ended = session.stop() or {}
                    base = self.store.run_dir(run["id"])
                    if ended.get("log"):
                        outcome["log"] = (folder / ended["log"]).relative_to(base).as_posix()
                    if ended.get("warnings"):
                        outcome["warnings"] = list(ended["warnings"])
                        for warning in ended["warnings"]:
                            self._emit("log", text=f"UWAGA: {warning}", level="warn")
                except Exception as error:  # noqa: BLE001
                    outcome.setdefault("reason", f"zamykanie programu: {error}")
        return outcome

    def _relative(self, run: dict, path: Path) -> str:
        return path.relative_to(self.store.run_dir(run["id"])).as_posix()

    def _step(self, run, scenario, session, folder: Path, number: int, step: dict, previous_shot):
        say = action_sentence(step)
        self._emit("step_started", number=number, say=say)
        record = {"index": number, "say": say, "do": step.get("do"), "ok": True, "checks": []}
        while True:
            try:
                action = session.act(step)
                break
            except Paused as pause:
                self._emit("log", text=f"Przebieg wstrzymany: {pause}. Wznów go na stronie.", level="warn")
                if not self._wait("paused", number=number, reason=str(pause)):
                    return None, previous_shot
                self._emit("log", text=f"Krok {number} od nowa")
            except StepFailed as error:
                record.update(ok=False, stop=True, reason=str(error), action={"ok": False, "detail": str(error)})
                self._emit("log", text=f"Krok {number}: nie da się wykonać – {error}", level="error")
                shot = folder / f"{number:02d}-po.png"
                if session.screenshot(shot):
                    record["screens"] = {"before": previous_shot, "after": self._relative(run, shot)}
                self._emit("step_finished", number=number, step=record)
                return record, previous_shot
        record["action"] = {"ok": True, "element": action.get("element"), "note": action.get("note")}
        self._emit("log", text=f"{say}{(' – ' + action['note']) if action.get('note') else ''}")
        checks = session.observe(step.get("expect", []), action) if step.get("expect") else []
        for check, outcome in zip(step.get("expect", []), checks):
            outcome.setdefault("say", check_sentence(check))
            outcome.setdefault("check", check.get("check"))
            outcome.setdefault("limit_ms", limit_of(check, run["limits"]))
            record["checks"].append(outcome)
            level = "" if outcome["ok"] else "error"
            timing = f" – {fmt(outcome.get('ms'))}" if outcome.get("ms") is not None else ""
            limit = f" (limit {fmt(outcome['limit_ms'])})" if outcome.get("limit_ms") else ""
            self._emit("log", text=f"{'OK' if outcome['ok'] else 'BŁĄD'}: {outcome['say']}{timing}{limit}"
                                   f"{'' if outcome['ok'] else ' – ' + str(outcome.get('detail', ''))}", level=level)
        health = session.health(action["start"])
        record["busy_ms"] = health.get("busy_ms")
        if not health.get("ok", True):
            record["checks"].append({"check": "health", "say": "program działa i odpowiada", "ok": False,
                                     "detail": health.get("detail")})
            record["stop"] = True
            record["reason"] = health.get("detail")
        record["ok"] = all(c["ok"] for c in record["checks"])
        shots = run["options"]["screenshots"]
        if shots == "each_step" or not record["ok"]:
            shot = folder / f"{number:02d}-po.png"
            if session.screenshot(shot):
                record["screens"] = {"before": previous_shot, "after": self._relative(run, shot)}
                previous_shot = record["screens"]["after"]
        self._emit("step_finished", number=number, step=record)
        return record, previous_shot


def merge_attempts(attempts: list) -> list:
    """With repeats: the first attempt's steps, each check with the median and the worst time
    of all attempts and failing when any attempt failed."""
    base = [dict(s) for s in attempts[0]["steps"]]
    for index, step in enumerate(base):
        checks = []
        for number, check in enumerate(step.get("checks", [])):
            merged = dict(check)
            values = []
            ok = True
            for attempt in attempts:
                if index < len(attempt["steps"]) and number < len(attempt["steps"][index].get("checks", [])):
                    other = attempt["steps"][index]["checks"][number]
                    values.append(other.get("ms"))
                    ok = ok and other.get("ok", False)
            measured = [v for v in values if v is not None]
            if len(attempts) > 1 and measured:
                merged["ms"] = results_mod.median_or_none(measured)
                merged["worst_ms"] = max(measured)
                merged["all_ms"] = values
                merged["ok"] = ok
            checks.append(merged)
        step["checks"] = checks
        step["ok"] = all(c.get("ok") for c in checks) if checks else step.get("ok", True)
    return base


def fmt(ms) -> str:
    if ms is None:
        return "–"
    if ms >= 1000:
        return f"{ms / 1000:.2f} s".replace(".", ",")
    return f"{round(ms)} ms"
