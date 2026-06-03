#!/usr/bin/env python3
"""Apply finalized translations to a Qt .ts file with a minimal, byte-surgical
diff: only the `<translation type="unfinished">...</translation>` blocks change.

Reads from a PRISTINE source copy and writes to a destination, so it is fully
idempotent — re-running with a corrected map always starts from the original
indexing. Document order of unfinished blocks here matches the `i` index emitted
by extract_unfinished.py.

translations JSON may be either:
  * an object  {"0": "Offen", "5": {"forms": ["%n Datei", "%n Dateien"]}, ...}
  * an array   ["Offen", null, ...]            (indexed positionally)
Plain string -> normal translation. {"forms": [...]} -> numerus (plural) entry.
A missing/null index leaves that block untouched (still unfinished).

Usage:  python3 apply_translations.py <src.ts.orig> <translations.json> <dst.ts>
"""
import sys
import re
import json
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape

UNFINISHED = re.compile(r'<translation type="unfinished">(.*?)</translation>', re.DOTALL)


def main() -> None:
    src, trans_path, dst = sys.argv[1], sys.argv[2], sys.argv[3]
    trans = json.load(open(trans_path, encoding="utf-8"))

    def lookup(i):
        if isinstance(trans, list):
            return trans[i] if i < len(trans) else None
        return trans.get(str(i), trans.get(i))

    text = open(src, encoding="utf-8").read()
    state = {"i": 0, "applied": 0, "skipped": []}

    def repl(_m):
        i = state["i"]
        state["i"] += 1
        t = lookup(i)
        if t is None or t == "":
            state["skipped"].append(i)
            return _m.group(0)  # leave unfinished
        if isinstance(t, dict) and "forms" in t:
            inner = "".join(
                "<numerusform>%s</numerusform>" % escape(f) for f in t["forms"]
            )
            state["applied"] += 1
            return "<translation>%s</translation>" % inner
        if isinstance(t, dict):
            t = t.get("text", "")
        state["applied"] += 1
        return "<translation>%s</translation>" % escape(t)

    newtext, n_matches = UNFINISHED.subn(repl, text)

    # Validate XML before writing anything.
    try:
        ET.fromstring(newtext)
    except ET.ParseError as e:
        print(json.dumps({"error": "INVALID_XML", "detail": str(e)}))
        sys.exit(2)

    open(dst, "w", encoding="utf-8").write(newtext)
    remaining = newtext.count('type="unfinished"')
    print(
        json.dumps(
            {
                "blocks_found": n_matches,
                "applied": state["applied"],
                "skipped_indices": state["skipped"],
                "remaining_unfinished": remaining,
                "valid_xml": True,
            },
            ensure_ascii=False,
        )
    )


if __name__ == "__main__":
    main()
