#!/usr/bin/env python3
"""
py_vollib Worker Handler
Covers all 3 models: Black (futures / forwards), Black-Scholes, Black-Scholes-Merton.
Each model supports: price, greeks, implied_volatility.

CLI: python vollib_service.py <operation> <json>     (prints exactly one JSON object)
  black_price | bs_price | bsm_price      {S, K, t (years), r, sigma, flag} (+ q for bsm_*)
  black_greeks | bs_greeks | bsm_greeks   same keys -> delta, gamma, vega, theta, rho
  black_iv | bs_iv | bsm_iv               {price, S, K, t, r, flag} (+ q) -> implied_volatility
  For black_*, S is the FORWARD / futures price and price is the discounted option price.
  flag: "c" / "p" (also accepted: "call" / "put").

Units are py_vollib's analytical Greeks (also returned in a "units" field):
  vega per 1 volatility point (0.01), theta per calendar day, rho per 1 % rate change,
  delta / gamma per 1.00 of the underlying.

Failures: {"error": "<plain message>"}; tracebacks go to stderr.
"""

import importlib.util
import json
import os
import sys
import traceback
from pathlib import Path

_HERE = Path(__file__).resolve().parent

try:
    import fincept_json
except ImportError:  # run by hand without PYTHONPATH=scripts
    sys.path.append(str(_HERE.parent.parent))
    import fincept_json

GREEK_UNITS = {
    "delta": "per 1.00 change in the underlying",
    "gamma": "change in delta per 1.00 change in the underlying",
    "vega": "per 1 volatility point (0.01)",
    "theta": "per calendar day",
    "rho": "per 1 % change in the rate",
}

_MODELS = None
_IMPORT_ERROR = None


def _load_local(name):
    """Load a sibling module by file path under a private name, so the local
    black.py can neither shadow nor be shadowed by the `black` code formatter."""
    spec = importlib.util.spec_from_file_location(f"_fincept_vollib_{name}", _HERE / f"{name}.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _models():
    global _MODELS, _IMPORT_ERROR
    if _MODELS is None and _IMPORT_ERROR is None:
        try:
            _MODELS = {
                "black": _load_local("black"),
                "bs": _load_local("black_scholes"),
                "bsm": _load_local("black_scholes_merton"),
            }
        except ImportError as e:
            _IMPORT_ERROR = (f"py_vollib is not installed in this Python environment ({e}); "
                             f"it belongs to venv-numpy2 (resources/requirements-numpy2.txt)")
    if _MODELS is None:
        raise _InputError(_IMPORT_ERROR)
    return _MODELS


class _InputError(ValueError):
    """A plain, user-facing failure (bad input or a missing library): no traceback."""
    pass


def _num(data, key):
    if key not in data or data[key] is None:
        raise _InputError(f"missing required parameter '{key}'")
    try:
        return float(data[key])
    except (TypeError, ValueError):
        raise _InputError(f"parameter '{key}' must be a number, got {data[key]!r}")


def _flag(data):
    raw = str(data.get("flag", "")).strip().lower()
    flag = {"c": "c", "call": "c", "p": "p", "put": "p"}.get(raw)
    if flag is None:
        raise _InputError(f"flag must be 'c' or 'p' (or 'call' / 'put'), got {data.get('flag')!r}")
    return flag


def _common(data, with_sigma=True, with_q=False, with_price=False):
    kw = {"S": _num(data, "S"), "K": _num(data, "K"), "t": _num(data, "t"), "r": _num(data, "r"),
          "flag": _flag(data)}
    if with_sigma:
        kw["sigma"] = _num(data, "sigma")
        if kw["sigma"] <= 0:
            raise _InputError("sigma must be > 0")
    if with_q:
        kw["q"] = _num(data, "q")
    if with_price:
        kw["price"] = _num(data, "price")
        if kw["price"] <= 0:
            raise _InputError("price must be > 0")
    if kw["S"] <= 0 or kw["K"] <= 0:
        raise _InputError("S and K must be > 0")
    if kw["t"] <= 0:
        raise _InputError("t (years to expiry) must be > 0")
    return kw


def _op(model, kind):
    def run(data):
        m = _models()[model]
        fn = getattr(m, f"calculate_{model}_{kind}")
        if kind == "iv":
            kw = _common(data, with_sigma=False, with_q=(model == "bsm"), with_price=True)
        else:
            kw = _common(data, with_sigma=True, with_q=(model == "bsm"))
        result = fn(**kw)
        if kind == "greeks":
            result = dict(result)
            result["units"] = GREEK_UNITS
        return result
    return run


DISPATCH = {
    f"{model}_{kind}": _op(model, kind)
    for model in ("black", "bs", "bsm")
    for kind in ("price", "greeks", "iv")
}


def main(args):
    """Args: [operation, json_data]. Returns the result dict (never raises)."""
    if len(args) < 2:
        return {"error": "Usage: vollib_service.py <operation> <json_data>; operations: "
                         + ", ".join(sorted(DISPATCH))}
    operation = args[0]
    handler = DISPATCH.get(operation)
    if handler is None:
        return {"error": f"Unknown operation: {operation}"}
    try:
        data = json.loads(args[1]) if args[1].strip() else {}
    except json.JSONDecodeError as e:
        return {"error": f"Invalid JSON parameters: {e}"}
    if not isinstance(data, dict):
        return {"error": "parameters must be a JSON object"}
    try:
        return handler(data)
    except _InputError as e:
        return {"error": str(e)}
    except Exception as e:
        traceback.print_exc(file=sys.stderr)
        name = type(e).__name__
        text = str(e).strip()
        if "Intrinsic" in name:
            return {"error": f"option price is below intrinsic value; no implied volatility exists ({name})"}
        if "AboveMaximum" in name or "Above" in name:
            return {"error": f"option price is above the model's maximum; no implied volatility exists ({name})"}
        return {"error": f"py_vollib error: {name}: {text}" if text else f"py_vollib error: {name}"}


if __name__ == "__main__":
    fincept_json.emit(main(sys.argv[1:]))
