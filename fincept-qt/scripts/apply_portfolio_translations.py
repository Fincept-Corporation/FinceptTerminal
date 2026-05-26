#!/usr/bin/env python3
"""Apply portfolio-string translations into Qt .ts files.

Reads `translations_<locale>.json` (produced by translation sub-agents) and
inserts each translated string into the matching <message><source>...</source>
entry inside the matching <context> block, removing the `type="unfinished"`
marker. Untouched messages are left as-is so other contexts and unprovided
strings stay at their current state.

Usage:
    python apply_portfolio_translations.py <locale>
e.g. apply_portfolio_translations.py zh_CN
"""
from __future__ import annotations

import io
import json
import sys
from pathlib import Path
from xml.etree import ElementTree as ET

# Force utf-8 output on Windows.
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")

ROOT = Path(__file__).resolve().parent.parent
TS_DIR = ROOT / "translations"
TRANS_DIR = ROOT  # translations_<locale>.json sit at project root


def apply_locale(locale: str) -> None:
    ts_path = TS_DIR / f"fincept_{locale}.ts"
    json_path = TRANS_DIR / f"translations_{locale}.json"

    if not ts_path.exists():
        print(f"[{locale}] missing TS: {ts_path}", file=sys.stderr)
        return
    if not json_path.exists():
        print(f"[{locale}] missing JSON: {json_path}", file=sys.stderr)
        return

    translations: dict[str, dict[str, str]] = json.loads(
        json_path.read_text(encoding="utf-8")
    )

    # Preserve the DOCTYPE / XML declaration when round-tripping.
    raw = ts_path.read_text(encoding="utf-8")
    header_end = raw.find("<TS")
    header = raw[:header_end] if header_end > 0 else ""

    tree = ET.parse(ts_path)
    root = tree.getroot()

    applied = 0
    missing = 0
    contexts_touched: set[str] = set()

    for ctx in root.findall("context"):
        name_el = ctx.find("name")
        if name_el is None or not name_el.text:
            continue
        ctx_name = name_el.text
        if ctx_name not in translations:
            continue

        translations_for_ctx = translations[ctx_name]
        contexts_touched.add(ctx_name)

        for msg in ctx.findall("message"):
            src_el = msg.find("source")
            trn_el = msg.find("translation")
            if src_el is None or trn_el is None:
                continue
            src = src_el.text or ""
            if src not in translations_for_ctx:
                missing += 1
                continue
            translated = translations_for_ctx[src]
            if not translated:
                continue
            trn_el.text = translated
            # Drop type="unfinished" — we have a real translation now.
            if "type" in trn_el.attrib:
                del trn_el.attrib["type"]
            applied += 1

    # ET strips the DOCTYPE; write back manually to keep it.
    body = ET.tostring(root, encoding="unicode")
    out = (header if header else "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!DOCTYPE TS>\n") + body
    ts_path.write_text(out, encoding="utf-8")

    print(
        f"[{locale}] applied {applied} translations across {len(contexts_touched)} contexts"
        f" ({missing} portfolio-context strings in TS had no JSON translation)",
        file=sys.stderr,
    )


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: apply_portfolio_translations.py <locale> [<locale>...]", file=sys.stderr)
        return 2
    for locale in sys.argv[1:]:
        apply_locale(locale)
    return 0


if __name__ == "__main__":
    sys.exit(main())
