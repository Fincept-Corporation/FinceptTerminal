"""
USDA FAS PSD Data Fetcher
Global agricultural supply and demand data — production, trade, and stocks
by commodity and country via the USDA Foreign Agricultural Service PSD Online API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('USDA_FAS_API_KEY', '')
BASE_URL = "https://apps.fas.usda.gov/psdonline/api/v1.0"

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


def get_commodities() -> Any:
    return _make_request("commodity")


def get_countries() -> Any:
    return _make_request("country")


def get_psd_data(commodity_code: str, country_code: str, year: str = None) -> Any:
    params = {
        "commodityCode": commodity_code,
        "countryCode": country_code,
    }
    if year:
        params["marketYear"] = year
    return _make_request("data", params)


def get_commodity_data(commodity_code: str, year: str = None) -> Any:
    params = {"commodityCode": commodity_code}
    if year:
        params["marketYear"] = year
    return _make_request("data", params)


def get_export_data(commodity_code: str, country_code: str) -> Any:
    params = {
        "commodityCode": commodity_code,
        "countryCode": country_code,
        "attributeId": "88",
    }
    return _make_request("data", params)


def get_regions() -> Any:
    return _make_request("region")


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "commodities":
        result = get_commodities()
    elif command == "countries":
        result = get_countries()
    elif command == "psd":
        commodity_code = args[1] if len(args) > 1 else "0440000"
        country_code = args[2] if len(args) > 2 else "US"
        year = args[3] if len(args) > 3 else None
        result = get_psd_data(commodity_code, country_code, year)
    elif command == "commodity":
        commodity_code = args[1] if len(args) > 1 else "0440000"
        year = args[2] if len(args) > 2 else None
        result = get_commodity_data(commodity_code, year)
    elif command == "exports":
        commodity_code = args[1] if len(args) > 1 else "0440000"
        country_code = args[2] if len(args) > 2 else "US"
        result = get_export_data(commodity_code, country_code)
    elif command == "regions":
        result = get_regions()

    print(json.dumps(result))


if __name__ == "__main__":
    main()
