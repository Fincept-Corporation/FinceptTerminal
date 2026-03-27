"""
SimFin Data Fetcher
Free fundamental financial data — income statements, balance sheets, cash flows for 4000+ US companies.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('SIMFIN_API_KEY', '')
BASE_URL = "https://backend.simfin.com/api/v3"

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

def get_income_statements(ticker: str, period: str = "annual", fyear: int = None) -> Any:
    params = {"ticker": ticker, "period": period, "api-key": API_KEY}
    if fyear:
        params["fyear"] = fyear
    return _make_request("companies/statements/pl", params)

def get_balance_sheets(ticker: str, period: str = "annual", fyear: int = None) -> Any:
    params = {"ticker": ticker, "period": period, "api-key": API_KEY}
    if fyear:
        params["fyear"] = fyear
    return _make_request("companies/statements/bs", params)

def get_cash_flows(ticker: str, period: str = "annual", fyear: int = None) -> Any:
    params = {"ticker": ticker, "period": period, "api-key": API_KEY}
    if fyear:
        params["fyear"] = fyear
    return _make_request("companies/statements/cf", params)

def get_company_list(market: str = "us") -> Any:
    return _make_request("companies/list", {"market": market, "api-key": API_KEY})

def get_prices(ticker: str, start_date: str = None, end_date: str = None) -> Any:
    params = {"ticker": ticker, "api-key": API_KEY}
    if start_date:
        params["start"] = start_date
    if end_date:
        params["end"] = end_date
    return _make_request("companies/prices/compact", params)

def get_ratios(ticker: str, period: str = "annual", fyear: int = None) -> Any:
    params = {"ticker": ticker, "period": period, "api-key": API_KEY}
    if fyear:
        params["fyear"] = fyear
    return _make_request("companies/statements/derived", params)

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "income":
        ticker = args[1] if len(args) > 1 else "AAPL"
        period = args[2] if len(args) > 2 else "annual"
        fyear = int(args[3]) if len(args) > 3 else None
        result = get_income_statements(ticker, period, fyear)
    elif command == "balance":
        ticker = args[1] if len(args) > 1 else "AAPL"
        period = args[2] if len(args) > 2 else "annual"
        fyear = int(args[3]) if len(args) > 3 else None
        result = get_balance_sheets(ticker, period, fyear)
    elif command == "cashflow":
        ticker = args[1] if len(args) > 1 else "AAPL"
        period = args[2] if len(args) > 2 else "annual"
        fyear = int(args[3]) if len(args) > 3 else None
        result = get_cash_flows(ticker, period, fyear)
    elif command == "companies":
        market = args[1] if len(args) > 1 else "us"
        result = get_company_list(market)
    elif command == "prices":
        ticker = args[1] if len(args) > 1 else "AAPL"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_prices(ticker, start_date, end_date)
    elif command == "ratios":
        ticker = args[1] if len(args) > 1 else "AAPL"
        period = args[2] if len(args) > 2 else "annual"
        fyear = int(args[3]) if len(args) > 3 else None
        result = get_ratios(ticker, period, fyear)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
