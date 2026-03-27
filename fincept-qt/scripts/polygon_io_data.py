"""
Polygon.io Data Fetcher
US stocks OHLCV, trades, quotes, market-wide aggregates, options, crypto,
and forex data via the Polygon.io REST API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('POLYGON_API_KEY', '')
BASE_URL = "https://api.polygon.io/v2"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["apiKey"] = API_KEY
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


def _make_request_v3(endpoint: str, params: Dict = None) -> Any:
    url = f"https://api.polygon.io/v3/{endpoint}"
    if params is None:
        params = {}
    params["apiKey"] = API_KEY
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


def get_aggregates(ticker: str, multiplier: str = "1", timespan: str = "day", from_date: str = "2024-01-01", to_date: str = "2024-12-31") -> Any:
    endpoint = f"aggs/ticker/{ticker.upper()}/range/{multiplier}/{timespan}/{from_date}/{to_date}"
    return _make_request(endpoint, {"adjusted": "true", "sort": "asc", "limit": 5000})


def get_daily_open_close(ticker: str, date: str) -> Any:
    endpoint = f"aggs/open-close/{ticker.upper()}/{date}"
    return _make_request(endpoint, {"adjusted": "true"})


def get_grouped_daily(date: str) -> Any:
    endpoint = f"aggs/grouped/locale/us/market/stocks/{date}"
    return _make_request(endpoint, {"adjusted": "true"})


def get_ticker_details(ticker: str) -> Any:
    return _make_request_v3(f"reference/tickers/{ticker.upper()}")


def get_market_status() -> Any:
    url = "https://api.polygon.io/v1/marketstatus/now"
    params = {"apiKey": API_KEY}
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


def get_crypto_daily(ticker: str, date: str) -> Any:
    endpoint = f"aggs/open-close/crypto/{ticker.upper()}/{date}"
    return _make_request(endpoint)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "aggregates":
        ticker = args[1] if len(args) > 1 else "AAPL"
        multiplier = args[2] if len(args) > 2 else "1"
        timespan = args[3] if len(args) > 3 else "day"
        from_date = args[4] if len(args) > 4 else "2024-01-01"
        to_date = args[5] if len(args) > 5 else "2024-12-31"
        result = get_aggregates(ticker, multiplier, timespan, from_date, to_date)
    elif command == "daily":
        ticker = args[1] if len(args) > 1 else "AAPL"
        date = args[2] if len(args) > 2 else "2024-01-02"
        result = get_daily_open_close(ticker, date)
    elif command == "grouped":
        date = args[1] if len(args) > 1 else "2024-01-02"
        result = get_grouped_daily(date)
    elif command == "details":
        ticker = args[1] if len(args) > 1 else "AAPL"
        result = get_ticker_details(ticker)
    elif command == "status":
        result = get_market_status()
    elif command == "crypto":
        ticker = args[1] if len(args) > 1 else "BTC-USD"
        date = args[2] if len(args) > 2 else "2024-01-02"
        result = get_crypto_daily(ticker, date)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
