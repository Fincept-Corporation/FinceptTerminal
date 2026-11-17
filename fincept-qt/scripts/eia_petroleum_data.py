"""
EIA Petroleum Data Fetcher
EIA Petroleum data: crude oil inventory, imports/exports, refinery runs, gasoline prices by region.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc
import fincept_json

API_KEY = os.environ.get('EIA_API_KEY', '')
BASE_URL = "https://api.eia.gov/v2/petroleum"
SERVICE = "EIA"

session = fc.http_session()


def _make_request(endpoint: str, params: Dict = None) -> Any:
    # Every EIA v2 endpoint answers HTTP 403 without a key: say so before any network call.
    if not API_KEY:
        return fc.missing_key("The EIA Open Data API", "https://www.eia.gov/opendata/register.php",
                              "EIA_API_KEY", what="a free API key")
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["api_key"] = API_KEY
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, "EIA_API_KEY", API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}


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
    print(fincept_json.dumps(result))


if __name__ == "__main__":
    main()
