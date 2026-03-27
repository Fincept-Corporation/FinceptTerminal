"""
UNICEF Data Fetcher
Child health, education, nutrition, and water/sanitation data for all countries
via the UNICEF SDMX public data API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('UNICEF_API_KEY', '')
BASE_URL = "https://sdmx.data.unicef.org/ws/public/sdmxapi/rest/data"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {"Accept": "application/json"}
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_child_mortality(country: str = "ALL", year: str = None) -> Any:
    flow = "UNICEF,CME_DF,1.0"
    key = f"MR_U5.{country}.."
    params = {}
    if year:
        params["startPeriod"] = year
        params["endPeriod"] = year
    return _make_request(f"{flow}/{key}", params)


def get_immunization(country: str = "ALL", vaccine: str = "DTP3", year: str = None) -> Any:
    flow = "UNICEF,IMMUNISATION,1.0"
    key = f"{vaccine}.{country}.."
    params = {}
    if year:
        params["startPeriod"] = year
        params["endPeriod"] = year
    return _make_request(f"{flow}/{key}", params)


def get_nutrition(country: str = "ALL", indicator: str = "STUNTING", year: str = None) -> Any:
    flow = "UNICEF,NUTRITION,1.0"
    key = f"{indicator}.{country}.."
    params = {}
    if year:
        params["startPeriod"] = year
        params["endPeriod"] = year
    return _make_request(f"{flow}/{key}", params)


def get_education(country: str = "ALL", level: str = "PRIMARY", sex: str = "T", year: str = None) -> Any:
    flow = "UNICEF,EDUCATION,1.0"
    key = f"NET_ATT_{level}.{country}.{sex}."
    params = {}
    if year:
        params["startPeriod"] = year
        params["endPeriod"] = year
    return _make_request(f"{flow}/{key}", params)


def get_water_sanitation(country: str = "ALL", indicator: str = "DRINKING_WATER", year: str = None) -> Any:
    flow = "UNICEF,WASH,1.0"
    key = f"{indicator}.{country}.."
    params = {}
    if year:
        params["startPeriod"] = year
        params["endPeriod"] = year
    return _make_request(f"{flow}/{key}", params)


def get_datasets() -> Any:
    base = "https://sdmx.data.unicef.org/ws/public/sdmxapi/rest/dataflow/UNICEF"
    return _make_request(base)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "mortality":
        country = args[1] if len(args) > 1 else "ALL"
        year = args[2] if len(args) > 2 else None
        result = get_child_mortality(country, year)
    elif command == "immunization":
        country = args[1] if len(args) > 1 else "ALL"
        vaccine = args[2] if len(args) > 2 else "DTP3"
        year = args[3] if len(args) > 3 else None
        result = get_immunization(country, vaccine, year)
    elif command == "nutrition":
        country = args[1] if len(args) > 1 else "ALL"
        indicator = args[2] if len(args) > 2 else "STUNTING"
        year = args[3] if len(args) > 3 else None
        result = get_nutrition(country, indicator, year)
    elif command == "education":
        country = args[1] if len(args) > 1 else "ALL"
        level = args[2] if len(args) > 2 else "PRIMARY"
        sex = args[3] if len(args) > 3 else "T"
        year = args[4] if len(args) > 4 else None
        result = get_education(country, level, sex, year)
    elif command == "wash":
        country = args[1] if len(args) > 1 else "ALL"
        indicator = args[2] if len(args) > 2 else "DRINKING_WATER"
        year = args[3] if len(args) > 3 else None
        result = get_water_sanitation(country, indicator, year)
    elif command == "datasets":
        result = get_datasets()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
