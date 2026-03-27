"""
OECD Energy Data Fetcher
OECD Energy Statistics: energy supply/demand, electricity, oil, gas, renewables for OECD countries.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('OECD_API_KEY', '')
BASE_URL = "https://sdmx.oecd.org/public/rest/data/OECD.IEA.RES,DSD_ENERGY_STAT"

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


def get_energy_supply(country: str, product: str, year: str) -> Any:
    key = f"{country}.{product}.TOTAL_SUPPLY"
    params = {"startPeriod": year, "endPeriod": year}
    return _make_request(key, params)


def get_electricity_data(country: str, source: str, year: str) -> Any:
    key = f"{country}.ELECT.{source}"
    params = {"startPeriod": year, "endPeriod": year}
    return _make_request(key, params)


def get_oil_data(country: str, indicator: str, year: str) -> Any:
    key = f"{country}.OIL.{indicator}"
    params = {"startPeriod": year, "endPeriod": year}
    return _make_request(key, params)


def get_gas_data(country: str, indicator: str, year: str) -> Any:
    key = f"{country}.GAS.{indicator}"
    params = {"startPeriod": year, "endPeriod": year}
    return _make_request(key, params)


def get_renewables(country: str, technology: str, year: str) -> Any:
    key = f"{country}.RENEW.{technology}"
    params = {"startPeriod": year, "endPeriod": year}
    return _make_request(key, params)


def get_countries() -> Any:
    url = "https://sdmx.oecd.org/public/rest/codelist/OECD.IEA.RES/CL_AREA_ENERGY"
    params = {"format": "jsondata"}
    return _make_request(url, params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "supply":
        country = args[1] if len(args) > 1 else "DEU"
        product = args[2] if len(args) > 2 else "COAL"
        year = args[3] if len(args) > 3 else "2022"
        result = get_energy_supply(country, product, year)
    elif command == "electricity":
        country = args[1] if len(args) > 1 else "DEU"
        source = args[2] if len(args) > 2 else "WIND"
        year = args[3] if len(args) > 3 else "2022"
        result = get_electricity_data(country, source, year)
    elif command == "oil":
        country = args[1] if len(args) > 1 else "USA"
        indicator = args[2] if len(args) > 2 else "PROD"
        year = args[3] if len(args) > 3 else "2022"
        result = get_oil_data(country, indicator, year)
    elif command == "gas":
        country = args[1] if len(args) > 1 else "USA"
        indicator = args[2] if len(args) > 2 else "PROD"
        year = args[3] if len(args) > 3 else "2022"
        result = get_gas_data(country, indicator, year)
    elif command == "renewables":
        country = args[1] if len(args) > 1 else "DEU"
        technology = args[2] if len(args) > 2 else "SOLAR"
        year = args[3] if len(args) > 3 else "2022"
        result = get_renewables(country, technology, year)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
