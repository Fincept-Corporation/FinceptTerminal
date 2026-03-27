"""
EODHD Data Fetcher
End-of-day historical data for 70+ exchanges, fundamental data, bulk downloads.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('EODHD_API_KEY', '')
BASE_URL = "https://eodhd.com/api"

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

def get_eod_data(ticker: str, exchange: str = "US", from_date: str = None, to_date: str = None, period: str = "d") -> Any:
    params = {"api_token": API_KEY, "fmt": "json", "period": period}
    if from_date:
        params["from"] = from_date
    if to_date:
        params["to"] = to_date
    return _make_request(f"eod/{ticker}.{exchange}", params)

def get_fundamentals(ticker: str, exchange: str = "US") -> Any:
    return _make_request(f"fundamentals/{ticker}.{exchange}", {"api_token": API_KEY, "fmt": "json"})

def get_dividends(ticker: str, exchange: str = "US") -> Any:
    return _make_request(f"div/{ticker}.{exchange}", {"api_token": API_KEY, "fmt": "json"})

def get_splits(ticker: str, exchange: str = "US") -> Any:
    return _make_request(f"splits/{ticker}.{exchange}", {"api_token": API_KEY, "fmt": "json"})

def get_exchange_symbols(exchange: str = "US") -> Any:
    return _make_request(f"exchange-symbol-list/{exchange}", {"api_token": API_KEY, "fmt": "json"})

def get_live_price(ticker: str, exchange: str = "US") -> Any:
    return _make_request(f"real-time/{ticker}.{exchange}", {"api_token": API_KEY, "fmt": "json"})

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "eod":
        ticker = args[1] if len(args) > 1 else "AAPL"
        exchange = args[2] if len(args) > 2 else "US"
        from_date = args[3] if len(args) > 3 else None
        to_date = args[4] if len(args) > 4 else None
        period = args[5] if len(args) > 5 else "d"
        result = get_eod_data(ticker, exchange, from_date, to_date, period)
    elif command == "fundamentals":
        ticker = args[1] if len(args) > 1 else "AAPL"
        exchange = args[2] if len(args) > 2 else "US"
        result = get_fundamentals(ticker, exchange)
    elif command == "dividends":
        ticker = args[1] if len(args) > 1 else "AAPL"
        exchange = args[2] if len(args) > 2 else "US"
        result = get_dividends(ticker, exchange)
    elif command == "splits":
        ticker = args[1] if len(args) > 1 else "AAPL"
        exchange = args[2] if len(args) > 2 else "US"
        result = get_splits(ticker, exchange)
    elif command == "symbols":
        exchange = args[1] if len(args) > 1 else "US"
        result = get_exchange_symbols(exchange)
    elif command == "live":
        ticker = args[1] if len(args) > 1 else "AAPL"
        exchange = args[2] if len(args) > 2 else "US"
        result = get_live_price(ticker, exchange)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
