"""
Investing Calendar Data Fetcher
Economic calendar, earnings, IPO, dividend, splits, bond calendars via public endpoints.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('INVESTING_API_KEY', '')
BASE_URL = "https://api.investing.com/api/financialdata/calendar"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {
            "User-Agent": "Mozilla/5.0 (compatible; FinceptTerminal/4.0)",
            "Accept": "application/json",
            "domain-id": "www",
        }
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

def _te_fallback(endpoint: str, params: Dict = None) -> Any:
    url = f"https://api.tradingeconomics.com/{endpoint}"
    try:
        headers = {"User-Agent": "Mozilla/5.0 (compatible; FinceptTerminal/4.0)", "Accept": "application/json"}
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

def get_economic_calendar(countries: List[str] = None, importance: List[int] = None, from_date: str = None, to_date: str = None) -> Any:
    params = {}
    if from_date:
        params["dateFrom"] = from_date
    if to_date:
        params["dateTo"] = to_date
    if countries:
        params["country[]"] = countries
    if importance:
        params["importance[]"] = importance
    result = _make_request("economic", params)
    if "error" in result:
        te_params = {}
        if from_date:
            te_params["d1"] = from_date
        if to_date:
            te_params["d2"] = to_date
        result = _te_fallback("calendar", te_params)
    return result

def get_earnings_calendar(from_date: str = None, to_date: str = None) -> Any:
    params = {}
    if from_date:
        params["dateFrom"] = from_date
    if to_date:
        params["dateTo"] = to_date
    result = _make_request("earnings", params)
    if "error" in result:
        result = _te_fallback("earnings", params)
    return result

def get_ipo_calendar(from_date: str = None, to_date: str = None) -> Any:
    params = {}
    if from_date:
        params["dateFrom"] = from_date
    if to_date:
        params["dateTo"] = to_date
    return _make_request("ipo", params)

def get_dividend_calendar(from_date: str = None, to_date: str = None) -> Any:
    params = {}
    if from_date:
        params["dateFrom"] = from_date
    if to_date:
        params["dateTo"] = to_date
    return _make_request("dividends", params)

def get_splits_calendar(from_date: str = None, to_date: str = None) -> Any:
    params = {}
    if from_date:
        params["dateFrom"] = from_date
    if to_date:
        params["dateTo"] = to_date
    return _make_request("splits", params)

def get_bond_calendar(from_date: str = None, to_date: str = None) -> Any:
    params = {}
    if from_date:
        params["dateFrom"] = from_date
    if to_date:
        params["dateTo"] = to_date
    return _make_request("bonds", params)

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "economic":
        countries = args[1].split(",") if len(args) > 1 else None
        from_date = args[2] if len(args) > 2 else None
        to_date = args[3] if len(args) > 3 else None
        result = get_economic_calendar(countries=countries, from_date=from_date, to_date=to_date)
    elif command == "earnings":
        from_date = args[1] if len(args) > 1 else None
        to_date = args[2] if len(args) > 2 else None
        result = get_earnings_calendar(from_date, to_date)
    elif command == "ipo":
        from_date = args[1] if len(args) > 1 else None
        to_date = args[2] if len(args) > 2 else None
        result = get_ipo_calendar(from_date, to_date)
    elif command == "dividends":
        from_date = args[1] if len(args) > 1 else None
        to_date = args[2] if len(args) > 2 else None
        result = get_dividend_calendar(from_date, to_date)
    elif command == "splits":
        from_date = args[1] if len(args) > 1 else None
        to_date = args[2] if len(args) > 2 else None
        result = get_splits_calendar(from_date, to_date)
    elif command == "bonds":
        from_date = args[1] if len(args) > 1 else None
        to_date = args[2] if len(args) > 2 else None
        result = get_bond_calendar(from_date, to_date)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
