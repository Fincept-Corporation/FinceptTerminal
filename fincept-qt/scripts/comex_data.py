"""
COMEX Data Fetcher
COMEX metals futures (Gold, Silver, Copper) via public CME/COMEX data: settlement prices, volume, open interest.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('CME_API_KEY', '')
BASE_URL = "https://www.cmegroup.com/CmeWS/mvc/Settlements/futures/settlements"

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


def get_gold_futures(date: str) -> Any:
    params = {"productCode": "GC", "date": date, "exchange": "COMEX"}
    return _make_request("GC/P", params)


def get_silver_futures(date: str) -> Any:
    params = {"productCode": "SI", "date": date, "exchange": "COMEX"}
    return _make_request("SI/P", params)


def get_copper_futures(date: str) -> Any:
    params = {"productCode": "HG", "date": date, "exchange": "COMEX"}
    return _make_request("HG/P", params)


def get_platinum_futures(date: str) -> Any:
    params = {"productCode": "PL", "date": date, "exchange": "NYMEX"}
    return _make_request("PL/P", params)


def get_palladium_futures(date: str) -> Any:
    params = {"productCode": "PA", "date": date, "exchange": "NYMEX"}
    return _make_request("PA/P", params)


def get_metals_settlement(metal: str, date: str) -> Any:
    metal_map = {
        "gold": "GC", "silver": "SI", "copper": "HG",
        "platinum": "PL", "palladium": "PA"
    }
    product_code = metal_map.get(metal.lower(), metal.upper())
    params = {"productCode": product_code, "date": date, "exchange": "COMEX"}
    return _make_request(f"{product_code}/P", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "gold":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_gold_futures(date)
    elif command == "silver":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_silver_futures(date)
    elif command == "copper":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_copper_futures(date)
    elif command == "platinum":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_platinum_futures(date)
    elif command == "palladium":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_palladium_futures(date)
    elif command == "settlement":
        metal = args[1] if len(args) > 1 else "gold"
        date = args[2] if len(args) > 2 else "2024-01-15"
        result = get_metals_settlement(metal, date)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
