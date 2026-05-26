"""
Databento FNO Chain Provider
=============================
Fetches option chain data from Databento for the FNO tab.
Produces OptionChain-compatible JSON for C++ consumption.

Commands:
    list_expiries  {"api_key": "...", "symbol": "SPY"}
    get_chain      {"api_key": "...", "symbol": "SPY", "expiry": "2026-06-20"}

Called via PythonRunner from OptionChainService.
"""

import sys
import json
import os
from datetime import datetime, timedelta
from typing import Dict, List, Any, Optional

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from databento_provider import DatabentoProvider, DATABENTO_AVAILABLE, DATABENTO_ERROR


def _parse_expiration(exp_str: str) -> Optional[datetime]:
    """Parse Databento expiration field (nanosecond epoch, ISO, or date string)."""
    s = str(exp_str).strip()
    if s.isdigit() and len(s) >= 16:
        try:
            return datetime.utcfromtimestamp(int(s) // 1_000_000_000)
        except (ValueError, OSError):
            pass
    if len(s) >= 10:
        try:
            return datetime.strptime(s[:10], "%Y-%m-%d")
        except ValueError:
            pass
    return None


def list_expiries(args: Dict[str, Any]) -> Dict[str, Any]:
    api_key = args.get("api_key", "")
    symbol = args.get("symbol", "SPY")
    date = args.get("date")

    try:
        provider = DatabentoProvider(api_key)
        defs = provider.get_options_definitions(symbol, date)
        if defs.get("error"):
            return defs

        today = datetime.now()
        seen = set()
        expiries = []
        for d in defs.get("data", []):
            exp_dt = _parse_expiration(d.get("expiration", ""))
            if exp_dt is None:
                continue
            exp_date = exp_dt.strftime("%Y-%m-%d")
            if exp_date in seen:
                continue
            if exp_dt <= today:
                continue
            seen.add(exp_date)
            expiries.append(exp_date)

        expiries.sort()
        return {
            "error": False,
            "symbol": symbol,
            "expiries": expiries,
            "count": len(expiries),
            "timestamp": int(datetime.now().timestamp()),
        }

    except Exception as e:
        return {
            "error": True,
            "message": f"Failed to list expiries: {e}",
            "symbol": symbol,
            "timestamp": int(datetime.now().timestamp()),
        }


def get_chain(args: Dict[str, Any]) -> Dict[str, Any]:
    api_key = args.get("api_key", "")
    symbol = args.get("symbol", "SPY")
    expiry = args.get("expiry", "")
    date = args.get("date")
    strike_window_pct = int(args.get("strike_window_pct", "30"))

    if not expiry:
        return {"error": True, "message": "expiry is required",
                "timestamp": int(datetime.now().timestamp())}

    try:
        provider = DatabentoProvider(api_key)

        # Step 1: Fetch definitions (cached 24h on disk)
        defs = provider.get_options_definitions(symbol, date)
        if defs.get("error"):
            return defs

        target_expiry = datetime.strptime(expiry, "%Y-%m-%d")

        # Filter definitions to the requested expiry
        filtered = []
        for d in defs.get("data", []):
            exp_dt = _parse_expiration(d.get("expiration", ""))
            if exp_dt is None:
                continue
            if abs((exp_dt - target_expiry).days) > 1:
                continue
            ic = str(d.get("instrument_class", "")).upper().strip()
            if ic not in ("C", "P"):
                continue
            strike = d.get("strike_price", 0)
            if strike <= 0:
                continue
            filtered.append(d)

        if not filtered:
            return {"error": True,
                    "message": f"No options found for {symbol} expiry {expiry}",
                    "timestamp": int(datetime.now().timestamp())}

        # Step 2: Fetch underlying spot price
        spot = _fetch_spot(provider, symbol, date)

        # Step 3: Filter to near-the-money if spot is available
        if spot > 0:
            near = []
            for d in filtered:
                moneyness = d["strike_price"] / spot
                lo = 1.0 - strike_window_pct / 100.0
                hi = 1.0 + strike_window_pct / 100.0
                if lo <= moneyness <= hi:
                    near.append(d)
            if near:
                filtered = near

        # Step 4: Group by strike into CE/PE pairs
        by_strike: Dict[float, Dict[str, Any]] = {}
        for d in filtered:
            strike = d["strike_price"]
            ic = str(d.get("instrument_class", "")).upper().strip()
            if strike not in by_strike:
                by_strike[strike] = {"strike": strike, "ce": None, "pe": None}
            if ic == "C":
                by_strike[strike]["ce"] = d
            elif ic == "P":
                by_strike[strike]["pe"] = d

        # Step 5: Fetch quotes via ohlcv-1d (cheaper and more reliable than cmbp-1)
        option_symbols = [d["raw_symbol"] for d in filtered[:400]]
        quotes = _fetch_option_quotes(provider, option_symbols, date)

        # Step 6: Assemble chain rows
        rows = []
        for strike in sorted(by_strike.keys()):
            pair = by_strike[strike]
            ce = pair["ce"]
            pe = pair["pe"]

            row = {
                "strike": strike,
                "lot_size": 100,
                "ce_token": int(ce["instrument_id"]) if ce else 0,
                "pe_token": int(pe["instrument_id"]) if pe else 0,
                "ce_symbol": ce["raw_symbol"] if ce else "",
                "pe_symbol": pe["raw_symbol"] if pe else "",
                "ce_bid": 0.0, "ce_ask": 0.0, "ce_ltp": 0.0,
                "ce_volume": 0, "ce_oi": 0,
                "pe_bid": 0.0, "pe_ask": 0.0, "pe_ltp": 0.0,
                "pe_volume": 0, "pe_oi": 0,
            }

            if ce and ce["raw_symbol"] in quotes:
                q = quotes[ce["raw_symbol"]]
                row["ce_ltp"] = q.get("close", 0)
                row["ce_bid"] = q.get("bid", q.get("close", 0))
                row["ce_ask"] = q.get("ask", q.get("close", 0))
                row["ce_volume"] = q.get("volume", 0)

            if pe and pe["raw_symbol"] in quotes:
                q = quotes[pe["raw_symbol"]]
                row["pe_ltp"] = q.get("close", 0)
                row["pe_bid"] = q.get("bid", q.get("close", 0))
                row["pe_ask"] = q.get("ask", q.get("close", 0))
                row["pe_volume"] = q.get("volume", 0)

            if row["ce_ltp"] > 0 or row["pe_ltp"] > 0:
                rows.append(row)

        if not rows:
            return {"error": True,
                    "message": f"No priced options for {symbol} expiry {expiry}",
                    "timestamp": int(datetime.now().timestamp())}

        return {
            "error": False,
            "symbol": symbol,
            "expiry": expiry,
            "spot": spot,
            "rows": rows,
            "count": len(rows),
            "timestamp": int(datetime.now().timestamp()),
        }

    except Exception as e:
        return {
            "error": True,
            "message": f"Failed to fetch chain: {e}",
            "symbol": symbol,
            "expiry": expiry,
            "timestamp": int(datetime.now().timestamp()),
        }


def _fetch_spot(provider: DatabentoProvider, symbol: str, date: str = None) -> float:
    """Fetch underlying spot price from equity OHLCV."""
    try:
        result = provider.get_historical_ohlcv(
            symbols=[symbol], days=5,
            dataset=None, schema="ohlcv-1d"
        )
        if result.get("error"):
            return 0.0
        data = result.get("data", {})
        bars = data.get(symbol, [])
        if not bars:
            return 0.0
        last = bars[-1]
        return float(last.get("close", 0))
    except Exception:
        return 0.0


def _fetch_option_quotes(
    provider: DatabentoProvider,
    symbols: List[str],
    date: str = None,
) -> Dict[str, Dict[str, Any]]:
    """Fetch option close prices via ohlcv-1d from OPRA.PILLAR."""
    if not symbols:
        return {}

    try:
        if not date:
            date = (datetime.now() - timedelta(days=1)).strftime("%Y-%m-%d")

        target_date = datetime.strptime(date, "%Y-%m-%d")
        ohlcv_start, ohlcv_end = provider._clamp_window(
            provider.OPRA_DATASET, target_date, target_date + timedelta(days=1),
            schema="ohlcv-1d"
        )

        cache_key = f"fno_ohlcv_{provider.OPRA_DATASET.replace('.', '_')}_{date}"
        cached = provider._cache_load(cache_key, max_age_seconds=6 * 3600)
        if cached is not None:
            return cached

        data = provider.client.timeseries.get_range(
            dataset=provider.OPRA_DATASET,
            schema="ohlcv-1d",
            symbols=symbols,
            stype_in="raw_symbol",
            start=ohlcv_start.strftime("%Y-%m-%dT%H:%M:%S"),
            end=ohlcv_end.strftime("%Y-%m-%dT%H:%M:%S"),
        )

        quotes = {}
        try:
            import pandas as pd
            df = data.to_df()
            if not df.empty:
                for sym, grp in df.groupby("symbol"):
                    grp_s = grp.sort_values("volume", ascending=False)
                    r = grp_s.iloc[0]
                    close = float(r.get("close", 0))
                    vol = int(r.get("volume", 0))
                    if close > 1e6:
                        close = close / 1e9
                    if close > 0:
                        quotes[str(sym)] = {
                            "close": close,
                            "volume": vol,
                        }
        except ImportError:
            for row in data:
                sym = str(getattr(row, "symbol", ""))
                close_raw = getattr(row, "close", 0)
                close = close_raw / 1e9 if close_raw > 1e6 else close_raw
                vol = getattr(row, "volume", 0)
                if close > 0 and sym:
                    if sym not in quotes or vol > quotes[sym].get("volume", 0):
                        quotes[sym] = {"close": float(close), "volume": int(vol)}

        if quotes:
            provider._cache_save(cache_key, quotes)

        return quotes

    except Exception:
        return {}


def main():
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": True,
            "message": "Usage: databento_fno_chain.py <command> <json_args>",
            "commands": ["list_expiries", "get_chain"],
            "databento_available": DATABENTO_AVAILABLE,
        }), flush=True)
        sys.exit(1)

    command = sys.argv[1]

    if not DATABENTO_AVAILABLE:
        print(json.dumps({
            "error": True,
            "message": f"databento package not installed: {DATABENTO_ERROR}",
        }), flush=True)
        sys.exit(1)

    args = {}
    if len(sys.argv) > 2:
        try:
            args = json.loads(sys.argv[2])
        except json.JSONDecodeError as e:
            print(json.dumps({
                "error": True,
                "message": f"Invalid JSON args: {e}",
            }), flush=True)
            sys.exit(1)

    if command == "list_expiries":
        result = list_expiries(args)
    elif command == "get_chain":
        result = get_chain(args)
    else:
        result = {
            "error": True,
            "message": f"Unknown command: {command}",
        }

    print(json.dumps(result, default=str), flush=True)


if __name__ == "__main__":
    main()
