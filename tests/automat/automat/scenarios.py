"""Scenarios: JSON files read fresh from the scenario folder, checked, and shown as sentences.

A file holds one scenario (an object) or several ({"scenarios": [...]} or a list). Every
*.json file under the folder counts, except those in folders whose names start with "_"
(_bloki holds the shared step blocks used by "use"). A new or changed file is picked up
the next time the list is read: nothing in the automaton has to change.
"""

import copy
import hashlib
import json
import re
from dataclasses import dataclass, field
from pathlib import Path

# --- The vocabulary ----------------------------------------------------------------------

ACTIONS = {
    # action: (required fields, optional fields)
    "menu": ({"path"}, {"window"}),
    "click": ({"target"}, {"button", "double", "modifiers", "offset"}),
    "keys": ({"keys"}, {"target"}),
    "type": ({"text"}, {"target", "replace"}),
    "select": ({"target", "item"}, set()),
    "toggle": ({"target"}, {"state"}),
    "drag": ({"target", "to"}, set()),
    "scroll": ({"target", "notches"}, set()),
    "file_dialog": ({"path"}, set()),
    "app": ({"app", "steps"}, {"open"}),
    "restart": (set(), set()),
    "wait": ({"ms"}, set()),
    "screenshot": (set(), {"name", "area"}),
    "use": ({"block"}, {"with"}),
    "sweep": ({"what"}, {"window", "in", "skip", "only", "open", "controls", "target"}),
}
# What a sweep goes through (automat.win.sweep)
SWEEPS = {"menu": "każdą pozycję menu", "context_menu": "każdą pozycję menu podręcznego",
          "toolbars": "każdy przycisk pasków narzędzi", "controls": "każdą kontrolkę", "pages": "każdą stronę"}
# What the automaton can do: every action and check of the vocabulary. "app" steps become
# "_app_open", the other program's steps and "_app_close" (expand_steps).
SUPPORTED_ACTIONS = (set(ACTIONS) - {"app", "use"}) | {"_app_open", "_app_close"}
APPS = {"notatnik": "Notatnik", "eksplorator": "Eksplorator plików", "word": "Word"}
SPECIAL_TARGETS = ("clicked", "editor")  # the element of the last action; the book's editor


def is_special(target) -> bool:
    return isinstance(target, str) and target in SPECIAL_TARGETS

CHECKS = {
    "window_shown": ({"window"}, {"within_ms"}),
    "window_closed": ({"window"}, {"within_ms"}),
    "visible": ({"target"}, {"within_ms"}),
    "not_visible": ({"target"}, {"within_ms"}),
    "enabled": ({"target"}, {"value", "within_ms"}),
    "checked": ({"target"}, {"value", "within_ms"}),
    "selected": ({"target"}, {"value", "within_ms"}),
    "focused": ({"target"}, {"within_ms"}),
    "text": ({"target"}, {"equals", "contains", "within_ms"}),
    "value": ({"target"}, {"equals", "contains", "within_ms"}),
    "editor_text": (set(), {"target", "equals", "contains", "not_contains", "within_ms"}),
    "clipboard": (set(), {"equals", "contains", "html_contains", "within_ms"}),
    "file": ({"path"}, {"contains", "within_ms"}),
    "color": ({"area"}, {"dark", "light", "equals", "tolerance", "within_ms"}),
    "image": ({"area", "pattern"}, {"tolerance", "mask", "within_ms"}),
    "screen_settled": ({"area"}, {"within_ms", "quiet_ms", "must_change"}),
    "busy_at_most_ms": ({"value"}, set()),
    "ocr": ({"area", "contains"}, {"within_ms"}),
}
SUPPORTED_CHECKS = set()  # filled below: all of CHECKS

ROLES = {"button", "check_box", "radio", "combo_box", "edit", "document", "list", "list_item",
         "tree", "tree_item", "tab", "menu_item", "menu", "text", "slider", "spinner", "group",
         "window", "toolbar", "cell", "header"}
TARGET_KEYS = {"window", "name", "role", "row", "column", "part", "index", "in"}
SOURCE_KINDS = {"plan", "mockup", "decision", "test_list"}
STEP_KEYS_COMMON = {"say", "do", "expect", "repeat", "note"}

# Default limits by the kind of check (the user's decision of 17.11.2026: defaults by kind,
# exceptions in scenarios). The automaton's settings may change these numbers.
DEFAULT_LIMITS_MS = {"window": 500, "reaction": 300, "theme": 1000}
WINDOW_CHECKS = {"window_shown", "window_closed"}
THEME_CHECKS = {"color"}
# What a file, a picture or a text on the screen holds is checked without a default time
# limit: only "within_ms" of the scenario gives one
UNTIMED_CHECKS = {"file", "image", "ocr"}
SUPPORTED_CHECKS.update(CHECKS)


def default_limit_kind(check: str) -> str:
    if check in WINDOW_CHECKS:
        return "window"
    if check in THEME_CHECKS:
        return "theme"
    return "reaction"


@dataclass
class Problem:
    where: str
    message: str

    def __str__(self) -> str:
        return f"{self.where}: {self.message}" if self.where else self.message


@dataclass
class Scenario:
    data: dict
    file: Path
    folder: Path
    problems: list = field(default_factory=list)
    steps: list = field(default_factory=list)  # with "use" blocks expanded

    @property
    def id(self) -> str:
        return self.data.get("id", "")

    @property
    def title(self) -> str:
        return self.data.get("title", self.id)

    @property
    def area(self) -> str:
        return self.data.get("area", "Inne")

    @property
    def tags(self) -> list:
        return list(self.data.get("tags", []))

    @property
    def valid(self) -> bool:
        return not self.problems

    def unsupported(self) -> list:
        """Actions and checks the automaton cannot do yet (the scenario is skipped)."""
        missing = []
        for step in self.steps:
            if step.get("do") not in SUPPORTED_ACTIONS:
                missing.append(f"działanie „{step.get('do')}”")
            for check in step.get("expect", []):
                if check.get("check") not in SUPPORTED_CHECKS:
                    missing.append(f"sprawdzenie „{check.get('check')}”")
        return sorted(set(missing))

    def expectations_hash(self) -> str:
        """What the user approves: the sources, the setup and the steps with their expected
        effects and limits. A change of any of them withdraws the approval; a change of the
        title or of the tags does not."""
        relevant = {"sources": self.data.get("sources", []), "setup": self.data.get("setup", {}),
                    "steps": self.steps}
        text = json.dumps(relevant, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        return hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]

    def summary(self, limits: dict = None) -> dict:
        return {
            "id": self.id, "title": self.title, "area": self.area, "tags": self.tags,
            "file": self.file.relative_to(self.folder).as_posix(),
            "sources": self.data.get("sources", []), "setup": self.data.get("setup", {}),
            "steps": [step_view(s, limits) for s in self.steps],
            "problems": [str(p) for p in self.problems], "unsupported": self.unsupported(),
            "hash": self.expectations_hash() if self.valid else None,
            "measures_time": any(_has_time(s) for s in self.steps),
            "covers": list(self.data.get("covers", []) or []),
        }


def _has_time(step: dict) -> bool:
    return any(c.get("check") in ("busy_at_most_ms", "screen_settled") or "within_ms" in c
               for c in step.get("expect", []))


# --- Reading -----------------------------------------------------------------------------

class Library:
    """The scenario folder. list() reads it again; unchanged files come from a cache."""

    def __init__(self, folder: Path):
        self.folder = Path(folder)
        self._cache = {}  # path -> (mtime_ns, size, [Scenario] or problem)

    def files(self) -> list:
        if not self.folder.is_dir():
            return []
        out = []
        for path in sorted(self.folder.rglob("*.json")):
            relative = path.relative_to(self.folder)
            if any(part.startswith("_") or part.startswith(".") for part in relative.parts[:-1]):
                continue
            out.append(path)
        return out

    def blocks(self) -> dict:
        found = {}
        for path in sorted(self.folder.glob("_bloki/*.json")):
            try:
                found[path.stem] = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, ValueError) as error:
                found[path.stem] = {"_error": str(error)}
        return found

    def list(self) -> list:
        """All scenarios, in file order, each checked; duplicate ids are problems."""
        blocks = self.blocks()
        block_stamp = tuple((p.name, p.stat().st_mtime_ns) for p in sorted(self.folder.glob("_bloki/*.json")))
        scenarios = []
        seen = set()
        live = set()
        for path in self.files():
            live.add(path)
            try:
                stat = path.stat()
            except OSError:
                continue
            stamp = (stat.st_mtime_ns, stat.st_size, block_stamp)
            cached = self._cache.get(path)
            if cached and cached[0] == stamp:
                loaded = cached[1]
            else:
                loaded = self._load(path, blocks)
                self._cache[path] = (stamp, loaded)
            for scenario in loaded:
                scenario = copy.copy(scenario)
                scenario.problems = list(scenario.problems)
                if scenario.id in seen:
                    scenario.problems.append(Problem("id", f"„{scenario.id}” jest też w innym pliku"))
                seen.add(scenario.id)
                scenarios.append(scenario)
        for gone in set(self._cache) - live:
            del self._cache[gone]
        return scenarios

    def get(self, scenario_id: str):
        for scenario in self.list():
            if scenario.id == scenario_id:
                return scenario
        return None

    def _load(self, path: Path, blocks: dict) -> list:
        try:
            raw = json.loads(path.read_text(encoding="utf-8"))
        except ValueError as error:
            bad = Scenario({"id": path.stem, "title": path.name}, path, self.folder)
            bad.problems.append(Problem("plik", f"to nie jest poprawny JSON: {error}"))
            return [bad]
        except OSError as error:
            bad = Scenario({"id": path.stem, "title": path.name}, path, self.folder)
            bad.problems.append(Problem("plik", f"nie da się odczytać: {error}"))
            return [bad]
        if isinstance(raw, dict) and "scenarios" in raw:
            items = raw["scenarios"]
        elif isinstance(raw, list):
            items = raw
        else:
            items = [raw]
        out = []
        for index, item in enumerate(items):
            if not isinstance(item, dict):
                bad = Scenario({"id": f"{path.stem}#{index + 1}"}, path, self.folder)
                bad.problems.append(Problem("plik", f"pozycja {index + 1} nie jest obiektem"))
                out.append(bad)
                continue
            if "id" not in item and len(items) == 1:
                item = dict(item, id=path.stem)
            scenario = Scenario(item, path, self.folder)
            scenario.steps, problems = expand_steps(item.get("steps"), blocks)
            scenario.problems = validate(item) + problems
            out.append(scenario)
        return out


def expand_steps(steps, blocks: dict, depth: int = 0):
    """Replace every {"do": "use", "block": name, "with": {...}} by the block's steps, with
    {{name}} placeholders filled from "with"."""
    problems = []
    if not isinstance(steps, list):
        return [], problems
    out = []
    for number, step in enumerate(steps, 1):
        if isinstance(step, dict) and step.get("do") == "app":
            inner, inner_problems = expand_steps(step.get("steps"), blocks, depth + 1)
            problems += inner_problems
            key = step.get("app")
            opening = {"do": "_app_open", "app": key, "_app": key}
            if "open" in step:
                opening["open"] = step["open"]
            if step.get("say"):
                opening["say"] = step["say"]
            out.append(opening)
            for inner_step in inner:
                out.append(dict(inner_step, _app=key))
            closing = {"do": "_app_close", "app": key}
            if "expect" in step:
                closing["expect"] = list(step["expect"])
            out.append(closing)
            continue
        if not isinstance(step, dict) or step.get("do") != "use":
            out.append(step)
            continue
        name = step.get("block")
        block = blocks.get(name)
        if block is None:
            problems.append(Problem(f"krok {number}", f"nie ma bloku „{name}” w folderze _bloki"))
            continue
        if "_error" in block:
            problems.append(Problem(f"blok {name}", f"błąd pliku: {block['_error']}"))
            continue
        if depth > 4:
            problems.append(Problem(f"blok {name}", "bloki zagnieżdżone zbyt głęboko"))
            continue
        values = step.get("with", {})
        filled = _fill(copy.deepcopy(block.get("steps", [])), values, problems, name)
        inner, inner_problems = expand_steps(filled, blocks, depth + 1)
        problems += inner_problems
        for inner_step in inner:
            inner_step = dict(inner_step)
            inner_step.setdefault("_block", name)
            out.append(inner_step)
        if "expect" in step:  # the use step's own expectations go with the block's last step
            if out:
                out[-1] = dict(out[-1], expect=list(out[-1].get("expect", [])) + list(step["expect"]))
    return out, problems


_PLACEHOLDER = re.compile(r"\{\{(\w+)\}\}")


def _fill(value, values: dict, problems: list, block: str):
    if isinstance(value, str):
        def replace(match):
            key = match.group(1)
            if key not in values:
                problems.append(Problem(f"blok {block}", f"brak wartości „{key}” w „with”"))
                return match.group(0)
            return str(values[key])
        return _PLACEHOLDER.sub(replace, value)
    if isinstance(value, list):
        return [_fill(v, values, problems, block) for v in value]
    if isinstance(value, dict):
        return {k: _fill(v, values, problems, block) for k, v in value.items()}
    return value


# --- Checking a scenario -----------------------------------------------------------------

def validate(data: dict) -> list:
    problems = []

    def need(condition, where, message):
        if not condition:
            problems.append(Problem(where, message))

    need(isinstance(data.get("id"), str) and re.fullmatch(r"[a-z0-9][a-z0-9-]*", data.get("id", "")),
         "id", "małe litery, cyfry i łączniki, np. „znaki-specjalne-ulubione”")
    need(isinstance(data.get("title"), str) and data.get("title"), "title", "brak tytułu")
    need(isinstance(data.get("tags", []), list), "tags", "znaczniki to lista napisów")
    covers = data.get("covers", [])
    need(isinstance(covers, list) and all(isinstance(c, str) and c.startswith("op-") for c in covers), "covers",
         "„covers” to lista numerów operacji ze spisu (pokrycie/operacje.json), np. „op-1a2b3c4d”")
    sources = data.get("sources")
    need(isinstance(sources, list) and sources, "sources",
         "każdy scenariusz potrzebuje źródła oczekiwań: planu, makiety albo decyzji")
    for i, source in enumerate(sources or [], 1):
        need(isinstance(source, dict) and source.get("kind") in SOURCE_KINDS and source.get("ref"),
             f"źródło {i}", "potrzebne „kind” (plan, mockup, decision albo test_list) i „ref”")
    setup = data.get("setup", {})
    need(isinstance(setup, dict), "setup", "to musi być obiekt")
    if isinstance(setup, dict):
        need(isinstance(setup.get("books", []), list), "setup.books", "lista książek z zestawu")
        opened = setup.get("open")
        if opened is not None:
            need(isinstance(opened, dict) and opened.get("book") in setup.get("books", []),
                 "setup.open", "książka do otwarcia musi być na liście „books”")
    steps = data.get("steps")
    need(isinstance(steps, list) and steps, "steps", "scenariusz potrzebuje kroków")
    for number, step in enumerate(steps or [], 1):
        problems += validate_step(step, f"krok {number}")
    return problems


def validate_step(step, where: str) -> list:
    problems = []
    if not isinstance(step, dict):
        return [Problem(where, "krok to obiekt z polem „do”")]
    action = step.get("do")
    if action not in ACTIONS:
        return [Problem(where, f"nieznane działanie „{action}”; znane: {', '.join(sorted(ACTIONS))}")]
    required, optional = ACTIONS[action]
    for key in sorted(required - set(step)):
        problems.append(Problem(where, f"działanie „{action}” potrzebuje pola „{key}”"))
    for key in sorted(set(step) - required - optional - STEP_KEYS_COMMON - {"block", "with"}):
        if not key.startswith("_"):
            problems.append(Problem(where, f"nieznane pole „{key}”"))
    if action == "menu" and not (isinstance(step.get("path"), list) and step.get("path")):
        problems.append(Problem(where, "„path” to lista pozycji menu, np. [\"Wstaw\", \"Znak specjalny...\"]"))
    if "target" in step and not is_special(step["target"]):
        problems += validate_target(step["target"], where)
    if action == "drag" and "to" in step:
        to = step["to"]
        if isinstance(to, dict) and set(to) == {"offset"}:
            if not (isinstance(to["offset"], list) and len(to["offset"]) == 2):
                problems.append(Problem(where, "„to”: {\"offset\": [x, y]} – przesunięcie w pikselach"))
        elif not is_special(to):
            problems += validate_target(to, where)
    if action == "app":
        if step.get("app") not in APPS:
            problems.append(Problem(where, f"nieznany program „{step.get('app')}”; znane: {', '.join(APPS)}"))
        inner = step.get("steps")
        if not (isinstance(inner, list) and inner):
            problems.append(Problem(where, "„steps” to lista kroków w tym programie"))
        else:
            for number, inner_step in enumerate(inner, 1):
                if isinstance(inner_step, dict) and inner_step.get("do") in ("app", "restart"):
                    problems.append(Problem(f"{where}, krok {number}", "w innym programie nie ma tego działania"))
                    continue
                problems += validate_step(inner_step, f"{where}, krok {number} w programie")
    if action == "sweep":
        problems += _validate_sweep(step, where)
    if "repeat" in step and not (isinstance(step["repeat"], int) and 1 <= step["repeat"] <= 50):
        problems.append(Problem(where, "„repeat” to liczba powtórzeń pomiaru od 1 do 50"))
    expects = step.get("expect", [])
    if not isinstance(expects, list):
        return problems + [Problem(where, "„expect” to lista sprawdzeń")]
    for i, check in enumerate(expects, 1):
        problems += validate_check(check, f"{where}, sprawdzenie {i}")
    return problems


def _validate_sweep(step: dict, where: str) -> list:
    problems = []
    what = step.get("what")
    if what not in SWEEPS:
        problems.append(Problem(where, f"„what” to jedno z: {', '.join(SWEEPS)}"))
    if what in ("controls", "pages") and not isinstance(step.get("window"), str):
        problems.append(Problem(where, f"przegląd „{what}” potrzebuje okna („window”)"))
    if what == "context_menu" and not (isinstance(step.get("target"), dict) or step.get("target") == "editor"):
        problems.append(Problem(where, "przegląd menu podręcznego potrzebuje elementu („target”), na którym klika prawym przyciskiem"))
    for key in ("skip", "only"):
        value = step.get(key, [])
        if not (isinstance(value, list) and all(isinstance(v, str) for v in value)):
            problems.append(Problem(where, f"„{key}” to lista nazw (wzorce z * i ?)"))
    opened = step.get("open", [])
    if not isinstance(opened, list):
        problems.append(Problem(where, "„open” to lista kroków, które otwierają okno"))
    else:
        for number, inner in enumerate(opened, 1):
            if isinstance(inner, dict) and inner.get("do") in ("sweep", "app", "use", "restart"):
                problems.append(Problem(f"{where}, krok otwarcia {number}", "tu nie ma tego działania"))
                continue
            problems += validate_step(inner, f"{where}, krok otwarcia {number}")
    return problems


def validate_check(check, where: str) -> list:
    if not isinstance(check, dict) or check.get("check") not in CHECKS:
        name = check.get("check") if isinstance(check, dict) else check
        return [Problem(where, f"nieznane sprawdzenie „{name}”; znane: {', '.join(sorted(CHECKS))}")]
    problems = []
    required, optional = CHECKS[check["check"]]
    for key in sorted(required - set(check)):
        problems.append(Problem(where, f"sprawdzenie „{check['check']}” potrzebuje pola „{key}”"))
    for key in sorted(set(check) - required - optional - {"check", "note"}):
        problems.append(Problem(where, f"nieznane pole „{key}”"))
    if "within_ms" in check and not (isinstance(check["within_ms"], int) and check["within_ms"] > 0):
        problems.append(Problem(where, "„within_ms” to liczba milisekund większa od zera"))
    target = check.get("target")
    if target is not None and not is_special(target):
        problems += validate_target(target, where)
    area = check.get("area")
    if area is not None and not is_special(area):
        problems += validate_target(area, where)
    if check.get("check") == "image":
        if not (isinstance(check.get("pattern"), str) and re.fullmatch(r"[a-z0-9][a-z0-9-]*", check.get("pattern", ""))):
            problems.append(Problem(where, "„pattern” to nazwa wzorca: małe litery, cyfry i łączniki"))
        for rect in check.get("mask", []) if isinstance(check.get("mask", []), list) else [None]:
            if not (isinstance(rect, list) and len(rect) == 4 and all(isinstance(v, int) for v in rect)):
                problems.append(Problem(where, "„mask” to lista prostokątów [x, y, szerokość, wysokość]"))
                break
    return problems


def validate_target(target, where: str) -> list:
    if not isinstance(target, dict):
        return [Problem(where, "element to obiekt, np. {\"window\": \"Znak specjalny\", \"name\": \"Lista\"}")]
    problems = []
    for key in sorted(set(target) - TARGET_KEYS):
        problems.append(Problem(where, f"nieznane pole elementu „{key}”"))
    if "role" in target and target["role"] not in ROLES:
        problems.append(Problem(where, f"nieznany rodzaj „{target['role']}”; znane: {', '.join(sorted(ROLES))}"))
    if not ({"name", "row", "window"} & set(target)):
        problems.append(Problem(where, "element potrzebuje co najmniej okna, napisu albo wiersza"))
    if ("column" in target or "part" in target) and "row" not in target:
        problems.append(Problem(where, "„column” i „part” wskazują element w wierszu: potrzebne też „row”"))
    return problems


# --- Sentences for the page --------------------------------------------------------------

def _q(text) -> str:
    return f"„{text}”"


def target_text(target) -> str:
    if target == "clicked":
        return "kliknięty element"
    if target == "editor":
        return "edytor"
    if not isinstance(target, dict):
        return "?"
    if "row" in target:
        where = f"wiersz {_q(target['row'])}"
        if "column" in target:
            where = f"komórka w wierszu {_q(target['row'])} i kolumnie {_q(target['column'])}"
        if "part" in target:
            where = f"{_q(target['part'])} w wierszu {_q(target['row'])}"
        if "name" in target:
            where += f" listy {_q(target['name'])}"
        return where
    if "name" in target:
        return _q(target["name"])
    return f"okno {_q(target.get('window'))}"


def _agree(adjective: str, target) -> str:
    """The adjective in the gender of the noun target_text() starts with: "okno" is neuter,
    "komórka" feminine, the rest (a row, a quoted name) masculine."""
    if isinstance(target, dict):
        if "row" in target and "column" in target and "part" not in target:
            return adjective[:-1] + "a"
        if not ({"row", "name"} & set(target)):
            return adjective[:-1] + "e"
    return adjective


def action_sentence(step: dict) -> str:
    sentence = _action_sentence(step)
    if step.get("_app") and step.get("do") not in ("_app_open", "_app_close") and not step.get("say"):
        return f"{APPS.get(step['_app'], step['_app'])}: {sentence[0].lower()}{sentence[1:]}"
    return sentence


def _action_sentence(step: dict) -> str:
    if step.get("say"):
        return step["say"]
    action = step.get("do")
    if action == "menu":
        return f"W menu kliknij {' > '.join(step.get('path', []))}"
    if action == "click":
        kind = {"right": "Kliknij prawym przyciskiem", "middle": "Kliknij środkowym przyciskiem"}.get(
            step.get("button"), "Kliknij dwukrotnie" if step.get("double") else "Kliknij")
        mods = "+".join(step.get("modifiers", []))
        return f"{kind}{' z ' + mods if mods else ''} {target_text(step.get('target'))}"
    if action == "keys":
        return f"Naciśnij {step.get('keys')}"
    if action == "type":
        where = f" w polu {target_text(step['target'])}" if "target" in step else ""
        return f"Napisz {_q(step.get('text'))}{where}"
    if action == "select":
        return f"Wybierz {_q(step.get('item'))} w {target_text(step.get('target'))}"
    if action == "toggle":
        state = {True: "Zaznacz", False: "Odznacz"}.get(step.get("state"), "Przełącz")
        return f"{state} {target_text(step.get('target'))}"
    if action == "scroll":
        return f"Przewiń {target_text(step.get('target'))} o {step.get('notches')}"
    if action == "drag":
        to = step.get("to")
        if isinstance(to, dict) and set(to) == {"offset"}:
            return f"Przeciągnij {target_text(step.get('target'))} o {to['offset'][0]}, {to['offset'][1]} px"
        return f"Przeciągnij {target_text(step.get('target'))} na {target_text(to)}"
    if action == "sweep":
        what = SWEEPS.get(step.get("what"), str(step.get("what")))
        window = step.get("window")
        where = " w oknie książki" if window == "*Kalahari" else f" w oknie {_q(window)}" if window else ""
        inside = f", w części {_q(step['in'])}" if step.get("in") else ""
        extra = " i każdą kontrolkę na niej" if step.get("controls") else ""
        on = f" {target_text(step['target'])}" if step.get("what") == "context_menu" and step.get("target") else ""
        return f"Przejdź przez {what}{on}{extra}{where}{inside} i sprawdź, czy każda reaguje bez błędu"
    if action == "restart":
        return "Zamknij program i uruchom go ponownie"
    if action == "wait":
        return f"Odczekaj {step.get('ms')} ms"
    if action == "screenshot":
        return "Zrób zrzut ekranu"
    if action == "file_dialog":
        return f"W systemowym oknie wyboru pliku wpisz {_q(step.get('path'))} i zatwierdź"
    if action == "app":
        return f"W programie {APPS.get(step.get('app'), step.get('app'))}: {len(step.get('steps', []))} kroków"
    if action == "_app_open":
        where = f" z {_q(step['open'])}" if step.get("open") else ""
        return f"Uruchom program {APPS.get(step.get('app'), step.get('app'))}{where}"
    if action == "_app_close":
        return f"Zamknij program {APPS.get(step.get('app'), step.get('app'))}"
    return str(action)


def check_sentence(check: dict) -> str:
    kind = check.get("check")
    target = check.get("target")
    if kind == "window_shown":
        return f"okno {_q(check.get('window'))} pojawia się"
    if kind == "window_closed":
        return f"okno {_q(check.get('window'))} znika"
    if kind == "visible":
        return f"{target_text(target)} {_agree('widoczny', target)}"
    if kind == "not_visible":
        return f"{target_text(target)} {_agree('niewidoczny', target)}"
    if kind == "enabled":
        return f"{target_text(target)} {_agree('dostępny' if check.get('value', True) else 'niedostępny', target)}"
    if kind == "checked":
        return f"{target_text(target)} {_agree('zaznaczony' if check.get('value', True) else 'niezaznaczony', target)}"
    if kind == "selected":
        return f"{target_text(target)} {_agree('wybrany' if check.get('value', True) else 'niewybrany', target)}"
    if kind == "focused":
        return f"{target_text(target)} ma fokus"
    if kind in ("text", "value"):
        if "equals" in check:
            return f"{target_text(target)} ma tekst {_q(check['equals'])}"
        return f"{target_text(target)} zawiera {_q(check.get('contains'))}"
    if kind == "editor_text":
        if "equals" in check:
            return f"tekst w edytorze to {_q(check['equals'])}"
        if "not_contains" in check:
            return f"tekst w edytorze nie zawiera {_q(check['not_contains'])}"
        return f"tekst w edytorze zawiera {_q(check.get('contains'))}"
    if kind == "clipboard":
        if "html_contains" in check:
            return f"w schowku HTML z {_q(check['html_contains'])}"
        return f"w schowku {_q(check.get('equals', check.get('contains')))}"
    if kind == "file":
        return f"plik {_q(check.get('path'))} istnieje" + (f" i zawiera {_q(check['contains'])}" if "contains" in check else "")
    if kind == "color":
        tone = "ciemne" if check.get("dark") else "jasne" if check.get("light") else f"w kolorze {check.get('equals')}"
        return f"tło {target_text(check.get('area'))} {tone}"
    if kind == "image":
        return f"{target_text(check.get('area'))} wygląda jak zatwierdzony wzorzec {_q(check.get('pattern'))}"
    if kind == "screen_settled":
        area = check.get("area")
        change = " (po zmianie)" if check.get("must_change") else ""
        return f"{target_text(area)} {_agree('gotowy', area)} na ekranie{change}"
    if kind == "busy_at_most_ms":
        return f"program nie jest zajęty dłużej niż {fmt_ms(check.get('value'))}"
    if kind == "ocr":
        return f"na obrazie {target_text(check.get('area'))} napis {_q(check.get('contains'))}"
    return str(kind)


def fmt_ms(ms) -> str:
    if ms is None:
        return "–"
    if ms >= 1000:
        return f"{ms / 1000:.2f}".rstrip("0").rstrip(".").replace(".", ",") + " s"
    return f"{round(ms)} ms"


def limit_of(check: dict, limits: dict = None):
    """The time limit of a check in ms: its own, or the default for its kind; None for checks
    without one."""
    if check.get("check") == "busy_at_most_ms":
        return check.get("value")
    if "within_ms" in check:
        return check["within_ms"]
    if check.get("check") in UNTIMED_CHECKS:
        return None
    limits = limits or DEFAULT_LIMITS_MS
    return limits.get(default_limit_kind(check.get("check", "")))


def step_view(step: dict, limits: dict = None) -> dict:
    return {
        "do": step.get("do"),
        "say": action_sentence(step),
        "expect": [{"check": c.get("check"), "say": check_sentence(c), "limit_ms": limit_of(c, limits),
                    "own_limit": "within_ms" in c} for c in step.get("expect", [])],
        "repeat": step.get("repeat", 1),
        "block": step.get("_block"),
    }
