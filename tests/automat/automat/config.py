"""Where things are, and the automaton's own settings.

The settings and the approvals live on the user's computer, outside the repository
(%LOCALAPPDATA%\\KalahariAutomat on Windows), so nothing committed to the repository can
change what the user approved.
"""

import json
import os
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path

from automat.scenarios import DEFAULT_LIMITS_MS

AUTOMAT_DIR = Path(__file__).resolve().parents[1]  # tests/automat
REPO_ROOT = AUTOMAT_DIR.parents[1]


def data_dir() -> Path:
    override = os.environ.get("KALAHARI_AUTOMAT_DATA")
    if override:
        return Path(override)
    if sys.platform == "win32":
        return Path(os.environ.get("LOCALAPPDATA", Path.home())) / "KalahariAutomat"
    return Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local" / "share")) / "kalahari-automat"


def default_program() -> Path:
    name = "kalahari.exe" if sys.platform == "win32" else "kalahari"
    for folder in ("build-windows", "build"):
        candidate = REPO_ROOT / folder / "bin" / name
        if candidate.exists():
            return candidate
    return REPO_ROOT / "build-windows" / "bin" / name


@dataclass
class Config:
    program: str = ""
    scenarios: str = str(AUTOMAT_DIR / "scenariusze")
    results: str = str(REPO_ROOT / "automat-wyniki")
    limits: dict = field(default_factory=lambda: dict(DEFAULT_LIMITS_MS))
    quiet_ms: int = 150  # "ready on screen": no change for this long
    screenshots: str = "each_step"  # or "on_failure"
    keep_runs: int = 30  # runs shown on the page; older ones stay on disk
    port: int = 8765
    restore_folders: bool = True  # copy the program's data folders outside APPDATA, put them back
    pause_on_mouse: bool = True  # someone moved the mouse: the run waits
    keep_awake: bool = True  # no sleep while a run lasts

    def __post_init__(self):
        if not self.program:
            self.program = str(default_program())

    @property
    def program_path(self) -> Path:
        return Path(self.program)

    @property
    def scenarios_path(self) -> Path:
        return Path(self.scenarios)

    @property
    def results_path(self) -> Path:
        return Path(self.results)

    @property
    def settings_presets(self) -> Path:
        return AUTOMAT_DIR / "ustawienia"

    def as_dict(self) -> dict:
        return asdict(self)


def config_file() -> Path:
    return data_dir() / "ustawienia.json"


def load() -> Config:
    path = config_file()
    if not path.exists():
        return Config()
    try:
        stored = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return Config()
    known = {k: v for k, v in stored.items() if k in Config.__dataclass_fields__}
    config = Config(**known)
    config.limits = {**DEFAULT_LIMITS_MS, **(config.limits or {})}
    return config


def save(config: Config) -> None:
    path = config_file()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(config.as_dict(), ensure_ascii=False, indent=2), encoding="utf-8")
