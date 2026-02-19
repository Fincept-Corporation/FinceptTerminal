#!/usr/bin/env python3
"""
Prediction markets data fetcher
Uses Manifold Markets API (globally accessible, no auth required)
as a fallback since Polymarket is geo-restricted in some regions.
"""

import sys
import json

try:
    import requests
except ImportError:
    print(json.dumps({"success": False, "error": "requests not available", "data": []}))
    sys.exit(1)

MANIFOLD_API = "https://api.manifold.markets/v0"


def get_markets(limit=10):
    """Fetch top active markets from Manifold Markets API"""
    try:
        url = f"{MANIFOLD_API}/markets"
        params = {
            "limit": limit,
            "sort": "last-bet-time",
            "order": "desc",
        }
        headers = {
            "Accept": "application/json",
            "User-Agent": "FinceptTerminal/3.3.1",
        }
        r = requests.get(url, params=params, headers=headers, timeout=15)
        r.raise_for_status()
        raw = r.json()

        # Normalize Manifold fields to match PolymarketMarket shape expected by frontend
        markets = []
        for m in raw:
            if m.get("outcomeType") != "BINARY":
                continue
            prob = float(m.get("probability", 0))
            yes_price = str(round(prob, 4))
            no_price = str(round(1 - prob, 4))
            volume = str(round(float(m.get("volume", 0)), 0))
            markets.append({
                "id": m.get("id", ""),
                "question": m.get("question", ""),
                "outcomePrices": [yes_price, no_price],
                "volume": volume,
                "active": not m.get("isResolved", False),
                "closed": m.get("isResolved", False),
                "url": m.get("url", ""),
            })
            if len(markets) >= limit:
                break

        return {"success": True, "data": markets, "count": len(markets)}
    except requests.exceptions.Timeout:
        return {"success": False, "error": "Request timed out", "data": []}
    except requests.exceptions.ConnectionError as e:
        return {"success": False, "error": f"Connection failed: {str(e)}", "data": []}
    except Exception as e:
        return {"success": False, "error": str(e), "data": []}


def main():
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command specified", "data": []}))
        sys.exit(1)

    cmd = sys.argv[1]
    if cmd == "get_markets":
        limit = int(sys.argv[2]) if len(sys.argv) > 2 else 10
        result = get_markets(limit)
    else:
        result = {"success": False, "error": f"Unknown command: {cmd}", "data": []}

    print(json.dumps(result, default=str))


if __name__ == "__main__":
    main()
