"""Free market context for the Alternative Investments analyzers (A4 live-data prefill).

Run through services::alt::AltInvestmentsService as `<script> <command> --stdin` with the input
object as JSON on stdin; prints one JSON object ({"success": true, "data": {...}} or
{"success": false, "error": "..."}). Only keyless public sources, each answered with its own
"as of" so the screen can say how old a figure is; nothing is estimated:

  commodity_context  {"root": "CL"}   CFTC Commitments of Traders positioning (latest weekly report,
                                      change on the week before) and the World Bank Pink Sheet monthly
                                      price history (last 36 months) for the futures root's commodity.
                                      Each part may fail on its own; the other is still returned.
  crypto             {"id": "bitcoin"} CoinGecko (keyless tier, rate-limited: the screen runs it only
                                      on a click and caches the reply): price, market cap, 24h volume,
                                      circulating and maximum supply.
  selftest           (offline) checks the root mapping and the history trimming.

Treasury yields and futures contract-month prices come from the terminal's shared
CurveDataService (scripts/derivatives/deriv_market.py), not from this script.
"""

from __future__ import annotations

import datetime as _dt
import json
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS_ROOT = os.path.abspath(os.path.join(_HERE, "..", ".."))
if _SCRIPTS_ROOT not in sys.path:
    sys.path.insert(0, _SCRIPTS_ROOT)

import fincept_json  # noqa: E402

# Futures root -> (CFTC COT identifier, World Bank Pink Sheet key, plain name). None = no such series.
ROOTS = {
    "CL": ("crude_oil", "crude_oil_wti", "WTI crude oil"),
    "BZ": (None, "crude_oil_brent", "Brent crude oil"),
    "NG": ("natural_gas", "natural_gas_us", "US natural gas"),
    "RB": ("gasoline", None, "RBOB gasoline"),
    "HO": ("heating_oil", None, "Heating oil"),
    "GC": ("gold", "gold", "Gold"),
    "SI": ("silver", "silver", "Silver"),
    "HG": ("copper", "copper", "Copper"),
    "ZC": ("corn", "maize", "Corn"),
    "ZS": ("soybeans", "soybean", "Soybeans"),
    "ZW": ("wheat", "wheat_srw", "Wheat (SRW)"),
}
HISTORY_MONTHS = 36


def _now() -> str:
    return _dt.datetime.now(_dt.timezone.utc).isoformat(timespec="seconds")


def _trim_history(series: dict, months: int = HISTORY_MONTHS) -> dict:
    data = [o for o in series.get("data", []) if o.get("value") is not None]
    data = data[-months:]
    vals = [float(o["value"]) for o in data]
    out = {"title": series.get("title", ""), "unit": series.get("unit", ""), "data": data}
    if vals:
        out.update({"last": vals[-1], "last_date": data[-1]["date"], "low": min(vals), "high": max(vals)})
        if len(vals) >= 13:
            out["change_12m_pct"] = (vals[-1] / vals[-13] - 1.0) * 100.0 if vals[-13] else None
    return out


def _positioning(identifier: str) -> dict:
    import cftc_data  # scripts/cftc_data.py (keyless Socrata API)
    res = cftc_data.CFTCDataWrapper().analyze_market_sentiment(identifier, "legacy")
    if not isinstance(res, dict) or "error" in res:
        err = res.get("error") if isinstance(res, dict) else None
        msg = err.get("error") if isinstance(err, dict) else (err or "no reply")
        return {"error": f"CFTC positioning not available ({msg})."}
    data = res.get("data", res)
    nc = data.get("non_commercial_positions", {}) or {}
    cm = data.get("commercial_positions", {}) or {}
    out = {
        "report_date": data.get("latest_report"),
        "market": data.get("market_name", ""),
        "open_interest": data.get("open_interest"),
        "change_in_oi": data.get("change_in_oi"),
        "non_commercial_net": nc.get("net"),
        "non_commercial_long": nc.get("long"),
        "non_commercial_short": nc.get("short"),
        "commercial_net": cm.get("net"),
        "source": "CFTC Commitments of Traders (legacy, futures only)",
    }
    lc, sc = nc.get("long_change"), nc.get("short_change")
    if lc is not None and sc is not None:
        out["non_commercial_net_change"] = lc - sc
    return out


def _history(key: str) -> dict:
    import world_bank_commodity_data as wb  # scripts/world_bank_commodity_data.py (keyless xlsx)
    year = _dt.date.today().year
    res = wb._many([key], year - 4, year, {})  # noqa: SLF001 — the module's own multi-series reader
    if "error" in res:
        return {"error": res["error"]}
    series = res.get("data", {}).get(key, {})
    if "error" in series:
        return {"error": series["error"]}
    out = _trim_history(series)
    out.update({"source": "World Bank Pink Sheet (monthly)", "updated": res.get("updated", "")})
    return out


def commodity_context(inp: dict) -> dict:
    root = str(inp.get("root", "")).upper().strip()
    if root not in ROOTS:
        return {"success": False, "error": "Market context is available for: " + ", ".join(sorted(ROOTS)) + "."}
    cot_id, wb_key, name = ROOTS[root]
    out = {"root": root, "name": name, "as_of": _now()}
    if cot_id:
        try:
            out["positioning"] = _positioning(cot_id)
        except Exception as e:  # network / format: report, keep the other half
            out["positioning"] = {"error": f"CFTC positioning not available ({type(e).__name__})."}
    else:
        out["positioning"] = {"error": f"The CFTC report has no {name} contract."}
    if wb_key:
        try:
            out["history"] = _history(wb_key)
        except Exception as e:
            out["history"] = {"error": f"World Bank price history not available ({type(e).__name__})."}
    else:
        out["history"] = {"error": f"The World Bank Pink Sheet has no {name} series."}
    if "error" in out["positioning"] and "error" in out["history"]:
        return {"success": False, "error": out["positioning"]["error"] + " " + out["history"]["error"]}
    return {"success": True, "data": out}


def crypto(inp: dict) -> dict:
    coin = str(inp.get("id", "")).strip().lower()
    if not coin:
        return {"success": False, "error": "Enter a CoinGecko coin id (e.g. bitcoin, ethereum)."}
    import coingecko  # scripts/coingecko.py (keyless public tier)
    res = coingecko.get_coin_details(coin)
    if not isinstance(res, dict) or res.get("error") or "market_data" not in res:
        err = res.get("error") if isinstance(res, dict) else None
        if isinstance(err, dict):
            err = err.get("error") or err.get("message")
        return {"success": False, "error": f"CoinGecko has no data for '{coin}'" + (f" ({err})." if err else ".")}
    md = res.get("market_data", {}) or {}

    def usd(field):
        v = md.get(field)
        return v.get("usd") if isinstance(v, dict) else None

    return {"success": True, "data": {
        "id": coin, "name": res.get("name", coin), "symbol": str(res.get("symbol", "")).upper(),
        "price_usd": usd("current_price"), "market_cap_usd": usd("market_cap"),
        "volume_24h_usd": usd("total_volume"), "circulating_supply": md.get("circulating_supply"),
        "max_supply": md.get("max_supply"), "last_updated": md.get("last_updated") or res.get("last_updated"),
        "source": "CoinGecko (public API)", "as_of": _now()}}


def selftest() -> dict:
    checks = {}
    checks["roots_have_a_source"] = all(c or w for c, w, _ in ROOTS.values())
    hist = _trim_history({"title": "T", "unit": "$", "data": [{"date": f"2023M{m:02d}", "value": float(m)}
                                                               for m in range(1, 13)] +
                         [{"date": f"2024M{m:02d}", "value": float(12 + m)} for m in range(1, 13)] +
                         [{"date": "2025M01", "value": None}]}, months=13)
    checks["trim_keeps_last_n"] = len(hist["data"]) == 13 and hist["last"] == 24.0
    checks["low_high"] = hist["low"] == 12.0 and hist["high"] == 24.0
    checks["change_12m"] = abs(hist["change_12m_pct"] - 100.0) < 1e-9  # 24 / 12 - 1
    failed = [k for k, v in checks.items() if not v]
    return {"success": not failed, "data": {"checks": checks, "failed": failed}}


def main(argv) -> int:
    cmd = argv[1] if len(argv) > 1 else ""
    inp = {}
    if "--stdin" in argv:
        raw = sys.stdin.read().lstrip("﻿")
        try:
            inp = json.loads(raw) if raw.strip() else {}
        except ValueError:
            print(fincept_json.dumps({"success": False, "error": "The input could not be read."}))
            return 1
    try:
        # The CFTC / World Bank / CoinGecko modules and their libraries may print; stdout carries
        # exactly one JSON reply, printed after the block.
        with fincept_json.stdout_to_stderr():
            if cmd == "commodity_context":
                out = commodity_context(inp)
            elif cmd == "crypto":
                out = crypto(inp)
            elif cmd == "selftest":
                out = selftest()
            else:
                out = {"success": False, "error": "usage: alt_market_snapshot.py commodity_context|crypto|selftest --stdin"}
    except Exception as e:  # never a traceback on stdout
        out = {"success": False, "error": f"The market data could not be read: {e}"}
    print(fincept_json.dumps(out))
    return 0 if out.get("success") else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
