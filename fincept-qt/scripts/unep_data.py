"""
UNEP Environmental Data Fetcher
Environmental indicators covering forests, freshwater, biodiversity, marine,
and land use via the UNEP Environmental Live data portal.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('UNEP_API_KEY', '')
BASE_URL = "https://environmentlive.unep.org/api"

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


def get_forest_data(country: str = None, year: str = None) -> Any:
    params = {"theme": "forests"}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    return _make_request("indicators/forests", params)


def get_freshwater_data(country: str = None, indicator: str = None, year: str = None) -> Any:
    params = {"theme": "freshwater"}
    if country:
        params["country"] = country
    if indicator:
        params["indicator"] = indicator
    if year:
        params["year"] = year
    return _make_request("indicators/freshwater", params)


def get_biodiversity_data(country: str = None, year: str = None) -> Any:
    params = {"theme": "biodiversity"}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    return _make_request("indicators/biodiversity", params)


def get_marine_data(country: str = None, year: str = None) -> Any:
    params = {"theme": "marine"}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    return _make_request("indicators/marine", params)


def get_land_use_data(country: str = None, year: str = None) -> Any:
    params = {"theme": "land"}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    return _make_request("indicators/land-use", params)


def get_indicators() -> Any:
    return _make_request("indicators")


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "forests":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_forest_data(country, year)
    elif command == "freshwater":
        country = args[1] if len(args) > 1 else None
        indicator = args[2] if len(args) > 2 else None
        year = args[3] if len(args) > 3 else None
        result = get_freshwater_data(country, indicator, year)
    elif command == "biodiversity":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_biodiversity_data(country, year)
    elif command == "marine":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_marine_data(country, year)
    elif command == "land_use":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_land_use_data(country, year)
    elif command == "indicators":
        result = get_indicators()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
