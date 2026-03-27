"""
World Economic Forum (WEF) Data Fetcher
Global Competitiveness Index, travel & tourism competitiveness, and energy
transition data via the TC Data 360 (World Bank-hosted WEF datasets) API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WEFORUM_API_KEY', '')
BASE_URL = "https://tcdata360.worldbank.org/api/v1"

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


def get_indicator(indicator_id: str, country: str = None, start_year: str = None, end_year: str = None) -> Any:
    params = {"indicators": indicator_id}
    if country:
        params["countries"] = country
    if start_year:
        params["dateRange"] = f"{start_year},{end_year or start_year}"
    return _make_request("data", params)


def get_topics() -> Any:
    return _make_request("topics")


def get_countries() -> Any:
    return _make_request("countries")


def get_rankings(indicator_id: str, year: str = None) -> Any:
    params = {"indicators": indicator_id}
    if year:
        params["dateRange"] = f"{year},{year}"
    return _make_request("data", params)


def get_dataset(dataset_id: str, country: str = None) -> Any:
    params = {"datasource": dataset_id}
    if country:
        params["countries"] = country
    return _make_request("data", params)


def search_indicators(query: str) -> Any:
    params = {"search": query}
    return _make_request("indicators", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "indicator":
        if len(args) < 2:
            result = {"error": "indicator_id required"}
        else:
            country = args[2] if len(args) > 2 else None
            start_year = args[3] if len(args) > 3 else None
            end_year = args[4] if len(args) > 4 else None
            result = get_indicator(args[1], country, start_year, end_year)
    elif command == "topics":
        result = get_topics()
    elif command == "countries":
        result = get_countries()
    elif command == "rankings":
        if len(args) < 2:
            result = {"error": "indicator_id required"}
        else:
            year = args[2] if len(args) > 2 else None
            result = get_rankings(args[1], year)
    elif command == "dataset":
        if len(args) < 2:
            result = {"error": "dataset_id required"}
        else:
            country = args[2] if len(args) > 2 else None
            result = get_dataset(args[1], country)
    elif command == "search":
        if len(args) < 2:
            result = {"error": "query required"}
        else:
            result = search_indicators(args[1])
    print(json.dumps(result))


if __name__ == "__main__":
    main()
