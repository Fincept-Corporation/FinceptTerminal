"""
UNCTAD Statistics Data Fetcher
United Nations Conference on Trade and Development: world trade, FDI flows,
maritime transport, investment policy, and development data.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('UNCTAD_API_KEY', '')
BASE_URL = "https://unctadstat-api.unctad.org/api"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
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


def get_fdi_flows(country: str = "USA", year: str = None) -> Any:
    params = {
        "reporterCode": country,
        "indicatorCode": "FDI_INFLOWS_SHARE",
        "format": "json",
    }
    if year:
        params["period"] = year
    return _make_request("GetData/FDI_INFLOWS_PROFILE", params)


def get_merchandise_trade(country: str = "USA", partner: str = "WLD", year: str = None) -> Any:
    params = {
        "reporterCode": country,
        "partnerCode": partner,
        "indicatorCode": "US_TOT",
        "format": "json",
    }
    if year:
        params["period"] = year
    return _make_request("GetData/TradeMerchandiseTotals", params)


def get_services_trade(country: str = "USA", year: str = None) -> Any:
    params = {
        "reporterCode": country,
        "format": "json",
    }
    if year:
        params["period"] = year
    return _make_request("GetData/TradeServices", params)


def get_maritime_transport(country: str = "USA", year: str = None) -> Any:
    params = {
        "reporterCode": country,
        "format": "json",
    }
    if year:
        params["period"] = year
    return _make_request("GetData/MaritimeProfile", params)


def get_development_indicators(country: str = "USA", year: str = None) -> Any:
    params = {
        "reporterCode": country,
        "format": "json",
    }
    if year:
        params["period"] = year
    return _make_request("GetData/DevelopmentProfile", params)


def get_datasets() -> Any:
    return _make_request("Datasets", {"format": "json"})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "fdi":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else None
        result = get_fdi_flows(country, year)
    elif command == "merchandise":
        country = args[1] if len(args) > 1 else "USA"
        partner = args[2] if len(args) > 2 else "WLD"
        year = args[3] if len(args) > 3 else None
        result = get_merchandise_trade(country, partner, year)
    elif command == "services":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else None
        result = get_services_trade(country, year)
    elif command == "maritime":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else None
        result = get_maritime_transport(country, year)
    elif command == "development":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else None
        result = get_development_indicators(country, year)
    elif command == "datasets":
        result = get_datasets()

    print(json.dumps(result))


if __name__ == "__main__":
    main()
