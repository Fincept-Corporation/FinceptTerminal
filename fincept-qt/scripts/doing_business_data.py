"""
Doing Business Data Fetcher
World Bank Doing Business / B-READY indicators: business environment scores for 190 countries.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.worldbank.org/v2/en/indicator"

INDICATORS = {
    "ease_of_doing_business": "IC.BUS.EASE.XQ",
    "starting_a_business_rank": "IC.REG.PROC",
    "trading_across_borders": "IC.IMP.DOCS",
    "getting_credit": "IC.CRD.INFO.XQ",
    "protecting_investors": "IC.PRT.SINV.WB.XQ",
    "paying_taxes": "IC.TAX.TOTL.CP.ZS",
    "resolving_insolvency": "IC.ISV.DURS",
    "enforcing_contracts": "IC.LGL.CRED.XQ"
}

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["format"] = "json"
    params["per_page"] = 100
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


def get_ease_of_doing_business(country: str = "all", year: int = 2020) -> Any:
    indicator = INDICATORS["ease_of_doing_business"]
    url = f"https://api.worldbank.org/v2/country/{country}/indicator/{indicator}"
    params = {"date": str(year), "format": "json"}
    return _make_request(url, params=params)


def get_starting_business(country: str = "all", year: int = 2020) -> Any:
    indicator = INDICATORS["starting_a_business_rank"]
    url = f"https://api.worldbank.org/v2/country/{country}/indicator/{indicator}"
    params = {"date": str(year), "format": "json"}
    return _make_request(url, params=params)


def get_trading_across_borders(country: str = "all", year: int = 2020) -> Any:
    indicator = INDICATORS["trading_across_borders"]
    url = f"https://api.worldbank.org/v2/country/{country}/indicator/{indicator}"
    params = {"date": str(year), "format": "json"}
    return _make_request(url, params=params)


def get_getting_credit(country: str = "all", year: int = 2020) -> Any:
    indicator = INDICATORS["getting_credit"]
    url = f"https://api.worldbank.org/v2/country/{country}/indicator/{indicator}"
    params = {"date": str(year), "format": "json"}
    return _make_request(url, params=params)


def get_rankings(year: int = 2020) -> Any:
    indicator = INDICATORS["ease_of_doing_business"]
    url = f"https://api.worldbank.org/v2/country/all/indicator/{indicator}"
    params = {"date": str(year), "format": "json", "per_page": 300}
    return _make_request(url, params=params)


def get_all_indicators(country: str = "US", year: int = 2020) -> Any:
    results = {}
    for name, indicator_code in INDICATORS.items():
        url = f"https://api.worldbank.org/v2/country/{country}/indicator/{indicator_code}"
        params = {"date": str(year), "format": "json"}
        results[name] = _make_request(url, params=params)
    return results


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "overall":
        country = args[1] if len(args) > 1 else "all"
        year = int(args[2]) if len(args) > 2 else 2020
        result = get_ease_of_doing_business(country, year)
    elif command == "starting":
        country = args[1] if len(args) > 1 else "all"
        year = int(args[2]) if len(args) > 2 else 2020
        result = get_starting_business(country, year)
    elif command == "trading":
        country = args[1] if len(args) > 1 else "all"
        year = int(args[2]) if len(args) > 2 else 2020
        result = get_trading_across_borders(country, year)
    elif command == "credit":
        country = args[1] if len(args) > 1 else "all"
        year = int(args[2]) if len(args) > 2 else 2020
        result = get_getting_credit(country, year)
    elif command == "rankings":
        year = int(args[1]) if len(args) > 1 else 2020
        result = get_rankings(year)
    elif command == "all":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2020
        result = get_all_indicators(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
