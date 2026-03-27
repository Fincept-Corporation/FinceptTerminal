"""
Yahoo Finance Data Fetcher
Market summary, trending, movers, options chain via public Yahoo JSON APIs.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('YH_FINANCE_API_KEY', '')
BASE_URL = "https://query1.finance.yahoo.com"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {"User-Agent": "Mozilla/5.0 (compatible; FinceptTerminal/4.0)"}
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

def get_market_summary(region: str = "US") -> Any:
    return _make_request("v6/finance/quote/marketSummary", {"region": region, "lang": "en-US"})

def get_trending(region: str = "US", count: int = 20) -> Any:
    return _make_request(f"v1/finance/trending/{region}", {"count": count})

def get_movers(region: str = "US", type_: str = "gainers", count: int = 25) -> Any:
    screener_map = {"gainers": "day_gainers", "losers": "day_losers", "active": "most_actives"}
    screener = screener_map.get(type_, "day_gainers")
    return _make_request(f"v1/finance/screener/predefined/saved", {"scrIds": screener, "count": count, "region": region})

def get_options_chain(symbol: str, date: str = None) -> Any:
    params = {}
    if date:
        params["date"] = date
    return _make_request(f"v7/finance/options/{symbol}", params)

def get_recommendations(symbol: str) -> Any:
    return _make_request(f"v6/finance/recommendationsbysymbol/{symbol}", {})

def get_similar_stocks(symbol: str) -> Any:
    return _make_request("v6/finance/recommendationsbysymbol/" + symbol, {"lang": "en-US"})

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "summary":
        region = args[1] if len(args) > 1 else "US"
        result = get_market_summary(region)
    elif command == "trending":
        region = args[1] if len(args) > 1 else "US"
        count = int(args[2]) if len(args) > 2 else 20
        result = get_trending(region, count)
    elif command == "movers":
        region = args[1] if len(args) > 1 else "US"
        type_ = args[2] if len(args) > 2 else "gainers"
        count = int(args[3]) if len(args) > 3 else 25
        result = get_movers(region, type_, count)
    elif command == "options":
        symbol = args[1] if len(args) > 1 else "AAPL"
        date = args[2] if len(args) > 2 else None
        result = get_options_chain(symbol, date)
    elif command == "recommendations":
        symbol = args[1] if len(args) > 1 else "AAPL"
        result = get_recommendations(symbol)
    elif command == "similar":
        symbol = args[1] if len(args) > 1 else "AAPL"
        result = get_similar_stocks(symbol)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
