#!/usr/bin/env python3
"""gen_data_connectors.py — build the Python data-connector catalogue for Fincept Terminal.

Every `fincept-qt/scripts/*_data.py` (plus the extra scripts named in the overrides file) is a
standalone CLI that follows one calling convention:

    python <script> <command> [args...]   ->  JSON on stdout ({"error": ...} on failure)

This generator reads those scripts with the stdlib `ast` module — it never imports or executes
them — and merges the hand-maintained corrections in `fincept-qt/scripts/connector_overrides.json`
to produce:

  fincept-qt/resources/data_connectors.json
      The catalogue the terminal loads at runtime (services/connectors/ConnectorCatalog): the
      Data Sources › CATALOG tab, the MCP `data_*` tools and the workflow data-source nodes all
      read it, so there is exactly one list of connectors, commands and API-key requirements.

  fincept-qt/src/config/KeyedConnectorCredentials.inc
      X-macro list of the connectors' API-key env vars. PythonRunner.cpp includes it to inject
      the keys from SecureStorage into every Python process (and to stop stripping them), and
      CredentialsSection.cpp includes it to render one Settings › Credentials card per key. Keys
      PythonRunner already manages by hand (kManagedCredentialKeys) are left out so no card is
      duplicated; the hand-maintained section at the bottom of the file is preserved verbatim.

What is extracted per script:
  * description — the MODULE docstring (or, when a script has none, its leading `#` comment
    block). Class/function docstrings are never used: that is how the old manifest ended up
    describing cboe_data.py as "Custom error class for CBOE API errors".
  * commands    — the values the dispatcher compares the command word against (if/elif chains,
    `match`, dispatch dicts, argparse sub-parsers/choices), in source order.
  * args_hint   — per command: a usage string found in the script ("cmd <id> [limit]"), else
    the positional reads in that command's branch (`args[1] if len(args) > 1 else "AUS"` ->
    `[state=AUS]`, a bare `sys.argv[2]` -> `<symbol>`).
  * env_keys    — credential-shaped env vars (`*_KEY`, `*_TOKEN`, `*_SECRET`, `*_EMAIL`, ...)
    the script reads AND actually uses. A key that is read into a variable and never referenced
    again is dropped: storing it would change nothing, so the catalogue must not ask for it.

`key_required` is NOT guessed: it must be recorded per keyed connector in the overrides file
(true only when the connector cannot return data without the key). The generator fails when a
keyed connector has no recorded decision, so a new keyed script cannot ship unreviewed.

Usage:
  python .github/scripts/gen_data_connectors.py              # regenerate both outputs
  python .github/scripts/gen_data_connectors.py --check      # exit 1 when an output is stale
  python .github/scripts/gen_data_connectors.py --report F   # also write a per-script report

Stdlib only; Python 3.9+.
"""

from __future__ import annotations

import argparse
import ast
import io
import json
import os
import re
import sys
import tokenize
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
QT = REPO / "fincept-qt"
SCRIPTS_DIR = QT / "scripts"
OVERRIDES_PATH = SCRIPTS_DIR / "connector_overrides.json"
CATALOG_PATH = QT / "resources" / "data_connectors.json"
INC_PATH = QT / "src" / "config" / "KeyedConnectorCredentials.inc"
RUNNER_CPP = QT / "src" / "python" / "PythonRunner.cpp"

SCHEMA_VERSION = 1

CATEGORIES = [
    "Macro & Central Banks",
    "Government & Statistics",
    "Markets & Equities",
    "Crypto & DeFi",
    "China & Asia",
    "Energy, Commodities & Climate",
    "Shipping",
    "Geospatial & Geopolitics",
    "News & Alternative Data",
    "Academic & Research",
    "Other",
]

# Env vars that look credential-shaped but are configuration, not secrets.
NON_CREDENTIAL_ENV = {"DATA_GOV_HK_LANGUAGE", "FINCEPT_YF_TIMEOUT", "APPDATA", "HOME", "PATH", "PYTHONPATH"}
CREDENTIAL_ENV_RE = re.compile(
    r"(_KEY|_TOKEN|_SECRET|_PASSWORD|_EMAIL|_USERNAME|_USER|_CLIENT_ID|_APP_ID|_ACCOUNT|_APP_TOKEN)$|^[A-Z0-9]+_KEY$")

# Keys PythonRunner manages by hand (kManagedCredentialKeys). Parsed from PythonRunner.cpp at
# generation time; this list is only the fallback when that file cannot be read.
FALLBACK_MANAGED_KEYS = [
    "ALPHA_VANTAGE_API_KEY", "POLYGON_API_KEY", "DATABENTO_API_KEY", "FRED_API_KEY", "NEWSAPI_KEY",
    "BINANCE_API_KEY", "BINANCE_SECRET_KEY", "KRAKEN_API_KEY", "KRAKEN_SECRET_KEY", "IEX_CLOUD_TOKEN",
    "FINNHUB_API_KEY", "TIINGO_API_KEY", "QUANDL_API_KEY", "POLYMARKET_API_KEY", "POLYMARKET_SECRET",
    "POLYMARKET_PASSPHRASE", "POLYMARKET_WALLET",
]

INC_HAND_MARKER = "// ── Hand-maintained additions"

DOC_TITLE_SUFFIXES = (
    " Comprehensive API Wrapper", " Data Fetcher", " Data Wrapper", " API Data Fetcher", " API Wrapper",
    " Data Explorer", " Wrapper", " Fetcher", " - COMPREHENSIVE VERSION",
)


# ─────────────────────────────────────────────────────────────────────────────
# Small helpers
# ─────────────────────────────────────────────────────────────────────────────

def die(msg: str) -> None:
    print(f"gen_data_connectors: error: {msg}", file=sys.stderr)
    sys.exit(2)


def load_json(path: Path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def is_argv(node) -> bool:
    return (isinstance(node, ast.Attribute) and node.attr == "argv" and isinstance(node.value, ast.Name)
            and node.value.id == "sys")


def const_int(node):
    if isinstance(node, ast.Constant) and isinstance(node.value, int) and not isinstance(node.value, bool):
        return node.value
    return None


def strip_str_methods(node):
    """sys.argv[1].lower().strip() -> sys.argv[1]"""
    while (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
           and node.func.attr in ("lower", "strip", "upper", "replace", "casefold")):
        node = node.func.value
    return node


def literal_text(node) -> str | None:
    """Render a simple default value for an args hint."""
    if isinstance(node, ast.Constant):
        if node.value is None:
            return None
        if isinstance(node.value, bool):
            return "true" if node.value else "false"
        if isinstance(node.value, (int, float)):
            return str(node.value)
        if isinstance(node.value, str):
            s = node.value
            if s == "" or len(s) > 40 or any(c in s for c in " \n\t"):
                return None
            return s
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
        inner = literal_text(node.operand)
        return f"-{inner}" if inner is not None else None
    return None


# ─────────────────────────────────────────────────────────────────────────────
# Per-script analysis
# ─────────────────────────────────────────────────────────────────────────────

class ScriptInfo:
    def __init__(self, path: Path):
        self.path = path
        self.name = path.name
        self.src = path.read_text(encoding="utf-8", errors="replace")
        self.error = None
        try:
            self.mod = ast.parse(self.src)
        except SyntaxError as e:
            self.mod = None
            self.error = f"syntax error: {e}"
        self.description_lines: list[str] = []
        self.commands: list[str] = []
        self.branches: dict[str, list] = {}  # command -> list of AST nodes (its dispatch branch)
        self.hints: dict[str, str] = {}
        self.env_read: list[str] = []  # credential env vars read
        self.env_used: list[str] = []  # ... and actually used
        self.has_main_guard = False
        if self.mod is not None:
            self._analyze()

    # ── description ─────────────────────────────────────────────────────────
    def _description(self):
        doc = ast.get_docstring(self.mod, clean=True)
        if doc and doc.strip():
            paragraph = []
            for line in doc.strip().splitlines():
                if not line.strip():
                    if paragraph:
                        break
                    continue
                paragraph.append(line.strip())
            return paragraph
        # No module docstring — fall back to the leading comment block (module-level header).
        lines = []
        for raw in self.src.splitlines():
            s = raw.strip()
            if not s:
                if lines:
                    break
                continue
            if not s.startswith("#"):
                break
            text = s.lstrip("#").strip()
            if text.startswith("!") or text.lower().startswith(("-*-", "coding", "type:")):
                continue
            if text:
                lines.append(text)
        return lines

    # ── command dispatch ────────────────────────────────────────────────────
    def _command_vars(self):
        """Names bound to the command word: `command = sys.argv[1]`, `cmd = args[0]`, ..."""
        arg_lists = {"args", "argv", "argv_list", "cli_args", "arguments"}
        self.rest_lists = set()  # lists holding only the arguments AFTER the command (sys.argv[2:])
        for node in ast.walk(self.mod):
            if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
                v = node.value
                if isinstance(v, ast.Subscript) and is_argv(v.value) and isinstance(v.slice, ast.Slice):
                    lo = const_int(v.slice.lower) if v.slice.lower is not None else 0
                    if lo == 1:
                        arg_lists.add(node.targets[0].id)
                    elif lo == 2:
                        self.rest_lists.add(node.targets[0].id)
        names = set()
        for node in ast.walk(self.mod):
            if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
                v = strip_str_methods(node.value)
                if isinstance(v, ast.Subscript):
                    idx = const_int(v.slice)
                    if is_argv(v.value) and idx == 1:
                        names.add(node.targets[0].id)
                    elif isinstance(v.value, ast.Name) and v.value.id in arg_lists and idx == 0:
                        names.add(node.targets[0].id)
        return names, arg_lists

    def _is_cmd_expr(self, node, cmd_vars) -> bool:
        node = strip_str_methods(node)
        if isinstance(node, ast.Name) and node.id in cmd_vars:
            return True
        if isinstance(node, ast.Subscript) and is_argv(node.value) and const_int(node.slice) == 1:
            return True
        if isinstance(node, ast.Attribute) and node.attr in ("command", "action", "cmd") and \
                isinstance(node.value, ast.Name) and node.value.id in ("args", "ns", "parsed", "opts"):
            return True
        return False

    def _compare_commands(self, test, cmd_vars):
        out = []
        if isinstance(test, ast.BoolOp) and isinstance(test.op, ast.Or):
            for v in test.values:
                sub = self._compare_commands(v, cmd_vars)
                if not sub:
                    return []
                out += sub
            return out
        if isinstance(test, ast.Compare) and len(test.ops) == 1 and self._is_cmd_expr(test.left, cmd_vars):
            op, comp = test.ops[0], test.comparators[0]
            if isinstance(op, ast.Eq) and isinstance(comp, ast.Constant) and isinstance(comp.value, str):
                out.append(comp.value)
            elif isinstance(op, ast.In) and isinstance(comp, (ast.Tuple, ast.List, ast.Set)):
                for e in comp.elts:
                    if isinstance(e, ast.Constant) and isinstance(e.value, str):
                        out.append(e.value)
        return out

    def _add_command(self, cmd: str, branch):
        if not isinstance(cmd, str) or not cmd or len(cmd) > 64 or any(c.isspace() for c in cmd):
            return
        if cmd.startswith("-"):
            return
        if cmd not in self.commands:
            self.commands.append(cmd)
        if branch is not None:
            self.branches.setdefault(cmd, []).extend(branch if isinstance(branch, list) else [branch])

    def _dispatch(self):
        cmd_vars, self.arg_lists = self._command_vars()
        self.cmd_vars = cmd_vars
        found = []  # (lineno, cmd, branch)

        for node in ast.walk(self.mod):
            if isinstance(node, ast.If):
                for c in self._compare_commands(node.test, cmd_vars):
                    found.append((node.lineno, c, node.body))
            elif isinstance(node, ast.Match) and self._is_cmd_expr(node.subject, cmd_vars):
                for case in node.cases:
                    pats = case.pattern.patterns if isinstance(case.pattern, ast.MatchOr) else [case.pattern]
                    for p in pats:
                        if isinstance(p, ast.MatchValue) and isinstance(p.value, ast.Constant):
                            found.append((case.pattern.lineno, p.value.value, case.body))

        # Dispatch dicts: {"cmd": handler, ...} indexed by / tested against the command word.
        dicts = {}
        for node in ast.walk(self.mod):
            if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name) \
                    and isinstance(node.value, ast.Dict) and node.value.keys:
                keys, vals = node.value.keys, node.value.values
                if all(isinstance(k, ast.Constant) and isinstance(k.value, str) for k in keys):
                    callable_vals = sum(isinstance(v, (ast.Lambda, ast.Name, ast.Attribute)) for v in vals)
                    if callable_vals >= max(1, len(vals) // 2):
                        dicts[node.targets[0].id] = node.value
        used = set()
        for node in ast.walk(self.mod):
            if isinstance(node, ast.Subscript) and isinstance(node.value, ast.Name) and node.value.id in dicts:
                if self._is_cmd_expr(node.slice, cmd_vars):
                    used.add(node.value.id)
            elif isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr == "get" \
                    and isinstance(node.func.value, ast.Name) and node.func.value.id in dicts and node.args:
                if self._is_cmd_expr(node.args[0], cmd_vars):
                    used.add(node.func.value.id)
            elif isinstance(node, ast.Compare) and len(node.ops) == 1 and \
                    isinstance(node.ops[0], (ast.In, ast.NotIn)) and isinstance(node.comparators[0], ast.Name) \
                    and node.comparators[0].id in dicts and self._is_cmd_expr(node.left, cmd_vars):
                used.add(node.comparators[0].id)
        funcs = {n.name: n for n in ast.walk(self.mod) if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))}
        for dname in used:
            d = dicts[dname]
            for k, v in zip(d.keys, d.values):
                # A handler given by name parses its own arguments — use its body as the branch.
                if isinstance(v, ast.Name) and v.id in funcs:
                    branch = list(funcs[v.id].body)
                elif isinstance(v, ast.Attribute) and v.attr in funcs:
                    branch = list(funcs[v.attr].body)
                else:
                    branch = [v]
                found.append((k.lineno, k.value, branch))

        # argparse: sub-parsers and `choices=` on a command-like positional.
        for node in ast.walk(self.mod):
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute):
                if node.func.attr == "add_parser" and node.args and isinstance(node.args[0], ast.Constant) \
                        and isinstance(node.args[0].value, str):
                    found.append((node.lineno, node.args[0].value, None))
                if node.func.attr == "add_argument" and node.args and isinstance(node.args[0], ast.Constant) and \
                        node.args[0].value in ("command", "action", "cmd", "endpoint", "--action", "--command"):
                    for kw in node.keywords:
                        if kw.arg == "choices" and isinstance(kw.value, (ast.List, ast.Tuple)):
                            for e in kw.value.elts:
                                if isinstance(e, ast.Constant) and isinstance(e.value, str):
                                    found.append((e.lineno, e.value, None))

        found.sort(key=lambda t: t[0])
        for _, cmd, branch in found:
            self._add_command(cmd, branch)

        # Arguments read once before the dispatch chain (`country = args[1] if len(args) > 1
        # else ""` ahead of `if command == ...`) apply to every command.
        self.preamble = []
        for fn in ast.walk(self.mod):
            if not isinstance(fn, (ast.FunctionDef, ast.AsyncFunctionDef)):
                continue
            pre = self._preamble_before_dispatch(fn.body, cmd_vars)
            if pre is not None:
                self.preamble = pre
                break

    def _preamble_before_dispatch(self, body, cmd_vars):
        for idx, stmt in enumerate(body):
            if isinstance(stmt, ast.If) and self._compare_commands(stmt.test, cmd_vars):
                return list(body[:idx])
            if isinstance(stmt, ast.Try):
                inner = self._preamble_before_dispatch(stmt.body, cmd_vars)
                if inner is not None:
                    return list(body[:idx]) + inner
        return None

    # ── argument hints ──────────────────────────────────────────────────────
    def _function_params(self):
        params = {}
        for node in ast.walk(self.mod):
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                names = [a.arg for a in node.args.args if a.arg not in ("self", "cls")]
                params.setdefault(node.name, names)
        return params

    def _usage_hints(self):
        """`cmd <id> [limit]` style usage text anywhere in strings or comments."""
        texts = []
        for node in ast.walk(self.mod):
            if isinstance(node, ast.Constant) and isinstance(node.value, str) and len(node.value) < 2000:
                texts.extend(node.value.splitlines())
            elif isinstance(node, ast.JoinedStr):
                pass
        try:
            for tok in tokenize.generate_tokens(io.StringIO(self.src).readline):
                if tok.type == tokenize.COMMENT:
                    texts.append(tok.string.lstrip("#").strip())
        except (tokenize.TokenError, IndentationError, SyntaxError):
            pass
        hints = {}
        cmd_alt = "|".join(re.escape(c) for c in sorted(self.commands, key=len, reverse=True))
        if not cmd_alt:
            return hints
        rx = re.compile(r"^\s*(?:[-*]\s*)?(?:usage:\s*)?(?:python3?\s+\S+\.py\s+)?(" + cmd_alt + r")\s+(.+?)\s*$",
                        re.IGNORECASE)
        for t in texts:
            t = t.strip().strip("\"',")
            m = rx.match(t)
            if not m:
                continue
            cmd, rest = m.group(1), m.group(2)
            if cmd not in self.commands:
                continue
            rest = re.sub(r"\s*\((?:e\.g\.|eg\b|i\.e\.)[^)]*\)", "", rest)
            rest = re.split(r"\s+(?:-|—|#|->|→)\s+|\s{2,}|\.\s+", rest)[0].strip().rstrip(".,;")
            rest = re.sub(r"^requires\s+", "", rest, flags=re.I)
            if not re.search(r"[<\[]|--", rest):
                continue
            if len(rest) > 160:
                continue
            prev = hints.get(cmd)
            if prev is None or len(rest) > len(prev):
                hints[cmd] = rest
        return hints

    def _branch_hint(self, cmd, fparams):
        """Positional reads inside a command's branch -> `<required> [optional=default]`."""
        nodes = self.branches.get(cmd)
        if not nodes:
            return None
        slots = self._slots(nodes, fparams)
        for pos, entry in self._slots(self.preamble, fparams).items():
            slots.setdefault(pos, entry)  # branch-specific reads win over the shared preamble
        if not slots:
            return ""
        parts = []
        for pos in sorted(slots):
            nm, required, default = slots[pos]
            nm = nm or f"arg{pos}"
            if required:
                parts.append(f"<{nm}>")
            elif default is not None:
                parts.append(f"[{nm}={default}]")
            else:
                parts.append(f"[{nm}]")
        return " ".join(parts)

    def _slots(self, nodes, fparams):
        """position -> (name, required, default) for every argv read under `nodes`."""
        slots = {}

        def position(sub):
            if not isinstance(sub, ast.Subscript):
                return None
            idx = const_int(sub.slice)
            if idx is None:
                return None
            if is_argv(sub.value):
                return idx - 1 if idx >= 2 else None
            if isinstance(sub.value, ast.Name) and sub.value.id in self.rest_lists:
                return idx + 1
            if isinstance(sub.value, ast.Name) and sub.value.id in self.arg_lists:
                return idx if idx >= 1 else None
            return None

        def has_len_call(node) -> bool:
            return any(isinstance(x, ast.Call) and isinstance(x.func, ast.Name) and x.func.id == "len"
                       for x in ast.walk(node))

        def name_for_value(parent_map, node):
            """Walk up from an argv read to the name it is bound to: an assignment target, a
            keyword argument, or the parameter it lands in when passed positionally to a function
            defined in the script."""
            cur = node
            while True:
                par = parent_map.get(id(cur))
                if par is None:
                    return None
                if isinstance(par, (ast.Assign, ast.AnnAssign)):
                    t = par.targets[0] if isinstance(par, ast.Assign) and len(par.targets) == 1 else (
                        par.target if isinstance(par, ast.AnnAssign) else None)
                    return t.id if isinstance(t, ast.Name) else None
                if isinstance(par, ast.keyword):
                    return par.arg
                if isinstance(par, ast.Call) and cur in par.args:
                    fname = par.func.attr if isinstance(par.func, ast.Attribute) else (
                        par.func.id if isinstance(par.func, ast.Name) else None)
                    if fname in ("int", "float", "str", "bool", "loads", "upper", "lower", "strip", "split"):
                        cur = par
                        continue
                    plist = fparams.get(fname) if fname else None
                    if plist is not None:
                        i = par.args.index(cur)
                        if i < len(plist):
                            return plist[i]
                    return None
                if isinstance(par, ast.IfExp) and cur is not par.body:
                    return None  # the read sits in the test/else of a conditional — not the value
                if isinstance(par, (ast.IfExp, ast.Call, ast.Attribute, ast.BinOp, ast.UnaryOp, ast.Subscript,
                                    ast.BoolOp, ast.Compare, ast.ListComp, ast.comprehension, ast.GeneratorExp)):
                    cur = par
                    continue
                return None

        def guarding_ifexp(parent_map, node):
            """The `X if len(args) > N else default` expression this read is the value of, if any."""
            cur, par = node, parent_map.get(id(node))
            while par is not None:
                if isinstance(par, ast.IfExp):
                    if cur is par.body and has_len_call(par.test):
                        return par
                    return None
                if isinstance(par, (ast.Call, ast.Attribute, ast.Subscript, ast.BinOp, ast.UnaryOp, ast.Compare)):
                    cur, par = par, parent_map.get(id(par))
                    continue
                return None
            return None

        for root in nodes:
            parent_map = {}
            for p in ast.walk(root):
                for ch in ast.iter_child_nodes(p):
                    parent_map[id(ch)] = p
            for n in ast.walk(root):
                pos = position(n)
                if pos is None:
                    continue
                guard = guarding_ifexp(parent_map, n)
                if guard is not None:
                    nm = name_for_value(parent_map, guard)
                    entry = (nm, False, literal_text(guard.orelse))
                else:
                    nm = name_for_value(parent_map, n)
                    entry = (nm, True, None)
                cur = slots.get(pos)
                if cur is None or (cur[0] is None and nm):
                    slots[pos] = entry
        return slots

    # ── env keys ────────────────────────────────────────────────────────────
    def _env(self):
        consts = {}
        for node in self.mod.body:
            if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name) \
                    and isinstance(node.value, ast.Constant) and isinstance(node.value.value, str):
                consts[node.targets[0].id] = node.value.value

        def env_name(a):
            if isinstance(a, ast.Constant) and isinstance(a.value, str):
                return a.value
            if isinstance(a, ast.Name) and a.id in consts:
                return consts[a.id]
            return None

        reads = []  # (env_name, node)
        for node in ast.walk(self.mod):
            nm = None
            if isinstance(node, ast.Call):
                f = node.func
                if isinstance(f, ast.Attribute) and f.attr == "getenv" and isinstance(f.value, ast.Name) \
                        and f.value.id == "os" and node.args:
                    nm = env_name(node.args[0])
                elif isinstance(f, ast.Attribute) and f.attr == "get" and node.args and (
                        (isinstance(f.value, ast.Attribute) and f.value.attr == "environ") or
                        (isinstance(f.value, ast.Name) and f.value.id == "environ")):
                    nm = env_name(node.args[0])
                elif isinstance(f, ast.Name) and f.id == "getenv" and node.args:
                    nm = env_name(node.args[0])
            elif isinstance(node, ast.Subscript) and isinstance(node.value, ast.Attribute) \
                    and node.value.attr == "environ" and isinstance(node.ctx, ast.Load):
                nm = env_name(node.slice)
            if nm and nm not in NON_CREDENTIAL_ENV and CREDENTIAL_ENV_RE.search(nm):
                reads.append((nm, node))

        # Which variable does each read land in? A read whose variable is never loaded again is
        # dead: the script would behave identically without the key.
        parent = {}
        for p in ast.walk(self.mod):
            for ch in ast.iter_child_nodes(p):
                parent[id(ch)] = p
        loads = {}
        for node in ast.walk(self.mod):
            if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load):
                loads[node.id] = loads.get(node.id, 0) + 1
            elif isinstance(node, ast.Attribute) and isinstance(node.ctx, ast.Load):
                loads["." + node.attr] = loads.get("." + node.attr, 0) + 1

        for nm, node in reads:
            if nm not in self.env_read:
                self.env_read.append(nm)
            cur, par = node, parent.get(id(node))
            target = None
            inline = False
            while par is not None:
                if isinstance(par, (ast.Assign, ast.AnnAssign)):
                    tgts = par.targets if isinstance(par, ast.Assign) else [par.target]
                    t = tgts[0] if len(tgts) == 1 else None
                    if isinstance(t, ast.Name):
                        target = t.id
                    elif isinstance(t, ast.Attribute):
                        target = "." + t.attr
                    else:
                        inline = True
                    break
                # Look through wrappers that keep the value: `x or os.environ.get(...)`,
                # `os.environ.get(...).strip()`, `a if c else os.getenv(...)`.
                if isinstance(par, (ast.BoolOp, ast.IfExp)) or (
                        isinstance(par, ast.Attribute) and par.attr in ("strip", "lower", "upper")) or (
                        isinstance(par, ast.Call) and isinstance(par.func, ast.Attribute)
                        and par.func.attr in ("strip", "lower", "upper")):
                    cur, par = par, parent.get(id(par))
                    continue
                inline = True  # consumed directly (call argument, dict value, comparison, ...)
                break
            used = inline or (target is not None and loads.get(target, 0) > 0)
            if used and nm not in self.env_used:
                self.env_used.append(nm)

    def _analyze(self):
        self.description_lines = self._description()
        self.has_main_guard = "__main__" in self.src
        self._dispatch()
        fparams = self._function_params()
        usage = self._usage_hints()
        for c in self.commands:
            if c in usage:
                self.hints[c] = usage[c]
            else:
                h = self._branch_hint(c, fparams)
                self.hints[c] = h or ""
        self._env()


# ─────────────────────────────────────────────────────────────────────────────
# Catalogue assembly
# ─────────────────────────────────────────────────────────────────────────────

def default_name(script: str) -> str:
    stem = script[:-3]
    if stem.endswith("_data"):
        stem = stem[:-5]
    return "data_" + stem


BOILERPLATE_RE = re.compile(
    r"^(Location|Path|File|Author|Version)\s*:|Returns JSON output for Qt|^Based on OpenBB\b|"
    r"^Modular, fault-tolerant design", re.I)


def split_title_desc(lines: list[str]):
    lines = [l for l in lines if not BOILERPLATE_RE.search(l)]
    lines = [re.sub(r"^Description\s*:\s*", "", l, flags=re.I) for l in lines]
    if not lines:
        return "", ""
    first = lines[0].rstrip(".")
    title = first
    for suf in DOC_TITLE_SUFFIXES:
        if title.endswith(suf):
            title = title[: -len(suf)].strip()
            break
    title = re.sub(r"\s+[-–—]\s*$", "", title).strip()
    rest = " ".join(lines[1:]).strip()
    desc = rest if rest else first
    desc = re.sub(r"\s+", " ", desc)
    if desc and desc[-1] not in ".!?)":
        desc += "."
    return title, desc


def parse_managed_keys() -> list[str]:
    try:
        text = RUNNER_CPP.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return list(FALLBACK_MANAGED_KEYS)
    m = re.search(r"kManagedCredentialKeys\s*=\s*\{(.*?)#define", text, re.S)
    if not m:
        return list(FALLBACK_MANAGED_KEYS)
    keys = re.findall(r'"([A-Z0-9_]+)"', m.group(1))
    return keys or list(FALLBACK_MANAGED_KEYS)


def parse_existing_inc():
    """Returns (generated {key: name} in order, hand-maintained section text, hand keys)."""
    if not INC_PATH.exists():
        return {}, "", set()
    text = INC_PATH.read_text(encoding="utf-8")
    idx = text.find(INC_HAND_MARKER)
    gen_part = text if idx < 0 else text[:idx]
    hand_part = "" if idx < 0 else text[idx:]
    gen = {}
    for k, n in re.findall(r'FINCEPT_KEYED_CRED\("([^"]+)",\s*"([^"]*)"\)', gen_part):
        gen[k] = n
    hand_keys = set(re.findall(r'FINCEPT_KEYED_CRED\("([^"]+)"', hand_part))
    return gen, hand_part, hand_keys


def credential_display_name(key: str, owners: list[dict], labels: dict, previous: dict) -> str:
    if key in labels:
        return labels[key]
    if key in previous:
        return previous[key]
    if owners:
        t = owners[0]["title"]
        t = re.sub(r"\s*\(.*?\)\s*", " ", t).strip()
        suffix = ""
        if key.endswith("_EMAIL"):
            suffix = " Email"
        elif key.endswith(("_USERNAME", "_USER")):
            suffix = " Username"
        elif key.endswith("_CLIENT_ID"):
            suffix = " Client ID"
        elif key.endswith("_SECRET") or key.endswith("_CLIENT_SECRET"):
            suffix = " Client Secret"
        return (t + suffix)[:60]
    words = re.sub(r"_(API_KEY|KEY|TOKEN)$", "", key).split("_")
    return " ".join(w if len(w) <= 4 else w.capitalize() for w in words)


def build(overrides: dict, report: list):
    exclude = overrides.get("exclude", {})
    include = overrides.get("include", [])
    conn_ov = overrides.get("connectors", {})
    errors = []

    scripts = sorted(p.name for p in SCRIPTS_DIR.glob("*.py"))
    candidates = [s for s in scripts if (s.endswith("_data.py") and s not in exclude) or s in include]
    for s in include:
        if s not in scripts:
            errors.append(f"overrides.include names a missing script: {s}")
    for s in list(exclude) + list(conn_ov):
        if s not in scripts:
            errors.append(f"overrides names a missing script: {s}")

    entries = []
    names_seen = {}
    for script in candidates:
        info = ScriptInfo(SCRIPTS_DIR / script)
        ov = conn_ov.get(script, {})
        rep = {"script": script, "commands": info.commands, "hints": info.hints, "env_read": info.env_read,
               "env_used": info.env_used, "doc": info.description_lines[:3], "error": info.error}
        report.append(rep)
        if info.error:
            errors.append(f"{script}: {info.error}")
            continue

        # Commands: detected (in source order), optionally replaced / extended / pruned by overrides.
        commands = list(info.commands)
        if "commands" in ov:
            commands = list(ov["commands"])
        for c in ov.get("commands_add", []):
            if c not in commands:
                commands.append(c)
        drop = set(ov.get("commands_drop", []))
        commands = [c for c in commands if c not in drop]
        if not commands:
            errors.append(f"{script}: no CLI commands detected — add it to overrides.exclude or list its commands")
            continue
        if not info.has_main_guard:
            errors.append(f"{script}: no __main__ guard — not a standalone CLI")
            continue
        # Every command must literally appear in the script (guards against typos in overrides).
        for c in commands:
            if f'"{c}"' not in info.src and f"'{c}'" not in info.src:
                errors.append(f"{script}: command '{c}' does not appear in the script")

        hint_ov = ov.get("args_hints", {})
        cmd_objs = []
        for c in commands:
            hint = hint_ov.get(c, info.hints.get(c, ""))
            cmd_objs.append({"name": c, "args_hint": hint})

        title_auto, desc_auto = split_title_desc(info.description_lines)
        title = ov.get("title", title_auto) or script
        desc = ov.get("desc", desc_auto) or title

        env_keys = list(info.env_used)
        for k in ov.get("env_keys_add", []):
            if k not in env_keys:
                env_keys.append(k)
        env_keys = [k for k in env_keys if k not in set(ov.get("env_keys_drop", []))]
        if "env_keys" in ov:
            env_keys = list(ov["env_keys"])

        if env_keys:
            if "key_required" not in ov:
                errors.append(f"{script}: reads {env_keys} but overrides has no key_required decision")
                key_required = False
            else:
                key_required = bool(ov["key_required"])
        else:
            key_required = False
            if ov.get("key_required"):
                errors.append(f"{script}: key_required=true but no env key is used")

        category = ov.get("category")
        if category is None:
            errors.append(f"{script}: no category in overrides")
            category = "Other"
        if category not in CATEGORIES:
            errors.append(f"{script}: unknown category '{category}'")
        region = ov.get("region", "Global")

        name = ov.get("name", default_name(script))
        if not re.fullmatch(r"[A-Za-z0-9_\-]{1,64}", name):
            errors.append(f"{script}: invalid connector name '{name}'")
        if name in names_seen:
            errors.append(f"{script}: name '{name}' already used by {names_seen[name]}")
        names_seen[name] = script

        default_command = ov.get("default_command")
        if default_command is None:
            default_command = next((c["name"] for c in cmd_objs if "<" not in c["args_hint"]), cmd_objs[0]["name"])
        if default_command not in commands:
            errors.append(f"{script}: default_command '{default_command}' is not a command")

        entry = {
            "name": name,
            "script": script,
            "title": title,
            "desc": desc,
            "category": category,
            "region": region,
            "commands": cmd_objs,
            "default_command": default_command,
            "env_keys": env_keys,
            "key_required": key_required,
        }
        for opt in ("key_note", "docs_url", "default_args", "ttl_sec", "broken"):
            if opt in ov:
                entry[opt] = ov[opt]
        if ov.get("mcp", True) is False:
            entry["mcp"] = False
        entries.append(entry)

    entries.sort(key=lambda e: (CATEGORIES.index(e["category"]) if e["category"] in CATEGORIES else 99,
                                e["title"].lower()))
    return entries, errors


def render_catalog(entries: list[dict]) -> str:
    counts = {c: 0 for c in CATEGORIES}
    for e in entries:
        counts[e["category"]] = counts.get(e["category"], 0) + 1
    doc = {
        "schema": SCHEMA_VERSION,
        "generated_by": ".github/scripts/gen_data_connectors.py — do not edit by hand; "
                        "fix fincept-qt/scripts/connector_overrides.json or the script, then regenerate",
        "convention": "python <script> <command> [args...] -> JSON on stdout; {\"error\": ...} means failure",
        "categories": [c for c in CATEGORIES if counts.get(c)],
        "connectors": entries,
    }
    # One connector per line keeps diffs reviewable while staying valid, compact JSON.
    head = json.dumps({k: v for k, v in doc.items() if k != "connectors"}, ensure_ascii=False, indent=1)
    head = head.rstrip()[:-1].rstrip()  # drop closing brace
    lines = [head + ",", ' "connectors": [']
    for i, e in enumerate(entries):
        sep = "," if i + 1 < len(entries) else ""
        lines.append("  " + json.dumps(e, ensure_ascii=False, separators=(", ", ": ")) + sep)
    lines.append(" ]")
    lines.append("}")
    return "\n".join(lines) + "\n"


def render_inc(entries: list[dict], overrides: dict, errors: list) -> str:
    managed = set(parse_managed_keys())
    previous, hand_part, hand_keys = parse_existing_inc()
    labels = overrides.get("credential_labels", {})
    owners = {}
    for e in entries:
        for k in e["env_keys"]:
            owners.setdefault(k, []).append(e)
    keys = sorted(k for k in owners if k not in managed and k not in hand_keys)
    lines = [
        "// AUTO-GENERATED by .github/scripts/gen_data_connectors.py from fincept-qt/resources/data_connectors.json",
        "// — do not edit by hand above the hand-maintained marker; rerun the generator instead.",
        f"// {len(keys)} API-key env vars used by keyed data connectors (excluding keys PythonRunner manages",
        "// itself in kManagedCredentialKeys and the hand-maintained entries below).",
        "// X-macro. Consumer #defines FINCEPT_KEYED_CRED(KEY, NAME) before #include.",
    ]
    for k in keys:
        name = credential_display_name(k, owners[k], labels, previous).replace('"', "'")
        lines.append(f'FINCEPT_KEYED_CRED("{k}", "{name}")')
    text = "\n".join(lines) + "\n"
    if hand_part:
        text += "\n" + hand_part.lstrip("\n")
        if not text.endswith("\n"):
            text += "\n"
    return text


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--check", action="store_true", help="exit 1 if an output file is out of date")
    ap.add_argument("--report", help="write a JSON analysis report to this path")
    args = ap.parse_args()

    if not OVERRIDES_PATH.exists():
        die(f"overrides file missing: {OVERRIDES_PATH}")
    overrides = load_json(OVERRIDES_PATH)
    report = []
    entries, errors = build(overrides, report)
    inc_text = render_inc(entries, overrides, errors)
    cat_text = render_catalog(entries)

    if args.report:
        with open(args.report, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=1, ensure_ascii=False)

    if errors:
        for e in errors:
            print("gen_data_connectors: " + e, file=sys.stderr)
        print(f"gen_data_connectors: {len(errors)} problem(s); outputs NOT written", file=sys.stderr)
        return 2

    outputs = [(CATALOG_PATH, cat_text), (INC_PATH, inc_text)]
    if args.check:
        stale = [str(p.relative_to(REPO)) for p, t in outputs
                 if not p.exists() or p.read_text(encoding="utf-8") != t]
        if stale:
            print("gen_data_connectors: stale: " + ", ".join(stale) + " — run .github/scripts/gen_data_connectors.py",
                  file=sys.stderr)
            return 1
        print(f"gen_data_connectors: up to date ({len(entries)} connectors)")
        return 0

    for path, text in outputs:
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    keyed = sum(1 for e in entries if e["env_keys"])
    required = sum(1 for e in entries if e["key_required"])
    print(f"gen_data_connectors: wrote {len(entries)} connectors ({len(entries) - keyed} keyless, {keyed} with keys, "
          f"{required} key-required) -> {CATALOG_PATH.relative_to(REPO)}; {INC_PATH.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
