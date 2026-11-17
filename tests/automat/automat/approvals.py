"""The user's approvals of scenarios, kept outside the repository.

An approval holds the fingerprint of what was approved (sources, setup, steps with their
expected effects and limits). When the scenario changes, the fingerprint no longer matches
and the scenario counts as changed until the user approves it again.
"""

import getpass
import json
import threading
from datetime import datetime
from pathlib import Path

from automat.config import data_dir


class Approvals:
    def __init__(self, path: Path = None):
        self.path = Path(path) if path else data_dir() / "zatwierdzenia.json"
        self._lock = threading.Lock()

    def _read(self) -> dict:
        try:
            return json.loads(self.path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return {}

    def _write(self, data: dict) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_suffix(".tmp")
        temporary.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
        temporary.replace(self.path)

    def status(self, scenario_id: str, fingerprint) -> dict:
        """{"state": "approved" | "changed" | "none", "date": ...}"""
        entry = self._read().get(scenario_id)
        if not entry:
            return {"state": "none"}
        state = "approved" if fingerprint and entry.get("hash") == fingerprint else "changed"
        return {"state": state, "date": entry.get("date"), "by": entry.get("by")}

    def approve(self, scenario_id: str, fingerprint: str) -> dict:
        with self._lock:
            data = self._read()
            data[scenario_id] = {"hash": fingerprint, "date": datetime.now().isoformat(timespec="seconds"),
                                 "by": getpass.getuser()}
            self._write(data)
            return data[scenario_id]

    def withdraw(self, scenario_id: str) -> None:
        with self._lock:
            data = self._read()
            data.pop(scenario_id, None)
            self._write(data)
