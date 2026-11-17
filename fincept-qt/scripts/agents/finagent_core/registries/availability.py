"""
Availability probe shared by the tool / model / embedder / vector-DB registries.

The catalogues list every integration the terminal knows about, but most of
them depend on an optional third-party package (tavily-python, lancedb,
boto3, ...) and agno renames classes between releases. Listing an entry that
cannot load, and then dropping it silently when an agent asks for it, made the
Agent Studio show "106 tools" while half of them could never run.

class_unavailable_reason() imports the catalogue path once per process and
returns None when the class is usable, else a short reason naming what is
missing — agno's own ImportError text already says which package to install.
"""

import ast
import contextlib
import importlib
import importlib.util
import io
import logging
import os
import sys
from typing import Dict, List, Optional, Tuple

_cache: Dict[str, Optional[str]] = {}
_static_cache: Dict[str, Optional[str]] = {}

# Packages that are part of this code base or of agno itself: their presence is
# not what the probe is about (a broken one shows up at real import time).
_INTERNAL_ROOTS = {"agno", "finagent_core", "fincept_mcp", "__future__"}
_STDLIB = set(getattr(sys, "stdlib_module_names", ()))


def _reason_from_import_error(exc: ImportError) -> str:
    text = str(exc).strip()
    first = text.splitlines()[0].strip() if text else ""
    name = getattr(exc, "name", None)
    if first.startswith("No module named") and name:
        return f"needs the Python package '{name.split('.')[0]}' (not installed)"
    return first or f"needs a package that is not installed ({type(exc).__name__})"


def class_unavailable_reason(class_path: str) -> Optional[str]:
    """None when `module.Class` imports, else why it cannot be used. Cached."""
    if class_path in _cache:
        return _cache[class_path]
    module_path, class_name = class_path.rsplit(".", 1)
    reason: Optional[str] = None
    # Some integrations print or log while importing; keep stdout (the JSON
    # channel) clean and the log quiet for a probe that may touch 150 modules.
    root = logging.getLogger()
    previous = root.manager.disable
    logging.disable(logging.CRITICAL)
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            module = importlib.import_module(module_path)
        if not hasattr(module, class_name):
            reason = (f"{class_name} does not exist in {module_path} "
                      f"(the installed agno version renamed or removed it)")
    except ImportError as e:
        reason = _reason_from_import_error(e)
    except Exception as e:  # a broken optional dependency must not break listing
        reason = f"{type(e).__name__}: {str(e).strip()[:200]}"
    finally:
        logging.disable(previous)
    _cache[class_path] = reason
    return reason


def _package_present(dotted: str) -> bool:
    """Is the package an import statement names installed? Checks the
    top-level package without importing it; for namespace roots (google.*,
    azure.*) the full dotted name is resolved, since the root alone says
    nothing."""
    top = dotted.split(".")[0]
    if top in _STDLIB or top in _INTERNAL_ROOTS:
        return True
    try:
        spec = importlib.util.find_spec(top)
    except Exception:
        return False
    if spec is None:
        return False
    is_namespace = spec.origin in (None, "namespace") or not spec.has_location
    if "." in dotted and is_namespace:
        try:
            return importlib.util.find_spec(dotted) is not None
        except Exception:
            return False
    return True


def _is_namespace(dotted: str) -> bool:
    """True when `dotted` is an installed namespace package (no __init__.py),
    e.g. google.cloud: `from google.cloud import bigquery` then needs the
    submodule google.cloud.bigquery, not just google.cloud."""
    try:
        spec = importlib.util.find_spec(dotted)
    except Exception:
        return False
    return spec is not None and spec.submodule_search_locations is not None and \
        spec.origin in (None, "namespace")


def _module_file(module_path: str) -> Optional[str]:
    """Source file of a module, found from its top-level package's location —
    without importing the intermediate packages (importlib.util.find_spec on a
    submodule executes every parent package, which defeats a static probe)."""
    parts = module_path.split(".")
    try:
        root = importlib.util.find_spec(parts[0])
    except Exception:
        return None
    if root is None:
        return None
    if not root.submodule_search_locations:
        return str(root.origin) if len(parts) == 1 and str(root.origin).endswith(".py") else None
    for base in root.submodule_search_locations:
        p = os.path.join(base, *parts[1:])
        init = os.path.join(p, "__init__.py")
        if os.path.isdir(p) and os.path.exists(init):
            return init
        if os.path.exists(p + ".py"):
            return p + ".py"
    return None


def _resolve_relative(module_path: str, is_package: bool, level: int, name: Optional[str]) -> str:
    parts = module_path.split(".")
    base = parts if is_package else parts[:-1]
    if level > 1:
        base = base[: len(base) - (level - 1)]
    return ".".join(base + ([name] if name else []))


def _raise_message(handler: ast.ExceptHandler) -> Optional[str]:
    """The literal text of `raise ImportError("...")` in an except block."""
    for node in ast.walk(handler):
        if isinstance(node, ast.Raise) and isinstance(node.exc, ast.Call) and node.exc.args:
            arg = node.exc.args[0]
            if isinstance(arg, ast.Constant) and isinstance(arg.value, str):
                return arg.value.strip()
            if isinstance(arg, ast.JoinedStr):
                text = "".join(v.value for v in arg.values if isinstance(v, ast.Constant))
                return text.strip() or None
    return None


def _catches_import_error(handler: ast.ExceptHandler) -> bool:
    names: List[str] = []
    t = handler.type
    for n in (t.elts if isinstance(t, ast.Tuple) else [t]):
        if isinstance(n, ast.Name):
            names.append(n.id)
        elif isinstance(n, ast.Attribute):
            names.append(n.attr)
    return t is None or any(x in ("ImportError", "ModuleNotFoundError", "Exception") for x in names)


def _imports_of(stmt: ast.stmt, module_path: str, is_package: bool) -> List[Tuple[str, List[Tuple[str, str]]]]:
    """[(absolute module, [(imported name, bound name)])] for an import statement."""
    if isinstance(stmt, ast.Import):
        return [(a.name, [(a.name, a.asname or a.name.split(".")[0])]) for a in stmt.names]
    if isinstance(stmt, ast.ImportFrom):
        mod = stmt.module or ""
        if stmt.level:
            mod = _resolve_relative(module_path, is_package, stmt.level, stmt.module)
        return [(mod, [(a.name, a.asname or a.name) for a in stmt.names])]
    return []


_parsed: Dict[str, Optional[ast.Module]] = {}
_deps_cache: Dict[str, Optional[str]] = {}


def _parse(path: str) -> Optional[ast.Module]:
    if path not in _parsed:
        try:
            with open(path, encoding="utf-8-sig") as fh:
                _parsed[path] = ast.parse(fh.read(), path)
        except Exception:
            _parsed[path] = None
    return _parsed[path]


def _follow_internal(mod: str, module_path: str) -> bool:
    """Internal (agno) modules whose own imports decide whether this one loads:
    siblings in the same integration package (agno.vectordb.cassandra.index)
    and private compat shims (agno.utils.models._mistral_compat). Core modules
    (agno.utils.log, agno.tools.function, ...) are not followed."""
    a, b = mod.split("."), module_path.split(".")
    if len(a) >= 3 and len(b) >= 3 and a[:3] == b[:3]:
        return True
    return any(part.startswith("_") and not part.startswith("__") for part in a)


def _static_reason(module_path: str, class_name: Optional[str], depth: int = 0) -> Optional[str]:
    """Read the module's source (nothing is executed) and decide whether
    `from module_path import class_name` would succeed: the name must be
    defined or re-exported at module level, and every module-level import of a
    third-party package must be installed — unguarded, or guarded by a
    `try/except ImportError` whose handler re-raises (agno's pattern for
    optional dependencies; its message names the package to install).
    class_name=None checks only the module's imports."""
    if class_name is None and module_path in _deps_cache:
        return _deps_cache[module_path]
    if class_name is None:
        _deps_cache[module_path] = None  # break import cycles
    path = _module_file(module_path)
    if path is None:
        return f"module {module_path} not found"
    tree = _parse(path)
    if tree is None:
        return None  # cannot tell statically; the real import decides at run time
    is_package = path.endswith("__init__.py")

    defined = set()
    reexports: Dict[str, Tuple[str, str]] = {}

    def visit(stmts: List[ast.stmt], guarded: bool, raises: bool, msg: Optional[str]) -> Optional[str]:
        for st in stmts:
            if isinstance(st, ast.Try):
                handlers = [h for h in st.handlers if _catches_import_error(h)]
                h_raises = any(any(isinstance(n, ast.Raise) for n in ast.walk(h)) for h in handlers)
                h_msg = next((m for m in (_raise_message(h) for h in handlers) if m), None)
                if handlers:
                    r = visit(st.body, True, h_raises, h_msg)
                else:
                    r = visit(st.body, guarded, raises, msg)
                if r:
                    return r
                for h in st.handlers:
                    r = visit(h.body, False, True, None)
                    if r:
                        return r
                r = visit(st.orelse + st.finalbody, guarded, raises, msg)
                if r:
                    return r
                continue
            if isinstance(st, ast.If):
                test = ast.unparse(st.test) if hasattr(ast, "unparse") else ""
                if "TYPE_CHECKING" in test:
                    continue
                r = visit(st.body + st.orelse, guarded, raises, msg)
                if r:
                    return r
                continue
            if isinstance(st, (ast.Import, ast.ImportFrom)):
                for mod, names in _imports_of(st, module_path, is_package):
                    root = mod.split(".")[0]
                    if root not in _INTERNAL_ROOTS and root not in _STDLIB and mod:
                        fatal = (not guarded) or raises
                        if fatal and not _package_present(mod):
                            return msg or f"needs the Python package '{root}' (not installed)"
                        if fatal and isinstance(st, ast.ImportFrom) and _is_namespace(root) \
                                and _is_namespace(mod):
                            for orig, _bound in names:
                                if not _package_present(f"{mod}.{orig}"):
                                    return msg or f"needs the Python package '{mod}.{orig}' (not installed)"
                    if root in _INTERNAL_ROOTS and mod != module_path and depth < 3                             and _follow_internal(mod, module_path):
                        r = _static_reason(mod, None, depth + 1)
                        if r:
                            return r
                    for orig, bound in names:
                        defined.add(bound)
                        if root in _INTERNAL_ROOTS and isinstance(st, ast.ImportFrom):
                            reexports[bound] = (mod, orig)
                continue
            if isinstance(st, ast.ClassDef):
                defined.add(st.name)
            elif isinstance(st, ast.Assign):
                for t in st.targets:
                    if isinstance(t, ast.Name):
                        defined.add(t.id)
            elif isinstance(st, ast.AnnAssign) and isinstance(st.target, ast.Name):
                defined.add(st.target.id)
        return None

    reason = visit(tree.body, False, True, None)
    if class_name is None:
        _deps_cache[module_path] = reason
        return reason
    if reason:
        return reason
    if class_name in reexports and depth < 3:
        mod, orig = reexports[class_name]
        if mod != module_path:
            return _static_reason(mod, orig, depth + 1)
    if class_name not in defined:
        return (f"{class_name} does not exist in {module_path} "
                f"(the installed agno version renamed or removed it)")
    return None


def class_static_reason(class_path: str) -> Optional[str]:
    """Fast availability check for listings: like class_unavailable_reason()
    but reads source instead of importing (a full import of every optional
    integration took ~30 s). The run path still imports for real, so a case
    this misses is reported when the entry is actually used. Cached."""
    if class_path in _cache:
        return _cache[class_path]
    if class_path not in _static_cache:
        module_path, class_name = class_path.rsplit(".", 1)
        try:
            _static_cache[class_path] = _static_reason(module_path, class_name)
        except Exception:
            _static_cache[class_path] = None
    return _static_cache[class_path]


def load_class(class_path: str):
    """Import and return the class; raises ImportError with the probe's reason."""
    reason = class_unavailable_reason(class_path)
    if reason:
        raise ImportError(reason)
    module_path, class_name = class_path.rsplit(".", 1)
    return getattr(importlib.import_module(module_path), class_name)
