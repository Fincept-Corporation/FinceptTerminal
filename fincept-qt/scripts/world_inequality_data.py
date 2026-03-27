"""
World Inequality Database (WID) Data Fetcher
Provides income and wealth inequality metrics including top income shares,
wealth distribution, Gini coefficients, and pre-tax income for 100+ countries.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WID_API_KEY', '')
BASE_URL = "https://wid.world/api"

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


def get_data(country: str, indicator: str, pop: str = "j", age: str = "992", year: int = None) -> Any:
    """Return a specific WID indicator for a country.

    indicator: WID variable code (e.g., 'sptinc', 'aptinc', 'wptinc')
    pop: population (j=equal-split adults, i=individuals)
    age: age group (992=all adults, 999=all)
    """
    params = {
        "country": country,
        "indicators": indicator,
        "perc": "p0p100",
        "pop": pop,
        "age": age,
        "format": "json"
    }
    if year:
        params["year"] = year
    data = _make_request("", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "indicator": indicator, "pop": pop, "age": age, "year": year, "data": data}


def get_countries() -> Any:
    """Return all countries and regions available in WID."""
    params = {"format": "json"}
    data = _make_request("countries", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"countries": data}


def get_indicators() -> Any:
    """Return all available WID variable codes and their descriptions."""
    params = {"format": "json"}
    data = _make_request("variables", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"indicators": data}


def get_top_income_shares(country: str, year: int = None) -> Any:
    """Return top 1%, 10% pre-tax income shares for a country."""
    results = {}
    for perc, label in [("p99p100", "top_1pct"), ("p90p100", "top_10pct"), ("p50p100", "top_50pct")]:
        params = {
            "country": country,
            "indicators": "sptinc",
            "perc": perc,
            "pop": "j",
            "age": "992",
            "format": "json"
        }
        if year:
            params["year"] = year
        data = _make_request("", params=params)
        results[label] = data if not (isinstance(data, dict) and "error" in data) else data
    return {"country": country, "year": year, "top_income_shares": results}


def get_wealth_distribution(country: str, year: int = None) -> Any:
    """Return wealth distribution shares by percentile for a country."""
    params = {
        "country": country,
        "indicators": "shweal",
        "perc": "p0p100",
        "pop": "j",
        "age": "992",
        "format": "json"
    }
    if year:
        params["year"] = year
    data = _make_request("", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "wealth_distribution": data}


def get_pretax_income(country: str, year: int = None) -> Any:
    """Return pre-tax national income per adult for a country."""
    params = {
        "country": country,
        "indicators": "aptinc",
        "perc": "p0p100",
        "pop": "j",
        "age": "992",
        "format": "json"
    }
    if year:
        params["year"] = year
    data = _make_request("", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "pretax_income": data}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "data":
        if len(args) < 3:
            result = {"error": "country and indicator required"}
        else:
            country = args[1]
            indicator = args[2]
            pop = args[3] if len(args) > 3 else "j"
            age = args[4] if len(args) > 4 else "992"
            year = int(args[5]) if len(args) > 5 else None
            result = get_data(country, indicator, pop, age, year)
    elif command == "countries":
        result = get_countries()
    elif command == "indicators":
        result = get_indicators()
    elif command == "top_shares":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_top_income_shares(country, year)
    elif command == "wealth":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_wealth_distribution(country, year)
    elif command == "income":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_pretax_income(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
