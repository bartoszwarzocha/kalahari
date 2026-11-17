"""The automaton's command line.

    python -m automat strona              the page in the browser (the usual way)
    python -m automat lista               the scenarios and what is wrong with them
    python -m automat uruchom ID ...      run scenarios here and now (--wszystkie: all)

On Windows it drives kalahari.exe; elsewhere only "lista" and the simulation (--symulacja)
work, for the automaton's own tests.
"""

import argparse
import os
import sys
from pathlib import Path

from automat import config as config_mod
from automat import fake
from automat.approvals import Approvals
from automat.results import Store, summary
from automat.runner import Runner, fmt
from automat.scenarios import Library

STATE_WORDS = {"ok": "zgodny", "fail": "niezgodny", "error": "błąd automatu", "skipped": "pominięty",
               "stopped": "zatrzymany", "waiting": "nie uruchomiony"}


def _windows_parts(simulation: bool):
    """(session factory, guard factory, where, screen info)."""
    if simulation:
        return fake.session_factory, None, fake.WHERE, {}
    if sys.platform != "win32":
        raise SystemExit("Automat obsługuje Kalahari tylko w Windows; tu działa „lista” i „--symulacja”.")
    from automat.win import guard, kalahari, make_dpi_aware, system  # noqa: PLC0415 - Windows only
    make_dpi_aware()
    info = system.session_info()
    where = os.environ.get("AUTOMAT_WHERE") or f"ten komputer ({info['user']})"
    return kalahari.session_factory, guard.RunGuard, where, info


def _config(args) -> config_mod.Config:
    config = config_mod.load()
    for key in ("program", "scenarios", "results"):
        value = getattr(args, key, None)
        if value:
            setattr(config, key, str(Path(value).resolve()))
    return config


def cmd_list(args) -> int:
    config = _config(args)
    library = Library(config.scenarios_path)
    scenarios = library.list()
    if not scenarios:
        print(f"Brak scenariuszy w {library.folder}")
        return 1
    bad = 0
    for scenario in scenarios:
        mark = "BŁĄD" if not scenario.valid else ("POMIJANY" if scenario.unsupported() else "ok")
        print(f"{mark:9} {scenario.id:40} {scenario.title}")
        for problem in scenario.problems:
            print(f"          - {problem}")
        if scenario.valid and scenario.unsupported():
            print(f"          - automat jeszcze nie umie: {', '.join(scenario.unsupported())}")
        bad += 0 if scenario.valid else 1
    print(f"\nScenariuszy: {len(scenarios)}, z błędami: {bad} (folder {library.folder})")
    return 1 if bad else 0


def cmd_run(args) -> int:
    config = _config(args)
    if args.zrzuty:
        config.screenshots = args.zrzuty
    sessions, guards, where, info = _windows_parts(args.symulacja)
    if args.gdzie:
        where = args.gdzie
    library = Library(config.scenarios_path)
    ids = [s.id for s in library.list()] if args.wszystkie else args.ids
    if not ids:
        print("Podaj identyfikatory scenariuszy albo --wszystkie.")
        return 2
    if args.bez_kopii:
        config.restore_folders = False
    runner = Runner(config, library, Approvals(config_mod.data_dir() / "zatwierdzenia.json"),
                    Store(config.results_path), sessions, where=where,
                    guard_factory=(lambda folder: guards(folder, config)) if guards else None)

    def show(event):
        kind = event["type"]
        if kind == "log":
            print(f"  {event['time']}  {event['text']}", flush=True)
        elif kind == "scenario_started":
            print(f"\n== {event['title']} ({event['id']})", flush=True)
        elif kind == "scenario_finished":
            result = event["result"]
            line = f"== wynik: {STATE_WORDS.get(result['state'], result['state'])}"
            if result.get("reason"):
                line += f" – {result['reason']}"
            print(line, flush=True)

    runner.listen(show)
    if info:
        print(f"Ekran: {info.get('screen')}, DPI {info.get('dpi')}, pulpit {info.get('input_desktop')}")
    run = runner.run_now(ids, {"repeat": args.powtorzenia, "stop_on_fail": args.stop})
    counts = summary(run)["counts"]
    folder = runner.store.run_dir(run["id"])
    print(f"\nPrzebieg {run['id']}: zgodne {counts['ok']}, niezgodne {counts['fail']}, błędy automatu "
          f"{counts['error']}, pominięte {counts['skipped']}; wyniki w {folder}")
    _step_summary(run, folder)
    return 0 if counts["fail"] == 0 and counts["error"] == 0 else 1


def _step_summary(run: dict, folder: Path) -> None:
    """A table for the GitHub job page, when there is one."""
    target = os.environ.get("GITHUB_STEP_SUMMARY")
    if not target:
        return
    lines = [f"## Automat testów: przebieg {run['id']}", "", f"Gdzie: {run.get('where')}", "",
             "| Scenariusz | Wynik | Kroki | Czasy |", "|---|---|---|---|"]
    for result in run["scenarios"]:
        steps = result.get("steps", [])
        timed = [c for s in steps for c in s.get("checks", []) if c.get("ms") is not None]
        times = ", ".join(f"{fmt(c['ms'])}{'' if c.get('ok') else ' ✗'}" for c in timed[:8])
        state = STATE_WORDS.get(result["state"], result["state"])
        if result.get("reason"):
            state += f" – {result['reason']}"
        lines.append(f"| {result.get('title', result['id'])} | {state} | {len(steps)} | {times} |")
    if run.get("your_data"):
        lines += ["", "Twoje dane: " + "; ".join(run["your_data"])]
    with open(target, "a", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")


def cmd_page(args) -> int:
    from automat.server import App, serve  # noqa: PLC0415
    config = _config(args)
    sessions, guards, where, info = _windows_parts(args.symulacja)
    app = App(config, sessions, guard_factory=(lambda folder: guards(folder, config)) if guards else None,
              where=where, screen_info=info)
    serve(app, args.port or config.port, open_browser=not args.bez_przegladarki)
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="automat", description="Automat testów Kalahari")
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--program", help="ścieżka do kalahari.exe")
    common.add_argument("--scenarios", help="folder scenariuszy")
    common.add_argument("--results", help="folder wyników")
    parts = parser.add_subparsers(dest="command", required=True)

    page = parts.add_parser("strona", parents=[common], help="strona automatu w przeglądarce")
    page.add_argument("--port", type=int)
    page.add_argument("--bez-przegladarki", action="store_true", help="nie otwieraj przeglądarki")
    page.add_argument("--symulacja", action="store_true", help="bez programu (do testów automatu)")
    page.set_defaults(handler=cmd_page)

    listing = parts.add_parser("lista", parents=[common], help="scenariusze i ich błędy")
    listing.set_defaults(handler=cmd_list)

    run = parts.add_parser("uruchom", parents=[common], help="uruchom scenariusze")
    run.add_argument("ids", nargs="*")
    run.add_argument("--wszystkie", action="store_true")
    run.add_argument("--powtorzenia", type=int, default=1)
    run.add_argument("--stop", action="store_true", help="zatrzymaj przy pierwszym błędzie")
    run.add_argument("--zrzuty", choices=["each_step", "on_failure"])
    run.add_argument("--gdzie", help="opis komputera w wynikach, np. GitHub (windows-latest)")
    run.add_argument("--bez-kopii", action="store_true",
                     help="bez kopii i przywracania folderów programu (maszyna tylko do testów)")
    run.add_argument("--symulacja", action="store_true", help="bez programu (do testów automatu)")
    run.set_defaults(handler=cmd_run)

    args = parser.parse_args(argv)
    return args.handler(args)


if __name__ == "__main__":
    sys.exit(main())
