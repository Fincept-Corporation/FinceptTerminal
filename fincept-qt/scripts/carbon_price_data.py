"""
Carbon Price Data Fetcher
Carbon pricing data: EU ETS prices, California CCA, RGGI allowances, global carbon markets.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('ICE_API_KEY', '')
BASE_URL = "https://www.theice.com/marketdata/reports"

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


def get_eu_ets_price(start_date: str, end_date: str) -> Any:
    params = {"market": "EU_ETS", "product": "EUA", "startDate": start_date, "endDate": end_date}
    return _make_request("carbon/eu_ets", params)


def get_california_cca(start_date: str, end_date: str) -> Any:
    params = {"market": "CALIFORNIA_CCA", "startDate": start_date, "endDate": end_date}
    return _make_request("carbon/california", params)


def get_rggi_price(start_date: str, end_date: str) -> Any:
    params = {"market": "RGGI", "startDate": start_date, "endDate": end_date}
    return _make_request("carbon/rggi", params)


def get_uk_ets_price(start_date: str, end_date: str) -> Any:
    params = {"market": "UK_ETS", "product": "UKA", "startDate": start_date, "endDate": end_date}
    return _make_request("carbon/uk_ets", params)


def get_voluntary_market_prices(standard: str, start_date: str, end_date: str) -> Any:
    params = {"standard": standard, "startDate": start_date, "endDate": end_date}
    return _make_request("carbon/voluntary", params)


def get_carbon_futures(contract: str, start_date: str, end_date: str) -> Any:
    params = {"contract": contract, "startDate": start_date, "endDate": end_date}
    return _make_request("carbon/futures", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "eu_ets":
        start_date = args[1] if len(args) > 1 else "2024-01-01"
        end_date = args[2] if len(args) > 2 else "2024-03-31"
        result = get_eu_ets_price(start_date, end_date)
    elif command == "california":
        start_date = args[1] if len(args) > 1 else "2024-01-01"
        end_date = args[2] if len(args) > 2 else "2024-03-31"
        result = get_california_cca(start_date, end_date)
    elif command == "rggi":
        start_date = args[1] if len(args) > 1 else "2024-01-01"
        end_date = args[2] if len(args) > 2 else "2024-03-31"
        result = get_rggi_price(start_date, end_date)
    elif command == "uk_ets":
        start_date = args[1] if len(args) > 1 else "2024-01-01"
        end_date = args[2] if len(args) > 2 else "2024-03-31"
        result = get_uk_ets_price(start_date, end_date)
    elif command == "voluntary":
        standard = args[1] if len(args) > 1 else "VCS"
        start_date = args[2] if len(args) > 2 else "2024-01-01"
        end_date = args[3] if len(args) > 3 else "2024-03-31"
        result = get_voluntary_market_prices(standard, start_date, end_date)
    elif command == "futures":
        contract = args[1] if len(args) > 1 else "EUA"
        start_date = args[2] if len(args) > 2 else "2024-01-01"
        end_date = args[3] if len(args) > 3 else "2024-03-31"
        result = get_carbon_futures(contract, start_date, end_date)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
