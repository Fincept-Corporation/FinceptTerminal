"""
FAO Extended Data Fetcher
Food security, nutrition, fisheries, forestry, land use, and emissions data
beyond basic FAOSTAT via the FAO Fenix Services API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('FAO_API_KEY', '')
BASE_URL = "https://fenixservices.fao.org/faostat/api/v1"

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


def get_food_security(country: str = None, indicator: str = None, year: str = None) -> Any:
    params = {"datasource": "FS"}
    if country:
        params["area"] = country
    if indicator:
        params["element"] = indicator
    if year:
        params["year"] = year
    return _make_request("en/data/FS", params)


def get_fisheries_data(country: str = None, year: str = None) -> Any:
    params = {"datasource": "FI"}
    if country:
        params["area"] = country
    if year:
        params["year"] = year
    return _make_request("en/data/FI", params)


def get_forestry_data(country: str = None, year: str = None) -> Any:
    params = {"datasource": "FO"}
    if country:
        params["area"] = country
    if year:
        params["year"] = year
    return _make_request("en/data/FO", params)


def get_land_use(country: str = None, year: str = None) -> Any:
    params = {"datasource": "RL"}
    if country:
        params["area"] = country
    if year:
        params["year"] = year
    return _make_request("en/data/RL", params)


def get_emissions(country: str = None, sector: str = None, year: str = None) -> Any:
    params = {"datasource": "GT"}
    if country:
        params["area"] = country
    if sector:
        params["item"] = sector
    if year:
        params["year"] = year
    return _make_request("en/data/GT", params)


def get_datasets() -> Any:
    return _make_request("en/definitions/types/dataset")


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "food_security":
        country = args[1] if len(args) > 1 else None
        indicator = args[2] if len(args) > 2 else None
        year = args[3] if len(args) > 3 else None
        result = get_food_security(country, indicator, year)
    elif command == "fisheries":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_fisheries_data(country, year)
    elif command == "forestry":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_forestry_data(country, year)
    elif command == "land_use":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_land_use(country, year)
    elif command == "emissions":
        country = args[1] if len(args) > 1 else None
        sector = args[2] if len(args) > 2 else None
        year = args[3] if len(args) > 3 else None
        result = get_emissions(country, sector, year)
    elif command == "datasets":
        result = get_datasets()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
