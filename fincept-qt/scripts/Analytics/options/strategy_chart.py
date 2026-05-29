"""
Strategy Chart (Multi-Leg Payoff + Aggregate Greeks)
===================================================

Computes the payoff curve of a multi-leg options strategy across a range of
underlying prices at expiry, plus the net (position-weighted) Greeks of the
combined position evaluated at the current spot.

Each leg: {strike, type (CE/PE/FUT), side (BUY/SELL), qty, premium, iv}.
Payoff at expiry for a spot S is the sum over legs of:

    option:  side_sign * qty * lot_size * (intrinsic(S) - premium)
    future:  side_sign * qty * lot_size * (S - entry_price)

where side_sign = +1 for BUY, -1 for SELL and intrinsic is max(S-K,0) for a
call, max(K-S,0) for a put. Net Greeks use Black-76 (options on the forward).

Aligned with the task spec (§12 strategy_chart) and OpenAlgo's
`services/strategy_chart_service.py` leg model (sign by side, OPTION legs drive
premium). Self-contained: numpy + scipy only (pure-python fallback). All
broker/Flask/DB fetching is stripped; legs + spot range are PASSED IN.

----------------------------------------------------------------------------
I/O CONVENTION (matches scripts/databento_fno_chain.py etc.)
----------------------------------------------------------------------------
    python strategy_chart.py compute '<json_args>'
    python strategy_chart.py compute @C:/path/to/spilled_args.json

argv[1] = command ("compute"); argv[2] = JSON args (or "@<path>" temp file).
Result JSON printed to stdout.

----------------------------------------------------------------------------
INPUT SCHEMA (argv[2] JSON object)
----------------------------------------------------------------------------
{
  "spot": 22500.0,                  # required current spot/forward, > 0
  "lot_size": 50,                   # default contract lot size. Default 1.
  "interest_rate": 0.0,             # optional decimal, for Greeks
  "time_to_expiry": 0.0192,         # optional years; else days_to_expiry; else 7/365
  "days_to_expiry": 7,              # optional
  "iv_is_decimal": false,           # treat leg "iv" as decimal (else percent)
  "spot_range": {                   # optional payoff X-axis. Defaults to +-15% of spot.
    "min": 20000, "max": 25000, "points": 101
  },
  "legs": [
    {
      "type": "CE",                 # CE | PE | FUT
      "side": "SELL",               # BUY | SELL
      "strike": 22500,              # required for CE/PE
      "qty": 1,                     # number of lots (default 1)
      "premium": 180.0,             # entry premium per share (option) / entry price (FUT)
      "iv": 12.5,                   # optional IV for Greeks (percent unless iv_is_decimal)
      "lot_size": 50                # optional per-leg lot size override
    }, ...
  ]
}

----------------------------------------------------------------------------
OUTPUT SCHEMA
----------------------------------------------------------------------------
{
  "error": false,
  "spot": 22500.0, "time_to_expiry": 0.0192, "interest_rate": 0.0,
  "net_premium": -345.0,            # net cashflow at entry (credit positive)
  "tag": "credit",                  # credit | debit | flat
  "payoff": [{"spot": 20000, "pnl": ...}, ...],   # payoff at expiry across spot range
  "breakevens": [22155.0, 22845.0], # spots where payoff crosses zero
  "max_profit": 17250.0,            # over the evaluated range (may be capped)
  "max_loss": -50000.0,
  "greeks": {                       # net position Greeks at current spot
    "delta": -0.02, "gamma": 0.0001, "theta": 35.2, "vega": -120.5, "rho": -4.3
  },
  "legs": [ {echoed normalized leg + per-leg greeks}, ... ],
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

    def _norm_cdf(x: float) -> float:
        return float(norm.cdf(x))

    def _norm_pdf(x: float) -> float:
        return float(norm.pdf(x))
except Exception:  # pragma: no cover
    def _norm_cdf(x: float) -> float:
        return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))

    def _norm_pdf(x: float) -> float:
        return math.exp(-0.5 * x * x) / math.sqrt(2.0 * math.pi)


# ── Black-76 Greeks (options on forward F) ───────────────────────────────────

def _black76_d1_d2(F, K, t, sigma):
    if F <= 0 or K <= 0 or t <= 0 or sigma <= 0:
        return None, None
    vst = sigma * math.sqrt(t)
    d1 = (math.log(F / K) + 0.5 * sigma * sigma * t) / vst
    return d1, d1 - vst


def black76_greeks(F, K, t, r, sigma, flag) -> Dict[str, float]:
    """Per-share Black-76 greeks. theta per-day, vega/rho per 1% move."""
    zero = {"delta": 0.0, "gamma": 0.0, "theta": 0.0, "vega": 0.0, "rho": 0.0}
    d1, d2 = _black76_d1_d2(F, K, t, sigma)
    if d1 is None:
        return zero
    disc = math.exp(-r * t)
    pdf = _norm_pdf(d1)
    sqrt_t = math.sqrt(t)
    gamma = disc * pdf / (F * sigma * sqrt_t)
    vega = disc * F * pdf * sqrt_t / 100.0  # per 1% vol change
    if flag == "c":
        delta = disc * _norm_cdf(d1)
        theta = (-F * disc * pdf * sigma / (2.0 * sqrt_t)
                 - r * K * disc * _norm_cdf(d2)
                 + r * F * disc * _norm_cdf(d1)) / 365.0
        rho = -t * disc * (F * _norm_cdf(d1) - K * _norm_cdf(d2)) / 100.0
    else:
        delta = -disc * _norm_cdf(-d1)
        theta = (-F * disc * pdf * sigma / (2.0 * sqrt_t)
                 + r * K * disc * _norm_cdf(-d2)
                 - r * F * disc * _norm_cdf(-d1)) / 365.0
        rho = -t * disc * (K * _norm_cdf(-d2) - F * _norm_cdf(-d1)) / 100.0
    return {"delta": delta, "gamma": gamma, "theta": theta, "vega": vega, "rho": rho}


def _to_float(v, default=0.0) -> float:
    try:
        return default if v is None else float(v)
    except (TypeError, ValueError):
        return default


def _resolve_tte(args: Dict[str, Any]) -> float:
    tte = _to_float(args.get("time_to_expiry"), 0.0)
    if tte > 0:
        return tte
    dte = _to_float(args.get("days_to_expiry"), 0.0)
    if dte > 0:
        return dte / 365.0
    return 7.0 / 365.0


def _normalize_leg(leg: Dict[str, Any], default_lot: int, iv_is_decimal: bool) -> Optional[Dict[str, Any]]:
    if not isinstance(leg, dict):
        return None
    typ = str(leg.get("type", "")).upper().strip()
    if typ in ("C", "CALL"):
        typ = "CE"
    elif typ in ("P", "PUT"):
        typ = "PE"
    elif typ in ("F", "FUTURE", "FUTURES"):
        typ = "FUT"
    if typ not in ("CE", "PE", "FUT"):
        return None
    side = str(leg.get("side", "")).upper().strip()
    if side not in ("BUY", "SELL"):
        return None
    qty = _to_float(leg.get("qty"), 1.0) or 1.0
    lot_size = int(_to_float(leg.get("lot_size"), 0.0)) or default_lot
    strike = _to_float(leg.get("strike"), 0.0)
    if typ in ("CE", "PE") and strike <= 0:
        return None
    iv_raw = _to_float(leg.get("iv"), 0.0)
    iv = (iv_raw if iv_is_decimal else iv_raw / 100.0) if iv_raw > 0 else 0.0
    return {
        "type": typ,
        "side": side,
        "sign": 1 if side == "BUY" else -1,
        "qty": qty,
        "lot_size": lot_size,
        "strike": strike,
        "premium": _to_float(leg.get("premium"), 0.0),
        "iv": iv,
    }


def _leg_payoff(leg: Dict[str, Any], S: float) -> float:
    """P&L of one leg at expiry for underlying price S (total, incl. lot * qty)."""
    contracts = leg["qty"] * leg["lot_size"]
    sign = leg["sign"]
    if leg["type"] == "CE":
        intrinsic = max(S - leg["strike"], 0.0)
        return sign * contracts * (intrinsic - leg["premium"])
    if leg["type"] == "PE":
        intrinsic = max(leg["strike"] - S, 0.0)
        return sign * contracts * (intrinsic - leg["premium"])
    # FUT: linear P&L from entry price (premium field holds entry price).
    return sign * contracts * (S - leg["premium"])


def compute(args: Dict[str, Any]) -> Dict[str, Any]:
    spot = _to_float(args.get("spot"), 0.0)
    if spot <= 0:
        return {"error": True, "message": "spot price is required and must be > 0",
                "timestamp": int(datetime.now().timestamp())}
    raw_legs = args.get("legs")
    if not isinstance(raw_legs, list) or not raw_legs:
        return {"error": True, "message": "legs must be a non-empty list",
                "timestamp": int(datetime.now().timestamp())}

    default_lot = int(_to_float(args.get("lot_size"), 1.0)) or 1
    iv_is_decimal = bool(args.get("iv_is_decimal", False))
    t = _resolve_tte(args)
    r = _to_float(args.get("interest_rate"), 0.0)

    legs = [nl for nl in (_normalize_leg(l, default_lot, iv_is_decimal) for l in raw_legs) if nl]
    if not legs:
        return {"error": True, "message": "No valid legs provided",
                "timestamp": int(datetime.now().timestamp())}

    # Spot range for the payoff X axis.
    rng = args.get("spot_range") or {}
    s_min = _to_float(rng.get("min"), 0.0)
    s_max = _to_float(rng.get("max"), 0.0)
    points = int(_to_float(rng.get("points"), 101.0)) or 101
    points = max(3, min(points, 2001))
    if s_min <= 0 or s_max <= 0 or s_max <= s_min:
        s_min = spot * 0.85
        s_max = spot * 1.15

    step = (s_max - s_min) / (points - 1)
    payoff: List[Dict[str, Any]] = []
    prev_s = None
    prev_pnl = None
    breakevens: List[float] = []
    for i in range(points):
        S = s_min + i * step
        pnl = sum(_leg_payoff(leg, S) for leg in legs)
        payoff.append({"spot": round(S, 2), "pnl": round(pnl, 2)})
        # Linear-interpolate zero crossings for breakeven points.
        if prev_pnl is not None and ((prev_pnl <= 0 <= pnl) or (prev_pnl >= 0 >= pnl)) and pnl != prev_pnl:
            be = prev_s + (0 - prev_pnl) * (S - prev_s) / (pnl - prev_pnl)
            breakevens.append(round(be, 2))
        prev_s, prev_pnl = S, pnl

    pnls = [p["pnl"] for p in payoff]
    max_profit = max(pnls)
    max_loss = min(pnls)

    # Net (position) Greeks at the current spot, position-weighted.
    net_greeks = {"delta": 0.0, "gamma": 0.0, "theta": 0.0, "vega": 0.0, "rho": 0.0}
    out_legs: List[Dict[str, Any]] = []
    for leg in legs:
        contracts = leg["qty"] * leg["lot_size"]
        leg_greeks = {"delta": 0.0, "gamma": 0.0, "theta": 0.0, "vega": 0.0, "rho": 0.0}
        if leg["type"] == "FUT":
            # Future: delta 1 per share, no convexity/decay.
            leg_greeks["delta"] = 1.0
        elif leg["iv"] > 0:
            flag = "c" if leg["type"] == "CE" else "p"
            leg_greeks = black76_greeks(spot, leg["strike"], t, r, leg["iv"], flag)
        for g in net_greeks:
            net_greeks[g] += leg["sign"] * contracts * leg_greeks[g]
        out_legs.append({
            "type": leg["type"], "side": leg["side"], "strike": leg["strike"],
            "qty": leg["qty"], "lot_size": leg["lot_size"], "premium": leg["premium"],
            "iv": round(leg["iv"], 6) if leg["iv"] else None,
            "greeks": {k: round(v, 8) for k, v in leg_greeks.items()},
        })

    # Net entry premium: credit (received) positive, debit (paid) negative.
    # For SELL we receive premium (+), for BUY we pay (-). FUT premia excluded.
    net_premium = 0.0
    for leg in legs:
        if leg["type"] == "FUT":
            continue
        contracts = leg["qty"] * leg["lot_size"]
        # SELL -> +premium received, BUY -> -premium paid.
        net_premium += (-leg["sign"]) * contracts * leg["premium"]
    tag = "credit" if net_premium > 0 else ("debit" if net_premium < 0 else "flat")

    return {
        "error": False,
        "spot": spot,
        "time_to_expiry": round(t, 6),
        "interest_rate": r,
        "net_premium": round(net_premium, 2),
        "tag": tag,
        "payoff": payoff,
        "breakevens": breakevens,
        "max_profit": round(max_profit, 2),
        "max_loss": round(max_loss, 2),
        "greeks": {k: round(v, 6) for k, v in net_greeks.items()},
        "legs": out_legs,
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
                          "message": "Usage: strategy_chart.py <command> <json_args>",
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
