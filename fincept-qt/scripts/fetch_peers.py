"""
Suggest peer companies for a ticker.

Uses Yahoo Finance's industry grouping: the ticker's industryKey, then the
largest companies in that industry (yfinance Industry.top_companies). When the
industry list is empty it falls back to the largest companies in the sector.
Funds and ETFs have no industry, so they return an empty list and the caller
keeps its own default.

Usage: python fetch_peers.py <symbol> [count]
Output: {"success": true, "symbol": "ASML", "industry": "...", "peers": ["LRCX", ...]}

Yahoo via fincept_yahoo: the profile from the shared daily cache (fy.info), the industry /
sector leaders from equity_research_data.top_companies (cached 3 days, shared with the Equity
Research and Relationship Map peers).
"""

import json
import os
import sys

try:
    import fincept_json
except ImportError:  # scripts dir not on sys.path (unusual launcher)
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    import fincept_json

import fincept_yahoo as fy  # noqa: E402  (guards every yfinance request of the process)


def suggest_peers(symbol, count=5):
    import equity_research_data as erd

    symbol = symbol.strip().upper()
    info = fy.info(symbol)
    industry_key = info.get("industryKey")
    sector_key = info.get("sectorKey")
    peers = []

    def take(rows):
        for r in rows or []:
            s = r["symbol"]
            if s != symbol and s not in peers:
                peers.append(s)
            if len(peers) >= count:
                break

    if industry_key:
        try:
            take(erd.top_companies("Industry", industry_key))
        except Exception:
            pass
    if len(peers) < count and sector_key:
        try:
            take(erd.top_companies("Sector", sector_key))
        except Exception:
            pass

    return {
        "success": True,
        "symbol": symbol,
        "industry": info.get("industry"),
        "sector": info.get("sector"),
        "peers": peers[:count],
    }


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        return json.dumps({"success": False, "error": "Usage: python fetch_peers.py <symbol> [count]"})
    try:
        count = int(args[1]) if len(args) > 1 else 5
    except ValueError:
        return json.dumps({"success": False, "error": f"count must be a whole number (got {args[1]!r})"})
    try:
        # yfinance prints some failures to stdout; stdout carries only the JSON line below.
        with fincept_json.stdout_to_stderr():
            result = suggest_peers(args[0], count)
        return fincept_json.dumps(result, ensure_ascii=True)
    except Exception as e:
        return fincept_json.dumps({"success": False, "symbol": args[0], "error": str(e), "peers": []},
                                  ensure_ascii=True)


if __name__ == "__main__":
    print(main())
