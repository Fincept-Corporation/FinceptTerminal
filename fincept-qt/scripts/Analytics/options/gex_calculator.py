"""
Gamma Exposure (GEX) Calculator
===============================

Computes per-strike Gamma Exposure for an option chain using the Black-76
model (options on futures/forwards — the correct model for Indian F&O and
also used here for equities/indices where the spot is treated as the forward).

    GEX(leg)  = gamma * open_interest * lot_size
    Net GEX   = Call GEX - Put GEX     (per strike)

Ported from OpenAlgo `services/gex_service.py` + the Black-76 greeks math in
`services/option_greeks_service.py`. All broker/Flask/DB fetching is stripped;
the option chain is PASSED IN. Self-contained: numpy + scipy only (with a
pure-python fallback for the normal distribution and IV root-find).

----------------------------------------------------------------------------
I/O CONVENTION (matches scripts/databento_fno_chain.py etc.)
----------------------------------------------------------------------------
Invoked by PythonRunner as:

    python gex_calculator.py compute '<json_args>'
    python gex_calculator.py compute @C:/path/to/spilled_args.json   (large args)

argv[1] = command  ("compute")
argv[2] = JSON args object (or "@<path>" pointing at a temp file to read+delete)

Result JSON is printed to stdout (single line).

----------------------------------------------------------------------------
INPUT SCHEMA  (argv[2] JSON object)
----------------------------------------------------------------------------
{
  "spot": 22500.0,                  # underlying spot / forward price (required, > 0)
  "expiry": "30JAN26",              # optional, echoed back; used for DTE if time_to_expiry absent
  "time_to_expiry": 0.0192,         # optional, years to expiry. If absent, derived from
                                    #   "days_to_expiry" or defaults to 7/365.
  "days_to_expiry": 7,              # optional alternative to time_to_expiry
  "interest_rate": 0.0,             # optional, annualized decimal (e.g. 0.065). Default 0.
  "lot_size": 50,                   # optional default lot size if a row omits it. Default 1.
  "chain": [                        # required: list of per-strike rows
    {
      "strike": 22500,
      "lot_size": 50,               # optional per-row override
      "ce_oi": 123400, "ce_iv": 12.5, "ce_ltp": 180.0,   # CE open interest / IV(%) / price
      "pe_oi": 98700,  "pe_iv": 13.1, "pe_ltp": 165.0    # PE open interest / IV(%) / price
    },
    ...
  ]
}

IV handling per leg, in priority order:
  1. If "ce_iv"/"pe_iv" given (> 0) it is used directly (interpreted as percent,
     e.g. 12.5 -> 0.125). Pass "iv_is_decimal": true to treat IV as a decimal.
  2. Else if "ce_ltp"/"pe_ltp" given (> 0) IV is back-solved from price via Black-76.
  3. Else that leg contributes zero gamma/GEX.

----------------------------------------------------------------------------
OUTPUT SCHEMA
----------------------------------------------------------------------------
{
  "error": false,
  "spot": 22500.0,
  "expiry": "30JAN26",
  "atm_strike": 22500,
  "time_to_expiry": 0.0192,
  "interest_rate": 0.0,
  "chain": [
    {"strike": 22500, "ce_oi": ..., "pe_oi": ...,
     "ce_gamma": 0.000123, "pe_gamma": 0.000119,
     "ce_iv": 0.125, "pe_iv": 0.131,
     "ce_gex": 760.5, "pe_gex": 700.2, "net_gex": 60.3}, ...
  ],
  "totals": {
     "total_ce_oi": ..., "total_pe_oi": ...,
     "total_ce_gex": ..., "total_pe_gex": ..., "total_net_gex": ...,
     "pcr_oi": 0.84
  },
  "oi_walls": {
     "call_wall": 23000,   # strike with the largest CE OI (gamma resistance)
     "put_wall": 22000     # strike with the largest PE OI (gamma support)
  },
  "timestamp": 1730000000
}
"""

import json
import math
import os
import sys
from datetime import datetime
from typing import Any, Dict, List, Optional

# numpy/scipy are available in the bundled venv-numpy2. Fall back gracefully so
# the script stays importable/runnable even without scipy.
try:
    from scipy.stats import norm
    from scipy.optimize import brentq

    def _norm_cdf(x: float) -> float:
        return float(norm.cdf(x))

    def _norm_pdf(x: float) -> float:
        return float(norm.pdf(x))

    _HAVE_SCIPY = True
except Exception:  # pragma: no cover - exercised only without scipy
    _HAVE_SCIPY = False

    def _norm_cdf(x: float) -> float:
        return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))

    def _norm_pdf(x: float) -> float:
        return math.exp(-0.5 * x * x) / math.sqrt(2.0 * math.pi)


# ── Black-76 core ────────────────────────────────────────────────────────────
# Black-76: options on a forward/future F. d1/d2 omit the (r - q) drift since F
# already embeds carry; the whole price is discounted by exp(-r * T).

def _black76_d1_d2(F: float, K: float, t: float, sigma: float):
    if F <= 0 or K <= 0 or t <= 0 or sigma <= 0:
        return None, None
    vol_sqrt_t = sigma * math.sqrt(t)
    d1 = (math.log(F / K) + 0.5 * sigma * sigma * t) / vol_sqrt_t
    d2 = d1 - vol_sqrt_t
    return d1, d2


def black76_price(F: float, K: float, t: float, r: float, sigma: float, flag: str) -> float:
    """Black-76 option price. flag 'c' for call, 'p' for put. r is decimal."""
    d1, d2 = _black76_d1_d2(F, K, t, sigma)
    if d1 is None:
        # Degenerate -> intrinsic (discounted)
        disc = math.exp(-r * max(t, 0.0))
        if flag == "c":
            return disc * max(F - K, 0.0)
        return disc * max(K - F, 0.0)
    disc = math.exp(-r * t)
    if flag == "c":
        return disc * (F * _norm_cdf(d1) - K * _norm_cdf(d2))
    return disc * (K * _norm_cdf(-d2) - F * _norm_cdf(-d1))


def black76_gamma(F: float, K: float, t: float, r: float, sigma: float) -> float:
    """Black-76 gamma (same for calls and puts). dGamma/dF^2 sensitivity."""
    d1, _ = _black76_d1_d2(F, K, t, sigma)
    if d1 is None:
        return 0.0
    disc = math.exp(-r * t)
    return disc * _norm_pdf(d1) / (F * sigma * math.sqrt(t))


def black76_implied_vol(price: float, F: float, K: float, t: float, r: float,
                        flag: str) -> Optional[float]:
    """
    Back-solve Black-76 implied volatility from an option price.

    Uses scipy.brentq when available, else a bounded Newton-Raphson with a
    bisection fallback. Returns None if the price is below intrinsic / no root.
    """
    if price is None or price <= 0 or F <= 0 or K <= 0 or t <= 0:
        return None

    disc = math.exp(-r * t)
    intrinsic = disc * (max(F - K, 0.0) if flag == "c" else max(K - F, 0.0))
    # Price at or below discounted intrinsic -> no positive-vol solution.
    if price <= intrinsic + 1e-9:
        return None

    def objective(sigma: float) -> float:
        return black76_price(F, K, t, r, sigma, flag) - price

    lo, hi = 1e-4, 5.0
    f_lo, f_hi = objective(lo), objective(hi)
    if f_lo * f_hi > 0:
        # Expand the upper bound once for very high-vol quotes.
        hi = 10.0
        f_hi = objective(hi)
        if f_lo * f_hi > 0:
            return None

    if _HAVE_SCIPY:
        try:
            return float(brentq(objective, lo, hi, xtol=1e-6, maxiter=100))
        except Exception:
            pass

    # Newton-Raphson seeded at a Brenner-Subrahmanyam-style guess, vega-driven.
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
    # Bisection fallback.
    for _ in range(100):
        mid = 0.5 * (lo + hi)
        f_mid = objective(mid)
        if abs(f_mid) < 1e-7:
            return mid
        if f_lo * f_mid < 0:
            hi = mid
        else:
            lo = mid
            f_lo = f_mid
    return 0.5 * (lo + hi)


# ── Helpers ──────────────────────────────────────────────────────────────────

def _to_float(v, default=0.0) -> float:
    try:
        if v is None:
            return default
        return float(v)
    except (TypeError, ValueError):
        return default


def _resolve_time_to_expiry(args: Dict[str, Any]) -> float:
    tte = args.get("time_to_expiry")
    if tte is not None:
        tte = _to_float(tte, 0.0)
        if tte > 0:
            return tte
    dte = args.get("days_to_expiry")
    if dte is not None:
        dte = _to_float(dte, 0.0)
        if dte > 0:
            return dte / 365.0
    # Default: ~1 trading week, avoids div-by-zero in the model.
    return 7.0 / 365.0


def _resolve_leg_iv(iv_raw, ltp, F, K, t, r, flag, iv_is_decimal) -> Optional[float]:
    """Return decimal IV for a leg: from the quoted IV if present, else solved."""
    iv = _to_float(iv_raw, 0.0)
    if iv > 0:
        return iv if iv_is_decimal else iv / 100.0
    ltp = _to_float(ltp, 0.0)
    if ltp > 0:
        return black76_implied_vol(ltp, F, K, t, r, flag)
    return None


# ── Main computation ─────────────────────────────────────────────────────────

def compute(args: Dict[str, Any]) -> Dict[str, Any]:
    spot = _to_float(args.get("spot"), 0.0)
    if spot <= 0:
        return {"error": True, "message": "spot price is required and must be > 0",
                "timestamp": int(datetime.now().timestamp())}

    chain = args.get("chain")
    if not isinstance(chain, list) or not chain:
        return {"error": True, "message": "chain must be a non-empty list",
                "timestamp": int(datetime.now().timestamp())}

    t = _resolve_time_to_expiry(args)
    r = _to_float(args.get("interest_rate"), 0.0)
    default_lot = int(_to_float(args.get("lot_size"), 1.0)) or 1
    iv_is_decimal = bool(args.get("iv_is_decimal", False))

    out_rows: List[Dict[str, Any]] = []
    atm_strike = None
    atm_dist = None

    for item in chain:
        if not isinstance(item, dict):
            continue
        strike = _to_float(item.get("strike"), 0.0)
        if strike <= 0:
            continue
        lot_size = int(_to_float(item.get("lot_size"), 0.0)) or default_lot

        # Track ATM as the strike nearest spot.
        dist = abs(strike - spot)
        if atm_dist is None or dist < atm_dist:
            atm_dist = dist
            atm_strike = strike

        ce_oi = _to_float(item.get("ce_oi"), 0.0)
        pe_oi = _to_float(item.get("pe_oi"), 0.0)

        ce_iv = _resolve_leg_iv(item.get("ce_iv"), item.get("ce_ltp"), spot, strike, t, r, "c", iv_is_decimal)
        pe_iv = _resolve_leg_iv(item.get("pe_iv"), item.get("pe_ltp"), spot, strike, t, r, "p", iv_is_decimal)

        ce_gamma = black76_gamma(spot, strike, t, r, ce_iv) if ce_iv and ce_iv > 0 else 0.0
        pe_gamma = black76_gamma(spot, strike, t, r, pe_iv) if pe_iv and pe_iv > 0 else 0.0

        ce_gex = ce_gamma * ce_oi * lot_size
        pe_gex = pe_gamma * pe_oi * lot_size
        net_gex = ce_gex - pe_gex

        out_rows.append({
            "strike": strike,
            "lot_size": lot_size,
            "ce_oi": ce_oi,
            "pe_oi": pe_oi,
            "ce_iv": round(ce_iv, 6) if ce_iv else None,
            "pe_iv": round(pe_iv, 6) if pe_iv else None,
            "ce_gamma": round(ce_gamma, 8),
            "pe_gamma": round(pe_gamma, 8),
            "ce_gex": round(ce_gex, 2),
            "pe_gex": round(pe_gex, 2),
            "net_gex": round(net_gex, 2),
        })

    out_rows.sort(key=lambda x: x["strike"])

    total_ce_oi = sum(rw["ce_oi"] for rw in out_rows)
    total_pe_oi = sum(rw["pe_oi"] for rw in out_rows)
    total_ce_gex = sum(rw["ce_gex"] for rw in out_rows)
    total_pe_gex = sum(rw["pe_gex"] for rw in out_rows)
    total_net_gex = sum(rw["net_gex"] for rw in out_rows)
    pcr_oi = round(total_pe_oi / total_ce_oi, 4) if total_ce_oi > 0 else 0.0

    # OI walls: the largest OI strikes act as gamma "magnets"/barriers.
    call_wall = max(out_rows, key=lambda x: x["ce_oi"])["strike"] if out_rows else None
    put_wall = max(out_rows, key=lambda x: x["pe_oi"])["strike"] if out_rows else None

    return {
        "error": False,
        "spot": spot,
        "expiry": args.get("expiry", ""),
        "atm_strike": atm_strike,
        "time_to_expiry": round(t, 6),
        "interest_rate": r,
        "chain": out_rows,
        "totals": {
            "total_ce_oi": total_ce_oi,
            "total_pe_oi": total_pe_oi,
            "total_ce_gex": round(total_ce_gex, 2),
            "total_pe_gex": round(total_pe_gex, 2),
            "total_net_gex": round(total_net_gex, 2),
            "pcr_oi": pcr_oi,
        },
        "oi_walls": {"call_wall": call_wall, "put_wall": put_wall},
        "timestamp": int(datetime.now().timestamp()),
    }


# ── CLI plumbing (matches Fincept PythonRunner convention) ───────────────────

def resolve_arg(arg: str) -> str:
    """If arg starts with '@', read content from that file path and delete it."""
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
                          "message": "Usage: gex_calculator.py <command> <json_args>",
                          "commands": ["compute"]}), flush=True)
        sys.exit(1)

    command = sys.argv[1]
    raw = resolve_arg(sys.argv[2]) if len(sys.argv) > 2 else "{}"

    try:
        args = json.loads(raw) if raw else {}
    except json.JSONDecodeError as e:
        print(json.dumps({"error": True, "message": f"Invalid JSON args: {e}"}), flush=True)
        sys.exit(1)

    if command == "compute":
        result = compute(args)
    else:
        result = {"error": True, "message": f"Unknown command: {command}"}

    print(json.dumps(result, default=str), flush=True)


if __name__ == "__main__":
    main()
