"""
EIA Petroleum Data Fetcher
EIA Petroleum data: crude oil inventory, imports/exports, refinery runs, gasoline prices by region.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('EIA_API_KEY', '')
BASE_URL = "https://api.eia.gov/v2/petroleum"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    if API_KEY:
        params["api_key"] = API_KEY
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


def get_crude_stocks(pad_district: str, start_period: str, end_period: str) -> Any:
    params = {"facets[paddistrict][]": pad_district, "start": start_period, "end": end_period, "frequency": "weekly"}
    return _make_request("stoc/wstk/data", params)


def get_imports(origin: str, start_period: str, end_period: str) -> Any:
    params = {"facets[originName][]": origin, "start": start_period, "end": end_period, "frequency": "monthly"}
    return _make_request("move/imp/data", params)


def get_refinery_runs(pad_district: str, start_period: str, end_period: str) -> Any:
    params = {"facets[paddistrict][]": pad_district, "start": start_period, "end": end_period, "frequency": "weekly"}
    return _make_request("pnp/wiup/data", params)


def get_gasoline_prices(grade: str, area: str, start_period: str, end_period: str) -> Any:
    params = {"facets[grade][]": grade, "facets[area][]": area, "start": start_period, "end": end_period, "frequency": "weekly"}
    return _make_request("pri/gnd/data", params)


def get_exports(destination: str, start_period: str, end_period: str) -> Any:
    params = {"facets[destinationName][]": destination, "start": start_period, "end": end_period, "frequency": "monthly"}
    return _make_request("move/exp/data", params)


def get_series(series_id: str, start: str, end: str) -> Any:
    url = f"https://api.eia.gov/v2/seriesid/{series_id}"
    params = {"start": start, "end": end}
    if API_KEY:
        params["api_key"] = API_KEY
    return _make_request(url, params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "stocks":
        pad_district = args[1] if len(args) > 1 else "1"
        start_period = args[2] if len(args) > 2 else "2024-01-01"
        end_period = args[3] if len(args) > 3 else "2024-03-31"
        result = get_crude_stocks(pad_district, start_period, end_period)
    elif command == "imports":
        origin = args[1] if len(args) > 1 else "Canada"
        start_period = args[2] if len(args) > 2 else "2024-01"
        end_period = args[3] if len(args) > 3 else "2024-03"
        result = get_imports(origin, start_period, end_period)
    elif command == "refinery":
        pad_district = args[1] if len(args) > 1 else "1"
        start_period = args[2] if len(args) > 2 else "2024-01-01"
        end_period = args[3] if len(args) > 3 else "2024-03-31"
        result = get_refinery_runs(pad_district, start_period, end_period)
    elif command == "gasoline":
        grade = args[1] if len(args) > 1 else "R"
        area = args[2] if len(args) > 2 else "U.S."
        start_period = args[3] if len(args) > 3 else "2024-01-01"
        end_period = args[4] if len(args) > 4 else "2024-03-31"
        result = get_gasoline_prices(grade, area, start_period, end_period)
    elif command == "exports":
        destination = args[1] if len(args) > 1 else "Mexico"
        start_period = args[2] if len(args) > 2 else "2024-01"
        end_period = args[3] if len(args) > 3 else "2024-03"
        result = get_exports(destination, start_period, end_period)
    elif command == "series":
        series_id = args[1] if len(args) > 1 else "PET.WCRSTUS1.W"
        start = args[2] if len(args) > 2 else "2024-01-01"
        end = args[3] if len(args) > 3 else "2024-03-31"
        result = get_series(series_id, start, end)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
