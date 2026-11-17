"""
SimFin Data Fetcher
Free fundamental financial data — income statements, balance sheets, cash flows for 4000+ US companies.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc

API_KEY = os.environ.get('SIMFIN_API_KEY', '')
BASE_URL = "https://backend.simfin.com/api/v3"

SERVICE = "SimFin"
KEY_ENV = "SIMFIN_API_KEY"

session = fc.http_session()

def _make_request(endpoint: str, params: Dict = None) -> Any:
    if not API_KEY:
        return fc.missing_key(SERVICE, "https://www.simfin.com/", KEY_ENV)
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        # SimFin v3 authenticates with the "Authorization: api-key <key>" header.
        headers = {"Authorization": f"api-key {API_KEY}", "Accept": "application/json"}
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, KEY_ENV, API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}

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
    try:
        main()
    except Exception as e:  # bad argument (e.g. a non-numeric limit): still answer in JSON
        print(json.dumps({"error": fc.scrub(f"{type(e).__name__}: {e}", API_KEY)}))
