"""
CoinMarketCap Data Fetcher
CoinMarketCap Basic tier: top crypto listings, prices, market caps, categories,
and global metrics via the CoinMarketCap Pro API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('CMC_API_KEY', '')
BASE_URL = "https://pro-api.coinmarketcap.com/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    headers = {"X-CMC_PRO_API_KEY": API_KEY, "Accept": "application/json"}
    try:
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_listings_latest(limit: str = "100", convert: str = "USD") -> Any:
    params = {
        "start": "1",
        "limit": limit,
        "convert": convert.upper(),
        "sort": "market_cap",
        "sort_dir": "desc",
    }
    return _make_request("cryptocurrency/listings/latest", params)


def get_quotes_latest(symbols: str, convert: str = "USD") -> Any:
    params = {"symbol": symbols.upper(), "convert": convert.upper()}
    return _make_request("cryptocurrency/quotes/latest", params)


def get_global_metrics(convert: str = "USD") -> Any:
    params = {"convert": convert.upper()}
    return _make_request("global-metrics/quotes/latest", params)


def get_categories(limit: str = "100") -> Any:
    params = {"start": "1", "limit": limit}
    return _make_request("cryptocurrency/categories", params)


def get_ohlcv_historical(symbol: str, time_start: str = None, time_end: str = None, convert: str = "USD") -> Any:
    params = {"symbol": symbol.upper(), "convert": convert.upper()}
    if time_start:
        params["time_start"] = time_start
    if time_end:
        params["time_end"] = time_end
    return _make_request("cryptocurrency/ohlcv/historical", params)


def get_trending(limit: str = "10") -> Any:
    params = {"start": "1", "limit": limit}
    return _make_request("cryptocurrency/trending/latest", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "listings":
        limit = args[1] if len(args) > 1 else "100"
        convert = args[2] if len(args) > 2 else "USD"
        result = get_listings_latest(limit, convert)
    elif command == "quotes":
        symbols = args[1] if len(args) > 1 else "BTC,ETH"
        convert = args[2] if len(args) > 2 else "USD"
        result = get_quotes_latest(symbols, convert)
    elif command == "global":
        convert = args[1] if len(args) > 1 else "USD"
        result = get_global_metrics(convert)
    elif command == "categories":
        limit = args[1] if len(args) > 1 else "100"
        result = get_categories(limit)
    elif command == "ohlcv":
        symbol = args[1] if len(args) > 1 else "BTC"
        time_start = args[2] if len(args) > 2 else None
        time_end = args[3] if len(args) > 3 else None
        convert = args[4] if len(args) > 4 else "USD"
        result = get_ohlcv_historical(symbol, time_start, time_end, convert)
    elif command == "trending":
        limit = args[1] if len(args) > 1 else "10"
        result = get_trending(limit)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
