"""
Global Innovation Index Data Fetcher
Global Innovation Index (WIPO/Cornell): innovation ranking and scores for 130+ countries.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://www.globalinnovationindex.org/api"

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
    return _make_request("countries")


def get_gii_scores(year: int = 2023, country: str = None) -> Any:
    params = {"year": year}
    if country:
        params["country"] = country
    return _make_request("scores", params=params)


def get_rankings(year: int = 2023, pillar: str = None) -> Any:
    params = {"year": year}
    if pillar:
        params["pillar"] = pillar
    return _make_request("rankings", params=params)


def get_trend(country: str, start_year: int = 2015, end_year: int = 2023) -> Any:
    params = {
        "country": country,
        "startYear": start_year,
        "endYear": end_year
    }
    return _make_request("trend", params=params)


def get_pillars(country: str, year: int = 2023) -> Any:
    params = {"country": country, "year": year}
    return _make_request("pillars", params=params)


def get_regional_scores(year: int = 2023, region: str = None) -> Any:
    params = {"year": year}
    if region:
        params["region"] = region
    return _make_request("regional", params=params)


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
    elif command == "scores":
        year = int(args[1]) if len(args) > 1 else 2023
        country = args[2] if len(args) > 2 else None
        result = get_gii_scores(year, country)
    elif command == "rankings":
        year = int(args[1]) if len(args) > 1 else 2023
        pillar = args[2] if len(args) > 2 else None
        result = get_rankings(year, pillar)
    elif command == "trend":
        country = args[1] if len(args) > 1 else "United States"
        start_year = int(args[2]) if len(args) > 2 else 2015
        end_year = int(args[3]) if len(args) > 3 else 2023
        result = get_trend(country, start_year, end_year)
    elif command == "pillars":
        country = args[1] if len(args) > 1 else "United States"
        year = int(args[2]) if len(args) > 2 else 2023
        result = get_pillars(country, year)
    elif command == "regional":
        year = int(args[1]) if len(args) > 1 else 2023
        region = args[2] if len(args) > 2 else None
        result = get_regional_scores(year, region)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
