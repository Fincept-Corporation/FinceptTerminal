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

import fincept_connector as fc
import fincept_json

API_KEY = os.environ.get('NUMBEO_API_KEY', '')
BASE_URL = "https://www.numbeo.com/api"
SERVICE = "Numbeo"

session = fc.http_session()


def _make_request(endpoint: str, params: Dict = None) -> Any:
    # Every Numbeo API endpoint needs the key: say so before any network call.
    if not API_KEY:
        return fc.missing_key(SERVICE, "https://www.numbeo.com/api/", "NUMBEO_API_KEY")
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["api_key"] = API_KEY
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, "NUMBEO_API_KEY", API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}
    # Numbeo answers HTTP 200 with {"error": "invalid api_key= <the key>"}: never echo the key.
    if isinstance(data, dict) and isinstance(data.get("error"), str):
        return {"error": f"{SERVICE}: {fc.scrub(data['error'], API_KEY)}"}
    return data


def get_cost_of_living(city: str = "New York, NY", currency: str = "USD") -> Any:
    params = {"query": city, "currency": currency}
    return _make_request("city_prices", params)


# The old paths quality_of_life / crime / healthcare / property_prices_by_city / historical_currency
# do not exist on numbeo.com/api (they return the HTML error page); these are the documented ones.
def get_quality_of_life(city: str = "New York, NY") -> Any:
    params = {"query": city}
    return _make_request("indices", params)  # quality_of_life_index and the other city indices


def get_crime_index(city: str = "New York, NY") -> Any:
    params = {"query": city}
    return _make_request("city_crime", params)


def get_healthcare_index(city: str = "New York, NY") -> Any:
    params = {"query": city}
    return _make_request("city_healthcare", params)


def get_property_prices(city: str = "New York, NY") -> Any:
    params = {"query": city}
    return _make_request("indices", params)  # property_price_to_income_ratio, mortgage/rent indices


def get_historical_cpi(city: str = "New York, NY", year: int = 2023) -> Any:
    params = {"query": city, "year": year}
    return _make_request("historical_city_prices", params)


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
    print(fincept_json.dumps(result))


if __name__ == "__main__":
    main()
