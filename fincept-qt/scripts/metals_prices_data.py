"""
Metals Prices Data Fetcher
Gold, silver, platinum, palladium, and copper real-time and historical prices
via the Metals.dev API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('METALS_DEV_API_KEY', '')
BASE_URL = "https://api.metals.dev/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["api_key"] = API_KEY
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


def get_latest(currencies: str = "USD", metals: str = "gold,silver,platinum,palladium") -> Any:
    params = {"currency": currencies, "unit": "troy_oz"}
    metals_list = [m.strip() for m in metals.split(",")]
    params["metals"] = ",".join(metals_list)
    return _make_request("latest", params)


def get_historical(date: str, currencies: str = "USD", metals: str = "gold,silver") -> Any:
    params = {"date": date, "currency": currencies, "unit": "troy_oz"}
    metals_list = [m.strip() for m in metals.split(",")]
    params["metals"] = ",".join(metals_list)
    return _make_request("historical", params)


def get_timeframe(start: str, end: str, currencies: str = "USD", metals: str = "gold") -> Any:
    params = {
        "start_date": start,
        "end_date": end,
        "currency": currencies,
        "unit": "troy_oz",
    }
    metals_list = [m.strip() for m in metals.split(",")]
    params["metals"] = ",".join(metals_list)
    return _make_request("timeframe", params)


def convert(from_metal: str, to_currency: str, amount: str = "1") -> Any:
    params = {
        "metal": from_metal.lower(),
        "currency": to_currency.upper(),
        "amount": amount,
        "unit": "troy_oz",
    }
    return _make_request("convert", params)


def get_supported_metals() -> Any:
    return {
        "metals": [
            "gold", "silver", "platinum", "palladium", "rhodium",
            "iridium", "osmium", "rhenium", "ruthenium", "copper",
            "aluminum", "lead", "nickel", "zinc", "tin"
        ]
    }


def get_supported_currencies() -> Any:
    return _make_request("currencies", {})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "latest":
        currencies = args[1] if len(args) > 1 else "USD"
        metals = args[2] if len(args) > 2 else "gold,silver,platinum,palladium"
        result = get_latest(currencies, metals)
    elif command == "historical":
        date = args[1] if len(args) > 1 else "2024-01-01"
        currencies = args[2] if len(args) > 2 else "USD"
        metals = args[3] if len(args) > 3 else "gold,silver"
        result = get_historical(date, currencies, metals)
    elif command == "timeframe":
        start = args[1] if len(args) > 1 else "2024-01-01"
        end = args[2] if len(args) > 2 else "2024-12-31"
        currencies = args[3] if len(args) > 3 else "USD"
        metals = args[4] if len(args) > 4 else "gold"
        result = get_timeframe(start, end, currencies, metals)
    elif command == "convert":
        from_metal = args[1] if len(args) > 1 else "gold"
        to_currency = args[2] if len(args) > 2 else "USD"
        amount = args[3] if len(args) > 3 else "1"
        result = convert(from_metal, to_currency, amount)
    elif command == "metals":
        result = get_supported_metals()
    elif command == "currencies":
        result = get_supported_currencies()

    print(json.dumps(result))


if __name__ == "__main__":
    main()
