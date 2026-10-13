"""
CoinGlass Data Fetcher
Crypto futures open interest, funding rates, liquidations, long/short ratios across 30+ exchanges.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('COINGLASS_API_KEY', '')
# The public v2 API was retired (HTTP 500); v4 needs a CoinGlass API key in the CG-API-KEY header.
BASE_URL = "https://open-api-v4.coinglass.com/api/futures"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _interval(v: str) -> str:
    """Accept the old v2 style (h4, d1, m15) as well as v4 style (4h, 1d, 15m)."""
    v = (v or "4h").lower()
    if len(v) > 1 and v[0] in "mhdw" and v[1:].isdigit():
        return v[1:] + v[0]
    return v


def _make_request(endpoint: str, params: Dict = None) -> Any:
    if not API_KEY:
        return {"error": "CoinGlass API v4 requires an API key: get one at https://www.coinglass.com/pricing "
                         "and save it as COINGLASS_API_KEY in Settings > Credentials"}
    url = f"{BASE_URL}/{endpoint}"
    try:
        response = session.get(url, params=params, headers={"CG-API-KEY": API_KEY, "accept": "application/json"},
                               timeout=30)
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}
    if isinstance(data, dict) and str(data.get("code", "0")) != "0":
        return {"error": f"CoinGlass: {data.get('msg') or data.get('code')}"}
    return data.get("data", data) if isinstance(data, dict) else data


def _filter_exchange(rows: Any, exchange: str = None) -> Any:
    if exchange and isinstance(rows, list):
        return [r for r in rows if str(r.get("exchange", "")).lower() == exchange.lower()] or rows
    return rows


def get_open_interest(symbol: str = "BTC", exchange: str = None) -> Any:
    return _filter_exchange(_make_request("open-interest/exchange-list", {"symbol": symbol.upper()}), exchange)


def get_funding_rate(symbol: str = "BTC", exchange: str = None) -> Any:
    return _filter_exchange(_make_request("funding-rate/exchange-list", {"symbol": symbol.upper()}), exchange)


def get_liquidations(symbol: str = "BTC", exchange: str = None, time_range: str = "h4") -> Any:
    rng = _interval(time_range)
    return _filter_exchange(_make_request("liquidation/exchange-list", {"symbol": symbol.upper(), "range": rng}),
                            exchange)


def get_long_short_ratio(symbol: str = "BTC", exchange: str = "Binance", period: str = "h4") -> Any:
    pair = symbol.upper() if symbol.upper().endswith(("USDT", "USD", "USDC")) else symbol.upper() + "USDT"
    return _make_request("global-long-short-account-ratio/history",
                         {"exchange": exchange, "symbol": pair, "interval": _interval(period), "limit": 100})


def get_aggregated_oi(symbol: str = "BTC") -> Any:
    return _make_request("open-interest/aggregated-history", {"symbol": symbol.upper(), "interval": "1d", "limit": 100})


def get_futures_markets() -> Any:
    return _make_request("supported-exchange-pairs", {})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "open_interest":
        symbol = args[1] if len(args) > 1 else "BTC"
        exchange = args[2] if len(args) > 2 else None
        result = get_open_interest(symbol, exchange)
    elif command == "funding":
        symbol = args[1] if len(args) > 1 else "BTC"
        exchange = args[2] if len(args) > 2 else None
        result = get_funding_rate(symbol, exchange)
    elif command == "liquidations":
        symbol = args[1] if len(args) > 1 else "BTC"
        exchange = args[2] if len(args) > 2 else None
        time_range = args[3] if len(args) > 3 else "h4"
        result = get_liquidations(symbol, exchange, time_range)
    elif command == "long_short":
        symbol = args[1] if len(args) > 1 else "BTC"
        exchange = args[2] if len(args) > 2 else "Binance"
        period = args[3] if len(args) > 3 else "h4"
        result = get_long_short_ratio(symbol, exchange, period)
    elif command == "aggregated_oi":
        symbol = args[1] if len(args) > 1 else "BTC"
        result = get_aggregated_oi(symbol)
    elif command == "markets":
        result = get_futures_markets()
    print(json.dumps(result))

if __name__ == "__main__":
    main()
