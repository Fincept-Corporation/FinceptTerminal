"""
AlphaSpread Data Fetcher
Intrinsic value, DCF, comparable company analysis for stocks — free tier.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('ALPHA_SPREAD_API_KEY', '')
BASE_URL = "https://alphaspread.com/api"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {"Authorization": f"Bearer {API_KEY}"}
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

def get_valuation(ticker: str) -> Any:
    return _make_request(f"v1/equity/{ticker}/valuation", {})

def get_dcf(ticker: str) -> Any:
    return _make_request(f"v1/equity/{ticker}/dcf", {})

def get_comparables(ticker: str) -> Any:
    return _make_request(f"v1/equity/{ticker}/comparables", {})

def get_watchlist_data(tickers: List[str]) -> Any:
    return _make_request("v1/equity/batch", {"tickers": ",".join(tickers)})

def get_score(ticker: str) -> Any:
    return _make_request(f"v1/equity/{ticker}/score", {})

def get_intrinsic_value_history(ticker: str) -> Any:
    return _make_request(f"v1/equity/{ticker}/intrinsic-value/history", {})

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "valuation":
        ticker = args[1] if len(args) > 1 else "AAPL"
        result = get_valuation(ticker)
    elif command == "dcf":
        ticker = args[1] if len(args) > 1 else "AAPL"
        result = get_dcf(ticker)
    elif command == "comparables":
        ticker = args[1] if len(args) > 1 else "AAPL"
        result = get_comparables(ticker)
    elif command == "watchlist":
        tickers = args[1].split(",") if len(args) > 1 else ["AAPL", "MSFT"]
        result = get_watchlist_data(tickers)
    elif command == "score":
        ticker = args[1] if len(args) > 1 else "AAPL"
        result = get_score(ticker)
    elif command == "history":
        ticker = args[1] if len(args) > 1 else "AAPL"
        result = get_intrinsic_value_history(ticker)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
