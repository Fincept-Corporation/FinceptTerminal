"""
USDA Crop Data Fetcher
USDA WASDE (World Agricultural Supply and Demand Estimates): monthly crop supply/demand forecasts.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('USDA_API_KEY', '')
BASE_URL = "https://apps.fas.usda.gov/psdonline/app/index.html"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"https://apps.fas.usda.gov/psdonline/api/v1/{endpoint}" if not endpoint.startswith('http') else endpoint
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


def get_wasde_report(commodity: str, country: str, year: str, month: str) -> Any:
    params = {"commodityCode": commodity, "countryCode": country, "marketYear": year, "monthId": month}
    return _make_request("data/commodity", params)


def get_corn_wasde(year: str) -> Any:
    params = {"commodityCode": "0440000", "marketYear": year}
    return _make_request("data/commodity", params)


def get_wheat_wasde(year: str) -> Any:
    params = {"commodityCode": "0410000", "marketYear": year}
    return _make_request("data/commodity", params)


def get_soybean_wasde(year: str) -> Any:
    params = {"commodityCode": "2222000", "marketYear": year}
    return _make_request("data/commodity", params)


def get_cotton_wasde(year: str) -> Any:
    params = {"commodityCode": "2631000", "marketYear": year}
    return _make_request("data/commodity", params)


def get_available_commodities() -> Any:
    return _make_request("lookup/commodity", {})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "wasde":
        commodity = args[1] if len(args) > 1 else "0440000"
        country = args[2] if len(args) > 2 else "US"
        year = args[3] if len(args) > 3 else "2024"
        month = args[4] if len(args) > 4 else "1"
        result = get_wasde_report(commodity, country, year, month)
    elif command == "corn":
        year = args[1] if len(args) > 1 else "2024"
        result = get_corn_wasde(year)
    elif command == "wheat":
        year = args[1] if len(args) > 1 else "2024"
        result = get_wheat_wasde(year)
    elif command == "soybeans":
        year = args[1] if len(args) > 1 else "2024"
        result = get_soybean_wasde(year)
    elif command == "cotton":
        year = args[1] if len(args) > 1 else "2024"
        result = get_cotton_wasde(year)
    elif command == "commodities":
        result = get_available_commodities()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
