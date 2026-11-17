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

import fincept_connector as fc
import fincept_json

API_KEY = os.environ.get('EMBER_API_KEY', '')
BASE_URL = "https://api.ember-energy.org"
SERVICE = "Ember"

session = fc.http_session()


def _make_request(endpoint: str, params: Dict = None) -> Any:
    # Every Ember v1 endpoint (options included) answers HTTP 403 "No API key set" without a key.
    if not API_KEY:
        return fc.missing_key("The Ember API", "https://ember-energy.org/data/api/", "EMBER_API_KEY",
                              what="a free API key")
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    params = dict(params or {})
    params["api_key"] = API_KEY
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, "EMBER_API_KEY", API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}


def _filters(country: str = None, year: int = None) -> Dict:
    """Ember v1 filter params: `entity_code` (ISO-3) or `entity` (country/region name), and
    `start_date`/`end_date` (YYYY). The API silently ignores unknown names such as `country`/`year`."""
    params = {}
    if country:
        c = str(country).strip()
        if len(c) == 3 and c.isalpha():
            params["entity_code"] = c.upper()
        else:
            params["entity"] = c
    if year:
        params["start_date"] = str(year)
        params["end_date"] = str(year)
    return params


def get_electricity_generation(country: str = None, year: int = None, source: str = None) -> Any:
    """Return electricity generation data, optionally filtered by country, year, and source."""
    params = _filters(country, year)
    if source:
        params["series"] = source
    data = _make_request("v1/electricity-generation/yearly", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "source": source, "data": data}


def get_carbon_intensity(country: str = None, year: int = None) -> Any:
    """Return carbon intensity of electricity (gCO2/kWh) for a country and year."""
    params = _filters(country, year)
    data = _make_request("v1/carbon-intensity/yearly", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "data": data}


def get_electricity_demand(country: str = None, year: int = None) -> Any:
    """Return electricity demand data for a country and year."""
    params = _filters(country, year)
    data = _make_request("v1/electricity-demand/yearly", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "data": data}


def get_power_sector_emissions(country: str = None, year: int = None) -> Any:
    """Return power sector CO2 emissions for a country and year."""
    params = _filters(country, year)
    data = _make_request("v1/power-sector-emissions/yearly", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "data": data}


def get_regional_data(region: str, year: int = None) -> Any:
    """Return aggregated electricity data for a world region."""
    params = _filters(None, year)
    params["entity"] = region
    params["is_aggregate_entity"] = "true"
    data = _make_request("v1/electricity-generation/yearly", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"region": region, "year": year, "data": data}


def get_countries() -> Any:
    """Return list of countries available in the Ember dataset."""
    # There is no /v1/countries; the options endpoint lists the accepted `entity` values.
    data = _make_request("v1/options/electricity-generation/yearly/entity")
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
    print(fincept_json.dumps(result))


if __name__ == "__main__":
    main()
