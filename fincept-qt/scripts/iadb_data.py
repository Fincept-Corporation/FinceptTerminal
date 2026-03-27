"""
Inter-American Development Bank (IADB) Data Fetcher
Latin America and Caribbean development data, projects, and economic indicators
from the IDB/IADB open data portal.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('IADB_API_KEY', '')
BASE_URL = "https://api.iadb.org/api"

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


def get_projects(country: str = None, sector: str = None, status: str = None, limit: int = 50) -> Any:
    params = {"rows": limit}
    if country:
        params["country"] = country
    if sector:
        params["sector"] = sector
    if status:
        params["status"] = status
    return _make_request("projects", params)


def get_project_details(project_id: str) -> Any:
    return _make_request(f"projects/{project_id}")


def get_indicators(country: str = None, indicator: str = None, year: str = None) -> Any:
    params = {}
    if country:
        params["country"] = country
    if indicator:
        params["indicator"] = indicator
    if year:
        params["year"] = year
    return _make_request("indicators", params)


def get_disbursements(country: str = None, year: str = None) -> Any:
    params = {}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    return _make_request("disbursements", params)


def get_sectors() -> Any:
    return _make_request("sectors")


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
    if command == "projects":
        country = args[1] if len(args) > 1 else None
        sector = args[2] if len(args) > 2 else None
        status = args[3] if len(args) > 3 else None
        limit = int(args[4]) if len(args) > 4 else 50
        result = get_projects(country, sector, status, limit)
    elif command == "project":
        if len(args) < 2:
            result = {"error": "project_id required"}
        else:
            result = get_project_details(args[1])
    elif command == "indicators":
        country = args[1] if len(args) > 1 else None
        indicator = args[2] if len(args) > 2 else None
        year = args[3] if len(args) > 3 else None
        result = get_indicators(country, indicator, year)
    elif command == "disbursements":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_disbursements(country, year)
    elif command == "sectors":
        result = get_sectors()
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
