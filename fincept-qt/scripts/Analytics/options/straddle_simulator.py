"""
Straddle Simulator (Intraday Short ATM Straddle)
===============================================

Simulates an intraday short ATM straddle with automatic strike adjustments:

    1. ENTRY  at the first candle      -> sell ATM CE + PE
    2. ADJUST when spot moves >= N pts -> exit old legs, re-enter at the new ATM
    3. EXIT   at the last candle       -> close the position

Produces a cumulative P&L time-series, a trade log, and a summary. Short
straddle P&L per leg = (entry_price - current_price) * quantity (premium decays
in the seller's favour).

Ported from OpenAlgo `services/custom_straddle_service.py`. All broker/Flask/DB
history fetching is stripped; the intraday option prices are PASSED IN. Pure
math — standard library only.

----------------------------------------------------------------------------
I/O CONVENTION (matches scripts/databento_fno_chain.py etc.)
----------------------------------------------------------------------------
    python straddle_simulator.py simulate '<json_args>'
    python straddle_simulator.py simulate @C:/path/to/spilled_args.json

argv[1] = command ("simulate"); argv[2] = JSON args (or "@<path>" temp file).
Result JSON printed to stdout.

----------------------------------------------------------------------------
INPUT SCHEMA (argv[2] JSON object)
----------------------------------------------------------------------------
{
  "underlying": "NIFTY",            # optional, echoed back
  "expiry": "30JAN26",              # optional, echoed back
  "lot_size": 50,                   # contract lot size. Default 1.
  "lots": 1,                        # number of lots. Default 1.
  "adjustment_points": 50,          # re-center when |spot - entry_strike| >= this. Default 50.
  "strike_step": 50,                # strike grid step used to round ATM. Default: auto from data.
  "candles": [                      # required: chronological intraday candles
    {
      "time": 1730000000,           # epoch seconds (or any sortable timestamp)
      "spot": 22500.0,              # underlying price at this candle
      "options": {                  # option close prices keyed by strike then leg
        "22500": {"ce": 180.0, "pe": 165.0},
        "22550": {"ce": 150.0, "pe": 190.0}
      }
    }, ...
  ]
}

The ATM strike per candle is the nearest available strike in that candle's
"options" map (or rounded to "strike_step" if provided). A candle is skipped if
its ATM legs have no price.

----------------------------------------------------------------------------
OUTPUT SCHEMA
----------------------------------------------------------------------------
{
  "error": false,
  "underlying": "NIFTY", "expiry": "30JAN26",
  "lot_size": 50, "lots": 1, "quantity": 50, "adjustment_points": 50,
  "pnl_series": [
    {"time": ..., "spot": ..., "atm_strike": ..., "entry_strike": ...,
     "ce_price": ..., "pe_price": ..., "straddle": ..., "pnl": ...}, ...
  ],
  "trades": [
    {"time": ..., "type": "ENTRY|ADJUSTMENT|EXIT", "strike": ..., "spot": ...,
     "ce_price": ..., "pe_price": ..., "straddle": ..., "leg_pnl": ...,
     "cumulative_pnl": ...}, ...
  ],
  "summary": {"total_pnl": ..., "total_adjustments": ..., "max_pnl": ..., "min_pnl": ...},
  "timestamp": 1730000000
}
"""

import json
import os
import sys
from datetime import datetime
from typing import Any, Dict, List, Optional


def _to_float(v, default=0.0) -> float:
    try:
        return default if v is None else float(v)
    except (TypeError, ValueError):
        return default


def _leg_price(options: Dict[str, Any], strike: float, leg: str) -> Optional[float]:
    """Look up a leg close price for a strike from a candle's options map."""
    if not isinstance(options, dict):
        return None
    # Strikes may be keyed as "22500" or "22500.0" or numeric — try a few forms.
    for key in (str(int(strike)) if float(strike).is_integer() else None,
                str(strike), repr(strike)):
        if key is None:
            continue
        cell = options.get(key)
        if isinstance(cell, dict):
            val = cell.get(leg)
            if val is not None:
                return _to_float(val, None)
    return None


def _nearest_atm(options: Dict[str, Any], spot: float, strike_step: float) -> Optional[float]:
    """Nearest strike to spot among the candle's option strikes (with both legs)."""
    if not isinstance(options, dict) or not options:
        return None
    strikes = []
    for k in options.keys():
        s = _to_float(k, None)
        if s is not None and s > 0:
            strikes.append(s)
    if not strikes:
        return None
    if strike_step and strike_step > 0:
        rounded = round(spot / strike_step) * strike_step
        # Snap the rounded value to an actual available strike if present.
        if any(abs(s - rounded) < 1e-6 for s in strikes):
            return rounded
    return min(strikes, key=lambda s: abs(s - spot))


def simulate(args: Dict[str, Any]) -> Dict[str, Any]:
    candles = args.get("candles")
    if not isinstance(candles, list) or not candles:
        return {"error": True, "message": "candles must be a non-empty list",
                "timestamp": int(datetime.now().timestamp())}

    lot_size = int(_to_float(args.get("lot_size"), 1.0)) or 1
    lots = int(_to_float(args.get("lots"), 1.0)) or 1
    quantity = lot_size * lots
    adjustment_points = _to_float(args.get("adjustment_points"), 50.0)
    strike_step = _to_float(args.get("strike_step"), 0.0)

    # Sort candles chronologically by their timestamp.
    candles = [c for c in candles if isinstance(c, dict)]
    candles.sort(key=lambda c: _to_float(c.get("time"), 0.0))

    pnl_series: List[Dict[str, Any]] = []
    trades: List[Dict[str, Any]] = []

    entry_strike: Optional[float] = None
    entry_ce = 0.0
    entry_pe = 0.0
    realized = 0.0
    adjustments = 0
    last_unrealized = 0.0

    n = len(candles)
    for i, candle in enumerate(candles):
        spot = _to_float(candle.get("spot"), 0.0)
        options = candle.get("options", {})
        ts = candle.get("time")
        if spot <= 0:
            continue
        atm = _nearest_atm(options, spot, strike_step)
        if atm is None:
            continue
        is_last = (i == n - 1)

        # ── ENTRY ──
        if entry_strike is None:
            ce0 = _leg_price(options, atm, "ce")
            pe0 = _leg_price(options, atm, "pe")
            if ce0 is None or pe0 is None:
                continue
            entry_strike, entry_ce, entry_pe = atm, ce0, pe0
            trades.append({
                "time": ts, "type": "ENTRY", "strike": atm, "spot": round(spot, 2),
                "ce_price": round(ce0, 2), "pe_price": round(pe0, 2),
                "straddle": round(ce0 + pe0, 2), "leg_pnl": 0.0,
                "cumulative_pnl": round(realized, 2),
            })
        else:
            # ── ADJUSTMENT ──
            if abs(atm - entry_strike) >= adjustment_points:
                old_ce = _leg_price(options, entry_strike, "ce")
                old_pe = _leg_price(options, entry_strike, "pe")
                new_ce = _leg_price(options, atm, "ce")
                new_pe = _leg_price(options, atm, "pe")
                if None not in (old_ce, old_pe, new_ce, new_pe):
                    leg_pnl = ((entry_ce - old_ce) + (entry_pe - old_pe)) * quantity
                    realized += leg_pnl
                    adjustments += 1
                    trades.append({
                        "time": ts, "type": "ADJUSTMENT",
                        "old_strike": entry_strike, "strike": atm, "spot": round(spot, 2),
                        "exit_ce": round(old_ce, 2), "exit_pe": round(old_pe, 2),
                        "exit_straddle": round(old_ce + old_pe, 2),
                        "ce_price": round(new_ce, 2), "pe_price": round(new_pe, 2),
                        "straddle": round(new_ce + new_pe, 2),
                        "leg_pnl": round(leg_pnl, 2),
                        "cumulative_pnl": round(realized, 2),
                    })
                    entry_strike, entry_ce, entry_pe = atm, new_ce, new_pe

        # ── Mark-to-market the open position ──
        cur_ce = _leg_price(options, entry_strike, "ce")
        cur_pe = _leg_price(options, entry_strike, "pe")
        if cur_ce is not None and cur_pe is not None:
            unrealized = ((entry_ce - cur_ce) + (entry_pe - cur_pe)) * quantity
            last_unrealized = unrealized
        else:
            unrealized = last_unrealized

        atm_ce = _leg_price(options, atm, "ce") or 0.0
        atm_pe = _leg_price(options, atm, "pe") or 0.0

        pnl_series.append({
            "time": ts, "spot": round(spot, 2), "atm_strike": atm,
            "entry_strike": entry_strike,
            "ce_price": round(atm_ce, 2), "pe_price": round(atm_pe, 2),
            "straddle": round(atm_ce + atm_pe, 2),
            "pnl": round(realized + unrealized, 2),
            "adjustments": adjustments,
        })

        # ── EXIT at last candle ──
        if is_last and entry_strike is not None:
            exit_ce = _leg_price(options, entry_strike, "ce")
            exit_pe = _leg_price(options, entry_strike, "pe")
            if exit_ce is not None and exit_pe is not None:
                leg_pnl = ((entry_ce - exit_ce) + (entry_pe - exit_pe)) * quantity
            else:
                leg_pnl = last_unrealized
            realized += leg_pnl
            trades.append({
                "time": ts, "type": "EXIT", "strike": entry_strike, "spot": round(spot, 2),
                "ce_price": round(exit_ce or 0.0, 2), "pe_price": round(exit_pe or 0.0, 2),
                "straddle": round((exit_ce or 0.0) + (exit_pe or 0.0), 2),
                "leg_pnl": round(leg_pnl, 2),
                "cumulative_pnl": round(realized, 2),
            })

    if not pnl_series:
        return {"error": True, "message": "No simulation data (option prices may be missing)",
                "timestamp": int(datetime.now().timestamp())}

    pnl_values = [p["pnl"] for p in pnl_series]

    return {
        "error": False,
        "underlying": args.get("underlying", ""),
        "expiry": args.get("expiry", ""),
        "lot_size": lot_size,
        "lots": lots,
        "quantity": quantity,
        "adjustment_points": adjustment_points,
        "pnl_series": pnl_series,
        "trades": trades,
        "summary": {
            "total_pnl": round(realized, 2),
            "total_adjustments": adjustments,
            "max_pnl": round(max(pnl_values), 2),
            "min_pnl": round(min(pnl_values), 2),
        },
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
                          "message": "Usage: straddle_simulator.py <command> <json_args>",
                          "commands": ["simulate"]}), flush=True)
        sys.exit(1)
    command = sys.argv[1]
    raw = resolve_arg(sys.argv[2]) if len(sys.argv) > 2 else "{}"
    try:
        args = json.loads(raw) if raw else {}
    except json.JSONDecodeError as e:
        print(json.dumps({"error": True, "message": f"Invalid JSON args: {e}"}), flush=True)
        sys.exit(1)
    result = simulate(args) if command == "simulate" else {"error": True, "message": f"Unknown command: {command}"}
    print(json.dumps(result, default=str), flush=True)


if __name__ == "__main__":
    main()
