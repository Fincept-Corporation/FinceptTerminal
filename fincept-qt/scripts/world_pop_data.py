"""
WorldPop Data Fetcher
High-resolution population grids, demographic estimates, and urbanization data
from the University of Southampton WorldPop project REST API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WORLDPOP_API_KEY', '')
BASE_URL = "https://www.worldpop.org/rest/data"

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


def get_population_grid(country: str = None, year: str = None, resolution: str = "100m") -> Any:
    params = {"popyear": year or "2020", "resolution": resolution}
    if country:
        params["iso3"] = country
    return _make_request("pop", params)


def get_urban_population(country: str = None, year: str = None) -> Any:
    params = {}
    if country:
        params["iso3"] = country
    if year:
        params["popyear"] = year
    return _make_request("urb", params)


def get_demographic_estimates(country: str = None, year: str = None) -> Any:
    params = {}
    if country:
        params["iso3"] = country
    if year:
        params["popyear"] = year
    return _make_request("age_structures", params)


def get_available_datasets(country: str = None) -> Any:
    params = {}
    if country:
        params["iso3"] = country
    return _make_request("pop", params)


def get_file_metadata(file_id: str) -> Any:
    return _make_request(f"pop/{file_id}")


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
    if command == "grid":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        resolution = args[3] if len(args) > 3 else "100m"
        result = get_population_grid(country, year, resolution)
    elif command == "urban":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_urban_population(country, year)
    elif command == "demographic":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_demographic_estimates(country, year)
    elif command == "datasets":
        country = args[1] if len(args) > 1 else None
        result = get_available_datasets(country)
    elif command == "metadata":
        if len(args) < 2:
            result = {"error": "file_id required"}
        else:
            result = get_file_metadata(args[1])
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
