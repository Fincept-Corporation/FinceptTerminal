"""
NYMEX Data Fetcher
NYMEX energy futures (WTI, Brent, Natural Gas, Gasoline) via public CME/NYMEX data.
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


def get_wti_crude(date: str) -> Any:
    params = {"productCode": "CL", "date": date, "exchange": "NYMEX"}
    return _make_request("CL/P", params)


def get_brent_crude(date: str) -> Any:
    params = {"productCode": "BZ", "date": date, "exchange": "NYMEX"}
    return _make_request("BZ/P", params)


def get_natural_gas(date: str) -> Any:
    params = {"productCode": "NG", "date": date, "exchange": "NYMEX"}
    return _make_request("NG/P", params)


def get_rbob_gasoline(date: str) -> Any:
    params = {"productCode": "RB", "date": date, "exchange": "NYMEX"}
    return _make_request("RB/P", params)


def get_heating_oil(date: str) -> Any:
    params = {"productCode": "HO", "date": date, "exchange": "NYMEX"}
    return _make_request("HO/P", params)


def get_energy_settlement(product: str, date: str) -> Any:
    product_map = {
        "wti": "CL", "brent": "BZ", "natural_gas": "NG",
        "gasoline": "RB", "heating_oil": "HO"
    }
    product_code = product_map.get(product.lower(), product.upper())
    params = {"productCode": product_code, "date": date, "exchange": "NYMEX"}
    return _make_request(f"{product_code}/P", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "wti":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_wti_crude(date)
    elif command == "brent":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_brent_crude(date)
    elif command == "natural_gas":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_natural_gas(date)
    elif command == "gasoline":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_rbob_gasoline(date)
    elif command == "heating_oil":
        date = args[1] if len(args) > 1 else "2024-01-15"
        result = get_heating_oil(date)
    elif command == "settlement":
        product = args[1] if len(args) > 1 else "wti"
        date = args[2] if len(args) > 2 else "2024-01-15"
        result = get_energy_settlement(product, date)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
