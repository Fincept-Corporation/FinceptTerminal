"""
UN Population Data Fetcher
UN World Population Prospects: population projections, demographic indicators for all countries 1950-2100.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://population.un.org/dataportalapi/api/v1"

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


def get_countries() -> Any:
    return _make_request("locations", params={"pageSize": 500})


def get_population(country: str, start_year: int = 2020, end_year: int = 2030, variant: str = "Medium") -> Any:
    params = {
        "locations": country,
        "startYear": start_year,
        "endYear": end_year,
        "variants": variant,
        "pageSize": 100
    }
    return _make_request("indicators/49/locations/data", params=params)


def get_age_structure(country: str, year: int = 2023, sex: str = "Both sexes", variant: str = "Medium") -> Any:
    params = {
        "locations": country,
        "startYear": year,
        "endYear": year,
        "variants": variant,
        "pageSize": 200
    }
    return _make_request("indicators/47/locations/data", params=params)


def get_fertility_rate(country: str, start_year: int = 2000, end_year: int = 2030) -> Any:
    params = {
        "locations": country,
        "startYear": start_year,
        "endYear": end_year,
        "variants": "Medium",
        "pageSize": 100
    }
    return _make_request("indicators/59/locations/data", params=params)


def get_life_expectancy(country: str, start_year: int = 2000, end_year: int = 2030, sex: str = "Both sexes") -> Any:
    params = {
        "locations": country,
        "startYear": start_year,
        "endYear": end_year,
        "variants": "Medium",
        "pageSize": 100
    }
    return _make_request("indicators/68/locations/data", params=params)


def get_migration(country: str, start_year: int = 2000, end_year: int = 2030) -> Any:
    params = {
        "locations": country,
        "startYear": start_year,
        "endYear": end_year,
        "variants": "Medium",
        "pageSize": 100
    }
    return _make_request("indicators/84/locations/data", params=params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "countries":
        result = get_countries()
    elif command == "population":
        country = args[1] if len(args) > 1 else "900"
        start_year = int(args[2]) if len(args) > 2 else 2020
        end_year = int(args[3]) if len(args) > 3 else 2030
        variant = args[4] if len(args) > 4 else "Medium"
        result = get_population(country, start_year, end_year, variant)
    elif command == "age_structure":
        country = args[1] if len(args) > 1 else "900"
        year = int(args[2]) if len(args) > 2 else 2023
        sex = args[3] if len(args) > 3 else "Both sexes"
        variant = args[4] if len(args) > 4 else "Medium"
        result = get_age_structure(country, year, sex, variant)
    elif command == "fertility":
        country = args[1] if len(args) > 1 else "900"
        start_year = int(args[2]) if len(args) > 2 else 2000
        end_year = int(args[3]) if len(args) > 3 else 2030
        result = get_fertility_rate(country, start_year, end_year)
    elif command == "life_expectancy":
        country = args[1] if len(args) > 1 else "900"
        start_year = int(args[2]) if len(args) > 2 else 2000
        end_year = int(args[3]) if len(args) > 3 else 2030
        sex = args[4] if len(args) > 4 else "Both sexes"
        result = get_life_expectancy(country, start_year, end_year, sex)
    elif command == "migration":
        country = args[1] if len(args) > 1 else "900"
        start_year = int(args[2]) if len(args) > 2 else 2000
        end_year = int(args[3]) if len(args) > 3 else 2030
        result = get_migration(country, start_year, end_year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
