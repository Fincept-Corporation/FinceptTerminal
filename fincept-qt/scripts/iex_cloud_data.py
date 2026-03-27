"""
IEX Cloud Data Fetcher
US stock quotes, news, earnings, financials, economic data — free tier.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('IEX_CLOUD_API_KEY', '')
BASE_URL = "https://cloud.iexapis.com/stable"

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

def get_quote(symbol: str) -> Any:
    return _make_request(f"stock/{symbol}/quote", {"token": API_KEY})

def get_chart(symbol: str, range_: str = "1m") -> Any:
    return _make_request(f"stock/{symbol}/chart/{range_}", {"token": API_KEY})

def get_financials(symbol: str, period: str = "annual") -> Any:
    return _make_request(f"stock/{symbol}/financials", {"token": API_KEY, "period": period})

def get_earnings(symbol: str, period: int = 4) -> Any:
    return _make_request(f"stock/{symbol}/earnings/{period}", {"token": API_KEY})

def get_news(symbol: str, last: int = 10) -> Any:
    return _make_request(f"stock/{symbol}/news/last/{last}", {"token": API_KEY})

def get_economic_data(indicator: str = "US_FEDFUNDS") -> Any:
    return _make_request(f"data-points/market/{indicator}", {"token": API_KEY})

def get_batch(symbols: str, types: str = "quote,news") -> Any:
    return _make_request("stock/market/batch", {"token": API_KEY, "symbols": symbols, "types": types})

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
    elif command == "chart":
        symbol = args[1] if len(args) > 1 else "AAPL"
        range_ = args[2] if len(args) > 2 else "1m"
        result = get_chart(symbol, range_)
    elif command == "financials":
        symbol = args[1] if len(args) > 1 else "AAPL"
        period = args[2] if len(args) > 2 else "annual"
        result = get_financials(symbol, period)
    elif command == "earnings":
        symbol = args[1] if len(args) > 1 else "AAPL"
        period = int(args[2]) if len(args) > 2 else 4
        result = get_earnings(symbol, period)
    elif command == "news":
        symbol = args[1] if len(args) > 1 else "AAPL"
        last = int(args[2]) if len(args) > 2 else 10
        result = get_news(symbol, last)
    elif command == "economic":
        indicator = args[1] if len(args) > 1 else "US_FEDFUNDS"
        result = get_economic_data(indicator)
    elif command == "batch":
        symbols = args[1] if len(args) > 1 else "AAPL,MSFT"
        types = args[2] if len(args) > 2 else "quote,news"
        result = get_batch(symbols, types)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
