"""
Alpha Vantage Extended Data Fetcher
Forex, crypto, commodities, economic indicators, sector performance.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc

API_KEY = os.environ.get('ALPHA_VANTAGE_API_KEY', '')
BASE_URL = "https://www.alphavantage.co/query"

SERVICE = "Alpha Vantage"
KEY_ENV = "ALPHA_VANTAGE_API_KEY"

session = fc.http_session()

def _make_request(endpoint: str, params: Dict = None) -> Any:
    if not API_KEY:
        return fc.missing_key(SERVICE, "https://www.alphavantage.co/support/#api-key", KEY_ENV)
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, KEY_ENV, API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}
    # Alpha Vantage answers HTTP 200 for every failure and puts the reason in the body:
    # "Error Message" (bad key / parameters), "Note" or "Information" (rate limit, premium-only).
    if isinstance(data, dict):
        for field in ("Error Message", "Note", "Information"):
            if data.get(field) and len(data) <= 2:
                return {"error": f"{SERVICE}: {fc.scrub(data[field], API_KEY)}"}
    return data

def get_forex_daily(from_symbol: str, to_symbol: str, outputsize: str = "compact") -> Any:
    params = {"function": "FX_DAILY", "from_symbol": from_symbol, "to_symbol": to_symbol, "outputsize": outputsize, "apikey": API_KEY}
    return _make_request(BASE_URL, params)

def get_crypto_daily(symbol: str, market: str = "USD") -> Any:
    params = {"function": "DIGITAL_CURRENCY_DAILY", "symbol": symbol, "market": market, "apikey": API_KEY}
    return _make_request(BASE_URL, params)

def get_commodity_daily(commodity: str = "WTI") -> Any:
    params = {"function": commodity, "interval": "monthly", "apikey": API_KEY}
    return _make_request(BASE_URL, params)

def get_sector_performance() -> Any:
    params = {"function": "SECTOR", "apikey": API_KEY}
    return _make_request(BASE_URL, params)

def get_economic_indicator(function: str = "REAL_GDP", interval: str = "annual") -> Any:
    params = {"function": function, "interval": interval, "apikey": API_KEY}
    return _make_request(BASE_URL, params)

def get_earnings_calendar(symbol: str = None, horizon: str = "3month") -> Any:
    params = {"function": "EARNINGS_CALENDAR", "horizon": horizon, "apikey": API_KEY}
    if symbol:
        params["symbol"] = symbol
    return _make_request(BASE_URL, params)

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "forex":
        from_symbol = args[1] if len(args) > 1 else "EUR"
        to_symbol = args[2] if len(args) > 2 else "USD"
        outputsize = args[3] if len(args) > 3 else "compact"
        result = get_forex_daily(from_symbol, to_symbol, outputsize)
    elif command == "crypto":
        symbol = args[1] if len(args) > 1 else "BTC"
        market = args[2] if len(args) > 2 else "USD"
        result = get_crypto_daily(symbol, market)
    elif command == "commodity":
        commodity = args[1] if len(args) > 1 else "WTI"
        result = get_commodity_daily(commodity)
    elif command == "sectors":
        result = get_sector_performance()
    elif command == "economic":
        function = args[1] if len(args) > 1 else "REAL_GDP"
        interval = args[2] if len(args) > 2 else "annual"
        result = get_economic_indicator(function, interval)
    elif command == "earnings":
        symbol = args[1] if len(args) > 1 else None
        horizon = args[2] if len(args) > 2 else "3month"
        result = get_earnings_calendar(symbol, horizon)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
