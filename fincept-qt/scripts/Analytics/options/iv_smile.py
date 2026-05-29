"""
Implied Volatility Smile
========================

Computes the implied-volatility curve across strikes for a single expiry using
the Black-76 model, plus ATM IV and the put/call IV skew.

Ported from OpenAlgo `services/iv_smile_service.py` (Black-76 IV from
`services/option_greeks_service.py`). Broker/Flask/DB fetching is stripped; the
option chain is PASSED IN. Self-contained: numpy + scipy only (pure-python
fallback for the normal distribution and IV root-find).

----------------------------------------------------------------------------
I/O CONVENTION (matches scripts/databento_fno_chain.py etc.)
----------------------------------------------------------------------------
    python iv_smile.py compute '<json_args>'
    python iv_smile.py compute @C:/path/to/spilled_args.json

argv[1] = command ("compute"); argv[2] = JSON args (or "@<path>" temp file).
Result JSON printed to stdout.

----------------------------------------------------------------------------
INPUT SCHEMA (argv[2] JSON object)
----------------------------------------------------------------------------
{
  "spot": 22500.0,                  # required, > 0
  "expiry": "30JAN26",              # optional, echoed back
  "time_to_expiry": 0.0192,         # optional years; else days_to_expiry; else 7/365
  "days_to_expiry": 7,              # optional
  "interest_rate": 0.0,             # optional decimal
  "iv_is_decimal": false,           # treat quoted IV as decimal (else percent)
  "chain": [
    {"strike": 22500, "ce_iv": 12.5, "pe_iv": 13.1,   # quoted IV (percent) OR
     "ce_ltp": 180.0, "pe_ltp": 165.0}                # prices to back-solve IV
  ]
}

----------------------------------------------------------------------------
OUTPUT SCHEMA
----------------------------------------------------------------------------
{
  "error": false,
  "spot": 22500.0, "expiry": "30JAN26", "atm_strike": 22500,
  "atm_iv": 0.128,                 # decimal, avg of ATM CE/PE
  "skew": 0.012,                   # OTM put IV - OTM call IV (~5% OTM proxy, decimal)
  "chain": [
    {"strike": 22500, "ce_iv": 0.125, "pe_iv": 0.131, "smile_iv": 0.128}, ...
  ],
  "timestamp": 1730000000
}
"smile_iv" is the OTM-convention curve point: PE IV below ATM, CE IV at/above.
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


def _black76_d1_d2(F: float, K: float, t: float, sigma: float):
    if F <= 0 or K <= 0 or t <= 0 or sigma <= 0:
        return None, None
    vol_sqrt_t = sigma * math.sqrt(t)
    d1 = (math.log(F / K) + 0.5 * sigma * sigma * t) / vol_sqrt_t
    return d1, d1 - vol_sqrt_t


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


def _resolve_time_to_expiry(args: Dict[str, Any]) -> float:
    tte = _to_float(args.get("time_to_expiry"), 0.0)
    if tte > 0:
        return tte
    dte = _to_float(args.get("days_to_expiry"), 0.0)
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
    chain = args.get("chain")
    if not isinstance(chain, list) or not chain:
        return {"error": True, "message": "chain must be a non-empty list",
                "timestamp": int(datetime.now().timestamp())}

    t = _resolve_time_to_expiry(args)
    r = _to_float(args.get("interest_rate"), 0.0)
    iv_is_decimal = bool(args.get("iv_is_decimal", False))

    rows: List[Dict[str, Any]] = []
    atm_strike, atm_dist = None, None
    for item in chain:
        if not isinstance(item, dict):
            continue
        strike = _to_float(item.get("strike"), 0.0)
        if strike <= 0:
            continue
        dist = abs(strike - spot)
        if atm_dist is None or dist < atm_dist:
            atm_dist, atm_strike = dist, strike

        ce_iv = _resolve_leg_iv(item.get("ce_iv"), item.get("ce_ltp"), spot, strike, t, r, "c", iv_is_decimal)
        pe_iv = _resolve_leg_iv(item.get("pe_iv"), item.get("pe_ltp"), spot, strike, t, r, "p", iv_is_decimal)
        rows.append({"strike": strike, "ce_iv": ce_iv, "pe_iv": pe_iv})

    rows.sort(key=lambda x: x["strike"])

    # OTM-convention smile point + rounded output.
    out_rows = []
    atm_ce_iv = atm_pe_iv = None
    for rw in rows:
        strike = rw["strike"]
        ce_iv, pe_iv = rw["ce_iv"], rw["pe_iv"]
        if strike == atm_strike:
            atm_ce_iv, atm_pe_iv = ce_iv, pe_iv
        if strike >= (atm_strike or spot):
            smile_iv = ce_iv if ce_iv else pe_iv
        else:
            smile_iv = pe_iv if pe_iv else ce_iv
        out_rows.append({
            "strike": strike,
            "ce_iv": round(ce_iv, 6) if ce_iv else None,
            "pe_iv": round(pe_iv, 6) if pe_iv else None,
            "smile_iv": round(smile_iv, 6) if smile_iv else None,
        })

    # ATM IV: average of CE & PE at ATM (fall back to whichever exists).
    atm_iv = None
    if atm_ce_iv and atm_pe_iv:
        atm_iv = (atm_ce_iv + atm_pe_iv) / 2.0
    elif atm_ce_iv:
        atm_iv = atm_ce_iv
    elif atm_pe_iv:
        atm_iv = atm_pe_iv

    # Skew: nearest OTM put IV (below ATM) minus nearest OTM call IV (above ATM),
    # probing ~5% away from ATM (25-delta proxy, per OpenAlgo).
    skew = None
    if atm_strike:
        otm = atm_strike * 0.05
        put_iv = None
        for rw in sorted(out_rows, key=lambda x: abs(x["strike"] - (atm_strike - otm))):
            if rw["strike"] < atm_strike and rw["pe_iv"] is not None:
                put_iv = rw["pe_iv"]
                break
        call_iv = None
        for rw in sorted(out_rows, key=lambda x: abs(x["strike"] - (atm_strike + otm))):
            if rw["strike"] > atm_strike and rw["ce_iv"] is not None:
                call_iv = rw["ce_iv"]
                break
        if put_iv is not None and call_iv is not None:
            skew = round(put_iv - call_iv, 6)

    return {
        "error": False,
        "spot": spot,
        "expiry": args.get("expiry", ""),
        "atm_strike": atm_strike,
        "time_to_expiry": round(t, 6),
        "interest_rate": r,
        "atm_iv": round(atm_iv, 6) if atm_iv else None,
        "skew": skew,
        "chain": out_rows,
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
                          "message": "Usage: iv_smile.py <command> <json_args>",
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
