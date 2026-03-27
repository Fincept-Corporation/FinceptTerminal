"""
WIPO Intellectual Property Statistics Data Fetcher
Patent filings, trademark applications, industrial designs, and IP statistics
by country via the WIPO IP Statistics Data Center.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WIPO_API_KEY', '')
BASE_URL = "https://www.wipo.int/ipstats/en/statistics"

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


def get_patent_filings(country: str = None, year: str = None, office: str = None) -> Any:
    params = {"indicator": "patent_filings", "type": "applications"}
    if country:
        params["origin"] = country
    if year:
        params["year"] = year
    if office:
        params["office"] = office
    url = "https://www.wipo.int/ipstats/api/data/patents"
    return _make_request(url, params)


def get_trademark_filings(country: str = None, year: str = None) -> Any:
    params = {"indicator": "trademark_filings", "type": "applications"}
    if country:
        params["origin"] = country
    if year:
        params["year"] = year
    url = "https://www.wipo.int/ipstats/api/data/trademarks"
    return _make_request(url, params)


def get_industrial_designs(country: str = None, year: str = None) -> Any:
    params = {"indicator": "design_filings", "type": "applications"}
    if country:
        params["origin"] = country
    if year:
        params["year"] = year
    url = "https://www.wipo.int/ipstats/api/data/designs"
    return _make_request(url, params)


def get_ip_indicators(country: str = None, year: str = None) -> Any:
    params = {}
    if country:
        params["country"] = country
    if year:
        params["year"] = year
    url = "https://www.wipo.int/ipstats/api/data/summary"
    return _make_request(url, params)


def get_rankings(ip_type: str = "patents", year: str = None) -> Any:
    params = {"type": ip_type, "order": "desc", "limit": 50}
    if year:
        params["year"] = year
    url = "https://www.wipo.int/ipstats/api/data/rankings"
    return _make_request(url, params)


def get_countries() -> Any:
    url = "https://www.wipo.int/ipstats/api/data/countries"
    return _make_request(url)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "patents":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        office = args[3] if len(args) > 3 else None
        result = get_patent_filings(country, year, office)
    elif command == "trademarks":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_trademark_filings(country, year)
    elif command == "designs":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_industrial_designs(country, year)
    elif command == "indicators":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_ip_indicators(country, year)
    elif command == "rankings":
        ip_type = args[1] if len(args) > 1 else "patents"
        year = args[2] if len(args) > 2 else None
        result = get_rankings(ip_type, year)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
