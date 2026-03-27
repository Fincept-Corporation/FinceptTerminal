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
BASE_URL = "https://open-api.coinglass.com/public/v2"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {"coinglassSecret": API_KEY}
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

def get_open_interest(symbol: str = "BTC", exchange: str = None) -> Any:
    params = {"symbol": symbol}
    if exchange:
        params["exchange"] = exchange
    return _make_request("openInterest", params)

def get_funding_rate(symbol: str = "BTC", exchange: str = None) -> Any:
    params = {"symbol": symbol}
    if exchange:
        params["exchange"] = exchange
    return _make_request("funding", params)

def get_liquidations(symbol: str = "BTC", exchange: str = None, time_range: str = "h4") -> Any:
    params = {"symbol": symbol, "timeType": time_range}
    if exchange:
        params["exchange"] = exchange
    return _make_request("liquidation/detail/chart", params)

def get_long_short_ratio(symbol: str = "BTC", exchange: str = "Binance", period: str = "h4") -> Any:
    params = {"symbol": symbol, "exchangeName": exchange, "period": period}
    return _make_request("longShort", params)

def get_aggregated_oi(symbol: str = "BTC") -> Any:
    return _make_request("openInterest/aggregated-history", {"symbol": symbol})

def get_futures_markets() -> Any:
    return _make_request("exchange/info", {})

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
