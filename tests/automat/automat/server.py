"""The automaton's page: a small HTTP server on 127.0.0.1 with one HTML page.

The page reads the scenarios, the runs and the settings through /api/..., starts and stops
runs, and follows a run live through /api/events (server-sent events). Only this computer
can reach the server; a request from another web page is refused (the Host header must be
the server's own, and every change needs the X-Automat header, which a page from elsewhere
cannot send without the browser asking the server first).
"""

import json
import mimetypes
import queue
import sys
import threading
import webbrowser
from datetime import datetime
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlparse

from automat import __version__
from automat import config as config_mod
from automat.approvals import Approvals
from automat.results import Store
from automat.runner import Runner
from automat.scenarios import DEFAULT_LIMITS_MS, Library

PAGE = Path(__file__).resolve().parent / "page" / "index.html"
MAX_UPLOAD = 512 * 1024 * 1024
SETTING_FIELDS = {"program": str, "scenarios": str, "results": str, "quiet_ms": int,
                  "screenshots": str, "keep_runs": int, "restore_folders": bool,
                  "pause_on_mouse": bool, "keep_awake": bool}


class App:
    """What the page works with: settings, scenarios, approvals, results and the runner."""

    def __init__(self, config, session_factory, guard_factory=None, where=None, screen_info=None):
        self.config = config
        self.session_factory = session_factory
        self.guard_factory = guard_factory
        self.screen_info = screen_info or {}
        self.approvals = Approvals(config_mod.data_dir() / "zatwierdzenia.json")
        self.library = Library(config.scenarios_path)
        self.store = Store(config.results_path)
        self.runner = Runner(config, self.library, self.approvals, self.store, session_factory,
                             where=where, guard_factory=guard_factory)
        self.live = {"scenario": None, "steps": {}, "log": [], "waiting": None, "prepare": None}
        self._live_lock = threading.Lock()
        self.runner.listen(self._follow)

    def _follow(self, event: dict) -> None:
        """What a page opened in the middle of a run needs: the current scenario, its
        finished steps and the newest lines of the log."""
        kind = event["type"]
        with self._live_lock:
            if kind == "run_started":
                self.live = {"scenario": None, "steps": {}, "log": [], "waiting": None, "prepare": None}
            elif kind == "prepare":
                self.live["prepare"] = event if event.get("text") else None
            elif kind == "scenario_started":
                self.live["scenario"] = event
                self.live["steps"] = {}
            elif kind == "step_finished":
                self.live["steps"][str(event["number"])] = event["step"]
            elif kind == "log":
                self.live["log"] = (self.live["log"] + [event])[-400:]
            elif kind == "waiting":
                self.live["waiting"] = event
            elif kind == "going_on":
                self.live["waiting"] = None

    # --- reading ----------------------------------------------------------------------
    def state(self) -> dict:
        program = Path(self.config.program)
        info = {"path": str(program), "exists": program.is_file()}
        if info["exists"]:
            info["modified"] = datetime.fromtimestamp(program.stat().st_mtime).isoformat(timespec="seconds")
        return {"version": __version__, "config": self.config.as_dict(), "program": info,
                "running": self.runner.running, "waiting": self.runner.waiting,
                "current": self.runner.current, "live": self._live_copy(), "where": self.runner.where,
                "screen": self.screen_info,
                "data_dir": str(config_mod.data_dir()), "windows": sys.platform == "win32"}

    def _live_copy(self) -> dict:
        with self._live_lock:
            return json.loads(json.dumps(self.live, ensure_ascii=False))

    def scenarios(self) -> dict:
        latest = self.store.latest_by_scenario(self.config.keep_runs)
        out = []
        for scenario in self.library.list():
            item = scenario.summary(self.config.limits)
            item["approval"] = self.approvals.status(scenario.id, item["hash"])
            item["latest"] = latest.get(scenario.id)
            out.append(item)
        return {"folder": str(self.library.folder), "blocks": sorted(self.library.blocks()), "scenarios": out}

    # --- changing -----------------------------------------------------------------------
    def run(self, body: dict) -> dict:
        ids = body.get("ids") or []
        known = {s.id for s in self.library.list()}
        unknown = [i for i in ids if i not in known]
        if not ids:
            raise ValueError("nie wybrano żadnego scenariusza")
        if unknown:
            raise ValueError("nie ma scenariuszy: " + ", ".join(unknown))
        return {"id": self.runner.start(ids, body.get("options") or {})}

    def update_settings(self, body: dict) -> dict:
        if self.runner.running:
            raise ValueError("ustawienia zmienia się, gdy nic nie trwa")
        for key, kind in SETTING_FIELDS.items():
            if key in body:
                value = body[key]
                if kind is int:
                    value = int(value)
                    if value < 1:
                        raise ValueError(f"{key}: liczba większa od zera")
                elif kind is bool:
                    value = bool(value)
                else:
                    value = str(value).strip()
                setattr(self.config, key, value)
        if self.config.screenshots not in ("each_step", "on_failure"):
            raise ValueError("zrzuty: each_step albo on_failure")
        if "limits" in body:
            limits = {}
            for kind in DEFAULT_LIMITS_MS:
                value = int(body["limits"].get(kind, self.config.limits.get(kind, DEFAULT_LIMITS_MS[kind])))
                if value < 1:
                    raise ValueError(f"próg {kind}: liczba milisekund większa od zera")
                limits[kind] = value
            self.config.limits = limits
        config_mod.save(self.config)
        self.library = Library(self.config.scenarios_path)
        self.store = Store(self.config.results_path)
        self.runner.library, self.runner.store = self.library, self.store
        return self.state()


class Handler(BaseHTTPRequestHandler):
    app: App = None
    port: int = 0
    server_version = "KalahariAutomat/" + __version__

    def log_message(self, *_):  # quiet: the page shows what matters
        pass

    # --- helpers --------------------------------------------------------------------------
    def _allowed(self) -> bool:
        host = (self.headers.get("Host") or "").lower()
        return host in (f"127.0.0.1:{self.port}", f"localhost:{self.port}")

    def _send(self, status, body: bytes, kind: str, extra=None) -> None:
        self.send_response(status)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        for key, value in (extra or {}).items():
            self.send_header(key, value)
        self.end_headers()
        self.wfile.write(body)

    def _json(self, data, status=HTTPStatus.OK) -> None:
        for _ in range(5):  # the run in progress changes while it is written out
            try:
                body = json.dumps(data, ensure_ascii=False).encode("utf-8")
                break
            except RuntimeError:
                continue
        else:
            body = b'{"error": "spr\xc3\xb3buj ponownie"}'
        self._send(status, body, "application/json; charset=utf-8")

    def _error(self, status, message: str) -> None:
        self._json({"error": message}, status)

    def _body(self) -> bytes:
        length = int(self.headers.get("Content-Length") or 0)
        if length > MAX_UPLOAD:
            raise ValueError("za duży plik")
        return self.rfile.read(length) if length else b""

    # --- GET ------------------------------------------------------------------------------
    def do_GET(self) -> None:  # noqa: N802 - the name http.server calls
        if not self._allowed():
            return self._error(HTTPStatus.FORBIDDEN, "tylko z tego komputera")
        path = urlparse(self.path).path
        app = self.app
        try:
            if path in ("/", "/index.html"):
                page = PAGE.read_bytes()
                return self._send(HTTPStatus.OK, page, "text/html; charset=utf-8",
                                  {"Content-Security-Policy": "default-src 'self'; style-src 'self' 'unsafe-inline'; "
                                                              "script-src 'self' 'unsafe-inline'; img-src 'self' data:"})
            if path == "/api/state":
                return self._json(app.state())
            if path == "/api/scenarios":
                return self._json(app.scenarios())
            if path == "/api/runs":
                return self._json({"runs": app.store.runs(app.config.keep_runs)})
            if path.startswith("/api/runs/"):
                run = app.store.load(unquote(path[len("/api/runs/"):]))
                return self._json(run) if run else self._error(HTTPStatus.NOT_FOUND, "nie ma takiego przebiegu")
            if path.startswith("/files/"):
                run_id, _, relative = unquote(path[len("/files/"):]).partition("/")
                target = app.store.file(run_id, relative)
                if target is None:
                    return self._error(HTTPStatus.NOT_FOUND, "nie ma takiego pliku")
                kind = mimetypes.guess_type(target.name)[0] or "application/octet-stream"
                if kind.startswith("text/") or target.suffix in (".log", ".json", ".txt"):
                    kind = "text/plain; charset=utf-8"
                return self._send(HTTPStatus.OK, target.read_bytes(), kind)
            if path == "/api/events":
                return self._events()
        except ValueError as error:
            return self._error(HTTPStatus.BAD_REQUEST, str(error))
        return self._error(HTTPStatus.NOT_FOUND, "nie ma takiej strony")

    def _events(self) -> None:
        events = queue.Queue(maxsize=5000)

        def listener(event):
            try:
                events.put_nowait(event)
            except queue.Full:
                pass

        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.app.runner.listen(listener)
        try:
            self.wfile.write(b": start\n\n")
            self.wfile.flush()
            while True:
                try:
                    event = events.get(timeout=15)
                    data = json.dumps(event, ensure_ascii=False)
                    self.wfile.write(f"data: {data}\n\n".encode("utf-8"))
                except queue.Empty:
                    self.wfile.write(b": still here\n\n")
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError, OSError):
            pass
        finally:
            self.app.runner.unlisten(listener)

    # --- POST -----------------------------------------------------------------------------
    def do_POST(self) -> None:  # noqa: N802
        if not self._allowed() or self.headers.get("X-Automat") != "1":
            return self._error(HTTPStatus.FORBIDDEN, "tylko ze strony automatu")
        path = urlparse(self.path).path
        app = self.app
        try:
            raw = self._body()
            if path == "/api/import":
                added = app.store.import_zip(raw, origin="github")
                return self._json({"added": added})
            body = json.loads(raw.decode("utf-8") or "{}") if raw else {}
            if path == "/api/run":
                return self._json(app.run(body))
            if path == "/api/stop":
                app.runner.stop()
                return self._json({"ok": True})
            if path == "/api/go-on":
                app.runner.go_on()
                return self._json({"ok": True})
            if path == "/api/settings":
                return self._json(app.update_settings(body))
        except RuntimeError as error:
            return self._error(HTTPStatus.CONFLICT, str(error))
        except (ValueError, KeyError, TypeError) as error:
            return self._error(HTTPStatus.BAD_REQUEST, str(error))
        return self._error(HTTPStatus.NOT_FOUND, "nie ma takiego polecenia")


def serve(app: App, port: int, open_browser: bool = True) -> None:
    handler = type("BoundHandler", (Handler,), {"app": app, "port": port})
    server = ThreadingHTTPServer(("127.0.0.1", port), handler)
    server.daemon_threads = True
    address = f"http://127.0.0.1:{port}/"
    print(f"Automat testów Kalahari: {address}  (Ctrl+C kończy)", flush=True)
    if open_browser:
        threading.Timer(0.5, lambda: webbrowser.open(address)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        app.runner.stop()
        server.server_close()
