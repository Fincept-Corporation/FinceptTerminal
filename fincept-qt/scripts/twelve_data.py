"""
Twelve Data Data Fetcher
Global stocks, forex, crypto, ETFs OHLCV — 800+ exchanges, 70+ countries.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('TWELVE_DATA_API_KEY', '')
BASE_URL = "https://api.twelvedata.com"

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

def get_time_series(symbol: str, interval: str = "1day", start_date: str = None, end_date: str = None, outputsize: int = 30) -> Any:
    params = {"symbol": symbol, "interval": interval, "outputsize": outputsize, "apikey": API_KEY}
    if start_date:
        params["start_date"] = start_date
    if end_date:
        params["end_date"] = end_date
    return _make_request("time_series", params)

def get_quote(symbol: str) -> Any:
    return _make_request("quote", {"symbol": symbol, "apikey": API_KEY})

def get_price(symbol: str) -> Any:
    return _make_request("price", {"symbol": symbol, "apikey": API_KEY})

def get_eod(symbol: str, date: str = None) -> Any:
    params = {"symbol": symbol, "apikey": API_KEY}
    if date:
        params["date"] = date
    return _make_request("eod", params)

def get_exchanges(type_: str = "stock", country: str = None) -> Any:
    params = {"type": type_, "apikey": API_KEY}
    if country:
        params["country"] = country
    return _make_request("exchanges", params)

def get_cryptocurrencies(symbol: str = None, exchange: str = None) -> Any:
    params = {"apikey": API_KEY}
    if symbol:
        params["symbol"] = symbol
    if exchange:
        params["exchange"] = exchange
    return _make_request("cryptocurrencies", params)

def get_forex_pairs(currency_group: str = None) -> Any:
    params = {"apikey": API_KEY}
    if currency_group:
        params["currency_group"] = currency_group
    return _make_request("forex_pairs", params)

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "time_series":
        symbol = args[1] if len(args) > 1 else "AAPL"
        interval = args[2] if len(args) > 2 else "1day"
        start_date = args[3] if len(args) > 3 else None
        end_date = args[4] if len(args) > 4 else None
        outputsize = int(args[5]) if len(args) > 5 else 30
        result = get_time_series(symbol, interval, start_date, end_date, outputsize)
    elif command == "quote":
        symbol = args[1] if len(args) > 1 else "AAPL"
        result = get_quote(symbol)
    elif command == "price":
        symbol = args[1] if len(args) > 1 else "AAPL"
        result = get_price(symbol)
    elif command == "eod":
        symbol = args[1] if len(args) > 1 else "AAPL"
        date = args[2] if len(args) > 2 else None
        result = get_eod(symbol, date)
    elif command == "exchanges":
        type_ = args[1] if len(args) > 1 else "stock"
        country = args[2] if len(args) > 2 else None
        result = get_exchanges(type_, country)
    elif command == "crypto":
        symbol = args[1] if len(args) > 1 else None
        exchange = args[2] if len(args) > 2 else None
        result = get_cryptocurrencies(symbol, exchange)
    elif command == "forex":
        currency_group = args[1] if len(args) > 1 else None
        result = get_forex_pairs(currency_group)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
