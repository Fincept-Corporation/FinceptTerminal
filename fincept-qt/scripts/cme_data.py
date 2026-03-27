"""
CME Data Fetcher
CME Group public market data: futures prices, volumes, open interest, settlement prices.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('CME_API_KEY', '')
BASE_URL = "https://www.cmegroup.com/api/v1"

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


def get_settlement_prices(product_code: str, date: str) -> Any:
    params = {"productCode": product_code, "date": date}
    return _make_request("settlements/futures", params)


def get_volume(product_code: str, date: str) -> Any:
    params = {"productCode": product_code, "date": date}
    return _make_request("volume/futures", params)


def get_open_interest(product_code: str, date: str) -> Any:
    params = {"productCode": product_code, "date": date}
    return _make_request("openinterest/futures", params)


def get_delayed_quotes(product_code: str) -> Any:
    params = {"productCode": product_code}
    return _make_request("quotes/delayed", params)


def get_product_info(product_code: str) -> Any:
    params = {"productCode": product_code}
    return _make_request("products/futures", params)


def get_calendar(product_code: str, year: str) -> Any:
    params = {"productCode": product_code, "year": year}
    return _make_request("calendar/futures", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "settlement":
        product_code = args[1] if len(args) > 1 else "ES"
        date = args[2] if len(args) > 2 else "2024-01-15"
        result = get_settlement_prices(product_code, date)
    elif command == "volume":
        product_code = args[1] if len(args) > 1 else "ES"
        date = args[2] if len(args) > 2 else "2024-01-15"
        result = get_volume(product_code, date)
    elif command == "open_interest":
        product_code = args[1] if len(args) > 1 else "ES"
        date = args[2] if len(args) > 2 else "2024-01-15"
        result = get_open_interest(product_code, date)
    elif command == "quotes":
        product_code = args[1] if len(args) > 1 else "ES"
        result = get_delayed_quotes(product_code)
    elif command == "product":
        product_code = args[1] if len(args) > 1 else "ES"
        result = get_product_info(product_code)
    elif command == "calendar":
        product_code = args[1] if len(args) > 1 else "ES"
        year = args[2] if len(args) > 2 else "2024"
        result = get_calendar(product_code, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
