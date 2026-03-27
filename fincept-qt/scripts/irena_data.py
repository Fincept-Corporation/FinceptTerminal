"""
IRENA Data Fetcher
International Renewable Energy Agency: renewable capacity, generation, investment, costs by country/technology.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('IRENA_API_KEY', '')
BASE_URL = "https://www.irena.org/IRENADocuments/IRENASTAT_Online_Tool_V5_Data"

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


def get_capacity(country: str, technology: str, year: str) -> Any:
    params = {"country": country, "technology": technology, "year": year, "dataType": "capacity"}
    return _make_request("capacity", params)


def get_generation(country: str, source: str, year: str) -> Any:
    params = {"country": country, "source": source, "year": year, "dataType": "generation"}
    return _make_request("generation", params)


def get_levelized_cost(technology: str, year: str) -> Any:
    params = {"technology": technology, "year": year, "dataType": "lcoe"}
    return _make_request("costs", params)


def get_investment(country: str, sector: str, year: str) -> Any:
    params = {"country": country, "sector": sector, "year": year, "dataType": "investment"}
    return _make_request("investment", params)


def get_jobs(country: str, technology: str, year: str) -> Any:
    params = {"country": country, "technology": technology, "year": year, "dataType": "jobs"}
    return _make_request("jobs", params)


def get_countries() -> Any:
    return _make_request("countries", {})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "capacity":
        country = args[1] if len(args) > 1 else "World"
        technology = args[2] if len(args) > 2 else "Solar"
        year = args[3] if len(args) > 3 else "2023"
        result = get_capacity(country, technology, year)
    elif command == "generation":
        country = args[1] if len(args) > 1 else "World"
        source = args[2] if len(args) > 2 else "Solar"
        year = args[3] if len(args) > 3 else "2023"
        result = get_generation(country, source, year)
    elif command == "lcoe":
        technology = args[1] if len(args) > 1 else "Solar PV"
        year = args[2] if len(args) > 2 else "2023"
        result = get_levelized_cost(technology, year)
    elif command == "investment":
        country = args[1] if len(args) > 1 else "World"
        sector = args[2] if len(args) > 2 else "Renewables"
        year = args[3] if len(args) > 3 else "2023"
        result = get_investment(country, sector, year)
    elif command == "jobs":
        country = args[1] if len(args) > 1 else "World"
        technology = args[2] if len(args) > 2 else "Solar"
        year = args[3] if len(args) > 3 else "2023"
        result = get_jobs(country, technology, year)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
