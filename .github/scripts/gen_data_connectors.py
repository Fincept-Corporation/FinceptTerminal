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
  * params      — per command, the typed arguments in argv order (ParamInference; the argv rules are
    PARAMS_CONVENTION, copied into the catalogue's "convention"): positional reads and their
    `len()` guards (required / optional + default), helper readers (`_a(3, "10")`, `_arg(rest, 0)`),
    `--name value` options (parsed-option dicts and `startswith("--x=")` loops), dispatch dicts
    called as `handler(*args)` (the handler signature), aliases (`command = "org-datasets"`) and
    usage text; kind from conversions (int()/float()/`== "true"`), literal choice sets / lookup
    dicts (enum), argument names and defaults. A command whose arguments cannot be followed gets
    params null (unknown), counted by --report.
  * sample      — not inferred: default_command + default_args for a connector that is not
    key_required (or its first command without required params), or the overrides "sample".

Overrides for params / sample (connector_overrides.json, per script):
    "params": {"<command>": [<param>, ...]}        replace the command's list (null = unknown)
              {"<command>": {"<param>": {fields}}}  patch fields of inferred params by name
                                                    ({"<param>": null} drops one)
              {"*": {"<param>": {fields}}}          patch that name in every command
    "sample": {"command": "...", "args": [...]} | null
A <param> is {name, required, default, kind, choices?, help?, named?, variadic?}; the generator
fails on an unknown kind, an enum without choices or a patch naming a param the script lacks.

`key_required` is NOT guessed: it must be recorded per keyed connector in the overrides file
(true only when the connector cannot return data without the key). The generator fails when a
keyed connector has no recorded decision, so a new keyed script cannot ship unreviewed.

Usage:
  python .github/scripts/gen_data_connectors.py              # regenerate both outputs
  python .github/scripts/gen_data_connectors.py --check      # exit 1 when an output is stale
  python .github/scripts/gen_data_connectors.py --report F   # also write a per-script report (with
                                                             # the params-null / sample counts)

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
        self.dict_handlers = {}  # command -> FunctionDef of a handler given by name in a dispatch dict
        self.dispatch_dicts = set(used)
        for dname in used:
            d = dicts[dname]
            for k, v in zip(d.keys, d.values):
                # A handler given by name parses its own arguments — use its body as the branch.
                if isinstance(v, ast.Name) and v.id in funcs:
                    branch = list(funcs[v.id].body)
                    self.dict_handlers.setdefault(k.value, funcs[v.id])
                elif isinstance(v, ast.Attribute) and v.attr in funcs:
                    branch = list(funcs[v.attr].body)
                    self.dict_handlers.setdefault(k.value, funcs[v.attr])
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
        self.params: dict[str, list | None] = {}
        self.param_notes: dict[str, str] = {}
        try:
            ParamInference(self, usage).run()
        except RecursionError:
            self.params = {c: None for c in self.commands}


# ─────────────────────────────────────────────────────────────────────────────
# Typed parameters
# ─────────────────────────────────────────────────────────────────────────────

PARAM_KINDS = ("string", "int", "float", "date", "enum", "symbol", "country", "bool")
PARAM_FIELDS = ("name", "required", "default", "kind", "choices", "help", "named", "variadic")

PARAMS_CONVENTION = (
    "commands[].params lists a command's arguments in argv order: [{name, required, default, kind, choices?, help?, "
    "named?, variadic?}]; kind is one of string|int|float|date|enum|symbol|country|bool. params null = not known "
    "(fall back to args_hint); [] = the command takes no arguments. Build argv as: <command>, then the positional "
    "params (those without named) in list order: send a required param always; an optional param may be left out "
    "only together with every positional param after it; to send a later param, send the earlier optional one's "
    "default in its place (the scripts cannot skip a position). default is always present: a value reproduces the "
    "script's own default (\"\" where the script reads an empty argument as not given, whatever the kind); null "
    "means the script has no sendable default for it (it computes one or does without), so such a param can only "
    "be omitted, never skipped - when a later param is sent the caller must supply a value for it. For an enum the "
    "default, when not null, is one of choices. After the positionals, each named param that is given is one argv item "
    "--<name>=<value> (any order); a "
    "named bool is sent as --<name> alone when true and left out when false. A variadic param is the last "
    "positional and may be sent as several argv items. Values are strings: int/float in plain decimal, bool as "
    "true/false, date as YYYY-MM-DD, enum exactly one of choices, symbol/country as the user typed them (the help "
    "says which code system when it matters).")

_DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
_INT_RE = re.compile(r"^-?\d+$")
_FLOAT_RE = re.compile(r"^-?\d+\.\d+$")

_KIND_NAME_RULES = (
    ("symbol", re.compile(r"^(symbols?|tickers?|stock_?(symbol|code)s?|coin(_?id)?s?|coin_ids|base_symbol|"
                          r"instrument_?(id)?|pair|pairs|trading_?pair)$")),
    ("country", re.compile(r"^(country|countries|country_?(code|iso|iso2|iso3|name)s?|iso[23]?(_?codes?)?|iso_?code|"
                           r"reporters?|partners?|reporter_?(code|iso)s?|partner_?(code|iso)s?|economy|economies|"
                           r"nation|ref_area|area_code)$")),
    ("int", re.compile(r"^(limit|rows|page|pages|n|num|count|top|top_n|size|per_page|page_size|offset|skip|max|"
                       r"max_results|max_records|num_results|last_n|lookback|days|weeks|months|years|hours|minutes|"
                       r"depth|window|horizon|n_periods|last_n_periods|year|start_year|end_year|from_year|to_year|"
                       r"base_year|fyear|fiscal_year|congress|max_rows|row_limit|.+_(limit|count|days|year|years|page|"
                       r"rows|size))$")),
    ("float", re.compile(r"^(lat|lon|lng|latitude|longitude|radius|alt|altitude|elevation|(min|max)_?(lat|lon|lng)|"
                         r"(lat|lon|lng)_?(min|max)|observer_(lat|lng|lon|alt)|radius_km)$")),
    ("date", re.compile(r"^(date|dates|start|end|from|to|since|until|as_of|asof|start_?date|end_?date|from_?date|"
                        r"to_?date|date_?from|date_?to|observation_(start|end)|rate_date|ref_date|.+_date)$")),
)

_GENERIC_PARAM_NAMES = {"value", "val", "v", "x", "arg", "a", "s", "text", "item", "raw", "param", "p", "tok", "t",
                        "result", "res", "data", "q_", "i", "k", "n_", "args", "argv", "rest"}

_BOOL_NEGATION = {"y": "n", "yes": "no", "1": "0", "on": "off", "Y": "N", "YES": "NO", "Yes": "No"}


def _norm_param_name(name: str) -> str:
    return name.strip().strip("_-").replace("-", "_").lower()


def name_kind(name: str):
    n = _norm_param_name(name or "")
    for kind, rx in _KIND_NAME_RULES:
        if rx.match(n):
            return kind
    return None


def _call_name(call):
    if not isinstance(call, ast.Call):
        return None
    f = call.func
    return f.id if isinstance(f, ast.Name) else (f.attr if isinstance(f, ast.Attribute) else None)


def parse_usage(text: str):
    """Usage text ("<series_id> [start_date] [limit=10] [--type gilt|ois]") -> list of param dicts, or None."""
    if not text:
        return None
    toks = re.findall(r"<[^>]*>|\[[^\]]*\]|--[\w-]+|\.\.\.|\S+", text)
    out = []
    pending_flag = None
    for tok in toks:
        if tok == "...":
            if out:
                out[-1]["variadic"] = True
            continue
        if tok.startswith("--") and not tok.startswith("[") and not tok.startswith("<"):
            pending_flag = tok[2:]
            continue
        if tok[0] not in "<[":
            pending_flag = None
            continue
        required = tok[0] == "<"
        inner = tok[1:-1].strip()
        named = False
        if pending_flag is not None:
            name, named, rest = pending_flag, True, inner
            pending_flag = None
        else:
            if inner.startswith("--"):
                named = True
                inner = inner[2:]
            parts = inner.split(None, 1)
            if not parts:
                continue
            name, rest = parts[0], (parts[1] if len(parts) > 1 else "")
        p = {"name": name, "required": required, "default": None}
        if "=" in name:
            name, dflt = name.split("=", 1)
            p["name"] = name
            if "|" in dflt:
                p["choices"] = [c for c in dflt.split("|") if c]
            elif dflt:
                p["default"] = dflt
        if "|" in p["name"]:
            choices = [c for c in p["name"].split("|") if c]
            p["name"] = "arg"
            p["choices"] = choices
        if named and rest and not rest.startswith("<") and "|" in rest and " " not in rest.strip():
            p["choices"] = [c for c in rest.strip().split("|") if c]
            rest = ""
        if named and rest:
            rest = rest.strip("<>. ")
        if p.get("choices") and not all(re.fullmatch(r"[\w.+-]+", c) and not re.search(r"YYYY|MM|DD", c)
                                        for c in p["choices"]):
            p.pop("choices")  # "DD/Mon/YYYY|now" describes a format, not a closed set
        if "," in p["name"]:
            # <series1,series2,...>: ONE argument holding a comma-separated list, not a variadic
            first = p["name"].split(",")[0]
            p["name"] = re.sub(r"_?\d+$", "", first) + "s"
            rest = (rest + "; " if rest else "") + "comma-separated list"
        elif p["name"].endswith("..."):
            p["name"] = p["name"][:-3]
            p["variadic"] = True
        if rest and rest not in ("...",):
            p["help"] = rest.strip()
        if named:
            p["named"] = True
        p["name"] = re.sub(r"[^\w-]", "", p["name"]) or "arg"
        out.append(p)
    return out or None


class ParamInference:
    """Typed `params` for every command of one ScriptInfo, read from the AST only.

    Positional reads (`args[2] if len(args) > 2 else "AUS"`, `sys.argv[3]`, helper calls such as
    `_a(3, "10")` / `_arg(rest, 0, "ALL")`), named options (`kwargs.get("start")` after a `--name
    value` parser, `arg.startswith("--limit=")`), dispatch dicts of methods called as `fn(*args)`
    (the method signature), and usage text. A command whose arguments are consumed in a way this
    cannot follow (the list handed to another function, computed indexes) gets None — unknown —
    unless usage text fills the gap."""

    def __init__(self, info: "ScriptInfo", usage: dict):
        self.info = info
        self.mod = info.mod
        self.nodes = list(ast.walk(self.mod))  # every node once; the passes below reuse it
        self._walks = {}
        self.usage = dict(usage)
        for k, v in self._usage_dict_hints().items():
            # keep whichever text names more arguments ("series requires <key>" vs "<key> [start] [end]")
            if len(parse_usage(v) or []) > len(parse_usage(self.usage.get(k, "")) or []):
                self.usage[k] = v
        self.arg_lists = set(info.arg_lists)
        self.rest_lists = set(info.rest_lists)
        self.parent = {}
        for p in self.nodes:
            for ch in ast.iter_child_nodes(p):
                self.parent[id(ch)] = p
        self.funcs = {}
        for n in self.nodes:
            if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)):
                self.funcs.setdefault(n.name, n)
        self._drop_dict_named_lists()
        self.consts = self._module_consts()
        self.colls_global, self.colls_local = self._collections()
        self.helpers = self._find_helpers()
        self._find_rest_aliases()
        self.flag_parsers = self._find_flag_parsers()
        self.flag_dicts = self._find_flag_dicts()
        self.starred_dispatch = self._starred_dispatch()

    def _walk(self, node):
        """ast.walk(node) as a list, computed once per node."""
        got = self._walks.get(id(node))
        if got is None:
            got = self._walks[id(node)] = list(ast.walk(node))
        return got

    # ── tables ──────────────────────────────────────────────────────────────
    def _usage_dict_hints(self):
        """{"cmd": "[start] [end]  — what it does"} usage dicts keyed by command."""
        out = {}
        cmds = set(self.info.commands)
        for node in self.nodes:
            if isinstance(node, ast.Dict) and node.keys and all(
                    isinstance(k, ast.Constant) and isinstance(k.value, str) for k in node.keys) and all(
                    isinstance(v, ast.Constant) and isinstance(v.value, str) for v in node.values):
                keys = [k.value for k in node.keys]
                if sum(k in cmds for k in keys) < max(2, len(keys) // 2):
                    continue
                for k, v in zip(keys, node.values):
                    text = re.split(r"\s+(?:-|—|–|#|->|→)\s+|\s{2,}", v.value.strip())[0].strip()
                    if k in cmds and text and re.search(r"[<\[]|--", text):
                        out.setdefault(k, text)
        return out

    def _drop_dict_named_lists(self):
        """`args = {}` (a dict of options) is not the argument list even though it is called args."""
        list_bound, dict_bound = set(), set()
        for node in self.nodes:
            if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
                nm = node.targets[0].id
                v = node.value
                if isinstance(v, ast.Dict):
                    dict_bound.add(nm)
                elif (isinstance(v, ast.Subscript) and is_argv(v.value)) or is_argv(v):
                    list_bound.add(nm)
            elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                for a in node.args.args:
                    if a.arg in ("args", "argv"):
                        list_bound.add(a.arg)
        for nm in dict_bound - list_bound:
            self.arg_lists.discard(nm)
            self.rest_lists.discard(nm)

    def _module_consts(self):
        consts = {}
        scopes = [self.mod] + [n for n in self.nodes if isinstance(n, ast.ClassDef)]
        for scope in scopes:
            for node in scope.body:
                if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
                    ok, v = self._lit(node.value, use_consts=False)
                    if ok:
                        consts.setdefault(node.targets[0].id, v)
        return consts

    @staticmethod
    def _coll_literal(value):
        if isinstance(value, ast.Dict) and value.keys and all(
                isinstance(k, ast.Constant) and isinstance(k.value, str) for k in value.keys):
            return [k.value for k in value.keys]
        if isinstance(value, (ast.List, ast.Tuple, ast.Set)) and value.elts and all(
                isinstance(e, ast.Constant) and isinstance(e.value, str) for e in value.elts):
            return [e.value for e in value.elts]
        return None

    def _collections(self):
        glob, local = {}, {}
        for node in self.nodes:
            if not (isinstance(node, (ast.Assign, ast.AnnAssign))):
                continue
            t = node.targets[0] if isinstance(node, ast.Assign) and len(node.targets) == 1 else getattr(node, "target", None)
            nm = t.id if isinstance(t, ast.Name) else (t.attr if isinstance(t, ast.Attribute) else None)
            if nm is None or node.value is None:
                continue
            keys = self._coll_literal(node.value)
            if not keys:
                continue
            fn = self._enclosing_func(node)
            if fn is None or isinstance(t, ast.Attribute):
                glob.setdefault(nm, keys)
            else:
                local.setdefault(id(fn), {}).setdefault(nm, keys)
        return glob, local

    def _enclosing_func(self, node):
        cur = self.parent.get(id(node))
        while cur is not None:
            if isinstance(cur, (ast.FunctionDef, ast.AsyncFunctionDef)):
                return cur
            if isinstance(cur, ast.ClassDef):
                return None
            cur = self.parent.get(id(cur))
        return None

    def _lit(self, node, use_consts=True):
        """(True, value) for a literal default (str/int/float/bool/None, -number, module constant)."""
        if node is None:
            return False, None
        if isinstance(node, ast.Constant) and (node.value is None or isinstance(node.value, (str, int, float, bool))):
            return True, node.value
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            ok, v = self._lit(node.operand, use_consts)
            if ok and isinstance(v, (int, float)) and not isinstance(v, bool):
                return True, -v
        if use_consts:
            nm = node.id if isinstance(node, ast.Name) else (
                node.attr if isinstance(node, ast.Attribute) and isinstance(node.value, ast.Name)
                and node.value.id in ("self", "cls") else None)
            if nm is not None and nm in getattr(self, "consts", {}):
                return True, self.consts[nm]
            if isinstance(node, ast.Call) and _call_name(node) in ("str", "int", "float") and len(node.args) == 1:
                ok, v = self._lit(node.args[0], use_consts)
                if ok and v is not None:
                    try:
                        return True, {"str": str, "int": int, "float": float}[_call_name(node)](v)
                    except (TypeError, ValueError):
                        return False, None
        return False, None

    def _param_default(self, fn, idx):
        args = fn.args.args
        defaults = fn.args.defaults
        first_default = len(args) - len(defaults)
        if idx is None or idx < first_default:
            return True, None
        return self._lit(defaults[idx - first_default])

    def _find_helpers(self):
        """Functions that read one argv position given as an argument: `_a(n, d=None)` over sys.argv,
        `_arg(args, index, default)`, closures `arg(i, default)` over the argument list, and helpers
        built on those (`_ls(n)` = `_a(n).split(",")`)."""
        helpers = {}
        for name, fn in self.funcs.items():
            pnames = [a.arg for a in fn.args.args]
            if not pnames or len(fn.body) > 8 or name in ("main",):
                continue
            spec = None
            for n in self._walk(fn):
                if isinstance(n, ast.Subscript) and isinstance(n.ctx, ast.Load) and isinstance(n.slice, ast.Name) \
                        and n.slice.id in pnames:
                    lst = None
                    if is_argv(n.value):
                        lst = "argv"
                    elif isinstance(n.value, ast.Name):
                        if n.value.id in pnames:
                            lst = ("param", pnames.index(n.value.id))
                        elif n.value.id in self.rest_lists:
                            lst = "rest"
                        elif n.value.id in self.arg_lists:
                            lst = "args"
                    if lst is not None:
                        spec = {"list": lst, "index": pnames.index(n.slice.id)}
                        break
            if spec is None:
                continue
            lst_idx = spec["list"][1] if isinstance(spec["list"], tuple) else None
            others = [i for i in range(len(pnames)) if i not in (spec["index"], lst_idx)]
            spec["default"] = others[0] if others else None
            spec["default_value"] = self._param_default(fn, spec["default"]) if spec["default"] is not None else (True, None)
            src = ast.unparse(fn)
            spec["conv"] = "int" if re.search(r"\bint\(", src) else "float" if re.search(r"\bfloat\(", src) else None
            # `argv[n] if len(argv) > n and argv[n] else d` / `val if val else d` / `!= ""`: an empty
            # argument counts as absent, so "" is a value a caller can send to skip this position
            idx_name = pnames[spec["index"]]
            spec["empty_ok"] = any(
                isinstance(x, (ast.IfExp, ast.If)) and (
                    any(isinstance(s, ast.Subscript) and isinstance(s.slice, ast.Name) and s.slice.id == idx_name
                        for s in ast.walk(x.test))
                    or (isinstance(x, ast.IfExp) and isinstance(x.test, ast.Name) and isinstance(x.body, ast.Name)
                        and x.test.id == x.body.id))
                or (isinstance(x, ast.Compare) and any(isinstance(c, ast.Constant) and c.value == "" for c in x.comparators))
                for x in ast.walk(fn))
            helpers[name] = spec
        for name, fn in self.funcs.items():
            if name in helpers or name == "main":
                continue
            pnames = [a.arg for a in fn.args.args]
            if not pnames or len(fn.body) > 6:
                continue
            for n in self._walk(fn):
                if isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and n.func.id in helpers and n.args \
                        and isinstance(n.args[0], ast.Name) and n.args[0].id in pnames:
                    base = helpers[n.func.id]
                    if base["list"] not in ("argv", "args", "rest"):
                        break
                    src = ast.unparse(fn)
                    conv = ("int_list" if "int(" in src and ".split(" in src else
                            "list" if ".split(" in src else "int" if "int(" in src else
                            "float" if "float(" in src else None)
                    helpers[name] = {"list": base["list"], "index": pnames.index(n.args[0].id),
                                     "default": 1 if len(pnames) > 1 else None,
                                     "default_value": self._param_default(fn, 1) if len(pnames) > 1 else (True, None),
                                     "conv": conv, "empty_ok": base.get("empty_ok", False)}
                    break
        return helpers

    def _find_rest_aliases(self):
        """`rest = args[1:]` is the argument list without the command when it is then read by index
        (`rest[0]`, `_arg(rest, 0)`); when it is iterated or handed on whole it is a variadic value."""
        cands = {}
        for node in self.nodes:
            if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
                v = node.value
                if isinstance(v, ast.Subscript) and isinstance(v.slice, ast.Slice) and isinstance(v.value, ast.Name) \
                        and v.value.id in self.arg_lists and v.slice.upper is None \
                        and const_int(v.slice.lower) == 1:
                    cands[node.targets[0].id] = node
        for nm in cands:
            for n in self.nodes:
                indexed = isinstance(n, ast.Subscript) and isinstance(n.value, ast.Name) and n.value.id == nm \
                    and const_int(n.slice) is not None
                via_helper = isinstance(n, ast.Call) and _call_name(n) in self.helpers and any(
                    isinstance(a, ast.Name) and a.id == nm for a in n.args)
                if indexed or via_helper:
                    self.rest_lists.add(nm)
                    break

    @staticmethod
    def _has_dashdash_startswith(node):
        for n in ast.walk(node):
            if isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute) and n.func.attr == "startswith" \
                    and n.args and isinstance(n.args[0], ast.Constant) and isinstance(n.args[0].value, str) \
                    and n.args[0].value.startswith("--"):
                return True
        return False

    def _find_flag_parsers(self):
        return {name for name, fn in self.funcs.items()
                if name != "main" and self._has_dashdash_startswith(fn) and
                any(isinstance(n, ast.Return) and n.value is not None for n in self._walk(fn))
                and not any(isinstance(n, ast.If) and self.info._compare_commands(n.test, self.info.cmd_vars)
                            for n in self._walk(fn))}

    def _find_flag_dicts(self):
        out = set()
        for node in self.nodes:
            if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name) \
                    and isinstance(node.value, ast.Call) and _call_name(node.value) in self.flag_parsers:
                out.add(node.targets[0].id)
            if isinstance(node, (ast.For, ast.While)) and self._has_dashdash_startswith(node):
                for n in ast.walk(node):
                    if isinstance(n, ast.Assign) and len(n.targets) == 1 and isinstance(n.targets[0], ast.Subscript) \
                            and isinstance(n.targets[0].value, ast.Name):
                        out.add(n.targets[0].value.id)
        return out

    def _starred_dispatch(self):
        """True when dict-dispatched handlers are called as `handler(*args)` (their signature is the CLI)."""
        for node in self.nodes:
            if isinstance(node, ast.Call):
                for a in node.args:
                    if isinstance(a, ast.Starred) and self._is_list(a.value):
                        return True
        return False

    def _is_list(self, node):
        if is_argv(node):
            return True
        if isinstance(node, ast.Name) and (node.id in self.arg_lists or node.id in self.rest_lists):
            return True
        if isinstance(node, ast.Subscript) and isinstance(node.slice, ast.Slice) and self._is_list(node.value):
            return True
        return False

    def _position(self, sub):
        if not isinstance(sub, ast.Subscript):
            return None
        idx = const_int(sub.slice)
        if idx is None:
            return None
        if is_argv(sub.value):
            return idx - 1 if idx >= 2 else None
        if isinstance(sub.value, ast.Name):
            if sub.value.id in self.rest_lists:
                return idx + 1 if idx >= 0 else None
            if sub.value.id in self.arg_lists:
                return idx if idx >= 1 else None
        return None

    def _slice_start(self, sub):
        """Argument position where a slice of the argument list starts (`args[2:]` -> 2), else None."""
        if not (isinstance(sub, ast.Subscript) and isinstance(sub.slice, ast.Slice)):
            return None
        lo = sub.slice.lower
        k = 0 if lo is None else const_int(lo)
        if k is None:
            return None
        if is_argv(sub.value):
            return k - 1 if k >= 2 else None
        if isinstance(sub.value, ast.Name):
            if sub.value.id in self.rest_lists:
                return k + 1
            if sub.value.id in self.arg_lists:
                return k if k >= 1 else None
        return None

    # ── guards and checks ───────────────────────────────────────────────────
    @staticmethod
    def _has_len(node):
        return any(isinstance(x, ast.Call) and isinstance(x.func, ast.Name) and x.func.id == "len"
                   for x in ast.walk(node))

    def _len_positive(self, test):
        """True when `test` holds only when enough arguments were given (`len(args) > 2`)."""
        if isinstance(test, ast.BoolOp):
            return any(self._len_positive(v) for v in test.values) if isinstance(test.op, ast.And) else False
        if isinstance(test, ast.Compare) and len(test.ops) == 1:
            op = test.ops[0]
            left_len, right_len = self._has_len(test.left), self._has_len(test.comparators[0])
            if left_len and isinstance(op, (ast.Gt, ast.GtE, ast.Eq)):
                return True
            if right_len and isinstance(op, (ast.Lt, ast.LtE)):
                return True
        if isinstance(test, ast.Subscript) and isinstance(test.slice, ast.Slice):
            return True  # `if args[2:]:`
        return False

    @staticmethod
    def _is_error_block(stmts):
        for s in stmts or []:
            for n in ast.walk(s):
                if isinstance(n, ast.Raise):
                    return True
                if isinstance(n, ast.Call):
                    f = n.func
                    fname = f.id if isinstance(f, ast.Name) else (f.attr if isinstance(f, ast.Attribute) else "")
                    if fname == "exit" or fname.endswith("Error") or fname == "missing_key" or re.match(
                            r"(?i)^_?(fail|err|error|die|abort|usage)(_|$)", fname):
                        return True
                if isinstance(n, ast.Dict) and any(isinstance(k, ast.Constant) and k.value == "error" for k in n.keys):
                    return True
        return False

    @staticmethod
    def _catches_index_error(handler):
        t = handler.type
        names = []
        if isinstance(t, ast.Name):
            names = [t.id]
        elif isinstance(t, ast.Tuple):
            names = [e.id for e in t.elts if isinstance(e, ast.Name)]
        return "IndexError" in names

    # ── one read ────────────────────────────────────────────────────────────
    def _param_name(self, fname, idx):
        fn = self.funcs.get(fname)
        if fn is None:
            return None
        names = [a.arg for a in fn.args.args if a.arg not in ("self", "cls")]
        return names[idx] if idx < len(names) else None

    def _entry(self, pm, node):
        """What one argument read tells: its name, whether it is optional, its default, conversions."""
        e = {"name": None, "required": True, "default": (False, None), "conv": None, "binds": [],
             "line": getattr(node, "lineno", 0), "help": None, "bool_lits": None}
        # `if not arg(1): return {"error": ...}` - the read itself is checked for presence
        up = pm.get(id(node))
        test_of = pm.get(id(up)) if isinstance(up, ast.UnaryOp) and isinstance(up.op, ast.Not) else None
        if isinstance(test_of, ast.If) and test_of.test is up and self._is_error_block(test_of.body):
            e["forces_required"] = True
        cur = node
        while True:
            par = pm.get(id(cur))
            if par is None:
                break
            if isinstance(par, ast.IfExp):
                if cur is par.body and self._has_len(par.test):
                    e["required"] = False
                    e["default"] = self._lit(par.orelse)
                    # `args[2] if len(args) > 2 and args[2] else None`: "" also means "absent"
                    me = ast.dump(node)
                    e["empty_ok"] = any(ast.dump(x) == me for x in ast.walk(par.test) if isinstance(x, ast.Subscript))
                    cur = par
                    continue
                if cur is par.body:
                    cur = par
                    continue
                break
            if isinstance(par, ast.BoolOp):
                idx = next((i for i, v in enumerate(par.values) if v is cur), 0)
                if isinstance(par.op, ast.And) and any(self._has_len(v) for v in par.values[:idx]):
                    e["required"] = False
                if isinstance(par.op, ast.Or) and idx == 0 and len(par.values) > 1:
                    ok, v = self._lit(par.values[1])
                    if ok:
                        e["default"] = (True, v)
                cur = par
                continue
            if isinstance(par, ast.Call):
                fname = _call_name(par)
                if cur is par.func:
                    cur = par
                    continue
                if cur in par.args:
                    if fname == "int":
                        e["conv"] = e["conv"] or "int"
                        cur = par
                        continue
                    if fname == "float":
                        e["conv"] = e["conv"] or "float"
                        cur = par
                        continue
                    if fname in ("str", "loads", "unquote", "unquote_plus", "Path", "strip", "upper", "lower"):
                        if fname == "loads":
                            e["help"] = "JSON"
                        cur = par
                        continue
                    if fname in self.helpers:
                        break
                    if fname and fname in self.funcs:
                        i = par.args.index(cur)
                        e["binds"].append(("param", fname, i))
                        e["name"] = e["name"] or self._param_name(fname, i)
                    break
                break
            if isinstance(par, ast.keyword):
                e["name"] = e["name"] or par.arg
                call = pm.get(id(par))
                fname = _call_name(call)
                if fname and fname in self.funcs:
                    e["binds"].append(("kw", fname, par.arg))
                break
            if isinstance(par, ast.Attribute):
                if par.attr == "split":
                    e["conv"] = e["conv"] or "list"
                if par.attr in ("lower", "upper", "strip", "split", "replace", "casefold", "title", "rstrip",
                                "lstrip", "isdigit"):
                    cur = par
                    continue
                break
            if isinstance(par, ast.Compare):
                lits = [c.value for c in [par.left] + par.comparators
                        if isinstance(c, ast.Constant) and isinstance(c.value, str)]
                for c in par.comparators:
                    if isinstance(c, (ast.Tuple, ast.List, ast.Set)):
                        lits += [x.value for x in c.elts if isinstance(x, ast.Constant) and isinstance(x.value, str)]
                low = [x.lower() for x in lits]
                if lits and any(x in ("true", "false", "y", "yes", "1", "on", "n", "no", "0") for x in low) \
                        and all(x in ("true", "false", "y", "yes", "1", "on", "n", "no", "0", "t", "f") for x in low):
                    e["conv"] = "bool"
                    e["bool_lits"] = lits
                    e["bool_negated"] = isinstance(par.ops[0], (ast.NotEq, ast.NotIn))
                    cur = par
                    continue
                break
            if isinstance(par, (ast.Assign, ast.AnnAssign)):
                t = par.targets[0] if isinstance(par, ast.Assign) and len(par.targets) == 1 else getattr(par, "target", None)
                if isinstance(t, ast.Name):
                    e["name"] = t.id
                    e["binds"].append(("var", t.id))
                elif isinstance(t, ast.Attribute):
                    e["name"] = t.attr
                break
            if isinstance(par, ast.Tuple):
                # `lat, lon = float(args[1]), float(args[2])`
                asg = pm.get(id(par))
                if isinstance(asg, ast.Assign) and len(asg.targets) == 1 and isinstance(asg.targets[0], ast.Tuple) \
                        and asg.value is par and len(asg.targets[0].elts) == len(par.elts):
                    t = asg.targets[0].elts[next(i for i, x in enumerate(par.elts) if x is cur)]
                    if isinstance(t, ast.Name):
                        e["name"] = t.id
                        e["binds"].append(("var", t.id))
                break
            if isinstance(par, (ast.BinOp, ast.UnaryOp, ast.Subscript, ast.JoinedStr, ast.FormattedValue)):
                if isinstance(par, ast.Subscript) and cur is par.slice:
                    break
                cur = par
                continue
            break
        # Statement guards: `if len(args) > 2: x = args[2]` (optional unless the else branch fails),
        # `try: x = args[2] except IndexError: ...`.
        node2 = cur
        while True:
            par = pm.get(id(node2))
            if par is None:
                break
            if isinstance(par, ast.If) and node2 is not par.test and self._has_len(par.test):
                if self._len_positive(par.test) and any(node2 is s for s in par.body) and e["required"]:
                    e["required"] = self._is_error_block(par.orelse)
            if isinstance(par, ast.Try) and any(node2 is s for s in par.body) and \
                    any(self._catches_index_error(h) for h in par.handlers):
                e["required"] = False
            node2 = par
        return e

    # ── reads under a set of nodes ──────────────────────────────────────────
    def _collect(self, roots):
        slots, flags, opaque = {}, {}, []
        for root in roots:
            pm = {}
            for p in ast.walk(root):
                for ch in ast.iter_child_nodes(p):
                    pm[id(ch)] = p
            pm.setdefault(id(root), self.parent.get(id(root)))
            handled = set()
            flag_loops = [n for n in ast.walk(root) if isinstance(n, (ast.For, ast.While))
                          and self._has_dashdash_startswith(n)]
            in_flag_loop = set()
            for loop in flag_loops:
                for n in ast.walk(loop):
                    in_flag_loop.add(id(n))
                self._flag_loop(loop, pm, flags, opaque)
            for n in ast.walk(root):
                if id(n) in in_flag_loop and not (isinstance(n, ast.Call) and _call_name(n) in self.helpers):
                    continue
                if isinstance(n, ast.Call) and _call_name(n) in self.helpers and isinstance(n.func, ast.Name):
                    r = self._helper_read(n)
                    if r == "opaque":
                        opaque.append(f"{n.func.id}() with a computed index")
                        continue
                    if r is None:
                        continue
                    pos, default, conv, empty_ok = r
                    e = self._entry(pm, n)
                    e["required"] = False
                    e["default"] = default
                    e["empty_ok"] = empty_ok
                    if conv:
                        e["conv"] = conv if conv != "int_list" else "list"
                        if conv == "int_list":
                            e["help"] = "comma-separated integers"
                    slots.setdefault(pos, []).append(e)
                    for a in n.args:
                        handled.add(id(a))
                    continue
                pos = self._position(n)
                if pos is not None:
                    handled.add(id(n.value))
                    slots.setdefault(pos, []).append(self._entry(pm, n))
                    continue
                # named options from a parsed-options dict
                if isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute) and n.func.attr == "get" \
                        and isinstance(n.func.value, ast.Name) and n.func.value.id in self.flag_dicts and n.args \
                        and isinstance(n.args[0], ast.Constant) and isinstance(n.args[0].value, str):
                    e = self._entry(pm, n)
                    e["required"] = False
                    e["default"] = self._lit(n.args[1]) if len(n.args) > 1 else (True, None)
                    flags.setdefault(n.args[0].value, []).append(e)
                    handled.add(id(n.func.value))
                    continue
                if isinstance(n, ast.Subscript) and isinstance(n.ctx, ast.Load) and isinstance(n.value, ast.Name) \
                        and n.value.id in self.flag_dicts and isinstance(n.slice, ast.Constant) \
                        and isinstance(n.slice.value, str):
                    e = self._entry(pm, n)
                    flags.setdefault(n.slice.value, []).append(e)
                    handled.add(id(n.value))
                    continue
            # Uses of the argument list this cannot follow.
            for n in ast.walk(root):
                if id(n) in handled or id(n) in in_flag_loop:
                    continue
                if not (is_argv(n) or (isinstance(n, ast.Name) and isinstance(n.ctx, ast.Load)
                                       and (n.id in self.arg_lists or n.id in self.rest_lists))):
                    continue
                par = pm.get(id(n))
                if isinstance(par, ast.Attribute) and is_argv(par):
                    continue  # the `sys` Name inside sys.argv
                if par is None or (isinstance(par, ast.Call) and _call_name(par) in ("len", "bool")) or \
                        isinstance(par, (ast.Compare, ast.UnaryOp, ast.BoolOp, ast.If, ast.IfExp, ast.Assert)):
                    continue
                if isinstance(par, ast.Subscript) and par.value is n:
                    if self._position(par) is not None or const_int(par.slice) == 0 or (
                            is_argv(n) and const_int(par.slice) == 1):
                        continue
                    start = self._slice_start(par)
                    if start is not None:
                        why = self._slice_use(pm, par, start, slots, flags)
                        if why:
                            opaque.append(why)
                        continue
                    if isinstance(par.slice, ast.Slice) and is_argv(n):
                        continue  # sys.argv[1:] bound as the argument list
                    opaque.append(f"argument list indexed with {ast.unparse(par.slice)[:30]}")
                    continue
                if isinstance(par, (ast.Assign, ast.AnnAssign)):
                    continue  # args = sys.argv / aliasing
                if isinstance(par, ast.Call) and _call_name(par) in self.flag_parsers:
                    continue
                if isinstance(par, ast.Starred) and self.starred_dispatch:
                    continue
                opaque.append(f"argument list passed on ({type(par).__name__})")
        return slots, flags, opaque

    def _slice_use(self, pm, sub, start, slots, flags):
        """`" ".join(args[2:])` -> one free-text param; `rest = args[2:]` / `for s in args[2:]` -> variadic."""
        par = pm.get(id(sub))
        if isinstance(par, ast.Assign) and len(par.targets) == 1 and isinstance(par.targets[0], ast.Name) and (
                par.targets[0].id in self.rest_lists or par.targets[0].id in self.arg_lists):
            return None  # the argument list itself (`args = sys.argv[2:]`, `rest = args[1:]`)
        if isinstance(par, ast.Call) and isinstance(par.func, ast.Attribute) and par.func.attr == "join" \
                and sub in par.args:
            e = self._entry(pm, par)
            e["required"] = False if not e["required"] else e["required"]
            e["help"] = "free text"
            slots.setdefault(start, []).append(e)
            return None
        if isinstance(par, ast.Call) and _call_name(par) in self.flag_parsers:
            return None
        if isinstance(par, (ast.Assign, ast.For, ast.comprehension)) or (
                isinstance(par, ast.Call) and _call_name(par) in ("list", "enumerate", "tuple")):
            e = {"name": None, "required": False, "default": (True, None), "conv": None, "binds": [], "line":
                 sub.lineno, "help": None, "variadic": True, "bool_lits": None}
            if isinstance(par, ast.Assign) and len(par.targets) == 1 and isinstance(par.targets[0], ast.Name):
                e["name"] = par.targets[0].id
                e["binds"].append(("var", par.targets[0].id))
            elif isinstance(par, ast.For) and isinstance(par.target, ast.Name):
                e["name"] = par.target.id + "s"
            slots.setdefault(start, []).append(e)
            return None
        return f"argument list slice passed on ({type(par).__name__})"

    def _flag_loop(self, loop, pm, flags, opaque):
        """`for arg in sys.argv[2:]: if arg.startswith("--limit="): limit = int(arg.split("=")[1])` and
        `while i < len(argv): if argv[i] == "--start-year": start_year = int(argv[i + 1])`."""
        for n in ast.walk(loop):
            if not isinstance(n, ast.If):
                continue
            t = n.test
            flag, kind = None, None
            if isinstance(t, ast.BoolOp) and isinstance(t.op, ast.And):
                t = t.values[0]
            if isinstance(t, ast.Call) and isinstance(t.func, ast.Attribute) and t.func.attr == "startswith" \
                    and t.args and isinstance(t.args[0], ast.Constant) and isinstance(t.args[0].value, str):
                s = t.args[0].value
                if s.startswith("--") and len(s) > 3 and s.endswith("="):
                    flag, kind = s[2:-1], "value"
            elif isinstance(t, ast.Compare) and len(t.ops) == 1 and isinstance(t.ops[0], ast.Eq) and \
                    isinstance(t.comparators[0], ast.Constant) and isinstance(t.comparators[0].value, str) and \
                    t.comparators[0].value.startswith("--") and len(t.comparators[0].value) > 2:
                flag, kind = t.comparators[0].value[2:], "token"
            if flag is None:
                if any(isinstance(x, ast.Call) and isinstance(x.func, ast.Attribute) and x.func.attr == "append"
                       for s in n.orelse for x in ast.walk(s)):
                    opaque.append("positional values mixed with --options")
                continue
            # default unknown here: _merge takes the variable's earlier literal (`partner = "wld"`)
            e = {"name": flag.replace("-", "_"), "required": False, "default": (False, None), "conv": None,
                 "binds": [], "line": n.lineno, "help": None, "bool_lits": None}
            assign = next((s for s in n.body if isinstance(s, ast.Assign)), None)
            if assign is not None and len(assign.targets) == 1 and isinstance(assign.targets[0], ast.Name):
                e["name"] = assign.targets[0].id
                e["binds"].append(("var", assign.targets[0].id))
                e["line"] = assign.lineno  # the read is this assignment, not a later rebinding
                v = assign.value
                if isinstance(v, ast.Constant) and isinstance(v.value, bool):
                    e["conv"] = "bool"
                    e["bool_lits"] = ["true"]
                    e["default"] = (True, not v.value)
                    e["presence"] = True
                else:
                    fn = _call_name(v) if isinstance(v, ast.Call) else None
                    if fn in ("int", "float"):
                        e["conv"] = fn
            elif kind == "token":
                e["conv"] = "bool"
                e["presence"] = True
                e["default"] = (True, False)
            flags.setdefault(flag, []).append(e)

    def _helper_read(self, call):
        fname = call.func.id
        spec = self.helpers.get(fname)
        fn = self.funcs.get(fname)
        if spec is None or fn is None:
            return None

        def argn(k):
            if k is None:
                return None
            if k < len(call.args):
                return call.args[k]
            pname = fn.args.args[k].arg if k < len(fn.args.args) else None
            for kw in call.keywords:
                if kw.arg == pname:
                    return kw.value
            return None

        lst = spec["list"]
        if isinstance(lst, tuple):
            ln = argn(lst[1])
            if is_argv(ln):
                lst = "argv"
            elif isinstance(ln, ast.Name) and ln.id in self.rest_lists:
                lst = "rest"
            elif isinstance(ln, ast.Name) and ln.id in self.arg_lists:
                lst = "args"
            else:
                return None
        idx_node = argn(spec["index"])
        idx = const_int(idx_node) if idx_node is not None else None
        if idx is None:
            return "opaque"
        pos = {"argv": idx - 1, "args": idx, "rest": idx + 1}[lst]
        if pos < 1:
            return None
        dn = argn(spec["default"])
        default = self._lit(dn) if dn is not None else spec["default_value"]
        return pos, default, spec["conv"], spec.get("empty_ok", False)

    # ── enum choices ────────────────────────────────────────────────────────
    def _coll_keys(self, node, fn_scope):
        if isinstance(node, (ast.Tuple, ast.List, ast.Set, ast.Dict)):
            return self._coll_literal(node)
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr == "keys":
            return self._coll_keys(node.func.value, fn_scope)
        nm = node.id if isinstance(node, ast.Name) else (node.attr if isinstance(node, ast.Attribute) else None)
        if nm is None:
            return None
        if fn_scope is not None:
            loc = self.colls_local.get(id(fn_scope), {})
            if nm in loc:
                return loc[nm]
        return self.colls_global.get(nm)

    @staticmethod
    def _refers(node, names):
        node = strip_str_methods(node)
        return isinstance(node, ast.Name) and node.id in names

    def _choices(self, names, roots, fn_scope, depth=0, seen=None):
        """(choices, strict) the value bound to `names` is checked against, following it into script
        functions it is passed to (two levels)."""
        seen = seen if seen is not None else set()
        for root in roots:
            for n in ast.walk(root):
                if isinstance(n, ast.Compare) and len(n.ops) == 1 and isinstance(n.ops[0], (ast.In, ast.NotIn)) \
                        and self._refers(n.left, names):
                    keys = self._coll_keys(n.comparators[0], fn_scope)
                    if keys and len(keys) >= 2:
                        par = self.parent.get(id(n))
                        strict = isinstance(par, ast.If) and (
                            (isinstance(n.ops[0], ast.NotIn) and self._is_error_block(par.body)) or
                            (isinstance(n.ops[0], ast.In) and self._is_error_block(par.orelse)))
                        if strict:
                            return keys, strict
                if isinstance(n, ast.Subscript) and isinstance(n.ctx, ast.Load) and self._refers(n.slice, names):
                    keys = self._coll_keys(n.value, fn_scope)
                    if keys and len(keys) >= 2 and not isinstance(n.value, (ast.List, ast.Tuple)):
                        return keys, True
                if isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute) and n.func.attr == "get" and \
                        n.args and self._refers(n.args[0], names):
                    if len(n.args) > 1 and self._refers(n.args[1], names):
                        continue
                    keys = self._coll_keys(n.func.value, fn_scope)
                    if keys and len(keys) >= 2:
                        return keys, len(n.args) == 1
        if depth >= 2:
            return None, False
        for root in roots:
            for n in ast.walk(root):
                if not isinstance(n, ast.Call):
                    continue
                fname = _call_name(n)
                fn = self.funcs.get(fname)
                if fn is None or fname in self.helpers:
                    continue
                pnames = [a.arg for a in fn.args.args if a.arg not in ("self", "cls")]
                targets = set()
                for i, a in enumerate(n.args):
                    if self._refers(a, names) and i < len(pnames):
                        targets.add(pnames[i])
                for kw in n.keywords:
                    if kw.arg and self._refers(kw.value, names):
                        targets.add(kw.arg)
                if not targets or (fname, frozenset(targets)) in seen:
                    continue
                seen.add((fname, frozenset(targets)))
                got, strict = self._choices(targets, fn.body, fn, depth + 1, seen)
                if got:
                    return got, strict
        return None, False

    def _entry_choices(self, entries, roots):
        for e in entries:
            for b in e.get("binds", []):
                if b[0] == "var":
                    fn_scope = self._enclosing_func(roots[0]) if roots else None
                    got = self._choices({b[1]}, roots, fn_scope)
                elif b[0] in ("param", "kw"):
                    fn = self.funcs.get(b[1])
                    if fn is None:
                        continue
                    pname = self._param_name(b[1], b[2]) if b[0] == "param" else b[2]
                    if not pname:
                        continue
                    got = self._choices({pname}, fn.body, fn, 1)
                else:
                    continue
                if got[0]:
                    return got
        return None, False

    # ── assembly ────────────────────────────────────────────────────────────
    def _later_required(self, entry, roots):
        """`x = _a(2)` ... `if not x: return {"error": ...}` -> the argument is required after all."""
        names = {b[1] for b in entry.get("binds", []) if b[0] == "var"}
        scopes = [(names, roots, entry["line"])] if names else []
        # ... or the function it is handed to rejects it (`def get_quote(self, symbol): if not symbol: return error`)
        def guard_head(fn):
            """The first statements of a function, where an argument check sits."""
            body = [s for s in fn.body if not (isinstance(s, ast.Expr) and isinstance(s.value, ast.Constant))]
            head = body[:3]
            if head and isinstance(head[0], ast.Try):
                head = head[0].body[:3]
            return head

        for b in entry.get("binds", []):
            if b[0] in ("param", "kw") and b[1] in self.funcs:
                pname = self._param_name(b[1], b[2]) if b[0] == "param" else b[2]
                if pname:
                    scopes.append(({pname}, guard_head(self.funcs[b[1]]), 0))
        for b in entry.get("binds", []):
            if b[0] == "var":
                for root in roots:
                    for n in ast.walk(root):
                        fname = _call_name(n)
                        if fname in self.funcs and fname not in self.helpers:
                            for i, a in enumerate(n.args):
                                if self._refers(a, {b[1]}):
                                    pname = self._param_name(fname, i)
                                    if pname:
                                        scopes.append(({pname}, guard_head(self.funcs[fname]), 0))
        return any(self._rejects_empty(nm, rs, line) for nm, rs, line in scopes)

    def _rejects_empty(self, names, roots, line):
        rebinds = [x.lineno for r in roots for x in ast.walk(r) if isinstance(x, (ast.Assign, ast.AugAssign))
                   and any(isinstance(t, ast.Name) and t.id in names for t in
                           (x.targets if isinstance(x, ast.Assign) else [x.target]))
                   and getattr(x, "lineno", 0) > line]
        for root in roots:
            for n in ast.walk(root):
                if not isinstance(n, ast.If) or getattr(n, "lineno", 0) < line:
                    continue
                if any(lr < n.lineno for lr in rebinds):
                    continue  # the value was replaced before this check (`if not cik: cik = lookup(symbol)`)
                # `if not x:` / `if x is None:` / `if not a or not b:` followed by an error
                terms = n.test.values if isinstance(n.test, ast.BoolOp) and isinstance(n.test.op, ast.Or) else [n.test]
                hit = any((isinstance(t, ast.UnaryOp) and isinstance(t.op, ast.Not) and self._refers(t.operand, names)) or (
                    isinstance(t, ast.Compare) and len(t.ops) == 1 and isinstance(t.ops[0], ast.Is)
                    and self._refers(t.left, names) and isinstance(t.comparators[0], ast.Constant)
                    and t.comparators[0].value is None) for t in terms)
                if hit and self._is_error_block(n.body):
                    return True
        return False

    def _required_upto(self, roots):
        """The highest argument position a `if len(args) < 3: <error>` guard in `roots` demands."""
        best = 0

        def threshold(test):
            if isinstance(test, ast.BoolOp) and isinstance(test.op, ast.Or):
                return max((threshold(v) for v in test.values), default=0)
            if not (isinstance(test, ast.Compare) and len(test.ops) == 1):
                return 0
            op, left, right = test.ops[0], test.left, test.comparators[0]
            if isinstance(op, (ast.Gt, ast.GtE)):  # 3 > len(args)
                left, right = right, left
                op = ast.Lt() if isinstance(op, ast.Gt) else ast.LtE()
            if not isinstance(op, (ast.Lt, ast.LtE)):
                return 0
            k = const_int(right)
            if k is None:
                return 0
            plus = 0
            if isinstance(left, ast.BinOp) and isinstance(left.op, ast.Add) and const_int(left.right) is not None:
                plus, left = const_int(left.right), left.left
            if not (isinstance(left, ast.Call) and isinstance(left.func, ast.Name) and left.func.id == "len"
                    and left.args):
                return 0
            lst = left.args[0]
            need = k - plus + (1 if isinstance(op, ast.LtE) else 0)  # list length that passes
            if is_argv(lst):
                return need - 2
            if isinstance(lst, ast.Name) and lst.id in self.rest_lists:
                return need
            if isinstance(lst, ast.Name) and lst.id in self.arg_lists:
                return need - 1
            return 0

        for root in roots:
            for n in ast.walk(root):
                if isinstance(n, ast.If) and self._is_error_block(n.body):
                    best = max(best, threshold(n.test))
        return best

    def _conv_downstream(self, entries, roots):
        """int / float when the value is converted where it is used (`int(start) < year` in the
        function it is handed to, or later in the branch), else None."""
        scopes = []
        for e in entries:
            for b in e.get("binds", []):
                if b[0] == "var":
                    scopes.append(({b[1]}, roots))
                elif b[0] in ("param", "kw") and b[1] in self.funcs:
                    pname = self._param_name(b[1], b[2]) if b[0] == "param" else b[2]
                    if pname:
                        scopes.append(({pname}, self.funcs[b[1]].body))
        seen = set()

        def search(names, rs, depth):
            for r in rs:
                for n in ast.walk(r):
                    if not isinstance(n, ast.Call):
                        continue
                    if _call_name(n) in ("int", "float") and n.args and isinstance(n.func, ast.Name) \
                            and self._refers(n.args[0], names):
                        return n.func.id
                    fname = _call_name(n)
                    if depth < 5 and fname in self.funcs and fname not in self.helpers:
                        fn = self.funcs[fname]
                        pnames = [a.arg for a in fn.args.args if a.arg not in ("self", "cls")]
                        targets = {pnames[i] for i, a in enumerate(n.args) if i < len(pnames) and self._refers(a, names)}
                        targets |= {kw.arg for kw in n.keywords if kw.arg and self._refers(kw.value, names)}
                        key = (fname, frozenset(targets))
                        if targets and key not in seen:
                            seen.add(key)
                            got = search(targets, fn.body, depth + 1)
                            if got:
                                return got
            return None

        for names, rs in scopes:
            got = search(names, rs, 1)
            if got:
                return got
        return None

    def _prior_literal(self, roots, name, line):
        best = None
        for root in roots:
            for n in ast.walk(root):
                if isinstance(n, ast.Assign) and len(n.targets) == 1 and isinstance(n.targets[0], ast.Name) \
                        and n.targets[0].id == name and getattr(n, "lineno", 0) < line:
                    ok, v = self._lit(n.value)
                    if ok and (best is None or n.lineno > best[0]):
                        best = (n.lineno, v)
        return (True, best[1]) if best else (False, None)

    def _merge(self, entries, roots, usage_tok, named=False):
        entries = sorted(entries, key=lambda e: e["line"])
        names = [e["name"] for e in entries if e["name"]]
        good = [n for n in names if _norm_param_name(n) not in _GENERIC_PARAM_NAMES and not n.startswith("_")]
        name = (good or names or [None])[0]
        if usage_tok and usage_tok["name"] != "arg" and (
                name is None or _norm_param_name(name) in _GENERIC_PARAM_NAMES or name.startswith("_")
                or (len(name) <= 2 and len(usage_tok["name"]) > 2)):
            name = usage_tok["name"]
        name = (name or "arg").lstrip("_") or "arg"
        required = all(e["required"] for e in entries)
        if required and any(e.get("variadic") for e in entries):
            required = False
        default = next((e["default"] for e in entries if e["default"][0]), (False, None))
        if not required and not default[0]:
            for e in entries:
                if e["name"] and not e["required"]:
                    d = self._prior_literal(roots, e["name"], e["line"])
                    if d[0]:
                        default = d
                        break
        if not required and default[1] in (None, "") and (
                any(e.get("forces_required") for e in entries) or any(self._later_required(e, roots) for e in entries)):
            required = True
        conv = next((e["conv"] for e in entries if e["conv"]), None) or self._conv_downstream(entries, roots)
        bool_e = next((e for e in entries if e["conv"] == "bool"), None)
        help_ = next((e["help"] for e in entries if e.get("help")), None)
        choices, strict = self._entry_choices(entries, roots)
        if usage_tok:
            if not choices and usage_tok.get("choices"):
                choices, strict = usage_tok["choices"], True
            uh = usage_tok.get("help")
            # a named option's usage word is an example value ("--compounded Y"), not help
            if not help_ and uh and not (named and (uh in (choices or []) or " " not in uh.strip())):
                help_ = uh
            if not default[0] and not required and usage_tok.get("default") is not None:
                default = (True, usage_tok["default"])
        if not required and (not default[0] or default[1] is None) and entries and all(
                e.get("empty_ok") for e in entries):
            default = (True, "")  # the script reads an empty argument as absent: "" skips this position
        p = {"name": name, "required": required, "default": default[1] if default[0] else None}
        dval = p["default"]
        if conv == "bool" and bool_e is not None:
            lits = bool_e.get("bool_lits") or []
            low = [x.lower() for x in lits]
            if bool_e.get("presence") or "true" in low or "false" in low:
                kind = "bool"
                if isinstance(dval, str) and dval.lower() in ("true", "false"):
                    dval = dval.lower() == "true"
                elif dval is None and not required:
                    dval = bool(bool_e.get("bool_negated"))
            else:
                yes = next((x for x in lits if x.lower() in ("y", "yes", "1", "on")), lits[0] if lits else "true")
                no = _BOOL_NEGATION.get(yes, "false")
                kind, choices, strict = "enum", [yes, no], True
                if isinstance(dval, bool) or dval is None:
                    dval = yes if dval else no
        else:
            kind = self._decide_kind(name, conv, dval, choices, strict)
        if kind == "int" and isinstance(dval, str) and _INT_RE.match(dval.strip()):
            dval = int(dval)
        elif kind == "float" and isinstance(dval, str) and (_FLOAT_RE.match(dval.strip()) or _INT_RE.match(dval.strip())):
            dval = float(dval)
        elif kind == "float" and isinstance(dval, int) and not isinstance(dval, bool):
            dval = float(dval)
        if kind in ("int", "float") and isinstance(dval, str) and dval != "":
            kind = "string"
        p["default"] = None if required else dval
        p["kind"] = kind
        if kind == "enum":
            p["choices"] = list(dict.fromkeys(choices))[:400]
            if isinstance(p["default"], str) and p["default"] not in p["choices"] and not required:
                p["choices"].append(p["default"])
        if conv == "list" and not help_:
            help_ = "comma-separated list"
        if help_:
            p["help"] = help_
        if named:
            p["named"] = True
        from_usage_only = all(e["line"] == 10 ** 9 for e in entries)
        if any(e.get("variadic") for e in entries) or (usage_tok and usage_tok.get("variadic") and from_usage_only):
            p["variadic"] = True  # the code's reads win over a usage text's "..." (boc "[currencies...]")
        return p

    @staticmethod
    def _decide_kind(name, conv, default, choices, strict):
        rule = name_kind(name)
        if conv == "list":  # one argument holding a comma-separated list ("2020,2021", "USD,EUR")
            return rule if rule in ("symbol", "country") else "string"
        if choices and (strict or rule not in ("symbol", "country")):
            return "enum"
        if conv in ("int", "float"):
            return conv
        if rule in ("symbol", "country"):
            return rule
        if isinstance(default, bool):
            return "bool"
        if isinstance(default, int):
            return "int"
        if isinstance(default, float):
            return "float"
        if isinstance(default, str) and default:
            d = default.strip()
            if _INT_RE.match(d) and not (len(d) > 1 and d.startswith("0")):
                return "int"
            if _FLOAT_RE.match(d):
                return "float"
            if _DATE_RE.match(d):
                return "date"
        if rule == "date":
            return "date" if default in (None, "") or (isinstance(default, str) and _DATE_RE.match(default)) else "string"
        if rule in ("int", "float"):
            return rule if default in (None, "") else "string"
        return "string"

    def _signature_params(self, fn):
        args = [a for a in fn.args.args if a.arg not in ("self", "cls")]
        first_default = len(fn.args.args) - len(fn.args.defaults)
        out = []
        for a in args:
            i = fn.args.args.index(a)
            if i >= first_default:
                ok, v = self._lit(fn.args.defaults[i - first_default])
                required, default = False, (v if ok else None)
            else:
                required, default = True, None
            ann = a.annotation
            ann_name = ann.id if isinstance(ann, ast.Name) else (
                ann.slice.id if isinstance(ann, ast.Subscript) and isinstance(ann.slice, ast.Name) else None)
            conv = {"int": "int", "float": "float"}.get(ann_name)
            if ann_name == "bool":
                kind = "bool"
            else:
                kind = self._decide_kind(a.arg, conv, default, None, False)
            p = {"name": a.arg, "required": required, "default": None if required else default, "kind": kind}
            if kind == "bool" and isinstance(default, bool):
                p["default"] = default
            out.append(p)
        if fn.args.vararg is not None:
            out.append({"name": fn.args.vararg.arg, "required": False, "default": None, "kind": "string",
                        "variadic": True})
        return out

    @staticmethod
    def _postprocess(params):
        if not params:
            return params
        by = {p["name"]: p for p in params}
        # Range ends share a kind: `start` = "2020-Q1" makes `end` a period too, not a date.
        for a, b in (("start", "end"), ("start_date", "end_date"), ("from", "to"), ("start_period", "end_period"),
                     ("from_date", "to_date"), ("date_from", "date_to"), ("start_year", "end_year")):
            pa, pb = by.get(a), by.get(b)
            if not pa or not pb or pa["kind"] == pb["kind"]:
                continue
            if pa["default"] not in (None, "") and pb["default"] in (None, ""):
                src, dst = pa, pb
            elif pb["default"] not in (None, "") and pa["default"] in (None, ""):
                src, dst = pb, pa
            else:
                continue
            if src["kind"] in ("string", "int", "date") and dst["kind"] in ("string", "int", "date"):
                dst["kind"] = src["kind"]
                if src.get("help") and not dst.get("help"):
                    dst["help"] = src["help"]
        seen = set()
        for i, p in enumerate(params):
            if p["name"] in seen:
                p["name"] = f"{p['name']}_{i + 1}"
            seen.add(p["name"])
            d = p.get("default")
            if p["kind"] == "int" and isinstance(d, str) and _INT_RE.match(d.strip()):
                p["default"] = int(d)
            elif p["kind"] == "float" and isinstance(d, (str, int)) and not isinstance(d, bool):
                try:
                    p["default"] = float(d)
                except ValueError:
                    pass
            elif p["kind"] == "bool" and isinstance(d, str) and d.lower() in ("true", "false"):
                p["default"] = d.lower() == "true"
        return params

    def _params_preamble(self):
        """Statements before the dispatch chain (as ScriptInfo.preamble), also recognising a chain
        that opens with a combined test (`if command in (...) and not country: error`)."""
        info = self.info

        def is_dispatch(test):
            if info._compare_commands(test, info.cmd_vars):
                return True
            return isinstance(test, ast.BoolOp) and isinstance(test.op, ast.And) and any(
                info._compare_commands(v, info.cmd_vars) for v in test.values)

        def before(body):
            for idx, stmt in enumerate(body):
                if isinstance(stmt, ast.If) and is_dispatch(stmt.test):
                    return list(body[:idx])
                if isinstance(stmt, ast.Try):
                    inner = before(stmt.body)
                    if inner is not None:
                        return list(body[:idx]) + inner
            return None

        for fn in self.nodes:
            if isinstance(fn, (ast.FunctionDef, ast.AsyncFunctionDef)):
                pre = before(fn.body)
                if pre is not None:
                    return pre
        return info.preamble

    def run(self):
        info = self.info
        preamble = self._params_preamble()
        pre_slots, pre_flags, _ = self._collect(preamble) if preamble else ({}, {}, [])
        for c in info.commands:
            try:
                params, note = self._command(c, pre_slots, pre_flags)
                params = self._postprocess(params)
            except RecursionError:
                params, note = None, "analysis too deep"
            info.params[c] = params
            if note:
                info.param_notes[c] = note
        # `elif command == "datasets": command = "org-datasets"`: an alias takes its target's params
        import copy
        for c in info.commands:
            target = self._alias_target(c)
            if target and info.params.get(target) is not None:
                info.params[c] = copy.deepcopy(info.params[target])
                info.param_notes[c] = f"alias of {target}"

    def _alias_target(self, c):
        for stmt in self.info.branches.get(c) or []:
            if isinstance(stmt, ast.Assign) and len(stmt.targets) == 1 and isinstance(stmt.targets[0], ast.Name) \
                    and stmt.targets[0].id in self.info.cmd_vars and isinstance(stmt.value, ast.Constant) \
                    and stmt.value.value in self.info.commands and stmt.value.value != c:
                return stmt.value.value
        return None

    def _command(self, c, pre_slots, pre_flags):
        nodes = self.info.branches.get(c) or []
        usage = parse_usage(self.usage.get(c, ""))
        handler = self.info.dict_handlers.get(c)
        if handler is not None and self.starred_dispatch:
            return self._signature_params(handler), "handler signature"
        if not nodes:
            return (usage, "usage text only") if usage is not None else (None, "no dispatch branch")
        # A branch that calls a script function which reads sys.argv itself: read that body too.
        extra, seen = [], set()
        for r in nodes:
            for n in ast.walk(r):
                fname = _call_name(n)
                fn = self.funcs.get(fname) if fname else None
                if fn is None or fname in seen or fname in self.helpers or fname in self.flag_parsers \
                        or fname == "main" or fn is self.info.dict_handlers.get(c):
                    continue
                seen.add(fname)
                if any(is_argv(x) for x in self._walk(fn)) and not any(
                        isinstance(x, ast.If) and self.info._compare_commands(x.test, self.info.cmd_vars)
                        for x in self._walk(fn)):
                    extra.extend(fn.body)
        nodes = list(nodes) + extra
        slots, flags, opaque = self._collect(nodes)
        used = {n.id for r in nodes for n in ast.walk(r) if isinstance(n, ast.Name) and isinstance(n.ctx, ast.Load)}
        for pos, entries in pre_slots.items():
            mine = [e for e in entries if any(b[0] == "var" and b[1] in used for b in e["binds"])]
            if mine:
                slots.setdefault(pos, []).extend(mine)
            elif pos in slots:
                slots[pos].extend(e for e in entries if not e["binds"])
        for fl, entries in pre_flags.items():
            mine = [e for e in entries if any(b[0] == "var" and b[1] in used for b in e["binds"]) or not e["binds"]]
            if mine and (any(b[0] == "var" and b[1] in used for e in mine for b in e["binds"])):
                flags.setdefault(fl, []).extend(mine)
        upos = [t for t in (usage or []) if not t.get("named")]
        unamed = {t["name"].replace("-", "_"): t for t in (usage or []) if t.get("named")}
        note = None
        if opaque:
            if usage is None:
                return None, "; ".join(dict.fromkeys(opaque))[:160]
            note = "usage text fills: " + "; ".join(dict.fromkeys(opaque))[:120]
            for i, tok in enumerate(upos, start=1):
                if i not in slots:
                    slots[i] = [{"name": tok["name"], "required": tok["required"],
                                 "default": (tok.get("default") is not None, tok.get("default")), "conv": None,
                                 "binds": [], "line": 10 ** 9, "help": tok.get("help"), "bool_lits": None,
                                 "variadic": tok.get("variadic", False)}]
            for nm, tok in unamed.items():
                if nm not in flags and tok["name"] not in flags:
                    flags[tok["name"]] = [{"name": tok["name"], "required": tok["required"],
                                           "default": (True, tok.get("default")), "conv": None, "binds": [],
                                           "line": 10 ** 9, "help": tok.get("help"), "bool_lits": None}]
        if slots and sorted(slots) != list(range(1, max(slots) + 1)):
            missing = [i for i in range(1, max(slots) + 1) if i not in slots]
            for i in missing:
                if i <= len(upos):
                    tok = upos[i - 1]
                    slots[i] = [{"name": tok["name"], "required": tok["required"],
                                 "default": (tok.get("default") is not None, tok.get("default")), "conv": None,
                                 "binds": [], "line": 10 ** 9, "help": tok.get("help"), "bool_lits": None}]
            if sorted(slots) != list(range(1, max(slots) + 1)):
                return None, f"argument positions {sorted(slots)} have a gap"
        params = []
        upto = self._required_upto(self.info.branches.get(c) or [])
        for pos in sorted(slots):
            p = self._merge(slots[pos], nodes, upos[pos - 1] if pos <= len(upos) else None)
            if pos <= upto and not p["required"]:
                p["required"] = True  # `if len(sys.argv) < 3: usage error` guards this position
                p["default"] = None
            params.append(p)
        for fl in flags:
            tok = unamed.get(fl.replace("-", "_"))
            p = self._merge(flags[fl], nodes, tok, named=True)
            p["name"] = fl
            params.append(p)
        # a variadic param swallows the rest: nothing positional may follow it
        for i, p in enumerate(params):
            if p.get("variadic") and not p.get("named") and any(not q.get("named") for q in params[i + 1:]):
                return None, "positional arguments after a variadic one"
        return params, note


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


def normalize_param(script: str, cmd: str, p, errors: list):
    """One param object in field order, with the documented defaults filled in."""
    if not isinstance(p, dict) or not isinstance(p.get("name"), str) or not p["name"]:
        errors.append(f"{script}: params for '{cmd}' contains an entry without a name: {p!r}")
        return None
    unknown = set(p) - set(PARAM_FIELDS)
    if unknown:
        errors.append(f"{script}: params '{cmd}.{p['name']}' has unknown field(s) {sorted(unknown)}")
    out = {"name": p["name"], "required": bool(p.get("required", False)),
           "default": None if p.get("required") else p.get("default"), "kind": p.get("kind", "string")}
    if out["kind"] not in PARAM_KINDS:
        errors.append(f"{script}: params '{cmd}.{p['name']}' has unknown kind '{out['kind']}'")
    if out["kind"] == "enum":
        ch = p.get("choices")
        if not isinstance(ch, list) or not ch or not all(isinstance(x, str) for x in ch):
            errors.append(f"{script}: params '{cmd}.{p['name']}' is an enum without a list of string choices")
        out["choices"] = ch
    for k in ("help",):
        if p.get(k):
            out[k] = p[k]
    for k in ("named", "variadic"):
        if p.get(k):
            out[k] = True
    return out


def apply_param_overrides(script: str, cmd: str, inferred, params_ov: dict, errors: list):
    """overrides.params: {"<command>": [full list] | null | {"<param>": {fields} | null}, "*": {...}}."""
    import copy

    params = copy.deepcopy(inferred)
    star = params_ov.get("*")
    if isinstance(star, dict) and params is not None:
        for p in params:
            patch = star.get(p["name"])
            if isinstance(patch, dict):
                p.update(patch)
    if cmd in params_ov:
        spec = params_ov[cmd]
        if spec is None:
            return None
        if isinstance(spec, list):
            params = spec
        elif isinstance(spec, dict):
            if params is None:
                errors.append(f"{script}: overrides.params patches '{cmd}' but its params are unknown - give the full list")
                return None
            by_name = {p["name"]: p for p in params}
            for pname, patch in spec.items():
                if pname not in by_name:
                    errors.append(f"{script}: overrides.params patches '{cmd}.{pname}', which the script does not read")
                    continue
                if patch is None:
                    params = [p for p in params if p["name"] != pname]
                else:
                    by_name[pname].update(patch)
                    if "kind" in patch and patch["kind"] != "enum":
                        by_name[pname].pop("choices", None)
        else:
            errors.append(f"{script}: overrides.params['{cmd}'] must be a list, an object or null")
    if params is None:
        return None
    out = [normalize_param(script, cmd, p, errors) for p in params]
    return [p for p in out if p is not None]


def derive_sample(script: str, ov: dict, cmd_objs: list, default_command: str, key_required: bool, errors: list):
    """The keyless call the Data Sources tab runs to show a connector: {command, args} or None."""
    names = [c["name"] for c in cmd_objs]
    if "sample" in ov:
        s = ov["sample"]
        if s is None:
            return None
        if not isinstance(s, dict) or s.get("command") not in names or not isinstance(s.get("args", []), list):
            errors.append(f"{script}: overrides.sample must be null or {{command, args}} naming a command")
            return None
        return {"command": s["command"], "args": [str(a) for a in s.get("args", [])]}
    if key_required:
        return None
    by = {c["name"]: c for c in cmd_objs}
    args = [str(a) for a in ov.get("default_args", [])]
    params = by[default_command]["params"]
    if params is None or len(args) >= sum(1 for p in params if p["required"]):
        return {"command": default_command, "args": args}
    for c in cmd_objs:
        if c["params"] is not None and not any(p["required"] for p in c["params"]):
            return {"command": c["name"], "args": []}
    return None


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
        params_ov = ov.get("params", {})
        for c in params_ov:
            if c != "*" and c not in commands:
                errors.append(f"{script}: overrides.params names an unknown command '{c}'")
        cmd_objs = []
        for c in commands:
            hint = hint_ov.get(c, info.hints.get(c, ""))
            params = apply_param_overrides(script, c, info.params.get(c), params_ov, errors)
            cmd_objs.append({"name": c, "args_hint": hint, "params": params})
        rep["params"] = {c["name"]: c["params"] for c in cmd_objs}
        rep["param_notes"] = {c: info.param_notes[c] for c in commands if c in info.param_notes}

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

        sample = derive_sample(script, ov, cmd_objs, default_command, key_required, errors)
        rep["sample_source"] = "override" if "sample" in ov else "derived"

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
        entry["sample"] = sample
        entries.append(entry)

    entries.sort(key=lambda e: (CATEGORIES.index(e["category"]) if e["category"] in CATEGORIES else 99,
                                e["title"].lower()))
    return entries, errors


def param_summary(entries: list[dict]) -> dict:
    cmds = [(e["script"], c) for e in entries for c in e["commands"]]
    null_by_script = {}
    for s, c in cmds:
        if c["params"] is None:
            null_by_script.setdefault(s, []).append(c["name"])
    return {
        "commands": len(cmds),
        "params_typed": sum(1 for _, c in cmds if c["params"] is not None),
        "params_none": sum(1 for _, c in cmds if c["params"] == []),
        "params_null": sum(1 for _, c in cmds if c["params"] is None),
        "params_null_by_script": null_by_script,
        "samples": sum(1 for e in entries if e.get("sample")),
        "samples_null": sum(1 for e in entries if not e.get("sample")),
    }


def render_catalog(entries: list[dict]) -> str:
    counts = {c: 0 for c in CATEGORIES}
    for e in entries:
        counts[e["category"]] = counts.get(e["category"], 0) + 1
    doc = {
        "schema": SCHEMA_VERSION,
        "generated_by": ".github/scripts/gen_data_connectors.py — do not edit by hand; "
                        "fix fincept-qt/scripts/connector_overrides.json or the script, then regenerate",
        "convention": "python <script> <command> [args...] -> JSON on stdout; {\"error\": ...} means failure. "
                      + PARAMS_CONVENTION + " sample = {command, args}: a keyless call that returns a table quickly, "
                      "for showing the connector; null = no keyless call can show it (every command needs an API "
                      "key - key_required - or a user value such as a file path, or the upstream is broken).",
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
        summary = param_summary(entries)
        with open(args.report, "w", encoding="utf-8") as f:
            json.dump({"summary": summary, "scripts": report}, f, indent=1, ensure_ascii=False)
        print(f"gen_data_connectors: params: {summary['commands']} commands - {summary['params_typed']} typed "
              f"({summary['params_none']} without arguments), {summary['params_null']} unknown (params: null); "
              f"samples: {summary['samples']} set, {summary['samples_null']} null")

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
