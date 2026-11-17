"""Print a finished run into the job log: every step with its checks, and the UI Automation
trees the run saved (what the automaton saw in the windows). On GitHub the log is the one
place that can always be read; the artifact with screenshots may be out of reach.

    python ci_report.py <results folder>
"""

import json
import sys
from pathlib import Path

TREE_LINES = 260


def walk(node, depth=0, out=None):
    out = [] if out is None else out
    name = node.get("name", "")
    extra = []
    if node.get("description"):
        extra.append(f"opis={node['description']!r}")
    for key in ("toggle", "value"):
        if key in node:
            extra.append(f"{key}={node[key]!r}")
    if not node.get("enabled", True):
        extra.append("niedostępny")
    out.append(f"{'  ' * depth}{node.get('type')} {name!r} {node.get('rect')} {' '.join(extra)}".rstrip())
    for child in node.get("children", []):
        walk(child, depth + 1, out)
    return out


def main(folder: Path) -> int:
    if not folder.is_dir():
        print("Brak folderu wyników", folder)
        return 1
    runs = sorted((p for p in folder.iterdir() if (p / "przebieg.json").is_file()), reverse=True)
    if not runs:
        print("Brak przebiegów w", folder)
        return 1
    run_dir = runs[0]
    run = json.loads((run_dir / "przebieg.json").read_text(encoding="utf-8"))
    print(f"Przebieg {run['id']} – {run.get('where')}, {run.get('duration_ms')} ms")
    if run.get("error"):
        print("BŁĄD AUTOMATU:", run["error"])
        print(run.get("trace", ""))
    for line in run.get("your_data", []):
        print("Twoje dane:", line)
    for result in run["scenarios"]:
        print(f"\n=== {result.get('title', result['id'])}: {result['state']}"
              + (f" – {result['reason']}" if result.get("reason") else ""))
        for warning in result.get("warnings", []):
            print("  UWAGA:", warning)
        for step in result.get("steps", []):
            action = step.get("action", {})
            note = f" [{action['note']}]" if action.get("note") else ""
            print(f"  {step['index']:2}. {'ok ' if step['ok'] else 'BŁĄD'} {step['say']}{note}"
                  f" (zajęty najdłużej {step.get('busy_ms')} ms)")
            if action.get("detail"):
                print(f"        ✗ {action['detail']}")
            if action.get("element"):
                print(f"        element: {action['element']}")
            for check in step.get("checks", []):
                mark = "✓" if check.get("ok") else "✗"
                print(f"        {mark} {check.get('say')}: {check.get('ms')} ms (limit {check.get('limit_ms')})"
                      f" – {check.get('detail')}")
        if result.get("trace"):
            print(result["trace"])
    seen = set()
    for tree in sorted(run_dir.rglob("drzewo-*.json")):
        name = tree.name
        if name in seen and not name.startswith(("drzewo-po-", "drzewo-po-niezgodnosci")):
            continue  # a window that looks the same in every scenario: once is enough
        seen.add(name)
        lines = []
        for root in json.loads(tree.read_text(encoding="utf-8")):
            walk(root, 0, lines)
        print(f"\n--- {tree.relative_to(run_dir)} ({len(lines)} elementów, pierwsze {TREE_LINES}) ---")
        print("\n".join(lines[:TREE_LINES]))
    failed = [r["id"] for r in run["scenarios"] if r.get("state") in ("fail", "error")]
    logs = [run_dir / i / "kalahari.log" for i in failed if (run_dir / i / "kalahari.log").is_file()][:3]
    for log in logs or sorted(run_dir.rglob("kalahari.log"))[:1]:
        text = log.read_text(encoding="utf-8", errors="replace").splitlines()
        print(f"\n--- {log.relative_to(run_dir)} (ostatnie 60 wierszy) ---")
        print("\n".join(text[-60:]))
    return 0


if __name__ == "__main__":
    sys.exit(main(Path(sys.argv[1] if len(sys.argv) > 1 else "automat-wyniki")))
