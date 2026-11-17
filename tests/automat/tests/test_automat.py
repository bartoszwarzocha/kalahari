"""The automaton's own tests: everything that does not need Windows (scenario files, runs,
results, approvals, the page's server), with the pretend program of automat.fake.

    python -m unittest discover -s tests/automat/tests -t tests/automat
"""

import io
import json
import os
import shutil
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

os.environ.setdefault("KALAHARI_AUTOMAT_DATA", tempfile.mkdtemp(prefix="automat-data-"))

from automat import books, fake  # noqa: E402
from automat import config as config_mod  # noqa: E402
from automat.approvals import Approvals  # noqa: E402
from automat.results import Store  # noqa: E402
from automat.runner import Paused, Runner  # noqa: E402
from automat.scenarios import Library, action_sentence, check_sentence, limit_of  # noqa: E402

SOURCES = [{"kind": "mockup", "ref": "makiety/znaki-specjalne.html"}]


def scenario(sid, steps=None, **extra):
    data = {"id": sid, "title": f"Scenariusz {sid}", "area": "Próby", "sources": SOURCES,
            "setup": {"books": ["karawana"]},
            "steps": steps or [{"do": "click", "target": {"window": "Okno", "name": "Przycisk"},
                                "expect": [{"check": "visible", "target": {"window": "Okno", "name": "Coś"}}]}]}
    data.update(extra)
    return data


class Folder(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp(prefix="automat-test-"))
        self.scenarios = self.root / "scenariusze"
        self.scenarios.mkdir()
        self.addCleanup(shutil.rmtree, self.root, True)

    def write(self, relative, data):
        path = self.scenarios / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")
        return path


class LibraryTest(Folder):
    def test_new_changed_and_removed_files_are_seen_without_restart(self):
        library = Library(self.scenarios)
        self.assertEqual(library.list(), [])
        path = self.write("a.json", scenario("pierwszy"))
        self.assertEqual([s.id for s in library.list()], ["pierwszy"])
        self.write("b/c.json", scenario("drugi"))
        self.assertEqual(sorted(s.id for s in library.list()), ["drugi", "pierwszy"])
        data = scenario("pierwszy", title="Nowy tytuł")
        path.write_text(json.dumps(data), encoding="utf-8")
        os.utime(path, (time.time() + 5, time.time() + 5))
        self.assertEqual(library.get("pierwszy").title, "Nowy tytuł")
        path.unlink()
        self.assertEqual([s.id for s in library.list()], ["drugi"])

    def test_folders_with_underscore_are_not_scenarios(self):
        self.write("_bloki/blok.json", {"steps": []})
        self.write("_szkice/x.json", scenario("szkic"))
        self.assertEqual(Library(self.scenarios).list(), [])

    def test_a_file_may_hold_several_scenarios(self):
        self.write("wiele.json", {"scenarios": [scenario("jeden"), scenario("dwa")]})
        self.write("lista.json", [scenario("trzy")])
        self.assertEqual(sorted(s.id for s in Library(self.scenarios).list()), ["dwa", "jeden", "trzy"])

    def test_problems_are_reported_not_hidden(self):
        self.write("zly.json", "{")
        (self.scenarios / "zly.json").write_text("{ nie json", encoding="utf-8")
        self.write("bez-zrodla.json", scenario("bez-zrodla", sources=[]))
        self.write("a.json", scenario("powtorka"))
        self.write("b.json", scenario("powtorka"))
        found = {s.file.name: s for s in Library(self.scenarios).list()}
        self.assertIn("to nie jest poprawny JSON", str(found["zly.json"].problems[0]))
        self.assertTrue(any("źródła" in str(p) for p in found["bez-zrodla.json"].problems))
        self.assertTrue(found["a.json"].valid)
        self.assertTrue(any("w innym pliku" in str(p) for p in found["b.json"].problems))

    def test_unknown_fields_and_parts_without_rows(self):
        steps = [{"do": "click", "target": {"window": "Okno", "name": "X", "part": "Gwiazdka"}, "kolor": 1}]
        self.write("a.json", scenario("pola", steps=steps))
        problems = " ".join(map(str, Library(self.scenarios).get("pola").problems))
        self.assertIn("nieznane pole „kolor”", problems)
        self.assertIn("potrzebne też „row”", problems)

    def test_blocks_are_filled_in(self):
        self.write("_bloki/otworz.json", {"steps": [{"do": "menu", "path": ["Wstaw", "{{co}}"]}]})
        steps = [{"do": "use", "block": "otworz", "with": {"co": "Znak specjalny…"},
                  "expect": [{"check": "window_shown", "window": "Znak specjalny"}]}]
        self.write("a.json", scenario("blok", steps=steps))
        found = Library(self.scenarios).get("blok")
        self.assertTrue(found.valid, found.problems)
        self.assertEqual(found.steps[0]["path"], ["Wstaw", "Znak specjalny…"])
        self.assertEqual(found.steps[0]["_block"], "otworz")
        self.assertEqual(found.steps[0]["expect"][0]["check"], "window_shown")

    def test_missing_block(self):
        self.write("a.json", scenario("brak", steps=[{"do": "use", "block": "nie-ma"}]))
        self.assertIn("nie ma bloku", str(Library(self.scenarios).get("brak").problems[0]))

    def test_fingerprint_follows_expectations_not_titles(self):
        self.write("a.json", scenario("odcisk"))
        library = Library(self.scenarios)
        before = library.get("odcisk").expectations_hash()
        self.write("a.json", scenario("odcisk", title="Inny tytuł"))
        os.utime(self.scenarios / "a.json", (time.time() + 5, time.time() + 5))
        self.assertEqual(library.get("odcisk").expectations_hash(), before)
        changed = scenario("odcisk")
        changed["steps"][0]["expect"][0]["within_ms"] = 900
        self.write("a.json", changed)
        os.utime(self.scenarios / "a.json", (time.time() + 10, time.time() + 10))
        self.assertNotEqual(library.get("odcisk").expectations_hash(), before)

    def test_limits_by_kind_and_own_limits(self):
        self.assertEqual(limit_of({"check": "window_shown"}), 500)
        self.assertEqual(limit_of({"check": "visible"}), 300)
        self.assertEqual(limit_of({"check": "color"}), 1000)
        self.assertEqual(limit_of({"check": "visible", "within_ms": 700}), 700)
        self.assertEqual(limit_of({"check": "busy_at_most_ms", "value": 250}), 250)
        self.assertEqual(limit_of({"check": "visible"}, {"window": 1, "reaction": 2, "theme": 3}), 2)

    def test_sentences_name_rows_and_parts(self):
        target = {"window": "Znak specjalny", "name": "Znaki", "row": "Euro", "part": "Ulubiony"}
        self.assertEqual(check_sentence({"check": "visible", "target": target}),
                         "„Ulubiony” w wierszu „Euro” listy „Znaki” widoczny")

    def test_the_repository_scenarios_are_valid(self):
        library = Library(config_mod.AUTOMAT_DIR / "scenariusze")
        scenarios = library.list()
        self.assertGreaterEqual(len(scenarios), 3)
        for found in scenarios:
            self.assertTrue(found.valid, f"{found.id}: {found.problems}")
            self.assertEqual(found.unsupported(), [], found.id)


class ApprovalsTest(unittest.TestCase):
    def test_approve_change_withdraw(self):
        folder = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, folder, True)
        approvals = Approvals(folder / "z.json")
        self.assertEqual(approvals.status("a", "111")["state"], "none")
        approvals.approve("a", "111")
        self.assertEqual(approvals.status("a", "111")["state"], "approved")
        self.assertEqual(approvals.status("a", "222")["state"], "changed")
        approvals.withdraw("a")
        self.assertEqual(approvals.status("a", "111")["state"], "none")


class PausingSession(fake.FakeSession):
    """Someone moves the mouse before the second step, once."""
    paused = False

    def act(self, step):
        if step.get("note") == "pause-here" and not PausingSession.paused:
            PausingSession.paused = True
            raise Paused("ktoś poruszył myszą")
        return super().act(step)


class RunnerTest(Folder):
    def make(self, factory=fake.session_factory, guard=None):
        config = config_mod.Config(program=str(self.root / "kalahari.exe"), scenarios=str(self.scenarios),
                                   results=str(self.root / "wyniki"))
        library = Library(self.scenarios)
        store = Store(config.results_path)
        runner = Runner(config, library, Approvals(self.root / "z.json"), store, factory, where=fake.WHERE,
                        guard_factory=guard)
        events = []
        runner.listen(events.append)
        return runner, store, events

    def test_a_run_writes_its_file_and_screenshots(self):
        self.write("a.json", scenario("dobry"))
        failing = scenario("zly")
        failing["steps"][0]["expect"][0]["note"] = "fake:fail"
        self.write("b.json", failing)
        runner, store, events = self.make()
        run = runner.run_now(["dobry", "zly"])
        saved = store.load(run["id"])
        self.assertEqual([s["state"] for s in saved["scenarios"]], ["ok", "fail"])
        self.assertEqual(saved["state"], "done")
        after = saved["scenarios"][0]["steps"][0]["screens"]["after"]
        self.assertEqual(after, "dobry/01-po.png")
        self.assertIsNotNone(store.file(run["id"], after))
        self.assertEqual(saved["scenarios"][0]["log"], "dobry/kalahari.log")
        kinds = [e["type"] for e in events]
        self.assertEqual(kinds[0], "run_started")
        self.assertEqual(kinds[-1], "run_finished")
        self.assertIn("scenario_finished", kinds)

    def test_slow_effects_fail_with_their_time(self):
        slow = scenario("wolny")
        slow["steps"][0]["expect"][0]["note"] = "fake:slow"
        self.write("a.json", slow)
        runner, store, _ = self.make()
        check = runner.run_now(["wolny"])["scenarios"][0]["steps"][0]["checks"][0]
        self.assertFalse(check["ok"])
        self.assertEqual(check["ms"], 450.0)
        self.assertEqual(check["limit_ms"], 300)

    def test_missing_element_stops_the_scenario(self):
        steps = [{"do": "click", "target": {"window": "Okno", "name": "A"}, "note": "fake:missing"},
                 {"do": "click", "target": {"window": "Okno", "name": "B"}}]
        self.write("a.json", scenario("brak", steps=steps))
        runner, _, _ = self.make()
        result = runner.run_now(["brak"])["scenarios"][0]
        self.assertEqual(result["state"], "fail")
        self.assertEqual(len(result["steps"]), 1)
        self.assertIn("krok 1", result["reason"])

    def test_what_cannot_run_here_and_invalid_are_not_run(self):
        word = scenario("w-wordzie", steps=[{"do": "app", "app": "word", "steps": [{"do": "keys", "keys": "Ctrl+V"}]}])
        word["tags"] = ["wymaga-worda"]
        self.write("a.json", word)
        self.write("b.json", scenario("zepsuty", sources=[]))

        def factory(*args):
            return fake.session_factory(*args)

        factory.skip_reason = lambda s: "nie ma Worda" if "wymaga-worda" in s.tags else None
        runner, _, _ = self.make(factory)
        results = runner.run_now(["w-wordzie", "zepsuty", "nie-ma"])["scenarios"]
        self.assertEqual([s["state"] for s in results], ["skipped", "error", "error"])
        self.assertEqual(results[0]["reason"], "nie ma Worda")

    def test_steps_in_another_program_become_steps_of_the_scenario(self):
        data = scenario("notatnik", steps=[
            {"do": "keys", "keys": "Ctrl+C"},
            {"do": "app", "app": "notatnik", "steps": [{"do": "keys", "keys": "Ctrl+V"}],
             "expect": [{"check": "clipboard", "contains": "A"}]}])
        self.write("a.json", data)
        found = Library(self.scenarios).get("notatnik")
        self.assertEqual(found.problems, [])
        self.assertEqual([s["do"] for s in found.steps], ["keys", "_app_open", "keys", "_app_close"])
        self.assertEqual(found.steps[2]["_app"], "notatnik")
        self.assertEqual(found.steps[3]["expect"][0]["check"], "clipboard")
        sentences = [action_sentence(s) for s in found.steps]
        self.assertEqual(sentences[1:], ["Uruchom program Notatnik", "Notatnik: naciśnij Ctrl+V",
                                         "Zamknij program Notatnik"])

    def test_unknown_program_and_bad_mask_are_problems(self):
        data = scenario("zle", steps=[
            {"do": "app", "app": "paint", "steps": [{"do": "keys", "keys": "Enter"}]},
            {"do": "keys", "keys": "Enter", "expect": [{"check": "image", "area": "editor", "pattern": "Zły wzór",
                                                        "mask": [[1, 2, 3]]}]}])
        self.write("a.json", data)
        problems = " ".join(map(str, Library(self.scenarios).get("zle").problems))
        self.assertIn("nieznany program", problems)
        self.assertIn("nazwa wzorca", problems)
        self.assertIn("prostokątów", problems)

    def test_start_failure_is_an_error(self):
        self.write("a.json", scenario("start", fake="start_fails"))
        runner, _, _ = self.make()
        result = runner.run_now(["start"])["scenarios"][0]
        self.assertEqual(result["state"], "error")
        self.assertIn("nie uruchomił się", result["reason"])

    def test_repeats_give_median_and_worst(self):
        self.write("a.json", scenario("powtorz"))
        runner, _, _ = self.make()
        result = runner.run_now(["powtorz"], {"repeat": 3})["scenarios"][0]
        check = result["steps"][0]["checks"][0]
        self.assertEqual(result["attempts"], 3)
        self.assertEqual(check["all_ms"], [40.0, 40.0, 40.0])
        self.assertEqual(check["worst_ms"], 40.0)

    def test_stop_on_first_failure(self):
        failing = scenario("pierwszy")
        failing["steps"][0]["expect"][0]["note"] = "fake:fail"
        self.write("a.json", failing)
        self.write("b.json", scenario("drugi"))
        runner, _, _ = self.make()
        states = [s["state"] for s in runner.run_now(["pierwszy", "drugi"], {"stop_on_fail": True})["scenarios"]]
        self.assertEqual(states, ["fail", "stopped"])

    def test_stop_ends_a_long_wait_at_once(self):
        long = scenario("dlugi")
        long["steps"][0]["note"] = "fake:long"  # a step that waits five seconds
        self.write("a.json", long)
        self.write("b.json", scenario("nastepny"))
        runner, store, _ = self.make()
        run_id = runner.start(["dlugi", "nastepny"])
        time.sleep(0.3)
        began = time.perf_counter()
        runner.stop()
        runner.wait(5)
        self.assertFalse(runner.running)
        self.assertLess(time.perf_counter() - began, 1.0)
        saved = store.load(run_id)
        self.assertEqual(saved["state"], "stopped")
        self.assertEqual([s["state"] for s in saved["scenarios"]], ["stopped", "stopped"])

    def test_step_by_step_waits_for_go_on(self):
        self.write("a.json", scenario("krok"))
        runner, _, events = self.make()

        def press_next():
            for _ in range(200):
                if runner.waiting == "next":
                    runner.go_on()
                    return
                time.sleep(0.01)

        helper = threading.Thread(target=press_next)
        helper.start()
        run = runner.run_now(["krok"], {"step_by_step": True})
        helper.join()
        self.assertEqual(run["scenarios"][0]["state"], "ok")
        self.assertIn("waiting", [e["type"] for e in events])

    def test_a_pause_repeats_the_step_after_go_on(self):
        steps = [{"do": "click", "target": {"window": "O", "name": "A"}},
                 {"do": "click", "target": {"window": "O", "name": "B"}, "note": "pause-here"}]
        self.write("a.json", scenario("pauza", steps=steps))
        PausingSession.paused = False
        runner, _, events = self.make(factory=lambda c, s, f: PausingSession(c, s, f))

        def resume():
            for _ in range(200):
                if runner.waiting == "paused":
                    runner.go_on()
                    return
                time.sleep(0.01)

        helper = threading.Thread(target=resume)
        helper.start()
        result = runner.run_now(["pauza"])["scenarios"][0]
        helper.join()
        self.assertEqual(result["state"], "ok")
        self.assertEqual(len(result["steps"]), 2)
        waits = [e for e in events if e["type"] == "waiting"]
        self.assertEqual(waits[0]["why"], "paused")

    def test_the_guard_runs_around_the_whole_run(self):
        self.write("a.json", scenario("straz"))
        calls = []

        class Guard:
            def __init__(self, folder):
                calls.append(("made", Path(folder).name))

            def before(self):
                calls.append("before")

            def after(self):
                calls.append("after")
                return ["przywrócono coś"]

        runner, store, _ = self.make(guard=Guard)
        run = runner.run_now(["straz"])
        self.assertEqual(calls, [("made", "_twoje-dane"), "before", "after"])
        self.assertEqual(store.load(run["id"])["your_data"], ["przywrócono coś"])


class StoreTest(Folder):
    def test_import_zip_keeps_inside_its_folder(self):
        store = Store(self.root / "wyniki")
        run = {"id": "20261117-101500", "started": "2026-11-17T10:15:00", "scenarios": []}
        data = io.BytesIO()
        with zipfile.ZipFile(data, "w") as archive:
            archive.writestr("przebieg/przebieg.json", json.dumps(run))
            archive.writestr("przebieg/a/01-po.png", b"png")
            archive.writestr("przebieg/../../ucieczka.txt", b"nie")
        added = store.import_zip(data.getvalue())
        self.assertEqual(added, ["20261117-101500-github"])
        self.assertTrue((self.root / "wyniki" / added[0] / "a" / "01-po.png").is_file())
        self.assertFalse((self.root / "ucieczka.txt").exists())
        self.assertEqual(store.load(added[0])["where"], "GitHub")
        self.assertIsNone(store.file(added[0], "../../ucieczka.txt"))

    def test_bad_run_ids_are_refused(self):
        with self.assertRaises(ValueError):
            Store(self.root).run_dir("../x")


class BooksTest(unittest.TestCase):
    def test_books_are_written_fresh(self):
        folder = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, folder, True)
        made = books.make_books(["karawana", "wyspa"], folder)
        manifest = json.loads(made["karawana"].read_text(encoding="utf-8"))
        self.assertEqual(manifest["books"][0]["title"], "Karawana")
        titles = [e["title"] for e in manifest["books"][0]["main"]]
        self.assertIn("Targ w Ghadames", titles)
        with self.assertRaises(ValueError):
            books.make_books(["nie-ma"], folder)


class ScreenDiffersTest(unittest.TestCase):
    """The comparison of a region before and after an action (pure Pillow, any system)."""

    def test_differs(self):
        import importlib.util  # noqa: PLC0415
        from PIL import Image, ImageDraw  # noqa: PLC0415
        # The module file alone: the automat.win package is for Windows only, and mss is
        # imported here, not started
        path = Path(__file__).resolve().parent.parent / "automat" / "win" / "screen.py"
        spec = importlib.util.spec_from_file_location("automat_screen_under_test", path)
        screen = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(screen)
        before = Image.new("RGB", (30, 20), "white")
        same = before.copy()
        star = before.copy()
        ImageDraw.Draw(star).rectangle((8, 4, 20, 14), fill="black")
        caret = before.copy()
        ImageDraw.Draw(caret).line((15, 2, 15, 18), fill="black")
        self.assertFalse(screen.differs(before, same))
        self.assertTrue(screen.differs(before, star))
        self.assertFalse(screen.differs(before, caret))
        self.assertTrue(screen.differs(before, caret, ignore_caret=False))
        self.assertTrue(screen.differs(before, Image.new("RGB", (31, 20), "white")))
        # Against an approved pattern: a mask leaves a part out, another size never matches
        self.assertTrue(screen.compare(before, same)["same"])
        result = screen.compare(before, star)
        self.assertFalse(result["same"])
        self.assertIsNotNone(result["diff"])
        self.assertIn("%", result["detail"])
        self.assertTrue(screen.compare(before, star, masks=[[8, 4, 13, 11]])["same"])
        self.assertFalse(screen.compare(before, Image.new("RGB", (31, 20), "white"))["same"])


class FilesTest(unittest.TestCase):
    """The text of the files the program writes, for the "file" check."""

    def test_text_of_word_book_and_plain_files(self):
        from automat.files import text_of  # noqa: PLC0415
        with tempfile.TemporaryDirectory() as folder:
            docx = Path(folder) / "k.docx"
            with zipfile.ZipFile(docx, "w") as archive:
                archive.writestr("word/document.xml", "<w:document><w:body><w:p><w:r><w:t>Targ w Ghadames"
                                                      "</w:t></w:r></w:p><w:p><w:r><w:t>Zażółć &amp; jaźń</w:t>"
                                                      "</w:r></w:p></w:body></w:document>")
                archive.writestr("word/styles.xml", "<w:styles><w:t>Nagłówek</w:t></w:styles>")
            text, problem = text_of(docx)
            self.assertIsNone(problem)
            self.assertIn("Targ w Ghadames\nZażółć & jaźń", text)
            self.assertNotIn("Nagłówek", text)
            chapter = Path(folder) / "r.kchapter"
            chapter.write_text(json.dumps({"content": {"kml": "<kml><p>Ala <b>ma</b> kota</p></kml>"}}),
                               encoding="utf-8")
            self.assertIn("Ala ma kota", text_of(chapter)[0])
            plain = Path(folder) / "t.md"
            plain.write_bytes("# Śląsk".encode("cp1250"))
            self.assertEqual(text_of(plain)[0], "# Śląsk")
            self.assertIsNone(text_of(Path(folder) / "nie-ma.txt")[0])


class ServerTest(Folder):
    def setUp(self):
        super().setUp()
        from automat.server import App, Handler  # noqa: PLC0415
        from http.server import ThreadingHTTPServer  # noqa: PLC0415
        self.write("a.json", scenario("strona"))
        config = config_mod.Config(program=str(self.root / "kalahari.exe"), scenarios=str(self.scenarios),
                                   results=str(self.root / "wyniki"))
        self.app = App(config, fake.session_factory, where=fake.WHERE)
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.port = self.server.server_address[1]
        self.server.RequestHandlerClass = type("H", (Handler,), {"app": self.app, "port": self.port})
        threading.Thread(target=self.server.serve_forever, daemon=True).start()
        self.addCleanup(self.server.server_close)
        self.addCleanup(self.server.shutdown)

    def call(self, path, body=None, headers=None):
        request = urllib.request.Request(f"http://127.0.0.1:{self.port}{path}",
                                         data=None if body is None else json.dumps(body).encode(),
                                         headers=headers or {})
        try:
            with urllib.request.urlopen(request, timeout=10) as answer:
                return answer.status, answer.read()
        except urllib.error.HTTPError as error:
            return error.code, error.read()

    def test_page_and_state(self):
        status, page = self.call("/")
        self.assertEqual(status, 200)
        self.assertIn("Automat testów Kalahari".encode(), page)
        status, state = self.call("/api/state")
        self.assertEqual(json.loads(state)["where"], fake.WHERE)

    def test_changes_need_the_page_header(self):
        status, _ = self.call("/api/run", {"ids": ["strona"]})
        self.assertEqual(status, 403)

    def test_other_hosts_are_refused(self):
        status, _ = self.call("/api/state", headers={"Host": "zly.example:80"})
        self.assertEqual(status, 403)

    def test_a_run_from_the_page(self):
        status, answer = self.call("/api/run", {"ids": ["strona"]}, {"X-Automat": "1"})
        self.assertEqual(status, 200, answer)
        self.app.runner.wait(10)
        status, runs = self.call("/api/runs")
        runs = json.loads(runs)["runs"]
        self.assertEqual(runs[0]["counts"]["ok"], 1)
        status, scenarios = self.call("/api/scenarios")
        self.assertEqual(json.loads(scenarios)["scenarios"][0]["latest"]["state"], "ok")
        status, picture = self.call(f"/files/{runs[0]['id']}/strona/01-po.png")
        self.assertEqual((status, picture[:4]), (200, b"\x89PNG"))
        status, _ = self.call(f"/files/{runs[0]['id']}/../../a.json")
        self.assertEqual(status, 404)

    def test_unknown_scenarios_are_refused(self):
        status, answer = self.call("/api/run", {"ids": ["nie-ma"]}, {"X-Automat": "1"})
        self.assertEqual(status, 400)
        self.assertIn("nie-ma", json.loads(answer)["error"])


if __name__ == "__main__":
    unittest.main()
