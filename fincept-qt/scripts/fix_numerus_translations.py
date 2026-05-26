#!/usr/bin/env python3
"""Repair numerus-form translation entries broken by the initial apply pass.

When the apply script wrote plural translations, it set the <translation>
element's `.text` directly. Qt's lrelease rejects that for `numerus="yes"`
messages — it needs the text inside <numerusform> children.

This script scans every fincept_<locale>.ts, finds messages whose parent has
`numerus="yes"`, and:
  1. captures the misplaced text from <translation>.text
  2. moves it into each <numerusform> child (replicating across forms)
  3. clears the parent <translation>.text and the trailing whitespace tail
     so the file is well-formed

Idempotent — re-running on already-repaired files is a no-op.
"""
from __future__ import annotations

import io
import sys
from pathlib import Path
from xml.etree import ElementTree as ET

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")

ROOT = Path(__file__).resolve().parent.parent
TS_DIR = ROOT / "translations"

LOCALES = ["zh_CN", "zh_HK", "id_ID", "vi_VN", "tr_TR",
           "de_DE", "pt_BR", "es_ES", "fr_FR", "it_IT"]


def fix_locale(locale: str) -> tuple[int, int]:
    ts_path = TS_DIR / f"fincept_{locale}.ts"
    if not ts_path.exists():
        return (0, 0)

    raw = ts_path.read_text(encoding="utf-8")
    header_end = raw.find("<TS")
    header = raw[:header_end] if header_end > 0 else "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!DOCTYPE TS>\n"

    tree = ET.parse(ts_path)
    root = tree.getroot()

    fixed = 0
    untouched_no_text = 0
    for msg in root.iter("message"):
        if msg.get("numerus") != "yes":
            continue
        trn = msg.find("translation")
        if trn is None:
            continue
        text = (trn.text or "").strip()
        forms = trn.findall("numerusform")
        if not text:
            untouched_no_text += 1
            continue
        if not forms:
            # Edge case: no numerusform children. Create at least one.
            nf = ET.SubElement(trn, "numerusform")
            nf.text = text
        else:
            for nf in forms:
                # Only overwrite if the numerusform is empty — preserve any
                # hand-curated per-form variations that may already exist.
                if not (nf.text and nf.text.strip()):
                    nf.text = text
        # Clear misplaced parent text + reset whitespace
        trn.text = None
        # Drop trailing whitespace tails between forms so the file stays tidy
        for nf in forms:
            nf.tail = "\n        " if nf is not forms[-1] else "\n    "
        # Drop the type="unfinished" marker once we have content.
        if "type" in trn.attrib and any((nf.text or "").strip() for nf in trn.findall("numerusform")):
            del trn.attrib["type"]
        fixed += 1

    body = ET.tostring(root, encoding="unicode")
    ts_path.write_text(header + body, encoding="utf-8")
    return (fixed, untouched_no_text)


def main() -> int:
    for loc in LOCALES:
        fixed, skipped = fix_locale(loc)
        print(f"[{loc}] repaired {fixed} numerus entries (skipped {skipped} empty)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
