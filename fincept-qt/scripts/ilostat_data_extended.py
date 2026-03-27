"""
ILOSTAT Extended Data Fetcher
ILO Statistics SDMX API: global labour force, unemployment, wages,
working hours, and employment data for 200+ countries.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('ILOSTAT_API_KEY', '')
BASE_URL = "https://sdmx.ilo.org/rest/data/ILO"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["format"] = "jsondata"
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


def get_unemployment(country: str = "USA", sex: str = "_T", age: str = "_T", freq: str = "A") -> Any:
    key = f"DF_UNE_TUNE_SEX_AGE_NB/{freq}.{country.upper()}.{sex}.{age}"
    return _make_request(key)


def get_labour_force(country: str = "USA", sex: str = "_T", freq: str = "A") -> Any:
    key = f"DF_EAP_TEAP_SEX_AGE_NB/{freq}.{country.upper()}.{sex}._T"
    return _make_request(key)


def get_wages(country: str = "USA", freq: str = "A") -> Any:
    key = f"DF_EAR_INEE_SEX_ECO_NB/{freq}.{country.upper()}._T._T"
    return _make_request(key)


def get_working_hours(country: str = "USA", sector: str = "_T", freq: str = "A") -> Any:
    key = f"DF_HOW_TEMP_SEX_ECO_NB/{freq}.{country.upper()}._T.{sector}"
    return _make_request(key)


def get_employment_by_sector(country: str = "USA", sector: str = "X") -> Any:
    key = f"DF_EMP_TEMP_SEX_ECO_NB/A.{country.upper()}._T.{sector}"
    return _make_request(key)


def get_datasets() -> Any:
    url = "https://sdmx.ilo.org/rest/dataflow/ILO"
    try:
        response = session.get(url, params={"format": "jsondata"}, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "unemployment":
        country = args[1] if len(args) > 1 else "USA"
        sex = args[2] if len(args) > 2 else "_T"
        age = args[3] if len(args) > 3 else "_T"
        freq = args[4] if len(args) > 4 else "A"
        result = get_unemployment(country, sex, age, freq)
    elif command == "labour_force":
        country = args[1] if len(args) > 1 else "USA"
        sex = args[2] if len(args) > 2 else "_T"
        freq = args[3] if len(args) > 3 else "A"
        result = get_labour_force(country, sex, freq)
    elif command == "wages":
        country = args[1] if len(args) > 1 else "USA"
        freq = args[2] if len(args) > 2 else "A"
        result = get_wages(country, freq)
    elif command == "hours":
        country = args[1] if len(args) > 1 else "USA"
        sector = args[2] if len(args) > 2 else "_T"
        freq = args[3] if len(args) > 3 else "A"
        result = get_working_hours(country, sector, freq)
    elif command == "employment":
        country = args[1] if len(args) > 1 else "USA"
        sector = args[2] if len(args) > 2 else "X"
        result = get_employment_by_sector(country, sector)
    elif command == "datasets":
        result = get_datasets()

    print(json.dumps(result))


if __name__ == "__main__":
    main()
