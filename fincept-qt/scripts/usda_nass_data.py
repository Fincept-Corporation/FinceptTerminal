"""
USDA NASS Quick Stats Data Fetcher
US agricultural production, prices, acreage, and livestock data for all commodities
via the USDA National Agricultural Statistics Service Quick Stats API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc

API_KEY = os.environ.get('USDA_NASS_API_KEY', '')
BASE_URL = "https://quickstats.nass.usda.gov/api"

SERVICE = "USDA NASS Quick Stats"
KEY_ENV = "USDA_NASS_API_KEY"

session = fc.http_session()


def _make_request(endpoint: str, params: Dict = None) -> Any:
    if not API_KEY:
        return fc.missing_key(SERVICE, "https://quickstats.nass.usda.gov/api", KEY_ENV)
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=30)
        if response.status_code == 413:
            return {"error": f"{SERVICE}: the query matches more than 50,000 rows - narrow it with a year or state"}
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, KEY_ENV, API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}
    # Quick Stats reports bad parameters / no matches as {"error": ["..."]}.
    if isinstance(data, dict) and data.get("error"):
        err = data["error"]
        return {"error": f"{SERVICE}: {fc.scrub(', '.join(map(str, err)) if isinstance(err, list) else err, API_KEY)}"}
    return data


def get_data(commodity: str, year: str = None, state: str = None) -> Any:
    params = {
        "key": API_KEY,
        "commodity_desc": commodity.upper(),
        "format": "JSON",
        "statisticcat_desc": "PRODUCTION",
    }
    if year:
        params["year"] = year
    if state:
        params["state_alpha"] = state.upper()
    return _make_request("api_GET", params)


def get_crops(year: str = None, state: str = None) -> Any:
    params = {
        "key": API_KEY,
        "sector_desc": "CROPS",
        "format": "JSON",
        "statisticcat_desc": "PRODUCTION",
        "agg_level_desc": "NATIONAL" if not state else "STATE",
    }
    if year:
        params["year"] = year
    if state:
        params["state_alpha"] = state.upper()
    return _make_request("api_GET", params)


def get_livestock(year: str = None, state: str = None) -> Any:
    params = {
        "key": API_KEY,
        "sector_desc": "ANIMALS & PRODUCTS",
        "format": "JSON",
        "statisticcat_desc": "INVENTORY",
        "agg_level_desc": "NATIONAL" if not state else "STATE",
    }
    if year:
        params["year"] = year
    if state:
        params["state_alpha"] = state.upper()
    return _make_request("api_GET", params)


def get_prices(commodity: str, year: str = None) -> Any:
    params = {
        "key": API_KEY,
        "commodity_desc": commodity.upper(),
        "statisticcat_desc": "PRICE RECEIVED",
        "format": "JSON",
    }
    if year:
        params["year"] = year
    return _make_request("api_GET", params)


def search_params(param_type: str) -> Any:
    valid_types = [
        "sector_desc", "group_desc", "commodity_desc", "class_desc",
        "prodn_practice_desc", "util_practice_desc", "statisticcat_desc",
        "unit_desc", "agg_level_desc", "state_alpha", "county_name"
    ]
    if param_type not in valid_types:
        return {"error": f"Invalid param_type. Valid options: {valid_types}"}
    params = {"key": API_KEY, "param": param_type}
    return _make_request("get_param_values", params)


def get_counts(filters: Dict) -> Any:
    params = {"key": API_KEY, "format": "JSON"}
    params.update(filters)
    return _make_request("get_counts", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "data":
        commodity = args[1] if len(args) > 1 else "CORN"
        year = args[2] if len(args) > 2 else None
        state = args[3] if len(args) > 3 else None
        result = get_data(commodity, year, state)
    elif command == "crops":
        year = args[1] if len(args) > 1 else None
        state = args[2] if len(args) > 2 else None
        result = get_crops(year, state)
    elif command == "livestock":
        year = args[1] if len(args) > 1 else None
        state = args[2] if len(args) > 2 else None
        result = get_livestock(year, state)
    elif command == "prices":
        commodity = args[1] if len(args) > 1 else "CORN"
        year = args[2] if len(args) > 2 else None
        result = get_prices(commodity, year)
    elif command == "params":
        param_type = args[1] if len(args) > 1 else "commodity_desc"
        result = search_params(param_type)
    elif command == "counts":
        filters = {}
        for i in range(1, len(args) - 1, 2):
            filters[args[i]] = args[i + 1]
        result = get_counts(filters)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
