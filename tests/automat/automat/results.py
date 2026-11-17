"""Results as files: one folder per run, with przebieg.json and the screenshots.

    automat-wyniki/
        20261117-093104/
            przebieg.json
            znaki-specjalne-ulubione/01-po.png ...

The page reads the newest runs from here; a ZIP of such a folder (for example from GitHub)
can be added with import_zip().
"""

import io
import json
import re
import shutil
import statistics
import threading
import zipfile
from datetime import datetime
from pathlib import Path

RUN_FILE = "przebieg.json"
_RUN_ID = re.compile(r"^[0-9]{8}-[0-9]{6}(-[a-z0-9-]+)?$")


def new_run_id() -> str:
    return datetime.now().strftime("%Y%m%d-%H%M%S")


class Store:
    def __init__(self, folder: Path):
        self.folder = Path(folder)
        self._lock = threading.Lock()

    def run_dir(self, run_id: str) -> Path:
        if not _RUN_ID.match(run_id):
            raise ValueError(f"bad run id: {run_id}")
        return self.folder / run_id

    def save(self, run: dict) -> None:
        folder = self.run_dir(run["id"])
        folder.mkdir(parents=True, exist_ok=True)
        with self._lock:
            temporary = folder / (RUN_FILE + ".tmp")
            temporary.write_text(json.dumps(run, ensure_ascii=False, indent=1), encoding="utf-8")
            temporary.replace(folder / RUN_FILE)

    def load(self, run_id: str):
        try:
            return json.loads((self.run_dir(run_id) / RUN_FILE).read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return None

    def run_ids(self) -> list:
        if not self.folder.is_dir():
            return []
        return sorted((p.name for p in self.folder.iterdir()
                       if p.is_dir() and _RUN_ID.match(p.name) and (p / RUN_FILE).exists()),
                      reverse=True)

    def runs(self, limit: int = 30) -> list:
        out = []
        for run_id in self.run_ids()[:limit]:
            run = self.load(run_id)
            if run:
                out.append(summary(run))
        return out

    def latest_by_scenario(self, limit: int = 30) -> dict:
        """The newest finished result of every scenario, from the last `limit` runs."""
        latest = {}
        for run_id in self.run_ids()[:limit]:
            run = self.load(run_id)
            if not run:
                continue
            for result in run.get("scenarios", []):
                if result.get("state") in ("ok", "fail", "error") and result["id"] not in latest:
                    latest[result["id"]] = {"run": run_id, "state": result["state"],
                                            "when": run.get("started"), "steps": step_times(result)}
        return latest

    def time_series(self, limit: int = 30) -> list:
        """For every measured check: its values in the last `limit` runs, oldest first."""
        series = {}
        for run_id in reversed(self.run_ids()[:limit]):
            run = self.load(run_id)
            if not run:
                continue
            for result in run.get("scenarios", []):
                for step in result.get("steps", []):
                    for number, check in enumerate(step.get("checks", [])):
                        if check.get("ms") is None or check.get("limit_ms") is None:
                            continue
                        key = f"{result['id']}#{step['index']}#{number}"
                        entry = series.setdefault(key, {
                            "key": key, "scenario": result["id"], "title": result.get("title"),
                            "step": step.get("say"), "check": check.get("say"), "points": []})
                        entry["limit_ms"] = check["limit_ms"]
                        entry["points"].append({"run": run_id, "when": run.get("started"),
                                                "ms": check["ms"], "ok": check.get("ok"),
                                                "program": run.get("program", {}).get("modified")})
        return list(series.values())

    def file(self, run_id: str, relative: str):
        """A file of a run (a screenshot); None when the path leaves the run folder."""
        base = self.run_dir(run_id).resolve()
        target = (base / relative).resolve()
        if base not in target.parents or not target.is_file():
            return None
        return target

    def import_zip(self, data: bytes, origin: str = "github") -> list:
        """Add the runs from a ZIP (an artifact of the GitHub job); returns their ids."""
        added = []
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            names = archive.namelist()
            roots = sorted({n.split("/")[0] for n in names if n.endswith("/" + RUN_FILE) or n == RUN_FILE})
            for root in roots:
                prefix = "" if root == RUN_FILE else root + "/"
                run = json.loads(archive.read(prefix + RUN_FILE).decode("utf-8"))
                run_id = run.get("id", "")
                if not re.match(r"^[0-9]{8}-[0-9]{6}$", run_id[:15]):
                    continue
                new_id = f"{run_id[:15]}-{origin}"
                target = self.run_dir(new_id)
                if target.exists():
                    shutil.rmtree(target)
                for name in names:
                    if not name.startswith(prefix) or name.endswith("/"):
                        continue
                    relative = Path(name[len(prefix):])
                    if relative.is_absolute() or ".." in relative.parts:
                        continue
                    destination = target / relative
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    destination.write_bytes(archive.read(name))
                run["id"] = new_id
                run.setdefault("where", "GitHub")
                self.save(run)
                added.append(new_id)
        return added


def summary(run: dict) -> dict:
    counts = {"ok": 0, "fail": 0, "error": 0, "skipped": 0, "stopped": 0}
    unapproved = 0
    for result in run.get("scenarios", []):
        if result.get("state") in counts:
            counts[result["state"]] += 1
        if result.get("approval") != "approved" and result.get("state") in ("ok", "fail"):
            unapproved += 1
    return {"id": run["id"], "started": run.get("started"), "finished": run.get("finished"),
            "where": run.get("where"), "state": run.get("state"), "counts": counts,
            "unapproved": unapproved, "total": len(run.get("scenarios", [])),
            "duration_ms": run.get("duration_ms"), "program": run.get("program", {})}


def step_times(result: dict) -> list:
    return [{"index": s.get("index"), "checks": [{"ms": c.get("ms"), "ok": c.get("ok")} for c in s.get("checks", [])]}
            for s in result.get("steps", [])]


def median_or_none(values):
    values = [v for v in values if v is not None]
    return round(statistics.median(values), 1) if values else None
