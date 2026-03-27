"""
African Development Bank (AfDB) Data Fetcher
African economic data, projects, infrastructure, and development indicators
from the AfDB Open Data for Africa portal.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('AFDB_API_KEY', '')
BASE_URL = "https://dataportal.opendataforafrica.org/api/1.0"

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


def get_datasets() -> Any:
    return _make_request("datasets")


def get_indicator_data(dataset_id: str = None, country: str = None, indicator: str = None, year: str = None) -> Any:
    params = {}
    if dataset_id:
        params["dataset"] = dataset_id
    if country:
        params["country"] = country
    if indicator:
        params["indicator"] = indicator
    if year:
        params["year"] = year
    return _make_request("data", params)


def get_countries() -> Any:
    return _make_request("countries")


def get_projects(country: str = None, sector: str = None, status: str = None) -> Any:
    params = {}
    if country:
        params["country"] = country
    if sector:
        params["sector"] = sector
    if status:
        params["status"] = status
    return _make_request("projects", params)


def get_economic_indicators(country: str = None, year: str = None) -> Any:
    params = {"category": "economic"}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    return _make_request("indicators", params)


def get_infrastructure_data(country: str = None, sector: str = None) -> Any:
    params = {"category": "infrastructure"}
    if country:
        params["country"] = country
    if sector:
        params["sector"] = sector
    return _make_request("infrastructure", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "datasets":
        result = get_datasets()
    elif command == "data":
        dataset_id = args[1] if len(args) > 1 else None
        country = args[2] if len(args) > 2 else None
        indicator = args[3] if len(args) > 3 else None
        year = args[4] if len(args) > 4 else None
        result = get_indicator_data(dataset_id, country, indicator, year)
    elif command == "countries":
        result = get_countries()
    elif command == "projects":
        country = args[1] if len(args) > 1 else None
        sector = args[2] if len(args) > 2 else None
        status = args[3] if len(args) > 3 else None
        result = get_projects(country, sector, status)
    elif command == "economic":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_economic_indicators(country, year)
    elif command == "infrastructure":
        country = args[1] if len(args) > 1 else None
        sector = args[2] if len(args) > 2 else None
        result = get_infrastructure_data(country, sector)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
