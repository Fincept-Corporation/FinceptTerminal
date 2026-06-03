#!/usr/bin/env python3
"""Insert brand-new translated strings into all 11 Qt .ts files under their
correct (namespace-qualified) contexts. Surgical text insertion — existing
content is untouched; new <message> blocks are appended to an existing context
(before its </context>) or a fresh <context> is created (before </TS>).

data.json shape:
{
  "<qualified::context>": [
     {"source": "Confirm Order", "t": {"de_DE": "…", "es_ES": "…", ...}},
     ...
  ],
  ...
}

Usage:  python3 apply_new_strings.py <data.json>
"""
import sys
import json
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape

LANGS = ["de_DE", "es_ES", "fr_FR", "id_ID", "it_IT", "pt_BR",
         "tr_TR", "vi_VN", "zh_CN", "zh_HK", "zh_TW"]


def message_block(source, translation):
    return [
        "    <message>",
        "        <source>%s</source>" % escape(source),
        "        <translation>%s</translation>" % escape(translation),
        "    </message>",
    ]


def insert_for_lang(path, lang, data):
    text = open(path, encoding="utf-8").read()
    trailing_nl = text.endswith("\n")
    lines = text.split("\n")
    added = 0
    for ctx, entries in data.items():
        blocks = []
        for e in entries:
            tr = e["t"].get(lang)
            if not tr:
                raise SystemExit("MISSING translation for [%s] %r in %s"
                                 % (lang, e["source"], ctx))
            blocks += message_block(e["source"], tr)
            added += 1
        name_line = "    <name>%s</name>" % ctx
        if name_line in lines:
            ni = lines.index(name_line)
            j = ni + 1
            while j < len(lines) and lines[j] != "</context>":
                j += 1
            lines[j:j] = blocks  # insert before </context>
        else:
            new_ctx = ["<context>", name_line] + blocks + ["</context>"]
            ti = lines.index("</TS>")
            lines[ti:ti] = new_ctx
    out = "\n".join(lines)
    if trailing_nl and not out.endswith("\n"):
        out += "\n"
    ET.fromstring(out)  # validate
    open(path, "w", encoding="utf-8").write(out)
    return added


def main():
    data = json.load(open(sys.argv[1], encoding="utf-8"))
    for lang in LANGS:
        path = "translations/fincept_%s.ts" % lang
        n = insert_for_lang(path, lang, data)
        msgs = open(path, encoding="utf-8").read().count("<message")
        print("%-8s +%d messages  total=%d" % (lang, n, msgs))


if __name__ == "__main__":
    main()
