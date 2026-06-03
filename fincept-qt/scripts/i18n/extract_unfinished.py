#!/usr/bin/env python3
"""Extract every <translation type="unfinished"> entry from a Qt .ts file, in
document order, with its context name, source string, and current draft.

The ordinal index `i` assigned here matches the document-order index used by
apply_translations.py — translate entry `i` and key your output JSON by `i`.

Output is JSONL (one compact JSON object per line) so an agent can read the
whole list in a single pass; the trailing line is a summary object with a
"_total" key.

Usage:  python3 extract_unfinished.py <file.ts>   # prints JSONL to stdout
"""
import sys
import json
import xml.etree.ElementTree as ET


def main() -> None:
    path = sys.argv[1]
    root = ET.parse(path).getroot()
    out = []
    i = 0
    for ctx in root.findall("context"):
        nm = ctx.find("name")
        cname = nm.text if nm is not None and nm.text else ""
        for msg in ctx.findall("message"):
            tr = msg.find("translation")
            if tr is None or tr.get("type") != "unfinished":
                continue
            src = msg.find("source")
            srctext = src.text if src is not None and src.text else ""
            numerus = msg.get("numerus") == "yes" or tr.find("numerusform") is not None
            if numerus:
                draft = [(nf.text or "") for nf in tr.findall("numerusform")]
            else:
                draft = tr.text or ""
            cmt = msg.find("comment")
            entry = {
                "i": i,
                "context": cname,
                "source": srctext,
                "numerus": numerus,
                "draft": draft,
            }
            if cmt is not None and cmt.text:
                entry["comment"] = cmt.text
            out.append(entry)
            i += 1
    for entry in out:
        sys.stdout.write(json.dumps(entry, ensure_ascii=False) + "\n")
    sys.stdout.write(json.dumps({"_total": len(out)}, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
