"""
Suggest peer companies for a ticker.

Uses Yahoo Finance's industry grouping: the ticker's industryKey, then the
largest companies in that industry (yfinance Industry.top_companies). When the
industry list is empty it falls back to the largest companies in the sector.
Funds and ETFs have no industry, so they return an empty list and the caller
keeps its own default.

Usage: python fetch_peers.py <symbol> [count]
Output: {"success": true, "symbol": "ASML", "industry": "...", "peers": ["LRCX", ...]}
"""

import json
import sys


def suggest_peers(symbol, count=5):
    import yfinance as yf

    symbol = symbol.strip().upper()
    info = yf.Ticker(symbol).info or {}
    industry_key = info.get("industryKey")
    sector_key = info.get("sectorKey")
    peers = []

    def take(df):
        if df is None or getattr(df, "empty", True):
            return
        for sym in list(df.index):
            s = str(sym).upper()
            if s != symbol and s not in peers:
                peers.append(s)
            if len(peers) >= count:
                break

    if industry_key:
        try:
            take(yf.Industry(industry_key).top_companies)
        except Exception:
            pass
    if len(peers) < count and sector_key:
        try:
            take(yf.Sector(sector_key).top_companies)
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
    count = int(args[1]) if len(args) > 1 else 5
    try:
        return json.dumps(suggest_peers(args[0], count))
    except Exception as e:
        return json.dumps({"success": False, "symbol": args[0], "error": str(e), "peers": []})


if __name__ == "__main__":
    print(main())
