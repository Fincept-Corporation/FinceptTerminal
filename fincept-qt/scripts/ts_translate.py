"""
.ts translation pipeline — extracts unfinished translations into JSON,
calls a translation backend (Anthropic Claude, OpenAI, or deep_translator),
and writes results back into the Qt .ts XML file.

The script intentionally uses Python's built-in xml.etree to avoid extra
dependencies. Qt's lupdate also writes plain XML, so byte-level diffs stay
clean as long as we don't touch the rest of the document.

Usage:
  # Step 1 — dump unfinished entries from a .ts to JSON (one chunk per file).
  python3 ts_translate.py extract \
      ../translations/fincept_zh_CN.ts \
      --chunks 8 --out-dir /tmp/ts_chunks

  # Step 2 — translate each chunk (call out to LLM or deep_translator).
  python3 ts_translate.py translate \
      /tmp/ts_chunks/chunk_001.json \
      --target zh_CN --provider anthropic

  # Step 3 — merge translated chunks back into the .ts.
  python3 ts_translate.py apply \
      ../translations/fincept_zh_CN.ts \
      /tmp/ts_chunks/*.translated.json

The translate step is optional: any chunk file with the
{ "translations": {source: translation} } shape can be applied directly,
so a human translator (or an external service) can take over without
changing the merge step.
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import re
import sys
import textwrap
import xml.etree.ElementTree as ET
from pathlib import Path


# ── XML helpers ─────────────────────────────────────────────────────────────

def _parse_tree(path: str) -> ET.ElementTree:
    """Parse a .ts file while preserving the XML prolog and entities."""
    parser = ET.XMLParser(encoding="utf-8")
    return ET.parse(path, parser=parser)


def _write_tree(tree: ET.ElementTree, path: str) -> None:
    """Write back with the same prolog Qt Linguist expects."""
    tree.write(path, encoding="utf-8", xml_declaration=False)
    # Qt's XML header is slightly different from ElementTree's default.
    with open(path, "rb") as f:
        body = f.read()
    header = b'<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n'
    if not body.startswith(b'<?xml'):
        body = header + body
    else:
        # Replace whatever default ET wrote with the canonical Qt prolog.
        body = re.sub(rb'^<\?xml[^>]*\?>\s*', header, body, count=1)
        if b'<!DOCTYPE TS>' not in body[:200]:
            body = body.replace(header, header, 1)
    with open(path, "wb") as f:
        f.write(body)


# ── Extract ─────────────────────────────────────────────────────────────────

def _iter_messages(tree: ET.ElementTree):
    """Yield (context_name, message_element, source_text, current_translation,
    is_unfinished) for every <message> in the document."""
    root = tree.getroot()
    for ctx in root.findall("context"):
        name_el = ctx.find("name")
        ctx_name = name_el.text if name_el is not None else ""
        for msg in ctx.findall("message"):
            src_el = msg.find("source")
            tr_el = msg.find("translation")
            if src_el is None or tr_el is None:
                continue
            source = src_el.text or ""
            current = tr_el.text or ""
            unfinished = (tr_el.get("type") == "unfinished")
            yield ctx_name, msg, source, current, unfinished


def cmd_extract(args: argparse.Namespace) -> int:
    tree = _parse_tree(args.ts_file)
    entries = []
    for ctx_name, _msg, source, current, unfinished in _iter_messages(tree):
        # We collect *only* entries that need translation:
        #   1. type="unfinished" with empty translation → never translated
        #   2. type="unfinished" with non-empty translation → flagged for
        #      review; treat the existing string as a hint but re-translate
        if not unfinished:
            continue
        if not source.strip():
            continue
        entries.append({
            "context": ctx_name,
            "source": source,
            "hint": current,  # may be empty
        })

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.chunks <= 1:
        out_path = out_dir / "chunk_001.json"
        out_path.write_text(json.dumps(entries, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"Wrote {len(entries)} entries to {out_path}")
        return 0

    # Split round-robin so each chunk has similar context variety.
    chunks = [[] for _ in range(args.chunks)]
    for i, e in enumerate(entries):
        chunks[i % args.chunks].append(e)

    for i, chunk in enumerate(chunks, start=1):
        out_path = out_dir / f"chunk_{i:03d}.json"
        out_path.write_text(json.dumps(chunk, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"Wrote {len(chunk)} entries to {out_path}")
    return 0


# ── Translate (LLM / deep_translator) ───────────────────────────────────────

GLOSSARY_ZH_CN = {
    # Core terminal nouns — keep consistent across the UI.
    "Dashboard": "仪表盘",
    "Portfolio": "投资组合",
    "Watchlist": "自选列表",
    "News": "新闻",
    "Equity": "股票",
    "Equities": "股票",
    "Bond": "债券",
    "Bonds": "债券",
    "Derivative": "衍生品",
    "Derivatives": "衍生品",
    "Crypto": "加密货币",
    "Trading": "交易",
    "Backtest": "回测",
    "Backtesting": "回测",
    "Indicator": "指标",
    "Volatility": "波动率",
    "Sentiment": "情绪",
    "Bullish": "看涨",
    "Bearish": "看跌",
    "Neutral": "中性",
    "Settings": "设置",
    "Profile": "档案",
    "Login": "登录",
    "Logout": "登出",
    "Sign In": "登录",
    "Sign Up": "注册",
    "Export": "导出",
    "Import": "导入",
    "Search": "搜索",
    "Refresh": "刷新",
    "Cancel": "取消",
    "OK": "确定",
    "Apply": "应用",
    "Save": "保存",
    "Delete": "删除",
    "Edit": "编辑",
    "New": "新建",
    "Open": "打开",
    "Close": "关闭",
    "Symbol": "代码",
    "Ticker": "股票代码",
    "Buy": "买入",
    "Sell": "卖出",
    "Long": "多头",
    "Short": "空头",
    "Order": "订单",
    "Position": "持仓",
    "Account": "账户",
}

TRANSLATE_SYSTEM_PROMPT = textwrap.dedent("""
    You are translating UI strings for Fincept Terminal, a professional
    financial analytics desktop application. Translate from English to
    Simplified Chinese (zh_CN) using terminology a Bloomberg/WIND user
    would expect.

    Rules:
      • Output ONLY a JSON object mapping each source string verbatim to
        its Chinese translation. No surrounding prose.
      • Preserve printf-style placeholders (%1, %2, {0}, %s, %d) and
        markup tags exactly as they appear in the source.
      • Preserve trailing punctuation, ellipses (…), colons.
      • Keep ALL-CAPS labels in ALL-CAPS when the Chinese form supports it
        (single-character Chinese words have no case — translate normally).
      • For finance jargon, prefer Mainland China terminology
        (e.g. "covered call" → "备兑认购", "yield" → "收益率").
      • Tickers, ISO codes, currency symbols, and product names stay in
        their original form ("AAPL", "USD", "CSI 300").
      • Empty source → empty translation.
""").strip()


def _build_translate_user_message(entries: list[dict]) -> str:
    sources = [e["source"] for e in entries]
    return (
        "Translate these strings. Return a single JSON object whose keys "
        "are the exact source strings and whose values are the Chinese "
        "translations.\n\nGlossary (use these when context allows):\n"
        + json.dumps(GLOSSARY_ZH_CN, ensure_ascii=False, indent=2)
        + "\n\nSources:\n"
        + json.dumps(sources, ensure_ascii=False, indent=2)
    )


def _translate_via_anthropic(entries: list[dict]) -> dict[str, str]:
    """Call Claude. Requires ANTHROPIC_API_KEY env var."""
    import urllib.request
    api_key = os.environ.get("ANTHROPIC_API_KEY")
    if not api_key:
        raise RuntimeError("ANTHROPIC_API_KEY not set")
    body = {
        "model": os.environ.get("ANTHROPIC_MODEL", "claude-sonnet-4-6"),
        "max_tokens": 8192,
        "system": TRANSLATE_SYSTEM_PROMPT,
        "messages": [
            {"role": "user", "content": _build_translate_user_message(entries)},
        ],
    }
    req = urllib.request.Request(
        os.environ.get("ANTHROPIC_BASE_URL", "https://api.anthropic.com") + "/v1/messages",
        data=json.dumps(body).encode("utf-8"),
        headers={
            "x-api-key": api_key,
            "anthropic-version": "2023-06-01",
            "content-type": "application/json",
        },
    )
    with urllib.request.urlopen(req, timeout=180) as r:
        resp = json.loads(r.read().decode("utf-8"))
    text = "".join(part.get("text", "") for part in resp.get("content", []))
    # Strip Markdown fences if the model wrapped JSON.
    text = re.sub(r"^```(?:json)?\s*|\s*```$", "", text.strip(), flags=re.MULTILINE)
    return json.loads(text)


def _translate_via_deep_translator(entries: list[dict], target: str) -> dict[str, str]:
    from deep_translator import GoogleTranslator
    # Map Qt locale to Google target code.
    target_code = {"zh_CN": "zh-CN", "zh_TW": "zh-TW", "zh_HK": "zh-TW"}.get(target, target)
    translator = GoogleTranslator(source="auto", target=target_code)
    out = {}
    for e in entries:
        try:
            out[e["source"]] = translator.translate(e["source"]) or e["source"]
        except Exception as ex:
            print(f"  warn: {e['source'][:40]!r} → {ex}", file=sys.stderr)
            out[e["source"]] = e["source"]
    return out


def cmd_translate(args: argparse.Namespace) -> int:
    entries = json.loads(Path(args.chunk_file).read_text(encoding="utf-8"))
    if args.provider == "anthropic":
        translations = _translate_via_anthropic(entries)
    elif args.provider == "deep_translator":
        translations = _translate_via_deep_translator(entries, args.target)
    else:
        raise SystemExit(f"unknown provider: {args.provider}")

    out_path = Path(args.chunk_file).with_suffix(".translated.json")
    out_path.write_text(
        json.dumps({"translations": translations}, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    print(f"Wrote {len(translations)} translations to {out_path}")
    return 0


# ── Apply ───────────────────────────────────────────────────────────────────

def _load_translation_maps(paths: list[str]) -> dict[str, str]:
    merged: dict[str, str] = {}
    for p in paths:
        data = json.loads(Path(p).read_text(encoding="utf-8"))
        if isinstance(data, dict) and "translations" in data:
            merged.update({k: v for k, v in data["translations"].items() if v and v.strip()})
        elif isinstance(data, dict):
            merged.update({k: v for k, v in data.items() if v and isinstance(v, str) and v.strip()})
        else:
            raise SystemExit(f"unsupported shape in {p}")
    return merged


def cmd_apply(args: argparse.Namespace) -> int:
    translations = _load_translation_maps(args.translation_files)
    tree = _parse_tree(args.ts_file)
    applied = 0
    skipped = 0
    for _ctx, msg, source, _current, unfinished in _iter_messages(tree):
        if not unfinished:
            continue
        if source not in translations:
            skipped += 1
            continue
        tr_el = msg.find("translation")
        text = translations[source]

        # Plural-aware messages (<message numerus="yes">) must store text
        # inside <numerusform> children — lrelease rejects characters
        # outside the form tags. Chinese has one plural form so a single
        # numerusform is correct.
        if msg.get("numerus") == "yes":
            # Drop any pre-existing children and rebuild from scratch.
            for child in list(tr_el):
                tr_el.remove(child)
            tr_el.text = "\n            "
            nf = ET.SubElement(tr_el, "numerusform")
            nf.text = text
            nf.tail = "\n        "
        else:
            tr_el.text = text
        # Removing `type="unfinished"` is what marks the row as done in
        # Qt Linguist; lrelease only emits finished rows into the .qm.
        if "type" in tr_el.attrib:
            del tr_el.attrib["type"]
        applied += 1
    _write_tree(tree, args.ts_file)
    print(f"Applied {applied} translations; {skipped} unfinished entries had no mapping.")
    return 0


# ── Stats ───────────────────────────────────────────────────────────────────

def cmd_stats(args: argparse.Namespace) -> int:
    tree = _parse_tree(args.ts_file)
    total = 0
    done = 0
    unfinished = 0
    for _ctx, _msg, _source, current, is_unfinished in _iter_messages(tree):
        total += 1
        if is_unfinished:
            unfinished += 1
        elif current.strip():
            done += 1
    print(f"{args.ts_file}: {done}/{total} translated ({unfinished} unfinished)")
    return 0


# ── CLI ─────────────────────────────────────────────────────────────────────

def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    e = sub.add_parser("extract", help="dump unfinished entries to JSON")
    e.add_argument("ts_file")
    e.add_argument("--out-dir", default="/tmp/ts_chunks")
    e.add_argument("--chunks", type=int, default=1)
    e.set_defaults(func=cmd_extract)

    t = sub.add_parser("translate", help="translate a chunk via LLM / deep_translator")
    t.add_argument("chunk_file")
    t.add_argument("--target", default="zh_CN")
    t.add_argument("--provider", choices=["anthropic", "deep_translator"], default="anthropic")
    t.set_defaults(func=cmd_translate)

    a = sub.add_parser("apply", help="merge translated chunks back into the .ts file")
    a.add_argument("ts_file")
    a.add_argument("translation_files", nargs="+")
    a.set_defaults(func=cmd_apply)

    s = sub.add_parser("stats", help="print translation coverage")
    s.add_argument("ts_file")
    s.set_defaults(func=cmd_stats)

    args = p.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
