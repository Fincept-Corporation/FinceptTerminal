"""
OpenBB Data Fetcher
OpenBB Platform public endpoints: stocks, macro, alternatives, ETF holdings, economic calendar.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc

API_KEY = os.environ.get('OPENBB_API_KEY', '')
BASE_URL = "https://sdk.openbb.co"

SERVICE = "OpenBB"
KEY_ENV = "OPENBB_API_KEY"

session = fc.http_session()

def _make_request(endpoint: str, params: Dict = None) -> Any:
    # Every OpenBB Hub endpoint answers HTTP 401 without a personal access token.
    if not API_KEY:
        return fc.missing_key(SERVICE, "https://my.openbb.co/", KEY_ENV, "a personal access token")
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {"Authorization": f"Bearer {API_KEY}"}
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, KEY_ENV, API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}

def get_equity_price(symbol: str, provider: str = "yfinance") -> Any:
    return _make_request("api/v1/equity/price/historical", {"symbol": symbol, "provider": provider})

def get_macro_indicators(country: str = "united_states", indicator: str = "gdp") -> Any:
    return _make_request("api/v1/economy/indicators", {"country": country, "indicator": indicator})

def get_news(symbols: str = None, limit: int = 20) -> Any:
    params = {"limit": limit}
    if symbols:
        params["symbols"] = symbols
    return _make_request("api/v1/news/world", params)

def get_etf_holdings(symbol: str, provider: str = "fmp") -> Any:
    return _make_request("api/v1/etf/holdings", {"symbol": symbol, "provider": provider})

def get_alternatives_indices(provider: str = "cboe") -> Any:
    return _make_request("api/v1/index/available", {"provider": provider})

def get_economy_calendar(start_date: str = None, end_date: str = None) -> Any:
    params = {}
    if start_date:
        params["start_date"] = start_date
    if end_date:
        params["end_date"] = end_date
    return _make_request("api/v1/economy/calendar", params)

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "equity":
        symbol = args[1] if len(args) > 1 else "AAPL"
        provider = args[2] if len(args) > 2 else "yfinance"
        result = get_equity_price(symbol, provider)
    elif command == "macro":
        country = args[1] if len(args) > 1 else "united_states"
        indicator = args[2] if len(args) > 2 else "gdp"
        result = get_macro_indicators(country, indicator)
    elif command == "news":
        symbols = args[1] if len(args) > 1 else None
        limit = int(args[2]) if len(args) > 2 else 20
        result = get_news(symbols, limit)
    elif command == "etf":
        symbol = args[1] if len(args) > 1 else "SPY"
        provider = args[2] if len(args) > 2 else "fmp"
        result = get_etf_holdings(symbol, provider)
    elif command == "alternatives":
        provider = args[1] if len(args) > 1 else "cboe"
        result = get_alternatives_indices(provider)
    elif command == "calendar":
        start_date = args[1] if len(args) > 1 else None
        end_date = args[2] if len(args) > 2 else None
        result = get_economy_calendar(start_date, end_date)
    print(json.dumps(result))

if __name__ == "__main__":
    try:
        main()
    except Exception as e:  # bad argument (e.g. a non-numeric limit): still answer in JSON
        print(json.dumps({"error": fc.scrub(f"{type(e).__name__}: {e}", API_KEY)}))
