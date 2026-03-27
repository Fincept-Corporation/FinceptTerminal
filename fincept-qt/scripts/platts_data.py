"""
Platts Data Fetcher
S&P Global Commodity Insights free tier / public commodity price indices.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('PLATTS_API_KEY', '')
BASE_URL = "https://api.platts.com/tradedata/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    headers = {}
    if API_KEY:
        headers["appkey"] = API_KEY
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


def get_assessment(symbol: str, from_date: str, to_date: str) -> Any:
    params = {"symbol": symbol, "fromDate": from_date, "toDate": to_date}
    return _make_request("assessments/history", params)


def get_latest_prices(symbol_list: List[str]) -> Any:
    params = {"symbol": ",".join(symbol_list)}
    return _make_request("assessments/latest", params)


def get_symbols(commodity_type: str) -> Any:
    params = {"commodity": commodity_type}
    return _make_request("symbols", params)


def get_market_data(market: str, from_date: str, to_date: str) -> Any:
    params = {"market": market, "fromDate": from_date, "toDate": to_date}
    return _make_request("market-data", params)


def get_news_feed(commodity: str) -> Any:
    params = {"commodity": commodity}
    return _make_request("news", params)


def get_available_datasets() -> Any:
    return _make_request("datasets", {})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "assessment":
        symbol = args[1] if len(args) > 1 else "PCAAS00"
        from_date = args[2] if len(args) > 2 else "2024-01-01"
        to_date = args[3] if len(args) > 3 else "2024-01-31"
        result = get_assessment(symbol, from_date, to_date)
    elif command == "latest":
        symbols = args[1].split(",") if len(args) > 1 else ["PCAAS00"]
        result = get_latest_prices(symbols)
    elif command == "symbols":
        commodity_type = args[1] if len(args) > 1 else "crude"
        result = get_symbols(commodity_type)
    elif command == "market":
        market = args[1] if len(args) > 1 else "crude"
        from_date = args[2] if len(args) > 2 else "2024-01-01"
        to_date = args[3] if len(args) > 3 else "2024-01-31"
        result = get_market_data(market, from_date, to_date)
    elif command == "news":
        commodity = args[1] if len(args) > 1 else "crude"
        result = get_news_feed(commodity)
    elif command == "datasets":
        result = get_available_datasets()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
