"""
Catalogue of the "fincept" engine: the strategy registry (scripts/strategies/_registry.py) as the
screen reads it, the parameters each strategy really reads, and get_capabilities.

ROWS (BT shape, data.strategies = {category: [row]}):
  id, name, category, description, params (specs), supported, runnable, reason, requires,
  missingPackages, optimizable, optimizableReason, constraints
  supported   the engine runs it (registry flag, generated from a full harness run of every entry)
  runnable    supported AND every third-party package it imports is installed in this Python
  reason      why not, whenever supported or runnable is false (null otherwise)
  optimizable the strategy reads parameters (QCAlgorithm.get_parameter / GetParameter with a literal
              default) - Optimize and Walk-Forward vary exactly those; a strategy that reads none would
              run the same code for every combination, so the screen hides Optimize for it
              (optimizableReason) and the provider refuses with an INPUT error
PARAMETER SPECS come from the strategy's own source (ast): id = the parameter name it reads,
  default = its literal default, type int / float / choice (bool and text defaults), min / max / step
  a range around the default the screen's spin boxes accept (the default always lies inside).
"""

from __future__ import annotations

import ast
import importlib.util
import math
import re
from typing import Any, Dict, List, Optional

_PARAM_CACHE: Dict[str, List[Dict[str, Any]]] = {}


def _label(name: str) -> str:
    words = re.sub(r'[-_]+', ' ', re.sub(r'(?<=[a-z0-9])(?=[A-Z])', ' ', str(name))).split()
    return ' '.join(w if w.isupper() else w.capitalize() for w in words) or str(name)


def _spec(name: str, default) -> Optional[Dict[str, Any]]:
    """A screen parameter spec for a get_parameter(name, default) call."""
    if isinstance(default, bool):
        return {'id': name, 'name': _label(name), 'label': _label(name), 'type': 'bool', 'default': default,
                'min': 0, 'max': 1, 'step': 1}
    if isinstance(default, int):
        d = int(default)
        if d > 0:
            lo, hi = max(1, d // 4), max(d * 3, d + 10)
            step = max(1, int(round(d / 20.0)))
        elif d == 0:
            lo, hi, step = 0, 100, 1
        else:
            lo, hi, step = d * 3, max(0, -d // 4), max(1, int(round(-d / 20.0)))
        return {'id': name, 'name': _label(name), 'label': _label(name), 'type': 'int', 'default': d,
                'min': lo, 'max': hi, 'step': step}
    if isinstance(default, float) and math.isfinite(default):
        d = float(default)
        if d > 0:
            lo, hi, step = d / 4.0, d * 3.0, d / 20.0
        elif d == 0:
            lo, hi, step = 0.0, 1.0, 0.05
        else:
            lo, hi, step = d * 3.0, d / 4.0, -d / 20.0
        r = lambda x: float(f'{x:.6g}')
        return {'id': name, 'name': _label(name), 'label': _label(name), 'type': 'float', 'default': d,
                'min': r(lo), 'max': r(hi), 'step': r(step)}
    if isinstance(default, str):
        return {'id': name, 'name': _label(name), 'label': _label(name), 'type': 'choice', 'default': default,
                'choices': [default]}
    return None


def scan_parameters(source: str) -> List[Dict[str, Any]]:
    """Parameters a strategy reads: get_parameter / GetParameter calls with a constant name, in source
    order (first default wins). A call without a literal default is listed with default null."""
    if 'get_parameter' not in source and 'GetParameter' not in source:
        return []
    try:
        tree = ast.parse(source)
    except SyntaxError:
        return []
    out: Dict[str, Dict[str, Any]] = {}
    for node in ast.walk(tree):
        if not isinstance(node, ast.Call):
            continue
        f = node.func
        name = f.attr if isinstance(f, ast.Attribute) else (f.id if isinstance(f, ast.Name) else None)
        if name not in ('get_parameter', 'GetParameter') or not node.args:
            continue
        a0 = node.args[0]
        if not (isinstance(a0, ast.Constant) and isinstance(a0.value, str)):
            continue
        pname = a0.value
        default = None
        dnode = node.args[1] if len(node.args) > 1 else next(
            (k.value for k in node.keywords if k.arg in ('default_value', 'default', 'defaultValue')), None)
        if dnode is not None:
            try:
                default = ast.literal_eval(dnode)
            except Exception:
                default = None
        if pname in out and out[pname].get('default') is not None:
            continue
        code_default = default
        cfg = _engine_defaults().get(pname)
        if cfg is not None and default is not None and not isinstance(default, bool):
            # the engine's default parameter set wins over the code default (what really runs)
            try:
                default = type(default)(float(cfg)) if isinstance(default, (int, float)) else type(default)(cfg)
            except (TypeError, ValueError):
                pass
        spec = _spec(pname, default) if default is not None else {
            'id': pname, 'name': _label(pname), 'label': _label(pname), 'type': 'choice', 'default': None,
            'choices': [], 'description': 'read without a default: the strategy needs this value'}
        if spec is not None:
            desc = f'get_parameter("{pname}") in the strategy code'
            if code_default is not None and default != code_default:
                desc += (f' (code default {code_default!r}; the engine\'s default parameter set - LEAN\'s config - '
                         f'supplies {default!r})')
            spec['description'] = spec.get('description') or desc
            out[pname] = spec
    return list(out.values())


def _engine_defaults() -> Dict[str, str]:
    """fincept_engine.algorithm.DEFAULT_PARAMETERS (LEAN's default config parameters)."""
    try:
        from fincept_engine.algorithm import DEFAULT_PARAMETERS
        return dict(DEFAULT_PARAMETERS)
    except Exception:
        return {}


def constraints_for(params: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """fast < slow / short < long pairs among numeric parameters (the screen enforces 'lt')."""
    ids = [p['id'] for p in params if p.get('type') in ('int', 'float')]
    out = []
    for a in ids:
        la = a.lower()
        for lo_w, hi_w in (('fast', 'slow'), ('short', 'long')):
            if lo_w in la:
                b = next((x for x in ids if x.lower() == la.replace(lo_w, hi_w)), None)
                if b:
                    out.append({'lt': [a, b], 'message': f'{a} must be smaller than {b}'})
    return out


def strategy_parameters(runner, strategy_id: str) -> List[Dict[str, Any]]:
    if strategy_id in _PARAM_CACHE:
        return _PARAM_CACHE[strategy_id]
    try:
        params = scan_parameters(runner.strategy_source(strategy_id))
    except Exception:
        params = []
    _PARAM_CACHE[strategy_id] = params
    return params


NOT_OPTIMIZABLE = ('the strategy reads no parameters (its code has no get_parameter call), so every combination '
                   'would run the same code; Optimize and Walk-Forward need a strategy that reads parameters')


def catalog(runner, include_all: bool = True) -> Dict[str, Any]:
    installed: Dict[str, bool] = {}

    def _have(pkg: str) -> bool:
        if pkg not in installed:
            try:
                installed[pkg] = importlib.util.find_spec(pkg) is not None
            except (ImportError, ValueError):
                installed[pkg] = False
        return installed[pkg]

    grouped: Dict[str, List[Dict[str, Any]]] = {}
    total = listed = engine_supported = runnable_n = optimizable_n = 0
    missing_packages: Dict[str, List[str]] = {}
    for s in runner.list_strategies():
        total += 1
        supported = bool(s.get('supported', True))
        reason = s.get('reason') or None
        if supported:
            engine_supported += 1
        missing = [p for p in (s.get('requires') or []) if not _have(p)]
        runnable = supported and not missing
        if supported and missing:
            for p in missing:
                missing_packages.setdefault(p, []).append(s['id'])
            reason = (f"needs the {missing[0]} package, which is not installed" if len(missing) == 1 else
                      f"needs the {', '.join(missing[:-1])} and {missing[-1]} packages, which are not installed")
        if runnable:
            runnable_n += 1
        if not runnable and not include_all:
            continue
        listed += 1
        params = strategy_parameters(runner, s['id'])
        optimizable = bool(params) and any(p.get('type') in ('int', 'float') for p in params)
        if optimizable and runnable:
            optimizable_n += 1
        row = {'id': s['id'], 'name': s['name'], 'category': s.get('category', 'General Strategy'),
               'description': f"{s['name']} ({s.get('path', '')})", 'params': params,
               'supported': supported, 'runnable': runnable, 'reason': None if runnable else (reason or 'not supported'),
               'requires': list(s.get('requires') or []), 'missingPackages': missing,
               'optimizable': optimizable,
               'optimizableReason': None if optimizable else NOT_OPTIMIZABLE}
        cons = constraints_for(params)
        if cons:
            row['constraints'] = cons
        grouped.setdefault(row['category'], []).append(row)
    return {'provider': 'fincept', 'strategies': grouped, 'total': total, 'listed': listed,
            'unsupported_hidden': 0 if include_all else total - listed, 'supported': engine_supported,
            'runnable_here': runnable_n, 'optimizable': optimizable_n, 'missing_packages': missing_packages}
