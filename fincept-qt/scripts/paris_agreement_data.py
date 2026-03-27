"""
Paris Agreement / NDC Registry Data Fetcher
National climate pledges (NDCs), GHG emissions inventories, sectoral targets,
and adaptation data via the Climate Watch API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('CLIMATE_WATCH_API_KEY', '')
BASE_URL = "https://www.climatewatchdata.org/api/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {}
        if API_KEY:
            headers["Authorization"] = f"Bearer {API_KEY}"
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_countries_ndc(country: str = None) -> Any:
    params = {}
    if country:
        params["iso_code3"] = country
    return _make_request("ndcs", params)


def get_ghg_emissions(country: str = None, gas: str = None, sector: str = None, year: str = None) -> Any:
    params = {}
    if country:
        params["regions[]"] = country
    if gas:
        params["gas"] = gas
    if sector:
        params["sector"] = sector
    if year:
        params["start_year"] = year
        params["end_year"] = year
    return _make_request("emissions", params)


def get_ndc_content(country: str) -> Any:
    params = {"iso_code3": country}
    return _make_request("ndcs/content", params)


def get_climate_targets(country: str = None) -> Any:
    params = {}
    if country:
        params["iso_code3"] = country
    return _make_request("ndcs/targets", params)


def get_adaptation_data(country: str = None) -> Any:
    params = {}
    if country:
        params["iso_code3"] = country
    return _make_request("ndcs/adaptation", params)


def get_countries() -> Any:
    return _make_request("countries")


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "ndc":
        country = args[1] if len(args) > 1 else None
        result = get_countries_ndc(country)
    elif command == "emissions":
        country = args[1] if len(args) > 1 else None
        gas = args[2] if len(args) > 2 else None
        sector = args[3] if len(args) > 3 else None
        year = args[4] if len(args) > 4 else None
        result = get_ghg_emissions(country, gas, sector, year)
    elif command == "ndc_content":
        if len(args) < 2:
            result = {"error": "country required"}
        else:
            result = get_ndc_content(args[1])
    elif command == "targets":
        country = args[1] if len(args) > 1 else None
        result = get_climate_targets(country)
    elif command == "adaptation":
        country = args[1] if len(args) > 1 else None
        result = get_adaptation_data(country)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
