"""
TradingView Data Fetcher
Screener data, economic calendar, market overview using public endpoints.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('TRADINGVIEW_API_KEY', '')
BASE_URL = "https://scanner.tradingview.com"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

def _post_screener(market: str, payload: Dict) -> Any:
    url = f"{BASE_URL}/{market}/scan"
    try:
        response = session.post(url, json=payload, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

def get_stock_screener(markets: List[str] = None, filters: List[Dict] = None, sort: Dict = None, columns: List[str] = None, limit: int = 50) -> Any:
    if markets is None:
        markets = ["america"]
    if columns is None:
        columns = ["name", "close", "change", "change_abs", "volume", "market_cap_basic"]
    payload = {"filter": filters or [], "options": {"lang": "en"}, "symbols": {"query": {"types": []}, "tickers": []},
               "columns": columns, "sort": sort or {"sortBy": "market_cap_basic", "sortOrder": "desc"}, "range": [0, limit]}
    return _post_screener(markets[0], payload)

def get_forex_screener(filters: List[Dict] = None, sort: Dict = None, limit: int = 50) -> Any:
    columns = ["name", "close", "change", "change_abs", "Recommend.All"]
    payload = {"filter": filters or [], "columns": columns,
               "sort": sort or {"sortBy": "change_abs", "sortOrder": "desc"}, "range": [0, limit]}
    return _post_screener("forex", payload)

def get_crypto_screener(filters: List[Dict] = None, sort: Dict = None, limit: int = 50) -> Any:
    columns = ["name", "close", "change", "change_abs", "volume", "market_cap_calc"]
    payload = {"filter": filters or [], "columns": columns,
               "sort": sort or {"sortBy": "market_cap_calc", "sortOrder": "desc"}, "range": [0, limit]}
    return _post_screener("crypto", payload)

def get_economic_calendar(countries: List[str] = None, importance: List[int] = None, from_date: str = None, to_date: str = None) -> Any:
    url = "https://economic-calendar.tradingview.com/events"
    params = {}
    if from_date:
        params["from"] = from_date
    if to_date:
        params["to"] = to_date
    if countries:
        params["countries"] = ",".join(countries)
    if importance:
        params["importance"] = ",".join(str(i) for i in importance)
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

def get_market_overview(market: str = "america") -> Any:
    columns = ["name", "close", "change", "change_abs", "volume"]
    payload = {"columns": columns, "sort": {"sortBy": "change", "sortOrder": "desc"}, "range": [0, 20]}
    return _post_screener(market, payload)

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "stocks":
        markets = args[1].split(",") if len(args) > 1 else ["america"]
        limit = int(args[2]) if len(args) > 2 else 50
        result = get_stock_screener(markets=markets, limit=limit)
    elif command == "forex":
        limit = int(args[1]) if len(args) > 1 else 50
        result = get_forex_screener(limit=limit)
    elif command == "crypto":
        limit = int(args[1]) if len(args) > 1 else 50
        result = get_crypto_screener(limit=limit)
    elif command == "calendar":
        countries = args[1].split(",") if len(args) > 1 else None
        from_date = args[2] if len(args) > 2 else None
        to_date = args[3] if len(args) > 3 else None
        result = get_economic_calendar(countries=countries, from_date=from_date, to_date=to_date)
    elif command == "overview":
        market = args[1] if len(args) > 1 else "america"
        result = get_market_overview(market)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
