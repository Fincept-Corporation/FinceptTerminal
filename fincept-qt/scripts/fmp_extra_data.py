"""
Financial Modeling Prep Extended Data Fetcher
DCF valuations, analyst estimates, insider trading, institutional holdings — extended free tier.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('FMP_API_KEY', '')
BASE_URL = "https://financialmodelingprep.com/api/v3"

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

def get_dcf_valuation(symbol: str) -> Any:
    return _make_request(f"discounted-cash-flow/{symbol}", {"apikey": API_KEY})

def get_analyst_estimates(symbol: str, period: str = "annual", limit: int = 10) -> Any:
    return _make_request(f"analyst-estimates/{symbol}", {"apikey": API_KEY, "period": period, "limit": limit})

def get_institutional_holders(symbol: str) -> Any:
    return _make_request(f"institutional-holder/{symbol}", {"apikey": API_KEY})

def get_insider_trades(symbol: str, limit: int = 50) -> Any:
    return _make_request("insider-trading", {"apikey": API_KEY, "symbol": symbol, "limit": limit})

def get_esg_scores(symbol: str) -> Any:
    return _make_request(f"esg-environmental-social-governance-data/{symbol}", {"apikey": API_KEY})

def get_sector_pe_ratio(date: str = None, exchange: str = "NYSE") -> Any:
    params = {"apikey": API_KEY, "exchange": exchange}
    if date:
        params["date"] = date
    return _make_request("sector_price_earning_ratio", params)

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "dcf":
        symbol = args[1] if len(args) > 1 else "AAPL"
        result = get_dcf_valuation(symbol)
    elif command == "estimates":
        symbol = args[1] if len(args) > 1 else "AAPL"
        period = args[2] if len(args) > 2 else "annual"
        limit = int(args[3]) if len(args) > 3 else 10
        result = get_analyst_estimates(symbol, period, limit)
    elif command == "institutional":
        symbol = args[1] if len(args) > 1 else "AAPL"
        result = get_institutional_holders(symbol)
    elif command == "insiders":
        symbol = args[1] if len(args) > 1 else "AAPL"
        limit = int(args[2]) if len(args) > 2 else 50
        result = get_insider_trades(symbol, limit)
    elif command == "esg":
        symbol = args[1] if len(args) > 1 else "AAPL"
        result = get_esg_scores(symbol)
    elif command == "sector_pe":
        date = args[1] if len(args) > 1 else None
        exchange = args[2] if len(args) > 2 else "NYSE"
        result = get_sector_pe_ratio(date, exchange)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
