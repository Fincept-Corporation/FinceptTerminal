"""
UNHCR Refugee Statistics Data Fetcher
Provides refugee populations, displacement figures, asylum applications,
demographic breakdowns, stateless persons, and returns data from UNHCR.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('UNHCR_API_KEY', '')
BASE_URL = "https://api.unhcr.org/population/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    default_params = {"limit": 100, "page": 1}
    if params:
        default_params.update(params)
    try:
        response = session.get(url, params=default_params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_population(country_of_origin: str = None, country_of_asylum: str = None, year: int = None) -> Any:
    """Return refugee and displaced population counts by origin and asylum country."""
    params = {}
    if country_of_origin:
        params["coo"] = country_of_origin.upper()
    if country_of_asylum:
        params["coa"] = country_of_asylum.upper()
    if year:
        params["year"] = year
    data = _make_request("population", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country_of_origin": country_of_origin, "country_of_asylum": country_of_asylum,
            "year": year, "population": data.get("items", []), "total": data.get("total", 0)}


def get_asylum_applications(country: str, year: int = None) -> Any:
    """Return asylum application counts for a country of asylum."""
    params = {"coa": country.upper()}
    if year:
        params["year"] = year
    data = _make_request("asylum-applications", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "applications": data.get("items", []),
            "total": data.get("total", 0)}


def get_demographics(country: str, year: int = None) -> Any:
    """Return age and gender demographic breakdown of refugee population in a country."""
    params = {"coa": country.upper()}
    if year:
        params["year"] = year
    data = _make_request("demographics", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "demographics": data.get("items", []),
            "total": data.get("total", 0)}


def get_stateless(country: str, year: int = None) -> Any:
    """Return stateless person counts for a country."""
    params = {"coa": country.upper()}
    if year:
        params["year"] = year
    data = _make_request("stateless", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "stateless": data.get("items", []),
            "total": data.get("total", 0)}


def get_overview(year: int = None) -> Any:
    """Return global displacement overview figures."""
    params = {}
    if year:
        params["year"] = year
    data = _make_request("population", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"year": year, "global_overview": data}


def get_countries() -> Any:
    """Return all countries of origin and asylum in the UNHCR database."""
    data = _make_request("countries", params={"limit": 300})
    if isinstance(data, dict) and "error" in data:
        return data
    return {"countries": data.get("items", []), "total": data.get("total", 0)}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "population":
        country_of_origin = args[1] if len(args) > 1 else None
        country_of_asylum = args[2] if len(args) > 2 else None
        year = int(args[3]) if len(args) > 3 else None
        result = get_population(country_of_origin, country_of_asylum, year)
    elif command == "asylum":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_asylum_applications(country, year)
    elif command == "demographics":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_demographics(country, year)
    elif command == "stateless":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_stateless(country, year)
    elif command == "overview":
        year = int(args[1]) if len(args) > 1 else None
        result = get_overview(year)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
