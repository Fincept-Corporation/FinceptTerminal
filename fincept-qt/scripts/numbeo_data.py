"""
Numbeo Data Fetcher
Numbeo: Cost of living, quality of life, crime, healthcare,
property prices by city/country (free key required via NUMBEO_API_KEY).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('NUMBEO_API_KEY', '')
BASE_URL = "https://www.numbeo.com/api"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    if API_KEY:
        params["api_key"] = API_KEY
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


def get_cost_of_living(city: str = "New York, NY", currency: str = "USD") -> Any:
    params = {"query": city, "currency": currency}
    return _make_request("city_prices", params)


def get_quality_of_life(city: str = "New York, NY") -> Any:
    params = {"query": city}
    return _make_request("quality_of_life", params)


def get_crime_index(city: str = "New York, NY") -> Any:
    params = {"query": city}
    return _make_request("crime", params)


def get_healthcare_index(city: str = "New York, NY") -> Any:
    params = {"query": city}
    return _make_request("healthcare", params)


def get_property_prices(city: str = "New York, NY") -> Any:
    params = {"query": city}
    return _make_request("property_prices_by_city", params)


def get_historical_cpi(city: str = "New York, NY", year: int = 2023) -> Any:
    params = {"query": city, "year": year}
    return _make_request("historical_currency", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "cost_of_living":
        city = args[1] if len(args) > 1 else "New York, NY"
        currency = args[2] if len(args) > 2 else "USD"
        result = get_cost_of_living(city, currency)
    elif command == "quality_of_life":
        city = args[1] if len(args) > 1 else "New York, NY"
        result = get_quality_of_life(city)
    elif command == "crime":
        city = args[1] if len(args) > 1 else "New York, NY"
        result = get_crime_index(city)
    elif command == "healthcare":
        city = args[1] if len(args) > 1 else "New York, NY"
        result = get_healthcare_index(city)
    elif command == "property":
        city = args[1] if len(args) > 1 else "New York, NY"
        result = get_property_prices(city)
    elif command == "cpi":
        city = args[1] if len(args) > 1 else "New York, NY"
        year = int(args[2]) if len(args) > 2 else 2023
        result = get_historical_cpi(city, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
