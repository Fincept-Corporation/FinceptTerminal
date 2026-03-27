"""
OPEC Data Fetcher
OPEC Annual Statistical Bulletin: oil production, reserves, exports, revenues by member country.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('OPEC_API_KEY', '')
BASE_URL = "https://asb.opec.org/data"

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


def get_crude_production(country: str, year: str) -> Any:
    params = {"country": country, "year": year, "indicator": "crude_production"}
    return _make_request("production", params)


def get_proven_reserves(country: str, year: str) -> Any:
    params = {"country": country, "year": year, "indicator": "proven_reserves"}
    return _make_request("reserves", params)


def get_export_revenues(country: str, year: str) -> Any:
    params = {"country": country, "year": year, "indicator": "export_revenues"}
    return _make_request("revenues", params)


def get_downstream_data(country: str, indicator: str, year: str) -> Any:
    params = {"country": country, "indicator": indicator, "year": year}
    return _make_request("downstream", params)


def get_world_oil_demand(year: str) -> Any:
    params = {"year": year, "indicator": "world_demand"}
    return _make_request("demand", params)


def get_member_countries() -> Any:
    return _make_request("members", {})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "production":
        country = args[1] if len(args) > 1 else "Saudi Arabia"
        year = args[2] if len(args) > 2 else "2023"
        result = get_crude_production(country, year)
    elif command == "reserves":
        country = args[1] if len(args) > 1 else "Saudi Arabia"
        year = args[2] if len(args) > 2 else "2023"
        result = get_proven_reserves(country, year)
    elif command == "revenues":
        country = args[1] if len(args) > 1 else "Saudi Arabia"
        year = args[2] if len(args) > 2 else "2023"
        result = get_export_revenues(country, year)
    elif command == "downstream":
        country = args[1] if len(args) > 1 else "Saudi Arabia"
        indicator = args[2] if len(args) > 2 else "refinery_capacity"
        year = args[3] if len(args) > 3 else "2023"
        result = get_downstream_data(country, indicator, year)
    elif command == "demand":
        year = args[1] if len(args) > 1 else "2023"
        result = get_world_oil_demand(year)
    elif command == "countries":
        result = get_member_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
