"""
WHO Immunization Data Fetcher
Provides vaccination coverage rates, disease incidence, immunization schedules,
stockout data, and global coverage statistics via WHO and UNICEF data sources.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WHO_IMMUNIZATION_API_KEY', '')
BASE_URL = "https://immunizationdata.who.int/api"

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


def get_coverage(vaccine: str, country: str, year: int = None) -> Any:
    """Return vaccination coverage rate for a specific vaccine and country."""
    params = {
        "vaccine": vaccine.upper(),
        "ISO_3_CODE": country.upper()
    }
    if year:
        params["YEAR"] = year
    data = _make_request("coverage", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"vaccine": vaccine, "country": country, "year": year, "coverage": data}


def get_global_coverage(vaccine: str, year: int = None) -> Any:
    """Return global vaccination coverage for a specific vaccine."""
    params = {"vaccine": vaccine.upper(), "group": "GLOBAL"}
    if year:
        params["YEAR"] = year
    data = _make_request("coverage", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"vaccine": vaccine, "year": year, "global_coverage": data}


def get_disease_incidence(disease: str, country: str, year: int = None) -> Any:
    """Return disease incidence/case counts for a vaccine-preventable disease."""
    params = {
        "disease": disease.upper(),
        "ISO_3_CODE": country.upper()
    }
    if year:
        params["YEAR"] = year
    data = _make_request("incidence", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"disease": disease, "country": country, "year": year, "incidence": data}


def get_stockout_data(country: str, year: int = None) -> Any:
    """Return vaccine stockout and supply disruption data for a country."""
    params = {"ISO_3_CODE": country.upper()}
    if year:
        params["YEAR"] = year
    data = _make_request("supply", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "stockout_data": data}


def get_schedule(country: str) -> Any:
    """Return the national immunization schedule for a country."""
    params = {"ISO_3_CODE": country.upper()}
    data = _make_request("schedule", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "schedule": data}


def get_countries() -> Any:
    """Return all countries available in the WHO immunization database."""
    data = _make_request("countries")
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
    if command == "coverage":
        if len(args) < 3:
            result = {"error": "vaccine and country required"}
        else:
            vaccine = args[1]
            country = args[2]
            year = int(args[3]) if len(args) > 3 else None
            result = get_coverage(vaccine, country, year)
    elif command == "global":
        vaccine = args[1] if len(args) > 1 else ""
        if not vaccine:
            result = {"error": "vaccine required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_global_coverage(vaccine, year)
    elif command == "incidence":
        if len(args) < 3:
            result = {"error": "disease and country required"}
        else:
            disease = args[1]
            country = args[2]
            year = int(args[3]) if len(args) > 3 else None
            result = get_disease_incidence(disease, country, year)
    elif command == "stockout":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_stockout_data(country, year)
    elif command == "schedule":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            result = get_schedule(country)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
