"""
Worldometers Data Fetcher
Worldometers real-time statistics: world population counter, COVID stats, countries data via disease.sh API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://disease.sh/v3/covid-19"

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


def get_global_covid() -> Any:
    return _make_request("all", params={"source": "worldometers"})


def get_country_covid(country: str) -> Any:
    return _make_request(f"countries/{country}", params={"source": "worldometers"})


def get_all_countries_covid() -> Any:
    return _make_request("countries", params={"source": "worldometers", "sort": "cases"})


def get_historical_covid(country: str, days: int = 30) -> Any:
    return _make_request(f"historical/{country}", params={"lastdays": days})


def get_covid_vaccines(country: str) -> Any:
    if country.lower() in ("all", "global", "world"):
        return _make_request("vaccine/coverage", params={"lastdays": 30, "fullData": True})
    return _make_request(f"vaccine/coverage/countries/{country}", params={"lastdays": 30, "fullData": True})


def get_continents_covid() -> Any:
    return _make_request("continents", params={"source": "worldometers"})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "global":
        result = get_global_covid()
    elif command == "country":
        country = args[1] if len(args) > 1 else "USA"
        result = get_country_covid(country)
    elif command == "all_countries":
        result = get_all_countries_covid()
    elif command == "historical":
        country = args[1] if len(args) > 1 else "USA"
        days = int(args[2]) if len(args) > 2 else 30
        result = get_historical_covid(country, days)
    elif command == "vaccines":
        country = args[1] if len(args) > 1 else "all"
        result = get_covid_vaccines(country)
    elif command == "continents":
        result = get_continents_covid()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
