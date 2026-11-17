"""Gather the operations the program must do from the user's test lists into operacje.json.

Every test list (przeglad/testy/*.md, without the results and the notes) says what to check:
the items of each "**Co sprawdzić:**" paragraph and, in the older lists, each numbered step.
Each of them becomes one operation with a stable id; scenarios name the operations they
check in "covers", and the page shows the operations no scenario checks yet.

    python pokrycie/zbierz_operacje.py <folder with the test lists>
"""

import hashlib
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SKIP = ("wyniki", "uwagi", "przewodnik", "notatka")


def _clean(text: str) -> str:
    text = re.sub(r"\s+", " ", text).strip()
    text = re.sub(r"\*\*|`", "", text)
    return text.strip(" .;")


def _items_of_paragraph(text: str) -> list:
    """Split "a; b (c; d); e" at the semicolons outside brackets."""
    out, depth, current = [], 0, ""
    for char in text:
        if char in "([":
            depth += 1
        elif char in ")]":
            depth = max(0, depth - 1)
        if char == ";" and depth == 0:
            out.append(current)
            current = ""
        else:
            current += char
    out.append(current)
    return [_clean(i) for i in out if _clean(i)]


def gather(folder: Path) -> list:
    operations = []
    seen = set()
    for path in sorted(folder.glob("*.md")):
        if any(word in path.stem for word in SKIP):
            continue
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        title = next((line[2:].strip() for line in lines if line.startswith("# ")), path.stem)
        has_checks = any("**Co sprawdzić:**" in line for line in lines)
        part = section = ""
        for line in lines:
            if line.startswith("## "):
                part, section = line[3:].strip(), ""
            elif line.startswith("### "):
                section = line[4:].strip()
            if has_checks:
                match = re.match(r"\*\*Co sprawdzić:\*\*\s*(.+)", line)
                items = _items_of_paragraph(match.group(1)) if match else []
            else:
                match = re.match(r"(\d+)\.\s+(.+)", line)
                items = [_clean(match.group(2))] if match else []
            for item in items:
                key = re.sub(r"\W+", " ", item.lower()).strip()
                if key in seen:
                    continue
                seen.add(key)
                where = " – ".join(x for x in (part, section) if x)
                digest = hashlib.sha1(f"{path.name}|{where}|{item}".encode("utf-8")).hexdigest()[:8]
                operations.append({"id": f"op-{digest}", "list": path.name, "list_title": title,
                                   "where": where, "text": item})
    return operations


def main(folder: Path) -> int:
    operations = gather(folder)
    out = HERE / "operacje.json"
    out.write_text(json.dumps({"source": "przeglad/testy (listy testów)", "operations": operations},
                              ensure_ascii=False, indent=1), encoding="utf-8")
    lists = sorted({o["list"] for o in operations})
    print(f"{len(operations)} operacji z {len(lists)} list -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(Path(sys.argv[1])))
