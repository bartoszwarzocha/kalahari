"""What the scenarios cover, and what they do not.

Two views, both only for finding gaps, never as a source of expectations:

- Operations: every item of the user's test lists (pokrycie/operacje.json, gathered by
  pokrycie/zbierz_operacje.py) with the scenarios that name it in "covers". An operation
  without a scenario is a gap.
- Elements: what the automaton saw in the program's real windows during the newest runs
  (menu items, buttons of toolbars and panels, fields of dialogs, Settings pages; each
  session writes them to elementy.json), against what the scenarios act on (their targets,
  menu paths and chosen items, and the elements the runs actually clicked). An element no
  scenario acts on is a gap.
"""

import fnmatch
import json
import unicodedata
from pathlib import Path

from automat.results import Store

AUTOMAT_DIR = Path(__file__).resolve().parent.parent
OPERATIONS_FILE = AUTOMAT_DIR / "pokrycie" / "operacje.json"
ELEMENTS_FILE = "elementy.json"
# Kinds of elements a person acts on; the rest (texts, groups, panes) only show things
ACTIVE = {"Button", "CheckBox", "RadioButton", "ComboBox", "Edit", "Spinner", "Slider", "TabItem",
          "MenuItem", "Hyperlink", "SplitButton", "TreeItem", "ListItem", "DataItem"}
# What a scenario did with an element: named it in a step with expectations, clicked it in a
# step, only went through it in a sweep, or nothing (a gap)
STATES = ("checked", "clicked", "swept", "untouched")
ROLE_KINDS = {
    "button": {"Button", "SplitButton"}, "check_box": {"CheckBox"}, "radio": {"RadioButton"},
    "combo_box": {"ComboBox"}, "edit": {"Edit", "Document"}, "document": {"Document", "Edit"},
    "list": {"List"}, "list_item": {"ListItem", "DataItem"}, "tree": {"Tree"}, "tree_item": {"TreeItem"},
    "tab": {"TabItem", "Tab"}, "menu_item": {"MenuItem"}, "menu": {"Menu"}, "text": {"Text"},
    "slider": {"Slider"}, "spinner": {"Spinner"}, "group": {"Group"}, "window": {"Window"},
    "toolbar": {"ToolBar"}, "cell": {"DataItem"}, "header": {"Header", "HeaderItem"},
}


def norm(text) -> str:
    """A name as a person reads it (as automat.win.kalahari.norm): no '&' of access keys,
    '…' and '...' alike, spaces and case not counted, the shortcut after a tab and the colon
    after a label left out."""
    text = unicodedata.normalize("NFC", str(text or "")).split("\t")[0]
    text = text.replace("&&", "\0").replace("&", "").replace("\0", "&")
    text = text.replace("…", "...")
    return " ".join(text.split()).casefold().rstrip(":").rstrip()


def load_operations(path: Path = OPERATIONS_FILE) -> list:
    try:
        return json.loads(Path(path).read_text(encoding="utf-8")).get("operations", [])
    except (OSError, ValueError):
        return []


# --- What the scenarios act on ----------------------------------------------------------------

def _targets_of_step(step: dict) -> list:
    """[(window pattern or None, kinds or None, name)] of what one step acts on."""
    out = []
    action = step.get("do")
    if action == "menu":
        path = [str(p) for p in step.get("path", [])]
        for depth in range(1, len(path) + 1):
            out.append((None, {"MenuItem"}, " > ".join(path[:depth])))
    for key in ("target", "to"):
        target = step.get(key)
        if not isinstance(target, dict):
            continue
        if "name" in target:
            kinds = ROLE_KINDS.get(target.get("role")) if target.get("role") else None
            out.append((target.get("window"), kinds, str(target["name"])))
        if "row" in target:
            out.append((target.get("window"), None, str(target["row"])))
    if action == "select" and step.get("item") is not None:
        target = step.get("target") or {}
        window = target.get("window") if isinstance(target, dict) else None
        out.append((window, None, str(step["item"])))
    return out


def scenario_targets(library) -> list:
    out = []
    for scenario in library.list():
        for step in scenario.steps:
            for window, kinds, name in _targets_of_step(step):
                out.append({"window": window, "kinds": kinds, "name": norm(name), "scenario": scenario.id})
    return out


# --- What the runs saw and clicked -------------------------------------------------------------

def _run_elements(store: Store, runs: int) -> tuple:
    """(seen, touched, run ids) from the elementy.json files of the newest runs."""
    seen, touched, used = {}, [], []
    for run_id in store.run_ids()[:runs]:
        folder = store.run_dir(run_id)
        files = sorted(folder.rglob(ELEMENTS_FILE))
        if not files:
            continue
        used.append(run_id)
        for path in files:
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                continue
            for element in data.get("seen", []):
                key = (element.get("window", ""), element.get("type", ""), norm(element.get("name")))
                if key not in seen:
                    seen[key] = dict(element, run=run_id)
            touched += data.get("touched", [])
    return seen, touched, used


def _matches(element: dict, target: dict) -> bool:
    if target["name"] != norm(element.get("name")):
        return False
    if target.get("kinds") and element.get("type") not in target["kinds"]:
        return False
    window = target.get("window")
    if window and window not in ("*Kalahari",):
        title = element.get("window_title") or element.get("window") or ""
        if not (fnmatch.fnmatchcase(norm(title), norm(window)) or norm(title) == norm(window)):
            return False
    return True


def elements(library, store: Store, runs: int = 10) -> dict:
    seen, touched, used = _run_elements(store, runs)
    targets = scenario_targets(library)
    clicked = {(t.get("window", ""), t.get("type", ""), norm(t.get("name"))) for t in touched if not t.get("sweep")}
    swept = {(t.get("window", ""), t.get("type", ""), norm(t.get("name"))) for t in touched if t.get("sweep")}
    by_name = {}
    for target in targets:
        by_name.setdefault(target["name"], []).append(target)
    out = []
    for key, element in seen.items():
        if element.get("type") not in ACTIVE:
            continue
        name = norm(element.get("name"))
        scenarios = sorted({t["scenario"] for t in by_name.get(name, []) if _matches(element, t)})
        state = ("checked" if scenarios else "clicked" if key in clicked else "swept" if key in swept
                 else "untouched")
        out.append({"window": element.get("window"), "type": element.get("type"), "name": element.get("name"),
                    "path": element.get("path"), "enabled": element.get("enabled", True),
                    "description": element.get("description"), "scenarios": scenarios, "state": state,
                    "run": element.get("run")})
    out.sort(key=lambda e: (e["window"] or "", e["path"] or "", e["name"] or ""))
    counts = {s: sum(1 for e in out if e["state"] == s) for s in STATES}
    return {"runs": used, "total": len(out), "counts": counts, "untouched": counts["untouched"],
            "unnamed": sum(1 for e in out if not e["name"]), "elements": out}


def operations(library, latest: dict, path: Path = OPERATIONS_FILE) -> dict:
    ops = load_operations(path)
    known = {o["id"] for o in ops}
    covering = {}
    unknown = []
    for scenario in library.list():
        for op_id in scenario.data.get("covers", []) or []:
            if op_id in known:
                covering.setdefault(op_id, []).append(scenario.id)
            else:
                unknown.append({"scenario": scenario.id, "id": op_id})
    out = []
    for op in ops:
        ids = covering.get(op["id"], [])
        states = [latest.get(i, {}).get("state", "none") for i in ids]
        out.append(dict(op, scenarios=ids, states=states))
    covered = sum(1 for o in out if o["scenarios"])
    passed = sum(1 for o in out if o["states"] and all(s == "ok" for s in o["states"]))
    return {"total": len(out), "covered": covered, "passed": passed, "unknown": unknown, "operations": out}
