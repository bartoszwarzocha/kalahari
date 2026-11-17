"""The text of a file the program wrote (an export, a saved chapter), for the "file" check.

Word (.docx), OpenDocument (.odt) and EPUB are ZIP files of XML: their text is the text of
the XML parts, without the tags. PDF goes through pypdf when it is installed. A book's JSON
files (.klh, .kchapter, .json) give all their texts. Anything else is read as UTF-8 text
(or, failing that, as Windows-1250).
"""

import html
import json
import re
import zipfile
from pathlib import Path

_TAG = re.compile(r"<[^>]+>")
_PARAGRAPH_END = re.compile(r"</(w:p|text:p|text:h|p|h[1-6]|li|div|tr)>", re.I)


def _xml_text(markup: str) -> str:
    markup = _PARAGRAPH_END.sub("\n", markup)
    markup = re.sub(r"<w:tab/>|<text:tab/>", "\t", markup)
    markup = re.sub(r"<w:br/>|<text:line-break/>|<br\s*/?>", "\n", markup, flags=re.I)
    return html.unescape(_TAG.sub("", markup))


def _zip_text(path: Path) -> str:
    parts = []
    with zipfile.ZipFile(path) as archive:
        for name in archive.namelist():
            lower = name.lower()
            if lower.endswith((".xml", ".xhtml", ".html", ".htm")) and not lower.startswith("meta-inf/"):
                if lower.endswith(("styles.xml", "settings.xml", "fonttable.xml", "theme1.xml")):
                    continue
                parts.append(_xml_text(archive.read(name).decode("utf-8", errors="replace")))
    return "\n".join(parts)


def _json_texts(value, out: list) -> None:
    if isinstance(value, str):
        out.append(value)
    elif isinstance(value, list):
        for item in value:
            _json_texts(item, out)
    elif isinstance(value, dict):
        for item in value.values():
            _json_texts(item, out)


def text_of(path: Path):
    """(text, None), or (None, why it cannot be read)."""
    path = Path(path)
    suffix = path.suffix.lower()
    try:
        if suffix in (".docx", ".odt", ".epub", ".dotx", ".ott"):
            return _zip_text(path), None
        if suffix == ".pdf":
            try:
                from pypdf import PdfReader  # noqa: PLC0415 - optional
            except ImportError:
                return None, "nie ma pakietu pypdf do odczytu PDF; pip install -r requirements.txt"
            reader = PdfReader(str(path))
            return "\n".join(page.extract_text() or "" for page in reader.pages), None
        raw = path.read_bytes()
        if suffix in (".klh", ".kchapter", ".json"):
            texts = []
            _json_texts(json.loads(raw.decode("utf-8")), texts)
            return "\n".join(_xml_text(t) if "<" in t else t for t in texts), None
        if suffix in (".html", ".htm", ".xhtml", ".xml", ".fodt"):
            return _xml_text(raw.decode("utf-8", errors="replace")), None
        try:
            return raw.decode("utf-8-sig"), None
        except UnicodeDecodeError:
            return raw.decode("cp1250", errors="replace"), None
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        return None, f"nie da się odczytać pliku {path.name}: {error}"
