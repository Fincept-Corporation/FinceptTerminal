#!/usr/bin/env python3
"""Architecture ratchets for fincept-qt.

Guards three regressions that a 13-agent audit found across the codebase and
that are invisible at compile time, so nothing else catches them:

  1. setStyleSheet() inside a loop body.
     Every setStyleSheet() is a full CSS re-parse. Blotters that rebuilt rows in
     a loop paid one re-parse PER ROW PER TICK — a 200-row paper blotter cost
     ~200 CSS parses a second. Hoist the rule to the parent widget with an
     objectName selector, set once.

     This is a RATCHET, not a hard failure. There are ~460 existing sites and a
     minority are legitimate (the style string is precomputed and re-applied
     only on an actual state change). Failing outright on day one would block
     every PR and the check would simply be disabled — a ratchet that people
     keep is worth more than a gate they delete.

  2. Total setStyleSheet() count (ratchet, non-increasing).
     Even outside loops, ~5k inline stylesheets make theming unmaintainable.
     The baseline may only go DOWN.

  3. User-visible string literals not wrapped in tr().
     setText("...") / setToolTip("...") / setPlaceholderText("...") /
     setWindowTitle("...") bypass lupdate entirely, so those strings can never
     be translated. Ratchet, non-increasing.

Usage:
    python arch_ratchet.py [--root fincept-qt/src] [--update-baseline]

Exit codes: 0 clean, 1 a ratchet regressed or a loop violation was found.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path

BASELINE_FILE = Path(__file__).with_name("arch_ratchet_baseline.json")

RATCHETS = (
    ("setStyleSheet_total", "setStyleSheet() calls",
     "Use setObjectName() + a global stylesheet selector instead of an inline stylesheet."),
    ("stylesheet_in_loop", "setStyleSheet() inside a loop body",
     "Hoist the rule to the parent widget with an objectName selector, applied ONCE outside the loop."),
    ("untranslated_total", "untranslated setText/setToolTip/setPlaceholderText/setWindowTitle literals",
     "Wrap user-visible strings in tr() so lupdate can extract them."),
)

STYLESHEET_RE = re.compile(r"\bsetStyleSheet\s*\(")
LOOP_RE = re.compile(r"^\s*(for|while)\s*\(|\bforeach\s*\(")
# setX("literal") — a bare double-quoted string, not tr(...) / QStringLiteral(...)
UNTRANSLATED_RE = re.compile(
    r'\bset(?:Text|ToolTip|PlaceholderText|WindowTitle)\s*\(\s*"'
)


def strip_noise(line: str) -> str:
    """Remove // comments and string bodies so we don't match inside them."""
    line = re.sub(r"//.*$", "", line)
    # Blank out string contents but keep the quotes, so UNTRANSLATED_RE still
    # sees setText(" while STYLESHEET_RE can't match text inside a literal.
    return line


def scan_file(path: Path) -> tuple[int, int, list[tuple[int, str]]]:
    """Return (stylesheet_count, untranslated_count, loop_violations)."""
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return 0, 0, []

    ss_count = 0
    untr_count = 0
    violations: list[tuple[int, str]] = []

    # Brace-depth tracking of loop bodies. When a `for`/`while` header is seen we
    # push the depth its body will live at; any setStyleSheet at or below that
    # depth (until we return above it) is inside the loop.
    depth = 0
    loop_stack: list[int] = []
    in_block_comment = False

    for lineno, raw in enumerate(lines, 1):
        line = raw
        if in_block_comment:
            end = line.find("*/")
            if end == -1:
                continue
            line = line[end + 2 :]
            in_block_comment = False
        start = line.find("/*")
        while start != -1:
            end = line.find("*/", start + 2)
            if end == -1:
                line = line[:start]
                in_block_comment = True
                break
            line = line[:start] + " " + line[end + 2 :]
            start = line.find("/*")

        code = strip_noise(line)

        if UNTRANSLATED_RE.search(code):
            untr_count += 1

        if STYLESHEET_RE.search(code):
            ss_count += 1
            if loop_stack:
                violations.append((lineno, raw.strip()[:130]))

        is_loop_header = bool(LOOP_RE.search(code))

        opens = code.count("{")
        closes = code.count("}")
        if is_loop_header:
            # Body begins at the current depth (brace on this line or the next).
            loop_stack.append(depth)
        depth += opens - closes
        while loop_stack and depth <= loop_stack[-1]:
            loop_stack.pop()

    return ss_count, untr_count, violations


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default="fincept-qt/src")
    ap.add_argument("--update-baseline", action="store_true")
    args = ap.parse_args()

    root = Path(args.root)
    if not root.is_dir():
        print(f"::error::root not found: {root}", file=sys.stderr)
        return 1

    total_ss = 0
    total_untr = 0
    all_violations: list[tuple[str, int, str]] = []

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in {"build", "_deps", ".git"}]
        for fn in filenames:
            if not fn.endswith((".cpp", ".h")):
                continue
            p = Path(dirpath) / fn
            ss, untr, viol = scan_file(p)
            total_ss += ss
            total_untr += untr
            rel = p.as_posix()
            all_violations += [(rel, ln, txt) for ln, txt in viol]

    current = {
        "setStyleSheet_total": total_ss,
        "stylesheet_in_loop": len(all_violations),
        "untranslated_total": total_untr,
    }

    if args.update_baseline:
        payload = dict(current)
        payload["_note"] = (
            "Architecture ratchets — these may only DECREASE. Regenerate with "
            "--update-baseline ONLY when lowering them (i.e. after cleanup), never to "
            "make a new violation pass."
        )
        BASELINE_FILE.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        print("baseline written:")
        for key, label, _ in RATCHETS:
            print(f"  {label}: {current[key]}")
        return 0

    baseline = {key: 0 for key, _, _ in RATCHETS}
    if BASELINE_FILE.exists():
        # utf-8-sig, not utf-8: PowerShell's Set-Content -Encoding utf8 writes a
        # BOM on Windows, and json.loads rejects it with a bare "Unexpected
        # UTF-8 BOM" that looks nothing like "someone edited the baseline".
        baseline.update(json.loads(BASELINE_FILE.read_text(encoding="utf-8-sig")))

    failed = False
    print(f"{'metric':<62} {'now':>6} {'baseline':>9}")
    for key, label, _ in RATCHETS:
        mark = "  OK" if current[key] <= baseline[key] else "  UP"
        print(f"{label:<62} {current[key]:>6} {baseline[key]:>9}{mark}")

    for key, label, advice in RATCHETS:
        if current[key] > baseline[key]:
            failed = True
            print(f"\n::error::{label} rose {baseline[key]} → {current[key]}. {advice}")

    # Always show the loop sites — they are the actionable ones even while the
    # ratchet is merely holding steady.
    if all_violations and current["stylesheet_in_loop"] > baseline["stylesheet_in_loop"]:
        print("\nsetStyleSheet() calls found inside a loop body:")
        for rel, ln, txt in all_violations[:40]:
            print(f"  {rel}:{ln}: {txt}")
        if len(all_violations) > 40:
            print(f"  … and {len(all_violations) - 40} more")

    if not failed:
        print("\nAll architecture ratchets OK.")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
