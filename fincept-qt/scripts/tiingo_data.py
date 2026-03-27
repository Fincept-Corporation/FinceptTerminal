"""
Tiingo Data Fetcher
EOD stock prices, intraday IEX data, crypto prices, forex, news — generous free tier.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('TIINGO_API_KEY', '')
BASE_URL = "https://api.tiingo.com"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {"Content-Type": "application/json", "Authorization": f"Token {API_KEY}"}
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

def get_daily_prices(ticker: str, start_date: str = None, end_date: str = None) -> Any:
    params = {}
    if start_date:
        params["startDate"] = start_date
    if end_date:
        params["endDate"] = end_date
    return _make_request(f"tiingo/daily/{ticker}/prices", params)

def get_intraday(ticker: str, start_date: str = None, end_date: str = None, resample_freq: str = "5min") -> Any:
    params = {"resampleFreq": resample_freq}
    if start_date:
        params["startDate"] = start_date
    if end_date:
        params["endDate"] = end_date
    return _make_request(f"iex/{ticker}", params)

def get_crypto_prices(ticker: str, start_date: str = None, end_date: str = None, resample_freq: str = "1hour") -> Any:
    params = {"resampleFreq": resample_freq}
    if start_date:
        params["startDate"] = start_date
    if end_date:
        params["endDate"] = end_date
    return _make_request(f"tiingo/crypto/{ticker}/prices", params)

def get_fx_prices(ticker: str, start_date: str = None, end_date: str = None, resample_freq: str = "1hour") -> Any:
    params = {"tickers": ticker, "resampleFreq": resample_freq}
    if start_date:
        params["startDate"] = start_date
    if end_date:
        params["endDate"] = end_date
    return _make_request("tiingo/fx/prices", params)

def get_news(tickers: str = None, tags: str = None, start_date: str = None, end_date: str = None, limit: int = 100) -> Any:
    params = {"limit": limit}
    if tickers:
        params["tickers"] = tickers
    if tags:
        params["tags"] = tags
    if start_date:
        params["startDate"] = start_date
    if end_date:
        params["endDate"] = end_date
    return _make_request("tiingo/news", params)

def get_ticker_metadata(ticker: str) -> Any:
    return _make_request(f"tiingo/daily/{ticker}", {})

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "daily":
        ticker = args[1] if len(args) > 1 else "AAPL"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_daily_prices(ticker, start_date, end_date)
    elif command == "intraday":
        ticker = args[1] if len(args) > 1 else "AAPL"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        resample_freq = args[4] if len(args) > 4 else "5min"
        result = get_intraday(ticker, start_date, end_date, resample_freq)
    elif command == "crypto":
        ticker = args[1] if len(args) > 1 else "btcusd"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        resample_freq = args[4] if len(args) > 4 else "1hour"
        result = get_crypto_prices(ticker, start_date, end_date, resample_freq)
    elif command == "forex":
        ticker = args[1] if len(args) > 1 else "eurusd"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        resample_freq = args[4] if len(args) > 4 else "1hour"
        result = get_fx_prices(ticker, start_date, end_date, resample_freq)
    elif command == "news":
        tickers = args[1] if len(args) > 1 else None
        tags = args[2] if len(args) > 2 else None
        start_date = args[3] if len(args) > 3 else None
        end_date = args[4] if len(args) > 4 else None
        limit = int(args[5]) if len(args) > 5 else 100
        result = get_news(tickers, tags, start_date, end_date, limit)
    elif command == "metadata":
        ticker = args[1] if len(args) > 1 else "AAPL"
        result = get_ticker_metadata(ticker)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
