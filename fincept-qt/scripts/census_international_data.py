"""
Census International Data Fetcher
US Census International Data: population, demographics, economic development indicators for all countries (IDB).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('CENSUS_API_KEY', '')
BASE_URL = "https://api.census.gov/data/timeseries/idb"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    if API_KEY:
        params["key"] = API_KEY
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
    return _make_request("1yr", params={"get": "GENC,NAME", "FIPS": "*", "time": "2023"})


def get_population_projection(country: str, year: int = 2023, sex: str = "0") -> Any:
    params = {
        "get": "POP,NAME,GENC",
        "FIPS": country,
        "time": str(year),
        "SEX": sex
    }
    return _make_request("1yr", params=params)


def get_age_structure(country: str, year: int = 2023) -> Any:
    params = {
        "get": "AGE,POP,NAME",
        "FIPS": country,
        "time": str(year),
        "SEX": "0"
    }
    return _make_request("5yr", params=params)


def get_birth_death_rates(country: str, year: int = 2023) -> Any:
    params = {
        "get": "CBR,CDR,NAME,GENC",
        "FIPS": country,
        "time": str(year)
    }
    return _make_request("1yr", params=params)


def get_infant_mortality(country: str, year: int = 2023) -> Any:
    params = {
        "get": "IMR,NAME,GENC",
        "FIPS": country,
        "time": str(year)
    }
    return _make_request("1yr", params=params)


def get_life_expectancy(country: str, year: int = 2023, sex: str = "0") -> Any:
    params = {
        "get": "E0,NAME,GENC",
        "FIPS": country,
        "time": str(year),
        "SEX": sex
    }
    return _make_request("1yr", params=params)


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
    elif command == "projection":
        country = args[1] if len(args) > 1 else "IN"
        year = int(args[2]) if len(args) > 2 else 2023
        sex = args[3] if len(args) > 3 else "0"
        result = get_population_projection(country, year, sex)
    elif command == "age_structure":
        country = args[1] if len(args) > 1 else "IN"
        year = int(args[2]) if len(args) > 2 else 2023
        result = get_age_structure(country, year)
    elif command == "birth_death":
        country = args[1] if len(args) > 1 else "IN"
        year = int(args[2]) if len(args) > 2 else 2023
        result = get_birth_death_rates(country, year)
    elif command == "infant_mortality":
        country = args[1] if len(args) > 1 else "IN"
        year = int(args[2]) if len(args) > 2 else 2023
        result = get_infant_mortality(country, year)
    elif command == "life_expectancy":
        country = args[1] if len(args) > 1 else "IN"
        year = int(args[2]) if len(args) > 2 else 2023
        sex = args[3] if len(args) > 3 else "0"
        result = get_life_expectancy(country, year, sex)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
