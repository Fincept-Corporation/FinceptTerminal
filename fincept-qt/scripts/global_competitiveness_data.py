"""
Global Competitiveness Data Fetcher
IMD World Competitiveness Yearbook + WEF GCI proxy via World Bank: competitiveness indicators by country.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.worldbank.org/v2/en/indicator"

COMPETITIVENESS_INDICATORS = {
    "infrastructure": "IS.AIR.PSGR",
    "technology_readiness": "IT.NET.USER.ZS",
    "market_efficiency": "IC.BUS.EASE.XQ",
    "institutions": "IQ.CPA.PUBS.XQ",
    "innovation": "IP.PAT.RESD",
    "business_sophistication": "IC.FRM.TRNG.ZS",
    "higher_education": "SE.TER.ENRR",
    "financial_market": "FB.BNK.CAPA.ZS"
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


def _fetch_indicator(indicator_code: str, country: str, year: int) -> Any:
    url = f"https://api.worldbank.org/v2/country/{country}/indicator/{indicator_code}"
    params = {"date": str(year), "format": "json"}
    return _make_request(url, params=params)


def get_infrastructure_score(country: str = "US", year: int = 2022) -> Any:
    return _fetch_indicator(COMPETITIVENESS_INDICATORS["infrastructure"], country, year)


def get_technology_readiness(country: str = "US", year: int = 2022) -> Any:
    return _fetch_indicator(COMPETITIVENESS_INDICATORS["technology_readiness"], country, year)


def get_market_efficiency(country: str = "US", year: int = 2022) -> Any:
    return _fetch_indicator(COMPETITIVENESS_INDICATORS["market_efficiency"], country, year)


def get_institutions_score(country: str = "US", year: int = 2022) -> Any:
    return _fetch_indicator(COMPETITIVENESS_INDICATORS["institutions"], country, year)


def get_innovation_score(country: str = "US", year: int = 2022) -> Any:
    return _fetch_indicator(COMPETITIVENESS_INDICATORS["innovation"], country, year)


def get_rankings(indicator: str = "technology_readiness", year: int = 2022) -> Any:
    indicator_code = COMPETITIVENESS_INDICATORS.get(indicator, COMPETITIVENESS_INDICATORS["technology_readiness"])
    url = f"https://api.worldbank.org/v2/country/all/indicator/{indicator_code}"
    params = {"date": str(year), "format": "json", "per_page": 300}
    return _make_request(url, params=params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "infrastructure":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_infrastructure_score(country, year)
    elif command == "technology":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_technology_readiness(country, year)
    elif command == "market":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_market_efficiency(country, year)
    elif command == "institutions":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_institutions_score(country, year)
    elif command == "innovation":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_innovation_score(country, year)
    elif command == "rankings":
        indicator = args[1] if len(args) > 1 else "technology_readiness"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_rankings(indicator, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
