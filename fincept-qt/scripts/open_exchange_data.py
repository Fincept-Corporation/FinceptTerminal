"""
Open Exchange Rates Data Fetcher
Open Exchange Rates free tier: 170+ currency rates, historical rates,
time-series, and OHLC data.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('OPENEXCHANGERATES_API_KEY', '')
BASE_URL = "https://openexchangerates.org/api"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["app_id"] = API_KEY
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


def get_latest(base: str = "USD", symbols: str = None) -> Any:
    params = {}
    if base and base != "USD":
        params["base"] = base.upper()
    if symbols:
        params["symbols"] = symbols.upper()
    return _make_request("latest.json", params)


def get_historical(date: str, base: str = "USD", symbols: str = None) -> Any:
    params = {}
    if base and base != "USD":
        params["base"] = base.upper()
    if symbols:
        params["symbols"] = symbols.upper()
    return _make_request(f"historical/{date}.json", params)


def get_currencies() -> Any:
    return _make_request("currencies.json")


def get_time_series(start: str, end: str, base: str = "USD", symbols: str = None) -> Any:
    params = {"start": start, "end": end}
    if base and base != "USD":
        params["base"] = base.upper()
    if symbols:
        params["symbols"] = symbols.upper()
    return _make_request("time-series.json", params)


def get_ohlc(start_time: str, period: str = "1m", base: str = "USD", symbols: str = None) -> Any:
    params = {"start_time": start_time, "period": period}
    if base and base != "USD":
        params["base"] = base.upper()
    if symbols:
        params["symbols"] = symbols.upper()
    return _make_request("ohlc.json", params)


def convert(from_currency: str, to_currency: str, amount: str = "1") -> Any:
    params = {"from": from_currency.upper(), "to": to_currency.upper(), "amount": amount}
    return _make_request("convert.json", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "latest":
        base = args[1] if len(args) > 1 else "USD"
        symbols = args[2] if len(args) > 2 else None
        result = get_latest(base, symbols)
    elif command == "historical":
        date = args[1] if len(args) > 1 else "2024-01-01"
        base = args[2] if len(args) > 2 else "USD"
        symbols = args[3] if len(args) > 3 else None
        result = get_historical(date, base, symbols)
    elif command == "currencies":
        result = get_currencies()
    elif command == "series":
        start = args[1] if len(args) > 1 else "2024-01-01"
        end = args[2] if len(args) > 2 else "2024-12-31"
        base = args[3] if len(args) > 3 else "USD"
        symbols = args[4] if len(args) > 4 else None
        result = get_time_series(start, end, base, symbols)
    elif command == "ohlc":
        start_time = args[1] if len(args) > 1 else "2024-01-01T00:00:00Z"
        period = args[2] if len(args) > 2 else "1m"
        base = args[3] if len(args) > 3 else "USD"
        symbols = args[4] if len(args) > 4 else None
        result = get_ohlc(start_time, period, base, symbols)
    elif command == "convert":
        from_currency = args[1] if len(args) > 1 else "USD"
        to_currency = args[2] if len(args) > 2 else "EUR"
        amount = args[3] if len(args) > 3 else "1"
        result = convert(from_currency, to_currency, amount)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
