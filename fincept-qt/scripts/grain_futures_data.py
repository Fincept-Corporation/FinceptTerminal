"""
Grain Futures Data Fetcher
CBOT grain futures (Corn, Wheat, Soybeans, Rice) via public CME/CBOT settlement data.
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


def get_corn_futures(date: str) -> Any:
    params = {"productCode": "ZC", "date": date, "exchange": "CBOT"}
    return _make_request("ZC/P", params)


def get_wheat_futures(date: str) -> Any:
    params = {"productCode": "ZW", "date": date, "exchange": "CBOT"}
    return _make_request("ZW/P", params)


def get_soybean_futures(date: str) -> Any:
    params = {"productCode": "ZS", "date": date, "exchange": "CBOT"}
    return _make_request("ZS/P", params)


def get_rice_futures(date: str) -> Any:
    params = {"productCode": "ZR", "date": date, "exchange": "CBOT"}
    return _make_request("ZR/P", params)


def get_oats_futures(date: str) -> Any:
    params = {"productCode": "ZO", "date": date, "exchange": "CBOT"}
    return _make_request("ZO/P", params)


def get_grain_settlement(grain: str, date: str) -> Any:
    grain_map = {
        "corn": "ZC", "wheat": "ZW", "soybeans": "ZS",
        "rice": "ZR", "oats": "ZO"
    }
    product_code = grain_map.get(grain.lower(), grain.upper())
    params = {"productCode": product_code, "date": date, "exchange": "CBOT"}
    return _make_request(f"{product_code}/P", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "corn":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_corn_futures(date)
    elif command == "wheat":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_wheat_futures(date)
    elif command == "soybeans":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_soybean_futures(date)
    elif command == "rice":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_rice_futures(date)
    elif command == "oats":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_oats_futures(date)
    elif command == "settlement":
        grain = args[1] if len(args) > 1 else "corn"
        date = args[2] if len(args) > 2 else "2024-01-15"
        result = get_grain_settlement(grain, date)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
