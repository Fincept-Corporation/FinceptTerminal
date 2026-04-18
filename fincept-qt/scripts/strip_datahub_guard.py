"""Strip FINCEPT_DATAHUB_ENABLED preprocessor guards.

Phase 10 task: the hub is now the only supported data path; the compile-time
flag is being removed. This tool rewrites source files to:

  - `#ifdef FINCEPT_DATAHUB_ENABLED` ... `#endif`               -> keep body, drop guard lines
  - `#ifdef FINCEPT_DATAHUB_ENABLED` ... `#else` ... `#endif`   -> keep "if" branch, drop else branch + guard lines
  - `#ifndef FINCEPT_DATAHUB_ENABLED` ... `#endif`              -> drop whole block (legacy-only)
  - `#ifndef FINCEPT_DATAHUB_ENABLED` ... `#else` ... `#endif`  -> keep "else" branch, drop legacy + guard lines

Handles nested `#if` / `#ifdef` / `#ifndef` / `#endif` for unrelated macros by
tracking depth only when inside a matching FINCEPT block. Preserves indentation
(only strips the preprocessor directive lines themselves).

Does NOT handle `#if defined(FINCEPT_DATAHUB_ENABLED)` / `#elif` variants; the
current codebase uses only `#ifdef`/`#ifndef` (verified by grep).

Usage:
    python strip_datahub_guard.py <file1> <file2> ...
    python strip_datahub_guard.py --dry-run <files>

The tool mutates files in place. Run under version control.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

MACRO = "FINCEPT_DATAHUB_ENABLED"

# Preprocessor directive regexes — tolerate leading whitespace and whitespace
# between `#` and the keyword.
RE_IFDEF = re.compile(r"^\s*#\s*ifdef\s+" + MACRO + r"\s*$")
RE_IFNDEF = re.compile(r"^\s*#\s*ifndef\s+" + MACRO + r"\s*$")
RE_IF_ANY = re.compile(r"^\s*#\s*(if|ifdef|ifndef)\b")
RE_ELSE = re.compile(r"^\s*#\s*else\b.*$")
RE_ENDIF = re.compile(r"^\s*#\s*endif\b.*$")


class StripError(RuntimeError):
    pass


def _strip_file(text: str, filename: str) -> str:
    """Return the rewritten text.

    Walk line-by-line. When we hit a matching `#ifdef FINCEPT` / `#ifndef FINCEPT`,
    enter "tracking" state and collect the two branches (if / else). Nested
    unrelated `#if*` directives bump a depth counter so we only match the
    #else/#endif that closes the outer guard. On #endif at depth 0, emit the
    selected branch.
    """
    out: list[str] = []
    lines = text.splitlines(keepends=True)
    i = 0
    n = len(lines)

    while i < n:
        line = lines[i]
        m_ifdef = RE_IFDEF.match(line)
        m_ifndef = RE_IFNDEF.match(line)

        if not (m_ifdef or m_ifndef):
            out.append(line)
            i += 1
            continue

        # We are at a guard opening. Collect the body until the matching #endif,
        # handling #else and nested unrelated #if* directives.
        guard_start = i
        is_ifdef = bool(m_ifdef)
        i += 1

        if_branch: list[str] = []
        else_branch: list[str] = []
        in_else = False
        depth = 0  # unrelated nested #if* depth

        while i < n:
            cur = lines[i]
            if RE_IF_ANY.match(cur):
                depth += 1
                (else_branch if in_else else if_branch).append(cur)
                i += 1
                continue
            if depth == 0 and RE_ELSE.match(cur):
                if in_else:
                    raise StripError(
                        f"{filename}:{i+1}: duplicate #else inside FINCEPT guard at line {guard_start+1}"
                    )
                in_else = True
                i += 1
                continue
            if RE_ENDIF.match(cur):
                if depth == 0:
                    # End of our FINCEPT guard.
                    i += 1
                    break
                depth -= 1
                (else_branch if in_else else if_branch).append(cur)
                i += 1
                continue
            (else_branch if in_else else if_branch).append(cur)
            i += 1
        else:
            raise StripError(
                f"{filename}:{guard_start+1}: unterminated FINCEPT guard"
            )

        # Select which branch to keep.
        if is_ifdef:
            kept = if_branch
        else:
            kept = else_branch  # #ifndef — keep the #else (legacy fallback dropped)

        # Strip one leading blank line if the guard opener was preceded by one
        # and the kept body starts with a blank — prevents double-blank runs.
        # (conservative: leave as-is; clang-format will clean up)
        out.extend(kept)

    return "".join(out)


def _process(path: Path, dry_run: bool) -> tuple[bool, int]:
    """Return (changed, removed_line_count)."""
    text = path.read_text(encoding="utf-8")
    try:
        new_text = _strip_file(text, str(path))
    except StripError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return (False, 0)

    if new_text == text:
        return (False, 0)

    removed = text.count("\n") - new_text.count("\n")
    if not dry_run:
        path.write_text(new_text, encoding="utf-8")
    return (True, removed)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("files", nargs="+", help="files to rewrite")
    ap.add_argument("--dry-run", action="store_true", help="report changes, don't write")
    args = ap.parse_args()

    total_changed = 0
    total_removed = 0
    for f in args.files:
        p = Path(f)
        if not p.is_file():
            print(f"skip: not a file: {f}", file=sys.stderr)
            continue
        changed, removed = _process(p, args.dry_run)
        if changed:
            total_changed += 1
            total_removed += removed
            action = "would rewrite" if args.dry_run else "rewrote"
            print(f"{action}: {f}  (-{removed} lines)")

    label = "would change" if args.dry_run else "changed"
    print(f"\nSummary: {label} {total_changed} file(s), removed {total_removed} lines.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
