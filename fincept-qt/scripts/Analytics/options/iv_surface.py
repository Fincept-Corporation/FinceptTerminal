"""
Implied Volatility Surface
=========================

Builds a 3D implied-volatility surface across strikes and expiries from a set
of per-expiry option chains, using the Black-76 model. Uses the OTM convention
(CE IV for strikes >= ATM, PE IV below ATM) like OpenAlgo's vol surface.

Ported from OpenAlgo `services/vol_surface_service.py`. Broker/Flask/DB fetching
is stripped; the multi-expiry chains are PASSED IN. Self-contained: numpy +
scipy only (pure-python fallback for normal dist and IV root-find).

----------------------------------------------------------------------------
I/O CONVENTION (matches scripts/databento_fno_chain.py etc.)
----------------------------------------------------------------------------
    python iv_surface.py compute '<json_args>'
    python iv_surface.py compute @C:/path/to/spilled_args.json

argv[1] = command ("compute"); argv[2] = JSON args (or "@<path>" temp file).
Result JSON printed to stdout.

----------------------------------------------------------------------------
INPUT SCHEMA (argv[2] JSON object)
----------------------------------------------------------------------------
{
  "spot": 22500.0,                  # required underlying spot/forward, > 0
  "interest_rate": 0.0,             # optional decimal
  "iv_is_decimal": false,           # treat quoted IV as decimal (else percent)
  "expiries": [                     # required: one entry per expiry
    {
      "expiry": "30JAN26",
      "time_to_expiry": 0.0192,     # optional years; else days_to_expiry; else 7/365
      "days_to_expiry": 7,          # optional
      "chain": [
        {"strike": 22500, "ce_iv": 12.5, "pe_iv": 13.1,
         "ce_ltp": 180.0, "pe_ltp": 165.0}, ...
      ]
    }, ...
  ]
}

Surface grid uses the intersection of strikes common to all expiries when there
are >= 3 common strikes; otherwise falls back to the union (gaps = null).

----------------------------------------------------------------------------
OUTPUT SCHEMA
----------------------------------------------------------------------------
{
  "error": false,
  "spot": 22500.0,
  "atm_strike": 22500,
  "strikes": [22000, 22500, 23000, ...],          # grid X axis
  "expiries": [{"expiry": "30JAN26", "dte": 7.0, "time_to_expiry": 0.0192}, ...],  # Y axis
  "surface": [[0.131, 0.128, 0.127, ...], ...],   # surface[expiry_idx][strike_idx] = decimal IV or null
  "points": [{"strike": 22500, "expiry": "30JAN26", "dte": 7.0, "iv": 0.128}, ...],
  "timestamp": 1730000000
}
"""

import json
import math
import os
import sys
from datetime import datetime
from typing import Any, Dict, List, Optional

try:
    from scipy.stats import norm
    from scipy.optimize import brentq

    def _norm_cdf(x: float) -> float:
        return float(norm.cdf(x))

    def _norm_pdf(x: float) -> float:
        return float(norm.pdf(x))

    _HAVE_SCIPY = True
except Exception:  # pragma: no cover
    _HAVE_SCIPY = False

    def _norm_cdf(x: float) -> float:
        return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))

    def _norm_pdf(x: float) -> float:
        return math.exp(-0.5 * x * x) / math.sqrt(2.0 * math.pi)


def _black76_d1_d2(F, K, t, sigma):
    if F <= 0 or K <= 0 or t <= 0 or sigma <= 0:
        return None, None
    vst = sigma * math.sqrt(t)
    d1 = (math.log(F / K) + 0.5 * sigma * sigma * t) / vst
    return d1, d1 - vst


def black76_price(F, K, t, r, sigma, flag) -> float:
    d1, d2 = _black76_d1_d2(F, K, t, sigma)
    if d1 is None:
        disc = math.exp(-r * max(t, 0.0))
        return disc * (max(F - K, 0.0) if flag == "c" else max(K - F, 0.0))
    disc = math.exp(-r * t)
    if flag == "c":
        return disc * (F * _norm_cdf(d1) - K * _norm_cdf(d2))
    return disc * (K * _norm_cdf(-d2) - F * _norm_cdf(-d1))


def black76_implied_vol(price, F, K, t, r, flag) -> Optional[float]:
    if price is None or price <= 0 or F <= 0 or K <= 0 or t <= 0:
        return None
    disc = math.exp(-r * t)
    intrinsic = disc * (max(F - K, 0.0) if flag == "c" else max(K - F, 0.0))
    if price <= intrinsic + 1e-9:
        return None

    def objective(sigma):
        return black76_price(F, K, t, r, sigma, flag) - price

    lo, hi = 1e-4, 5.0
    f_lo, f_hi = objective(lo), objective(hi)
    if f_lo * f_hi > 0:
        hi = 10.0
        f_hi = objective(hi)
        if f_lo * f_hi > 0:
            return None
    if _HAVE_SCIPY:
        try:
            return float(brentq(objective, lo, hi, xtol=1e-6, maxiter=100))
        except Exception:
            pass
    sigma = max(min(math.sqrt(2.0 * math.pi / t) * price / F, hi), lo)
    for _ in range(60):
        d1, _ = _black76_d1_d2(F, K, t, sigma)
        if d1 is None:
            break
        vega = disc * F * _norm_pdf(d1) * math.sqrt(t)
        diff = black76_price(F, K, t, r, sigma, flag) - price
        if abs(diff) < 1e-7:
            return sigma
        if vega < 1e-12:
            break
        sigma -= diff / vega
        if sigma <= lo or sigma >= hi:
            break
    for _ in range(100):
        mid = 0.5 * (lo + hi)
        f_mid = objective(mid)
        if abs(f_mid) < 1e-7:
            return mid
        if f_lo * f_mid < 0:
            hi = mid
        else:
            lo, f_lo = mid, f_mid
    return 0.5 * (lo + hi)


def _to_float(v, default=0.0) -> float:
    try:
        return default if v is None else float(v)
    except (TypeError, ValueError):
        return default


def _resolve_tte(entry: Dict[str, Any]) -> float:
    tte = _to_float(entry.get("time_to_expiry"), 0.0)
    if tte > 0:
        return tte
    dte = _to_float(entry.get("days_to_expiry"), 0.0)
    if dte > 0:
        return dte / 365.0
    return 7.0 / 365.0


def _resolve_leg_iv(iv_raw, ltp, F, K, t, r, flag, iv_is_decimal) -> Optional[float]:
    iv = _to_float(iv_raw, 0.0)
    if iv > 0:
        return iv if iv_is_decimal else iv / 100.0
    ltp = _to_float(ltp, 0.0)
    if ltp > 0:
        return black76_implied_vol(ltp, F, K, t, r, flag)
    return None


def compute(args: Dict[str, Any]) -> Dict[str, Any]:
    spot = _to_float(args.get("spot"), 0.0)
    if spot <= 0:
        return {"error": True, "message": "spot price is required and must be > 0",
                "timestamp": int(datetime.now().timestamp())}
    expiries = args.get("expiries")
    if not isinstance(expiries, list) or not expiries:
        return {"error": True, "message": "expiries must be a non-empty list",
                "timestamp": int(datetime.now().timestamp())}

    r = _to_float(args.get("interest_rate"), 0.0)
    iv_is_decimal = bool(args.get("iv_is_decimal", False))

    # Per-expiry: parse rows into {strike: (ce_iv, pe_iv)} maps.
    parsed_expiries = []
    for entry in expiries:
        if not isinstance(entry, dict):
            continue
        chain = entry.get("chain")
        if not isinstance(chain, list) or not chain:
            continue
        t = _resolve_tte(entry)
        iv_by_strike: Dict[float, Dict[str, Optional[float]]] = {}
        for item in chain:
            if not isinstance(item, dict):
                continue
            strike = _to_float(item.get("strike"), 0.0)
            if strike <= 0:
                continue
            ce_iv = _resolve_leg_iv(item.get("ce_iv"), item.get("ce_ltp"), spot, strike, t, r, "c", iv_is_decimal)
            pe_iv = _resolve_leg_iv(item.get("pe_iv"), item.get("pe_ltp"), spot, strike, t, r, "p", iv_is_decimal)
            iv_by_strike[strike] = {"ce": ce_iv, "pe": pe_iv}
        if iv_by_strike:
            parsed_expiries.append({
                "expiry": entry.get("expiry", ""),
                "time_to_expiry": t,
                "iv_by_strike": iv_by_strike,
            })

    if not parsed_expiries:
        return {"error": True, "message": "No valid expiry data found",
                "timestamp": int(datetime.now().timestamp())}

    # ATM strike: nearest-to-spot among the first expiry's strikes.
    first_strikes = sorted(parsed_expiries[0]["iv_by_strike"].keys())
    atm_strike = min(first_strikes, key=lambda k: abs(k - spot))

    # Grid X axis: intersection of all expiries' strikes if >= 3, else union.
    strike_sets = [set(e["iv_by_strike"].keys()) for e in parsed_expiries]
    common = set.intersection(*strike_sets)
    if len(common) >= 3:
        grid_strikes = sorted(common)
    else:
        grid_strikes = sorted(set().union(*strike_sets))

    surface: List[List[Optional[float]]] = []
    expiry_info: List[Dict[str, Any]] = []
    points: List[Dict[str, Any]] = []

    for e in parsed_expiries:
        t = e["time_to_expiry"]
        ivmap = e["iv_by_strike"]
        row: List[Optional[float]] = []
        for strike in grid_strikes:
            cell = ivmap.get(strike)
            iv_val = None
            if cell:
                # OTM convention: CE at/above ATM, PE below ATM, with fallback.
                if strike >= atm_strike:
                    iv_val = cell["ce"] if cell["ce"] else cell["pe"]
                else:
                    iv_val = cell["pe"] if cell["pe"] else cell["ce"]
            iv_round = round(iv_val, 6) if iv_val and iv_val > 0 else None
            row.append(iv_round)
            if iv_round is not None:
                points.append({
                    "strike": strike,
                    "expiry": e["expiry"],
                    "dte": round(t * 365.0, 2),
                    "iv": iv_round,
                })
        surface.append(row)
        expiry_info.append({
            "expiry": e["expiry"],
            "dte": round(t * 365.0, 2),
            "time_to_expiry": round(t, 6),
        })

    return {
        "error": False,
        "spot": spot,
        "interest_rate": r,
        "atm_strike": atm_strike,
        "strikes": grid_strikes,
        "expiries": expiry_info,
        "surface": surface,
        "points": points,
        "timestamp": int(datetime.now().timestamp()),
    }


def resolve_arg(arg: str) -> str:
    if arg and arg.startswith("@"):
        path = arg[1:]
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = f.read()
            try:
                os.remove(path)
            except OSError:
                pass
            return data
        except OSError:
            return arg
    return arg


def main():
    if len(sys.argv) < 2:
        print(json.dumps({"error": True,
                          "message": "Usage: iv_surface.py <command> <json_args>",
                          "commands": ["compute"]}), flush=True)
        sys.exit(1)
    command = sys.argv[1]
    raw = resolve_arg(sys.argv[2]) if len(sys.argv) > 2 else "{}"
    try:
        args = json.loads(raw) if raw else {}
    except json.JSONDecodeError as e:
        print(json.dumps({"error": True, "message": f"Invalid JSON args: {e}"}), flush=True)
        sys.exit(1)
    result = compute(args) if command == "compute" else {"error": True, "message": f"Unknown command: {command}"}
    print(json.dumps(result, default=str), flush=True)


if __name__ == "__main__":
    main()
