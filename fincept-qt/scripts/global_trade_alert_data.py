"""
Global Trade Alert Data Fetcher
Global Trade Alert: protectionist and liberalizing measures, trade policy interventions worldwide.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://www.globaltradealert.org/api"

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


def get_jurisdictions() -> Any:
    return _make_request("jurisdictions")


def get_interventions(country: str = None, year: int = None, type_: str = None, limit: int = 50) -> Any:
    params = {"limit": limit}
    if country:
        params["jurisdiction"] = country
    if year:
        params["year"] = year
    if type_:
        params["intervention_type"] = type_
    return _make_request("interventions", params=params)


def get_intervention_details(id_: int) -> Any:
    return _make_request(f"interventions/{id_}")


def get_affected_sectors(country: str = None, year: int = None) -> Any:
    params = {}
    if country:
        params["jurisdiction"] = country
    if year:
        params["year"] = year
    return _make_request("sectors", params=params)


def get_implementing_countries(year: int = None, measure_type: str = None) -> Any:
    params = {}
    if year:
        params["year"] = year
    if measure_type:
        params["measure_type"] = measure_type
    return _make_request("implementing_jurisdictions", params=params)


def get_statistics(country: str = None, year: int = None) -> Any:
    params = {}
    if country:
        params["jurisdiction"] = country
    if year:
        params["year"] = year
    return _make_request("statistics", params=params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "jurisdictions":
        result = get_jurisdictions()
    elif command == "interventions":
        country = args[1] if len(args) > 1 else None
        year = int(args[2]) if len(args) > 2 else None
        type_ = args[3] if len(args) > 3 else None
        limit = int(args[4]) if len(args) > 4 else 50
        result = get_interventions(country, year, type_, limit)
    elif command == "details":
        id_ = int(args[1]) if len(args) > 1 else 1
        result = get_intervention_details(id_)
    elif command == "sectors":
        country = args[1] if len(args) > 1 else None
        year = int(args[2]) if len(args) > 2 else None
        result = get_affected_sectors(country, year)
    elif command == "countries":
        year = int(args[1]) if len(args) > 1 else None
        measure_type = args[2] if len(args) > 2 else None
        result = get_implementing_countries(year, measure_type)
    elif command == "statistics":
        country = args[1] if len(args) > 1 else None
        year = int(args[2]) if len(args) > 2 else None
        result = get_statistics(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
