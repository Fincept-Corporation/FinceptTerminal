"""
UNDP Human Development Data Fetcher
Human Development Index (HDI), Gender Inequality Index (GII), and
Multidimensional Poverty Index (MPI) for all countries via the UNDP HDR API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('UNDP_API_KEY', '')
BASE_URL = "https://api.hdr.undp.org/v5/api"

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


def get_hdi(country: str = None, start_year: str = None, end_year: str = None) -> Any:
    params = {"indicator_id": "137906"}
    if country:
        params["country_code"] = country
    if start_year:
        params["year_from"] = start_year
    if end_year:
        params["year_to"] = end_year
    return _make_request("indicator/data", params)


def get_gii(country: str = None, start_year: str = None, end_year: str = None) -> Any:
    params = {"indicator_id": "68606"}
    if country:
        params["country_code"] = country
    if start_year:
        params["year_from"] = start_year
    if end_year:
        params["year_to"] = end_year
    return _make_request("indicator/data", params)


def get_mpi(country: str = None, year: str = None) -> Any:
    params = {"indicator_id": "137931"}
    if country:
        params["country_code"] = country
    if year:
        params["year_from"] = year
        params["year_to"] = year
    return _make_request("indicator/data", params)


def get_country_profile(country_code: str) -> Any:
    return _make_request(f"country/{country_code}")


def get_indicators() -> Any:
    return _make_request("indicator")


def get_rankings(indicator: str = None, year: str = None) -> Any:
    params = {}
    if indicator:
        params["indicator_id"] = indicator
    if year:
        params["year"] = year
    return _make_request("indicator/rankings", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "hdi":
        country = args[1] if len(args) > 1 else None
        start_year = args[2] if len(args) > 2 else None
        end_year = args[3] if len(args) > 3 else None
        result = get_hdi(country, start_year, end_year)
    elif command == "gii":
        country = args[1] if len(args) > 1 else None
        start_year = args[2] if len(args) > 2 else None
        end_year = args[3] if len(args) > 3 else None
        result = get_gii(country, start_year, end_year)
    elif command == "mpi":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_mpi(country, year)
    elif command == "profile":
        if len(args) < 2:
            result = {"error": "country_code required"}
        else:
            result = get_country_profile(args[1])
    elif command == "indicators":
        result = get_indicators()
    elif command == "rankings":
        indicator = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_rankings(indicator, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
