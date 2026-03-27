"""
UNFPA Population Fund Data Fetcher
Demographic data, reproductive health, and population projections from
the United Nations Population Fund open data resources.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('UNFPA_API_KEY', '')
BASE_URL = "https://www.unfpa.org/data"

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


def get_population_data(country: str = None, year: str = None) -> Any:
    params = {"indicator": "population_total"}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    return _make_request("api/population", params)


def get_fertility_rate(country: str = None, year: str = None) -> Any:
    params = {"indicator": "tfr"}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    return _make_request("api/fertility", params)


def get_maternal_health(country: str = None, indicator: str = None, year: str = None) -> Any:
    params = {}
    if country:
        params["country"] = country
    if indicator:
        params["indicator"] = indicator
    if year:
        params["year"] = year
    return _make_request("api/maternal-health", params)


def get_youth_data(country: str = None, year: str = None) -> Any:
    params = {"age_group": "15-24"}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    return _make_request("api/youth", params)


def get_reproductive_health(country: str = None, year: str = None) -> Any:
    params = {}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    return _make_request("api/reproductive-health", params)


def get_countries() -> Any:
    return _make_request("api/countries")


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "population":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_population_data(country, year)
    elif command == "fertility":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_fertility_rate(country, year)
    elif command == "maternal":
        country = args[1] if len(args) > 1 else None
        indicator = args[2] if len(args) > 2 else None
        year = args[3] if len(args) > 3 else None
        result = get_maternal_health(country, indicator, year)
    elif command == "youth":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_youth_data(country, year)
    elif command == "reproductive":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_reproductive_health(country, year)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
