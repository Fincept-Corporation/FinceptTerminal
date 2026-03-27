"""
EIU Democracy Index Data Fetcher
EIU Democracy Index + Economist open data: democracy scores, regime types for 167 countries.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://www.eiu.com/n/solutions/democracy-index"

DEMOCRACY_INDEX_STATIC = {
    "source": "Economist Intelligence Unit Democracy Index",
    "description": "Annual measure of democracy for 167 countries across 5 categories",
    "categories": [
        "Electoral process and pluralism",
        "Functioning of government",
        "Political participation",
        "Political culture",
        "Civil liberties"
    ],
    "regime_types": [
        {"type": "Full democracy", "score_range": "8-10"},
        {"type": "Flawed democracy", "score_range": "6-7.9"},
        {"type": "Hybrid regime", "score_range": "4-5.9"},
        {"type": "Authoritarian", "score_range": "0-3.9"}
    ]
}

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


def _fetch_via_worldbank(indicator: str, country: str, year: int) -> Any:
    url = f"https://api.worldbank.org/v2/country/{country}/indicator/{indicator}"
    params = {"date": str(year), "format": "json"}
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except Exception as e:
        return {"error": str(e)}


def get_democracy_scores(year: int = 2023, country: str = None) -> Any:
    params = {"year": year}
    if country:
        params["country"] = country
    result = _make_request("data", params=params)
    if "error" in result:
        result["fallback_info"] = DEMOCRACY_INDEX_STATIC
    return result


def get_regime_type(country: str, year: int = 2023) -> Any:
    params = {"country": country, "year": year}
    result = _make_request("regime", params=params)
    if "error" in result:
        result["reference"] = DEMOCRACY_INDEX_STATIC["regime_types"]
    return result


def get_rankings(year: int = 2023, category: str = None) -> Any:
    params = {"year": year}
    if category:
        params["category"] = category
    return _make_request("rankings", params=params)


def get_trend(country: str, start_year: int = 2010, end_year: int = 2023) -> Any:
    params = {
        "country": country,
        "startYear": start_year,
        "endYear": end_year
    }
    return _make_request("trend", params=params)


def get_categories(country: str, year: int = 2023) -> Any:
    params = {"country": country, "year": year}
    result = _make_request("categories", params=params)
    if "error" in result:
        result["categories"] = DEMOCRACY_INDEX_STATIC["categories"]
    return result


def get_regional_averages(year: int = 2023) -> Any:
    return _make_request("regional", params={"year": year})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "scores":
        year = int(args[1]) if len(args) > 1 else 2023
        country = args[2] if len(args) > 2 else None
        result = get_democracy_scores(year, country)
    elif command == "regime":
        country = args[1] if len(args) > 1 else "United States"
        year = int(args[2]) if len(args) > 2 else 2023
        result = get_regime_type(country, year)
    elif command == "rankings":
        year = int(args[1]) if len(args) > 1 else 2023
        category = args[2] if len(args) > 2 else None
        result = get_rankings(year, category)
    elif command == "trend":
        country = args[1] if len(args) > 1 else "United States"
        start_year = int(args[2]) if len(args) > 2 else 2010
        end_year = int(args[3]) if len(args) > 3 else 2023
        result = get_trend(country, start_year, end_year)
    elif command == "categories":
        country = args[1] if len(args) > 1 else "United States"
        year = int(args[2]) if len(args) > 2 else 2023
        result = get_categories(country, year)
    elif command == "regional":
        year = int(args[1]) if len(args) > 1 else 2023
        result = get_regional_averages(year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
