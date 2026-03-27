"""
LME Data Fetcher
London Metal Exchange prices: copper, aluminium, zinc, lead, nickel, tin — cash and 3-month prices.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('LME_API_KEY', '')
BASE_URL = "https://www.lme.com/en/api"

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


def get_official_prices(metal: str, date: str) -> Any:
    params = {"metal": metal, "date": date, "priceType": "official"}
    return _make_request("prices/official", params)


def get_ring_prices(metal: str, start_date: str, end_date: str) -> Any:
    params = {"metal": metal, "startDate": start_date, "endDate": end_date, "priceType": "ring"}
    return _make_request("prices/ring", params)


def get_stocks(metal: str, warehouse: str, date: str) -> Any:
    params = {"metal": metal, "warehouse": warehouse, "date": date}
    return _make_request("stocks", params)


def get_cancelled_warrants(metal: str, date: str) -> Any:
    params = {"metal": metal, "date": date}
    return _make_request("warrants/cancelled", params)


def get_volume(metal: str, date: str) -> Any:
    params = {"metal": metal, "date": date}
    return _make_request("volume", params)


def get_available_metals() -> Any:
    return _make_request("metals", {})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "official":
        metal = args[1] if len(args) > 1 else "copper"
        date = args[2] if len(args) > 2 else "2024-01-15"
        result = get_official_prices(metal, date)
    elif command == "ring":
        metal = args[1] if len(args) > 1 else "copper"
        start_date = args[2] if len(args) > 2 else "2024-01-01"
        end_date = args[3] if len(args) > 3 else "2024-01-31"
        result = get_ring_prices(metal, start_date, end_date)
    elif command == "stocks":
        metal = args[1] if len(args) > 1 else "copper"
        warehouse = args[2] if len(args) > 2 else "all"
        date = args[3] if len(args) > 3 else "2024-01-15"
        result = get_stocks(metal, warehouse, date)
    elif command == "warrants":
        metal = args[1] if len(args) > 1 else "copper"
        date = args[2] if len(args) > 2 else "2024-01-15"
        result = get_cancelled_warrants(metal, date)
    elif command == "volume":
        metal = args[1] if len(args) > 1 else "copper"
        date = args[2] if len(args) > 2 else "2024-01-15"
        result = get_volume(metal, date)
    elif command == "metals":
        result = get_available_metals()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
