"""
Open Interest Tracker + Max Pain
================================

Aggregates open interest across an option chain (totals, PCR, per-strike PCR)
and computes the Max Pain strike — the strike at which option writers suffer the
least aggregate loss at expiry.

Ported from OpenAlgo `services/oi_tracker_service.py`. Broker/Flask/DB fetching
is stripped; the option chain is PASSED IN. Pure math — no external deps beyond
the standard library (numpy used only if available, not required).

----------------------------------------------------------------------------
I/O CONVENTION (matches scripts/databento_fno_chain.py etc.)
----------------------------------------------------------------------------
    python oi_tracker.py compute '<json_args>'
    python oi_tracker.py compute @C:/path/to/spilled_args.json

argv[1] = command ("compute"); argv[2] = JSON args (or "@<path>" temp file).
Result JSON printed to stdout.

----------------------------------------------------------------------------
INPUT SCHEMA (argv[2] JSON object)
----------------------------------------------------------------------------
{
  "spot": 22500.0,                  # optional underlying spot (used to mark ATM)
  "expiry": "30JAN26",              # optional, echoed back
  "lot_size": 50,                   # optional default lot size. Default 1.
  "chain": [                        # required: per-strike rows
    {"strike": 22500, "lot_size": 50,
     "ce_oi": 123400, "pe_oi": 98700,
     "ce_volume": 45000, "pe_volume": 38000,
     "ce_oi_change": 5000, "pe_oi_change": -2000}     # optional OI change fields
  ]
}

----------------------------------------------------------------------------
OUTPUT SCHEMA
----------------------------------------------------------------------------
{
  "error": false,
  "spot": 22500.0, "expiry": "30JAN26", "atm_strike": 22500, "lot_size": 50,
  "totals": {
    "total_ce_oi": ..., "total_pe_oi": ...,
    "total_ce_volume": ..., "total_pe_volume": ...,
    "total_ce_oi_change": ..., "total_pe_oi_change": ...,
    "pcr_oi": 0.84, "pcr_volume": 0.92
  },
  "chain": [
    {"strike": 22500, "ce_oi": ..., "pe_oi": ..., "pcr": 0.80,
     "ce_volume": ..., "pe_volume": ..., "ce_oi_change": ..., "pe_oi_change": ...}, ...
  ],
  "max_pain_strike": 22400,
  "pain_data": [
    {"strike": 22400, "ce_pain": ..., "pe_pain": ..., "total_pain": ...}, ...
  ],
  "support_resistance": {"max_ce_oi_strike": 23000, "max_pe_oi_strike": 22000},
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


def compute(args: Dict[str, Any]) -> Dict[str, Any]:
    chain = args.get("chain")
    if not isinstance(chain, list) or not chain:
        return {"error": True, "message": "chain must be a non-empty list",
                "timestamp": int(datetime.now().timestamp())}

    spot = _to_float(args.get("spot"), 0.0)
    default_lot = int(_to_float(args.get("lot_size"), 1.0)) or 1

    rows: List[Dict[str, Any]] = []
    lot_size_seen: Optional[int] = None
    atm_strike, atm_dist = None, None

    for item in chain:
        if not isinstance(item, dict):
            continue
        strike = _to_float(item.get("strike"), 0.0)
        if strike <= 0:
            continue
        row_lot = int(_to_float(item.get("lot_size"), 0.0))
        if row_lot > 0 and lot_size_seen is None:
            lot_size_seen = row_lot

        if spot > 0:
            dist = abs(strike - spot)
            if atm_dist is None or dist < atm_dist:
                atm_dist, atm_strike = dist, strike

        ce_oi = _to_float(item.get("ce_oi"), 0.0)
        pe_oi = _to_float(item.get("pe_oi"), 0.0)
        rows.append({
            "strike": strike,
            "ce_oi": ce_oi,
            "pe_oi": pe_oi,
            "ce_volume": _to_float(item.get("ce_volume"), 0.0),
            "pe_volume": _to_float(item.get("pe_volume"), 0.0),
            "ce_oi_change": _to_float(item.get("ce_oi_change"), 0.0),
            "pe_oi_change": _to_float(item.get("pe_oi_change"), 0.0),
            "pcr": round(pe_oi / ce_oi, 4) if ce_oi > 0 else 0.0,
        })

    if not rows:
        return {"error": True, "message": "No valid strike rows",
                "timestamp": int(datetime.now().timestamp())}

    rows.sort(key=lambda x: x["strike"])
    lot_size = lot_size_seen or default_lot

    total_ce_oi = sum(rw["ce_oi"] for rw in rows)
    total_pe_oi = sum(rw["pe_oi"] for rw in rows)
    total_ce_vol = sum(rw["ce_volume"] for rw in rows)
    total_pe_vol = sum(rw["pe_volume"] for rw in rows)
    total_ce_oi_chg = sum(rw["ce_oi_change"] for rw in rows)
    total_pe_oi_chg = sum(rw["pe_oi_change"] for rw in rows)

    pcr_oi = round(total_pe_oi / total_ce_oi, 4) if total_ce_oi > 0 else 0.0
    pcr_vol = round(total_pe_vol / total_ce_vol, 4) if total_ce_vol > 0 else 0.0

    # ── Max Pain ──────────────────────────────────────────────────────────────
    # For each candidate settlement strike, sum writer losses:
    #   CE writers lose (candidate - strike) * ce_oi for all strikes BELOW candidate
    #   PE writers lose (strike - candidate) * pe_oi for all strikes ABOVE candidate
    # Max pain = candidate with minimum total writer loss.
    pain_data: List[Dict[str, Any]] = []
    for candidate in rows:
        c_strike = candidate["strike"]
        ce_pain = 0.0
        pe_pain = 0.0
        for item in rows:
            strike = item["strike"]
            if c_strike > strike and item["ce_oi"] > 0:
                ce_pain += (c_strike - strike) * item["ce_oi"]
            if c_strike < strike and item["pe_oi"] > 0:
                pe_pain += (strike - c_strike) * item["pe_oi"]
        total_pain = ce_pain + pe_pain
        pain_data.append({
            "strike": c_strike,
            "ce_pain": round(ce_pain, 2),
            "pe_pain": round(pe_pain, 2),
            "total_pain": round(total_pain, 2),
        })

    max_pain_entry = min(pain_data, key=lambda x: x["total_pain"])
    max_pain_strike = max_pain_entry["strike"]

    # Support/resistance proxies: highest PE OI = support, highest CE OI = resistance.
    max_ce = max(rows, key=lambda x: x["ce_oi"])["strike"]
    max_pe = max(rows, key=lambda x: x["pe_oi"])["strike"]

    return {
        "error": False,
        "spot": spot,
        "expiry": args.get("expiry", ""),
        "atm_strike": atm_strike,
        "lot_size": lot_size,
        "totals": {
            "total_ce_oi": total_ce_oi,
            "total_pe_oi": total_pe_oi,
            "total_ce_volume": total_ce_vol,
            "total_pe_volume": total_pe_vol,
            "total_ce_oi_change": total_ce_oi_chg,
            "total_pe_oi_change": total_pe_oi_chg,
            "pcr_oi": pcr_oi,
            "pcr_volume": pcr_vol,
        },
        "chain": rows,
        "max_pain_strike": max_pain_strike,
        "pain_data": pain_data,
        "support_resistance": {"max_ce_oi_strike": max_ce, "max_pe_oi_strike": max_pe},
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
                          "message": "Usage: oi_tracker.py <command> <json_args>",
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
