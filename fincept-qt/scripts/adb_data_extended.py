"""
Asian Development Bank (ADB) Extended Data Fetcher
Additional indicators covering poverty, gender, climate, and infrastructure
for Asia-Pacific countries via the ADB Key Indicators Database (KIDB).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('ADB_API_KEY', '')
BASE_URL = "https://kidb.adb.org/api"

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


def get_gdp_data(country: str = None, year: str = None) -> Any:
    params = {"indicator": "GDP", "category": "economy"}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    return _make_request("indicators/gdp", params)


def get_poverty_data(country: str = None, year: str = None) -> Any:
    params = {"category": "poverty"}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    return _make_request("indicators/poverty", params)


def get_gender_data(country: str = None, indicator: str = None, year: str = None) -> Any:
    params = {"category": "gender"}
    if country:
        params["country"] = country
    if indicator:
        params["indicator"] = indicator
    if year:
        params["year"] = year
    return _make_request("indicators/gender", params)


def get_climate_data(country: str = None, indicator: str = None, year: str = None) -> Any:
    params = {"category": "climate"}
    if country:
        params["country"] = country
    if indicator:
        params["indicator"] = indicator
    if year:
        params["year"] = year
    return _make_request("indicators/climate", params)


def get_infrastructure_data(country: str = None, sector: str = None, year: str = None) -> Any:
    params = {"category": "infrastructure"}
    if country:
        params["country"] = country
    if sector:
        params["sector"] = sector
    if year:
        params["year"] = year
    return _make_request("indicators/infrastructure", params)


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
    if command == "gdp":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_gdp_data(country, year)
    elif command == "poverty":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_poverty_data(country, year)
    elif command == "gender":
        country = args[1] if len(args) > 1 else None
        indicator = args[2] if len(args) > 2 else None
        year = args[3] if len(args) > 3 else None
        result = get_gender_data(country, indicator, year)
    elif command == "climate":
        country = args[1] if len(args) > 1 else None
        indicator = args[2] if len(args) > 2 else None
        year = args[3] if len(args) > 3 else None
        result = get_climate_data(country, indicator, year)
    elif command == "infrastructure":
        country = args[1] if len(args) > 1 else None
        sector = args[2] if len(args) > 2 else None
        year = args[3] if len(args) > 3 else None
        result = get_infrastructure_data(country, sector, year)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
