"""
Marketstack Data Fetcher
End-of-day data for 70+ global exchanges, tickers, splits, dividends.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('MARKETSTACK_API_KEY', '')
BASE_URL = "https://api.marketstack.com/v1"

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

def get_eod(symbols: str, date_from: str = None, date_to: str = None, limit: int = 100) -> Any:
    params = {"access_key": API_KEY, "symbols": symbols, "limit": limit}
    if date_from:
        params["date_from"] = date_from
    if date_to:
        params["date_to"] = date_to
    return _make_request("eod", params)

def get_intraday(symbols: str, interval: str = "1hour", date_from: str = None, date_to: str = None) -> Any:
    params = {"access_key": API_KEY, "symbols": symbols, "interval": interval}
    if date_from:
        params["date_from"] = date_from
    if date_to:
        params["date_to"] = date_to
    return _make_request("intraday", params)

def get_tickers(exchange: str = None, limit: int = 100, search: str = None) -> Any:
    params = {"access_key": API_KEY, "limit": limit}
    if exchange:
        params["exchange"] = exchange
    if search:
        params["search"] = search
    return _make_request("tickers", params)

def get_exchanges(limit: int = 100, search: str = None) -> Any:
    params = {"access_key": API_KEY, "limit": limit}
    if search:
        params["search"] = search
    return _make_request("exchanges", params)

def get_splits(symbols: str, date_from: str = None, date_to: str = None) -> Any:
    params = {"access_key": API_KEY, "symbols": symbols}
    if date_from:
        params["date_from"] = date_from
    if date_to:
        params["date_to"] = date_to
    return _make_request("splits", params)

def get_dividends(symbols: str, date_from: str = None, date_to: str = None) -> Any:
    params = {"access_key": API_KEY, "symbols": symbols}
    if date_from:
        params["date_from"] = date_from
    if date_to:
        params["date_to"] = date_to
    return _make_request("dividends", params)

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "eod":
        symbols = args[1] if len(args) > 1 else "AAPL"
        date_from = args[2] if len(args) > 2 else None
        date_to = args[3] if len(args) > 3 else None
        limit = int(args[4]) if len(args) > 4 else 100
        result = get_eod(symbols, date_from, date_to, limit)
    elif command == "intraday":
        symbols = args[1] if len(args) > 1 else "AAPL"
        interval = args[2] if len(args) > 2 else "1hour"
        date_from = args[3] if len(args) > 3 else None
        date_to = args[4] if len(args) > 4 else None
        result = get_intraday(symbols, interval, date_from, date_to)
    elif command == "tickers":
        exchange = args[1] if len(args) > 1 else None
        limit = int(args[2]) if len(args) > 2 else 100
        search = args[3] if len(args) > 3 else None
        result = get_tickers(exchange, limit, search)
    elif command == "exchanges":
        limit = int(args[1]) if len(args) > 1 else 100
        search = args[2] if len(args) > 2 else None
        result = get_exchanges(limit, search)
    elif command == "splits":
        symbols = args[1] if len(args) > 1 else "AAPL"
        date_from = args[2] if len(args) > 2 else None
        date_to = args[3] if len(args) > 3 else None
        result = get_splits(symbols, date_from, date_to)
    elif command == "dividends":
        symbols = args[1] if len(args) > 1 else "AAPL"
        date_from = args[2] if len(args) > 2 else None
        date_to = args[3] if len(args) > 3 else None
        result = get_dividends(symbols, date_from, date_to)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
