"""
RSF Press Freedom Data Fetcher
Reporters Without Borders Press Freedom Index: annual rankings and scores for 180 countries.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://rsf.org/api"

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
    return _make_request("index", params={"year": 2024})


def get_index_scores(year: int = 2024) -> Any:
    return _make_request("index", params={"year": year})


def get_country_score(country: str, year: int = 2024) -> Any:
    params = {"country": country, "year": year}
    return _make_request("country", params=params)


def get_rankings(year: int = 2024, region: str = None) -> Any:
    params = {"year": year}
    if region:
        params["region"] = region
    return _make_request("ranking", params=params)


def get_trend(country: str, start_year: int = 2015, end_year: int = 2024) -> Any:
    params = {
        "country": country,
        "startYear": start_year,
        "endYear": end_year
    }
    return _make_request("trend", params=params)


def get_indicators(country: str, year: int = 2024) -> Any:
    params = {"country": country, "year": year}
    return _make_request("indicators", params=params)


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
        year = int(args[1]) if len(args) > 1 else 2024
        result = get_index_scores(year)
    elif command == "country":
        country = args[1] if len(args) > 1 else "FRA"
        year = int(args[2]) if len(args) > 2 else 2024
        result = get_country_score(country, year)
    elif command == "rankings":
        year = int(args[1]) if len(args) > 1 else 2024
        region = args[2] if len(args) > 2 else None
        result = get_rankings(year, region)
    elif command == "trend":
        country = args[1] if len(args) > 1 else "FRA"
        start_year = int(args[2]) if len(args) > 2 else 2015
        end_year = int(args[3]) if len(args) > 3 else 2024
        result = get_trend(country, start_year, end_year)
    elif command == "indicators":
        country = args[1] if len(args) > 1 else "FRA"
        year = int(args[2]) if len(args) > 2 else 2024
        result = get_indicators(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
