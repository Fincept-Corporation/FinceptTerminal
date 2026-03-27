"""
Ember Energy Data Fetcher
Provides yearly/monthly electricity generation, demand, CO2 emissions, and
carbon intensity for 200+ countries. Data licensed under CC BY 4.0.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('EMBER_API_KEY', '')
BASE_URL = "https://api.ember-energy.org"

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


def get_electricity_generation(country: str = None, year: int = None, source: str = None) -> Any:
    """Return electricity generation data, optionally filtered by country, year, and source."""
    params = {"api_key": API_KEY} if API_KEY else {}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    if source:
        params["series"] = source
    data = _make_request("v1/electricity-generation/yearly", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "source": source, "data": data}


def get_carbon_intensity(country: str = None, year: int = None) -> Any:
    """Return carbon intensity of electricity (gCO2/kWh) for a country and year."""
    params = {"api_key": API_KEY} if API_KEY else {}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    data = _make_request("v1/carbon-intensity/yearly", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "data": data}


def get_electricity_demand(country: str = None, year: int = None) -> Any:
    """Return electricity demand data for a country and year."""
    params = {"api_key": API_KEY} if API_KEY else {}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    data = _make_request("v1/electricity-demand/yearly", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "data": data}


def get_power_sector_emissions(country: str = None, year: int = None) -> Any:
    """Return power sector CO2 emissions for a country and year."""
    params = {"api_key": API_KEY} if API_KEY else {}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    data = _make_request("v1/power-sector-emissions/yearly", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "data": data}


def get_regional_data(region: str, year: int = None) -> Any:
    """Return aggregated electricity data for a world region."""
    params = {"api_key": API_KEY} if API_KEY else {}
    params["region"] = region
    if year:
        params["year"] = year
    data = _make_request("v1/electricity-generation/yearly", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"region": region, "year": year, "data": data}


def get_countries() -> Any:
    """Return list of countries available in the Ember dataset."""
    params = {"api_key": API_KEY} if API_KEY else {}
    data = _make_request("v1/countries", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"countries": data}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "generation":
        country = args[1] if len(args) > 1 else None
        year = int(args[2]) if len(args) > 2 else None
        source = args[3] if len(args) > 3 else None
        result = get_electricity_generation(country, year, source)
    elif command == "carbon_intensity":
        country = args[1] if len(args) > 1 else None
        year = int(args[2]) if len(args) > 2 else None
        result = get_carbon_intensity(country, year)
    elif command == "demand":
        country = args[1] if len(args) > 1 else None
        year = int(args[2]) if len(args) > 2 else None
        result = get_electricity_demand(country, year)
    elif command == "emissions":
        country = args[1] if len(args) > 1 else None
        year = int(args[2]) if len(args) > 2 else None
        result = get_power_sector_emissions(country, year)
    elif command == "regional":
        region = args[1] if len(args) > 1 else ""
        year = int(args[2]) if len(args) > 2 else None
        if not region:
            result = {"error": "region required"}
        else:
            result = get_regional_data(region, year)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
