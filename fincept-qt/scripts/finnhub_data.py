"""
Finnhub Data Fetcher
Real-time quotes, fundamentals, earnings calendar, SEC filings, sentiment,
forex, and crypto data via the Finnhub Stock API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('FINNHUB_API_KEY', '')
BASE_URL = "https://finnhub.io/api/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["token"] = API_KEY
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


def get_quote(symbol: str) -> Any:
    return _make_request("quote", {"symbol": symbol.upper()})


def get_company_profile(symbol: str) -> Any:
    return _make_request("stock/profile2", {"symbol": symbol.upper()})


def get_financials(symbol: str, statement: str = "ic", freq: str = "annual") -> Any:
    return _make_request("financials-reported", {
        "symbol": symbol.upper(),
        "freq": freq,
    })


def get_earnings_calendar(from_date: str, to_date: str) -> Any:
    return _make_request("calendar/earnings", {
        "from": from_date,
        "to": to_date,
    })


def get_news(symbol: str, from_date: str, to_date: str) -> Any:
    return _make_request("company-news", {
        "symbol": symbol.upper(),
        "from": from_date,
        "to": to_date,
    })


def get_forex_rates(base: str = "USD") -> Any:
    return _make_request("forex/rates", {"base": base.upper()})


def get_crypto_candles(symbol: str, resolution: str = "D", from_ts: str = None, to_ts: str = None) -> Any:
    import time
    params = {
        "symbol": symbol,
        "resolution": resolution,
        "from": from_ts if from_ts else str(int(time.time()) - 86400 * 30),
        "to": to_ts if to_ts else str(int(time.time())),
    }
    return _make_request("crypto/candle", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "quote":
        symbol = args[1] if len(args) > 1 else "AAPL"
        result = get_quote(symbol)
    elif command == "profile":
        symbol = args[1] if len(args) > 1 else "AAPL"
        result = get_company_profile(symbol)
    elif command == "financials":
        symbol = args[1] if len(args) > 1 else "AAPL"
        statement = args[2] if len(args) > 2 else "ic"
        freq = args[3] if len(args) > 3 else "annual"
        result = get_financials(symbol, statement, freq)
    elif command == "earnings":
        from_date = args[1] if len(args) > 1 else "2024-01-01"
        to_date = args[2] if len(args) > 2 else "2024-12-31"
        result = get_earnings_calendar(from_date, to_date)
    elif command == "news":
        symbol = args[1] if len(args) > 1 else "AAPL"
        from_date = args[2] if len(args) > 2 else "2024-01-01"
        to_date = args[3] if len(args) > 3 else "2024-12-31"
        result = get_news(symbol, from_date, to_date)
    elif command == "forex":
        base = args[1] if len(args) > 1 else "USD"
        result = get_forex_rates(base)
    elif command == "crypto":
        symbol = args[1] if len(args) > 1 else "BINANCE:BTCUSDT"
        resolution = args[2] if len(args) > 2 else "D"
        from_ts = args[3] if len(args) > 3 else None
        to_ts = args[4] if len(args) > 4 else None
        result = get_crypto_candles(symbol, resolution, from_ts, to_ts)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
